#include "DimmerManager.h"
#include <vector>
#include <algorithm>

// Helper to get friendly monitor name
static std::wstring GetMonitorFriendlyName(HMONITOR hMonitor, int index) {
    MONITORINFOEXW mi;
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMonitor, &mi)) {
        return L"Monitor " + std::to_wstring(index + 1) + L" (" + std::wstring(mi.szDevice) + L")";
    }
    return L"Monitor " + std::to_wstring(index + 1);
}

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    auto* list = reinterpret_cast<std::vector<ActiveMonitorInfo>*>(dwData);
    MONITORINFOEXW mi;
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMonitor, &mi)) {
        ActiveMonitorInfo info;
        info.id = mi.szDevice;
        info.friendlyName = GetMonitorFriendlyName(hMonitor, static_cast<int>(list->size()));
        info.rect = mi.rcMonitor;
        list->push_back(info);
    }
    return TRUE;
}

void DimmerManager::Initialize(HINSTANCE hInst) {
    m_hInst = hInst;
    if (!m_classRegistered) {
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = OverlayWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr; // Let WM_PAINT handle background to support custom warm tints
        wc.lpszClassName = L"WinDimmer64OverlayClass";
        RegisterClassExW(&wc);
        m_classRegistered = true;
    }
}

void DimmerManager::RefreshMonitors() {
    std::map<std::wstring, std::pair<int, bool>> savedValues;
    for (const auto& mon : m_monitors) {
        savedValues[mon.id] = {mon.dimValue, mon.enabled};
    }

    DestroyOverlays();
    m_monitors.clear();

    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&m_monitors));

    for (auto& mon : m_monitors) {
        auto it = savedValues.find(mon.id);
        if (it != savedValues.end()) {
            mon.dimValue = it->second.first;
            mon.enabled = it->second.second;
        }
        CreateOverlayForMonitor(mon);
    }
}

void DimmerManager::CreateOverlayForMonitor(ActiveMonitorInfo& info) {
    int w = info.rect.right - info.rect.left;
    int h = info.rect.bottom - info.rect.top;

    info.hwndOverlay = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"WinDimmer64OverlayClass",
        L"WinDimmer64Overlay",
        WS_POPUP,
        info.rect.left, info.rect.top, w, h,
        nullptr, nullptr, m_hInst, &info
    );

    if (info.hwndOverlay) {
        // Start from 0 dim for a beautiful fade-in on startup!
        info.currentDimValue = 0;
        SetLayeredWindowAttributes(info.hwndOverlay, 0, 0, LWA_ALPHA);

        ShowWindow(info.hwndOverlay, SW_SHOWNOACTIVATE);
        UpdateWindow(info.hwndOverlay);

        TriggerFade(info.hwndOverlay);
    }
}

void DimmerManager::SetMonitorDim(const std::wstring& id, int value) {
    for (auto& mon : m_monitors) {
        if (mon.id == id) {
            mon.dimValue = value;
            if (mon.hwndOverlay) {
                TriggerFade(mon.hwndOverlay);
            }
            break;
        }
    }
}

void DimmerManager::SetMonitorEnabled(const std::wstring& id, bool enabled) {
    for (auto& mon : m_monitors) {
        if (mon.id == id) {
            mon.enabled = enabled;
            if (mon.hwndOverlay) {
                TriggerFade(mon.hwndOverlay);
            }
            break;
        }
    }
}

void DimmerManager::SetShowBoundaries(bool show) {
    m_showBoundaries = show;
    for (auto& mon : m_monitors) {
        if (mon.hwndOverlay) {
            InvalidateRect(mon.hwndOverlay, nullptr, TRUE);
        }
    }
}

void DimmerManager::SetWarmTint(bool warm) {
    m_warmTint = warm;
    for (auto& mon : m_monitors) {
        if (mon.hwndOverlay) {
            InvalidateRect(mon.hwndOverlay, nullptr, TRUE);
        }
    }
}

void DimmerManager::SetFocusMode(bool focus) {
    m_focusMode = focus;
    for (auto& mon : m_monitors) {
        if (mon.hwndOverlay) {
            TriggerFade(mon.hwndOverlay);
        }
    }
}

void DimmerManager::SetIdleState(bool idle, int idleLevel) {
    if (m_isIdleState != idle || m_idleDimLevel != idleLevel) {
        m_isIdleState = idle;
        m_idleDimLevel = idleLevel;
        for (auto& mon : m_monitors) {
            if (mon.hwndOverlay) {
                TriggerFade(mon.hwndOverlay);
            }
        }
        UpdateCursorDimming();
    }
}

void DimmerManager::SetDimmingEnabled(bool enabled) {
    m_dimmingEnabled = enabled;
    for (auto& mon : m_monitors) {
        if (mon.hwndOverlay) {
            TriggerFade(mon.hwndOverlay);
        }
    }
    UpdateCursorDimming();
}

HCURSOR DimmerManager::CreateDimmedCursor() {
    HCURSOR hOriginal = LoadCursor(nullptr, IDC_ARROW);
    ICONINFO ii = { 0 };
    if (!GetIconInfo(hOriginal, &ii))
        return CopyIcon(hOriginal);

    if (!ii.hbmColor) {
        // Monochrome cursor, return a copy
        HCURSOR hCopy = CopyIcon(hOriginal);
        DeleteObject(ii.hbmColor);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        return hCopy;
    }

    BITMAP bm = { 0 };
    GetObject(ii.hbmColor, sizeof(bm), &bm);

    if (bm.bmBitsPixel != 32 || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        HCURSOR hCopy = CopyIcon(hOriginal);
        DeleteObject(ii.hbmColor);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        return hCopy;
    }

    int pixelCount = bm.bmWidth * bm.bmHeight;
    std::vector<DWORD> pixels(pixelCount);
    GetBitmapBits(ii.hbmColor, pixelCount * static_cast<int>(sizeof(DWORD)), pixels.data());

    for (auto& pixel : pixels) {
        BYTE* ch = reinterpret_cast<BYTE*>(&pixel);
        ch[0] = 0; // B
        ch[1] = 0; // G
        ch[2] = 0; // R
    }

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bm.bmWidth;
    bmi.bmiHeader.biHeight = bm.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hbmColor = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hbmColor || !pBits) {
        if (hbmColor) DeleteObject(hbmColor);
        DeleteObject(ii.hbmColor);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        return CopyIcon(hOriginal);
    }
    memcpy(pBits, pixels.data(), pixelCount * static_cast<size_t>(sizeof(DWORD)));

    HBITMAP hbmMask = nullptr;
    if (ii.hbmMask) {
        BITMAP bmMask = { 0 };
        GetObject(ii.hbmMask, sizeof(bmMask), &bmMask);
        std::vector<BYTE> maskBits(static_cast<size_t>(bmMask.bmWidthBytes) * bmMask.bmHeight);
        GetBitmapBits(ii.hbmMask, static_cast<LONG>(maskBits.size()), maskBits.data());
        hbmMask = CreateBitmap(bmMask.bmWidth, bmMask.bmHeight, 1, 1, maskBits.data());
    }
    if (!hbmMask)
        hbmMask = CreateBitmap(1, 1, 1, 1, nullptr);

    ICONINFO iiNew = { 0 };
    iiNew.fIcon = FALSE;
    iiNew.xHotspot = ii.xHotspot;
    iiNew.yHotspot = ii.yHotspot;
    iiNew.hbmColor = hbmColor;
    iiNew.hbmMask = hbmMask;

    HCURSOR hDimmed = CreateIconIndirect(&iiNew);

    DeleteObject(ii.hbmColor);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    DeleteObject(hbmColor);
    DeleteObject(hbmMask);

    return hDimmed;
}

void DimmerManager::UpdateCursorDimming() {
    int dimLevel = 0;
    if (m_dimmingEnabled) {
        for (const auto& mon : m_monitors) {
            if (mon.enabled && mon.dimValue > dimLevel)
                dimLevel = mon.dimValue;
        }
    }
    if (m_isIdleState && m_idleDimLevel > dimLevel)
        dimLevel = m_idleDimLevel;

    bool shouldDim = dimLevel >= 5;

    if (shouldDim && !m_cursorDimmed) {
        CURSORINFO ci = { sizeof(CURSORINFO) };
        GetCursorInfo(&ci);
        m_hOriginalArrow = CopyIcon(ci.hCursor);
        if (!m_hOriginalArrow)
            m_hOriginalArrow = CopyIcon(LoadCursor(nullptr, IDC_ARROW));

        HCURSOR hDimmed = CreateDimmedCursor();
        if (hDimmed) {
            SetSystemCursor(hDimmed, OCR_NORMAL);
            m_cursorDimmed = true;
        }
    } else if (!shouldDim && m_cursorDimmed) {
        if (m_hOriginalArrow) {
            SetSystemCursor(m_hOriginalArrow, OCR_NORMAL);
            m_hOriginalArrow = nullptr;
        }
        m_cursorDimmed = false;
    }
}

void DimmerManager::TriggerFade(HWND hwnd) {
    if (hwnd) {
        SetTimer(hwnd, 1, 16, nullptr);
    }
}

void DimmerManager::DestroyOverlays() {
    for (auto& mon : m_monitors) {
        if (mon.hwndOverlay) {
            DestroyWindow(mon.hwndOverlay);
            mon.hwndOverlay = nullptr;
        }
    }
}

DimmerManager::~DimmerManager() {
    DestroyOverlays();
    if (m_cursorDimmed && m_hOriginalArrow) {
        SetSystemCursor(m_hOriginalArrow, OCR_NORMAL);
    }
    if (m_classRegistered) {
        UnregisterClassW(L"WinDimmer64OverlayClass", m_hInst);
    }
}

LRESULT CALLBACK DimmerManager::OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ActiveMonitorInfo* info = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        info = reinterpret_cast<ActiveMonitorInfo*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(info));
    } else if (msg == WM_NCDESTROY) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    } else {
        info = reinterpret_cast<ActiveMonitorInfo*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    switch (msg) {
        case WM_TIMER: {
            if (wp == 1 && info) {
                int target = 0;
                if (DimmerManager::Instance().IsIdleState()) {
                    // Dim when user is away (Idle Dimming)
                    target = DimmerManager::Instance().GetIdleDimLevel();
                } else if (DimmerManager::Instance().IsDimmingEnabled() && info->enabled) {
                    // Active dimming is enabled right now
                    target = info->dimValue;
                    if (DimmerManager::Instance().GetFocusMode()) {
                        POINT pt;
                        if (GetCursorPos(&pt)) {
                            HMONITOR hActive = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
                            MONITORINFOEXW mi;
                            mi.cbSize = sizeof(mi);
                            if (GetMonitorInfoW(hActive, &mi)) {
                                if (wcscmp(info->id.c_str(), mi.szDevice) != 0) {
                                    // Dim inactive monitor deeper (+25%, capped at 90%)
                                    target = info->dimValue + 25;
                                    if (target > 90) target = 90;
                                }
                            }
                        }
                    }
                }

                int diff = target - info->currentDimValue;
                if (diff == 0) {
                    KillTimer(hwnd, 1);
                } else {
                    // Exponential decay fade
                    int step = (diff > 0) ? (diff + 3) / 4 : (diff - 3) / 4;
                    if (abs(step) < 1) step = (diff > 0) ? 1 : -1;
                    info->currentDimValue += step;

                    BYTE alpha = static_cast<BYTE>((info->currentDimValue / 100.0) * 255.0);
                    SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (info) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                HBRUSH bgBrush;
                if (DimmerManager::Instance().GetWarmTint()) {
                    bgBrush = CreateSolidBrush(RGB(255, 130, 45)); // curated soothing warm amber
                } else {
                    bgBrush = CreateSolidBrush(RGB(0, 0, 0)); // standard black
                }
                FillRect(hdc, &rc, bgBrush);
                DeleteObject(bgBrush);

                if (DimmerManager::Instance().GetShowBoundaries()) {
                    HBRUSH yellowBrush = CreateSolidBrush(RGB(255, 235, 59));
                    FrameRect(hdc, &rc, yellowBrush);
                    for (int i = 1; i < 5; ++i) {
                        InflateRect(&rc, -1, -1);
                        FrameRect(hdc, &rc, yellowBrush);
                    }
                    DeleteObject(yellowBrush);
                }
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}
