// ui/TrayIcon.h
#pragma once

#include <windows.h>
#include <functional>
#include <string>

namespace rm {

class TrayIcon {
public:
    using OnShowMain = std::function<void()>;
    using OnShowHud = std::function<void()>;
    using OnExit = std::function<void()>;
    using OnSettings = std::function<void()>;

    TrayIcon();
    ~TrayIcon();

    bool Create(HINSTANCE hInst, HWND owner, const std::wstring& tip);
    void Destroy();
    void UpdateTip(const std::wstring& tip);

    // Callbacks (set before Create or anytime).
    void SetCallbacks(OnShowMain show_main, OnShowHud show_hud,
                      OnSettings settings, OnExit exit_cb);

    // Process WM_USER+100 / tray-related messages.
    void OnMessage(UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void ShowMenu();

    HWND hwnd_ = nullptr;
    HINSTANCE hinst_ = nullptr;
    UINT uid_ = 1;
    NOTIFYICONDATAW nid_{};

    OnShowMain on_show_main_;
    OnShowHud on_show_hud_;
    OnSettings on_settings_;
    OnExit on_exit_;
};

} // namespace rm