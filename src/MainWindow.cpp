#define NOMINMAX
#include "MainWindow.h"
#include "ErrorCodes.h"
#include "DimmerManager.h"
#include <algorithm>
#include <dwmapi.h>
#include <shellapi.h>
#include <strsafe.h>
#include <winhttp.h>
#include <winver.h>
#include <format>

#ifndef D2DERR_RECREATED
#define D2DERR_RECREATED ((HRESULT)0x88980001L)
#endif

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comdlg32.lib")

static const wchar_t* APP_VERSION = L"v1.8.4";

static int CompareVersion(const wchar_t* verA, const wchar_t* verB) {
    int majA = 0, minA = 0, patA = 0;
    int majB = 0, minB = 0, patB = 0;
    // C++23: Manual parsing to avoid swscanf
    swscanf(verA, L"v%d.%d.%d", &majA, &minA, &patA);
    swscanf(verB, L"v%d.%d.%d", &majB, &minB, &patB);
    if (majA != majB) return majA - majB;
    if (minA != minB) return minA - minB;
    return patA - patB;
}

static std::wstring GetOwnVersion() {
    wchar_t filepath[MAX_PATH];
    DWORD pathLen = GetModuleFileNameW(nullptr, filepath, MAX_PATH);
    if (pathLen == 0 || pathLen >= MAX_PATH) {
        return L"unknown";
    }
    filepath[pathLen] = L'\0';
    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeW(filepath, &dummy);
    if (size == 0) return L"unknown";
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(filepath, 0, size, data.data())) return L"unknown";
    VS_FIXEDFILEINFO* pFileInfo = nullptr;
    UINT len = 0;
    if (VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&pFileInfo), &len) && len > 0 && pFileInfo) {
        return std::format(L"v{}.{}.{}",
            static_cast<int>(HIWORD(pFileInfo->dwProductVersionMS)),
            static_cast<int>(LOWORD(pFileInfo->dwProductVersionMS)),
            static_cast<int>(HIWORD(pFileInfo->dwProductVersionLS)));
    }
    return L"unknown";
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
    wcscpy_s(nid.szTip, ARRAYSIZE(nid.szTip), tip);
    return Shell_NotifyIconW(NIM_ADD, &nid);
}

static bool RemoveTrayIcon(HWND hwnd, UINT uID) {
    NOTIFYICONDATAW nid = { 0 };
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = uID;
    return Shell_NotifyIconW(NIM_DELETE, &nid);
}

// C++ exception wrapper for CreateImpl. Must be a member so it can access
// private CreateImpl(); kept in a separate method because clang/MinGW does
// not allow nesting C++ `try` inside SEH `__try` in the same function.
bool MainWindow::CreateImplSafe(MainWindow* self, HINSTANCE hInst, int nCmdShow) {
    try {
        return self->CreateImpl(hInst, nCmdShow);
    } catch (...) {
        LogError(ErrorCode::E108);
        return false;
    }
}

bool MainWindow::Create(HINSTANCE hInst, int nCmdShow) {
    __try {
        return CreateImplSafe(this, hInst, nCmdShow);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        LogError(ErrorCode::E108, static_cast<HRESULT>(GetExceptionCode()));
        return false;
    }
}

bool MainWindow::CreateImpl(HINSTANCE hInst, int nCmdShow) {
    m_hInst = hInst;
    LoadSettings();

    // ── v1.6.5: OLED preset is the default for first launch ──
    // Apply only on first launch — existing user configs are preserved.
    // IMPORTANT: We set config fields directly here instead of calling
    // ApplyPreset(0), because DimmerManager is not initialized yet at this
    // point — ApplyPreset calls SetDimmingEnabled/SetWarmTint/SyncMonitors
    // which all require a running DimmerManager.
    if (GetFileAttributesW(ConfigManager::GetConfigPath().c_str()) == INVALID_FILE_ATTRIBUTES) {
        m_config.dimmingEnabled = false;  // AGENTS.md: never auto-dim on launch
        m_config.warmTint       = false;
        m_config.masterValue    = 90;
        m_config.masterEnabled  = true;
        for (auto& mon : m_config.monitors) {
            mon.value   = 90;
            mon.enabled = true;
        }
    }

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

    UINT systemDpi = GetDpiForSystem();
    if (systemDpi == 0) systemDpi = 96;
    float systemScale = systemDpi / 96.0f;

    int scaledW = static_cast<int>(m_windowWidth * systemScale);
    int scaledH = static_cast<int>(m_windowHeight * systemScale);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (m_config.showInTaskbar) {
        // Standard window
    } else {
        style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    }

    RECT rc = { 0, 0, scaledW, scaledH };
    AdjustWindowRectExForDpi(&rc, style, FALSE, WS_EX_APPWINDOW, systemDpi);
    int windowW = rc.right - rc.left;
    int windowH = rc.bottom - rc.top;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - windowW) / 2;
    int y = (screenH - windowH) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"IdleDimmerMainClass",
        L"IdleDimmer - Control Panel",
        style,
        x, y, windowW, windowH,
        nullptr, nullptr, hInst, this
    );

    if (!m_hwnd) {
        LogError(ErrorCode::E107, HRESULT_FROM_WIN32(GetLastError()));
        return false;
    }

    // Enable Windows 11 rounded corners and dark/light theme
    BOOL useDark = !m_config.lightMode;
    HRESULT hrDark = DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
    if (FAILED(hrDark)) {
        OutputDebugStringW(L"[IdleDimmer] DwmSetWindowAttribute (DWMWA_USE_IMMERSIVE_DARK_MODE) failed.\n");
    }
    
    DWORD cornerPreference = DWMWCP_ROUND;
    HRESULT hrCorner = DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
    if (FAILED(hrCorner)) {
        OutputDebugStringW(L"[IdleDimmer] DwmSetWindowAttribute (DWMWA_WINDOW_CORNER_PREFERENCE) failed.\n");
    }


    m_appVersion = GetOwnVersion();

    // Defer all remotely-blocking operations to WM_APP+2 so the message loop
    // starts BEFORE any D2D/COM/tray/DWM interactions. Certification VMs on
    // Win11 24H2+ (build 26200+) have different DWM timing and WDDM 3.2 GPU
    // driver behavior that can cause synchronous D2D init / Shell_NotifyIcon /
    // overlay-window creation to hang when called before the message loop.
    //
    // Only Initialize() (window-class registration) stays synchronous — it is
    // a single RegisterClassExW call with zero IPC or DWM involvement.
    DimmerManager::Instance().Initialize(hInst);

    PostMessageW(m_hwnd, WM_APP + 2, static_cast<WPARAM>(nCmdShow), 0);

    return true;
}

UINT MainWindow::GetWindowDpi() const {
    if (!m_hwnd) return 96;
    UINT dpi = 0;
    if (IsWindowVisible(m_hwnd)) {
        dpi = GetDpiForWindow(m_hwnd);
    }
    if (dpi == 0) {
        dpi = GetDpiForSystem();
    }
    return (dpi > 0) ? dpi : 96;
}

float MainWindow::GetDpiScale() const {
    return GetWindowDpi() / 96.0f;
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
            return hr;
        }
    }
    if (!m_pRenderTarget) {
        UINT dpi = GetWindowDpi();
        float scale = dpi / 96.0f;

        int scaledW = static_cast<int>(m_windowWidth * scale);
        int scaledH = static_cast<int>(m_windowHeight * scale);
        RECT rcAdjust = { 0, 0, scaledW, scaledH };
        AdjustWindowRectExForDpi(&rcAdjust, GetWindowLongW(m_hwnd, GWL_STYLE), FALSE, GetWindowLongW(m_hwnd, GWL_EXSTYLE), dpi);
        int newW = rcAdjust.right - rcAdjust.left;
        int newH = rcAdjust.bottom - rcAdjust.top;

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

        RECT rc;
        GetClientRect(m_hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(
            std::max<UINT32>(1, rc.right - rc.left),
            std::max<UINT32>(1, rc.bottom - rc.top)
        );
        
        hr = m_pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &m_pRenderTarget
        );

        if (FAILED(hr)) {
            LogError(ErrorCode::E206, hr);
            return hr;
        }

        dpi = GetWindowDpi();
        if (dpi > 0) {
            m_pRenderTarget->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
        }

        // Create brushes
        hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x131315), &m_pBrushBg);
        if (FAILED(hr)) { LogError(ErrorCode::E207, hr); DiscardGraphicsResources(); return hr; }

        hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x1F1F24), &m_pBrushCard);
        if (FAILED(hr)) { LogError(ErrorCode::E207, hr); DiscardGraphicsResources(); return hr; }

        hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x2D2D34), &m_pBrushCardBorder);
        if (FAILED(hr)) { LogError(ErrorCode::E207, hr); DiscardGraphicsResources(); return hr; }

        hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF), &m_pBrushText);
        if (FAILED(hr)) { LogError(ErrorCode::E207, hr); DiscardGraphicsResources(); return hr; }

        hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x8E8E93), &m_pBrushTextMuted);
        if (FAILED(hr)) { LogError(ErrorCode::E207, hr); DiscardGraphicsResources(); return hr; }

        hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x0078D4), &m_pBrushAccent);
        if (FAILED(hr)) { LogError(ErrorCode::E207, hr); DiscardGraphicsResources(); return hr; }

        hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x2B88D8), &m_pBrushAccentHover);
        if (FAILED(hr)) { LogError(ErrorCode::E207, hr); DiscardGraphicsResources(); return hr; }

        hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x44444A), &m_pBrushTrack);
        if (FAILED(hr)) { LogError(ErrorCode::E207, hr); DiscardGraphicsResources(); return hr; }
    }

    if (!m_pDWriteFactory || !m_pTextFormatTitle || !m_pTextFormatBody || !m_pTextFormatDetail) {
        // Clean up first if any is missing to prevent partial initialization issues
        if (m_pTextFormatTitle) { m_pTextFormatTitle->Release(); m_pTextFormatTitle = nullptr; }
        if (m_pTextFormatBody) { m_pTextFormatBody->Release(); m_pTextFormatBody = nullptr; }
        if (m_pTextFormatDetail) { m_pTextFormatDetail->Release(); m_pTextFormatDetail = nullptr; }
        if (!m_pDWriteFactory) {
            hr = DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
            );
            if (FAILED(hr)) {
                LogError(ErrorCode::E202, hr);
                return hr;
            }
        }

        hr = m_pDWriteFactory->CreateTextFormat(
            L"Segoe UI Variable Display", nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            20.0f, L"en-us", &m_pTextFormatTitle
        );
        if (FAILED(hr)) {
            // Fallback for systems without Segoe UI Variable
            hr = m_pDWriteFactory->CreateTextFormat(
                L"Segoe UI", nullptr,
                DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                20.0f, L"en-us", &m_pTextFormatTitle
            );
        }
        if (FAILED(hr)) { LogError(ErrorCode::E203, hr); DiscardGraphicsResources(); return hr; }

        hr = m_pDWriteFactory->CreateTextFormat(
            L"Segoe UI Variable Text", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            13.0f, L"en-us", &m_pTextFormatBody
        );
        if (FAILED(hr)) {
            hr = m_pDWriteFactory->CreateTextFormat(
                L"Segoe UI", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                13.0f, L"en-us", &m_pTextFormatBody
            );
        }
        if (FAILED(hr)) { LogError(ErrorCode::E204, hr); DiscardGraphicsResources(); return hr; }

        hr = m_pDWriteFactory->CreateTextFormat(
            L"Segoe UI Variable Text", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            10.5f, L"en-us", &m_pTextFormatDetail
        );
        if (FAILED(hr)) {
            hr = m_pDWriteFactory->CreateTextFormat(
                L"Segoe UI", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                10.5f, L"en-us", &m_pTextFormatDetail
            );
        }
        if (FAILED(hr)) { LogError(ErrorCode::E205, hr); DiscardGraphicsResources(); return hr; }
    }

    return hr;
}

void MainWindow::DiscardGraphicsResources() {
    // If D2D resources are discarded at runtime (device loss / D2DERR_RECREATED),
    // reset the ready flag so OnPaint falls back to GDI and WM_APP+4 retries.
    m_d2dReady = false;
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
    m_presets.clear();
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

    // ── v1.6.5 (Todo 5): Preset Buttons Row ──
    // 4 compact monochrome buttons: [OLED] [Gaming] [Reading] [Night]
    yOffset += 6;
    yOffset += 16; // space for section header label
    {
        int btnW = (CONTENT_WIDTH - 40 - 35 - 18) / 4;  // 4 buttons, 6px gaps
        int btnH = 28;
        int gap  = 6;
        const wchar_t* names[4] = { L"OLED", L"Gaming", L"Reading", L"Night" };
        for (int i = 0; i < 4; ++i) {
            UIPresetButton btn;
            btn.id = i;
            btn.label = names[i];
            btn.rect.left   = 20 + i * (btnW + gap);
            btn.rect.top    = yOffset;
            btn.rect.right  = btn.rect.left + btnW;
            btn.rect.bottom = btn.rect.top + btnH;
            m_presets.push_back(btn);
        }
        yOffset = m_presets.back().rect.bottom + 10;
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

    // ── SCHEDULE section (v1.6.5 Todo 8) ──
    yOffset += 8;
    yOffset += 16; // space for section header label
    AddCheckbox(L"ScheduleEnabled", m_config.scheduleEnabled,
                &m_config.scheduleEnabled, L"Schedule Dimming", 0, yOffset);
    yOffset += 22;

    if (m_config.scheduleEnabled) {
        // Two sliders: Start time and End time, snapped to 15-minute intervals.
        // Slider value = minutesOfDay / 1439. On commit we round to nearest 15 mins.
        UISlider startSlider;
        startSlider.isScheduleStart = true;
        startSlider.value = m_config.scheduleStartMins / 1439.0f;
        startSlider.active = true;
        startSlider.rect.left = 20;
        startSlider.rect.top = yOffset + 4;
        startSlider.rect.right = m_windowWidth - 35;
        startSlider.rect.bottom = startSlider.rect.top + 58;
        m_sliders.push_back(startSlider);

        UISlider endSlider;
        endSlider.isScheduleEnd = true;
        endSlider.value = m_config.scheduleEndMins / 1439.0f;
        endSlider.active = true;
        endSlider.rect.left = 20;
        endSlider.rect.top = startSlider.rect.bottom + 10;
        endSlider.rect.right = m_windowWidth - 35;
        endSlider.rect.bottom = endSlider.rect.top + 58;
        m_sliders.push_back(endSlider);

        yOffset = endSlider.rect.bottom + 4;
    }

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

    // v1.6.5 (Todo 6): Import / Export profile buttons below the panel header.
    int profileY = panelTop + 32;
    int profileH = 22;
    m_importProfileRect = { panelLeft + 10, profileY,
                            panelLeft + 10 + (PANEL_WIDTH - 30) / 2, profileY + profileH };
    m_exportProfileRect = { m_importProfileRect.right + 6, profileY,
                            panelRight - 10, profileY + profileH };

    m_blockedItems.clear();
    m_blockedContentHeight = 0;
    if (m_blockedExpanded) {
        int itemY = panelTop + 70; // leave room for header + import/export row
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

    UINT dpi = GetWindowDpi();
    float scale = dpi / 96.0f;
    int scaledW = static_cast<int>(m_windowWidth * scale);
    int scaledH = static_cast<int>(m_windowHeight * scale);

    RECT rc = { 0, 0, scaledW, scaledH };
    AdjustWindowRectExForDpi(&rc, GetWindowLongW(m_hwnd, GWL_STYLE), FALSE, GetWindowLongW(m_hwnd, GWL_EXSTYLE), dpi);
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
    self->m_hSession = hSession;
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
                        if (WinHttpQueryDataAvailable(hRequest, &size) && size > 0) {
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
                                            wcscpy_s(latestVer, 32, std::format(L"v{}", ver).c_str());
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
        HINTERNET hSes = reinterpret_cast<HINTERNET>(InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(&self->m_hSession),
            nullptr
        ));
        if (hSes) {
            WinHttpCloseHandle(hSes);
        }
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

typedef LONG(WINAPI* PFN_GetCurrentPackageFullName)(UINT32*, PWSTR);

bool MainWindow::IsPackaged() const {
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (hKernel32) {
        auto pfn = reinterpret_cast<PFN_GetCurrentPackageFullName>(
            GetProcAddress(hKernel32, "GetCurrentPackageFullName")
        );
        if (pfn) {
            wchar_t name[128];
            UINT32 length = 128;
            LONG rc = pfn(&length, name);
            // APPMODEL_ERROR_NO_PACKAGE is 15700L
            return (rc != 15700L);
        }
    }
    return false;
}



void MainWindow::LoadSettings() {
    m_config = ConfigManager::LoadConfig(ConfigManager::GetConfigPath());
    DimmerManager::Instance().SetBlockedApps(m_config.blockedApps);
    // ── v1.6.5: push schedule settings into DimmerManager ──
    DimmerManager::Instance().SetScheduleRange(
        m_config.scheduleStartMins,
        m_config.scheduleEndMins,
        m_config.scheduleDimLevel);
    DimmerManager::Instance().SetScheduleEnabled(m_config.scheduleEnabled);
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
    if (IsPackaged()) {
        return;
    }
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
        D2D1_SIZE_U size = D2D1::SizeU(std::max<UINT32>(1, width), std::max<UINT32>(1, height));
        m_pRenderTarget->Resize(size);
    }
}

MainWindow::~MainWindow() {
    KillTimer(m_hwnd, 202);
    KillTimer(m_hwnd, 203);
    RemoveTrayIcon(m_hwnd, 1);
    DiscardGraphicsResources();
    if (m_pFactory) m_pFactory->Release();
    if (m_pDWriteFactory) m_pDWriteFactory->Release();
    if (m_pTextFormatTitle) m_pTextFormatTitle->Release();
    if (m_pTextFormatBody) m_pTextFormatBody->Release();
    if (m_pTextFormatDetail) m_pTextFormatDetail->Release();
    
    HINTERNET hSes = reinterpret_cast<HINTERNET>(InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(&m_hSession),
        nullptr
    ));
    if (hSes) {
        WinHttpCloseHandle(hSes);
    }
    if (m_hUpdateThread) {
        WaitForSingleObject(m_hUpdateThread, 3000);
        CloseHandle(m_hUpdateThread);
        m_hUpdateThread = nullptr;
    }
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        if (self) {
            self->m_hwnd = hwnd;
        }
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    // ── v1.7.7: Top-level SEH around the ENTIRE WndProc body ──
    // Previously only WM_PAINT was wrapped. Crashes from WM_APP+2 (deferred
    // init), WM_TIMER, WM_DISPLAYCHANGE, and WM_POWERBROADCAST all propagated
    // through DispatchMessageW uncaught — killing the process on cert VMs.
    if (self) {
        __try {
            return self->WndProcImpl(hwnd, msg, wp, lp);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LogError(ErrorCode::E211, static_cast<HRESULT>(GetExceptionCode()));
            // Return 0 for most messages so the pump keeps going.
            // WM_PAINT needs ValidateRect to prevent infinite repaint loops.
            if (msg == WM_PAINT) {
                ValidateRect(hwnd, nullptr);
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ── v1.7.7: Inner WndProc handler, called through the top-level __try/__except
// wrapper above. Separated so Clang/MinGW allows C++ try/catch here without
// mixing SEH and C++ exceptions in the same function body.
LRESULT MainWindow::WndProcImpl(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            OnPaint();
            // D2D paint crash is caught by the top-level SEH — on crash,
            // WndProc's __except calls ValidateRect, logs the error, and
            // returns 0. The next paint will use the GDI fallback path.
            ValidateRect(hwnd, nullptr);
            return 0;
        }
        case WM_SIZE: {
            OnResize(LOWORD(lp), HIWORD(lp));
            return 0;
        }
        case WM_DPICHANGED: {
            LPRECT lprcSuggested = (LPRECT)lp;
            SetWindowPos(hwnd,
                         nullptr,
                         lprcSuggested->left,
                         lprcSuggested->top,
                         lprcSuggested->right - lprcSuggested->left,
                         lprcSuggested->bottom - lprcSuggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            
            if (m_pRenderTarget) {
                UINT dpi = LOWORD(wp);
                m_pRenderTarget->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
            }
            
            UpdateLayout();
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        case WM_MOUSEMOVE: {
            float scale = GetDpiScale();
            int lx = static_cast<int>(static_cast<short>(LOWORD(lp)) / scale);
            int ly = static_cast<int>(static_cast<short>(HIWORD(lp)) / scale);
            HandleMouseMove(lx, ly);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            float scale = GetDpiScale();
            int lx = static_cast<int>(static_cast<short>(LOWORD(lp)) / scale);
            int ly = static_cast<int>(static_cast<short>(HIWORD(lp)) / scale);
            HandleLButtonDown(lx, ly);
            return 0;
        }
        case WM_LBUTTONUP: {
            float scale = GetDpiScale();
            int lx = static_cast<int>(static_cast<short>(LOWORD(lp)) / scale);
            int ly = static_cast<int>(static_cast<short>(HIWORD(lp)) / scale);
            HandleLButtonUp(lx, ly);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            HandleMouseWheel(GET_WHEEL_DELTA_WPARAM(wp), static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
            return 0;
        }
        case WM_KEYDOWN: {
            HandleKeyDown(wp);
            return 0;
        }
        case WM_TIMER: {
            if (wp == 202) {
                DimmerManager::Instance().CheckVideoPlayback();
                if (m_config.idleDimEnabled) {
                    LASTINPUTINFO lii = { 0 };
                    lii.cbSize = sizeof(lii);
                    if (GetLastInputInfo(&lii)) {
                        DWORD idleTime = GetTickCount() - lii.dwTime;
                        DWORD threshold = m_config.idleMinutes * 60 * 1000;
                        if (idleTime >= threshold) {
                            if (!DimmerManager::Instance().IsIdleState()) {
                                DimmerManager::Instance().SetIdleState(true, m_config.idleDimLevel);
                                if (m_config.idleTurnOff) {
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
            } else if (wp == 203) {
                KillTimer(hwnd, 203);
                if (!m_hUpdateThread) {
                    m_hUpdateThread = CreateThread(nullptr, 0, CheckForUpdatesThread, this, 0, nullptr);
                }
            } else if (wp == 204) {
                // Retry tray icon (may have failed during early startup
                // when Explorer was not yet ready to process it).
                if (!m_trayAdded) {
                    HICON hAppIcon = LoadIconW(m_hInst, MAKEINTRESOURCEW(101));
                    m_trayAdded = AddTrayIcon(hwnd, 1, hAppIcon, L"IdleDimmer Screen Brightness");
                    if (m_trayAdded) {
                        KillTimer(hwnd, 204);
                    }
                } else {
                    KillTimer(hwnd, 204);
                }
            }
            return 0;
        }
        case WM_APP + 2: {
            // ── v1.7.7: Deferred startup initialization ──
            // C++ try/catch wraps the whole body so that a C++ exception
            // (bad_alloc, runtime_error, etc.) from DimmerManager or
            // ConfigManager doesn't crash the process. The minimal
            // fallback ensures the tray icon + timers are live so the
            // process isn't invisible/dead to the cert tester.
            try {
                DimmerManager::Instance().RefreshMonitors();
                SyncMonitorsWithConfig();
                DimmerManager::Instance().SetWarmTint(m_config.warmTint);
                DimmerManager::Instance().SetDimmingEnabled(m_config.dimmingEnabled);
            } catch (...) {
                LogError(ErrorCode::E108);
                // Overlays failed — app still runs with a visible window
                // and tray icon below.
            }

            if (!SetTimer(hwnd, 202, 1000, nullptr)) {
                LogError(ErrorCode::E209, HRESULT_FROM_WIN32(GetLastError()));
            }

            if (!IsPackaged()) {
                SetTimer(hwnd, 203, 15000, nullptr);
            } else {
                m_updateChecked = true;
            }

            // Try tray icon once. If it fails (common during early startup
            // when Explorer is still loading), retry via timer 204.
            HICON hAppIcon = LoadIconW(m_hInst, MAKEINTRESOURCEW(101));
            m_trayAdded = AddTrayIcon(hwnd, 1, hAppIcon, L"IdleDimmer Screen Brightness");
            if (!m_trayAdded) {
                SetTimer(hwnd, 204, 4000, nullptr);
            }

            UpdateLayout();

            int nCmdShow = static_cast<int>(wp);
            if (m_config.startWithWindows && m_config.closeToTray) {
                ShowWindow(hwnd, SW_HIDE);
            } else {
                ShowWindow(hwnd, nCmdShow);
            }

            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        case WM_APP + 4: {
            // Async D2D initialization. D2D calls on WDDM 3.2 drivers
            // (build 26200+) can throw SEH access violations — caught by
            // the top-level WndProc __try/__except.
            if (!m_d2dReady && !m_d2dFailed) {
                HRESULT hr = CreateGraphicsResources();
                if (SUCCEEDED(hr)) {
                    m_d2dReady = true;
                    InvalidateRect(hwnd, nullptr, TRUE);
                } else {
                    m_d2dFailed = true;
                    DiscardGraphicsResources();
                }
            }
            return 0;
        }
        case WM_DISPLAYCHANGE: {
            try {
                DimmerManager::Instance().RefreshMonitors();
                SyncMonitorsWithConfig();
                UpdateLayout();
                InvalidateRect(hwnd, nullptr, TRUE);
            } catch (...) {
                LogError(ErrorCode::E108);
            }
            return 0;
        }
        case WM_POWERBROADCAST: {
            if (wp == PBT_APMRESUMESUSPEND || wp == PBT_APMRESUMEAUTOMATIC) {
                try {
                    DimmerManager::Instance().RefreshMonitors();
                    SyncMonitorsWithConfig();
                    UpdateLayout();
                    InvalidateRect(hwnd, nullptr, TRUE);
                } catch (...) {
                    LogError(ErrorCode::E108);
                }
            }
            return TRUE;
        }
        case WM_UPDATE_CHECK: {
            OnUpdateCheckComplete(wp, lp);
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
                Show(true);
            }
            return 0;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wp);
            if (wmId == ID_TRAY_SHOW) {
                Show(true);
            } else if (wmId == ID_TRAY_EXIT) {
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_CLOSE: {
            if (m_config.closeToTray) {
                Show(false);
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
    return DefWindowProcW(hwnd, msg, wp, lp);
}


// v1.6.5 (Todo 5): Preset Profile application
// ════════════════════════════════════════════════════════════════════════════
//
// Four curated presets. Each one mutates the live config in place and then
// calls SyncMonitorsWithConfig() so overlay dim values track the new master,
// plus SetWarmTint / SetDimmingEnabled on DimmerManager for immediate visual
// feedback. The change is undoable (caller has already pushed the undo state).
//
//   OLED    — dimming on,  warm tint off, master 90 (true-black OLED dimming)
//   Gaming  — dimming off, warm tint off, master 0  (vibrant, undimmed)
//   Reading — dimming on,  warm tint on,  master 30 (soothing low dim + amber)
//   Night   — dimming on,  warm tint on,  master 80 (heavy dim + amber)
void MainWindow::ApplyPreset(int id) {
    switch (id) {
        case 0: // OLED
            m_config.dimmingEnabled = true;
            m_config.warmTint       = false;
            m_config.masterValue    = 90;
            break;
        case 1: // Gaming
            m_config.dimmingEnabled = false;
            m_config.warmTint       = false;
            m_config.masterValue    = 0;
            break;
        case 2: // Reading
            m_config.dimmingEnabled = true;
            m_config.warmTint       = true;
            m_config.masterValue    = 30;
            break;
        case 3: // Night
            m_config.dimmingEnabled = true;
            m_config.warmTint       = true;
            m_config.masterValue    = 80;
            break;
        default:
            return;
    }

    // Propagate master value to every monitor.
    for (auto& mon : m_config.monitors) {
        mon.value   = m_config.masterValue;
        mon.enabled = true;
    }

    // Refresh live DimmerManager state.
    DimmerManager::Instance().SetDimmingEnabled(m_config.dimmingEnabled);
    DimmerManager::Instance().SetWarmTint(m_config.warmTint);
    SyncMonitorsWithConfig();

    // Reflect new checkbox states in the live checkbox vector so the next
    // repaint doesn't show stale toggle positions.
    for (auto& cb : m_checkboxes) {
        if (cb.settingName == L"DimmingEnabled") cb.checked = m_config.dimmingEnabled;
        else if (cb.settingName == L"WarmTint")  cb.checked = m_config.warmTint;
        else if (cb.settingName == L"MasterEnabled") cb.checked = true;
        else if (!cb.monitorId.empty()) cb.checked = true;
    }

    UpdateLayout();
    InvalidateRect(m_hwnd, nullptr, FALSE);

    // v1.6.5 (Todo 4): OSD feedback for the preset application.
    const wchar_t* presetNames[4] = { L"OLED", L"Gaming", L"Reading", L"Night" };
    if (id >= 0 && id < 4) {
        DimmerManager::Instance().ShowOSD(std::format(L"Preset: {}  |  {}%",
                         presetNames[id], 100 - m_config.masterValue));
    }
}

// ════════════════════════════════════════════════════════════════════════════
// v1.6.5 (Todo 6): Profile Import / Export via Win32 GetOpenFileNameW /
// GetSaveFileNameW. We reuse the existing ConfigManager SaveConfig/LoadConfig
// helpers, since the file format is identical — an exported profile is just
// a dimmer.ini file written to a user-chosen path.
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::ShowExportProfileDialog() {
    wchar_t szFile[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwnd;
    ofn.lpstrFile   = szFile;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrFilter = L"IdleDimmer Profile (*.ini)\0*.ini\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"ini";
    ofn.lpstrTitle  = L"Export IdleDimmer Profile";
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (GetSaveFileNameW(&ofn)) {
        ConfigManager::SaveConfig(szFile, m_config);
        DimmerManager::Instance().ShowOSD(L"Profile exported");
    }
}

void MainWindow::ShowImportProfileDialog() {
    wchar_t szFile[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwnd;
    ofn.lpstrFile   = szFile;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrFilter = L"IdleDimmer Profile (*.ini)\0*.ini\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle  = L"Import IdleDimmer Profile";
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileNameW(&ofn)) {
        // Preserve monitor IDs that are currently attached — overwrite only
        // the values/enabled flags from the imported file for known monitors,
        // and import everything else (closeToTray, warmTint, etc.) wholesale.
        AppConfig imported = ConfigManager::LoadConfig(szFile);
        m_config = imported;

        DimmerManager::Instance().SetBlockedApps(m_config.blockedApps);
        DimmerManager::Instance().SetWarmTint(m_config.warmTint);
        DimmerManager::Instance().SetDimmingEnabled(m_config.dimmingEnabled);
        DimmerManager::Instance().SetScheduleRange(
            m_config.scheduleStartMins, m_config.scheduleEndMins, m_config.scheduleDimLevel);
        DimmerManager::Instance().SetScheduleEnabled(m_config.scheduleEnabled);
        DimmerManager::Instance().CheckSchedule();

        SyncMonitorsWithConfig();
        UpdateLayout();
        SaveSettings();
        InvalidateRect(m_hwnd, nullptr, FALSE);

        DimmerManager::Instance().ShowOSD(L"Profile imported");
    }
}
