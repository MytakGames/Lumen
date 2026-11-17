#pragma once

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "settings/Settings.h"  // LoadStatus

namespace rl::accounts {

struct Account {
    std::string id;           // local identifier (UUID)
    std::int64_t userId = 0;  // Roblox user id (0 = imported, never logged in)
    std::string username;
    std::string displayName;
    std::string alias;        // optional local nickname
    int colorIndex = -1;      // index into the UI accent swatches, -1 = automatic
    bool enabled = true;
    std::string protectedCookie;  // DPAPI blob (Base64). Empty = needs login.
    std::time_t addedAt = 0;
    std::time_t lastUsedAt = 0;

    const std::string& label() const { return alias.empty() ? (displayName.empty() ? username : displayName) : alias; }
    bool hasSession() const { return !protectedCookie.empty(); }
};

// Ordered list of accounts persisted to accounts.json (atomic writes).
// Pure data + persistence; no network, no UI. Order in the vector is the
// user-defined order.
class AccountStore {
public:
    explicit AccountStore(std::filesystem::path file) : file_(std::move(file)) {}

    settings::LoadStatus load();  // corrupt -> backed up, list emptied
    bool save() const;

    const std::vector<Account>& all() const { return accounts_; }
    Account* find(const std::string& id);
    const Account* find(const std::string& id) const;
    Account* findByUserId(std::int64_t userId);

    Account& add(Account account);  // assigns id/addedAt when missing
    bool remove(const std::string& id);
    bool move(const std::string& id, int delta);  // -1 up, +1 down
    void clear() { accounts_.clear(); }

    // Metadata only: cookies are never exported.
    std::string exportMetadata() const;
    // Adds accounts not already present (matched by userId, else username).
    // Returns the number added, or -1 if the document is invalid.
    int importMetadata(const std::string& json);

    static std::string toJson(const std::vector<Account>& accounts, bool includeSecrets);
    static std::vector<Account> fromJson(const std::string& text);  // throws on malformed input

private:
    std::filesystem::path file_;
    std::vector<Account> accounts_;
};

}  // namespace rl::accounts
