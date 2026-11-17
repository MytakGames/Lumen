#include "i18n/Localizer.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <nlohmann/json.hpp>

#include "core/AtomicFile.h"
#include "core/Logger.h"
#include "core/StringUtil.h"

namespace rl::i18n {
namespace {

using json = nlohmann::json;

void flatten(const json& node, const std::string& prefix, std::unordered_map<std::string, std::string>& out) {
    for (auto it = node.begin(); it != node.end(); ++it) {
        const std::string key = prefix.empty() ? it.key() : prefix + "." + it.key();
        if (it->is_object()) {
            flatten(*it, key, out);
        } else if (it->is_string()) {
            out[key] = it->get<std::string>();
        } else {
            log::warn("i18n", "Ignoring non-string value for key " + key);
        }
    }
}

std::tm toLocal(std::time_t t) {
    std::tm tm{};
    localtime_s(&tm, &t);
    return tm;
}

}  // namespace

std::string applyArgs(std::string text, const Args& args) {
    for (const auto& [name, value] : args) text = str::replaceAll(std::move(text), "{" + name + "}", value);
    return text;
}

bool Localizer::loadFromString(const std::string& code, const std::string& jsonText) {
    try {
        const json j = json::parse(jsonText);
        if (!j.is_object()) return false;
        Table table;
        flatten(j, "", table);
        tables_[code] = std::move(table);
        return true;
    } catch (const std::exception& e) {
        log::error("i18n", "Invalid locale '" + code + "': " + e.what());
        return false;
    }
}

bool Localizer::loadDirectory(const std::filesystem::path& dir) {
    std::error_code ec;
    bool any = false;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        const auto text = fs::readAll(entry.path());
        if (text && loadFromString(entry.path().stem().string(), *text)) any = true;
    }
    if (!any) log::error("i18n", "No locale files found in " + dir.string());
    return any && hasLanguage(kFallback);
}

void Localizer::setLanguage(const std::string& code) {
    current_ = hasLanguage(code) ? code : kFallback;
    log::info("i18n", "Language set to " + current_);
}

std::vector<std::string> Localizer::availableLanguages() const {
    std::vector<std::string> out;
    for (const auto& [code, table] : tables_) out.push_back(code);
    return out;
}

std::string Localizer::languageDisplayName(const std::string& code) const {
    if (const auto* name = lookup(code, "_meta.name")) return *name;
    return code;
}

const std::string* Localizer::lookup(const std::string& code, std::string_view key) const {
    const auto t = tables_.find(code);
    if (t == tables_.end()) return nullptr;
    const auto it = t->second.find(std::string(key));
    return it == t->second.end() ? nullptr : &it->second;
}

const std::string& Localizer::tr(std::string_view key) const {
    if (const auto* s = lookup(current_, key)) return *s;
    if (const auto* s = lookup(kFallback, key)) return *s;
    auto [it, inserted] = missing_.emplace(key);
    if (inserted) log::warn("i18n", "Missing translation key: " + *it);
    return *it;
}

std::string Localizer::format(std::string_view key, const Args& args) const { return applyArgs(tr(key), args); }

std::string Localizer::plural(std::string_view key, long long n, Args args) const {
    // Both Italian and English: "one" only for exactly 1.
    const std::string form = std::string(key) + (n == 1 ? ".one" : ".other");
    args.emplace_back("count", formatNumber(n));
    return applyArgs(tr(form), args);
}

std::string Localizer::formatNumber(long long value) const {
    const char sep = current_ == "it" ? '.' : ',';
    const bool negative = value < 0;
    unsigned long long v = negative ? 0ULL - static_cast<unsigned long long>(value) : static_cast<unsigned long long>(value);
    const std::string digits = std::to_string(v);
    std::string out;
    out.reserve(digits.size() + digits.size() / 3 + 1);
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i != 0 && (digits.size() - i) % 3 == 0) out.push_back(sep);
        out.push_back(digits[i]);
    }
    return negative ? "-" + out : out;
}

std::string Localizer::formatDate(std::time_t t) const {
    const std::tm tm = toLocal(t);
    char buf[32];
    if (current_ == "it")
        std::snprintf(buf, sizeof(buf), "%02d/%02d/%04d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
    else
        std::snprintf(buf, sizeof(buf), "%02d/%02d/%04d", tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900);
    return buf;
}

std::string Localizer::formatDateTime(std::time_t t) const {
    const std::tm tm = toLocal(t);
    char buf[32];
    if (current_ == "it") {
        std::snprintf(buf, sizeof(buf), " %02d:%02d", tm.tm_hour, tm.tm_min);
    } else {
        const int h12 = tm.tm_hour % 12 == 0 ? 12 : tm.tm_hour % 12;
        std::snprintf(buf, sizeof(buf), " %d:%02d %s", h12, tm.tm_min, tm.tm_hour < 12 ? "AM" : "PM");
    }
    return formatDate(t) + buf;
}

std::string Localizer::formatRelative(std::time_t then, std::time_t now) const {
    const long long secs = static_cast<long long>(now - then);
    if (secs < 60) return tr("time.just_now");
    const long long mins = secs / 60;
    if (mins < 60) return plural("time.minutes_ago", mins);
    const long long hours = mins / 60;
    if (hours < 24) return plural("time.hours_ago", hours);
    const long long days = hours / 24;
    if (days < 30) return plural("time.days_ago", days);
    return formatDate(then);
}

std::vector<std::string> Localizer::keys(const std::string& code) const {
    std::vector<std::string> out;
    const auto t = tables_.find(code);
    if (t == tables_.end()) return out;
    for (const auto& [k, v] : t->second) out.push_back(k);
    std::sort(out.begin(), out.end());
    return out;
}

std::string Localizer::detectSystemLanguage() {
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_ITALIAN ? "it" : "en";
}

}  // namespace rl::i18n
