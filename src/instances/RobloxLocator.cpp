#include "instances/RobloxLocator.h"

#include <shlobj.h>
#include <tlhelp32.h>

#include <system_error>

#include "core/Logger.h"
#include "core/StringUtil.h"

namespace rl::instances {
namespace {

std::wstring readCommand(HKEY root, const wchar_t* subKey) {
    DWORD bytes = 0;
    if (RegGetValueW(root, subKey, nullptr, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, nullptr, &bytes) !=
            ERROR_SUCCESS ||
        bytes == 0)
        return {};
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(root, subKey, nullptr, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, value.data(), &bytes) !=
        ERROR_SUCCESS)
        return {};
    value.resize(wcsnlen(value.c_str(), value.size()));
    return value;
}

}  // namespace

std::wstring exeFromCommand(const std::wstring& command) {
    std::wstring c = command;
    while (!c.empty() && c.front() == L' ') c.erase(c.begin());
    if (c.empty()) return {};
    if (c.front() == L'"') {
        const auto end = c.find(L'"', 1);
        return end == std::wstring::npos ? c.substr(1) : c.substr(1, end - 1);
    }
    const auto lower = str::toWide(str::toLower(str::toUtf8(c)));
    const auto exe = lower.find(L".exe");
    if (exe != std::wstring::npos) return c.substr(0, exe + 4);
    const auto space = c.find(L' ');
    return space == std::wstring::npos ? c : c.substr(0, space);
}

RobloxInstall locateRoblox() {
    RobloxInstall out;
    constexpr wchar_t kKey[] = L"Software\\Classes\\roblox-player\\shell\\open\\command";
    std::wstring command = readCommand(HKEY_CURRENT_USER, kKey);
    if (command.empty()) command = readCommand(HKEY_CLASSES_ROOT, L"roblox-player\\shell\\open\\command");
    if (command.empty()) {
        log::warn("roblox", "roblox-player protocol is not registered");
        return out;
    }
    out.protocolHandler = exeFromCommand(command);
    std::error_code ec;
    out.installed = !out.protocolHandler.empty() && std::filesystem::exists(out.protocolHandler, ec);
    if (!out.installed) log::warn("roblox", "Protocol handler missing on disk: " + out.protocolHandler.string());
    return out;
}

std::filesystem::path robloxCookieFile() {
    PWSTR raw = nullptr;
    std::filesystem::path out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw))) {
        out = std::filesystem::path(raw) / L"Roblox" / L"LocalStorage" / L"RobloxCookies.dat";
    }
    CoTaskMemFree(raw);
    return out;
}

std::vector<DWORD> runningPlayerPids() {
    std::vector<DWORD> pids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return pids;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, kPlayerProcess) == 0) pids.push_back(pe.th32ProcessID);
    }
    CloseHandle(snap);
    return pids;
}

std::string buildLaunchUri(const std::string& ticket, std::int64_t placeId, std::int64_t launchTimeMs,
                           const std::string& browserTrackerId, const std::string& joinAttemptId,
                           const std::string& locale) {
    const std::string placeLauncher =
        "https://www.roblox.com/Game/PlaceLauncher.ashx?request=RequestGame&browserTrackerId=" + browserTrackerId +
        "&placeId=" + std::to_string(placeId) + "&isPlayTogetherGame=false&joinAttemptId=" + joinAttemptId +
        "&joinAttemptOrigin=PlayButton";
    return "roblox-player:1+launchmode:play+gameinfo:" + ticket + "+launchtime:" + std::to_string(launchTimeMs) +
           "+placelauncherurl:" + str::urlEncode(placeLauncher) + "+browsertrackerid:" + browserTrackerId +
           "+robloxLocale:" + locale + "+gameLocale:" + locale + "+channel:+LaunchExp:InApp";
}

}  // namespace rl::instances
