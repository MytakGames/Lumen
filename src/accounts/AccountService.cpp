#include "accounts/AccountService.h"

#include <ctime>

#include "accounts/CookieVault.h"
#include "core/AtomicFile.h"
#include "core/Logger.h"

namespace rl::accounts {

AccountService::AccountService(AccountStore& store, api::RobloxApi& api, ThreadPool& pool, MainThreadQueue& main,
                               Notify notify, AliveFlag alive)
    : store_(store), api_(api), pool_(pool), main_(main), notify_(std::move(notify)), alive_(std::move(alive)) {}

SessionState AccountService::state(const std::string& id) const {
    const Account* a = store_.find(id);
    if (!a) return SessionState::Unknown;
    if (!a->hasSession()) return SessionState::NeedsLogin;
    const auto it = states_.find(id);
    return it == states_.end() ? SessionState::Unknown : it->second;
}

std::string AccountService::avatarUrl(const std::string& id) const {
    const Account* a = store_.find(id);
    if (!a) return {};
    const auto it = avatars_.find(a->userId);
    return it == avatars_.end() ? std::string{} : it->second;
}

void AccountService::persist() {
    if (!store_.save()) notify_({Notice::Level::Error, "error.io", {}});
}

void AccountService::addFromCookie(SecureString cookie, std::function<void(ErrorCode, const std::string&)> done) {
    auto shared = std::make_shared<SecureString>(std::move(cookie));
    pool_.post([this, shared, done = std::move(done), alive = alive_] {
        auto user = api_.authenticatedUser(*shared);
        auto protectedBlob = user ? vault::protect(*shared) : Result<std::string>::fail(user.error, user.httpStatus);
        shared->wipe();
        main_.post([this, alive, user = std::move(user), blob = std::move(protectedBlob), done]() mutable {
            if (!*alive) return;
            if (!user) {
                log::warn("accounts", std::string("Login validation failed: ") + errorName(user.error));
                done(user.error, {});
                return;
            }
            if (!blob) {
                done(ErrorCode::CryptoFailed, {});
                return;
            }
            Account* existing = store_.findByUserId(user.value->id);
            if (!existing) {
                Account a;
                a.userId = user.value->id;
                a.username = user.value->name;
                a.displayName = user.value->displayName;
                existing = &store_.add(std::move(a));
                log::info("accounts", "Account added (user id " + std::to_string(existing->userId) + ")");
            } else {
                log::info("accounts", "Session refreshed (user id " + std::to_string(existing->userId) + ")");
            }
            existing->username = user.value->name;
            existing->displayName = user.value->displayName;
            existing->protectedCookie = std::move(*blob.value);
            states_[existing->id] = SessionState::Valid;
            const std::string id = existing->id;
            persist();
            refreshAvatars();
            done(ErrorCode::None, id);
        });
    });
}

void AccountService::validate(const std::string& id) {
    auto cookie = cookieFor(id);
    if (!cookie) {
        if (const Account* a = store_.find(id); a && a->hasSession()) {
            states_[id] = SessionState::Error;
            notify_({Notice::Level::Error, errorKey(cookie.error), {}});
        }
        return;
    }
    states_[id] = SessionState::Checking;
    auto shared = std::make_shared<SecureString>(std::move(*cookie.value));
    pool_.post([this, id, shared, alive = alive_] {
        auto user = api_.authenticatedUser(*shared);
        shared->wipe();
        main_.post([this, id, alive, error = user.error] {
            if (!*alive) return;
            if (!store_.find(id)) return;
            if (error == ErrorCode::None) {
                states_[id] = SessionState::Valid;
            } else if (error == ErrorCode::Unauthorized) {
                states_[id] = SessionState::Expired;
            } else {
                states_[id] = SessionState::Error;
            }
        });
    });
}

void AccountService::validateAll() {
    for (const auto& a : store_.all()) {
        if (a.hasSession() && a.enabled) validate(a.id);
    }
}

void AccountService::refreshAvatars() {
    std::vector<std::int64_t> ids;
    for (const auto& a : store_.all()) {
        if (a.userId && !avatars_.count(a.userId)) ids.push_back(a.userId);
    }
    if (ids.empty()) return;
    pool_.post([this, ids, alive = alive_] {
        auto urls = api_.userHeadshots(ids);
        if (!urls) return;
        main_.post([this, alive, map = std::move(*urls.value)] {
            if (!*alive) return;
            for (const auto& [uid, url] : map) avatars_[uid] = url;
        });
    });
}

void AccountService::setAlias(const std::string& id, const std::string& alias) {
    if (Account* a = store_.find(id)) {
        a->alias = alias;
        persist();
    }
}

void AccountService::setColor(const std::string& id, int colorIndex) {
    if (Account* a = store_.find(id)) {
        a->colorIndex = colorIndex;
        persist();
    }
}

void AccountService::setEnabled(const std::string& id, bool enabled) {
    if (Account* a = store_.find(id)) {
        a->enabled = enabled;
        persist();
    }
}

void AccountService::move(const std::string& id, int delta) {
    if (store_.move(id, delta)) persist();
}

void AccountService::remove(const std::string& id) {
    if (store_.remove(id)) {
        states_.erase(id);
        persist();
        log::info("accounts", "Account removed");
    }
}

void AccountService::markUsed(const std::string& id) {
    if (Account* a = store_.find(id)) {
        a->lastUsedAt = std::time(nullptr);
        persist();
    }
}

void AccountService::markExpired(const std::string& id) { states_[id] = SessionState::Expired; }

void AccountService::removeAll() {
    store_.clear();
    states_.clear();
    persist();
}

bool AccountService::exportTo(const std::filesystem::path& file) const {
    return fs::writeAtomic(file, store_.exportMetadata());
}

int AccountService::importFrom(const std::filesystem::path& file) {
    const auto text = fs::readAll(file);
    if (!text) return -1;
    const int added = store_.importMetadata(*text);
    if (added > 0) persist();
    return added;
}

Result<SecureString> AccountService::cookieFor(const std::string& id) const {
    const Account* a = store_.find(id);
    if (!a || !a->hasSession()) return Result<SecureString>::fail(ErrorCode::Unauthorized);
    return vault::unprotect(a->protectedCookie);
}

}  // namespace rl::accounts
