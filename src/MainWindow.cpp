#include "MainWindow.h"
#include "DimmerManager.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <strsafe.h>

#ifndef D2DERR_RECREATED
#define D2DERR_RECREATED ((HRESULT)0x8898000CL)
#endif

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
    m_undoStack.clear();
    m_canUndo = false;
    m_changeCount = 0;

    // Register MainWindow Class
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"WinDimmer64MainClass";
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(101));
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

    // Enable Windows 11 rounded corners and dark/light theme
    BOOL useDark = !m_config.lightMode;
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
    SyncMonitorsWithConfig();

    // Apply warm tint, focus mode, and active dimming settings
    DimmerManager::Instance().SetWarmTint(m_config.warmTint);
    DimmerManager::Instance().SetFocusMode(m_config.focusMode);
    DimmerManager::Instance().SetDimmingEnabled(m_config.dimmingEnabled);

    // Register global hotkeys
    RegisterHotKey(m_hwnd, 101, MOD_CONTROL | MOD_ALT, VK_UP);
    RegisterHotKey(m_hwnd, 102, MOD_CONTROL | MOD_ALT, VK_DOWN);
    RegisterHotKey(m_hwnd, 103, MOD_CONTROL | MOD_ALT, 0x44); // 'D' key

    // Start Focus Mode checking timer (150ms interval)
    SetTimer(m_hwnd, 201, 150, nullptr);

    // Start Inactivity/Idle checking timer (1000ms interval)
    SetTimer(m_hwnd, 202, 1000, nullptr);

    // Add to system tray
    HICON hAppIcon = LoadIconW(m_hInst, MAKEINTRESOURCEW(101));
    AddTrayIcon(m_hwnd, 1, hAppIcon, L"WinDimmer64 Screen Brightness");

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
                L"Segoe UI Variable Display", nullptr,
                DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                20.0f, L"en-us", &m_pTextFormatTitle
            );
            m_pDWriteFactory->CreateTextFormat(
                L"Segoe UI Variable Text", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                13.0f, L"en-us", &m_pTextFormatBody
            );
            m_pDWriteFactory->CreateTextFormat(
                L"Segoe UI Variable Text", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                10.5f, L"en-us", &m_pTextFormatDetail
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

    // Start yOffset below the header
    int yOffset = 52;

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
        mcb.label = L""; // Master uses standard title text
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
        cb.pValue = nullptr; // Mon enables are handled via individual loop logic
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
    AddCheckbox(L"FocusMode", m_config.focusMode, &m_config.focusMode, L"Focus Highlight", 1, yOffset);
    yOffset += 22;
    AddCheckbox(L"LightMode", m_config.lightMode, &m_config.lightMode, L"Light Mode", 0, yOffset);
    AddCheckbox(L"ShowBoundaries", m_config.showBoundaries, &m_config.showBoundaries, L"Boundary Diagnostics", 1, yOffset);
    yOffset += 22;

    // ── APPLICATION section ──
    yOffset += 8; // gap between sections
    yOffset += 16; // space for section header label
    AddCheckbox(L"CloseToTray", m_config.closeToTray, &m_config.closeToTray, L"Close to Tray", 0, yOffset);
    AddCheckbox(L"StartWithWindows", m_config.startWithWindows, &m_config.startWithWindows, L"Start with Windows", 1, yOffset);
    yOffset += 22;

    // Undo entry (action, not a toggle)
    {
        UICheckbox cb;
        cb.settingName = L"Undo";
        cb.label = L"Undo Changes";
        cb.rect.left = 25;
        cb.rect.top = yOffset;
        cb.rect.right = 25 + 34;
        cb.rect.bottom = yOffset + 18;
        m_checkboxes.push_back(cb);
    }
    yOffset += 22;

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
    
    RECT rc = { 0, 0, m_windowWidth, m_windowHeight };
    AdjustWindowRectEx(&rc, GetWindowLongW(m_hwnd, GWL_STYLE), FALSE, GetWindowLongW(m_hwnd, GWL_EXSTYLE));
    SetWindowPos(m_hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void MainWindow::OnPaint() {
    if (FAILED(CreateGraphicsResources())) return;

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->SetTransform(D2D1::IdentityMatrix());
    
    // Dynamic Premium Slate / Grey monochrome Theme Tokens
    if (m_config.lightMode) {
        m_pBrushBg->SetColor(D2D1::ColorF(0xF0F0F2));
        m_pBrushCard->SetColor(D2D1::ColorF(0xFFFFFF));
        m_pBrushCardBorder->SetColor(D2D1::ColorF(0xE0E0E0));
        m_pBrushText->SetColor(D2D1::ColorF(0x1F1F1F));
        m_pBrushTextMuted->SetColor(D2D1::ColorF(0x757575));
        m_pBrushTrack->SetColor(D2D1::ColorF(0xE0E0E2));
        m_pBrushAccent->SetColor(D2D1::ColorF(0x7A7A7E));
        m_pBrushAccentHover->SetColor(D2D1::ColorF(0x555558));
    } else {
        // Sleek Industrial Dark Slate Theme
        m_pBrushBg->SetColor(D2D1::ColorF(0x121212));
        m_pBrushCard->SetColor(D2D1::ColorF(0x1E1E1E));
        m_pBrushCardBorder->SetColor(D2D1::ColorF(0x2D2D2D));
        m_pBrushText->SetColor(D2D1::ColorF(0xE1E1E1));
        m_pBrushTextMuted->SetColor(D2D1::ColorF(0x808080));
        m_pBrushTrack->SetColor(D2D1::ColorF(0x2D2D2D));
        m_pBrushAccent->SetColor(D2D1::ColorF(0x8E8E93));
        m_pBrushAccentHover->SetColor(D2D1::ColorF(0xC7C7CC));
    }

    // Clear screen
    m_pRenderTarget->Clear(m_pBrushBg->GetColor());

    // Render interactive cards & sliders
    for (const auto& slider : m_sliders) {
        // Draw round cornered card
        D2D1_ROUNDED_RECT cardRect = D2D1::RoundedRect(
            D2D1::RectF(slider.rect.left, slider.rect.top, slider.rect.right, slider.rect.bottom),
            8.0f, 8.0f
        );
        m_pRenderTarget->FillRoundedRectangle(cardRect, m_pBrushCard);
        
        // Active border glow! Glow with accent color if active, otherwise standard card border
        m_pRenderTarget->DrawRoundedRectangle(
            cardRect, 
            slider.active ? m_pBrushAccent : m_pBrushCardBorder, 
            slider.active ? 1.6f : 1.2f
        );

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

        // Draw monitor label (shift text left if checkbox is inside the card)
        float textLeft = (slider.isIdleMinutes || slider.isIdleDimLevel) ? (slider.rect.left + 20.0f) : (slider.rect.left + 60.0f);
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

        // Draw track slider bar (thicker rounded techie progress bar)
        float trackY = slider.rect.bottom - 22.0f;
        float trackLeft = slider.rect.left + 20.0f;
        float trackRight = slider.rect.right - 20.0f;
        float trackWidth = trackRight - trackLeft;

        m_pRenderTarget->DrawLine(
            D2D1::Point2F(trackLeft, trackY),
            D2D1::Point2F(trackRight, trackY),
            m_pBrushTrack,
            6.0f
        );

        // Active progress track segment
        float thumbX = trackLeft + (slider.value * trackWidth);
        if (slider.active) {
            m_pRenderTarget->DrawLine(
                D2D1::Point2F(trackLeft, trackY),
                D2D1::Point2F(thumbX, trackY),
                m_pBrushAccent,
                6.0f
            );
        }

        // Technical Dual-Ring Slider Knob (mixing console style)
        if (slider.active) {
            m_pRenderTarget->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(thumbX, trackY), 8.0f, 8.0f),
                slider.isHovered ? m_pBrushAccentHover : m_pBrushAccent
            );
            m_pRenderTarget->DrawEllipse(
                D2D1::Ellipse(D2D1::Point2F(thumbX, trackY), 10.0f, 10.0f),
                m_pBrushText,
                1.5f
            );
        } else {
            m_pRenderTarget->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(thumbX, trackY), 7.0f, 7.0f),
                m_pBrushTrack
            );
        }
    }

    // Draw grouped section header labels above their first toggle
    for (const auto& cb : m_checkboxes) {
        if (cb.settingName == L"DimmingEnabled" && !cb.label.empty()) {
            m_pRenderTarget->DrawText(
                L"DIMMING", 7, m_pTextFormatDetail,
                D2D1::RectF(25.0f, cb.rect.top - 18.0f, 200.0f, cb.rect.top - 2.0f),
                m_pBrushTextMuted
            );
        } else if (cb.settingName == L"WarmTint" && !cb.label.empty()) {
            m_pRenderTarget->DrawText(
                L"DISPLAY", 7, m_pTextFormatDetail,
                D2D1::RectF(25.0f, cb.rect.top - 18.0f, 200.0f, cb.rect.top - 2.0f),
                m_pBrushTextMuted
            );
        } else if (cb.settingName == L"CloseToTray" && !cb.label.empty()) {
            m_pRenderTarget->DrawText(
                L"APPLICATION", 11, m_pTextFormatDetail,
                D2D1::RectF(25.0f, cb.rect.top - 18.0f, 200.0f, cb.rect.top - 2.0f),
                m_pBrushTextMuted
            );
        }
    }

    // Render high-tech sliding toggle switches
    for (const auto& cb : m_checkboxes) {
        if (cb.settingName == L"Undo") {
            // Undo is an action button, not a toggle
            wchar_t undoLabel[32] = { 0 };
            if (m_changeCount > 0) {
                StringCchPrintfW(undoLabel, ARRAYSIZE(undoLabel), L"Undo (%d)", m_changeCount);
            } else {
                StringCchCopyW(undoLabel, ARRAYSIZE(undoLabel), L"Undo Changes");
            }
            m_pRenderTarget->DrawText(
                undoLabel, static_cast<UINT32>(wcslen(undoLabel)),
                m_pTextFormatDetail,
                D2D1::RectF(cb.rect.right + 10.0f, cb.rect.top + 1.0f, m_windowWidth - 20.0f, cb.rect.bottom + 15.0f),
                m_canUndo ? m_pBrushAccent : m_pBrushTextMuted
            );
            continue;
        }

        D2D1_ROUNDED_RECT switchTrack = D2D1::RoundedRect(
            D2D1::RectF(cb.rect.left, cb.rect.top, cb.rect.right, cb.rect.bottom),
            9.0f, 9.0f
        );

        if (cb.checked) {
            m_pRenderTarget->FillRoundedRectangle(switchTrack, m_pBrushAccent);
            m_pRenderTarget->DrawRoundedRectangle(switchTrack, cb.isHovered ? m_pBrushAccentHover : m_pBrushAccent, 1.2f);
        } else {
            m_pRenderTarget->FillRoundedRectangle(switchTrack, m_pBrushTrack);
            m_pRenderTarget->DrawRoundedRectangle(switchTrack, cb.isHovered ? m_pBrushAccentHover : m_pBrushCardBorder, 1.2f);
        }

        // Draw circular sliding toggle knob
        float knobX = cb.checked ? (cb.rect.left + 25.0f) : (cb.rect.left + 9.0f);
        float knobY = cb.rect.top + 9.0f;
        m_pRenderTarget->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(knobX, knobY), 6.0f, 6.0f),
            cb.checked ? m_pBrushText : m_pBrushTextMuted
        );

        if (!cb.label.empty()) {
            float labelRight = (cb.rect.left < m_windowWidth / 2) ? (m_windowWidth / 2.0f - 10.0f) : (m_windowWidth - 20.0f);
            m_pRenderTarget->DrawText(
                cb.label.c_str(), static_cast<UINT32>(cb.label.length()),
                m_pTextFormatDetail,
                D2D1::RectF(cb.rect.right + 10.0f, cb.rect.top + 1.0f, labelRight, cb.rect.bottom + 15.0f),
                m_pBrushText
            );
        }
    }

    // Technical Separator Line before Footer Metadata
    float footerY = m_windowHeight - 35.0f;
    m_pRenderTarget->DrawLine(
        D2D1::Point2F(20.0f, footerY),
        D2D1::Point2F(m_windowWidth - 35.0f, footerY),
        m_pBrushCardBorder,
        1.0f
    );

    // Dynamic Dimmer Status Indicator
    bool anyDimmerActive = false;
    const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
    for (const auto& mon : activeMons) {
        if (mon.enabled && mon.dimValue > 0) {
            anyDimmerActive = true;
            break;
        }
    }

    wchar_t statusStr[64] = { 0 };
    if (anyDimmerActive) {
        StringCchCopyW(statusStr, ARRAYSIZE(statusStr), L"SYSTEM: ACTIVE");
    } else {
        StringCchCopyW(statusStr, ARRAYSIZE(statusStr), L"SYSTEM: STANDBY");
    }

    m_pRenderTarget->DrawText(
        statusStr, static_cast<UINT32>(wcslen(statusStr)),
        m_pTextFormatDetail,
        D2D1::RectF(25.0f, footerY + 10.0f, 250.0f, footerY + 28.0f),
        anyDimmerActive ? m_pBrushAccent : m_pBrushTextMuted
    );

    // Version Number in footer right
    const wchar_t* versionStr = L"v1.0.7";
    m_pRenderTarget->DrawText(
        versionStr, 6,
        m_pTextFormatDetail,
        D2D1::RectF(m_windowWidth - 85.0f, footerY + 10.0f, m_windowWidth - 35.0f, footerY + 28.0f),
        m_pBrushTextMuted
    );

    HRESULT hr = m_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATED) {
        DiscardGraphicsResources();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
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

            // Auto-enable dimming when user actively drags a monitor slider
            if (!slider.isIdleMinutes && !slider.isIdleDimLevel && !m_config.dimmingEnabled) {
                m_config.dimmingEnabled = true;
                DimmerManager::Instance().SetDimmingEnabled(true);
                for (auto& cb : m_checkboxes) {
                    if (cb.settingName == L"DimmingEnabled") { cb.checked = true; break; }
                }
            }

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
                if (m_config.groupDim) {
                    m_config.masterValue = actualDim;
                    const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
                    for (const auto& mon : activeMons) {
                        DimmerManager::Instance().SetMonitorDim(mon.id, actualDim);
                    }
                    for (auto& monConf : m_config.monitors) {
                        monConf.value = actualDim;
                    }
                    for (auto& other : m_sliders) {
                        if (!other.isIdleMinutes && !other.isIdleDimLevel) {
                            other.value = slider.value;
                        }
                    }
                } else {
                    DimmerManager::Instance().SetMonitorDim(slider.monitorId, actualDim);
                    for (auto& monConf : m_config.monitors) {
                        if (monConf.id == slider.monitorId) {
                            monConf.value = actualDim;
                            break;
                        }
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

void MainWindow::PushUndoState() {
    m_undoStack.push_back(m_config);
    m_canUndo = true;
    m_changeCount = static_cast<int>(m_undoStack.size());
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
            PushUndoState();
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
            PushUndoState();
            cb.checked = !cb.checked;
            
            // Dynamic configuration binding assignment
            if (cb.pValue) {
                *cb.pValue = cb.checked;
            }

            // Apply setting immediately
            if (!cb.monitorId.empty()) {
                // Individual screen checkbox
                if (m_config.groupDim) {
                    m_config.masterEnabled = cb.checked;
                    const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
                    for (const auto& mon : activeMons) {
                        DimmerManager::Instance().SetMonitorEnabled(mon.id, cb.checked);
                    }
                    for (auto& monConf : m_config.monitors) {
                        monConf.enabled = cb.checked;
                    }
                    for (auto& sl : m_sliders) {
                        if (!sl.isIdleMinutes && !sl.isIdleDimLevel) {
                            sl.active = cb.checked;
                        }
                    }
                    for (auto& otherCb : m_checkboxes) {
                        if (!otherCb.monitorId.empty() || otherCb.settingName == L"MasterEnabled") {
                            otherCb.checked = cb.checked;
                        }
                    }
                } else {
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
                }
            } else if (cb.settingName == L"MasterEnabled") {
                // Enable/disable all individual monitor sliders
                const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
                for (const auto& mon : activeMons) {
                    DimmerManager::Instance().SetMonitorEnabled(mon.id, cb.checked);
                }
                for (auto& monConf : m_config.monitors) {
                    monConf.enabled = cb.checked;
                }
                for (auto& sl : m_sliders) {
                    if (!sl.isIdleMinutes && !sl.isIdleDimLevel) {
                        sl.active = cb.checked;
                    }
                }
                for (auto& otherCb : m_checkboxes) {
                    if (!otherCb.monitorId.empty()) {
                        otherCb.checked = cb.checked;
                    }
                }
            } else if (cb.settingName == L"CloseToTray") {
                // Handled dynamically
            } else if (cb.settingName == L"Undo") {
                if (m_canUndo) {
                    m_config = m_undoStack.back();
                    m_undoStack.pop_back();
                    m_canUndo = !m_undoStack.empty();
                    m_changeCount = static_cast<int>(m_undoStack.size());

                    SyncMonitorsWithConfig();

                    const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
                    for (const auto& mon : activeMons) {
                        for (const auto& savedMon : m_config.monitors) {
                            if (savedMon.id == mon.id) {
                                DimmerManager::Instance().SetMonitorDim(mon.id, savedMon.value);
                                DimmerManager::Instance().SetMonitorEnabled(mon.id, savedMon.enabled);
                                break;
                            }
                        }
                    }
                    DimmerManager::Instance().SetWarmTint(m_config.warmTint);
                    DimmerManager::Instance().SetFocusMode(m_config.focusMode);
                    DimmerManager::Instance().SetShowBoundaries(m_config.showBoundaries);
                    if (!m_config.idleDimEnabled) {
                        DimmerManager::Instance().SetIdleState(false);
                    }
                    DimmerManager::Instance().SetDimmingEnabled(m_config.dimmingEnabled);

                    BOOL useDark = !m_config.lightMode;
                    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
                    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

                    UpdateLayout();
                    SaveSettings();
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                }
            } else if (cb.settingName == L"ShowBoundaries") {
                DimmerManager::Instance().SetShowBoundaries(cb.checked);
            } else if (cb.settingName == L"StartWithWindows") {
                ToggleStartWithWindows(cb.checked);
            } else if (cb.settingName == L"WarmTint") {
                DimmerManager::Instance().SetWarmTint(cb.checked);
            } else if (cb.settingName == L"FocusMode") {
                DimmerManager::Instance().SetFocusMode(cb.checked);
            } else if (cb.settingName == L"IdleDimEnabled") {
                if (!cb.checked) {
                    DimmerManager::Instance().SetIdleState(false);
                }
                UpdateLayout();
            } else if (cb.settingName == L"IdleTurnOff") {
                // Handled dynamically
            } else if (cb.settingName == L"DimmingEnabled") {
                DimmerManager::Instance().SetDimmingEnabled(cb.checked);
            } else if (cb.settingName == L"GroupDim") {
                if (cb.checked) {
                    const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
                    for (const auto& mon : activeMons) {
                        DimmerManager::Instance().SetMonitorDim(mon.id, m_config.masterValue);
                        DimmerManager::Instance().SetMonitorEnabled(mon.id, m_config.masterEnabled);
                    }
                    for (auto& monConf : m_config.monitors) {
                        monConf.value = m_config.masterValue;
                        monConf.enabled = m_config.masterEnabled;
                    }
                    for (auto& sl : m_sliders) {
                        if (!sl.isMaster && !sl.isIdleMinutes && !sl.isIdleDimLevel) {
                            sl.value = m_config.masterValue / 90.0f;
                            sl.active = m_config.masterEnabled;
                        }
                    }
                    for (auto& otherCb : m_checkboxes) {
                        if (!otherCb.monitorId.empty()) {
                            otherCb.checked = m_config.masterEnabled;
                        }
                    }
                }
            } else if (cb.settingName == L"LightMode") {
                BOOL useDark = !cb.checked;
                DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
                SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
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
            PushUndoState();
            float step = (delta > 0) ? 0.02f : -0.02f;
            slider.value += step;
            if (slider.value < 0.0f) slider.value = 0.0f;
            if (slider.value > 1.0f) slider.value = 1.0f;

            // Auto-enable dimming when user scrolls a monitor slider
            if (!slider.isIdleMinutes && !slider.isIdleDimLevel && !m_config.dimmingEnabled) {
                m_config.dimmingEnabled = true;
                DimmerManager::Instance().SetDimmingEnabled(true);
                for (auto& cb : m_checkboxes) {
                    if (cb.settingName == L"DimmingEnabled") { cb.checked = true; break; }
                }
            }

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
                if (m_config.groupDim) {
                    m_config.masterValue = actualDim;
                    for (auto& mon : DimmerManager::Instance().GetActiveMonitors()) {
                        DimmerManager::Instance().SetMonitorDim(mon.id, actualDim);
                    }
                    for (auto& monConf : m_config.monitors) {
                        monConf.value = actualDim;
                    }
                    for (auto& other : m_sliders) {
                        if (!other.isIdleMinutes && !other.isIdleDimLevel) other.value = slider.value;
                    }
                } else {
                    DimmerManager::Instance().SetMonitorDim(slider.monitorId, actualDim);
                    for (auto& monConf : m_config.monitors) {
                        if (monConf.id == slider.monitorId) {
                            monConf.value = actualDim;
                            break;
                        }
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
                PushUndoState();
                slider.value += step;
                if (slider.value < 0.0f) slider.value = 0.0f;
                if (slider.value > 1.0f) slider.value = 1.0f;

                // Auto-enable dimming when user arrow-keys a monitor slider
                if (!slider.isIdleMinutes && !slider.isIdleDimLevel && !m_config.dimmingEnabled) {
                    m_config.dimmingEnabled = true;
                    DimmerManager::Instance().SetDimmingEnabled(true);
                    for (auto& cb : m_checkboxes) {
                        if (cb.settingName == L"DimmingEnabled") { cb.checked = true; break; }
                    }
                }

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
                    if (m_config.groupDim) {
                        m_config.masterValue = actualDim;
                        for (auto& mon : DimmerManager::Instance().GetActiveMonitors()) {
                            DimmerManager::Instance().SetMonitorDim(mon.id, actualDim);
                        }
                        for (auto& monConf : m_config.monitors) {
                            monConf.value = actualDim;
                        }
                        for (auto& other : m_sliders) {
                            if (!other.isIdleMinutes && !other.isIdleDimLevel) other.value = slider.value;
                        }
                    } else {
                        DimmerManager::Instance().SetMonitorDim(slider.monitorId, actualDim);
                        for (auto& monConf : m_config.monitors) {
                            if (monConf.id == slider.monitorId) {
                                monConf.value = actualDim;
                                break;
                            }
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
                    self->PushUndoState();
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
                    DimmerManager::Instance().UpdateCursorDimming();
                    self->SaveSettings();
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else if (wp == 103) {
                    self->PushUndoState();
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
                    DimmerManager::Instance().UpdateCursorDimming();
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
                self->SyncMonitorsWithConfig();
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
