#pragma once
#define NOMINMAX
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
    bool startWithWindows = false;
    
    // Idle Energy & OLED Saver
    bool idleDimEnabled = true;
    int idleMinutes = 5; // 1 to 60 minutes
    int idleDimLevel = 90; // 0% to 90% dimming (capped for screen visibility safety)

    int masterValue = 75; // 0 to 90 (default 75% dimming)
    bool masterEnabled = true;
    bool lightMode = false;
    bool dimmingEnabled = false; // Default to false so it does NOT dim on startup!
    std::vector<MonitorConfig> monitors;

    // ── Time-of-Day Scheduling (v1.6.5) ──
    // All times are stored as minutes-of-day (0..1439).
    // Default schedule: 22:00 (1320) to 07:00 (420) — overnight.
    bool scheduleEnabled = false;
    int  scheduleStartMins = 1320; // 22:00
    int  scheduleEndMins   = 420;  // 07:00
    // Dim level applied during scheduled period. Range 0..90 (matches master scale).
    int  scheduleDimLevel  = 60;

    AppConfig() {
    }
};

class ConfigManager {
public:
    static AppConfig LoadConfig(const std::wstring& filePath);
    static void SaveConfig(const std::wstring& filePath, const AppConfig& config);
    static std::wstring GetConfigPath();
};
