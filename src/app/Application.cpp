#include "app/Application.h"

#include <shellapi.h>

#include <ctime>
#include <system_error>

#include "app/FileDialogs.h"
#include "core/Autostart.h"
#include "core/Logger.h"
#include "core/Paths.h"
#include "core/StringUtil.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "instances/RobloxLocator.h"
#include "ui/Components.h"
#include "ui/Fonts.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace rl::app {
namespace {

constexpr double kSaveDebounceSeconds = 0.6;
constexpr unsigned kIdleWakeMs = 500;

enum TrayCommand : UINT { kCmdOpen = 100, kCmdExit = 101, kCmdInstance = 1000 };

COLORREF toColorRef(const ImVec4& c) {
    return RGB(static_cast<int>(c.x * 255.0f), static_cast<int>(c.y * 255.0f), static_cast<int>(c.z * 255.0f));
}

// README.it.md for Italian, README.md (English) otherwise.
std::filesystem::path readmePath(const std::string& language = "en") {
    std::error_code ec;
    const auto dir = paths::executableDir();
    if (language == "it" && std::filesystem::exists(dir / L"README.it.md", ec)) return dir / L"README.it.md";
    const auto p = dir / L"README.md";
    return std::filesystem::exists(p, ec) ? p : std::filesystem::path{};
}

void shellOpen(const std::wstring& target) {
    ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

ui::ToastKind toastKind(Notice::Level l) {
    switch (l) {
        case Notice::Level::Success: return ui::ToastKind::Success;
        case Notice::Level::Warning: return ui::ToastKind::Warning;
        case Notice::Level::Error: return ui::ToastKind::Error;
        case Notice::Level::Info: break;
    }
    return ui::ToastKind::Info;
}

}  // namespace

Application::Application(LaunchOptions options) : options_(options) {}

Application::~Application() { shutdown(); }

int Application::run() {
    if (!initialize()) return 1;
    bool running = true;
    while (running) {
        const bool hidden = window_.isMinimized() || !window_.isVisible();
        running = window_.processMessages(framesToRender_ <= 0 || hidden, kIdleWakeMs);
        if (!running) break;
        mainQueue_.drain();  // async results are applied even while hidden in the tray
        if (hidden) {
            saveSettingsIfDue(false);
            continue;
        }
        frame();
    }
    return 0;
}

// --- startup ---------------------------------------------------------------------------

bool Application::initialize() {
    log::init(paths::logsDir());
    log::info("app", std::string("Lumen ") + RL_VERSION + " starting");

    const auto loaded = settings::load(paths::settingsFile());
    settings_ = loaded.settings;
    if (loaded.status == settings::LoadStatus::Corrupt) {
        startupNotices_.push_back({Notice::Level::Warning, "toast.settings_corrupt",
                                   {{"file", loaded.backupPath.filename().string()}}});
    }

    if (!localizer_.loadDirectory(paths::executableDir() / L"locales")) {
        // Without string tables nothing can be localized: the only hard-coded UI text.
        MessageBoxW(nullptr, L"Locale files are missing (locales\\en.json). Please reinstall Lumen.",
                    L"Lumen", MB_ICONERROR);
        return false;
    }
    if (settings_.language.empty()) {
        settings_.language = i18n::Localizer::detectSystemLanguage();
        log::info("app", "First run, detected language: " + settings_.language);
        settingsDirty_ = true;
    }
    localizer_.setLanguage(settings_.language);

    createServices();

    const bool startHidden = options_.autostart && settings_.startMinimizedToTray;
    const int showCmd = startHidden ? SW_HIDE : (settings_.windowMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
    if (!window_.create(str::toWide(localizer_.tr("app.title")), settings_.windowWidth, settings_.windowHeight,
                        showCmd)) {
        MessageBoxW(nullptr, str::toWide(localizer_.tr("error.d3d")).c_str(), L"Lumen", MB_ICONERROR);
        return false;
    }
    window_.setMessageHook([this](UINT m, WPARAM w, LPARAM l, LRESULT& out) { return onWindowMessage(m, w, l, out); });
    mainQueue_.setWaker([this] { window_.wake(); });
    tray_.add(window_.hwnd(), str::toWide(localizer_.tr("app.title")));

    initImGui();
    textures_ = std::make_unique<ui::TextureCache>(window_.device(), *api_, *pool_, mainQueue_,
                                                   paths::cacheDir() / L"thumbnails", alive_);
    applySettings(true);
    textures_->trimDiskAsync();
    bindContext();

    if (settings_.restoreLastScreen) layout_.navigate(ui::screenFromString(settings_.lastScreen));
    for (const auto& n : startupNotices_) notify(n);
    startupNotices_.clear();

    if (!instances::locateRoblox().installed) {
        notify({Notice::Level::Warning, "error.not_installed", {}});
    }
    accounts_->validateAll();
    accounts_->refreshAvatars();
    {
        // Leftover from the earlier embedded-browser sign-in (removed).
        std::error_code ec;
        std::filesystem::remove_all(paths::dataDir() / L"webview", ec);
    }
    return true;
}

void Application::createServices() {
    pool_ = std::make_unique<ThreadPool>(6);
    api_ = std::make_unique<api::RobloxApi>();
    const auto data = paths::dataDir();

    auto reportCorrupt = [this](settings::LoadStatus s, const char* file) {
        if (s == settings::LoadStatus::Corrupt) {
            startupNotices_.push_back({Notice::Level::Warning, "toast.file_corrupt", {{"file", file}}});
        }
    };
    accountStore_ = std::make_unique<accounts::AccountStore>(data / L"accounts.json");
    reportCorrupt(accountStore_->load(), "accounts.json");
    recents_ = std::make_unique<games::GameList>(data / L"recent_games.json", static_cast<size_t>(settings_.recentGamesMax));
    reportCorrupt(recents_->load(), "recent_games.json");
    favorites_ = std::make_unique<games::GameList>(data / L"favorites.json", 0);
    reportCorrupt(favorites_->load(), "favorites.json");

    Notify n = [this](Notice notice) { notify(notice); };
    accounts_ = std::make_unique<accounts::AccountService>(*accountStore_, *api_, *pool_, mainQueue_, n, alive_);
    games_ = std::make_unique<games::GameService>(*api_, *pool_, mainQueue_, *recents_, *favorites_, n, alive_);
    instances_ = std::make_unique<instances::InstanceManager>(*accounts_, *api_, *pool_, mainQueue_, n, alive_);
    quickLogin_ = std::make_unique<accounts::QuickLoginFlow>(
        *api_, *pool_, mainQueue_, *accounts_, alive_, [](const std::string& url) { shellOpen(str::toWide(url)); });
}

void Application::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // layout is code-driven; nothing to persist
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplWin32_Init(window_.hwnd());
    ImGui_ImplDX11_Init(window_.device(), window_.context());
    imguiReady_ = true;
    rebuildDpiResources();
}

void Application::bindContext() {
    ctx_ = std::make_unique<ui::UiContext>(settings_, localizer_);
    ui::UiContext& c = *ctx_;
    c.accounts = accounts_.get();
    c.login = quickLogin_.get();
    c.instances = instances_.get();
    c.games = games_.get();
    logo_ = ui::loadResourceImage(window_.device(), ui::kLogoResourceId);
    if (!logo_) log::warn("app", "Embedded logo could not be loaded; using the vector fallback");
    c.logo = reinterpret_cast<std::uint64_t>(logo_.get());
    c.textures = textures_.get();
    c.settingsChanged = [this] { onSettingsChanged(); };
    c.resetAll = [this] { resetAll(); };
    c.clearCache = [this] { clearCache(); };
    c.clearHistory = [this] {
        games_->clearHistory();
        notify({Notice::Level::Success, "toast.history_cleared", {}});
    };
    c.openDataFolder = [] { shellOpen(paths::dataDir().wstring()); };
    if (!readmePath().empty())
        c.openReadme = [this] { shellOpen(readmePath(settings_.language).wstring()); };
    c.openUrl = [](const char* url) { shellOpen(str::toWide(url)); };
    c.addAccount = [this] { startLogin({}); };
    c.reloginAccount = [this](const std::string& id) { startLogin(id); };
    c.loginInProgress = [this] { return quickLogin_->busy(); };
    c.exportAccounts = [this] { exportAccounts(); };
    c.importAccounts = [this] { importAccounts(); };
    c.openGame = [this](std::int64_t uid) { layout_.openGame(uid); };
    c.goBack = [this] { layout_.back(); };
    c.launchGame = [this](const std::string& acc, std::int64_t uid) { launchGame(acc, uid); };
    c.defaultAccountId = [this] { return defaultAccountId(); };
}

void Application::rebuildDpiResources() {
    const float scale = window_.dpiScale();
    ui::setDpiScale(scale);
    ui::loadFonts(scale);
    ui::applyTheme(settings_.theme, scale);
    ImGui_ImplDX11_InvalidateDeviceObjects();  // font texture is recreated on the next NewFrame
}

// --- frame -----------------------------------------------------------------------------

void Application::frame() {
    if (window_.consumeDpiChanged()) rebuildDpiResources();
    window_.beginFrame();
    games_->tick();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    layout_.draw(*ctx_);
    const std::string screen = ui::screenToString(layout_.current());
    if (settings_.lastScreen != screen) {
        settings_.lastScreen = screen;
        settingsDirty_ = true;
        lastEdit_ = ImGui::GetTime();
    }

    ImGui::Render();
    const ImVec4 bg = ui::palette().bg;
    const float clear[4] = {bg.x, bg.y, bg.z, 1.0f};
    window_.bindAndClear(clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    window_.present();

    // Keep rendering a few frames after input so ImGui can settle hover/active states.
    const ImGuiIO& io = ImGui::GetIO();
    const bool input = io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f || io.WantTextInput ||
                       ImGui::IsAnyItemActive() || io.MouseWheel != 0.0f;
    if (ui::consumeRedrawRequest() || input) {
        framesToRender_ = 3;
    } else {
        --framesToRender_;
    }
    saveSettingsIfDue(false);
}

// --- window / tray -----------------------------------------------------------------------

bool Application::onWindowMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& out) {
    if (msg == TrayIcon::kCallbackMessage) {
        switch (LOWORD(lp)) {
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case NIN_SELECT:
            case NIN_KEYSELECT:
                window_.show();
                framesToRender_ = 3;
                break;
            case WM_CONTEXTMENU:
            case WM_RBUTTONUP:
                showTrayMenu();
                break;
            default:
                break;
        }
        out = 0;
        return true;
    }
    if (msg == TrayIcon::taskbarCreatedMessage()) {
        tray_.add(window_.hwnd(), str::toWide(localizer_.tr("app.title")));
        return false;
    }
    if (msg == WM_CLOSE && settings_.closeToTray && tray_.visible()) {
        window_.hide();
        if (!trayHintShown_) {
            tray_.balloon(str::toWide(localizer_.tr("app.title")), str::toWide(localizer_.tr("tray.still_running")));
            trayHintShown_ = true;
        }
        out = 0;
        return true;
    }
    if (msg == WM_SETTINGCHANGE && lp && settings_.theme == settings::Theme::System &&
        lstrcmpW(reinterpret_cast<LPCWSTR>(lp), L"ImmersiveColorSet") == 0) {
        ui::applyTheme(settings_.theme, window_.dpiScale());
        updateWindowChrome();
        framesToRender_ = 3;
    }
    (void)wp;
    return false;
}

void Application::showTrayMenu() {
    std::vector<TrayIcon::MenuItem> items;
    items.push_back({kCmdOpen, str::toWide(localizer_.tr("tray.open")), true, true});
    items.push_back({});
    const int active = instances_->activeCount();
    items.push_back({kCmdInstance, str::toWide(localizer_.format("tray.instances", {{"count", std::to_string(active)}})),
                     false, false});
    UINT i = 1;
    for (const auto& inst : instances_->instances()) {
        if (inst.status != instances::InstanceStatus::Running && inst.status != instances::InstanceStatus::Launching)
            continue;
        const std::string game = inst.gameName.empty() ? std::to_string(inst.placeId) : inst.gameName;
        items.push_back({kCmdInstance + i++, str::toWide("    " + inst.accountLabel + "  \xE2\x80\x94  " + game), false, false});
    }
    items.push_back({});
    items.push_back({kCmdExit, str::toWide(localizer_.tr("tray.exit")), true, false});

    switch (tray_.showMenu(items)) {
        case kCmdOpen:
            window_.show();
            framesToRender_ = 3;
            break;
        case kCmdExit:
            window_.exit();
            break;
        default:
            break;
    }
}

// --- notices & settings ----------------------------------------------------------------------

void Application::notify(const Notice& n) {
    if (!imguiReady_) {
        startupNotices_.push_back(n);
        return;
    }
    ui::pushToast(localizer_.format(n.key, n.args), toastKind(n.level),
                  n.level == Notice::Level::Error ? 7.0f : 5.0f);
}

void Application::onSettingsChanged() {
    applySettings(false);
    settingsDirty_ = true;
    lastEdit_ = ImGui::GetTime();
}

void Application::applySettings(bool force) {
    if (force || settings_.language != applied_.language) {
        localizer_.setLanguage(settings_.language);
        window_.setTitle(str::toWide(localizer_.tr("app.title")));
        tray_.setTooltip(str::toWide(localizer_.tr("app.title")));
    }
    if (force || settings_.theme != applied_.theme) {
        ui::applyTheme(settings_.theme, window_.dpiScale());
        updateWindowChrome();
    }
    ui::setAnimationsEnabled(settings_.animations);

    instances_->configure(settings_.multiInstanceEnabled, settings_.maxInstances, settings_.instanceLaunchDelayMs);
    api_->cache().setTtl(std::chrono::minutes(settings_.apiCacheTtlMinutes));
    games_->setRecentMax(static_cast<size_t>(settings_.recentGamesMax));
    if (textures_) {
        textures_->setDiskLimitMb(settings_.thumbnailCacheMaxMb);
        if (!force && settings_.thumbnailCacheMaxMb < applied_.thumbnailCacheMaxMb) textures_->trimDiskAsync();
    }
    if (!force && settings_.recentGamesMax < applied_.recentGamesMax) recents_->save();

    if (force) {
        // The registry is the source of truth for autostart (the user may change it elsewhere).
        settings_.launchWithWindows = autostart::isEnabled();
    } else if (settings_.launchWithWindows != applied_.launchWithWindows) {
        if (!autostart::setEnabled(settings_.launchWithWindows, paths::executablePath())) {
            settings_.launchWithWindows = !settings_.launchWithWindows;
            notify({Notice::Level::Error, "toast.autostart_failed", {}});
        }
    }
    if (!force && settings_.multiInstanceEnabled && !applied_.multiInstanceEnabled &&
        !instances::runningPlayerPids().empty()) {
        notify({Notice::Level::Warning, "error.multi_unavailable", {}});
    }
    applied_ = settings_;
}

void Application::updateWindowChrome() {
    const ui::Palette& p = ui::palette();
    window_.setTitleBarColors(p.dark, toColorRef(p.sidebar), toColorRef(p.text));
}

void Application::saveSettingsIfDue(bool force) {
    if (!settingsDirty_) return;
    if (!force && imguiReady_ && ImGui::GetTime() - lastEdit_ < kSaveDebounceSeconds) {
        ui::requestRedraw();  // wake up again to flush the save
        return;
    }
    if (settings::save(paths::settingsFile(), settings_)) {
        settingsDirty_ = false;
    } else {
        if (imguiReady_) notify({Notice::Level::Error, "toast.settings_save_failed", {}});
        settingsDirty_ = false;  // avoid a toast storm; the next edit retries
    }
}

// --- actions -----------------------------------------------------------------------------

std::string Application::defaultAccountId() const {
    for (const auto& a : accounts_->accounts()) {
        if (a.enabled && a.hasSession() && !instances_->accountActive(a.id)) return a.id;
    }
    for (const auto& a : accounts_->accounts()) {
        if (a.enabled && a.hasSession()) return a.id;
    }
    return {};
}

void Application::startLogin(const std::string& expectedAccountId) {
    if (quickLogin_->busy()) return;
    quickLogin_->start(expectedAccountId, [this, expectedAccountId](ErrorCode e, const std::string& id) {
        framesToRender_ = 3;
        if (e != ErrorCode::None) return;  // the dialog shows the error and offers a retry
        quickLogin_->dismiss();
        window_.show();  // come back from the browser
        const accounts::Account* a = accounts_->find(id);
        const std::string name = a ? a->label() : "";
        if (!expectedAccountId.empty() && expectedAccountId != id) {
            notify({Notice::Level::Warning, "accounts.relogin_other", {{"name", name}}});
        } else {
            notify({Notice::Level::Success, expectedAccountId.empty() ? "accounts.added" : "accounts.refreshed",
                    {{"name", name}}});
        }
    });
}

void Application::launchGame(const std::string& accountId, std::int64_t universeId) {
    std::int64_t placeId = 0;
    std::string name;
    if (const auto* g = games_->game(universeId)) {
        placeId = g->rootPlaceId;
        name = g->name;
    }
    if (!placeId) {
        const games::GameEntry* e = games_->recents().find(universeId);
        if (!e) e = games_->favorites().find(universeId);
        if (e) {
            placeId = e->placeId;
            if (name.empty()) name = e->name;
        }
    }
    if (!placeId) {
        notify({Notice::Level::Warning, "launch.details_missing", {}});
        games_->ensureDetails(universeId);
        return;
    }
    instances::LaunchRequest req;
    req.accountId = accountId;
    req.placeId = placeId;
    req.universeId = universeId;
    req.gameName = name;
    doLaunch(req);
}

void Application::doLaunch(instances::LaunchRequest req) {
    const ErrorCode pre = instances_->check(req);
    switch (pre) {
        case ErrorCode::None:
            break;
        case ErrorCode::AccountBusy:
            ui::requestConfirm({localizer_.tr("launch.same_account.title"), localizer_.tr("launch.same_account.body"),
                                localizer_.tr("launch.same_account.confirm"), localizer_.tr("common.cancel"), false,
                                [this, req]() mutable {
                                    req.allowSameAccount = true;
                                    doLaunch(req);
                                }});
            return;
        case ErrorCode::RobloxNotInstalled:
            ui::requestConfirm({localizer_.tr("launch.not_installed.title"), localizer_.tr("error.not_installed"),
                                localizer_.tr("launch.not_installed.download"), localizer_.tr("common.cancel"), false,
                                [] { shellOpen(str::toWide(instances::kDownloadUrl)); }});
            return;
        case ErrorCode::InstanceLimit:
            notify({Notice::Level::Warning,
                    settings_.multiInstanceEnabled ? "error.instance_limit" : "error.instance_limit_single",
                    {{"max", std::to_string(instances_->maxInstances())}}});
            return;
        default:
            notify({Notice::Level::Error, errorKey(pre), {}});
            return;
    }

    const accounts::Account* a = accounts_->find(req.accountId);
    notify({Notice::Level::Info, "launch.starting", {{"game", req.gameName}, {"account", a ? a->label() : ""}}});
    const std::int64_t universeId = req.universeId;
    instances_->launch(req, [this, universeId](ErrorCode e) {
        if (e == ErrorCode::None) {
            games_->recordPlayed(universeId);
            notify({Notice::Level::Success, "launch.started", {}});
        } else if (e == ErrorCode::InstanceLimit) {
            notify({Notice::Level::Warning, "error.instance_limit", {{"max", std::to_string(instances_->maxInstances())}}});
        } else {
            notify({Notice::Level::Error, errorKey(e), {}});
        }
        framesToRender_ = 3;
    });
}

void Application::clearCache() {
    textures_->clear();
    games_->clearMemoryCache();
    notify({Notice::Level::Success, "toast.cache_cleared", {}});
}

void Application::resetAll() {
    quickLogin_->cancel();
    const settings::Settings old = settings_;
    settings_ = settings::Settings{};
    settings_.language = old.language;  // keep the language the user is reading
    settings_.windowWidth = old.windowWidth;
    settings_.windowHeight = old.windowHeight;
    settings_.windowMaximized = old.windowMaximized;
    accounts_->removeAll();
    games_->clearHistory();
    while (!favorites_->items().empty()) games_->toggleFavorite(favorites_->items().front().universeId);
    textures_->clear();
    games_->clearMemoryCache();
    onSettingsChanged();
    notify({Notice::Level::Success, "toast.reset_done", {}});
    log::info("app", "Full reset performed");
}

void Application::exportAccounts() {
    const auto file = saveJsonDialog(window_.hwnd(), str::toWide(localizer_.tr("accounts.export")),
                                     L"roblox-launcher-accounts.json");
    if (!file) return;
    if (accounts_->exportTo(*file)) {
        notify({Notice::Level::Success, "accounts.exported", {{"file", file->filename().string()}}});
    } else {
        notify({Notice::Level::Error, "error.io", {}});
    }
}

void Application::importAccounts() {
    const auto file = openJsonDialog(window_.hwnd(), str::toWide(localizer_.tr("accounts.import")));
    if (!file) return;
    const int added = accounts_->importFrom(*file);
    if (added < 0) {
        notify({Notice::Level::Error, "accounts.import_invalid", {}});
    } else {
        notify({Notice::Level::Success, "accounts.imported", {{"count", std::to_string(added)}}});
    }
}

// --- shutdown ----------------------------------------------------------------------------

void Application::shutdown() {
    if (alive_ && *alive_) {
        *alive_ = false;
        if (quickLogin_) quickLogin_->cancel();
        pool_.reset();  // joins workers; queued work is dropped
    }
    if (!imguiReady_) {
        log::shutdown();
        return;
    }
    window_.placement(settings_.windowWidth, settings_.windowHeight, settings_.windowMaximized);
    settings_.clamp();
    settingsDirty_ = true;
    saveSettingsIfDue(true);
    tray_.remove();

    ctx_.reset();
    textures_.reset();  // releases GPU textures before the device goes away
    logo_.reset();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    imguiReady_ = false;
    log::info("app", "Shutdown complete");
    log::shutdown();
}

}  // namespace rl::app
