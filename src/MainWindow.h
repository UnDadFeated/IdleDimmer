#pragma once
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <string>
#include "ConfigManager.h"

// Struct for UI slider interactions
struct UISlider {
    std::wstring monitorId;
    bool isMaster = false;
    bool isIdleMinutes = false;
    bool isIdleDimLevel = false;
    bool isScheduleStart = false;   // v1.6.5 (Todo 8): minutes-of-day slider
    bool isScheduleEnd   = false;   // v1.6.5 (Todo 8): minutes-of-day slider
    RECT rect;
    float value = 0.0f; // 0.0 to 1.0
    bool active = true;
    bool isDragging = false;
    bool isHovered = false;
};

struct UICheckbox {
    std::wstring monitorId;
    std::wstring settingName; // "CloseToTray", "ShowInTaskbar", etc.
    RECT rect;
    bool checked = false;
    bool isHovered = false;
    bool* pValue = nullptr;
    std::wstring label;
};

class MainWindow {
public:
    static MainWindow& Instance() {
        static MainWindow instance;
        return instance;
    }

    bool Create(HINSTANCE hInst, int nCmdShow);
    void Show(bool show);
    HWND GetHWND() const { return m_hwnd; }
    UINT GetWindowDpi() const;
    float GetDpiScale() const;

private:
    MainWindow() = default;
    ~MainWindow();

    bool CreateImpl(HINSTANCE hInst, int nCmdShow);
    static bool CreateImplSafe(MainWindow* self, HINSTANCE hInst, int nCmdShow);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT WndProcImpl(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    
    // Direct2D Graphics
    HRESULT CreateGraphicsResources();
    void DiscardGraphicsResources();
    void OnPaint();
    void OnResize(UINT width, UINT height);
    
    // UI Helpers & Layout
    void UpdateLayout();
    void HandleMouseMove(int x, int y);
    void HandleLButtonDown(int x, int y);
    void HandleLButtonUp(int x, int y);
    void HandleMouseWheel(short delta, int x, int y);
    void HandleKeyDown(WPARAM key);
    
    // Config and auto-run
    void LoadSettings();
    void SaveSettings();
    void ToggleStartWithWindows(bool enable);
    void SyncMonitorsWithConfig();

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInst = nullptr;

    // Direct2D Interfaces
    ID2D1Factory* m_pFactory = nullptr;
    ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;
    IDWriteFactory* m_pDWriteFactory = nullptr;
    IDWriteTextFormat* m_pTextFormatTitle = nullptr;
    IDWriteTextFormat* m_pTextFormatBody = nullptr;
    IDWriteTextFormat* m_pTextFormatDetail = nullptr;

    // Brushes
    ID2D1SolidColorBrush* m_pBrushBg = nullptr;
    ID2D1SolidColorBrush* m_pBrushCard = nullptr;
    ID2D1SolidColorBrush* m_pBrushCardBorder = nullptr;
    ID2D1SolidColorBrush* m_pBrushText = nullptr;
    ID2D1SolidColorBrush* m_pBrushTextMuted = nullptr;
    ID2D1SolidColorBrush* m_pBrushAccent = nullptr;
    ID2D1SolidColorBrush* m_pBrushAccentHover = nullptr;
    ID2D1SolidColorBrush* m_pBrushTrack = nullptr;

    // UI Interactive components
    std::vector<UISlider> m_sliders;
    std::vector<UICheckbox> m_checkboxes;
    int m_windowWidth = 480;
    int m_windowHeight = 520;
    AppConfig m_config;

    // Current app version read from resource
    std::wstring m_appVersion;

    // Drag-drop tracking
    bool m_isDraggingAny = false;

    //     // Undo features
    RECT m_undoRect = {};
    std::vector<AppConfig> m_undoStack;
    bool m_canUndo = false;
    int m_changeCount = 0;
    void PushUndoState();

    // Update checking
    bool m_updateChecked = false;
    bool m_updateChecking = false;
    bool m_updateAvailable = false;
    std::wstring m_latestVersion;
    HANDLE m_hUpdateThread = nullptr;
    HINTERNET m_hSession = nullptr;
    RECT m_updateCheckRect = {};
    static DWORD WINAPI CheckForUpdatesThread(LPVOID lpParam);
    void OnUpdateCheckComplete(WPARAM wp, LPARAM lp);
    bool IsPackaged() const;

    // Safety-net state for certification startup resilience
    bool m_d2dReady = false;    // D2D successfully initialized at least once
    bool m_d2dFailed = false;   // D2D permanently failed (SEH crash recovery)
    bool m_trayAdded = false;   // Shell_NotifyIcon succeeded
    bool m_isSessionEnding = false; // System session is ending or package update requested close

    static const int CONTENT_WIDTH = 430;
    void Repaint() { InvalidateRect(m_hwnd, nullptr, FALSE); }
};
