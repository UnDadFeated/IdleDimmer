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
    bool startWithWindows = false;
    bool warmTint = false;
    
    // Idle Energy & OLED Saver
    bool idleDimEnabled = true;
    int idleMinutes = 5; // 1 to 60 minutes
    int idleDimLevel = 90; // 0% to 100% dimming
    bool idleTurnOff = false;

    int masterValue = 75; // 0 to 90 (default 75% dimming)
    bool masterEnabled = true;
    bool lightMode = false;
    bool dimmingEnabled = false; // Default to false so it does NOT dim on startup!
    std::vector<MonitorConfig> monitors;
    std::vector<std::wstring> blockedApps;
    AppConfig() {
        blockedApps = {
            L"vlc.exe", L"mpc-hc.exe", L"mpc-hc64.exe", L"mpc-be.exe", L"mpc-be64.exe",
            L"potplayer.exe", L"wmplayer.exe", L"Plex.exe", L"PlexScriptHost.exe",
            L"kodi.exe", L"mpv.exe", L"mpv.net.exe", L"netflix.exe", L"screenbox.exe",
            L"kmplayer.exe", L"kmp.exe", L"gom.exe", L"smplayer.exe"
        };
    }
};

class ConfigManager {
public:
    static AppConfig LoadConfig(const std::wstring& filePath);
    static void SaveConfig(const std::wstring& filePath, const AppConfig& config);
    static std::wstring GetConfigPath();
};
