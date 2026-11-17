#pragma once

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rl::instances {

inline constexpr char kDownloadUrl[] = "https://www.roblox.com/download";
inline constexpr wchar_t kPlayerProcess[] = L"RobloxPlayerBeta.exe";

struct RobloxInstall {
    bool installed = false;
    std::filesystem::path protocolHandler;  // what roblox-player: opens (Roblox or e.g. Bloxstrap)
};

// Roblox is considered installed when the roblox-player: protocol is registered
// for the current user (or machine) and its handler exists on disk.
RobloxInstall locateRoblox();

// %LOCALAPPDATA%\Roblox\LocalStorage\RobloxCookies.dat (shared by all clients).
std::filesystem::path robloxCookieFile();

// PIDs of running RobloxPlayerBeta.exe processes.
std::vector<DWORD> runningPlayerPids();

// Builds the roblox-player: URI that joins `placeId` with a one-time auth ticket.
// Pure function (unit tested).
std::string buildLaunchUri(const std::string& ticket, std::int64_t placeId, std::int64_t launchTimeMs,
                           const std::string& browserTrackerId, const std::string& joinAttemptId,
                           const std::string& locale);

// Extracts the executable path from a shell "open" command line.
std::wstring exeFromCommand(const std::wstring& command);

}  // namespace rl::instances
