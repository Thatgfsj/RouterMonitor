// main.cpp - WinMain entry, message loop, and credential dialog.
#include "Config.h"
#include "Credentials.h"
#include "ui/MainWindow.h"
#include "ui/HudWindow.h"
#include "DiagLog.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <cstdio>
#include <string>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")

namespace {

struct CredResult {
    bool ok = false;
    std::string host, user, pass;
};

struct CredDialog {
    HWND hwnd = nullptr;
    HWND hHost = nullptr, hUser = nullptr, hPass = nullptr;
    CredResult* result = nullptr;
};

static LRESULT CALLBACK CredWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    CredDialog* dlg = nullptr;
    if (m == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(l);
        dlg = reinterpret_cast<CredDialog*>(cs->lpCreateParams);
        SetWindowLongPtr(h, GWLP_USERDATA, (LONG_PTR)dlg);
        // Labels
        CreateWindowExW(0, L"STATIC", L"路由器地址:", WS_CHILD | WS_VISIBLE,
                        12, 14, 80, 20, h, (HMENU)2001, cs->hInstance, nullptr);
        CreateWindowExW(0, L"STATIC", L"用户名:", WS_CHILD | WS_VISIBLE,
                        12, 50, 80, 20, h, (HMENU)2002, cs->hInstance, nullptr);
        CreateWindowExW(0, L"STATIC", L"密码:", WS_CHILD | WS_VISIBLE,
                        12, 86, 80, 20, h, (HMENU)2003, cs->hInstance, nullptr);
        // Edits
        dlg->hHost = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                     100, 12, 200, 22, h, (HMENU)1001, cs->hInstance, nullptr);
        dlg->hUser = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                     100, 48, 200, 22, h, (HMENU)1002, cs->hInstance, nullptr);
        dlg->hPass = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL,
                                     100, 84, 200, 22, h, (HMENU)1003, cs->hInstance, nullptr);
        // Buttons
        CreateWindowExW(0, L"BUTTON", L"登录", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                        100, 120, 90, 28, h, (HMENU)IDOK, cs->hInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE,
                        210, 120, 90, 28, h, (HMENU)IDCANCEL, cs->hInstance, nullptr);

        SetWindowTextW(dlg->hHost, L"192.168.10.1");
        SetWindowTextW(dlg->hUser, L"admin");
        SetFocus(dlg->hPass);
        return 0;
    } else {
        dlg = reinterpret_cast<CredDialog*>(GetWindowLongPtr(h, GWLP_USERDATA));
    }
    if (!dlg) return DefWindowProc(h, m, w, l);
    switch (m) {
        case WM_COMMAND: {
            int id = LOWORD(w);
            if (id == IDOK) {
                wchar_t buf[256];
                GetWindowTextW(dlg->hHost, buf, 256);
                char narrow[256];
                WideCharToMultiByte(CP_UTF8, 0, buf, -1, narrow, 256, nullptr, nullptr);
                if (dlg->result) dlg->result->host = narrow;
                GetWindowTextW(dlg->hUser, buf, 256);
                WideCharToMultiByte(CP_UTF8, 0, buf, -1, narrow, 256, nullptr, nullptr);
                if (dlg->result) dlg->result->user = narrow;
                GetWindowTextW(dlg->hPass, buf, 256);
                WideCharToMultiByte(CP_UTF8, 0, buf, -1, narrow, 256, nullptr, nullptr);
                if (dlg->result) dlg->result->pass = narrow;
                if (dlg->result) dlg->result->ok = true;
                DestroyWindow(h);
                return 0;
            }
            if (id == IDCANCEL) {
                DestroyWindow(h);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(h, m, w, l);
}

bool ShowCredentialUI(HINSTANCE hInst, HWND parent, CredResult* out) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = CredWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
        wc.lpszClassName = L"RouterMonitorCredDlg";
        wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
        wc.hIconSm = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
        if (!RegisterClassExW(&wc)) {
            DWORD err = GetLastError();
            if (err != 1410) {
                wchar_t buf[128];
                swprintf_s(buf, L"RegisterClass(CredDlg) failed: %lu", err);
                MessageBoxW(nullptr, buf, L"RouterMonitor Error", MB_ICONERROR);
                return false;
            }
        }
        registered = true;
    }
    CredDialog dlg;
    dlg.result = out;

    int x = 200, y = 200;
    if (parent) {
        RECT rc; GetWindowRect(parent, &rc);
        x = rc.left + 40; y = rc.top + 40;
    }

    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RouterMonitorCredDlg",
                             L"RouterMonitor - 首次登录",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                             x, y, 340, 180,
                             parent, nullptr, hInst, &dlg);
    dlg.hwnd = h;
    if (!h) return false;

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (!IsWindow(h)) break;
    }
    return out->ok;
}

} // anonymous namespace

// ---------------- WinMain ----------------

// mingw CRT with -municode entry point calls wWinMain; with -mwindows it uses
// wWinMainCRTStartup -> wmain -> wWinMain. Either way, the HINSTANCE is correct.
// (Earlier failure was not from the entry point but from WNDCLASSEX init order.)
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR cmdline, int nCmdShow) {
    rm::DiagLog("=== wWinMain entered, hInst=%p", hInst);

    // Init common controls (for tooltips etc.)
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // GDI+
    ULONG_PTR gdiplus_token;
    Gdiplus::GdiplusStartupInput gdipsi;
    Gdiplus::GdiplusStartup(&gdiplus_token, &gdipsi, nullptr);

    // Load non-credential config (poll interval, UI prefs).
    rm::Config cfg;
    cfg.Load();

    // Load router credentials from credentials.json (independent of config.ini).
    rm::Credentials cred;
    bool force_login = wcsstr(cmdline, L"--login") != nullptr;
    if (force_login || !cred.Load() || !cred.Valid()) {
        CredResult cr;
        HWND placeholder = nullptr;
        if (!ShowCredentialUI(hInst, placeholder, &cr)) {
            Gdiplus::GdiplusShutdown(gdiplus_token);
            return 0;
        }
        cred.host = cr.host;
        cred.user = cr.user;
        cred.pass = cr.pass;
        if (!cred.Save()) {
            rm::DiagLog("[main] WARNING: failed to save credentials.json; will prompt every run");
        }
    }

    // Register window classes
    if (!rm::RegisterMainWindowClass(hInst)) {
        MessageBoxW(nullptr, L"RegisterClass failed", L"Error", MB_ICONERROR);
        return 1;
    }
    if (!rm::RegisterHudWindowClass(hInst)) {
        MessageBoxW(nullptr, L"RegisterClass(HUD) failed", L"Error", MB_ICONERROR);
        return 1;
    }

    // Create main window
    rm::MainWindow mw;
    if (!mw.Create(hInst, L"RouterMonitor - ZTE RAX3000Ze 网速监控")) {
        MessageBoxW(nullptr, L"CreateWindow failed", L"Error", MB_ICONERROR);
        return 1;
    }
    mw.Show(cfg.start_minimized ? SW_HIDE : nCmdShow);

    // Start polling
    mw.StartPolling(cred.host, cred.user, cred.pass,
                    cfg.poll_interval_ms, cfg.history_capacity);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    Gdiplus::GdiplusShutdown(gdiplus_token);
    return (int)msg.wParam;
}