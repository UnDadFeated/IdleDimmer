#pragma once
#include <windows.h>
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

private:
    MainWindow() = default;
    ~MainWindow();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    
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

    // Focus Mode tracking
    std::wstring m_lastActiveMonitorId;

    // Drag-drop tracking
    bool m_isDraggingAny = false;

    //     // Undo features
    RECT m_undoRect = { 0 };
    std::vector<AppConfig> m_undoStack;
    bool m_canUndo = false;
    int m_changeCount = 0;
    void PushUndoState();

    // Update checking
    bool m_updateChecked = false;
    bool m_updateAvailable = false;
    std::wstring m_latestVersion;
    static DWORD WINAPI CheckForUpdatesThread(LPVOID lpParam);
    void OnUpdateCheckComplete();
};
