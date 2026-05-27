#include "DimmerManager.h"
#include <iostream>

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
        SetWindowLongPtrW(info.hwndOverlay, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&info));
        
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
    if (m_classRegistered) {
        UnregisterClassW(L"WinDimmer64OverlayClass", m_hInst);
    }
}

LRESULT CALLBACK DimmerManager::OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* info = reinterpret_cast<ActiveMonitorInfo*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_TIMER: {
            if (wp == 1 && info) {
                int target = info->enabled ? info->dimValue : 0;
                if (info->enabled && DimmerManager::Instance().IsIdleState()) {
                    target = DimmerManager::Instance().GetIdleDimLevel();
                } else if (info->enabled && DimmerManager::Instance().GetFocusMode()) {
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
