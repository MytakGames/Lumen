#include "accounts/AccountStore.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "core/AtomicFile.h"
#include "core/Logger.h"
#include "core/StringUtil.h"

namespace rl::accounts {
namespace {

using json = nlohmann::json;
constexpr int kSchemaVersion = 1;

template <class T>
T value(const json& j, const char* key, T fallback) {
    const auto it = j.find(key);
    if (it == j.end() || it->is_null()) return fallback;
    try {
        return it->get<T>();
    } catch (...) {
        return fallback;
    }
}

}  // namespace

std::string AccountStore::toJson(const std::vector<Account>& accounts, bool includeSecrets) {
    json list = json::array();
    for (const auto& a : accounts) {
        json e = {
            {"id", a.id},
            {"userId", a.userId},
            {"username", a.username},
            {"displayName", a.displayName},
            {"alias", a.alias},
            {"colorIndex", a.colorIndex},
            {"enabled", a.enabled},
            {"addedAt", static_cast<std::int64_t>(a.addedAt)},
            {"lastUsedAt", static_cast<std::int64_t>(a.lastUsedAt)},
        };
        if (includeSecrets && !a.protectedCookie.empty()) e["protectedCookie"] = a.protectedCookie;
        list.push_back(std::move(e));
    }
    return json{{"version", kSchemaVersion}, {"accounts", std::move(list)}}.dump(2);
}

std::vector<Account> AccountStore::fromJson(const std::string& text) {
    const json j = json::parse(text);
    if (!j.is_object() || !j.contains("accounts") || !j["accounts"].is_array()) {
        throw std::runtime_error("accounts document has no 'accounts' array");
    }
    std::vector<Account> out;
    for (const auto& e : j["accounts"]) {
        if (!e.is_object()) continue;
        Account a;
        a.id = value<std::string>(e, "id", "");
        a.userId = value<std::int64_t>(e, "userId", 0);
        a.username = value<std::string>(e, "username", "");
        a.displayName = value<std::string>(e, "displayName", "");
        a.alias = value<std::string>(e, "alias", "");
        a.colorIndex = value<int>(e, "colorIndex", -1);
        a.enabled = value<bool>(e, "enabled", true);
        a.protectedCookie = value<std::string>(e, "protectedCookie", "");
        a.addedAt = static_cast<std::time_t>(value<std::int64_t>(e, "addedAt", 0));
        a.lastUsedAt = static_cast<std::time_t>(value<std::int64_t>(e, "lastUsedAt", 0));
        if (a.id.empty()) a.id = str::uuid4();
        if (a.username.empty() && a.alias.empty() && a.displayName.empty()) continue;
        out.push_back(std::move(a));
    }
    return out;
}

settings::LoadStatus AccountStore::load() {
    accounts_.clear();
    const auto text = fs::readAll(file_);
    if (!text) return settings::LoadStatus::Missing;
    try {
        accounts_ = fromJson(*text);
        return settings::LoadStatus::Ok;
    } catch (const std::exception& e) {
        log::error("accounts", std::string("Corrupt accounts file: ") + e.what());
        fs::backupCorrupt(file_);
        return settings::LoadStatus::Corrupt;
    }
}

bool AccountStore::save() const { return fs::writeAtomic(file_, toJson(accounts_, true)); }

Account* AccountStore::find(const std::string& id) {
    const auto it = std::find_if(accounts_.begin(), accounts_.end(), [&](const Account& a) { return a.id == id; });
    return it == accounts_.end() ? nullptr : &*it;
}

const Account* AccountStore::find(const std::string& id) const {
    const auto it = std::find_if(accounts_.begin(), accounts_.end(), [&](const Account& a) { return a.id == id; });
    return it == accounts_.end() ? nullptr : &*it;
}

Account* AccountStore::findByUserId(std::int64_t userId) {
    if (userId == 0) return nullptr;
    const auto it =
        std::find_if(accounts_.begin(), accounts_.end(), [&](const Account& a) { return a.userId == userId; });
    return it == accounts_.end() ? nullptr : &*it;
}

Account& AccountStore::add(Account account) {
    if (account.id.empty()) account.id = str::uuid4();
    if (account.addedAt == 0) account.addedAt = std::time(nullptr);
    accounts_.push_back(std::move(account));
    return accounts_.back();
}

bool AccountStore::remove(const std::string& id) {
    const auto it = std::find_if(accounts_.begin(), accounts_.end(), [&](const Account& a) { return a.id == id; });
    if (it == accounts_.end()) return false;
    if (!it->protectedCookie.empty()) {
        // Best effort: the encrypted blob is useless elsewhere, but don't leave copies around.
        std::fill(it->protectedCookie.begin(), it->protectedCookie.end(), '\0');
    }
    accounts_.erase(it);
    return true;
}

bool AccountStore::move(const std::string& id, int delta) {
    const auto it = std::find_if(accounts_.begin(), accounts_.end(), [&](const Account& a) { return a.id == id; });
    if (it == accounts_.end()) return false;
    const auto index = static_cast<long long>(it - accounts_.begin());
    const long long target = index + delta;
    if (target < 0 || target >= static_cast<long long>(accounts_.size())) return false;
    std::swap(accounts_[static_cast<size_t>(index)], accounts_[static_cast<size_t>(target)]);
    return true;
}

std::string AccountStore::exportMetadata() const { return toJson(accounts_, false); }

int AccountStore::importMetadata(const std::string& text) {
    std::vector<Account> incoming;
    try {
        incoming = fromJson(text);
    } catch (const std::exception& e) {
        log::warn("accounts", std::string("Import rejected: ") + e.what());
        return -1;
    }
    int added = 0;
    for (auto& a : incoming) {
        a.protectedCookie.clear();  // never trust/import secrets
        const bool exists = std::any_of(accounts_.begin(), accounts_.end(), [&](const Account& b) {
            return (a.userId != 0 && a.userId == b.userId) ||
                   (a.userId == 0 && !a.username.empty() && a.username == b.username);
        });
        if (exists) continue;
        a.id = str::uuid4();
        add(std::move(a));
        ++added;
    }
    return added;
}

}  // namespace rl::accounts
