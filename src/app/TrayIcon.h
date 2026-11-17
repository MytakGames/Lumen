#pragma once

#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

namespace rl::app {

// Notification-area icon with a context menu.
class TrayIcon {
public:
    static constexpr UINT kCallbackMessage = WM_APP + 1;

    struct MenuItem {
        UINT id = 0;  // 0 = separator
        std::wstring text;
        bool enabled = true;
        bool bold = false;
    };

    ~TrayIcon() { remove(); }

    bool add(HWND owner, const std::wstring& tooltip);
    void remove();
    void setTooltip(const std::wstring& tooltip);
    void balloon(const std::wstring& title, const std::wstring& text);
    // Shows the menu at the cursor and returns the chosen id (0 = dismissed).
    UINT showMenu(const std::vector<MenuItem>& items);
    // Explorer restarted: call add() again when this message arrives.
    static UINT taskbarCreatedMessage();
    bool visible() const { return added_; }

private:
    NOTIFYICONDATAW data_{};
    bool added_ = false;
};

}  // namespace rl::app
