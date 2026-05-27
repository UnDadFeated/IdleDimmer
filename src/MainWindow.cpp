#include "MainWindow.h"
#include "DimmerManager.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <strsafe.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")

#define WM_TRAYICON (WM_USER + 100)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SHOW 1002

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

    // Register MainWindow Class
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"WinDimmer64MainClass";
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

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
        L"WinDimmer64MainClass",
        L"WinDimmer64 - Control Panel",
        style,
        x, y, m_windowWidth, m_windowHeight,
        nullptr, nullptr, hInst, this
    );

    if (!m_hwnd) return false;

    // Enable Windows 11 rounded corners and dark theme
    BOOL useDark = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
    
    DWORD cornerPreference = DWMWCP_ROUND;
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));

    // Initialize D2D
    if (FAILED(CreateGraphicsResources())) {
        return false;
    }

    DimmerManager::Instance().Initialize(hInst);
    DimmerManager::Instance().RefreshMonitors();

    // Restore screen boundary settings
    DimmerManager::Instance().SetShowBoundaries(m_config.showBoundaries);

    // Synchronize monitor settings from loaded configuration
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

    // Apply warm tint and focus mode settings
    DimmerManager::Instance().SetWarmTint(m_config.warmTint);
    DimmerManager::Instance().SetFocusMode(m_config.focusMode);

    // Register global hotkeys
    RegisterHotKey(m_hwnd, 101, MOD_CONTROL | MOD_ALT, VK_UP);
    RegisterHotKey(m_hwnd, 102, MOD_CONTROL | MOD_ALT, VK_DOWN);
    RegisterHotKey(m_hwnd, 103, MOD_CONTROL | MOD_ALT, 0x44); // 'D' key

    // Start Focus Mode checking timer (150ms interval)
    SetTimer(m_hwnd, 201, 150, nullptr);

    // Start Inactivity/Idle checking timer (1000ms interval)
    SetTimer(m_hwnd, 202, 1000, nullptr);

    // Add to system tray
    AddTrayIcon(m_hwnd, 1, LoadIcon(nullptr, IDI_APPLICATION), L"WinDimmer64 Screen Brightness");

    UpdateLayout();

    ShowWindow(m_hwnd, nCmdShow);
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

        if (SUCCEEDED(hr)) {
            // Create brushes
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x131315), &m_pBrushBg);
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x1F1F24), &m_pBrushCard);
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x2D2D34), &m_pBrushCardBorder);
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF), &m_pBrushText);
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x8E8E93), &m_pBrushTextMuted);
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x0078D4), &m_pBrushAccent);
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x2B88D8), &m_pBrushAccentHover);
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x44444A), &m_pBrushTrack);
        }
    }

    if (SUCCEEDED(hr) && !m_pDWriteFactory) {
        hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
        );

        if (SUCCEEDED(hr)) {
            m_pDWriteFactory->CreateTextFormat(
                L"Segoe UI Variable Text", nullptr,
                DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                18.0f, L"en-us", &m_pTextFormatTitle
            );
            m_pDWriteFactory->CreateTextFormat(
                L"Segoe UI Variable Text", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                14.0f, L"en-us", &m_pTextFormatBody
            );
            m_pDWriteFactory->CreateTextFormat(
                L"Segoe UI Variable Text", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                11.0f, L"en-us", &m_pTextFormatDetail
            );
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

    const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();

    // 1. Header (WinDimmer64 Title)
    // Offset standard positions
    int yOffset = 20;

    // 2. Master Slider Card (If multiple screens)
    if (activeMons.size() > 1) {
        UISlider master;
        master.isMaster = true;
        master.value = m_config.masterValue / 90.0f;
        master.active = m_config.masterEnabled;
        
        master.rect.left = 20;
        master.rect.top = yOffset + 50;
        master.rect.right = m_windowWidth - 35;
        master.rect.bottom = master.rect.top + 80;

        m_sliders.push_back(master);

        UICheckbox mcb;
        mcb.settingName = L"MasterEnabled";
        mcb.checked = m_config.masterEnabled;
        mcb.rect.left = master.rect.left + 15;
        mcb.rect.top = master.rect.top + 15;
        mcb.rect.right = mcb.rect.left + 20;
        mcb.rect.bottom = mcb.rect.top + 20;
        m_checkboxes.push_back(mcb);

        yOffset += 100;
    }

    // 3. Individual Screen Sliders
    for (const auto& mon : activeMons) {
        UISlider slider;
        slider.monitorId = mon.id;
        slider.value = mon.dimValue / 90.0f;
        slider.active = mon.enabled;
        
        slider.rect.left = 20;
        slider.rect.top = yOffset + 45;
        slider.rect.right = m_windowWidth - 35;
        slider.rect.bottom = slider.rect.top + 75;

        m_sliders.push_back(slider);

        UICheckbox cb;
        cb.monitorId = mon.id;
        cb.checked = mon.enabled;
        cb.rect.left = slider.rect.left + 15;
        cb.rect.top = slider.rect.top + 15;
        cb.rect.right = cb.rect.left + 20;
        cb.rect.bottom = cb.rect.top + 20;
        m_checkboxes.push_back(cb);

        yOffset += 90;
    }

    // 4. Settings Checkboxes (Footer Area)
    yOffset += 20;
    
    // Close minimizes to tray
    UICheckbox ctt;
    ctt.settingName = L"CloseToTray";
    ctt.checked = m_config.closeToTray;
    ctt.rect.left = 25;
    ctt.rect.top = yOffset;
    ctt.rect.right = ctt.rect.left + 18;
    ctt.rect.bottom = ctt.rect.top + 18;
    m_checkboxes.push_back(ctt);

    // Show in taskbar
    UICheckbox sit;
    sit.settingName = L"ShowInTaskbar";
    sit.checked = m_config.showInTaskbar;
    sit.rect.left = 250;
    sit.rect.top = yOffset;
    sit.rect.right = sit.rect.left + 18;
    sit.rect.bottom = sit.rect.top + 18;
    m_checkboxes.push_back(sit);

    yOffset += 28;

    // Show boundary diagnostics
    UICheckbox sbd;
    sbd.settingName = L"ShowBoundaries";
    sbd.checked = m_config.showBoundaries;
    sbd.rect.left = 25;
    sbd.rect.top = yOffset;
    sbd.rect.right = sbd.rect.left + 18;
    sbd.rect.bottom = sbd.rect.top + 18;
    m_checkboxes.push_back(sbd);

    // Start with windows
    UICheckbox sww;
    sww.settingName = L"StartWithWindows";
    sww.checked = m_config.startWithWindows;
    sww.rect.left = 250;
    sww.rect.top = yOffset;
    sww.rect.right = sww.rect.left + 18;
    sww.rect.bottom = sww.rect.top + 18;
    m_checkboxes.push_back(sww);

    yOffset += 28;

    // Eye-Saver Warm Tint
    UICheckbox wt;
    wt.settingName = L"WarmTint";
    wt.checked = m_config.warmTint;
    wt.rect.left = 25;
    wt.rect.top = yOffset;
    wt.rect.right = wt.rect.left + 18;
    wt.rect.bottom = wt.rect.top + 18;
    m_checkboxes.push_back(wt);

    // Active Monitor Focus Mode
    UICheckbox fm;
    fm.settingName = L"FocusMode";
    fm.checked = m_config.focusMode;
    fm.rect.left = 250;
    fm.rect.top = yOffset;
    fm.rect.right = fm.rect.left + 18;
    fm.rect.bottom = fm.rect.top + 18;
    m_checkboxes.push_back(fm);

    yOffset += 28;

    // Enable Idle Dimming
    UICheckbox ide;
    ide.settingName = L"IdleDimEnabled";
    ide.checked = m_config.idleDimEnabled;
    ide.rect.left = 25;
    ide.rect.top = yOffset;
    ide.rect.right = ide.rect.left + 18;
    ide.rect.bottom = ide.rect.top + 18;
    m_checkboxes.push_back(ide);

    // Turn off screens on timeout
    UICheckbox ito;
    ito.settingName = L"IdleTurnOff";
    ito.checked = m_config.idleTurnOff;
    ito.rect.left = 250;
    ito.rect.top = yOffset;
    ito.rect.right = ito.rect.left + 18;
    ito.rect.bottom = ito.rect.top + 18;
    m_checkboxes.push_back(ito);

    if (m_config.idleDimEnabled) {
        // Inactivity Minutes Slider Card
        UISlider idleMin;
        idleMin.isIdleMinutes = true;
        idleMin.value = (m_config.idleMinutes - 1) / 59.0f; // 1 to 60
        idleMin.active = true;
        idleMin.rect.left = 20;
        idleMin.rect.top = yOffset + 30;
        idleMin.rect.right = m_windowWidth - 35;
        idleMin.rect.bottom = idleMin.rect.top + 65;
        m_sliders.push_back(idleMin);

        // Inactivity Dim Level Slider Card
        UISlider idleLvl;
        idleLvl.isIdleDimLevel = true;
        idleLvl.value = m_config.idleDimLevel / 100.0f; // 0 to 100
        idleLvl.active = true;
        idleLvl.rect.left = 20;
        idleLvl.rect.top = yOffset + 105;
        idleLvl.rect.right = m_windowWidth - 35;
        idleLvl.rect.bottom = idleLvl.rect.top + 65;
        m_sliders.push_back(idleLvl);

        yOffset += 150;
    }

    // Calculate required window height dynamically
    m_windowHeight = yOffset + 65;
    
    RECT rc = { 0, 0, m_windowWidth, m_windowHeight };
    AdjustWindowRectEx(&rc, GetWindowLongW(m_hwnd, GWL_STYLE), FALSE, GetWindowLongW(m_hwnd, GWL_EXSTYLE));
    SetWindowPos(m_hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void MainWindow::OnPaint() {
    if (FAILED(CreateGraphicsResources())) return;

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->SetTransform(D2D1::IdentityMatrix());
    
    // Clear screen
    m_pRenderTarget->Clear(D2D1::ColorF(0x131315));

    // Draw Title Header
    m_pRenderTarget->DrawText(
        L"WinDimmer64", 12,
        m_pTextFormatTitle,
        D2D1::RectF(20.0f, 15.0f, 300.0f, 45.0f),
        m_pBrushText
    );

    // Render interactive cards & sliders
    for (const auto& slider : m_sliders) {
        // Draw round cornered card
        D2D1_ROUNDED_RECT cardRect = D2D1::RoundedRect(
            D2D1::RectF(slider.rect.left, slider.rect.top, slider.rect.right, slider.rect.bottom),
            8.0f, 8.0f
        );
        m_pRenderTarget->FillRoundedRectangle(cardRect, m_pBrushCard);
        m_pRenderTarget->DrawRoundedRectangle(cardRect, m_pBrushCardBorder, 1.2f);

        // Find associated name and value string
        std::wstring displayName;
        wchar_t pctStr[32] = { 0 };
        if (slider.isMaster) {
            displayName = L"Master Controller (All Monitors)";
            int pct = static_cast<int>(slider.value * 90.0f);
            StringCchPrintfW(pctStr, ARRAYSIZE(pctStr), L"%d%%", pct);
        } else if (slider.isIdleMinutes) {
            displayName = L"Inactivity Timeout";
            int mins = static_cast<int>(slider.value * 59.0f) + 1;
            StringCchPrintfW(pctStr, ARRAYSIZE(pctStr), L"%d min", mins);
        } else if (slider.isIdleDimLevel) {
            displayName = L"Inactivity Dim Level";
            int lvl = static_cast<int>(slider.value * 100.0f);
            StringCchPrintfW(pctStr, ARRAYSIZE(pctStr), L"%d%%", lvl);
        } else {
            const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
            for (const auto& mon : activeMons) {
                if (mon.id == slider.monitorId) {
                    displayName = mon.friendlyName;
                    break;
                }
            }
            int pct = static_cast<int>(slider.value * 90.0f);
            StringCchPrintfW(pctStr, ARRAYSIZE(pctStr), L"%d%%", pct);
        }

        // Draw monitor label
        float textLeft = slider.rect.left + 20.0f; // Shift text slightly left for full card spacing
        m_pRenderTarget->DrawText(
            displayName.c_str(), static_cast<UINT32>(displayName.length()),
            m_pTextFormatBody,
            D2D1::RectF(textLeft, slider.rect.top + 14.0f, slider.rect.right - 90.0f, slider.rect.top + 34.0f),
            slider.active ? m_pBrushText : m_pBrushTextMuted
        );

        // Draw current dim % or minutes value
        m_pRenderTarget->DrawText(
            pctStr, static_cast<UINT32>(wcslen(pctStr)),
            m_pTextFormatBody,
            D2D1::RectF(slider.rect.right - 85.0f, slider.rect.top + 14.0f, slider.rect.right - 15.0f, slider.rect.top + 34.0f),
            slider.active ? m_pBrushText : m_pBrushTextMuted
        );

        // Draw track slider bar
        float trackY = slider.rect.bottom - 22.0f;
        float trackLeft = slider.rect.left + 20.0f;
        float trackRight = slider.rect.right - 20.0f;
        float trackWidth = trackRight - trackLeft;

        m_pRenderTarget->DrawLine(
            D2D1::Point2F(trackLeft, trackY),
            D2D1::Point2F(trackRight, trackY),
            m_pBrushTrack,
            4.0f
        );

        // Active segment
        float thumbX = trackLeft + (slider.value * trackWidth);
        if (slider.active) {
            m_pRenderTarget->DrawLine(
                D2D1::Point2F(trackLeft, trackY),
                D2D1::Point2F(thumbX, trackY),
                m_pBrushAccent,
                4.0f
            );
        }

        // Thumb circular handle
        m_pRenderTarget->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(thumbX, trackY), 7.0f, 7.0f),
            (slider.isHovered && slider.active) ? m_pBrushAccentHover : (slider.active ? m_pBrushAccent : m_pBrushTrack)
        );
    }

    // Render checkbox/toggle icons
    for (const auto& cb : m_checkboxes) {
        // Render stylized custom checkboxes
        D2D1_RECT_F boxRect = D2D1::RectF(cb.rect.left, cb.rect.top, cb.rect.right, cb.rect.bottom);
        m_pRenderTarget->DrawRectangle(boxRect, cb.isHovered ? m_pBrushAccentHover : m_pBrushCardBorder, 1.5f);

        if (cb.checked) {
            D2D1_RECT_F innerRect = D2D1::RectF(cb.rect.left + 3, cb.rect.top + 3, cb.rect.right - 3, cb.rect.bottom - 3);
            m_pRenderTarget->FillRectangle(innerRect, m_pBrushAccent);
        }

        // Text label matching checkbox target setting
        std::wstring textLabel;
        if (cb.settingName == L"CloseToTray") textLabel = L"Close button hides to tray";
        else if (cb.settingName == L"ShowInTaskbar") textLabel = L"Show in taskbar";
        else if (cb.settingName == L"ShowBoundaries") textLabel = L"Show boundary diagnostics";
        else if (cb.settingName == L"StartWithWindows") textLabel = L"Start with Windows";
        else if (cb.settingName == L"WarmTint") textLabel = L"Eye-Saver Warm Amber Tint";
        else if (cb.settingName == L"FocusMode") textLabel = L"Focused Screen Highlight";
        else if (cb.settingName == L"IdleDimEnabled") textLabel = L"Dim screen when idle";
        else if (cb.settingName == L"IdleTurnOff") textLabel = L"Turn off screen on idle";

        if (!textLabel.empty()) {
            m_pRenderTarget->DrawText(
                textLabel.c_str(), static_cast<UINT32>(textLabel.length()),
                m_pTextFormatDetail,
                D2D1::RectF(cb.rect.right + 8.0f, cb.rect.top + 1.0f, cb.rect.right + 220.0f, cb.rect.bottom + 10.0f),
                m_pBrushText
            );
        }
    }

    m_pRenderTarget->EndDraw();
}

void MainWindow::HandleMouseMove(int x, int y) {
    bool needsRepaint = false;
    
    // Sliders check
    for (auto& slider : m_sliders) {
        float trackLeft = slider.rect.left + 20.0f;
        float trackRight = slider.rect.right - 20.0f;
        float trackWidth = trackRight - trackLeft;
        float trackY = slider.rect.bottom - 22.0f;

        if (slider.isDragging && slider.active) {
            float relX = static_cast<float>(x) - trackLeft;
            slider.value = relX / trackWidth;
            if (slider.value < 0.0f) slider.value = 0.0f;
            if (slider.value > 1.0f) slider.value = 1.0f;

            if (slider.isMaster) {
                int actualDim = static_cast<int>(slider.value * 90.0f);
                m_config.masterValue = actualDim;
                const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
                for (const auto& mon : activeMons) {
                    DimmerManager::Instance().SetMonitorDim(mon.id, actualDim);
                }
                for (auto& monConf : m_config.monitors) {
                    monConf.value = actualDim;
                }
                for (auto& other : m_sliders) {
                    if (!other.isMaster && !other.isIdleMinutes && !other.isIdleDimLevel) {
                        other.value = slider.value;
                    }
                }
            } else if (slider.isIdleMinutes) {
                m_config.idleMinutes = static_cast<int>(slider.value * 59.0f) + 1;
            } else if (slider.isIdleDimLevel) {
                m_config.idleDimLevel = static_cast<int>(slider.value * 100.0f);
                // If currently idle, apply the level immediately to preview it!
                if (DimmerManager::Instance().IsIdleState()) {
                    DimmerManager::Instance().SetIdleState(true, m_config.idleDimLevel);
                }
            } else {
                int actualDim = static_cast<int>(slider.value * 90.0f);
                DimmerManager::Instance().SetMonitorDim(slider.monitorId, actualDim);
                for (auto& monConf : m_config.monitors) {
                    if (monConf.id == slider.monitorId) {
                        monConf.value = actualDim;
                        break;
                    }
                }
            }
            needsRepaint = true;
        }

        // Hover checking
        float thumbX = trackLeft + (slider.value * trackWidth);
        bool wasHovered = slider.isHovered;
        slider.isHovered = (abs(x - thumbX) < 12 && abs(y - trackY) < 12);
        if (slider.isHovered != wasHovered) {
            needsRepaint = true;
        }
    }

    // Checkboxes check
    for (auto& cb : m_checkboxes) {
        bool wasHovered = cb.isHovered;
        cb.isHovered = (x >= cb.rect.left && x <= cb.rect.right && y >= cb.rect.top && y <= cb.rect.bottom);
        if (cb.isHovered != wasHovered) {
            needsRepaint = true;
        }
    }

    if (needsRepaint) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::HandleLButtonDown(int x, int y) {
    for (auto& slider : m_sliders) {
        float trackLeft = slider.rect.left + 20.0f;
        float trackRight = slider.rect.right - 20.0f;
        float trackWidth = trackRight - trackLeft;
        float trackY = slider.rect.bottom - 22.0f;

        float thumbX = trackLeft + (slider.value * trackWidth);
        // If clicked anywhere near the track or the thumb
        if (slider.active && x >= trackLeft - 5 && x <= trackRight + 5 && abs(y - trackY) < 10) {
            slider.isDragging = true;
            m_isDraggingAny = true;
            SetCapture(m_hwnd);
            HandleMouseMove(x, y); // Immediately position thumb to mouse
            break;
        }
    }

    // Checkboxes check
    for (auto& cb : m_checkboxes) {
        if (x >= cb.rect.left && x <= cb.rect.right && y >= cb.rect.top && y <= cb.rect.bottom) {
            cb.checked = !cb.checked;
            
            // Apply setting immediately
            if (!cb.monitorId.empty()) {
                // Individual screen checkbox
                DimmerManager::Instance().SetMonitorEnabled(cb.monitorId, cb.checked);
                for (auto& monConf : m_config.monitors) {
                    if (monConf.id == cb.monitorId) {
                        monConf.enabled = cb.checked;
                        break;
                    }
                }
                // Refresh slider active status
                for (auto& sl : m_sliders) {
                    if (sl.monitorId == cb.monitorId) {
                        sl.active = cb.checked;
                        break;
                    }
                }
            } else if (cb.settingName == L"MasterEnabled") {
                m_config.masterEnabled = cb.checked;
                // Enable/disable all individual monitor sliders
                const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
                for (const auto& mon : activeMons) {
                    DimmerManager::Instance().SetMonitorEnabled(mon.id, cb.checked);
                }
                for (auto& monConf : m_config.monitors) {
                    monConf.enabled = cb.checked;
                }
                for (auto& sl : m_sliders) {
                    sl.active = cb.checked;
                }
            } else if (cb.settingName == L"CloseToTray") {
                m_config.closeToTray = cb.checked;
            } else if (cb.settingName == L"ShowInTaskbar") {
                m_config.showInTaskbar = cb.checked;
                // Standard styles need window recreation or update, simpler to just write to config and let user know,
                // or dynamic GWL_STYLE toggle:
                LONG_PTR style = GetWindowLongPtrW(m_hwnd, GWL_STYLE);
                if (cb.checked) {
                    SetWindowLongPtrW(m_hwnd, GWL_STYLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
                } else {
                    SetWindowLongPtrW(m_hwnd, GWL_STYLE, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
                }
                SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            } else if (cb.settingName == L"ShowBoundaries") {
                m_config.showBoundaries = cb.checked;
                DimmerManager::Instance().SetShowBoundaries(cb.checked);
            } else if (cb.settingName == L"StartWithWindows") {
                m_config.startWithWindows = cb.checked;
                ToggleStartWithWindows(cb.checked);
            } else if (cb.settingName == L"WarmTint") {
                m_config.warmTint = cb.checked;
                DimmerManager::Instance().SetWarmTint(cb.checked);
            } else if (cb.settingName == L"FocusMode") {
                m_config.focusMode = cb.checked;
                DimmerManager::Instance().SetFocusMode(cb.checked);
            } else if (cb.settingName == L"IdleDimEnabled") {
                m_config.idleDimEnabled = cb.checked;
                if (!cb.checked) {
                    DimmerManager::Instance().SetIdleState(false);
                }
                UpdateLayout();
            } else if (cb.settingName == L"IdleTurnOff") {
                m_config.idleTurnOff = cb.checked;
            }

            SaveSettings();
            InvalidateRect(m_hwnd, nullptr, FALSE);
            break;
        }
    }
}

void MainWindow::HandleLButtonUp(int x, int y) {
    if (m_isDraggingAny) {
        for (auto& slider : m_sliders) {
            if (slider.isDragging) {
                slider.isDragging = false;
                break;
            }
        }
        m_isDraggingAny = false;
        ReleaseCapture();
        SaveSettings();
    }
}

void MainWindow::HandleMouseWheel(short delta, int x, int y) {
    // Zoom/increment the slider currently hovered
    POINT pt = { x, y };
    ScreenToClient(m_hwnd, &pt);

    bool changed = false;
    for (auto& slider : m_sliders) {
        if (slider.active && pt.x >= slider.rect.left && pt.x <= slider.rect.right && pt.y >= slider.rect.top && pt.y <= slider.rect.bottom) {
            float step = (delta > 0) ? 0.02f : -0.02f;
            slider.value += step;
            if (slider.value < 0.0f) slider.value = 0.0f;
            if (slider.value > 1.0f) slider.value = 1.0f;

            if (slider.isMaster) {
                int actualDim = static_cast<int>(slider.value * 90.0f);
                m_config.masterValue = actualDim;
                for (auto& mon : DimmerManager::Instance().GetActiveMonitors()) {
                    DimmerManager::Instance().SetMonitorDim(mon.id, actualDim);
                }
                for (auto& monConf : m_config.monitors) {
                    monConf.value = actualDim;
                }
                for (auto& other : m_sliders) {
                    if (!other.isMaster && !other.isIdleMinutes && !other.isIdleDimLevel) other.value = slider.value;
                }
            } else if (slider.isIdleMinutes) {
                m_config.idleMinutes = static_cast<int>(slider.value * 59.0f) + 1;
            } else if (slider.isIdleDimLevel) {
                m_config.idleDimLevel = static_cast<int>(slider.value * 100.0f);
                if (DimmerManager::Instance().IsIdleState()) {
                    DimmerManager::Instance().SetIdleState(true, m_config.idleDimLevel);
                }
            } else {
                int actualDim = static_cast<int>(slider.value * 90.0f);
                DimmerManager::Instance().SetMonitorDim(slider.monitorId, actualDim);
                for (auto& monConf : m_config.monitors) {
                    if (monConf.id == slider.monitorId) {
                        monConf.value = actualDim;
                        break;
                    }
                }
            }
            changed = true;
            break;
        }
    }

    if (changed) {
        SaveSettings();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::HandleKeyDown(WPARAM key) {
    // Keyboard accessibility: Left/Right arrow keys adjust active slider
    bool changed = false;
    for (auto& slider : m_sliders) {
        // If slider is hovered (focused by mouse position) or active
        if (slider.active && slider.isHovered) {
            float step = 0.0f;
            if (key == VK_RIGHT || key == VK_UP) step = 0.02f;
            else if (key == VK_LEFT || key == VK_DOWN) step = -0.02f;

            if (step != 0.0f) {
                slider.value += step;
                if (slider.value < 0.0f) slider.value = 0.0f;
                if (slider.value > 1.0f) slider.value = 1.0f;

                if (slider.isMaster) {
                    int actualDim = static_cast<int>(slider.value * 90.0f);
                    m_config.masterValue = actualDim;
                    for (auto& mon : DimmerManager::Instance().GetActiveMonitors()) {
                        DimmerManager::Instance().SetMonitorDim(mon.id, actualDim);
                    }
                    for (auto& monConf : m_config.monitors) {
                        monConf.value = actualDim;
                    }
                    for (auto& other : m_sliders) {
                        if (!other.isMaster && !other.isIdleMinutes && !other.isIdleDimLevel) other.value = slider.value;
                    }
                } else if (slider.isIdleMinutes) {
                    m_config.idleMinutes = static_cast<int>(slider.value * 59.0f) + 1;
                } else if (slider.isIdleDimLevel) {
                    m_config.idleDimLevel = static_cast<int>(slider.value * 100.0f);
                    if (DimmerManager::Instance().IsIdleState()) {
                        DimmerManager::Instance().SetIdleState(true, m_config.idleDimLevel);
                    }
                } else {
                    int actualDim = static_cast<int>(slider.value * 90.0f);
                    DimmerManager::Instance().SetMonitorDim(slider.monitorId, actualDim);
                    for (auto& monConf : m_config.monitors) {
                        if (monConf.id == slider.monitorId) {
                            monConf.value = actualDim;
                            break;
                        }
                    }
                }
                changed = true;
                break;
            }
        }
    }

    if (changed) {
        SaveSettings();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::LoadSettings() {
    m_config = ConfigManager::LoadConfig(ConfigManager::GetConfigPath());
}

void MainWindow::SaveSettings() {
    ConfigManager::SaveConfig(ConfigManager::GetConfigPath(), m_config);
}

void MainWindow::ToggleStartWithWindows(bool enable) {
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey);
    if (lRes == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            RegSetValueExW(hKey, L"WinDimmer64", 0, REG_SZ, reinterpret_cast<const BYTE*>(exePath), static_cast<DWORD>((wcslen(exePath) + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, L"WinDimmer64");
        }
        RegCloseKey(hKey);
    }
}

void MainWindow::OnResize(UINT width, UINT height) {
    if (m_pRenderTarget) {
        D2D1_SIZE_U size = D2D1::SizeU(width, height);
        m_pRenderTarget->Resize(size);
    }
}

MainWindow::~MainWindow() {
    KillTimer(m_hwnd, 201);
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
                    int delta = (wp == 101) ? -5 : 5;
                    self->m_config.masterValue += delta;
                    if (self->m_config.masterValue < 0) self->m_config.masterValue = 0;
                    if (self->m_config.masterValue > 90) self->m_config.masterValue = 90;

                    for (const auto& mon : DimmerManager::Instance().GetActiveMonitors()) {
                        DimmerManager::Instance().SetMonitorDim(mon.id, self->m_config.masterValue);
                    }
                    for (auto& monConf : self->m_config.monitors) {
                        monConf.value = self->m_config.masterValue;
                    }
                    for (auto& sl : self->m_sliders) {
                        sl.value = self->m_config.masterValue / 90.0f;
                    }
                    self->SaveSettings();
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else if (wp == 103) {
                    self->m_config.masterEnabled = !self->m_config.masterEnabled;
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
                        if (cb.settingName == L"MasterEnabled") {
                            cb.checked = self->m_config.masterEnabled;
                            break;
                        }
                    }
                    self->SaveSettings();
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            case WM_TIMER: {
                if (wp == 201) {
                    if (self->m_config.focusMode) {
                        POINT pt;
                        if (GetCursorPos(&pt)) {
                            HMONITOR hActive = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
                            MONITORINFOEXW mi;
                            mi.cbSize = sizeof(mi);
                            if (GetMonitorInfoW(hActive, &mi)) {
                                std::wstring activeId = mi.szDevice;
                                if (activeId != self->m_lastActiveMonitorId) {
                                    self->m_lastActiveMonitorId = activeId;
                                    for (const auto& mon : DimmerManager::Instance().GetActiveMonitors()) {
                                        DimmerManager::Instance().TriggerFade(mon.hwndOverlay);
                                    }
                                }
                            }
                        }
                    }
                } else if (wp == 202) {
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
                                        // SC_MONITORPOWER with parameter 2 turns screens off physically
                                        PostMessageW(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
                                    }
                                }
                            } else {
                                if (DimmerManager::Instance().IsIdleState()) {
                                    DimmerManager::Instance().SetIdleState(false);
                                }
                            }
                        }
                    }
                }
                return 0;
            }
            case WM_DISPLAYCHANGE: {
                DimmerManager::Instance().RefreshMonitors();
                self->UpdateLayout();
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
            case WM_TRAYICON: {
                if (lp == WM_RBUTTONUP) {
                    POINT pt;
                    GetCursorPos(&pt);
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"Open Settings");
                    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit WinDimmer64");
                    
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
