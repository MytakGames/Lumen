#pragma once

#include <filesystem>

// "Start with Windows" via HKCU\Software\Microsoft\Windows\CurrentVersion\Run.
// Per-user, no admin rights required.
namespace rl::autostart {

inline constexpr wchar_t kLaunchArg[] = L"--autostart";

bool isEnabled();
bool setEnabled(bool enabled, const std::filesystem::path& exePath);

}  // namespace rl::autostart
