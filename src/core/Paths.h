#pragma once

#include <filesystem>

// Well-known locations. All app data lives under %LOCALAPPDATA%\Lumen
// (local, not roaming: DPAPI-protected cookies are bound to this machine anyway).
// An existing %LOCALAPPDATA%\RobloxLauncher folder (pre-rename) is moved there once.
namespace rl::paths {

std::filesystem::path dataDir();      // created on first use
std::filesystem::path logsDir();
std::filesystem::path cacheDir();
std::filesystem::path settingsFile();
std::filesystem::path executableDir();
std::filesystem::path executablePath();

// Tests redirect all data to a temporary folder.
void setDataDirOverride(const std::filesystem::path& dir);

}  // namespace rl::paths
