#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "accounts/AccountStore.h"
#include "core/Error.h"
#include "core/Notice.h"
#include "core/SecureString.h"
#include "core/ThreadPool.h"
#include "robloxapi/RobloxApi.h"

namespace rl::accounts {

enum class SessionState { Unknown, Checking, Valid, Expired, NeedsLogin, Error };

// Account use cases on top of AccountStore. Must be used from the UI thread;
// network work runs on the pool and results come back via MainThreadQueue.
class AccountService {
public:
    AccountService(AccountStore& store, api::RobloxApi& api, ThreadPool& pool, MainThreadQueue& main, Notify notify,
                   AliveFlag alive);

    const std::vector<Account>& accounts() const { return store_.all(); }
    const Account* find(const std::string& id) const { return store_.find(id); }
    SessionState state(const std::string& id) const;
    std::string avatarUrl(const std::string& id) const;

    // Validates the cookie, fetches the profile, then adds a new account or
    // refreshes the session of the existing one with the same Roblox user.
    // `done` receives the account id on success.
    void addFromCookie(SecureString cookie, std::function<void(ErrorCode, const std::string&)> done);

    void validate(const std::string& id);
    void validateAll();
    void refreshAvatars();

    void setAlias(const std::string& id, const std::string& alias);
    void setColor(const std::string& id, int colorIndex);
    void setEnabled(const std::string& id, bool enabled);
    void move(const std::string& id, int delta);
    void remove(const std::string& id);
    void markUsed(const std::string& id);
    void markExpired(const std::string& id);
    void removeAll();

    bool exportTo(const std::filesystem::path& file) const;
    int importFrom(const std::filesystem::path& file);  // -1 on invalid file

    // Decrypts the account's session cookie. UI thread only.
    Result<SecureString> cookieFor(const std::string& id) const;

private:
    void persist();

    AccountStore& store_;
    api::RobloxApi& api_;
    ThreadPool& pool_;
    MainThreadQueue& main_;
    Notify notify_;
    AliveFlag alive_;  // guards callbacks posted after shutdown
    std::map<std::string, SessionState> states_;
    std::map<std::int64_t, std::string> avatars_;  // userId -> image URL
};

}  // namespace rl::accounts
