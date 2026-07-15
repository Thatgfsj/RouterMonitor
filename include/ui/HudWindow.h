// ui/HudWindow.h
#pragma once

#include <windows.h>

namespace rm {

class HudWindow {
public:
    HudWindow();
    ~HudWindow();

    bool Create(HINSTANCE hInst);
    void Show();
    void Hide();
    bool IsVisible() const { return visible_; }
    void Toggle();
    void Destroy();

    void UpdateRates(double total_tx_kbps, double total_rx_kbps, bool connected);

    // For tray menu to invoke
    static void EnableDrag(HWND hwnd);

public:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
private:
    LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);
    void OnPaint();
    void OnLButtonDown(int x, int y);

    HWND hwnd_ = nullptr;
    HINSTANCE hinst_ = nullptr;
    HFONT font_ = nullptr;
    HFONT font_big_ = nullptr;
    bool visible_ = false;
    bool connected_ = false;
    double tx_ = 0;
    double rx_ = 0;

    bool dragging_ = false;
    POINT drag_offset_{};
};

bool RegisterHudWindowClass(HINSTANCE hInst);

} // namespace rm