#pragma once

#include <windows.h>
#include <d3d11.h>

#include <functional>
#include <string>

#include "core/ComPtr.h"

namespace rl::app {

// Win32 window + Direct3D 11 swap chain hosting the Dear ImGui UI.
class Window {
public:
    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Return true from the hook to consume a message (result in `out`).
    using MessageHook = std::function<bool(UINT msg, WPARAM wp, LPARAM lp, LRESULT& out)>;

    bool create(const std::wstring& title, int width, int height, int showCmd);
    void setMessageHook(MessageHook hook) { hook_ = std::move(hook); }

    void show();  // restore + bring to front
    void hide();
    bool isVisible() const { return hwnd_ && IsWindowVisible(hwnd_); }
    // Destroys the window (bypassing the hook) after capturing its placement.
    void exit();
    void wake() const {
        if (hwnd_) PostMessageW(hwnd_, WM_NULL, 0, 0);
    }

    // Pumps pending messages. Returns false once WM_QUIT has been received.
    // When `idle` is true, blocks until a message arrives or `timeoutMs` elapses.
    bool processMessages(bool idle, unsigned timeoutMs);

    void beginFrame();  // applies pending swap-chain resize
    void bindAndClear(const float clearColor[4]);
    void present();

    void setTitle(const std::wstring& title);
    void setTitleBarColors(bool dark, COLORREF caption, COLORREF text);

    HWND hwnd() const { return hwnd_; }
    ID3D11Device* device() const { return device_.get(); }
    ID3D11DeviceContext* context() const { return context_.get(); }
    float dpiScale() const;
    bool isMinimized() const { return IsIconic(hwnd_) != FALSE; }
    bool consumeDpiChanged();
    // Normal (restored) client size and maximized flag, for persistence.
    void placement(int& width, int& height, bool& maximized) const;

private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    bool createDevice();
    void createRenderTarget();

    HWND hwnd_ = nullptr;
    MessageHook hook_;
    bool exiting_ = false;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swapChain_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    UINT pendingWidth_ = 0, pendingHeight_ = 0;
    bool occluded_ = false;
    bool dpiChanged_ = false;
    bool closed_ = false;
    int closedWidth_ = 0, closedHeight_ = 0;
    bool closedMaximized_ = false;
};

}  // namespace rl::app
