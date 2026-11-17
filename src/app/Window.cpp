#include "app/Window.h"

#include <dwmapi.h>

#include "core/Logger.h"
#include "imgui.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace rl::app {
namespace {
constexpr wchar_t kClassName[] = L"RobloxLauncherWindow";
// Not defined by older SDK / MinGW headers.
constexpr DWORD kDwmUseImmersiveDarkMode = 20;
constexpr DWORD kDwmCaptionColor = 35;
constexpr DWORD kDwmTextColor = 36;
constexpr int kMinWidth = 900, kMinHeight = 600;
}  // namespace

Window::~Window() {
    rtv_.reset();
    swapChain_.reset();
    context_.reset();
    device_.reset();
    if (hwnd_) DestroyWindow(hwnd_);
    UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
}

bool Window::create(const std::wstring& title, int width, int height, int showCmd) {
    const HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = &Window::wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc)) {
        log::error("window", "RegisterClassEx failed");
        return false;
    }

    // Size is stored in 96-DPI units and scaled for the primary monitor.
    const float scale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY));
    const int w = static_cast<int>(static_cast<float>(width) * scale);
    const int h = static_cast<int>(static_cast<float>(height) * scale);
    hwnd_ = CreateWindowExW(0, kClassName, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, w, h,
                            nullptr, nullptr, inst, this);
    if (!hwnd_) {
        log::error("window", "CreateWindowEx failed");
        return false;
    }
    if (!createDevice()) {
        log::error("window", "Direct3D 11 device creation failed");
        return false;
    }
    ShowWindow(hwnd_, showCmd);
    UpdateWindow(hwnd_);
    return true;
}

bool Window::createDevice() {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd_;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    UINT flags = 0;
#ifndef NDEBUG
    // flags |= D3D11_CREATE_DEVICE_DEBUG;  // requires the Graphics Tools optional feature
#endif
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got{};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 2,
                                               D3D11_SDK_VERSION, &sd, swapChain_.put(), device_.put(), &got,
                                               context_.put());
    if (hr == DXGI_ERROR_UNSUPPORTED) {
        log::warn("window", "Hardware device unavailable, falling back to WARP");
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels, 2,
                                           D3D11_SDK_VERSION, &sd, swapChain_.put(), device_.put(), &got,
                                           context_.put());
    }
    if (FAILED(hr)) return false;
    createRenderTarget();
    return true;
}

void Window::createRenderTarget() {
    ComPtr<ID3D11Texture2D> back;
    swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(back.put()));
    if (back) device_->CreateRenderTargetView(back.get(), nullptr, rtv_.put());
}

bool Window::processMessages(bool idle, unsigned timeoutMs) {
    if (idle) MsgWaitForMultipleObjects(0, nullptr, FALSE, timeoutMs, QS_ALLINPUT);
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) return false;
    }
    return true;
}

void Window::beginFrame() {
    if (occluded_ && swapChain_->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
        Sleep(10);
    }
    occluded_ = false;
    if (pendingWidth_ != 0 && pendingHeight_ != 0) {
        rtv_.reset();
        swapChain_->ResizeBuffers(0, pendingWidth_, pendingHeight_, DXGI_FORMAT_UNKNOWN,
                                  DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
        pendingWidth_ = pendingHeight_ = 0;
        createRenderTarget();
    }
}

void Window::bindAndClear(const float clearColor[4]) {
    ID3D11RenderTargetView* rtv = rtv_.get();
    context_->OMSetRenderTargets(1, &rtv, nullptr);
    context_->ClearRenderTargetView(rtv, clearColor);
}

void Window::present() {
    const HRESULT hr = swapChain_->Present(1, 0);  // vsync
    occluded_ = hr == DXGI_STATUS_OCCLUDED;
}

void Window::setTitle(const std::wstring& title) { SetWindowTextW(hwnd_, title.c_str()); }

void Window::setTitleBarColors(bool dark, COLORREF caption, COLORREF text) {
    const BOOL useDark = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd_, kDwmUseImmersiveDarkMode, &useDark, sizeof(useDark));
    // Windows 11 only; silently ignored on Windows 10.
    DwmSetWindowAttribute(hwnd_, kDwmCaptionColor, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd_, kDwmTextColor, &text, sizeof(text));
}

float Window::dpiScale() const { return ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd_); }

bool Window::consumeDpiChanged() {
    const bool changed = dpiChanged_;
    dpiChanged_ = false;
    return changed;
}

void Window::placement(int& width, int& height, bool& maximized) const {
    if (closed_ || !hwnd_) {
        if (closed_) {
            width = closedWidth_;
            height = closedHeight_;
            maximized = closedMaximized_;
        }
        return;
    }
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    GetWindowPlacement(hwnd_, &wp);
    const float scale = dpiScale();
    width = static_cast<int>(static_cast<float>(wp.rcNormalPosition.right - wp.rcNormalPosition.left) / scale);
    height = static_cast<int>(static_cast<float>(wp.rcNormalPosition.bottom - wp.rcNormalPosition.top) / scale);
    maximized = wp.showCmd == SW_SHOWMAXIMIZED;
}

LRESULT CALLBACK Window::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return TRUE;
    auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) {
        if (!self->hwnd_ && msg != WM_NCDESTROY) self->hwnd_ = hwnd;  // first messages arrive before CreateWindowEx returns
        if (self->hwnd_ == hwnd) return self->handle(msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void Window::show() {
    if (!hwnd_) return;
    if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
}

void Window::hide() {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

void Window::exit() {
    if (!hwnd_) return;
    exiting_ = true;
    placement(closedWidth_, closedHeight_, closedMaximized_);
    closed_ = true;
    DestroyWindow(hwnd_);
}

LRESULT Window::handle(UINT msg, WPARAM wp, LPARAM lp) {
    if (hook_ && !exiting_) {
        LRESULT out = 0;
        if (hook_(msg, wp, lp, out)) return out;
    }
    switch (msg) {
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) {
                pendingWidth_ = LOWORD(lp);
                pendingHeight_ = HIWORD(lp);
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lp);
            const float scale = dpiScale();
            info->ptMinTrackSize.x = static_cast<LONG>(kMinWidth * scale);
            info->ptMinTrackSize.y = static_cast<LONG>(kMinHeight * scale);
            return 0;
        }
        case WM_DPICHANGED: {
            const RECT* r = reinterpret_cast<RECT*>(lp);
            SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            dpiChanged_ = true;
            return 0;
        }
        case WM_SYSCOMMAND:
            if ((wp & 0xfff0) == SC_KEYMENU) return 0;  // no ALT application menu
            break;
        case WM_CLOSE:
            // Capture the placement while the window still exists.
            placement(closedWidth_, closedHeight_, closedMaximized_);
            closed_ = true;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_NCDESTROY:
            hwnd_ = nullptr;
            break;
        default:
            break;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace rl::app
