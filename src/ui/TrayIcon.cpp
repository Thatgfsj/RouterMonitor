// ui/TrayIcon.cpp
#include "ui/TrayIcon.h"
#include <windowsx.h>

namespace rm {

TrayIcon::TrayIcon() = default;
TrayIcon::~TrayIcon() { Destroy(); }

bool TrayIcon::Create(HINSTANCE hInst, HWND owner, const std::wstring& tip) {
    hinst_ = hInst;
    hwnd_ = owner;
    nid_ = {};
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = owner;
    nid_.uID = uid_;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = WM_USER + 100;
    nid_.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    wcsncpy_s(nid_.szTip, tip.c_str(), _TRUNCATE);
    return Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
}

void TrayIcon::Destroy() {
    if (hwnd_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        nid_ = {};
        hwnd_ = nullptr;
    }
}

void TrayIcon::UpdateTip(const std::wstring& tip) {
    if (!hwnd_) return;
    nid_.uFlags = NIF_TIP;
    wcsncpy_s(nid_.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::SetCallbacks(OnShowMain show_main, OnShowHud show_hud,
                            OnSettings settings, OnExit exit_cb) {
    on_show_main_ = std::move(show_main);
    on_show_hud_ = std::move(show_hud);
    on_settings_ = std::move(settings);
    on_exit_ = std::move(exit_cb);
}

void TrayIcon::ShowMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L"显示主窗口");
    AppendMenuW(menu, MF_STRING, 2, L"显示/隐藏 HUD");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, L"设置...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 4, L"退出");

    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);

    switch (cmd) {
        case 1: if (on_show_main_) on_show_main_(); break;
        case 2: if (on_show_hud_) on_show_hud_(); break;
        case 3: if (on_settings_) on_settings_(); break;
        case 4: if (on_exit_) on_exit_(); break;
    }
}

void TrayIcon::OnMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_USER + 100 && (UINT)wParam == uid_) {
        switch (lParam) {
            case WM_RBUTTONUP:
            case WM_CONTEXTMENU:
                ShowMenu();
                break;
            case WM_LBUTTONDBLCLK:
            case WM_LBUTTONUP:
                if (on_show_main_) on_show_main_();
                break;
        }
    }
}

} // namespace rm