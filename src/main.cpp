#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <string>

#include "app/Application.h"
#include "core/Autostart.h"
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

rl::app::LaunchOptions parseArgs() {
    rl::app::LaunchOptions opts;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; ++i) {
        if (std::wstring(argv[i]) == rl::autostart::kLaunchArg) opts.autostart = true;
    }
    LocalFree(argv);
    return opts;
}

}  // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    HANDLE guard = CreateMutexW(nullptr, TRUE, L"Local\\Lumen.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        activateExistingInstance();
        if (guard) CloseHandle(guard);
        return 0;
    }

    ImGui_ImplWin32_EnableDpiAwareness();
    // WebView2 and the shell dialogs need a single-threaded apartment on the UI thread.
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int rc = 0;
    {
        rl::app::Application app(parseArgs());
        rc = app.run();
    }
    if (SUCCEEDED(com)) CoUninitialize();
    if (guard) {
        ReleaseMutex(guard);
        CloseHandle(guard);
    }
    return rc;
}
