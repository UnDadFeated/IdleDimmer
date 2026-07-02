#pragma once
#define NOMINMAX
#include <windows.h>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <d2d1.h>
#include <dwrite.h>
#include <mutex>
#include <thread>
#include <atomic>

struct ActiveMonitorInfo {
    std::wstring id;
    std::wstring friendlyName;
    RECT rect;
    HWND hwndOverlay = nullptr;
    int dimValue = 30; // 0 to 90
    int currentDimValue = 0; // for smooth transition fading
    bool enabled = true;
    HMONITOR hMonitor = nullptr;
    bool hasVideo = false;
    bool hasAudioVideo = false;
    bool hasFullscreenVideo = false;
};

class DimmerManager {
public:
    static DimmerManager& Instance() {
        static DimmerManager instance;
        return instance;
    }

    void Initialize(HINSTANCE hInst);
    void RefreshMonitors();
    void SetMonitorDim(std::wstring_view id, int value);
    void SetMonitorEnabled(std::wstring_view id, bool enabled);
    void SetShowBoundaries(bool show);
    void SetWarmTint(bool warm);
    void SetFocusMode(bool focus);
    void SetIdleState(bool idle, int idleLevel = 90);
    void TriggerFade(HWND hwnd);
    void DestroyOverlays();

    const std::vector<ActiveMonitorInfo>& GetActiveMonitors() const { return m_monitors; }
    bool GetWarmTint() const { return m_warmTint; }
    bool IsIdleState() const { return m_isIdleState; }
    int GetIdleDimLevel() const { return m_idleDimLevel; }
    void SetDimmingEnabled(bool enabled);
    bool IsDimmingEnabled() const { return m_dimmingEnabled; }
    void UpdateCursorDimming();
    void CheckVideoPlayback();
    void SetBlockedApps(const std::vector<std::wstring>& apps);

    // ── v1.6.5: Time-of-day scheduling ──
    void SetScheduleEnabled(bool enabled) { m_scheduleEnabled = enabled; }
    void SetScheduleRange(int startMins, int endMins, int dimLevel);
    bool IsScheduleActive() const { return m_scheduleActive; }
    int  GetScheduleDimLevel() const { return m_scheduleDimLevel; }
    void CheckSchedule();

    // ── v1.6.5 (Todos 3+4): Click-through OSD ──
    // ShowOSD(text) pops a sleek dark card at the bottom-right of the primary
    // monitor for ~2 seconds, then fades out. The window is non-activating,
    // click-through and topmost — it never steals focus or intercepts input.
    void ShowOSD(const std::wstring& text);
    void DestroyOSD();
    bool IsVideoDetected() const {
        for (const auto& mon : m_monitors) {
            if (mon.hasVideo) return true;
        }
        return false;
    }
    bool IsAnyBlockedAppPlayingAudio();
    POINT GetLastMousePos() const { return m_lastMousePos; }
    void SetLastMousePos(POINT pt) { m_lastMousePos = pt; }
    bool IsSettingCursorPos() const { return m_isSettingCursorPos; }
    void SetSettingCursorPos(bool val) { m_isSettingCursorPos = val; }
    bool HasShiftedCursor() const { return m_cursorShifted; }
    void ShiftCursorForIdle();

private:
    DimmerManager() = default;
    ~DimmerManager();

    DimmerManager(const DimmerManager&) = delete;
    DimmerManager& operator=(const DimmerManager&) = delete;

    void CreateOverlayForMonitor(ActiveMonitorInfo& info);
    static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void CheckAudioPlaybackAsync();

    HINSTANCE m_hInst = nullptr;
    std::vector<ActiveMonitorInfo> m_monitors;
    bool m_warmTint = false;

    // Asynchronous audio checking
    std::atomic<bool> m_audioCheckInFlight = false;
    std::mutex m_audioMutex;
    std::vector<std::wstring> m_audioPlayingProcesses;  // process names with active audio
    bool m_isIdleState = false;
    int m_idleDimLevel = 90;
    bool m_dimmingEnabled = false;
    bool m_classRegistered = false;

    bool m_cursorHidden = false;
    int m_videoCheckTick = 0;
    bool m_isFullscreenAppActive = false;
    std::vector<std::wstring> m_blockedApps;
    POINT m_lastMousePos = { -1, -1 };
    bool m_isSettingCursorPos = false;
    bool m_cursorShifted = false;

    // ── v1.6.5: Scheduling state ──
    bool m_scheduleEnabled  = false;
    bool m_scheduleActive   = false;  // true while current wall-clock is inside the schedule
    int  m_scheduleStartMins = 1320;  // 22:00
    int  m_scheduleEndMins   = 420;   // 07:00
    int  m_scheduleDimLevel  = 60;

    // ── v1.6.5 (Todos 3+4): OSD window state ──
    HWND m_hwndOSD = nullptr;
    bool m_osdClassRegistered = false;
    // Direct2D resources for OSD. Stored as raw pointers; lifecycle is fully
    // owned by DimmerManager. They are created lazily on first paint and
    // released in DestroyOSD / destructor.
    ID2D1Factory*             m_pOSDFactory       = nullptr;
    ID2D1HwndRenderTarget*    m_pOSDTarget        = nullptr;
    IDWriteFactory*           m_pOSDDWrite        = nullptr;
    IDWriteTextFormat*        m_pOSDTextFormat    = nullptr;
    ID2D1SolidColorBrush*     m_pOSDBrushBg       = nullptr;
    ID2D1SolidColorBrush*     m_pOSDBrushText     = nullptr;
    ID2D1SolidColorBrush*     m_pOSDBrushAccent   = nullptr;
    std::wstring              m_osdText;
    int                       m_osdAlpha = 0;     // 0..255 current alpha
    int                       m_osdTargetAlpha = 0;
    void EnsureOSDResources();
    void DiscardOSDResources();
    static LRESULT CALLBACK OSDWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};
