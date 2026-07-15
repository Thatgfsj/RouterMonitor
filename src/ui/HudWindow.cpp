// ui/HudWindow.cpp
#include "ui/HudWindow.h"

#include <windowsx.h>
#include <cstdio>
#include <algorithm>
#include <string>

namespace rm {

static std::wstring WidenUtf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

static std::wstring FormatRateKBps(double kbps) {
    if (kbps < 0) kbps = 0;
    wchar_t buf[64];
    if (kbps >= 1024.0) {
        double mbps = kbps / 1024.0;
        swprintf_s(buf, mbps >= 100 ? L"%.1f MB/s" : L"%.2f MB/s", mbps);
    } else if (kbps >= 1.0) {
        swprintf_s(buf, L"%.0f KB/s", kbps);
    } else {
        swprintf_s(buf, L"%.0f B/s", kbps * 1024.0);
    }
    return buf;
}

HudWindow::HudWindow() = default;
HudWindow::~HudWindow() { Destroy(); }

static const wchar_t* kHudWndClass = L"RouterMonitorHudWnd";

bool RegisterHudWindowClass(HINSTANCE hInst) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = HudWindow::WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = kHudWndClass;
    wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    wc.hIconSm = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) {
        DWORD err = GetLastError();
        if (err == 1410) return true;  // already exists
        wchar_t buf[256];
        swprintf_s(buf, L"RegisterClassExW(HUD) failed: %lu", err);
        MessageBoxW(nullptr, buf, L"RouterMonitor Error", MB_ICONERROR);
        return false;
    }
    return true;
}

bool HudWindow::Create(HINSTANCE hInst) {
    hinst_ = hInst;
    font_ = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, FF_SWISS, L"Segoe UI");
    font_big_ = CreateFontW(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, FF_SWISS, L"Segoe UI");

    int W = 240, H = 80;
    int X = (GetSystemMetrics(SM_CXSCREEN) - W) / 2;
    int Y = 80;

    hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                            kHudWndClass, L"HUD",
                            WS_POPUP,
                            X, Y, W, H,
                            nullptr, nullptr, hInst, this);
    if (!hwnd_) return false;
    SetLayeredWindowAttributes(hwnd_, 0, 220, LWA_ALPHA);
    return true;
}

void HudWindow::Destroy() {
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
    if (font_) { DeleteObject(font_); font_ = nullptr; }
    if (font_big_) { DeleteObject(font_big_); font_big_ = nullptr; }
}

void HudWindow::Show() {
    if (hwnd_) { ShowWindow(hwnd_, SW_SHOWNOACTIVATE); visible_ = true; }
}

void HudWindow::Hide() {
    if (hwnd_) { ShowWindow(hwnd_, SW_HIDE); visible_ = false; }
}

void HudWindow::Toggle() {
    if (!hwnd_) return;
    if (visible_) Hide(); else Show();
}

void HudWindow::UpdateRates(double tx, double rx, bool connected) {
    tx_ = tx; rx_ = rx; connected_ = connected;
    if (visible_) InvalidateRect(hwnd_, nullptr, FALSE);
}

LRESULT CALLBACK HudWindow::WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    HudWindow* self = nullptr;
    if (m == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(l);
        self = reinterpret_cast<HudWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(h, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = reinterpret_cast<HudWindow*>(GetWindowLongPtr(h, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(h, m, w, l);
    return DefWindowProc(h, m, w, l);
}

LRESULT HudWindow::HandleMessage(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_NCCREATE:
        case WM_NCCALCSIZE:
            return DefWindowProc(h, m, w, l);
        case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd_, &ps); OnPaint(); EndPaint(hwnd_, &ps); return 0; }
        case WM_ERASEBKGND: return 1;
        case WM_LBUTTONDOWN: OnLButtonDown(GET_X_LPARAM(l), GET_Y_LPARAM(l)); return 0;
        case WM_RBUTTONUP: {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 1, L"关闭 HUD");
            AppendMenuW(menu, MF_STRING, 2, L"退出程序");
            POINT pt; GetCursorPos(&pt);
            SetForegroundWindow(hwnd_);
            int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd_, nullptr);
            DestroyMenu(menu);
            if (cmd == 1) Hide();
            else if (cmd == 2) { Hide(); PostQuitMessage(0); }
            return 0;
        }
        case WM_LBUTTONDBLCLK: Hide(); return 0;
    }
    return DefWindowProc(hwnd_, m, w, l);
}

void HudWindow::OnLButtonDown(int x, int y) {
    SetCapture(hwnd_);
    dragging_ = true;
    POINT pt{x, y};
    ClientToScreen(hwnd_, &pt);
    RECT rc; GetWindowRect(hwnd_, &rc);
    drag_offset_.x = pt.x - rc.left;
    drag_offset_.y = pt.y - rc.top;
}

static void FillSolid(HDC dc, const RECT& rc, COLORREF c) {
    HBRUSH br = CreateSolidBrush(c);
    FillRect(dc, &rc, br);
    DeleteObject(br);
}

static COLORREF SpeedColor(double kbps) {
    if (kbps >= 5 * 1024) return RGB(220, 50, 47);
    if (kbps >= 1 * 1024) return RGB(255, 140, 0);
    if (kbps >= 100)      return RGB(80, 200, 120);
    return RGB(180, 180, 180);
}

void HudWindow::OnPaint() {
    if (dragging_) {
        // Move window while dragging.
        POINT pt; GetCursorPos(&pt);
        SetWindowPos(hwnd_, nullptr, pt.x - drag_offset_.x, pt.y - drag_offset_.y,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    RECT rc; GetClientRect(hwnd_, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;

    HDC mem_dc = CreateCompatibleDC(GetDC(hwnd_));
    HBITMAP mem_bmp = CreateCompatibleBitmap(GetDC(hwnd_), W, H);
    HBITMAP old_bmp = (HBITMAP)SelectObject(mem_dc, mem_bmp);

    // Background: rounded rect using brush + region
    HBRUSH bg = CreateSolidBrush(RGB(28, 32, 44));
    FillRect(mem_dc, &rc, bg);
    DeleteObject(bg);

    // Border
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(80, 90, 110));
    HPEN old_pen = (HPEN)SelectObject(mem_dc, pen);
    HBRUSH null_br = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH old_br = (HBRUSH)SelectObject(mem_dc, null_br);
    Rectangle(mem_dc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(mem_dc, old_br);
    SelectObject(mem_dc, old_pen);
    DeleteObject(pen);

    // Connection status dot
    HBRUSH dot = CreateSolidBrush(connected_ ? RGB(40, 200, 80) : RGB(220, 80, 70));
    RECT dot_rc = {8, 8, 18, 18};
    FillRect(mem_dc, &dot_rc, dot);
    DeleteObject(dot);

    SetBkMode(mem_dc, TRANSPARENT);

    HFONT old_f = (HFONT)SelectObject(mem_dc, font_big_);
    SetTextColor(mem_dc, SpeedColor(rx_));
    RECT rx_rc = {28, 8, W/2 - 4, H - 8};
    DrawTextW(mem_dc, (L"↓ " + FormatRateKBps(rx_)).c_str(), -1, &rx_rc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(mem_dc, SpeedColor(tx_));
    RECT tx_rc = {W/2, 8, W - 8, H - 8};
    DrawTextW(mem_dc, (L"↑ " + FormatRateKBps(tx_)).c_str(), -1, &tx_rc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(mem_dc, old_f);

    HDC screen = GetDC(hwnd_);
    BitBlt(screen, 0, 0, W, H, mem_dc, 0, 0, SRCCOPY);
    ReleaseDC(hwnd_, screen);

    SelectObject(mem_dc, old_bmp);
    DeleteObject(mem_bmp);
    DeleteDC(mem_dc);
}

} // namespace rm