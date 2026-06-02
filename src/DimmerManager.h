#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>

struct ActiveMonitorInfo {
    std::wstring id;
    std::wstring friendlyName;
    RECT rect;
    HWND hwndOverlay = nullptr;
    int dimValue = 30; // 0 to 90
    int currentDimValue = 0; // for smooth transition fading
    bool enabled = true;
};

class DimmerManager {
public:
    static DimmerManager& Instance() {
        static DimmerManager instance;
        return instance;
    }

    void Initialize(HINSTANCE hInst);
    void RefreshMonitors();
    void SetMonitorDim(const std::wstring& id, int value);
    void SetMonitorEnabled(const std::wstring& id, bool enabled);
    void SetShowBoundaries(bool show);
    void SetWarmTint(bool warm);
    void SetFocusMode(bool focus);
    void SetIdleState(bool idle, int idleLevel = 90);
    void TriggerFade(HWND hwnd);
    void DestroyOverlays();

    const std::vector<ActiveMonitorInfo>& GetActiveMonitors() const { return m_monitors; }
    bool GetShowBoundaries() const { return m_showBoundaries; }
    bool GetWarmTint() const { return m_warmTint; }
    bool GetFocusMode() const { return m_focusMode; }
    bool IsIdleState() const { return m_isIdleState; }
    int GetIdleDimLevel() const { return m_idleDimLevel; }
    void SetDimmingEnabled(bool enabled);
    bool IsDimmingEnabled() const { return m_dimmingEnabled; }
    void UpdateCursorDimming();
    void CheckVideoPlayback();
    void SetBlockedApps(const std::vector<std::wstring>& apps);
    bool IsVideoDetected() const { return m_videoDetected; }
    bool IsAnyAppPlayingAudio(const std::vector<std::wstring>& apps);

private:
    DimmerManager()
        : m_browserApps{ L"chrome.exe", L"msedge.exe", L"firefox.exe", L"opera.exe", L"brave.exe" } {}
    ~DimmerManager();

    void CreateOverlayForMonitor(ActiveMonitorInfo& info);
    static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    HINSTANCE m_hInst = nullptr;
    std::vector<ActiveMonitorInfo> m_monitors;
    bool m_showBoundaries = false;
    bool m_warmTint = false;
    bool m_focusMode = false;
    bool m_isIdleState = false;
    int m_idleDimLevel = 90;
    bool m_dimmingEnabled = false;
    bool m_classRegistered = false;

    bool m_cursorHidden = false;
    bool m_videoDetected = false;
    std::vector<std::wstring> m_blockedApps;
    std::vector<std::wstring> m_browserApps;
};
