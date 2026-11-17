#pragma once

#include <filesystem>
#include <string>

namespace rl::settings {

enum class Theme { Dark, Light, System };

struct Settings {
    // General
    std::string language;  // "it" / "en"; empty = detect from the OS on first run
    Theme theme = Theme::Dark;
    bool animations = true;

    // Startup
    bool restoreLastScreen = true;
    std::string lastScreen = "home";
    bool launchWithWindows = false;
    bool startMinimizedToTray = false;
    bool closeToTray = true;

    // Instances
    bool multiInstanceEnabled = false;  // opt-in, see README "Multi-istanza"
    int maxInstances = 4;
    int instanceLaunchDelayMs = 3000;

    // Data & cache
    int thumbnailCacheMaxMb = 200;
    int recentGamesMax = 20;
    int apiCacheTtlMinutes = 10;

    // Window
    int windowWidth = 1280;
    int windowHeight = 800;
    bool windowMaximized = false;

    // Forces every value into its valid range.
    void clamp();
};

// Valid ranges, shared by clamp() and the settings UI.
namespace limits {
inline constexpr int kMaxInstancesMin = 1, kMaxInstancesMax = 10;
inline constexpr int kLaunchDelayMin = 0, kLaunchDelayMax = 30000;
inline constexpr int kThumbCacheMin = 16, kThumbCacheMax = 2048;
inline constexpr int kRecentMin = 5, kRecentMax = 100;
inline constexpr int kApiTtlMin = 1, kApiTtlMax = 240;
}  // namespace limits

enum class LoadStatus { Ok, Missing, Corrupt };

struct LoadResult {
    Settings settings;
    LoadStatus status = LoadStatus::Missing;
    std::filesystem::path backupPath;  // set when status == Corrupt
};

std::string toJson(const Settings& s);
// Throws std::exception on malformed input; unknown or mistyped
// fields fall back to their defaults.
Settings fromJson(const std::string& text);

// Missing file -> defaults. Unparsable file -> backup + defaults.
LoadResult load(const std::filesystem::path& path);
bool save(const std::filesystem::path& path, const Settings& s);

const char* themeToString(Theme t);
Theme themeFromString(const std::string& s);

}  // namespace rl::settings
