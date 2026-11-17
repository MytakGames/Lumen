#pragma once

#include <ctime>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rl::i18n {

// Named placeholders: {"name", "Mario"} replaces "{name}".
using Args = std::vector<std::pair<std::string, std::string>>;

// Loads flat or nested JSON string tables (one file per language, named
// <code>.json) and resolves keys for the active language, falling back to
// English and finally to the key itself. Switching language takes effect on
// the next lookup, so the UI updates on the next frame without a restart.
// Not thread-safe: use from the UI thread.
class Localizer {
public:
    static constexpr const char* kFallback = "en";

    bool loadDirectory(const std::filesystem::path& dir);
    bool loadFromString(const std::string& code, const std::string& jsonText);

    void setLanguage(const std::string& code);
    const std::string& language() const { return current_; }
    bool hasLanguage(const std::string& code) const { return tables_.count(code) != 0; }
    std::vector<std::string> availableLanguages() const;
    std::string languageDisplayName(const std::string& code) const;  // from "_meta.name"

    // The returned reference stays valid until the next load*() call.
    const std::string& tr(std::string_view key) const;
    const char* c(std::string_view key) const { return tr(key).c_str(); }

    std::string format(std::string_view key, const Args& args) const;
    // Uses "<key>.one" / "<key>.other" and injects {count} as a formatted number.
    std::string plural(std::string_view key, long long n, Args args = {}) const;

    std::string formatNumber(long long value) const;
    std::string formatDateTime(std::time_t t) const;
    std::string formatDate(std::time_t t) const;
    // "just now", "5 minutes ago", ... computed against `now`.
    std::string formatRelative(std::time_t then, std::time_t now) const;

    std::vector<std::string> keys(const std::string& code) const;

    // "it" when the Windows UI language is Italian, otherwise "en".
    static std::string detectSystemLanguage();

private:
    using Table = std::unordered_map<std::string, std::string>;
    const std::string* lookup(const std::string& code, std::string_view key) const;

    std::map<std::string, Table> tables_;
    std::string current_ = kFallback;
    mutable std::set<std::string, std::less<>> missing_;  // stable storage for unknown keys
};

std::string applyArgs(std::string text, const Args& args);

}  // namespace rl::i18n
