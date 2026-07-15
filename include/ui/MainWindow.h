// ui/MainWindow.h
#pragma once

#include "../RouterApi.h"
#include "../PollerThread.h"
#include "TrayIcon.h"

#include <windows.h>
#include <memory>

namespace rm {

class HudWindow;

struct SortState {
    enum Column { ColDevName = 0, ColIp, ColRadio, ColRx, ColTx, ColCount };
    Column col = ColRx;
    bool descending = true;
};

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE hInst, const std::wstring& title);
    void Show(int nCmdShow);
    HWND Hwnd() const { return hwnd_; }

    // Set initial config + start polling.
    void StartPolling(const std::string& host, const std::string& user,
                      const std::string& pass, int interval_ms, size_t history_capacity);

    // Tear down.
    void Shutdown();

    void ToggleHud();
    void ShowSettingsDialog();
    bool IsHudVisible() const;

    // Snapshot delivery (called from worker thread, posts to UI thread).
    void PostSnapshot(std::shared_ptr<const Snapshot> snap);

public:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
private:
    LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);

    void OnPaint();
    void OnSize(int w, int h);
    void OnLButtonDown(int x, int y);
    void OnLButtonDblClk(int x, int y);
    void OnMouseMove(int x, int y);
    void OnCaptureChanged();
    void OnSortClick(int col);
    void OnCommand(WPARAM wParam, LPARAM lParam);

    void DrawTable(HDC dc, const RECT& rc);
    void DrawHeader(HDC dc, const RECT& rc);
    void DrawSummary(HDC dc, const RECT& rc);
    void DrawTrend(HDC dc, const RECT& rc);

    int RowFromY(int y) const;
    int ColFromX(int x) const;
    static std::wstring FormatRate(double kbps);
    static COLORREF SpeedColor(double kbps);

    void ApplySnapshot(std::shared_ptr<const Snapshot> snap);

    // Window layout constants (computed in OnSize).
    int summary_h_ = 56;
    int header_h_ = 28;
    int row_h_ = 30;
    int trend_h_ = 140;
    int table_h_ = 0;
    int trend_top_ = 0;

    int hover_row_ = -1;
    int selected_row_ = -1;       // -1 = none

    std::vector<RECT> col_rects_; // 5 columns (no MAC)
    SortState sort_;

    // UI filters
    HWND hwnd_sort_combo_ = nullptr;
    bool show_only_online_  = false;
    bool show_only_local_   = false;
    HWND hwnd_only_local_btn_ = nullptr;
    std::wstring my_ip_;             // detected from getaccount addr

    HWND hwnd_ = nullptr;
    HINSTANCE hinst_ = nullptr;
    HFONT font_ui_ = nullptr;
    HFONT font_bold_ = nullptr;
    HFONT font_big_ = nullptr;

    // Current snapshot
    std::shared_ptr<const Snapshot> snap_;
    std::vector<const DeviceState*> sorted_;   // pointer into snap_->devices, sorted by sort_

    TrayIcon tray_;
    std::unique_ptr<HudWindow> hud_;

    PollerThread poller_;
    std::string cfg_host_, cfg_user_, cfg_pass_;
    int cfg_interval_ = 2000;
    size_t cfg_history_capacity_ = 300;
};

// Window class registration helper (called from main).
bool RegisterMainWindowClass(HINSTANCE hInst);

} // namespace rm