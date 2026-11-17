#include "app/TrayIcon.h"

namespace rl::app {

UINT TrayIcon::taskbarCreatedMessage() {
    static const UINT msg = RegisterWindowMessageW(L"TaskbarCreated");
    return msg;
}

bool TrayIcon::add(HWND owner, const std::wstring& tooltip) {
    remove();
    data_ = {};
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner;
    data_.uID = 1;
    data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = kCallbackMessage;
    data_.hIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON,
                                                GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    wcsncpy_s(data_.szTip, tooltip.c_str(), _TRUNCATE);
    added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
    if (added_) {
        data_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data_);
    }
    return added_;
}

void TrayIcon::remove() {
    if (added_) Shell_NotifyIconW(NIM_DELETE, &data_);
    added_ = false;
    if (data_.hIcon) {
        DestroyIcon(data_.hIcon);
        data_.hIcon = nullptr;
    }
}

void TrayIcon::setTooltip(const std::wstring& tooltip) {
    if (!added_) return;
    data_.uFlags = NIF_TIP | NIF_SHOWTIP;
    wcsncpy_s(data_.szTip, tooltip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &data_);
}

void TrayIcon::balloon(const std::wstring& title, const std::wstring& text) {
    if (!added_) return;
    data_.uFlags = NIF_INFO;
    data_.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
    wcsncpy_s(data_.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(data_.szInfo, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &data_);
}

UINT TrayIcon::showMenu(const std::vector<MenuItem>& items) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return 0;
    for (const auto& item : items) {
        if (item.id == 0) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            continue;
        }
        AppendMenuW(menu, MF_STRING | (item.enabled ? MF_ENABLED : MF_GRAYED), item.id, item.text.c_str());
        if (item.bold) SetMenuDefaultItem(menu, item.id, FALSE);
    }
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(data_.hWnd);  // required so the menu closes when clicking elsewhere
    const UINT cmd = static_cast<UINT>(
        TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, data_.hWnd, nullptr));
    PostMessageW(data_.hWnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
    return cmd;
}

}  // namespace rl::app
