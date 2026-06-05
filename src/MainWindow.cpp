#include "MainWindow.h"
#include "ErrorCodes.h"
#include "DimmerManager.h"
#include <algorithm>
#include <dwmapi.h>
#include <shellapi.h>
#include <strsafe.h>
#include <winhttp.h>

#ifndef D2DERR_RECREATED
#define D2DERR_RECREATED ((HRESULT)0x8898000CL)
#endif

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winhttp.lib")

static const wchar_t* APP_VERSION = L"v1.5.3";

static int CompareVersion(const wchar_t* verA, const wchar_t* verB) {
    int majA = 0, minA = 0, patA = 0;
    int majB = 0, minB = 0, patB = 0;
    swscanf(verA, L"v%d.%d.%d", &majA, &minA, &patA);
    swscanf(verB, L"v%d.%d.%d", &majB, &minB, &patB);
    if (majA != majB) return majA - majB;
    if (minA != minB) return minA - minB;
    return patA - patB;
}

#define WM_TRAYICON (WM_USER + 100)
#define WM_UPDATE_CHECK (WM_USER + 101)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SHOW 1002

// Add App dialog
static const wchar_t* ADD_DLG_CLASS = L"IdleDimmerAddAppDlg";
static bool g_addDlgRegistered = false;
static wchar_t g_addDlgResult[128];
static bool g_addDlgConfirmed;

static LRESULT CALLBACK AddAppDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCT)lp)->hInstance;
            HWND hEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 12, 30, 240, 22, hwnd, (HMENU)100, hInst, NULL);
            CreateWindowW(L"STATIC", L"Process name (e.g. chrome.exe):", WS_CHILD | WS_VISIBLE, 12, 12, 240, 14, hwnd, NULL, hInst, NULL);
            CreateWindowW(L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 82, 62, 64, 24, hwnd, (HMENU)IDOK, hInst, NULL);
            CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 154, 62, 64, 24, hwnd, (HMENU)IDCANCEL, hInst, NULL);
            SendMessageW(hEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
            SetFocus(hEdit);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK) {
                g_addDlgResult[0] = 0;
                GetDlgItemTextW(hwnd, 100, g_addDlgResult, 128);
                size_t len = wcslen(g_addDlgResult);
                while (len > 0 && g_addDlgResult[len - 1] == L' ') g_addDlgResult[--len] = 0;
                if (len > 0) { g_addDlgConfirmed = true; DestroyWindow(hwnd); }
            } else if (LOWORD(wp) == IDCANCEL) {
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static bool AddTrayIcon(HWND hwnd, UINT uID, HICON hIcon, const wchar_t* tip) {
    NOTIFYICONDATAW nid = { 0 };
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = uID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hIcon;
    StringCchCopyW(nid.szTip, ARRAYSIZE(nid.szTip), tip);
    return Shell_NotifyIconW(NIM_ADD, &nid);
}

static bool RemoveTrayIcon(HWND hwnd, UINT uID) {
    NOTIFYICONDATAW nid = { 0 };
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = uID;
    return Shell_NotifyIconW(NIM_DELETE, &nid);
}

bool MainWindow::Create(HINSTANCE hInst, int nCmdShow) {
    m_hInst = hInst;
    LoadSettings();
    m_undoStack.clear();
    m_canUndo = false;
    m_changeCount = 0;

    // Thread will be created after m_hwnd exists to prevent race condition

    // Register MainWindow Class
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"IdleDimmerMainClass";
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(101));
    if (!RegisterClassExW(&wc)) {
        LogError(ErrorCode::E106, HRESULT_FROM_WIN32(GetLastError()));
        return false;
    }

    // Initial position in center of screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - m_windowWidth) / 2;
    int y = (screenH - m_windowHeight) / 2;

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (m_config.showInTaskbar) {
        // Standard window
    } else {
        style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    }

    m_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"IdleDimmerMainClass",
        L"IdleDimmer - Control Panel",
        style,
        x, y, m_windowWidth, m_windowHeight,
        nullptr, nullptr, hInst, this
    );

    if (!m_hwnd) {
        LogError(ErrorCode::E107, HRESULT_FROM_WIN32(GetLastError()));
        return false;
    }

    // Enable Windows 11 rounded corners and dark/light theme
    BOOL useDark = !m_config.lightMode;
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
    
    DWORD cornerPreference = DWMWCP_ROUND;
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));

    // Initialize D2D
    if (FAILED(CreateGraphicsResources())) {
        return false;
    }

    m_appVersion = APP_VERSION;

    DimmerManager::Instance().Initialize(hInst);
    DimmerManager::Instance().RefreshMonitors();

    // Synchronize monitor settings from loaded configuration
    SyncMonitorsWithConfig();

    // Apply warm tint and active dimming settings
    DimmerManager::Instance().SetWarmTint(m_config.warmTint);
    DimmerManager::Instance().SetDimmingEnabled(m_config.dimmingEnabled);

    // Register global hotkeys
    if (!RegisterHotKey(m_hwnd, 101, MOD_CONTROL | MOD_ALT, VK_UP)) {
        LogError(ErrorCode::E210, HRESULT_FROM_WIN32(GetLastError()));
    }
    if (!RegisterHotKey(m_hwnd, 102, MOD_CONTROL | MOD_ALT, VK_DOWN)) {
        LogError(ErrorCode::E211, HRESULT_FROM_WIN32(GetLastError()));
    }
    if (!RegisterHotKey(m_hwnd, 103, MOD_CONTROL | MOD_ALT, 0x44)) { // 'D' key
        LogError(ErrorCode::E212, HRESULT_FROM_WIN32(GetLastError()));
    }

    // Start Inactivity/Idle checking timer (1000ms interval)
    if (!SetTimer(m_hwnd, 202, 1000, nullptr)) {
        LogError(ErrorCode::E209, HRESULT_FROM_WIN32(GetLastError()));
    }

    // Add to system tray
    HICON hAppIcon = LoadIconW(m_hInst, MAKEINTRESOURCEW(101));
    if (!AddTrayIcon(m_hwnd, 1, hAppIcon, L"IdleDimmer Screen Brightness")) {
        LogError(ErrorCode::E213, HRESULT_FROM_WIN32(GetLastError()));
    }

    m_hUpdateThread = CreateThread(nullptr, 0, CheckForUpdatesThread, this, 0, nullptr);

    UpdateLayout();

    // If started with Windows and Close to Tray is enabled, start minimized to tray
    if (m_config.startWithWindows && m_config.closeToTray) {
        ShowWindow(m_hwnd, SW_HIDE);
    } else {
        ShowWindow(m_hwnd, nCmdShow);
    }
    UpdateWindow(m_hwnd);

    return true;
}

void MainWindow::Show(bool show) {
    if (show) {
        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
    } else {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

HRESULT MainWindow::CreateGraphicsResources() {
    HRESULT hr = S_OK;
    if (!m_pFactory) {
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pFactory);
        if (FAILED(hr)) {
            LogError(ErrorCode::E201, hr);
        }
    }
    if (SUCCEEDED(hr) && !m_pRenderTarget) {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        
        hr = m_pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &m_pRenderTarget
        );

        if (FAILED(hr)) {
            LogError(ErrorCode::E206, hr);
        }

        if (SUCCEEDED(hr)) {
            // Create brushes
            HRESULT hrBrush = S_OK;
            hrBrush = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x131315), &m_pBrushBg);
            if (FAILED(hrBrush)) LogError(ErrorCode::E207, hrBrush);

            hrBrush = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x1F1F24), &m_pBrushCard);
            if (FAILED(hrBrush)) LogError(ErrorCode::E207, hrBrush);

            hrBrush = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x2D2D34), &m_pBrushCardBorder);
            if (FAILED(hrBrush)) LogError(ErrorCode::E207, hrBrush);

            hrBrush = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF), &m_pBrushText);
            if (FAILED(hrBrush)) LogError(ErrorCode::E207, hrBrush);

            hrBrush = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x8E8E93), &m_pBrushTextMuted);
            if (FAILED(hrBrush)) LogError(ErrorCode::E207, hrBrush);

            hrBrush = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x0078D4), &m_pBrushAccent);
            if (FAILED(hrBrush)) LogError(ErrorCode::E207, hrBrush);

            hrBrush = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x2B88D8), &m_pBrushAccentHover);
            if (FAILED(hrBrush)) LogError(ErrorCode::E207, hrBrush);

            hrBrush = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x44444A), &m_pBrushTrack);
            if (FAILED(hrBrush)) LogError(ErrorCode::E207, hrBrush);
        }
    }

    if (SUCCEEDED(hr) && !m_pDWriteFactory) {
        hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
        );

        if (FAILED(hr)) {
            LogError(ErrorCode::E202, hr);
        }

        if (SUCCEEDED(hr)) {
            HRESULT hrFmt = S_OK;
            hrFmt = m_pDWriteFactory->CreateTextFormat(
                L"Segoe UI Variable Display", nullptr,
                DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                20.0f, L"en-us", &m_pTextFormatTitle
            );
            if (FAILED(hrFmt)) LogError(ErrorCode::E203, hrFmt);

            hrFmt = m_pDWriteFactory->CreateTextFormat(
                L"Segoe UI Variable Text", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                13.0f, L"en-us", &m_pTextFormatBody
            );
            if (FAILED(hrFmt)) LogError(ErrorCode::E204, hrFmt);

            hrFmt = m_pDWriteFactory->CreateTextFormat(
                L"Segoe UI Variable Text", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                10.5f, L"en-us", &m_pTextFormatDetail
            );
            if (FAILED(hrFmt)) LogError(ErrorCode::E205, hrFmt);
        }
    }

    return hr;
}

void MainWindow::DiscardGraphicsResources() {
    if (m_pBrushBg) { m_pBrushBg->Release(); m_pBrushBg = nullptr; }
    if (m_pBrushCard) { m_pBrushCard->Release(); m_pBrushCard = nullptr; }
    if (m_pBrushCardBorder) { m_pBrushCardBorder->Release(); m_pBrushCardBorder = nullptr; }
    if (m_pBrushText) { m_pBrushText->Release(); m_pBrushText = nullptr; }
    if (m_pBrushTextMuted) { m_pBrushTextMuted->Release(); m_pBrushTextMuted = nullptr; }
    if (m_pBrushAccent) { m_pBrushAccent->Release(); m_pBrushAccent = nullptr; }
    if (m_pBrushAccentHover) { m_pBrushAccentHover->Release(); m_pBrushAccentHover = nullptr; }
    if (m_pBrushTrack) { m_pBrushTrack->Release(); m_pBrushTrack = nullptr; }
    
    if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }
}

void MainWindow::UpdateLayout() {
    m_sliders.clear();
    m_checkboxes.clear();
    m_windowWidth = CONTENT_WIDTH;

    const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();

    // Start yOffset below the header
    int yOffset = 30;

    // 2. Master Slider Card (If multiple screens)
    if (activeMons.size() > 1) {
        UISlider master;
        master.isMaster = true;
        master.value = m_config.masterValue / 90.0f;
        master.active = m_config.masterEnabled;
        
        master.rect.left = 20;
        master.rect.top = yOffset;
        master.rect.right = m_windowWidth - 35;
        master.rect.bottom = master.rect.top + 65;

        m_sliders.push_back(master);

        UICheckbox mcb;
        mcb.settingName = L"MasterEnabled";
        mcb.checked = m_config.masterEnabled;
        mcb.pValue = &m_config.masterEnabled;
        mcb.label = L"";
        mcb.rect.left = master.rect.left + 12;
        mcb.rect.top = master.rect.top + 12;
        mcb.rect.right = mcb.rect.left + 34;
        mcb.rect.bottom = mcb.rect.top + 18;
        m_checkboxes.push_back(mcb);

        yOffset = master.rect.bottom + 10;
    }

    // 3. Individual Screen Sliders
    for (const auto& mon : activeMons) {
        UISlider slider;
        slider.monitorId = mon.id;
        slider.value = mon.dimValue / 90.0f;
        slider.active = mon.enabled;
        
        slider.rect.left = 20;
        slider.rect.top = yOffset;
        slider.rect.right = m_windowWidth - 35;
        slider.rect.bottom = slider.rect.top + 65;

        m_sliders.push_back(slider);

        UICheckbox cb;
        cb.monitorId = mon.id;
        cb.checked = mon.enabled;
        cb.pValue = nullptr;
        cb.label = L"";
        cb.rect.left = slider.rect.left + 12;
        cb.rect.top = slider.rect.top + 12;
        cb.rect.right = cb.rect.left + 34;
        cb.rect.bottom = cb.rect.top + 18;
        m_checkboxes.push_back(cb);

        yOffset = slider.rect.bottom + 10;
    }

    // Space before settings
    yOffset += 5;

    // 4. Settings Checkboxes (Grouped Footer Area)
    // Helper lambda: places a checkbox at absolute yPos
    auto AddCheckbox = [&](const std::wstring& name, bool checked, bool* pValue, const std::wstring& label, int col, int yPos) {
        UICheckbox cb;
        cb.settingName = name;
        cb.checked = checked;
        cb.pValue = pValue;
        cb.label = label;
        cb.rect.left = (col == 0) ? 25 : 255;
        cb.rect.top = yPos;
        cb.rect.right = cb.rect.left + 34;
        cb.rect.bottom = cb.rect.top + 18;
        m_checkboxes.push_back(cb);
    };

    // ── DIMMING section ──
    yOffset += 16; // space for section header label
    AddCheckbox(L"DimmingEnabled", m_config.dimmingEnabled, &m_config.dimmingEnabled, L"Active Dimming", 0, yOffset);
    AddCheckbox(L"IdleDimEnabled", m_config.idleDimEnabled, &m_config.idleDimEnabled, L"Dim When Away", 1, yOffset);
    yOffset += 22;
    AddCheckbox(L"IdleTurnOff", m_config.idleTurnOff, &m_config.idleTurnOff, L"Turn Off When Away", 0, yOffset);
    yOffset += 22;

    // ── DISPLAY section ──
    yOffset += 8; // gap between sections
    yOffset += 16; // space for section header label
    AddCheckbox(L"WarmTint", m_config.warmTint, &m_config.warmTint, L"Warm Amber Tint", 0, yOffset);
    AddCheckbox(L"LightMode", m_config.lightMode, &m_config.lightMode, L"Light Mode", 1, yOffset);
    yOffset += 22;

    // ── APPLICATION section ──
    yOffset += 8; // gap between sections
    yOffset += 16; // space for section header label
    AddCheckbox(L"CloseToTray", m_config.closeToTray, &m_config.closeToTray, L"Close to Tray", 0, yOffset);
    AddCheckbox(L"StartWithWindows", m_config.startWithWindows, &m_config.startWithWindows, L"Start with Windows", 1, yOffset);
    yOffset += 22;

    // ── BLOCKED APPS ──
    m_blockedArrowRect = { CONTENT_WIDTH - 120, 5, CONTENT_WIDTH - 80, 25 };

    if (m_config.idleDimEnabled) {
        // Inactivity Minutes Slider Card
        UISlider idleMin;
        idleMin.isIdleMinutes = true;
        idleMin.value = (m_config.idleMinutes - 1) / 59.0f; // 1 to 60
        idleMin.active = true;
        idleMin.rect.left = 20;
        idleMin.rect.top = yOffset + 15;
        idleMin.rect.right = m_windowWidth - 35;
        idleMin.rect.bottom = idleMin.rect.top + 58;
        m_sliders.push_back(idleMin);

        // Inactivity Dim Level Slider Card
        UISlider idleLvl;
        idleLvl.isIdleDimLevel = true;
        idleLvl.value = m_config.idleDimLevel / 100.0f; // 0 to 100
        idleLvl.active = true;
        idleLvl.rect.left = 20;
        idleLvl.rect.top = idleMin.rect.bottom + 15;
        idleLvl.rect.right = m_windowWidth - 35;
        idleLvl.rect.bottom = idleLvl.rect.top + 58;
        m_sliders.push_back(idleLvl);

        yOffset = idleLvl.rect.bottom;
    }

    // Calculate required window height dynamically
    m_windowHeight = yOffset + 42;

    // Footer undo rect (centered in footer area)
    m_undoRect.left = m_windowWidth / 2 - 70;
    m_undoRect.top = m_windowHeight - 25;
    m_undoRect.right = m_windowWidth / 2 + 70;
    m_undoRect.bottom = m_windowHeight;

    // ── RIGHT-SIDE PANEL LAYOUT ──
    int panelLeft = CONTENT_WIDTH + 10;
    int panelTop = 30;
    int panelRight = panelLeft + PANEL_WIDTH;
    int panelBottom = m_windowHeight - 42;
    m_blockedPanelRect = { panelLeft, panelTop, panelRight, panelBottom };

    m_blockedAddRect = { panelLeft + 10, panelTop + 8, panelRight - 10, panelTop + 26 };

    m_blockedItems.clear();
    m_blockedContentHeight = 0;
    if (m_blockedExpanded) {
        int itemY = panelTop + 44;
        for (const auto& app : m_config.blockedApps) {
            UIBlockedAppItem item;
            item.name = app;
            item.textRect = { panelLeft + 10, itemY, panelRight - 50, itemY + 22 };
            item.removeRect = { panelRight - 38, itemY + 2, panelRight - 14, itemY + 20 };
            m_blockedItems.push_back(item);
            itemY += 24;
        }
        itemY += 10;
        m_blockedContentHeight = itemY - panelTop;
    }

    m_windowWidth = m_blockedExpanded ? (CONTENT_WIDTH + 10 + PANEL_WIDTH + 10) : CONTENT_WIDTH;

    RECT rc = { 0, 0, m_windowWidth, m_windowHeight };
    AdjustWindowRectEx(&rc, GetWindowLongW(m_hwnd, GWL_STYLE), FALSE, GetWindowLongW(m_hwnd, GWL_EXSTYLE));
    int newW = rc.right - rc.left;
    int newH = rc.bottom - rc.top;
    RECT oldRc;
    GetWindowRect(m_hwnd, &oldRc);
    int newX = oldRc.left;
    int newY = oldRc.top;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (newX + newW > screenW) newX = screenW - newW;
    if (newX < 0) newX = 0;
    if (newY + newH > screenH) newY = screenH - newH;
    if (newY < 0) newY = 0;
    SetWindowPos(m_hwnd, nullptr, newX, newY, newW, newH, SWP_NOZORDER | SWP_NOACTIVATE);
}



void MainWindow::PushUndoState() {
    m_undoStack.push_back(m_config);
    m_canUndo = true;
    m_changeCount = static_cast<int>(m_undoStack.size());
}

DWORD WINAPI MainWindow::CheckForUpdatesThread(LPVOID lpParam) {
    MainWindow* self = reinterpret_cast<MainWindow*>(lpParam);
    
    HINTERNET hSession = WinHttpOpen(L"IdleDimmer/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (hSession) {
        HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
                L"/repos/UnDadFeated/IdleDimmer/releases/latest", nullptr, nullptr, nullptr,
                WINHTTP_FLAG_SECURE);
            if (hRequest) {
                if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0)) {
                    if (WinHttpReceiveResponse(hRequest, nullptr)) {
                        DWORD size = 0;
                        WinHttpQueryDataAvailable(hRequest, &size);
                        if (size > 0) {
                            std::vector<char> buf(size + 1);
                            DWORD read = 0;
                            if (WinHttpReadData(hRequest, buf.data(), size, &read)) {
                                buf[read] = 0;
                                const char* tag = strstr(buf.data(), "\"tag_name\"");
                                if (tag) {
                                    tag += 10; // skip "tag_name"
                                    while (*tag == ' ' || *tag == '\t' || *tag == ':') {
                                        tag++;
                                    }
                                    if (*tag == '\"') tag++;
                                    if (*tag == 'v') tag++;
                                    
                                    const char* end = strchr(tag, '\"');
                                    if (end) {
                                        int len = static_cast<int>(end - tag);
                                        if (len > 0 && len < 20) {
                                            wchar_t ver[32] = { 0 };
                                            MultiByteToWideChar(CP_UTF8, 0, tag, len, ver, 32);
                                            wchar_t* latestVer = new wchar_t[32];
                                            StringCchPrintfW(latestVer, 32, L"v%s", ver);
                                            BOOL posted = FALSE;
                                            if (CompareVersion(latestVer, self->m_appVersion.c_str()) > 0)
                                                posted = PostMessageW(self->m_hwnd, WM_UPDATE_CHECK, reinterpret_cast<WPARAM>(latestVer), 1);
                                            else
                                                posted = PostMessageW(self->m_hwnd, WM_UPDATE_CHECK, reinterpret_cast<WPARAM>(latestVer), 0);
                                            
                                            if (!posted) {
                                                delete[] latestVer;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    }
    
    self->m_updateChecked = true;
    PostMessageW(self->m_hwnd, WM_UPDATE_CHECK, 0, 0);
    return 0;
}

void MainWindow::OnUpdateCheckComplete(WPARAM wp, LPARAM lp) {
    wchar_t* latestVer = reinterpret_cast<wchar_t*>(wp);
    if (latestVer) {
        m_latestVersion = latestVer;
        m_updateAvailable = (lp != 0);
        delete[] latestVer;
    }
    m_updateChecked = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}



void MainWindow::LoadSettings() {
    m_config = ConfigManager::LoadConfig(ConfigManager::GetConfigPath());
    DimmerManager::Instance().SetBlockedApps(m_config.blockedApps);
}

void MainWindow::SaveSettings() {
    ConfigManager::SaveConfig(ConfigManager::GetConfigPath(), m_config);
}

void MainWindow::ShowAddAppDialog() {
    if (!g_addDlgRegistered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = AddAppDlgProc;
        wc.hInstance = m_hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = ADD_DLG_CLASS;
        if (!RegisterClassExW(&wc)) {
            LogError(ErrorCode::E214, HRESULT_FROM_WIN32(GetLastError()));
            return;
        }
        g_addDlgRegistered = true;
    }

    g_addDlgConfirmed = false;
    g_addDlgResult[0] = 0;

    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, ADD_DLG_CLASS, L"Add Blocked App",
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 270, 105,
        m_hwnd, NULL, m_hInst, NULL);

    if (!hDlg) {
        LogError(ErrorCode::E215, HRESULT_FROM_WIN32(GetLastError()));
        return;
    }

    EnableWindow(m_hwnd, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(m_hwnd, TRUE);
    SetForegroundWindow(m_hwnd);

    if (g_addDlgConfirmed && g_addDlgResult[0]) {
        std::wstring name = g_addDlgResult;
        CharLowerW(&name[0]);
        if (name.find(L'.') == std::wstring::npos) name += L".exe";
        bool dup = false;
        for (const auto& app : m_config.blockedApps) {
            if (lstrcmpiW(app.c_str(), name.c_str()) == 0) { dup = true; break; }
        }
        if (!dup) {
            m_config.blockedApps.push_back(name);
            DimmerManager::Instance().SetBlockedApps(m_config.blockedApps);
            UpdateLayout();
            SaveSettings();
            Repaint();
        }
    }
}

void MainWindow::SyncMonitorsWithConfig() {
    const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
    for (const auto& mon : activeMons) {
        bool found = false;
        for (auto& savedMon : m_config.monitors) {
            if (savedMon.id == mon.id) {
                DimmerManager::Instance().SetMonitorDim(mon.id, savedMon.value);
                DimmerManager::Instance().SetMonitorEnabled(mon.id, savedMon.enabled);
                found = true;
                break;
            }
        }
        if (!found) {
            // New monitor, add to config list
            MonitorConfig newMon;
            newMon.id = mon.id;
            newMon.value = m_config.masterValue;
            newMon.enabled = true;
            m_config.monitors.push_back(newMon);
            DimmerManager::Instance().SetMonitorDim(mon.id, newMon.value);
        }
    }
    SaveSettings();
}

void MainWindow::ToggleStartWithWindows(bool enable) {
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey);
    if (lRes == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            LONG lResSet = RegSetValueExW(hKey, L"IdleDimmer", 0, REG_SZ, reinterpret_cast<const BYTE*>(exePath), static_cast<DWORD>((wcslen(exePath) + 1) * sizeof(wchar_t)));
            if (lResSet != ERROR_SUCCESS) {
                LogError(ErrorCode::E306, HRESULT_FROM_WIN32(lResSet));
            }
        } else {
            LONG lResDel = RegDeleteValueW(hKey, L"IdleDimmer");
            if (lResDel != ERROR_SUCCESS && lResDel != ERROR_FILE_NOT_FOUND) {
                LogError(ErrorCode::E307, HRESULT_FROM_WIN32(lResDel));
            }
        }
        RegCloseKey(hKey);
    } else {
        LogError(ErrorCode::E305, HRESULT_FROM_WIN32(lRes));
    }
}

void MainWindow::OnResize(UINT width, UINT height) {
    if (m_pRenderTarget) {
        D2D1_SIZE_U size = D2D1::SizeU(width, height);
        m_pRenderTarget->Resize(size);
    }
}

MainWindow::~MainWindow() {
    KillTimer(m_hwnd, 202);
    UnregisterHotKey(m_hwnd, 101);
    UnregisterHotKey(m_hwnd, 102);
    UnregisterHotKey(m_hwnd, 103);
    RemoveTrayIcon(m_hwnd, 1);
    DiscardGraphicsResources();
    if (m_pFactory) m_pFactory->Release();
    if (m_pDWriteFactory) m_pDWriteFactory->Release();
    if (m_pTextFormatTitle) m_pTextFormatTitle->Release();
    if (m_pTextFormatBody) m_pTextFormatBody->Release();
    if (m_pTextFormatDetail) m_pTextFormatDetail->Release();
    if (m_hUpdateThread) {
        WaitForSingleObject(m_hUpdateThread, 1000);
        CloseHandle(m_hUpdateThread);
        m_hUpdateThread = nullptr;
    }
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        switch (msg) {
            case WM_PAINT: {
                self->OnPaint();
                ValidateRect(hwnd, nullptr);
                return 0;
            }
            case WM_SIZE: {
                self->OnResize(LOWORD(lp), HIWORD(lp));
                return 0;
            }
            case WM_MOUSEMOVE: {
                self->HandleMouseMove(static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
                return 0;
            }
            case WM_LBUTTONDOWN: {
                self->HandleLButtonDown(static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
                return 0;
            }
            case WM_LBUTTONUP: {
                self->HandleLButtonUp(static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
                return 0;
            }
            case WM_MOUSEWHEEL: {
                self->HandleMouseWheel(GET_WHEEL_DELTA_WPARAM(wp), static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
                return 0;
            }
            case WM_KEYDOWN: {
                self->HandleKeyDown(wp);
                return 0;
            }
            case WM_HOTKEY: {
                if (wp == 101 || wp == 102) {
                    self->PushUndoState();
                    int delta = (wp == 101) ? -5 : 5;
                    self->m_config.masterValue += delta;
                    if (self->m_config.masterValue < 0) self->m_config.masterValue = 0;
                    if (self->m_config.masterValue > 90) self->m_config.masterValue = 90;

                    // If active dimming is off, auto-enable it so the hotkey takes effect immediately!
                    if (!self->m_config.dimmingEnabled) {
                        self->m_config.dimmingEnabled = true;
                        DimmerManager::Instance().SetDimmingEnabled(true);
                        for (auto& cb : self->m_checkboxes) {
                            if (cb.settingName == L"DimmingEnabled") { cb.checked = true; break; }
                        }
                    }

                    for (const auto& mon : DimmerManager::Instance().GetActiveMonitors()) {
                        DimmerManager::Instance().SetMonitorDim(mon.id, self->m_config.masterValue);
                    }
                    for (auto& monConf : self->m_config.monitors) {
                        monConf.value = self->m_config.masterValue;
                    }
                    for (auto& sl : self->m_sliders) {
                        if (!sl.isIdleMinutes && !sl.isIdleDimLevel) {
                            sl.value = self->m_config.masterValue / 90.0f;
                        }
                    }
                    DimmerManager::Instance().UpdateCursorDimming();
                    self->SaveSettings();
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else if (wp == 103) {
                    self->PushUndoState();
                    self->m_config.masterEnabled = !self->m_config.masterEnabled;
                    self->m_config.dimmingEnabled = self->m_config.masterEnabled;
                    DimmerManager::Instance().SetDimmingEnabled(self->m_config.masterEnabled);
                    for (const auto& mon : DimmerManager::Instance().GetActiveMonitors()) {
                        DimmerManager::Instance().SetMonitorEnabled(mon.id, self->m_config.masterEnabled);
                    }
                    for (auto& monConf : self->m_config.monitors) {
                        monConf.enabled = self->m_config.masterEnabled;
                    }
                    for (auto& sl : self->m_sliders) {
                        sl.active = self->m_config.masterEnabled;
                    }
                    for (auto& cb : self->m_checkboxes) {
                        if (cb.settingName == L"MasterEnabled" || !cb.monitorId.empty() || cb.settingName == L"DimmingEnabled") {
                            cb.checked = self->m_config.masterEnabled;
                        }
                    }
                    DimmerManager::Instance().UpdateCursorDimming();
                    self->SaveSettings();
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            case WM_TIMER: {
                if (wp == 202) {
                    DimmerManager::Instance().CheckVideoPlayback();
                    if (self->m_config.idleDimEnabled) {
                        LASTINPUTINFO lii = { 0 };
                        lii.cbSize = sizeof(lii);
                        if (GetLastInputInfo(&lii)) {
                            DWORD idleTime = GetTickCount() - lii.dwTime;
                            DWORD threshold = self->m_config.idleMinutes * 60 * 1000;
                            if (idleTime >= threshold) {
                                if (!DimmerManager::Instance().IsIdleState()) {
                                    DimmerManager::Instance().SetIdleState(true, self->m_config.idleDimLevel);
                                    if (self->m_config.idleTurnOff) {
                                        HWND hShell = FindWindowW(L"Shell_TrayWnd", NULL);
                                        if (hShell) {
                                            PostMessageW(hShell, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
                                        } else {
                                            SendMessageW(GetDesktopWindow(), WM_SYSCOMMAND, SC_MONITORPOWER, 2);
                                        }
                                    }
                                }
                            } else {
                                if (DimmerManager::Instance().IsIdleState()) {
                                    DimmerManager::Instance().SetIdleState(false);
                                }
                            }
                        }
                    } else if (DimmerManager::Instance().IsIdleState()) {
                        DimmerManager::Instance().SetIdleState(false);
                    }
                }
                return 0;
            }
            case WM_DISPLAYCHANGE: {
                DimmerManager::Instance().RefreshMonitors();
                self->SyncMonitorsWithConfig();
                self->UpdateLayout();
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
            case WM_UPDATE_CHECK: {
                self->OnUpdateCheckComplete(wp, lp);
                return 0;
            }
            case WM_TRAYICON: {
                if (lp == WM_RBUTTONUP) {
                    POINT pt;
                    GetCursorPos(&pt);
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"Open Settings");
                    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit IdleDimmer");
                    
                    // Display popup menu matching dark theme styling (as much as standard Win32 popup menu allows)
                    SetForegroundWindow(hwnd);
                    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                    DestroyMenu(hMenu);
                } else if (lp == WM_LBUTTONDBLCLK) {
                    self->Show(true);
                }
                return 0;
            }
            case WM_COMMAND: {
                int wmId = LOWORD(wp);
                if (wmId == ID_TRAY_SHOW) {
                    self->Show(true);
                } else if (wmId == ID_TRAY_EXIT) {
                    DestroyWindow(hwnd);
                }
                return 0;
            }
            case WM_CLOSE: {
                if (self->m_config.closeToTray) {
                    self->Show(false);
                } else {
                    DestroyWindow(hwnd);
                }
                return 0;
            }
            case WM_DESTROY: {
                PostQuitMessage(0);
                return 0;
            }
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
