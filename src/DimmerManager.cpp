#define NOMINMAX
#include "DimmerManager.h"
#include "ErrorCodes.h"
#include <vector>
#include <algorithm>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <shellapi.h>
#include <wrl/client.h>
#include <d2d1.h>
/* MinGW d2d1.h may not define this HRESULT */
#ifndef D2DERR_RECREATED
#define D2DERR_RECREATED ((HRESULT)0x88980001)
#endif
#include <dwrite.h>
#include <strsafe.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

using Microsoft::WRL::ComPtr;

// Define IAudioMeterInformation manually as MinGW headers only forward declare it
#ifndef __IAudioMeterInformation_INTERFACE_DEFINED__
#define __IAudioMeterInformation_INTERFACE_DEFINED__
#if defined(__cplusplus) && !defined(CINTERFACE)
interface IAudioMeterInformation : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float *pfPeak) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT *pnChannelCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT32 u32ChannelCount, float *afPeaks) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD *pdwHardwareSupportMask) = 0;
};
#endif
#endif

#include <initguid.h>
DEFINE_GUID(IID_IAudioMeterInformation, 0xC02216F6, 0x8C67, 0x4B5B, 0x9D, 0x00, 0xD0, 0x08, 0xE7, 0x3E, 0x00, 0x64);


// Helper to get friendly monitor name
static std::wstring GetMonitorFriendlyName(HMONITOR hMonitor, int index) {
    MONITORINFOEXW mi;
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMonitor, &mi)) {
        mi.szDevice[CCHDEVICENAME - 1] = L'\0';
        return L"Monitor " + std::to_wstring(index + 1) + L" (" + std::wstring(mi.szDevice) + L")";
    }
    return L"Monitor " + std::to_wstring(index + 1);
}

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    auto* list = reinterpret_cast<std::vector<ActiveMonitorInfo>*>(dwData);
    MONITORINFOEXW mi;
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMonitor, &mi)) {
        mi.szDevice[CCHDEVICENAME - 1] = L'\0';
        ActiveMonitorInfo info;
        info.id = mi.szDevice;
        info.friendlyName = GetMonitorFriendlyName(hMonitor, static_cast<int>(list->size()));
        info.rect = mi.rcMonitor;
        info.hMonitor = hMonitor;
        info.hasVideo = false;
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
        wc.hCursor = nullptr;
        wc.hbrBackground = nullptr; // Let WM_PAINT handle background to support custom warm tints
        wc.lpszClassName = L"IdleDimmerOverlayClass";
        if (!RegisterClassExW(&wc)) {
            LogError(ErrorCode::E402, HRESULT_FROM_WIN32(GetLastError()));
        } else {
            m_classRegistered = true;
        }
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
    UpdateCursorDimming();
}

void DimmerManager::CreateOverlayForMonitor(ActiveMonitorInfo& info) {
    int w = info.rect.right - info.rect.left;
    int h = info.rect.bottom - info.rect.top;

    info.hwndOverlay = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"IdleDimmerOverlayClass",
        L"IdleDimmerOverlay",
        WS_POPUP,
        info.rect.left, info.rect.top, w, h,
        nullptr, nullptr, m_hInst, &info
    );

    if (info.hwndOverlay) {
        // Start from 0 dim for a beautiful fade-in on startup!
        info.currentDimValue = 0;
        if (!SetLayeredWindowAttributes(info.hwndOverlay, 0, 0, LWA_ALPHA)) {
            LogError(ErrorCode::E404, HRESULT_FROM_WIN32(GetLastError()));
        }

        ShowWindow(info.hwndOverlay, SW_SHOWNOACTIVATE);
        UpdateWindow(info.hwndOverlay);

        // Defer fade start so WM_NCCREATE/GWLP_USERDATA is fully processed first
        PostMessageW(info.hwndOverlay, WM_APP + 1, 0, 0);
    } else {
        LogError(ErrorCode::E403, HRESULT_FROM_WIN32(GetLastError()));
    }
}

void DimmerManager::SetMonitorDim(std::wstring_view id, int value) {
    for (auto& mon : m_monitors) {
        if (mon.id == id) {
            mon.dimValue = value;
            if (mon.hwndOverlay) {
                TriggerFade(mon.hwndOverlay);
            }
            break;
        }
    }
    UpdateCursorDimming();
}

void DimmerManager::SetMonitorEnabled(std::wstring_view id, bool enabled) {
    for (auto& mon : m_monitors) {
        if (mon.id == id) {
            mon.enabled = enabled;
            if (mon.hwndOverlay) {
                TriggerFade(mon.hwndOverlay);
            }
            break;
        }
    }
    UpdateCursorDimming();
}

void DimmerManager::SetShowBoundaries(bool show) {
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
}

void DimmerManager::SetIdleState(bool idle, int idleLevel) {
    if (m_isIdleState != idle || m_idleDimLevel != idleLevel) {
        m_isIdleState = idle;
        m_idleDimLevel = idleLevel;
        m_cursorShifted = false;
        
        if (idle) {
            GetCursorPos(&m_lastMousePos);
        } else {
            m_lastMousePos = { -1, -1 };
        }

        for (auto& mon : m_monitors) {
            if (mon.hwndOverlay) {
                // Toggle WS_EX_TRANSPARENT: remove during idle (capture mouse for cursor hiding),
                // add back when exiting idle (click-through mode)
                LONG_PTR exStyle = GetWindowLongPtrW(mon.hwndOverlay, GWL_EXSTYLE);
                if (idle) {
                    exStyle &= ~WS_EX_TRANSPARENT;
                } else {
                    exStyle |= WS_EX_TRANSPARENT;
                }
                SetWindowLongPtrW(mon.hwndOverlay, GWL_EXSTYLE, exStyle);
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

void DimmerManager::UpdateCursorDimming() {
    int dimLevel = 0;
    if (m_isIdleState) {
        dimLevel = m_idleDimLevel;

        // If any connected monitor has video playing, the user is nearby.
        // In that case, we should not hide the cursor.
        for (const auto& mon : m_monitors) {
            if (mon.hasVideo) {
                dimLevel = 0;
                break;
            }
        }
    }

    bool shouldHide = dimLevel >= 5;

    if (shouldHide && !m_cursorHidden) {
        while (ShowCursor(FALSE) >= 0);
        m_cursorHidden = true;
    } else if (!shouldHide && m_cursorHidden) {
        while (ShowCursor(TRUE) < 0);
        m_cursorHidden = false;
    }
}

void DimmerManager::ShiftCursorForIdle() {
    if (m_cursorShifted) return;
    m_cursorShifted = true;

    POINT pt;
    if (GetCursorPos(&pt)) {
        m_isSettingCursorPos = true;
        int dx = (pt.x > 0) ? -1 : 1;
        SetCursorPos(pt.x + dx, pt.y);
    }
}

typedef HRESULT (WINAPI* PFN_SHQueryUserNotificationState)(QUERY_USER_NOTIFICATION_STATE*);

static bool IsFullscreenAppActive() {
    bool active = false;
    HMODULE hShell32 = LoadLibraryW(L"shell32.dll");
    if (hShell32) {
        auto pfn = reinterpret_cast<PFN_SHQueryUserNotificationState>(
            GetProcAddress(hShell32, "SHQueryUserNotificationState")
        );
        if (pfn) {
            QUERY_USER_NOTIFICATION_STATE state;
            if (SUCCEEDED(pfn(&state))) {
                if (state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_PRESENTATION_MODE) {
                    active = true;
                }
            }
        }
        FreeLibrary(hShell32);
    }
    return active;
}

static std::wstring GetProcessNameFromPid(DWORD pid) {
    wchar_t exe[MAX_PATH] = { 0 };
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc) {
        DWORD size = MAX_PATH;
        if (!QueryFullProcessImageNameW(hProc, 0, exe, &size)) {
            LogError(ErrorCode::E406, HRESULT_FROM_WIN32(GetLastError()));
        }
        CloseHandle(hProc);
    } else {
        LogError(ErrorCode::E418, HRESULT_FROM_WIN32(GetLastError()));
    }
    if (exe[0]) {
        const wchar_t* fname = wcsrchr(exe, L'\\');
        return fname ? fname + 1 : exe;
    }
    return L"";
}

static bool IsForegroundWindowFullscreen() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;

    // A minimized window cannot be fullscreen.
    if (IsIconic(hwnd)) return false;

    // Skip the desktop, taskbar, and IdleDimmer windows
    wchar_t className[256];
    if (GetClassNameW(hwnd, className, 256)) {
        if (wcscmp(className, L"Progman") == 0 || 
            wcscmp(className, L"WorkerW") == 0 || 
            wcscmp(className, L"Shell_TrayWnd") == 0 ||
            wcscmp(className, L"Shell_SecondaryTrayWnd") == 0 ||
            wcscmp(className, L"IdleDimmerMainClass") == 0 ||
            wcscmp(className, L"IdleDimmerOverlayClass") == 0) {
            return false;
        }
    }

    RECT rcWindow;
    if (GetWindowRect(hwnd, &rcWindow)) {
        HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfoW(hMonitor, &mi)) {
            // Check if window bounds cover the monitor bounds
            if (rcWindow.left <= mi.rcMonitor.left &&
                rcWindow.top <= mi.rcMonitor.top &&
                rcWindow.right >= mi.rcMonitor.right &&
                rcWindow.bottom >= mi.rcMonitor.bottom) {
                
                LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
                // True fullscreen = borderless popup OR captionless fullscreen (not just "not maximized")
                bool isBorderlessFullscreen = (style & WS_POPUP) && !(style & WS_CAPTION);
                bool isCaptionlessFullscreen = !(style & WS_MAXIMIZE) && !(style & WS_CAPTION);
                if (isBorderlessFullscreen || isCaptionlessFullscreen) {
                    return true;
                }
            }
        }
    }
    return false;
}

static std::wstring GetProcessNameFromPidNoLog(DWORD pid) {
    wchar_t exe[MAX_PATH] = { 0 };
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc) {
        DWORD size = MAX_PATH;
        QueryFullProcessImageNameW(hProc, 0, exe, &size);
        CloseHandle(hProc);
    }
    if (exe[0]) {
        const wchar_t* fname = wcsrchr(exe, L'\\');
        return fname ? fname + 1 : exe;
    }
    return L"";
}

struct ProcessNameMonitorInfo {
    std::wstring processName;
    std::vector<HMONITOR> monitors;
};

static BOOL CALLBACK EnumWindowsProcForProcessName(HWND hwnd, LPARAM lParam) {
    auto* info = reinterpret_cast<ProcessNameMonitorInfo*>(lParam);
    if (IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != 0) {
            std::wstring fname = GetProcessNameFromPidNoLog(pid);
            if (lstrcmpiW(fname.c_str(), info->processName.c_str()) == 0) {
                HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                if (hMon && std::find(info->monitors.begin(), info->monitors.end(), hMon) == info->monitors.end()) {
                    info->monitors.push_back(hMon);
                }
            }
        }
    }
    return TRUE;
}

static std::vector<HMONITOR> GetMonitorsForProcessName(const std::wstring& processName) {
    ProcessNameMonitorInfo info = { processName, {} };
    EnumWindows(EnumWindowsProcForProcessName, reinterpret_cast<LPARAM>(&info));
    return info.monitors;
}


void DimmerManager::CheckVideoPlayback() {
    m_videoCheckTick++;
    bool doFullCheck = (m_videoCheckTick % 5 == 0);

    if (doFullCheck) {
        m_isFullscreenAppActive = IsFullscreenAppActive();
    }

    // Determine current fullscreen monitor (on every second tick)
    HMONITOR hCurrentFullscreenMon = nullptr;
    if (IsForegroundWindowFullscreen() || m_isFullscreenAppActive) {
        HWND hFore = GetForegroundWindow();
        if (hFore) {
            hCurrentFullscreenMon = MonitorFromWindow(hFore, MONITOR_DEFAULTTONEAREST);
        }
    }

    if (doFullCheck) {
        // Reset audio video state on full check (every 5 seconds)
        for (auto& mon : m_monitors) {
            mon.hasAudioVideo = false;
        }

        // Check audio playback for browsers and blocked apps
        ComPtr<IMMDeviceEnumerator> pEnumerator;
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), &pEnumerator
        );
        if (SUCCEEDED(hr) && pEnumerator) {
            ComPtr<IMMDevice> pDevice;
            hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
            if (SUCCEEDED(hr) && pDevice) {
                ComPtr<IAudioSessionManager2> pSessionManager;
                hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, &pSessionManager);
                if (SUCCEEDED(hr) && pSessionManager) {
                    ComPtr<IAudioSessionEnumerator> pSessionEnumerator;
                    hr = pSessionManager->GetSessionEnumerator(&pSessionEnumerator);
                    if (SUCCEEDED(hr) && pSessionEnumerator) {
                        int count = 0;
                        if (SUCCEEDED(pSessionEnumerator->GetCount(&count))) {
                            for (int i = 0; i < count; ++i) {
                                ComPtr<IAudioSessionControl> pSessionControl;
                                hr = pSessionEnumerator->GetSession(i, &pSessionControl);
                                if (SUCCEEDED(hr) && pSessionControl) {
                                    ComPtr<IAudioSessionControl2> pSessionControl2;
                                    hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), &pSessionControl2);
                                    if (SUCCEEDED(hr) && pSessionControl2) {
                                        DWORD pid = 0;
                                        if (SUCCEEDED(pSessionControl2->GetProcessId(&pid)) && pid != 0) {
                                            std::wstring fname = GetProcessNameFromPid(pid);
                                            bool isTarget = false;
                                            for (const auto& name : m_blockedApps) {
                                                if (lstrcmpiW(fname.c_str(), name.c_str()) == 0) {
                                                    isTarget = true;
                                                    break;
                                                }
                                            }
                                            if (!isTarget) {
                                                // auto-check major browsers
                                                isTarget = (lstrcmpiW(fname.c_str(), L"chrome.exe") == 0 ||
                                                            lstrcmpiW(fname.c_str(), L"firefox.exe") == 0 ||
                                                            lstrcmpiW(fname.c_str(), L"msedge.exe") == 0 ||
                                                            lstrcmpiW(fname.c_str(), L"opera.exe") == 0 ||
                                                            lstrcmpiW(fname.c_str(), L"brave.exe") == 0);
                                            }
 
                                            if (isTarget) {
                                                ComPtr<IAudioMeterInformation> pMeter;
                                                hr = pSessionControl->QueryInterface(IID_IAudioMeterInformation, &pMeter);
                                                if (SUCCEEDED(hr) && pMeter) {
                                                    float peak = 0.0f;
                                                    if (SUCCEEDED(pMeter->GetPeakValue(&peak)) && peak > 0.0001f) {
                                                        // This process is playing audio. Set hasAudioVideo = true for its monitors
                                                        std::vector<HMONITOR> mons = GetMonitorsForProcessName(fname);
                                                        for (auto hMon : mons) {
                                                            for (auto& mon : m_monitors) {
                                                                 if (mon.hMonitor == hMon) {
                                                                     mon.hasAudioVideo = true;
                                                                 }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Update hasFullscreenVideo on every tick and combine with hasAudioVideo.
    // ── v1.6.5 (Todo 7): SW_HIDE overlays during video bypass ──
    // When a monitor is in video-bypass state, we hide the overlay window
    // completely with SW_HIDE instead of just fading it to alpha=0. This
    // eliminates any chance of a stale layered window intercepting clicks
    // or showing up in PowerPoint / presentation / screen-capture modes.
    // When video bypass ends we restore the overlay with SW_SHOWNOACTIVATE.
    for (auto& mon : m_monitors) {
        mon.hasFullscreenVideo = (mon.hMonitor == hCurrentFullscreenMon);

        bool newHasVideo = mon.hasAudioVideo || mon.hasFullscreenVideo;
        if (newHasVideo != mon.hasVideo) {
            mon.hasVideo = newHasVideo;
            if (mon.hwndOverlay) {
                if (newHasVideo) {
                    // Entering bypass: hide overlay completely.
                    ShowWindow(mon.hwndOverlay, SW_HIDE);
                } else {
                    // Leaving bypass: show again, no activation, no focus steal.
                    ShowWindow(mon.hwndOverlay, SW_SHOWNOACTIVATE);
                    TriggerFade(mon.hwndOverlay);
                }
            }
            UpdateCursorDimming();
        }
    }

    // ── v1.6.5 (Todo 2): Drive time-of-day scheduling on the same 1s tick ──
    CheckSchedule();
}

bool DimmerManager::IsAnyBlockedAppPlayingAudio() {
    return false;
}

void DimmerManager::SetBlockedApps(const std::vector<std::wstring>& apps) {
    m_blockedApps = apps;
}

// ── v1.6.5: Time-of-day scheduling ──
//
// A schedule is "active" right now if the current local wall-clock (as
// minutes-of-day) is between the configured start and end. Two cases:
//   1. Same-day span  (start < end): active if start <= now < end.
//   2. Overnight span (start >= end): active if now >= start OR now < end.
//      e.g. 22:00 → 07:00 wraps past midnight.
//
// When the schedule transitions active <-> inactive we trigger a fade on
// every monitor overlay. The overlay's WM_TIMER handler reads
// IsScheduleActive() and uses the schedule dim level as the fade target
// when neither idle nor video-bypass takes precedence.
void DimmerManager::SetScheduleRange(int startMins, int endMins, int dimLevel) {
    if (startMins < 0) startMins = 0;
    if (startMins >= 1440) startMins = 1439;
    if (endMins < 0) endMins = 0;
    if (endMins >= 1440) endMins = 1439;
    if (dimLevel < 0) dimLevel = 0;
    if (dimLevel > 90) dimLevel = 90;

    m_scheduleStartMins = startMins;
    m_scheduleEndMins   = endMins;
    m_scheduleDimLevel  = dimLevel;
}

void DimmerManager::CheckSchedule() {
    if (!m_scheduleEnabled) {
        if (m_scheduleActive) {
            m_scheduleActive = false;
            for (auto& mon : m_monitors) {
                if (mon.hwndOverlay) TriggerFade(mon.hwndOverlay);
            }
        }
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    int nowMins = st.wHour * 60 + st.wMinute;

    bool inSpan;
    if (m_scheduleStartMins < m_scheduleEndMins) {
        // Same-day span: e.g. 09:00 → 17:00
        inSpan = (nowMins >= m_scheduleStartMins && nowMins < m_scheduleEndMins);
    } else if (m_scheduleStartMins > m_scheduleEndMins) {
        // Overnight span: e.g. 22:00 → 07:00 wraps midnight
        inSpan = (nowMins >= m_scheduleStartMins || nowMins < m_scheduleEndMins);
    } else {
        // start == end means "always on"
        inSpan = true;
    }

    if (inSpan != m_scheduleActive) {
        m_scheduleActive = inSpan;
        for (auto& mon : m_monitors) {
            if (mon.hwndOverlay) TriggerFade(mon.hwndOverlay);
        }
        UpdateCursorDimming();
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
    DestroyOSD();
    DestroyOverlays();
    if (m_cursorHidden) {
        ShowCursor(TRUE);
    }
    if (m_classRegistered) {
        UnregisterClassW(L"IdleDimmerOverlayClass", m_hInst);
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
        case WM_APP + 1: {
            // Deferred fade start after overlay creation
            DimmerManager::Instance().TriggerFade(hwnd);
            return 0;
        }
        case WM_TIMER: {
            if (wp == 1 && info) {
                int target = 0;
                if (info->hasVideo) {
                    // v1.6.5 (Todo 7): overlay is hidden via SW_HIDE by
                    // CheckVideoPlayback; if we still get a timer tick here
                    // while hasVideo is true, just hold alpha at 0.
                    target = 0;
                } else if (DimmerManager::Instance().IsIdleState() && info->enabled) {
                    target = (std::max)(info->dimValue, DimmerManager::Instance().GetIdleDimLevel());
                    if (target > 100) target = 100;
                } else if (DimmerManager::Instance().IsScheduleActive() && info->enabled) {
                    // v1.6.5 (Todo 2): scheduled dimming. Use the schedule
                    // dim level if it is higher than the user-set per-monitor
                    // value (i.e. schedule can darken further, not lighten).
                    target = (std::max)(info->dimValue,
                                        DimmerManager::Instance().GetScheduleDimLevel());
                    if (target > 100) target = 100;
                } else if (DimmerManager::Instance().IsDimmingEnabled() && info->enabled) {
                    // Active dimming is enabled right now
                    target = info->dimValue;
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

                    if (DimmerManager::Instance().IsIdleState() && info->currentDimValue >= 1 && !DimmerManager::Instance().HasShiftedCursor()) {
                        POINT pt;
                        if (GetCursorPos(&pt)) {
                            RECT rcWnd;
                            GetWindowRect(hwnd, &rcWnd);
                            if (PtInRect(&rcWnd, pt)) {
                                DimmerManager::Instance().ShiftCursorForIdle();
                            }
                        }
                    }
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
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_WINDOWPOSCHANGING: {
            WINDOWPOS* wpos = reinterpret_cast<WINDOWPOS*>(lp);
            wpos->hwndInsertAfter = HWND_TOPMOST;
            wpos->flags &= ~SWP_NOZORDER;
            return 0;
        }
        case WM_NCHITTEST: {
            if (DimmerManager::Instance().IsIdleState()) {
                return HTCLIENT;
            } else {
                return HTTRANSPARENT;
            }
        }
        case WM_SETCURSOR: {
            if (DimmerManager::Instance().IsIdleState()) {
                SetCursor(nullptr);
            } else {
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
            }
            return TRUE;
        }
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            if (DimmerManager::Instance().IsIdleState()) {
                if (DimmerManager::Instance().IsSettingCursorPos()) {
                    DimmerManager::Instance().SetSettingCursorPos(false);
                    return DefWindowProcW(hwnd, msg, wp, lp);
                }
                POINT pt;
                if (GetCursorPos(&pt)) {
                    POINT lastPt = DimmerManager::Instance().GetLastMousePos();
                    if (pt.x == lastPt.x && pt.y == lastPt.y) {
                        return DefWindowProcW(hwnd, msg, wp, lp);
                    }
                    DimmerManager::Instance().SetLastMousePos(pt);
                }

                DimmerManager::Instance().SetIdleState(false);
                HWND hwndMain = FindWindowW(L"IdleDimmerMainClass", nullptr);
                if (hwndMain) {
                    InvalidateRect(hwndMain, nullptr, FALSE);
                }
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// v1.6.5 (Todos 3+4): Click-through OSD window
// ════════════════════════════════════════════════════════════════════════════
//
// Design notes:
//   - Window class "IdleDimmerOSDClass" is registered alongside the overlay
//     class in Initialize(). The OSD uses its own Direct2D factory and
//     render target, completely isolated from the MainWindow D2D state —
//     that way a D2D device-lost on the OSD cannot crash the settings UI.
//   - Window styles: WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE
//                    | WS_EX_TOOLWINDOW | WS_EX_TOPMOST.
//     * LAYERED   → per-pixel alpha + smooth fade via SetLayeredWindowAttributes
//     * TRANSPARENT → click-through: mouse events pass through to windows below
//     * NOACTIVATE → never steals focus (certification requirement)
//     * TOOLWINDOW → never appears in taskbar / Alt+Tab
//     * TOPMOST    → sits above fullscreen apps so the user actually sees it
//   - The window is positioned bottom-right of the primary monitor with a
//     24px margin. It is 220×64 px (enough for "Brightness: 75%" + status line).
//   - ShowOSD sets a 2s display timer; when it fires, we switch to fade-out
//     mode (target alpha = 0) and run a ~16ms timer until alpha reaches 0,
//     at which point we hide the window but keep it around for the next call.
//   - If Direct2D / DirectWrite fails to initialize on the host system
//     (e.g. stripped-down Windows images), we fall back to a plain GDI
//     BitBlt + DrawTextW paint path so the OSD still works.

#define OSD_TIMER_DISPLAY  901   // 2s display timer
#define OSD_TIMER_FADE     902   // ~16ms fade-out timer
#define OSD_W 230
#define OSD_H 68

static RECT CalcOSDRect() {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    RECT rc;
    rc.left   = sw - OSD_W - 24;
    rc.top    = sh - OSD_H - 24;
    rc.right  = rc.left + OSD_W;
    rc.bottom = rc.top  + OSD_H;
    return rc;
}

void DimmerManager::EnsureOSDResources() {
    if (m_pOSDFactory && m_pOSDTarget) return; // already initialized

    if (!m_pOSDFactory) {
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pOSDFactory);
    }
    if (!m_pOSDDWrite) {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                            __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown**>(&m_pOSDDWrite));
    }
    if (!m_pOSDTextFormat && m_pOSDDWrite) {
        // Try Segoe UI Variable first, fall back to Segoe UI (matches v1.6.3
        // font-fallback hardening for stripped Windows images).
        HRESULT hr = m_pOSDDWrite->CreateTextFormat(
            L"Segoe UI Variable Text", nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"en-us", &m_pOSDTextFormat);
        if (FAILED(hr) && m_pOSDDWrite) {
            m_pOSDDWrite->CreateTextFormat(
                L"Segoe UI", nullptr,
                DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"en-us", &m_pOSDTextFormat);
        }
    }

    if (!m_pOSDTarget && m_pOSDFactory && m_hwndOSD) {
        RECT rc;
        GetClientRect(m_hwndOSD, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(
            std::max<UINT32>(1, rc.right - rc.left),
            std::max<UINT32>(1, rc.bottom - rc.top));
        m_pOSDFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwndOSD, size),
            &m_pOSDTarget);
    }
    if (m_pOSDTarget) {
        if (!m_pOSDBrushBg)     m_pOSDTarget->CreateSolidColorBrush(D2D1::ColorF(0x1E1E1E, 0.92f), &m_pOSDBrushBg);
        if (!m_pOSDBrushText)   m_pOSDTarget->CreateSolidColorBrush(D2D1::ColorF(0xE1E1E1, 1.0f),  &m_pOSDBrushText);
        if (!m_pOSDBrushAccent) m_pOSDTarget->CreateSolidColorBrush(D2D1::ColorF(0x8E8E93, 1.0f),  &m_pOSDBrushAccent);
    }
}

void DimmerManager::DiscardOSDResources() {
    if (m_pOSDBrushBg)     { m_pOSDBrushBg->Release();     m_pOSDBrushBg = nullptr; }
    if (m_pOSDBrushText)   { m_pOSDBrushText->Release();   m_pOSDBrushText = nullptr; }
    if (m_pOSDBrushAccent) { m_pOSDBrushAccent->Release(); m_pOSDBrushAccent = nullptr; }
    if (m_pOSDTextFormat)  { m_pOSDTextFormat->Release();  m_pOSDTextFormat = nullptr; }
    if (m_pOSDTarget)      { m_pOSDTarget->Release();      m_pOSDTarget = nullptr; }
    if (m_pOSDDWrite)      { m_pOSDDWrite->Release();      m_pOSDDWrite = nullptr; }
    if (m_pOSDFactory)     { m_pOSDFactory->Release();     m_pOSDFactory = nullptr; }
}

void DimmerManager::ShowOSD(const std::wstring& text) {
    if (!m_hInst) return; // not initialized yet

    if (!m_osdClassRegistered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = OSDWndProc;
        wc.hInstance = m_hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"IdleDimmerOSDClass";
        if (!RegisterClassExW(&wc)) {
            return;
        }
        m_osdClassRegistered = true;
    }

    if (!m_hwndOSD) {
        RECT rc = CalcOSDRect();
        m_hwndOSD = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
                WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            L"IdleDimmerOSDClass",
            L"IdleDimmerOSD",
            WS_POPUP,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, m_hInst, nullptr);
        if (!m_hwndOSD) return;
        // Start fully transparent; we'll fade in.
        SetLayeredWindowAttributes(m_hwndOSD, 0, 0, LWA_ALPHA);
    } else {
        // Reposition in case monitor resolution changed since last show.
        RECT rc = CalcOSDRect();
        SetWindowPos(m_hwndOSD, HWND_TOPMOST, rc.left, rc.top,
                     rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    m_osdText = text;
    m_osdAlpha = 0;
    m_osdTargetAlpha = 235; // ~92% opaque — leaves a soft translucency
    SetLayeredWindowAttributes(m_hwndOSD, 0, (BYTE)m_osdAlpha, LWA_ALPHA);

    ShowWindow(m_hwndOSD, SW_SHOWNOACTIVATE);
    InvalidateRect(m_hwndOSD, nullptr, FALSE);

    // Arm display timer (2 seconds) and the fade-in timer (~16ms).
    SetTimer(m_hwndOSD, OSD_TIMER_DISPLAY, 2000, nullptr);
    SetTimer(m_hwndOSD, OSD_TIMER_FADE,    16,   nullptr);
}

void DimmerManager::DestroyOSD() {
    if (m_hwndOSD) {
        KillTimer(m_hwndOSD, OSD_TIMER_DISPLAY);
        KillTimer(m_hwndOSD, OSD_TIMER_FADE);
        DestroyWindow(m_hwndOSD);
        m_hwndOSD = nullptr;
    }
    DiscardOSDResources();
    if (m_osdClassRegistered && m_hInst) {
        UnregisterClassW(L"IdleDimmerOSDClass", m_hInst);
        m_osdClassRegistered = false;
    }
}

LRESULT CALLBACK DimmerManager::OSDWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            auto& osd = DimmerManager::Instance();
            osd.EnsureOSDResources();

            bool drewWithD2D = false;
            if (osd.m_pOSDTarget && osd.m_pOSDBrushBg && osd.m_pOSDBrushText &&
                osd.m_pOSDTextFormat) {
                osd.m_pOSDTarget->BeginDraw();
                osd.m_pOSDTarget->SetTransform(D2D1::IdentityMatrix());

                D2D1_ROUNDED_RECT card = D2D1::RoundedRect(
                    D2D1::RectF(0, 0, (float)OSD_W, (float)OSD_H),
                    10.0f, 10.0f);
                osd.m_pOSDTarget->FillRoundedRectangle(card, osd.m_pOSDBrushBg);
                osd.m_pOSDTarget->DrawRoundedRectangle(card, osd.m_pOSDBrushAccent, 1.0f);

                // Accent bar on the left edge (3px wide) for a techy look.
                D2D1_RECT_F bar = D2D1::RectF(0, 8, 3, (float)OSD_H - 8);
                osd.m_pOSDTarget->FillRectangle(bar, osd.m_pOSDBrushAccent);

                // Text — centered vertically.
                osd.m_pOSDTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                osd.m_pOSDTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                osd.m_pOSDTarget->DrawText(
                    osd.m_osdText.c_str(),
                    (UINT32)osd.m_osdText.length(),
                    osd.m_pOSDTextFormat,
                    D2D1::RectF(14.0f, 0, (float)OSD_W - 12.0f, (float)OSD_H),
                    osd.m_pOSDBrushText);

                HRESULT hr = osd.m_pOSDTarget->EndDraw();
                if (hr == D2DERR_RECREATED) {
                    osd.DiscardOSDResources();
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                drewWithD2D = true;
            }

            if (!drewWithD2D) {
                // ── GDI fallback (stripped Windows images w/o D2D) ──
                RECT rc;
                GetClientRect(hwnd, &rc);
                // Fill background dark.
                HBRUSH bg = CreateSolidBrush(RGB(0x1E, 0x1E, 0x1E));
                FillRect(hdc, &rc, bg);
                DeleteObject(bg);
                // Draw text in light grey.
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(0xE1, 0xE1, 0xE1));
                HFONT hFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                          CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                          DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                HFONT hOld = (HFONT)SelectObject(hdc, hFont);
                RECT textRc = { 14, 0, OSD_W - 12, OSD_H };
                DrawTextW(hdc, osd.m_osdText.c_str(), (int)osd.m_osdText.length(),
                          &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, hOld);
                DeleteObject(hFont);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER: {
            auto& osd = DimmerManager::Instance();
            if (wp == OSD_TIMER_DISPLAY) {
                // 2s display elapsed → begin fade-out.
                KillTimer(hwnd, OSD_TIMER_DISPLAY);
                osd.m_osdTargetAlpha = 0;
            } else if (wp == OSD_TIMER_FADE) {
                int diff = osd.m_osdTargetAlpha - osd.m_osdAlpha;
                if (diff == 0) {
                    if (osd.m_osdTargetAlpha == 0) {
                        // Fully faded out — stop timer + hide window.
                        KillTimer(hwnd, OSD_TIMER_FADE);
                        ShowWindow(hwnd, SW_HIDE);
                    }
                    // Else: at peak, idle until display timer fires.
                } else {
                    int step = (diff > 0) ? (diff + 7) / 8 : (diff - 7) / 8;
                    if (step == 0) step = (diff > 0) ? 1 : -1;
                    osd.m_osdAlpha += step;
                    if (osd.m_osdAlpha < 0)   osd.m_osdAlpha = 0;
                    if (osd.m_osdAlpha > 255) osd.m_osdAlpha = 255;
                    SetLayeredWindowAttributes(hwnd, 0, (BYTE)osd.m_osdAlpha, LWA_ALPHA);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }
        case WM_NCHITTEST:
            // Click-through: anything that "hits" the OSD passes straight through.
            return HTTRANSPARENT;
        case WM_WINDOWPOSCHANGING: {
            WINDOWPOS* wpos = reinterpret_cast<WINDOWPOS*>(lp);
            wpos->hwndInsertAfter = HWND_TOPMOST;
            wpos->flags &= ~SWP_NOZORDER;
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}
