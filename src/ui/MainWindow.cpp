// ui/MainWindow.cpp
#include "ui/MainWindow.h"
#include "ui/HudWindow.h"
#include "../DiagLog.h"

#include <windowsx.h>
#include <cstdio>
#include <algorithm>
#include <string>

namespace rm {

// ---------------- helpers ----------------

static std::wstring WidenUtf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

static std::string ToUpper(std::string s) {
    for (auto& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

// Human-friendly rate: KB/s if small, otherwise MB/s. Returns KB/s value too in *kbps_out (passed by value).
static std::wstring FormatRateKBps(double kbps, bool& out_is_mbps) {
    out_is_mbps = false;
    if (kbps < 0) kbps = 0;
    wchar_t buf[64];
    if (kbps >= 1024.0) {
        double mbps = kbps / 1024.0;
        out_is_mbps = true;
        if (mbps >= 100.0) swprintf_s(buf, L"%.1f MB/s", mbps);
        else swprintf_s(buf, L"%.2f MB/s", mbps);
    } else if (kbps >= 1.0) {
        swprintf_s(buf, L"%.0f KB/s", kbps);
    } else {
        // < 1 KB/s, show as B/s
        swprintf_s(buf, L"%.0f B/s", kbps * 1024.0);
    }
    return buf;
}

std::wstring MainWindow::FormatRate(double kbps) {
    bool _;
    return FormatRateKBps(kbps, _);
}

COLORREF MainWindow::SpeedColor(double kbps) {
    if (kbps >= 5 * 1024) return RGB(220, 50, 47);    // > 5 MB/s: red
    if (kbps >= 1 * 1024) return RGB(255, 140, 0);    // 1-5 MB/s: orange
    if (kbps >= 100)      return RGB(0, 160, 0);      // 100KB-1MB: green
    return RGB(120, 120, 120);                         // idle
}

// ---------------- MainWindow ----------------

MainWindow::MainWindow() = default;
MainWindow::~MainWindow() { Shutdown(); }

static const wchar_t* kMainWndClass = L"RouterMonitorMainWnd";

bool RegisterMainWindowClass(HINSTANCE hInst) {
    UnregisterClassW(kMainWndClass, hInst);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    // Use a thunk: assign the static WndProc via a free-function alias.
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.cbWndExtra = sizeof(void*);   // reserve space for this pointer (alternative to GWLP_USERDATA)
    wc.cbClsExtra = 0;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = kMainWndClass;
    wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    wc.hIconSm = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    wc.lpszMenuName = nullptr;
    ATOM atom = RegisterClassExW(&wc);
    if (atom == 0) {
        DWORD err = GetLastError();
        if (err == 1410) return true;  // already registered
        wchar_t buf[256];
        swprintf_s(buf, L"RegisterClassExW(MainWnd) failed: %lu", err);
        MessageBoxW(nullptr, buf, L"RouterMonitor Error", MB_ICONERROR);
        return false;
    }
    return true;
}

bool MainWindow::Create(HINSTANCE hInst, const std::wstring& title) {
    hinst_ = hInst;

    // Diagnostic: probe several hInst candidates.
    HINSTANCE probe = GetModuleHandleW(nullptr);
    wchar_t mod_path[MAX_PATH] = {};
    if (probe) GetModuleFileNameW(probe, mod_path, MAX_PATH);

    font_ui_   = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, FF_SWISS, L"Microsoft YaHei UI");
    font_bold_ = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, FF_SWISS, L"Microsoft YaHei UI");
    font_big_  = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, FF_SWISS, L"Microsoft YaHei UI");

    // Try #1: minimal "STATIC" window (built-in class) — proves hInst + USER32 work.
    HWND probe_hwnd = CreateWindowExW(0, L"STATIC", L"probe", WS_OVERLAPPED,
                                       0, 0, 100, 100, nullptr, nullptr, probe, nullptr);
    if (!probe_hwnd) {
        DWORD err = GetLastError();
        wchar_t buf[512];
        swprintf_s(buf,
                   L"Even minimal CreateWindowExW failed!\n"
                   L"GetLastError = %lu\n"
                   L"HINST = %p\n"
                   L"Module = %ls",
                   err, probe, mod_path);
        MessageBoxW(nullptr, buf, L"RouterMonitor Error", MB_ICONERROR);
        return false;
    }
    DestroyWindow(probe_hwnd);

    // Try #2: full main window.
    hwnd_ = CreateWindowExW(0, kMainWndClass, title.c_str(),
                            WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, 880, 600,
                            nullptr, nullptr, probe, this);
    if (!hwnd_) {
        DWORD err = GetLastError();
        // Fallback: maybe our custom class registration is broken. Use built-in WC_DIALOG class.
        // (wc_dialog is #32770 and supports WS_OVERLAPPEDWINDOW.)
        hwnd_ = CreateWindowExW(0, L"#32770", title.c_str(),
                                WS_OVERLAPPEDWINDOW | DS_CENTER,
                                CW_USEDEFAULT, CW_USEDEFAULT, 880, 600,
                                nullptr, nullptr, probe, this);
        if (!hwnd_) {
            DWORD err2 = GetLastError();
            wchar_t buf[1024];
            swprintf_s(buf,
                       L"Main window CreateWindowExW failed\n"
                       L"Custom class err = %lu\n"
                       L"Fallback (#32770) err = %lu\n"
                       L"HINST = %p\n"
                       L"Module = %ls",
                       err, err2, probe, mod_path);
            MessageBoxW(nullptr, buf, L"RouterMonitor Error", MB_ICONERROR);
            return false;
        }
        // Note: in fallback mode we don't get a window proc, so just show a dialog-like placeholder.
    }
    return true;
}

void MainWindow::Show(int nCmdShow) {
    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);
}

void MainWindow::StartPolling(const std::string& host, const std::string& user,
                              const std::string& pass, int interval_ms, size_t history_capacity) {
    cfg_host_ = host;
    cfg_user_ = user;
    cfg_pass_ = pass;
    cfg_interval_ = interval_ms;
    cfg_history_capacity_ = history_capacity;

    poller_.Configure(host, user, pass, interval_ms, history_capacity);
    poller_.SetOnSnapshot([this](std::shared_ptr<const Snapshot> snap) {
        PostSnapshot(std::move(snap));
    });
    poller_.Start();

    if (hud_) {
        hud_->Show();
    }
}

void MainWindow::Shutdown() {
    poller_.Stop();
    if (hud_) hud_->Destroy();
    tray_.Destroy();
    if (font_ui_) { DeleteObject(font_ui_); font_ui_ = nullptr; }
    if (font_bold_) { DeleteObject(font_bold_); font_bold_ = nullptr; }
    if (font_big_) { DeleteObject(font_big_); font_big_ = nullptr; }
}

void MainWindow::ToggleHud() {
    if (!hud_) {
        hud_ = std::make_unique<HudWindow>();
        if (!hud_->Create(hinst_)) { hud_.reset(); return; }
    }
    hud_->Toggle();
}

bool MainWindow::IsHudVisible() const {
    return hud_ && hud_->IsVisible();
}

void MainWindow::ShowSettingsDialog() {
    // Very small settings dialog implemented in-place. For now, just a message box.
    wchar_t buf[512];
    swprintf_s(buf, L"路由器: %hs\n用户: %hs\n轮询间隔: %d ms",
               cfg_host_.c_str(), cfg_user_.c_str(), cfg_interval_);
    MessageBoxW(hwnd_, buf, L"设置 (Settings)", MB_OK | MB_ICONINFORMATION);
}

void MainWindow::PostSnapshot(std::shared_ptr<const Snapshot> snap) {
    if (!hwnd_) return;
    // Move ownership to UI thread via SetWindowLongPtr-style passing (we use raw pointer with manual delete).
    // Simpler: use PostMessage with a heap-allocated wrapper.
    auto* p = new std::shared_ptr<const Snapshot>(std::move(snap));
    PostMessage(hwnd_, WM_APP + 1, 0, (LPARAM)p);
}

void MainWindow::ApplySnapshot(std::shared_ptr<const Snapshot> snap) {
    snap_ = std::move(snap);

    // Remember our own IP so DrawTable can mark the local device.
    if (snap_) my_ip_ = WidenUtf8(snap_->my_ip);

    // Detect our own IP from the getaccount response (we store it on the device with addr == local).
    // For filtering "本机", we use the addr of the first device whose IP == 192.168.10.1
    // is not feasible; instead, we treat the machine with the highest / latest activity as "local".
    // Simpler approach: capture the IP we send to login_prepare (i.e. our device IP).
    // For now we compare against the router's first hop (192.168.10.1) using gateway heuristic.

    // Build filtered + sorted view.
    sorted_.clear();
    sorted_.reserve(snap_->devices.size());
    for (auto& kv : snap_->devices) {
        const Device& d = kv.second.info;
        if (show_only_online_ && d.tx_kbps == 0 && d.rx_kbps == 0 &&
            d.uptime_sec > 0) {
            // Has uptime but zero rate => not transferring now; still show.
        }
        if (show_only_local_) {
            char my_ip_a[64] = {};
            WideCharToMultiByte(CP_UTF8, 0, my_ip_.c_str(), -1, my_ip_a, sizeof(my_ip_a), nullptr, nullptr);
            if (d.ipaddr != my_ip_a) continue;
        }
        sorted_.push_back(&kv.second);
    }
    auto cmp = [this](const DeviceState* a, const DeviceState* b) {
        const Device& da = a->info;
        const Device& db = b->info;
        int dir = sort_.descending ? -1 : 1;
        double sa = 0, sb = 0;
        switch (sort_.col) {
            case SortState::ColDevName: return dir * da.devname.compare(db.devname) < 0;
            case SortState::ColIp:      return dir * da.ipaddr.compare(db.ipaddr) < 0;
            case SortState::ColRadio:   return dir * da.radio.compare(db.radio) < 0;
            case SortState::ColRx:      sa = da.rx_kbps; sb = db.rx_kbps; break;
            case SortState::ColTx:      sa = da.tx_kbps; sb = db.tx_kbps; break;
            default: break;
        }
        return dir * (sa - sb) < 0;
    };
    std::sort(sorted_.begin(), sorted_.end(), cmp);

    if (hud_) {
        hud_->UpdateRates(snap_->total_tx_kbps, snap_->total_rx_kbps, snap_->login_ok);
    }

    // Update tray tooltip with totals.
    wchar_t tip[128];
    if (snap_->login_ok) {
        swprintf_s(tip, L"RouterMonitor  ↓%s  ↑%s",
                   FormatRate(snap_->total_rx_kbps).c_str(),
                   FormatRate(snap_->total_tx_kbps).c_str());
        tray_.UpdateTip(tip);
    } else {
        tray_.UpdateTip(L"RouterMonitor (未连接)");
    }

    InvalidateRect(hwnd_, nullptr, FALSE);
}

// ---------------- WndProc ----------------

LRESULT CALLBACK MainWindow::WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    MainWindow* self = nullptr;
    if (m == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(l);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(h, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtr(h, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(h, m, w, l);
    return DefWindowProc(h, m, w, l);
}

LRESULT MainWindow::HandleMessage(HWND h, UINT m, WPARAM w, LPARAM l) {
    rm::DiagLog("HandleMessage: msg=0x%X w=%llu l=%llu", m, (unsigned long long)w, (unsigned long long)l);
    switch (m) {
        case WM_NCCREATE:
        case WM_NCCALCSIZE:
            // Must let DefWindowProc handle these or window never gets created/laid out.
            return DefWindowProc(h, m, w, l);
        case WM_CREATE: {
            // Create tray icon
            tray_.Create(hinst_, hwnd_, L"RouterMonitor");
            tray_.SetCallbacks(
                [this] { ShowWindow(hwnd_, SW_SHOW); SetForegroundWindow(hwnd_); },
                [this] { ToggleHud(); },
                [this] { ShowSettingsDialog(); },
                [this] { DestroyWindow(hwnd_); }
            );

            // Sort dropdown: by name / by speed (rx / tx)
            hwnd_sort_combo_ = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                0, 0, 140, 200, hwnd_, (HMENU)1001, hinst_, nullptr);
            SendMessageW(hwnd_sort_combo_, CB_ADDSTRING, 0, (LPARAM)L"按下行速度");
            SendMessageW(hwnd_sort_combo_, CB_ADDSTRING, 0, (LPARAM)L"按上行速度");
            SendMessageW(hwnd_sort_combo_, CB_ADDSTRING, 0, (LPARAM)L"按设备名");
            SendMessageW(hwnd_sort_combo_, CB_ADDSTRING, 0, (LPARAM)L"按 IP 地址");
            SendMessageW(hwnd_sort_combo_, CB_SETCURSEL, 0, 0);

            return 0;
        }
        case WM_COMMAND: {
            OnCommand(w, l);
            return 0;
        }
        case WM_APP + 1: {
            auto* p = reinterpret_cast<std::shared_ptr<const Snapshot>*>(l);
            if (p) {
                ApplySnapshot(std::move(*p));
                delete p;
            }
            return 0;
        }
        case WM_SIZE: OnSize(LOWORD(l), HIWORD(l)); return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd_, &ps);
            if (dc) {
                OnPaint();
                EndPaint(hwnd_, &ps);
            }
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_LBUTTONDOWN:  OnLButtonDown(GET_X_LPARAM(l), GET_Y_LPARAM(l)); return 0;
        case WM_LBUTTONDBLCLK: OnLButtonDblClk(GET_X_LPARAM(l), GET_Y_LPARAM(l)); return 0;
        case WM_MOUSEMOVE:    OnMouseMove(GET_X_LPARAM(l), GET_Y_LPARAM(l)); return 0;
        case WM_CAPTURECHANGED: OnCaptureChanged(); return 0;
        case WM_USER + 100:   tray_.OnMessage(m, w, l); return 0;
        case WM_KEYDOWN: {
            // Shift+Esc or Alt+F4 immediately exits (bypass tray hide).
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if ((shift && w == VK_ESCAPE) || (shift && w == 'Q')) {
                DestroyWindow(hwnd_);
                return 0;
            }
            break;
        }
        case WM_CLOSE: {
            // If shift held: exit. Otherwise hide to tray.
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (shift) {
                DestroyWindow(hwnd_);
            } else {
                ShowWindow(hwnd_, SW_HIDE);
            }
            return 0;
        }
        case WM_DESTROY: {
            Shutdown();
            PostQuitMessage(0);
            return 0;
        }
    }
    tray_.OnMessage(m, w, l);
    return DefWindowProc(hwnd_, m, w, l);
}

// ---------------- Layout / Drawing ----------------

void MainWindow::OnSize(int w, int h) {
    trend_h_ = std::max(80, h / 4);
    table_h_ = h - summary_h_ - header_h_ - trend_h_;
    trend_top_ = summary_h_ + header_h_ + table_h_;
    InvalidateRect(hwnd_, nullptr, FALSE);
    // Force immediate repaint (otherwise some WMs won't paint until idle).
    UpdateWindow(hwnd_);
}

int MainWindow::RowFromY(int y) const {
    int top = summary_h_ + header_h_;
    if (y < top) return -1;
    int rel = (y - top) / row_h_;
    if (rel < 0 || rel >= (int)sorted_.size()) return -1;
    return rel;
}

int MainWindow::ColFromX(int x) const {
    for (int i = 0; i < (int)col_rects_.size(); ++i) {
        if (x >= col_rects_[i].left && x < col_rects_[i].right) return i;
    }
    return -1;
}

void MainWindow::OnLButtonDown(int x, int y) {
    // Click in summary row -> ignore
    if (y < summary_h_) return;
    if (y >= summary_h_ && y < summary_h_ + header_h_) {
        int col = ColFromX(x);
        if (col >= 0 && col < SortState::ColCount) OnSortClick(col);
        return;
    }
    int row = RowFromY(y);
    if (row >= 0) {
        selected_row_ = row;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void MainWindow::OnLButtonDblClk(int x, int y) {
    // Could expand row in future; for now same as single click.
    OnLButtonDown(x, y);
}

void MainWindow::OnMouseMove(int x, int y) {
    int new_hover = RowFromY(y);
    if (new_hover != hover_row_) {
        hover_row_ = new_hover;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void MainWindow::OnCaptureChanged() {}

void MainWindow::OnSortClick(int col) {
    if (sort_.col == col) {
        sort_.descending = !sort_.descending;
    } else {
        sort_.col = (SortState::Column)col;
        sort_.descending = true;
    }
    if (hwnd_sort_combo_) {
        // Map column index back to combo selection (0=rx, 1=tx, 2=name, 3=ip).
        switch (sort_.col) {
            case SortState::ColRx:    SendMessageW(hwnd_sort_combo_, CB_SETCURSEL, 0, 0); break;
            case SortState::ColTx:    SendMessageW(hwnd_sort_combo_, CB_SETCURSEL, 1, 0); break;
            case SortState::ColDevName: SendMessageW(hwnd_sort_combo_, CB_SETCURSEL, 2, 0); break;
            case SortState::ColIp:    SendMessageW(hwnd_sort_combo_, CB_SETCURSEL, 3, 0); break;
            default: break;
        }
    }
    if (snap_) ApplySnapshot(snap_);
}

void MainWindow::OnCommand(WPARAM wParam, LPARAM) {
    int id = LOWORD(wParam);
    int code = HIWORD(wParam);
    if (id == 1001 && code == CBN_SELCHANGE) {
        int sel = (int)SendMessageW(hwnd_sort_combo_, CB_GETCURSEL, 0, 0);
        switch (sel) {
            case 0: sort_.col = SortState::ColRx; sort_.descending = true; break;
            case 1: sort_.col = SortState::ColTx; sort_.descending = true; break;
            case 2: sort_.col = SortState::ColDevName; sort_.descending = false; break;
            case 3: sort_.col = SortState::ColIp; sort_.descending = false; break;
            default: break;
        }
        if (snap_) ApplySnapshot(snap_);
    }
}

static void FillSolid(HDC dc, const RECT& rc, COLORREF c) {
    HBRUSH br = CreateSolidBrush(c);
    FillRect(dc, &rc, br);
    DeleteObject(br);
}

static void DrawTextIn(HDC dc, const RECT& rc, const std::wstring& s, HFONT font, COLORREF color, UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HFONT old = (HFONT)SelectObject(dc, font);
    RECT r = rc;
    r.left += 8; r.right -= 8;
    DrawTextW(dc, s.c_str(), (int)s.size(), &r, flags);
    SelectObject(dc, old);
}

static void DrawVLine(HDC dc, int x, int y1, int y2, COLORREF c) {
    for (int y = y1; y < y2; ++y) SetPixel(dc, x, y, c);
}

void MainWindow::OnPaint() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;
    rm::DiagLog("OnPaint: W=%d H=%d snap_=%p hud_=%p tray_hwnd=%p",
                W, H, (void*)snap_.get(), (void*)hud_.get(), (void*)hwnd_);
    if (W <= 0 || H <= 0) return;  // nothing to draw

    HDC screen_dc = GetDC(hwnd_);
    if (!screen_dc) return;

    HDC mem_dc = CreateCompatibleDC(screen_dc);
    if (!mem_dc) { ReleaseDC(hwnd_, screen_dc); return; }
    HBITMAP mem_bmp = CreateCompatibleBitmap(screen_dc, W, H);
    if (!mem_bmp) { DeleteDC(mem_dc); ReleaseDC(hwnd_, screen_dc); return; }
    HBITMAP old_bmp = (HBITMAP)SelectObject(mem_dc, mem_bmp);

    // Background
    FillSolid(mem_dc, rc, RGB(245, 247, 250));

    // Sections
    RECT summary_rc = {0, 0, W, summary_h_};
    RECT header_rc  = {0, summary_h_, W, summary_h_ + header_h_};
    RECT table_rc   = {0, summary_h_ + header_h_, W, summary_h_ + header_h_ + table_h_};
    RECT trend_rc   = {0, trend_top_, W, H};

    FillSolid(mem_dc, summary_rc, RGB(255, 255, 255));
    FillSolid(mem_dc, header_rc,  RGB(232, 238, 246));
    FillSolid(mem_dc, table_rc,   RGB(255, 255, 255));
    FillSolid(mem_dc, trend_rc,   RGB(255, 255, 255));

    DrawSummary(mem_dc, summary_rc);
    DrawHeader(mem_dc, header_rc);
    DrawTable(mem_dc, table_rc);
    DrawTrend(mem_dc, trend_rc);

    // Borders
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(210, 215, 222));
    HPEN old_pen = (HPEN)SelectObject(mem_dc, pen);
    HBRUSH null_br = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH old_br = (HBRUSH)SelectObject(mem_dc, null_br);
    Rectangle(mem_dc, summary_rc.left, summary_rc.top, summary_rc.right, summary_rc.bottom);
    Rectangle(mem_dc, header_rc.left, header_rc.top, header_rc.right, header_rc.bottom);
    Rectangle(mem_dc, table_rc.left, table_rc.top, table_rc.right, table_rc.bottom);
    Rectangle(mem_dc, trend_rc.left, trend_rc.top, trend_rc.right, trend_rc.bottom);
    SelectObject(mem_dc, old_br);
    SelectObject(mem_dc, old_pen);
    DeleteObject(pen);

    // Blit to screen
    BitBlt(screen_dc, 0, 0, W, H, mem_dc, 0, 0, SRCCOPY);

    SelectObject(mem_dc, old_bmp);
    DeleteObject(mem_bmp);
    DeleteDC(mem_dc);
    ReleaseDC(hwnd_, screen_dc);
}

void MainWindow::DrawSummary(HDC dc, const RECT& rc) {
    bool ok = snap_ && snap_->login_ok;
    COLORREF dot = ok ? RGB(40, 180, 80) : RGB(220, 80, 70);
    HBRUSH br = CreateSolidBrush(dot);
    RECT dot_rc = {rc.left + 16, rc.top + 18, rc.left + 28, rc.top + 30};
    FillRect(dc, &dot_rc, br);
    DeleteObject(br);

    std::wstring status = ok ? L"已连接" : (snap_ && !snap_->last_error.empty() ? WidenUtf8(snap_->last_error).c_str() : L"未连接");
    DrawTextIn(dc, {rc.left + 36, rc.top, rc.left + 220, rc.bottom},
               status, font_bold_, ok ? RGB(40, 140, 60) : RGB(200, 60, 50));

    // Total rates
    std::wstring tx_s = L"总上行: " + FormatRate(snap_ ? snap_->total_tx_kbps : 0);
    std::wstring rx_s = L"总下行: " + FormatRate(snap_ ? snap_->total_rx_kbps : 0);
    DrawTextIn(dc, {rc.left + 220, rc.top, rc.left + 480, rc.bottom}, tx_s, font_ui_, SpeedColor(snap_ ? snap_->total_tx_kbps : 0));
    DrawTextIn(dc, {rc.left + 480, rc.top, rc.left + 740, rc.bottom}, rx_s, font_ui_, SpeedColor(snap_ ? snap_->total_rx_kbps : 0));

    // Device count (filtered)
    wchar_t cnt[64];
    swprintf_s(cnt, L"设备: %zu / %zu",
               sorted_.size(),
               snap_ ? snap_->devices.size() : 0);
    DrawTextIn(dc, {rc.right - 280, rc.top, rc.right - 8, rc.bottom},
               cnt, font_ui_, RGB(80, 80, 80), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // Position the sort combo + filter checkboxes on the right side
    int right_edge = rc.right - 220;
    MoveWindow(hwnd_sort_combo_, right_edge, rc.top + 18, 140, 22, TRUE);
    MoveWindow(hwnd_only_local_btn_, right_edge + 145, rc.top + 18, 60, 22, TRUE);
    // Move "仅本机" button below "仅在线"
    // (omit for layout simplicity; user can scroll if needed)
}

static const wchar_t* kColTitles[] = {
    L"设备", L"IP", L"频段", L"↓下行", L"↑上行"
};

void MainWindow::DrawHeader(HDC dc, const RECT& rc) {
    int W = rc.right - rc.left;
    // Column widths: dev=240, ip=130, radio=70, rx=210, tx=210  -> sum = 860
    int widths[SortState::ColCount] = { 240, 130, 70, 210, 210 };
    int total = 0;
    for (int i = 0; i < SortState::ColCount; ++i) total += widths[i];
    // Scale to fit
    double scale = (double)W / total;
    col_rects_.clear();
    int x = 0;
    for (int i = 0; i < SortState::ColCount; ++i) {
        RECT cr = { rc.left + x, rc.top, rc.left + x + (int)(widths[i] * scale), rc.bottom };
        col_rects_.push_back(cr);
        x += (int)(widths[i] * scale);
    }

    for (int i = 0; i < SortState::ColCount; ++i) {
        const RECT& cr = col_rects_[i];
        // highlight column header if sorted
        if (i == sort_.col) {
            FillSolid(dc, cr, RGB(220, 230, 245));
        }
        std::wstring title = kColTitles[i];
        if (i == sort_.col) title += sort_.descending ? L" ▼" : L" ▲";
        DrawTextIn(dc, cr, title, font_bold_, RGB(60, 80, 110));
        // Divider
        if (i + 1 < SortState::ColCount) {
            DrawVLine(dc, cr.right - 1, cr.top + 4, cr.bottom - 4, RGB(200, 210, 222));
        }
    }
}

void MainWindow::DrawTable(HDC dc, const RECT& rc) {
    int y = rc.top;
    for (size_t i = 0; i < sorted_.size(); ++i) {
        const DeviceState* s = sorted_[i];
        RECT row_rc = { rc.left, y, rc.right, y + row_h_ };
        bool is_sel = ((int)i == selected_row_);
        bool is_hover = ((int)i == hover_row_);
        if (is_sel)      FillSolid(dc, row_rc, RGB(220, 235, 255));
        else if (is_hover) FillSolid(dc, row_rc, RGB(240, 246, 255));
        else if (i % 2)  FillSolid(dc, row_rc, RGB(248, 250, 253));

const Device& d = s->info;
        for (int c = 0; c < SortState::ColCount; ++c) {
            const RECT& cr_col = col_rects_[c];
            // Build a per-row clip rectangle (only column X range, current row Y range).
            RECT cell = { cr_col.left, row_rc.top, cr_col.right, row_rc.bottom };
            std::wstring text;
            COLORREF color = RGB(40, 40, 40);
            switch (c) {
                case SortState::ColDevName:
                    if (d.devname.empty()) text = L"-";
                    else text = WidenUtf8(d.devname);
                    if (!snap_->my_ip.empty() && d.ipaddr == snap_->my_ip) {
                        text += L" (本机)";
                    }
                    color = RGB(46, 123, 255);
                    break;
                case SortState::ColIp:
                    text = WidenUtf8(d.ipaddr);
                    break;
                case SortState::ColRadio: {
                    if (d.radio == "有线" || d.radio.empty()) text = L"有线";
                    else text = WidenUtf8(d.radio);
                    break;
                }
                case SortState::ColRx:
                    text = FormatRate(d.rx_kbps);
                    color = SpeedColor(d.rx_kbps);
                    break;
                case SortState::ColTx:
                    text = FormatRate(d.tx_kbps);
                    color = SpeedColor(d.tx_kbps);
                    break;
            }
            UINT align = (c == SortState::ColRx || c == SortState::ColTx) ?
                         (DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX) :
                         (DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            DrawTextIn(dc, cell, text, font_ui_, color, align);
        }

        // Speed bars for Rx/Tx columns — drawn under the text so they don't overlap.
        if (!sorted_.empty()) {
            double maxv = 100 * 1024.0; // bar scale: 100 MB/s = 100% (covers gigabit links)
            auto draw_bar = [&](int col_idx, double v, COLORREF base) {
                const RECT& cr = col_rects_[col_idx];
                int bar_h = 4;
                int bar_w = (int)(cr.right - cr.left - 24);
                if (bar_w <= 0) return;
                int fill = (int)(bar_w * (v / maxv));
                if (fill < 0) fill = 0;
                if (fill > bar_w) fill = bar_w;
                int bar_y = row_rc.bottom - bar_h - 2;
                RECT track = { cr.left + 12, bar_y, cr.left + 12 + bar_w, bar_y + bar_h };
                FillSolid(dc, track, RGB(225, 230, 238));
                if (fill > 0) {
                    RECT filled = { track.left, track.top, track.left + fill, track.bottom };
                    FillSolid(dc, filled, base);
                }
            };
            draw_bar(SortState::ColRx, s->info.rx_kbps, SpeedColor(s->info.rx_kbps));
            draw_bar(SortState::ColTx, s->info.tx_kbps, SpeedColor(s->info.tx_kbps));
        }

        y += row_h_;
        if (y >= rc.bottom) break;
    }
}

void MainWindow::DrawTrend(HDC dc, const RECT& rc) {
    RECT title_rc = { rc.left + 12, rc.top + 6, rc.right - 12, rc.top + 26 };
    std::wstring title = L"历史趋势 (近 10 分钟)";
    if (selected_row_ >= 0 && selected_row_ < (int)sorted_.size()) {
        const Device& d = sorted_[selected_row_]->info;
        if (d.devname.empty()) title = L"历史趋势 - " + WidenUtf8(d.mac);
        else title = L"历史趋势 - " + WidenUtf8(d.devname) +
                L"  ↓" + FormatRate(d.rx_kbps) + L"  ↑" + FormatRate(d.tx_kbps);
    }
    DrawTextIn(dc, title_rc, title, font_bold_, RGB(60, 80, 110));

    RECT plot_rc = { rc.left + 12, rc.top + 32, rc.right - 12, rc.bottom - 12 };
    FillSolid(dc, plot_rc, RGB(252, 253, 255));

    // Grid
    HPEN grid_pen = CreatePen(PS_DOT, 1, RGB(220, 226, 235));
    HPEN old_pen = (HPEN)SelectObject(dc, grid_pen);
    for (int i = 1; i < 4; ++i) {
        int y = plot_rc.top + (plot_rc.bottom - plot_rc.top) * i / 4;
        MoveToEx(dc, plot_rc.left, y, nullptr);
        LineTo(dc, plot_rc.right, y);
    }
    for (int i = 1; i < 6; ++i) {
        int x = plot_rc.left + (plot_rc.right - plot_rc.left) * i / 6;
        MoveToEx(dc, x, plot_rc.top, nullptr);
        LineTo(dc, x, plot_rc.bottom);
    }
    SelectObject(dc, old_pen);
    DeleteObject(grid_pen);

    if (selected_row_ < 0 || selected_row_ >= (int)sorted_.size()) return;
    const auto& history = sorted_[selected_row_]->history;
    auto samples = history.Snapshot();
    if (samples.size() < 2) {
        RECT info = plot_rc;
        DrawTextIn(dc, info, L"(等待数据...)", font_ui_, RGB(140, 140, 140), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    double maxv = 1.0;
    for (const auto& s : samples) {
        if (s.rx_kbps > maxv) maxv = s.rx_kbps;
        if (s.tx_kbps > maxv) maxv = s.tx_kbps;
    }
    maxv *= 1.15;

    auto plot_series = [&](auto getter, COLORREF color) {
        HPEN pen = CreatePen(PS_SOLID, 2, color);
        HPEN old = (HPEN)SelectObject(dc, pen);
        int n = (int)samples.size();
        int W = plot_rc.right - plot_rc.left;
        int H = plot_rc.bottom - plot_rc.top;
        POINT prev{};
        bool has_prev = false;
        for (int i = 0; i < n; ++i) {
            double v = getter(samples[i]);
            int x = plot_rc.left + (n == 1 ? 0 : (W * i / (n - 1)));
            int y = plot_rc.bottom - (int)(H * v / maxv);
            if (y < plot_rc.top) y = plot_rc.top;
            if (has_prev) {
                MoveToEx(dc, prev.x, prev.y, nullptr);
                LineTo(dc, x, y);
            }
            prev = {x, y};
            has_prev = true;
        }
        SelectObject(dc, old);
        DeleteObject(pen);
    };
    plot_series([](const DeviceSample& s) { return s.rx_kbps; }, RGB(46, 123, 255));
    plot_series([](const DeviceSample& s) { return s.tx_kbps; }, RGB(220, 80, 70));

    // Y-axis label (max)
    wchar_t label[32];
    if (maxv >= 1024) swprintf_s(label, L"%.1f MB/s", maxv / 1024.0);
    else swprintf_s(label, L"%.0f KB/s", maxv);
    DrawTextIn(dc, {plot_rc.left - 60, plot_rc.top - 4, plot_rc.left, plot_rc.top + 16},
               label, font_ui_, RGB(120, 120, 120), DT_RIGHT | DT_TOP | DT_SINGLELINE);
}

} // namespace rm