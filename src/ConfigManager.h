#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct MonitorConfig {
    std::wstring id;
    int value = 0; // 0 to 90 (representing opacity percentage)
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
    bool idleDimEnabled = false;
    int idleMinutes = 5; // 1 to 60 minutes
    int idleDimLevel = 90; // 0% to 100% dimming
    bool idleTurnOff = false;

    int masterValue = 30; // 0 to 90
    bool masterEnabled = true;
    std::vector<MonitorConfig> monitors;
};

class ConfigManager {
public:
    static AppConfig LoadConfig(const std::wstring& filePath);
    static void SaveConfig(const std::wstring& filePath, const AppConfig& config);
    static std::wstring GetConfigPath();
};
