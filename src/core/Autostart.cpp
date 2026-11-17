#include "core/Autostart.h"

#include <windows.h>

#include <string>

#include "core/Logger.h"

namespace rl::autostart {
namespace {
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"Lumen";
constexpr wchar_t kLegacyValueName[] = L"RobloxLauncher";  // before the rename
}  // namespace

bool isEnabled() {
    return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, nullptr, nullptr) ==
           ERROR_SUCCESS;
}

bool setEnabled(bool enabled, const std::filesystem::path& exePath) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        log::error("autostart", "Cannot open Run key");
        return false;
    }
    RegDeleteValueW(key, kLegacyValueName);
    LSTATUS rc;
    if (enabled) {
        const std::wstring cmd = L"\"" + exePath.wstring() + L"\" " + kLaunchArg;
        rc = RegSetValueExW(key, kValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(cmd.c_str()),
                            static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        rc = RegDeleteValueW(key, kValueName);
        if (rc == ERROR_FILE_NOT_FOUND) rc = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS) log::error("autostart", "Registry update failed: " + std::to_string(rc));
    return rc == ERROR_SUCCESS;
}

}  // namespace rl::autostart
