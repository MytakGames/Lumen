#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <cwctype>
#include <filesystem>
#include <functional>
#include <string>

#include "app/Application.h"
#include "core/Autostart.h"
#include "core/Paths.h"
#include "imgui_impl_win32.h"

namespace {

// Only one launcher at a time: a second start brings the existing window forward.
bool activateExistingInstance() {
    HWND existing = FindWindowW(L"RobloxLauncherWindow", nullptr);
    if (!existing) return false;
    if (IsIconic(existing)) ShowWindow(existing, SW_RESTORE);
    ShowWindow(existing, SW_SHOW);
    SetForegroundWindow(existing);
    return true;
}

// --autostart          started by Windows at sign-in
// --data-dir <folder>  use another data folder (portable copy, separate profile, demos).
//                      Each data folder has its own single-instance lock.
rl::app::LaunchOptions parseArgs(std::wstring& dataDir) {
    rl::app::LaunchOptions opts;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == rl::autostart::kLaunchArg) opts.autostart = true;
        if (arg == L"--data-dir" && i + 1 < argc) dataDir = argv[++i];
    }
    LocalFree(argv);
    return opts;
}

std::wstring instanceMutexName(const std::wstring& dataDir) {
    if (dataDir.empty()) return L"Local\\Lumen.SingleInstance";
    std::wstring lower = std::filesystem::absolute(dataDir).wstring();
    for (auto& ch : lower) ch = static_cast<wchar_t>(towlower(ch));
    return L"Local\\Lumen.SingleInstance." + std::to_wstring(std::hash<std::wstring>{}(lower));
}

}  // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    std::wstring dataDir;
    const rl::app::LaunchOptions options = parseArgs(dataDir);
    if (!dataDir.empty()) rl::paths::setDataDirOverride(std::filesystem::absolute(dataDir));

    HANDLE guard = CreateMutexW(nullptr, TRUE, instanceMutexName(dataDir).c_str());
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (dataDir.empty()) activateExistingInstance();
        if (guard) CloseHandle(guard);
        return 0;
    }

    ImGui_ImplWin32_EnableDpiAwareness();
    // The shell dialogs need a single-threaded apartment on the UI thread.
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int rc = 0;
    {
        rl::app::Application app(options);
        rc = app.run();
    }
    if (SUCCEEDED(com)) CoUninitialize();
    if (guard) {
        ReleaseMutex(guard);
        CloseHandle(guard);
    }
    return rc;
}
