#pragma once

#include <windows.h>

#include <filesystem>
#include <optional>
#include <string>

namespace rl::app {

std::optional<std::filesystem::path> saveJsonDialog(HWND owner, const std::wstring& title, const std::wstring& defaultName);
std::optional<std::filesystem::path> openJsonDialog(HWND owner, const std::wstring& title);

}  // namespace rl::app
