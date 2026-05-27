#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct MonitorConfig {
    std::wstring id;
    int value = 75; // 0 to 90 (default 75% dimming)
    bool enabled = true;
};

struct AppConfig {
    bool closeToTray = true;
    bool showInTaskbar = false;
    bool showBoundaries = false;
    bool startWithWindows = false;
    bool warmTint = false;
    bool focusMode = false;
    
    // Idle Energy & OLED Saver
    bool idleDimEnabled = true;
    int idleMinutes = 5; // 1 to 60 minutes
    int idleDimLevel = 90; // 0% to 100% dimming
    bool idleTurnOff = false;

    int masterValue = 75; // 0 to 90 (default 75% dimming)
    bool masterEnabled = true;
    bool lightMode = false;
    bool dimmingEnabled = false; // Default to false so it does NOT dim on startup!
    bool groupDim = true;
    std::vector<MonitorConfig> monitors;
};

class ConfigManager {
public:
    static AppConfig LoadConfig(const std::wstring& filePath);
    static void SaveConfig(const std::wstring& filePath, const AppConfig& config);
    static std::wstring GetConfigPath();
};
