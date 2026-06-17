#define NOMINMAX
#include "ConfigManager.h"
#include "ErrorCodes.h"
#include <fstream>
#include <sstream>
#include <locale>
#include <shlobj.h>
#include <algorithm>

static std::wstring Trim(const std::wstring& str) {
    size_t first = str.find_first_not_of(L" \t\r\n\"");
    if (first == std::wstring::npos) return L"";
    size_t last = str.find_last_not_of(L" \t\r\n\"");
    return str.substr(first, (last - first + 1));
}

static bool ParseBool(const std::wstring& val) {
    return val == L"true" || val == L"1";
}

static int ParseInt(const std::wstring& val) {
    try {
        return std::stoi(val);
    } catch (...) {
        return 0;
    }
}

std::wstring ConfigManager::GetConfigPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::wstring appDir = std::wstring(path) + L"\\IdleDimmer";
        CreateDirectoryW(appDir.c_str(), NULL);
        return appDir + L"\\dimmer.ini";
    }
    LogError(ErrorCode::E301, HRESULT_FROM_WIN32(GetLastError()));
    return L"dimmer.ini"; // Fallback to local
}

AppConfig ConfigManager::LoadConfig(const std::wstring& filePath) {
    AppConfig config;
    std::wifstream file(filePath.c_str());
    file.imbue(std::locale(""));
    if (!file.is_open()) {
        DWORD attrs = GetFileAttributesW(filePath.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            LogError(ErrorCode::E302, HRESULT_FROM_WIN32(GetLastError()));
        }
        return config; // Return defaults
    }

    std::wstringstream buffer;
    buffer << file.rdbuf();
    std::wstring content = buffer.str();
    file.close();

    if (content.empty() || content.find(L"{") == std::wstring::npos || content.find(L"}") == std::wstring::npos) {
        LogError(ErrorCode::E303);
        return config;
    }

    // Simple robust JSON scanner
    size_t pos = 0;
    auto findValue = [&](const std::wstring& key, std::wstring& outVal) -> bool {
        size_t keyPos = content.find(L"\"" + key + L"\"");
        if (keyPos == std::wstring::npos) return false;
        size_t colonPos = content.find(L":", keyPos);
        if (colonPos == std::wstring::npos) return false;
        size_t commaPos = content.find(L",", colonPos);
        size_t bracePos = content.find(L"}", colonPos);
        size_t endPos = (commaPos < bracePos) ? commaPos : bracePos;
        if (endPos == std::wstring::npos) return false;
        outVal = Trim(content.substr(colonPos + 1, endPos - colonPos - 1));
        return true;
    };

    std::wstring val;
    if (findValue(L"CloseToTray", val)) config.closeToTray = ParseBool(val);
    if (findValue(L"ShowInTaskbar", val)) config.showInTaskbar = ParseBool(val);
    if (findValue(L"StartWithWindows", val)) config.startWithWindows = ParseBool(val);
    if (findValue(L"WarmTint", val)) config.warmTint = ParseBool(val);
    if (findValue(L"IdleDimEnabled", val)) config.idleDimEnabled = ParseBool(val);
    if (findValue(L"IdleMinutes", val)) config.idleMinutes = ParseInt(val);
    if (findValue(L"IdleDimLevel", val)) config.idleDimLevel = ParseInt(val);
    if (findValue(L"IdleTurnOff", val)) config.idleTurnOff = ParseBool(val);
    if (findValue(L"MasterValue", val)) config.masterValue = ParseInt(val);
    if (findValue(L"MasterEnabled", val)) config.masterEnabled = ParseBool(val);
    if (findValue(L"LightMode", val)) config.lightMode = ParseBool(val);
    if (findValue(L"DimmingEnabled", val)) config.dimmingEnabled = ParseBool(val);
    // Parse BlockedApps array
    size_t blockedPos = content.find(L"\"BlockedApps\"");
    if (blockedPos != std::wstring::npos) {
        size_t arrStart = content.find(L"[", blockedPos);
        size_t arrEnd = content.find(L"]", blockedPos);
        if (arrStart != std::wstring::npos && arrEnd != std::wstring::npos && arrEnd > arrStart) {
            std::wstring arrContent = content.substr(arrStart + 1, arrEnd - arrStart - 1);
            size_t q = 0;
            config.blockedApps.clear();
            while ((q = arrContent.find(L'"', q)) != std::wstring::npos) {
                size_t q2 = arrContent.find(L'"', q + 1);
                if (q2 == std::wstring::npos) break;
                config.blockedApps.push_back(arrContent.substr(q + 1, q2 - q - 1));
                q = q2 + 1;
            }
        }
    }

    // Parse Monitors array
    size_t monitorsPos = content.find(L"\"Monitors\"");
    if (monitorsPos != std::wstring::npos) {
        size_t arrayStart = content.find(L"[", monitorsPos);
        size_t arrayEnd = content.find(L"]", monitorsPos);
        if (arrayStart != std::wstring::npos && arrayEnd != std::wstring::npos && arrayEnd > arrayStart) {
            std::wstring arrayContent = content.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            size_t objStart = 0;
            while ((objStart = arrayContent.find(L"{", objStart)) != std::wstring::npos) {
                size_t objEnd = arrayContent.find(L"}", objStart);
                if (objEnd == std::wstring::npos) break;
                std::wstring objStr = arrayContent.substr(objStart + 1, objEnd - objStart - 1);
                
                MonitorConfig mon;
                // Parse fields in this monitor object
                auto findField = [&](const std::wstring& key, std::wstring& outF) -> bool {
                    size_t kPos = objStr.find(L"\"" + key + L"\"");
                    if (kPos == std::wstring::npos) return false;
                    size_t cPos = objStr.find(L":", kPos);
                    if (cPos == std::wstring::npos) return false;
                    size_t commaP = objStr.find(L",", cPos);
                    size_t endP = (commaP != std::wstring::npos) ? commaP : objStr.length();
                    outF = Trim(objStr.substr(cPos + 1, endP - cPos - 1));
                    return true;
                };

                if (findField(L"Id", val)) mon.id = val;
                if (findField(L"Value", val)) mon.value = ParseInt(val);
                if (findField(L"Enabled", val)) mon.enabled = ParseBool(val);

                config.monitors.push_back(mon);
                objStart = objEnd + 1;
            }
        }
    }

    return config;
}

void ConfigManager::SaveConfig(const std::wstring& filePath, const AppConfig& config) {
    std::wofstream file(filePath.c_str());
    file.imbue(std::locale(""));
    if (!file.is_open()) {
        LogError(ErrorCode::E304, HRESULT_FROM_WIN32(GetLastError()));
        return;
    }

    file << L"{\n";
    file << L"  \"CloseToTray\": " << (config.closeToTray ? L"true" : L"false") << L",\n";
    file << L"  \"ShowInTaskbar\": " << (config.showInTaskbar ? L"true" : L"false") << L",\n";
    file << L"  \"StartWithWindows\": " << (config.startWithWindows ? L"true" : L"false") << L",\n";
    file << L"  \"WarmTint\": " << (config.warmTint ? L"true" : L"false") << L",\n";
    file << L"  \"IdleDimEnabled\": " << (config.idleDimEnabled ? L"true" : L"false") << L",\n";
    file << L"  \"IdleMinutes\": " << config.idleMinutes << L",\n";
    file << L"  \"IdleDimLevel\": " << config.idleDimLevel << L",\n";
    file << L"  \"IdleTurnOff\": " << (config.idleTurnOff ? L"true" : L"false") << L",\n";
    file << L"  \"MasterValue\": " << config.masterValue << L",\n";
    file << L"  \"MasterEnabled\": " << (config.masterEnabled ? L"true" : L"false") << L",\n";
    file << L"  \"LightMode\": " << (config.lightMode ? L"true" : L"false") << L",\n";
    file << L"  \"DimmingEnabled\": " << (config.dimmingEnabled ? L"true" : L"false") << L",\n";

    file << L"  \"BlockedApps\": [";
    for (size_t i = 0; i < config.blockedApps.size(); ++i) {
        if (i > 0) file << L", ";
        file << L"\"" << config.blockedApps[i] << L"\"";
    }
    file << L"],\n";

    file << L"  \"Monitors\": [\n";

    for (size_t i = 0; i < config.monitors.size(); ++i) {
        const auto& mon = config.monitors[i];
        file << L"    {\n";
        file << L"      \"Id\": \"" << mon.id << L"\",\n";
        file << L"      \"Value\": " << mon.value << L",\n";
        file << L"      \"Enabled\": " << (mon.enabled ? L"true" : L"false") << L"\n";
        file << L"    }" << (i + 1 < config.monitors.size() ? L"," : L"") << L"\n";
    }

    file << L"  ]\n";
    file << L"}\n";
    file.close();
}
