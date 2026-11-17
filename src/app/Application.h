#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "accounts/AccountService.h"
#include "accounts/AccountStore.h"
#include "accounts/QuickLoginFlow.h"
#include "app/TrayIcon.h"
#include "app/Window.h"
#include "core/Notice.h"
#include "core/ThreadPool.h"
#include "games/GameList.h"
#include "games/GameService.h"
#include "i18n/Localizer.h"
#include "instances/InstanceManager.h"
#include "robloxapi/RobloxApi.h"
#include "settings/Settings.h"
#include "ui/MainLayout.h"
#include "ui/TextureCache.h"

namespace rl::app {

struct LaunchOptions {
    bool autostart = false;  // started by Windows at sign-in
};

// Composition root: owns the services, the window and the UI, and implements
// the UiContext actions. Business rules live in the services; this layer only
// sequences them and turns outcomes into user-facing messages.
class Application {
public:
    explicit Application(LaunchOptions options);
    ~Application();
    int run();

private:
    bool initialize();
    void createServices();
    void initImGui();
    void bindContext();
    void rebuildDpiResources();
    void frame();
    void shutdown();

    bool onWindowMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& out);
    void showTrayMenu();

    void notify(const Notice& n);
    void onSettingsChanged();
    void applySettings(bool force);
    void saveSettingsIfDue(bool force);
    void updateWindowChrome();

    void startLogin(const std::string& expectedAccountId);
    void launchGame(const std::string& accountId, std::int64_t universeId);
    void doLaunch(instances::LaunchRequest req);
    void resetAll();
    void clearCache();
    void exportAccounts();
    void importAccounts();
    std::string defaultAccountId() const;

    LaunchOptions options_;
    i18n::Localizer localizer_;
    settings::Settings settings_;
    settings::Settings applied_;  // last state pushed to OS/theme, for diffing

    // Destruction order matters: workers are joined in shutdown() before the
    // services and queue they post to are destroyed.
    AliveFlag alive_ = makeAliveFlag();
    MainThreadQueue mainQueue_;
    std::unique_ptr<api::RobloxApi> api_;
    std::unique_ptr<accounts::AccountStore> accountStore_;
    std::unique_ptr<games::GameList> recents_;
    std::unique_ptr<games::GameList> favorites_;
    std::unique_ptr<accounts::AccountService> accounts_;
    std::unique_ptr<accounts::QuickLoginFlow> quickLogin_;
    std::unique_ptr<games::GameService> games_;
    std::unique_ptr<instances::InstanceManager> instances_;
    std::unique_ptr<ui::TextureCache> textures_;
    ComPtr<ID3D11ShaderResourceView> logo_;
    std::unique_ptr<ThreadPool> pool_;

    Window window_;
    TrayIcon tray_;
    bool trayHintShown_ = false;

    ui::MainLayout layout_;
    std::unique_ptr<ui::UiContext> ctx_;

    bool imguiReady_ = false;
    bool settingsDirty_ = false;
    double lastEdit_ = 0.0;
    int framesToRender_ = 3;
    std::vector<Notice> startupNotices_;
};

}  // namespace rl::app
