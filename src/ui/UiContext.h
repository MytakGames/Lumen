#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "i18n/Localizer.h"
#include "settings/Settings.h"

namespace rl::accounts {
class AccountService;
class QuickLoginFlow;
}
namespace rl::instances {
class InstanceManager;
}
namespace rl::games {
class GameService;
}

namespace rl::ui {

class TextureCache;

// What screens may read and the actions they may request. Screens hold no
// business logic: they render service state and call these hooks, which the
// application layer implements.
struct UiContext {
    UiContext(settings::Settings& s, const i18n::Localizer& l) : settings(s), tr(l) {}

    settings::Settings& settings;
    const i18n::Localizer& tr;

    accounts::AccountService* accounts = nullptr;
    accounts::QuickLoginFlow* login = nullptr;
    instances::InstanceManager* instances = nullptr;
    games::GameService* games = nullptr;
    TextureCache* textures = nullptr;
    std::uint64_t logo = 0;  // ImTextureID of the app logo, 0 = draw the vector fallback

    std::function<void()> settingsChanged;  // apply + persist after an edit
    std::function<void()> resetAll;
    std::function<void()> clearCache;
    std::function<void()> clearHistory;
    std::function<void()> openDataFolder;
    std::function<void()> openReadme;  // empty when no README is shipped
    std::function<void(const char*)> openUrl;

    std::function<void()> addAccount;                         // starts the code + browser sign-in
    std::function<void(const std::string&)> reloginAccount;  // refresh an expired session
    std::function<bool()> loginInProgress;
    std::function<void()> exportAccounts;
    std::function<void()> importAccounts;

    std::function<void(std::int64_t universeId)> openGame;
    std::function<void()> goBack;
    // Launches a game; handles confirmations and error messages.
    std::function<void(const std::string& accountId, std::int64_t universeId)> launchGame;
    std::function<std::string()> defaultAccountId;  // first enabled account with a session
};

class Screen {
public:
    virtual ~Screen() = default;
    virtual void onShow(UiContext&) {}
    virtual void draw(UiContext& ctx) = 0;
};

}  // namespace rl::ui
