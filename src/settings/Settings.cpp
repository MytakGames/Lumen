#include "settings/Settings.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <type_traits>

#include "core/AtomicFile.h"
#include "core/Logger.h"

namespace rl::settings {
namespace {

using json = nlohmann::json;
constexpr int kSchemaVersion = 1;

template <class T>
void read(const json& j, const char* key, T& out) {
    const auto it = j.find(key);
    if (it == j.end()) return;
    if constexpr (std::is_same_v<T, bool>) {
        if (it->is_boolean()) out = it->get<bool>();
    } else if constexpr (std::is_integral_v<T>) {
        if (it->is_number_integer()) out = it->get<T>();
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (it->is_string()) out = it->get<std::string>();
    }
}

}  // namespace

void Settings::clamp() {
    using namespace limits;
    if (language != "it" && language != "en") language.clear();
    maxInstances = std::clamp(maxInstances, kMaxInstancesMin, kMaxInstancesMax);
    instanceLaunchDelayMs = std::clamp(instanceLaunchDelayMs, kLaunchDelayMin, kLaunchDelayMax);
    thumbnailCacheMaxMb = std::clamp(thumbnailCacheMaxMb, kThumbCacheMin, kThumbCacheMax);
    recentGamesMax = std::clamp(recentGamesMax, kRecentMin, kRecentMax);
    apiCacheTtlMinutes = std::clamp(apiCacheTtlMinutes, kApiTtlMin, kApiTtlMax);
    windowWidth = std::clamp(windowWidth, 900, 7680);
    windowHeight = std::clamp(windowHeight, 600, 4320);
}

const char* themeToString(Theme t) {
    switch (t) {
        case Theme::Dark: return "dark";
        case Theme::Light: return "light";
        case Theme::System: return "system";
    }
    return "dark";
}

Theme themeFromString(const std::string& s) {
    if (s == "light") return Theme::Light;
    if (s == "system") return Theme::System;
    return Theme::Dark;
}

std::string toJson(const Settings& s) {
    json j = {
        {"version", kSchemaVersion},
        {"language", s.language},
        {"theme", themeToString(s.theme)},
        {"animations", s.animations},
        {"restoreLastScreen", s.restoreLastScreen},
        {"lastScreen", s.lastScreen},
        {"launchWithWindows", s.launchWithWindows},
        {"startMinimizedToTray", s.startMinimizedToTray},
        {"closeToTray", s.closeToTray},
        {"multiInstanceEnabled", s.multiInstanceEnabled},
        {"maxInstances", s.maxInstances},
        {"instanceLaunchDelayMs", s.instanceLaunchDelayMs},
        {"thumbnailCacheMaxMb", s.thumbnailCacheMaxMb},
        {"recentGamesMax", s.recentGamesMax},
        {"apiCacheTtlMinutes", s.apiCacheTtlMinutes},
        {"windowWidth", s.windowWidth},
        {"windowHeight", s.windowHeight},
        {"windowMaximized", s.windowMaximized},
    };
    return j.dump(2);
}

Settings fromJson(const std::string& text) {
    const json j = json::parse(text);
    if (!j.is_object()) throw std::runtime_error("settings root is not an object");

    Settings s;
    read(j, "language", s.language);
    std::string theme = themeToString(s.theme);
    read(j, "theme", theme);
    s.theme = themeFromString(theme);
    read(j, "animations", s.animations);
    read(j, "restoreLastScreen", s.restoreLastScreen);
    read(j, "lastScreen", s.lastScreen);
    read(j, "launchWithWindows", s.launchWithWindows);
    read(j, "startMinimizedToTray", s.startMinimizedToTray);
    read(j, "closeToTray", s.closeToTray);
    read(j, "multiInstanceEnabled", s.multiInstanceEnabled);
    read(j, "maxInstances", s.maxInstances);
    read(j, "instanceLaunchDelayMs", s.instanceLaunchDelayMs);
    read(j, "thumbnailCacheMaxMb", s.thumbnailCacheMaxMb);
    read(j, "recentGamesMax", s.recentGamesMax);
    read(j, "apiCacheTtlMinutes", s.apiCacheTtlMinutes);
    read(j, "windowWidth", s.windowWidth);
    read(j, "windowHeight", s.windowHeight);
    read(j, "windowMaximized", s.windowMaximized);
    s.clamp();
    return s;
}

LoadResult load(const std::filesystem::path& path) {
    LoadResult result;
    const auto text = fs::readAll(path);
    if (!text) {
        result.status = LoadStatus::Missing;
        log::info("settings", "No settings file, using defaults");
        return result;
    }
    try {
        result.settings = fromJson(*text);
        result.status = LoadStatus::Ok;
    } catch (const std::exception& e) {
        log::error("settings", std::string("Corrupt settings file: ") + e.what());
        result.status = LoadStatus::Corrupt;
        result.backupPath = fs::backupCorrupt(path);
        result.settings = Settings{};
    }
    return result;
}

bool save(const std::filesystem::path& path, const Settings& s) {
    return fs::writeAtomic(path, toJson(s));
}

}  // namespace rl::settings
