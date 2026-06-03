#include "DimmerManager.h"
#include "ErrorCodes.h"
#include <vector>
#include <algorithm>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <shellapi.h>

// Define IAudioMeterInformation manually as MinGW headers only forward declare it
#ifndef __IAudioMeterInformation_INTERFACE_DEFINED__
#define __IAudioMeterInformation_INTERFACE_DEFINED__

const IID IID_IAudioMeterInformation = {0xC02216F6, 0x8C67, 0x4B5B, {0x9D, 0x00, 0xD0, 0x08, 0xE7, 0x3E, 0x00, 0x64}};

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
        WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
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

        TriggerFade(info.hwndOverlay);
    } else {
        LogError(ErrorCode::E403, HRESULT_FROM_WIN32(GetLastError()));
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
    UpdateCursorDimming();
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
        
        if (idle) {
            GetCursorPos(&m_lastMousePos);
        } else {
            m_lastMousePos = { -1, -1 };
        }

        for (auto& mon : m_monitors) {
            if (mon.hwndOverlay) {
                TriggerFade(mon.hwndOverlay);
            }
        }
        UpdateCursorDimming();

        if (idle) {
            // Force cursor to update/hide immediately by shifting it 1px to trigger WM_SETCURSOR and hit-testing
            POINT pt;
            if (GetCursorPos(&pt)) {
                m_isSettingCursorPos = true;
                int dx = (pt.x > 0) ? -1 : 1;
                SetCursorPos(pt.x + dx, pt.y);
                SetCursorPos(pt.x, pt.y);
                m_isSettingCursorPos = false;
            }
        }
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
    if (m_isIdleState)
        dimLevel = m_idleDimLevel;

    // Check if the monitor under the cursor is playing video/audio.
    // If so, do not hide the cursor or dim the cursor.
    POINT pt;
    if (GetCursorPos(&pt)) {
        HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        for (const auto& mon : m_monitors) {
            if (mon.hMonitor == hMon && mon.hasVideo) {
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

// Helper struct for child window enumeration (UWP app container breakout)
struct UwpWindowSearchInfo {
    DWORD hostPid;
    DWORD targetPid;
};

static BOOL CALLBACK EnumUwpChildProc(HWND hwnd, LPARAM lParam) {
    auto* info = reinterpret_cast<UwpWindowSearchInfo*>(lParam);
    DWORD childPid = 0;
    GetWindowThreadProcessId(hwnd, &childPid);
    if (childPid != 0 && childPid != info->hostPid) {
        info->targetPid = childPid;
        return FALSE; // Found the real application window, stop enumerating
    }
    return TRUE;
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

static DWORD GetRealProcessId(HWND hwnd) {
    if (!hwnd) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    // Check if the foreground window process is ApplicationFrameHost.exe
    wchar_t exe[MAX_PATH] = { 0 };
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc) {
        DWORD size = MAX_PATH;
        if (!QueryFullProcessImageNameW(hProc, 0, exe, &size)) {
            LogError(ErrorCode::E406, HRESULT_FROM_WIN32(GetLastError()));
        }
        CloseHandle(hProc);
    } else {
        LogError(ErrorCode::E417, HRESULT_FROM_WIN32(GetLastError()));
    }

    if (exe[0]) {
        const wchar_t* fname = wcsrchr(exe, L'\\');
        fname = fname ? fname + 1 : exe;
        if (lstrcmpiW(fname, L"ApplicationFrameHost.exe") == 0) {
            UwpWindowSearchInfo info = { pid, 0 };
            EnumChildWindows(hwnd, EnumUwpChildProc, reinterpret_cast<LPARAM>(&info));
            if (info.targetPid != 0) {
                return info.targetPid;
            }
        }
    }
    return pid;
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

struct PidMonitorInfo {
    DWORD pid;
    std::vector<HMONITOR> monitors;
};

static BOOL CALLBACK EnumWindowsProcForPid(HWND hwnd, LPARAM lParam) {
    auto* info = reinterpret_cast<PidMonitorInfo*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == info->pid && IsWindowVisible(hwnd)) {
        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (hMon && std::find(info->monitors.begin(), info->monitors.end(), hMon) == info->monitors.end()) {
            info->monitors.push_back(hMon);
        }
    }
    return TRUE;
}

static std::vector<HMONITOR> GetMonitorsForPid(DWORD pid) {
    PidMonitorInfo info = { pid, {} };
    EnumWindows(EnumWindowsProcForPid, reinterpret_cast<LPARAM>(&info));
    return info.monitors;
}

void DimmerManager::CheckVideoPlayback() {
    m_videoCheckTick++;
    bool doFullCheck = (m_videoCheckTick % 5 == 0);

    if (doFullCheck) {
        // Reset all monitors video state
        for (auto& mon : m_monitors) {
            mon.hasVideo = false;
        }

        // 1. Check fullscreen window monitor
        if (IsForegroundWindowFullscreen() || IsFullscreenAppActive()) {
            HWND hFore = GetForegroundWindow();
            if (hFore) {
                HMONITOR hMon = MonitorFromWindow(hFore, MONITOR_DEFAULTTONEAREST);
                for (auto& mon : m_monitors) {
                    if (mon.hMonitor == hMon) {
                        mon.hasVideo = true;
                    }
                }
            }
        }

        // 2. Check audio playback for browsers and blocked apps
        IMMDeviceEnumerator* pEnumerator = nullptr;
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&pEnumerator)
        );
        if (SUCCEEDED(hr)) {
            IMMDevice* pDevice = nullptr;
            hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
            if (SUCCEEDED(hr)) {
                IAudioSessionManager2* pSessionManager = nullptr;
                hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                    reinterpret_cast<void**>(&pSessionManager));
                if (SUCCEEDED(hr)) {
                    IAudioSessionEnumerator* pSessionEnumerator = nullptr;
                    hr = pSessionManager->GetSessionEnumerator(&pSessionEnumerator);
                    if (SUCCEEDED(hr)) {
                        int count = 0;
                        if (SUCCEEDED(pSessionEnumerator->GetCount(&count))) {
                            for (int i = 0; i < count; ++i) {
                                IAudioSessionControl* pSessionControl = nullptr;
                                hr = pSessionEnumerator->GetSession(i, &pSessionControl);
                                if (SUCCEEDED(hr)) {
                                    IAudioSessionControl2* pSessionControl2 = nullptr;
                                    hr = pSessionControl->QueryInterface(
                                        __uuidof(IAudioSessionControl2),
                                        reinterpret_cast<void**>(&pSessionControl2));
                                    if (SUCCEEDED(hr)) {
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
                                                IAudioMeterInformation* pMeter = nullptr;
                                                hr = pSessionControl->QueryInterface(
                                                    IID_IAudioMeterInformation,
                                                    reinterpret_cast<void**>(&pMeter));
                                                if (SUCCEEDED(hr)) {
                                                    float peak = 0.0f;
                                                    if (SUCCEEDED(pMeter->GetPeakValue(&peak)) && peak > 0.0001f) {
                                                        // This process is playing audio. Set hasVideo = true for its monitors
                                                        std::vector<HMONITOR> mons = GetMonitorsForPid(pid);
                                                        for (auto hMon : mons) {
                                                            for (auto& mon : m_monitors) {
                                                                if (mon.hMonitor == hMon) {
                                                                    mon.hasVideo = true;
                                                                }
                                                            }
                                                        }
                                                    }
                                                    pMeter->Release();
                                                }
                                            }
                                        }
                                        pSessionControl2->Release();
                                    }
                                    pSessionControl->Release();
                                }
                            }
                        }
                        pSessionEnumerator->Release();
                    }
                    pSessionManager->Release();
                }
                pDevice->Release();
            }
            pEnumerator->Release();
        }

        // Trigger fades for monitor overlays to match their video status
        for (auto& mon : m_monitors) {
            if (mon.hwndOverlay) TriggerFade(mon.hwndOverlay);
        }
        UpdateCursorDimming();
    } else {
        // Lightweight tick: check if foreground window is fullscreen
        if (IsForegroundWindowFullscreen()) {
            HWND hFore = GetForegroundWindow();
            if (hFore) {
                HMONITOR hMon = MonitorFromWindow(hFore, MONITOR_DEFAULTTONEAREST);
                for (auto& mon : m_monitors) {
                    if (mon.hMonitor == hMon && !mon.hasVideo) {
                        mon.hasVideo = true;
                        if (mon.hwndOverlay) TriggerFade(mon.hwndOverlay);
                        UpdateCursorDimming();
                    }
                }
            }
        }
    }
}

bool DimmerManager::IsAnyBlockedAppPlayingAudio() {
    return false;
}

void DimmerManager::SetBlockedApps(const std::vector<std::wstring>& apps) {
    m_blockedApps = apps;
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
        case WM_TIMER: {
            if (wp == 1 && info) {
                int target = 0;
                if (info->hasVideo) {
                    target = 0;
                } else if (DimmerManager::Instance().IsIdleState() && info->enabled) {
                    target = (std::max)(info->dimValue, DimmerManager::Instance().GetIdleDimLevel());
                    if (target > 90) target = 90;
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

                // Draw a fake cursor under the overlay during idle dimming
                if (DimmerManager::Instance().IsIdleState()) {
                    POINT pt = DimmerManager::Instance().GetLastMousePos();
                    RECT rcWnd;
                    GetWindowRect(hwnd, &rcWnd);
                    if (PtInRect(&rcWnd, pt)) {
                        POINT clientPt = pt;
                        ScreenToClient(hwnd, &clientPt);
                        HCURSOR hCursor = LoadCursor(nullptr, IDC_ARROW);
                        if (hCursor) {
                            DrawIconEx(hdc, clientPt.x, clientPt.y, hCursor, 0, 0, 0, nullptr, DI_NORMAL);
                        }
                    }
                }
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
