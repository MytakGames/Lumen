#include "ui/MainLayout.h"

#include <algorithm>

#include "instances/InstanceManager.h"
#include "ui/Components.h"
#include "ui/Fonts.h"
#include "ui/LoginDialog.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/screens/AccountsScreen.h"
#include "ui/screens/GameDetailScreen.h"
#include "ui/screens/HomeScreen.h"
#include "ui/screens/InstancesScreen.h"
#include "ui/screens/SettingsScreen.h"

namespace rl::ui {
namespace {

constexpr float kSidebarWidth = 236.0f;
constexpr float kContentMaxWidth = 1180.0f;

struct NavEntry {
    ScreenId id;
    const char* key;
    Icon icon;
};
constexpr NavEntry kNav[] = {
    {ScreenId::Home, "nav.home", Icon::Home},
    {ScreenId::Accounts, "nav.accounts", Icon::Accounts},
    {ScreenId::Instances, "nav.instances", Icon::Instances},
};

}  // namespace

const char* screenToString(ScreenId id) {
    switch (id) {
        case ScreenId::Home: return "home";
        case ScreenId::Accounts: return "accounts";
        case ScreenId::Instances: return "instances";
        case ScreenId::Settings: return "settings";
        case ScreenId::GameDetail: return "home";  // a detail page is not restored on restart
        case ScreenId::Count: break;
    }
    return "home";
}

ScreenId screenFromString(const std::string& s) {
    if (s == "accounts") return ScreenId::Accounts;
    if (s == "instances") return ScreenId::Instances;
    if (s == "settings") return ScreenId::Settings;
    return ScreenId::Home;
}

MainLayout::MainLayout() {
    auto at = [this](ScreenId id) -> std::unique_ptr<Screen>& { return screens_[static_cast<size_t>(id)]; };
    at(ScreenId::Home) = std::make_unique<HomeScreen>();
    at(ScreenId::Accounts) = std::make_unique<AccountsScreen>();
    at(ScreenId::Instances) = std::make_unique<InstancesScreen>();
    at(ScreenId::Settings) = std::make_unique<SettingsScreen>();
    auto detail = std::make_unique<GameDetailScreen>();
    detail_ = detail.get();
    at(ScreenId::GameDetail) = std::move(detail);
}

void MainLayout::navigate(ScreenId id) {
    if (id == ScreenId::Count) return;
    if (id == current_ && id != ScreenId::GameDetail) return;
    if (current_ != id) history_.push_back(current_);
    if (history_.size() > 20) history_.erase(history_.begin());
    current_ = id;
    transition_ = 0.0f;
    showPending_ = true;
    scrollTop_ = true;
    requestRedraw();
}

void MainLayout::openGame(std::int64_t universeId) {
    detail_->setGame(universeId);
    if (current_ == ScreenId::GameDetail) {
        showPending_ = true;
        scrollTop_ = true;
        transition_ = 0.0f;
    } else {
        navigate(ScreenId::GameDetail);
    }
}

void MainLayout::back() {
    ScreenId target = ScreenId::Home;
    while (!history_.empty()) {
        target = history_.back();
        history_.pop_back();
        if (target != ScreenId::GameDetail) break;
    }
    if (target == current_) return;
    current_ = target;
    transition_ = 0.0f;
    showPending_ = true;
    scrollTop_ = true;
}

void MainLayout::draw(UiContext& ctx) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    constexpr ImGuiWindowFlags rootFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                           ImGuiWindowFlags_NoScrollbar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("##root", nullptr, rootFlags);
    ImGui::PopStyleVar();

    if (showPending_) {
        showPending_ = false;
        screens_[static_cast<size_t>(current_)]->onShow(ctx);
    }

    drawSidebar(ctx);
    ImGui::SameLine(0, 0);
    drawContent(ctx);
    drawConfirm();
    drawLoginDialog(ctx);

    ImGui::End();
    drawToasts();
}

void MainLayout::drawSidebar(UiContext& ctx) {
    const Palette& p = palette();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.sidebar);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {dp(14), dp(18)});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {dp(8), dp(4)});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::BeginChild("##sidebar", {dp(kSidebarWidth), 0}, ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar);

    // Brand
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 brand = ImGui::GetCursorScreenPos();
    if (ctx.logo) {
        const float half = dp(19);
        dl->AddImage(static_cast<ImTextureID>(ctx.logo), {brand.x + dp(20) - half, brand.y + dp(20) - half},
                     {brand.x + dp(20) + half, brand.y + dp(20) + half});
    } else {
        drawIcon(dl, Icon::Logo, {brand.x + dp(20), brand.y + dp(20)}, dp(36), toU32(p.onAccent));
    }
    ImGui::SetCursorScreenPos({brand.x + dp(48), brand.y + dp(1)});
    ImGui::PushFont(fonts().semibold);
    ImGui::TextUnformatted(ctx.tr.c("app.title"));
    ImGui::PopFont();
    ImGui::SetCursorScreenPos({brand.x + dp(48), brand.y + dp(21)});
    ImGui::PushFont(fonts().small);
    ImGui::TextColored(p.textMuted, "%s", ctx.tr.c("app.subtitle"));
    ImGui::PopFont();
    ImGui::SetCursorScreenPos({brand.x, brand.y + dp(64)});
    ImGui::Dummy({0, 0});

    const ScreenId highlighted = current_ == ScreenId::GameDetail ? ScreenId::Home : current_;
    for (const auto& e : kNav) {
        const ImVec2 itemPos = ImGui::GetCursorScreenPos();
        if (navItem(e.key, ctx.tr.c(e.key), e.icon, highlighted == e.id)) navigate(e.id);
        // Live badge with the number of running instances.
        if (e.id == ScreenId::Instances && ctx.instances && ctx.instances->activeCount() > 0) {
            const std::string n = std::to_string(ctx.instances->activeCount());
            ImGui::PushFont(fonts().small);
            const ImVec2 ts = ImGui::CalcTextSize(n.c_str());
            const float w = std::max(dp(20), ts.x + dp(10));
            const ImVec2 bmin(itemPos.x + ImGui::GetContentRegionAvail().x - w - dp(10), itemPos.y + dp(11));
            dl->AddRectFilled(bmin, {bmin.x + w, bmin.y + dp(20)}, toU32(p.accent), dp(10));
            dl->AddText({bmin.x + (w - ts.x) * 0.5f, bmin.y + (dp(20) - ts.y) * 0.5f}, toU32(p.onAccent), n.c_str());
            ImGui::PopFont();
        }
    }

    // Settings pinned at the bottom, version below it.
    const float bottomBlock = dp(42) + dp(30);
    const float y = ImGui::GetWindowHeight() - bottomBlock - dp(18);
    if (ImGui::GetCursorPosY() < y) ImGui::SetCursorPosY(y);
    if (navItem("nav.settings", ctx.tr.c("nav.settings"), Icon::Settings, current_ == ScreenId::Settings)) {
        navigate(ScreenId::Settings);
    }
    ImGui::PushFont(fonts().small);
    const std::string version = ctx.tr.format("app.version", {{"version", RL_VERSION}});
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + dp(14));
    ImGui::TextColored(p.textMuted, "%s", version.c_str());
    ImGui::PopFont();

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
}

void MainLayout::drawContent(UiContext& ctx) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {dp(36), dp(28)});
    ImGui::BeginChild("##content", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
    if (scrollTop_) {
        ImGui::SetScrollY(0.0f);
        scrollTop_ = false;
    }

    // Fade + slide-up when switching screens.
    if (transition_ < 1.0f) {
        transition_ = animationsEnabled() ? std::min(1.0f, transition_ + ImGui::GetIO().DeltaTime * 4.0f) : 1.0f;
        requestRedraw();
    }
    const float t = easeOutCubic(transition_);

    // Center a column of at most kContentMaxWidth; narrower windows use the full width.
    const float avail = ImGui::GetContentRegionAvail().x;
    const float width = std::min(avail, dp(current_ == ScreenId::Settings || current_ == ScreenId::Accounts
                                               ? 900.0f
                                               : kContentMaxWidth));
    const float indent = (avail - width) * 0.5f;
    ImGui::SetCursorPos({ImGui::GetCursorPosX() + indent, ImGui::GetCursorPosY() + (1.0f - t) * dp(14)});

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.25f + 0.75f * t);
    ImGui::BeginChild("##screen", {width, 0}, ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar);
    screens_[static_cast<size_t>(current_)]->draw(ctx);
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

}  // namespace rl::ui
