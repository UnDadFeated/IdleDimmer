#pragma once
#include <windows.h>
#include <string>
#include <format>
#include <source_location>

enum class ErrorCode {
    SUCCESS = 0,
    
    // Area 1: Initialization & Entry (E100 - E199)
    E101 = 101, // Single instance mutex creation failed
    E102 = 102, // Another instance of the application is already running
    E103 = 103, // COM library initialization failed
    E104 = 104, // DPI awareness context setting failed
    E105 = 105, // MainWindow instance creation failed
    E106 = 106, // Main window class registration failed
    E107 = 107, // Main window handle creation failed
    E108 = 108, // MainWindow initialization crash / structured exception
    
    // Area 2: Settings Window & GUI (E200 - E299)
    E201 = 201, // Direct2D factory creation failed
    E202 = 202, // DirectWrite factory creation failed
    E203 = 203, // Title text format creation failed
    E204 = 204, // Body text format creation failed
    E205 = 205, // Detail text format creation failed
    E206 = 206, // Hwnd Render Target creation failed
    E207 = 207, // Solid color brush creation failed
    E208 = 208, // Focus Mode cursor tracking timer registration failed
    E209 = 209, // System idle detection timer registration failed
    E213 = 213, // Tray icon addition failed
    E214 = 214, // RegisterClassExW failed for Add App Dialog
    E215 = 215, // CreateWindowExW failed for Add App Dialog
    
    // Area 3: Config Management (E300 - E399)
    E301 = 301, // Config file path resolution failed
    E302 = 302, // Failed to open config file for reading
    E303 = 303, // Hand-parsed JSON format invalid/config corrupted
    E304 = 304, // Failed to open config file for writing
    E305 = 305, // Registry key opening failed for startup registry entry
    E306 = 306, // Registry value setting failed for startup registry entry
    E307 = 307, // Registry value deletion failed for startup registry entry
    
    // Area 4: Monitor Overlay & Dimming (E400 - E499)
    E401 = 401, // Monitor enumeration failed (EnumDisplayMonitors)
    E402 = 402, // Overlay window class registration failed
    E403 = 403, // CreateWindowExW failed for monitor overlay
    E404 = 404, // SetLayeredWindowAttributes failed for monitor overlay
    E405 = 405, // OpenProcess failed for foreground process query (video check)
    E406 = 406, // QueryFullProcessImageNameW failed for foreground process path (video check)
    E407 = 407, // CoCreateInstance failed for MMDeviceEnumerator (audio check)
    E408 = 408, // GetDefaultAudioEndpoint failed (audio check)
    E409 = 409, // Device Activate failed for IAudioSessionManager2 (audio check)
    E410 = 410, // GetSessionEnumerator failed (audio check)
    E411 = 411, // GetSession failed (audio check)
    E412 = 412, // QueryInterface failed for IAudioSessionControl2 (audio check)
    E413 = 413, // QueryInterface failed for IAudioMeterInformation (audio check)
    E414 = 414, // GetPeakValue failed (audio check)
    E415 = 415, // OpenProcess failed during audio session PID query
    E416 = 416, // QueryFullProcessImageNameW failed during audio session PID query
    
    // Area 4 continued: Overlay & Dimming (E417+)
    E417 = 417, // OpenProcess failed in GetRealProcessId for UWP process resolution
    E418 = 418, // OpenProcess failed in GetProcessNameFromPid for video/audio check
    E419 = 419, // CreateWindowExW failed in setup
    E420 = 420, // SetWindowDisplayAffinity failed for overlay
    
    // Area 5: Setup & Installer (E500 - E599)
    E501 = 501, // Resource lookup failed (FindResourceW)
    E502 = 502, // Resource loading failed (LoadResource)
    E503 = 503, // Resource data access failed (LockResource)
    E504 = 504, // Destination directory creation failed
    E505 = 505, // Executable file creation failed
    E506 = 506, // Executable write failed
    E507 = 507, // Setup mutex creation failed
    E508 = 508  // Setup mutex already exists
};

inline std::wstring g_lastAppErrorStr = L"SUCCESS";

inline const wchar_t* GetErrorCodeName(ErrorCode code) {
    switch (code) {
        case ErrorCode::E101: return L"E101";
        case ErrorCode::E102: return L"E102";
        case ErrorCode::E103: return L"E103";
        case ErrorCode::E104: return L"E104";
        case ErrorCode::E105: return L"E105";
        case ErrorCode::E106: return L"E106";
        case ErrorCode::E107: return L"E107";
        case ErrorCode::E108: return L"E108";
        case ErrorCode::E201: return L"E201";
        case ErrorCode::E202: return L"E202";
        case ErrorCode::E203: return L"E203";
        case ErrorCode::E204: return L"E204";
        case ErrorCode::E205: return L"E205";
        case ErrorCode::E206: return L"E206";
        case ErrorCode::E207: return L"E207";
        case ErrorCode::E208: return L"E208";
        case ErrorCode::E209: return L"E209";
        case ErrorCode::E213: return L"E213";
        case ErrorCode::E214: return L"E214";
        case ErrorCode::E215: return L"E215";
        case ErrorCode::E301: return L"E301";
        case ErrorCode::E302: return L"E302";
        case ErrorCode::E303: return L"E303";
        case ErrorCode::E304: return L"E304";
        case ErrorCode::E305: return L"E305";
        case ErrorCode::E306: return L"E306";
        case ErrorCode::E307: return L"E307";
        case ErrorCode::E401: return L"E401";
        case ErrorCode::E402: return L"E402";
        case ErrorCode::E403: return L"E403";
        case ErrorCode::E404: return L"E404";
        case ErrorCode::E405: return L"E405";
        case ErrorCode::E406: return L"E406";
        case ErrorCode::E407: return L"E407";
        case ErrorCode::E408: return L"E408";
        case ErrorCode::E409: return L"E409";
        case ErrorCode::E410: return L"E410";
        case ErrorCode::E411: return L"E411";
        case ErrorCode::E412: return L"E412";
        case ErrorCode::E413: return L"E413";
        case ErrorCode::E414: return L"E414";
        case ErrorCode::E415: return L"E415";
        case ErrorCode::E416: return L"E416";
        case ErrorCode::E417: return L"E417";
        case ErrorCode::E418: return L"E418";
        case ErrorCode::E419: return L"E419";
        case ErrorCode::E420: return L"E420";
        case ErrorCode::E501: return L"E501";
        case ErrorCode::E502: return L"E502";
        case ErrorCode::E503: return L"E503";
        case ErrorCode::E504: return L"E504";
        case ErrorCode::E505: return L"E505";
        case ErrorCode::E506: return L"E506";
        case ErrorCode::E507: return L"E507";
        case ErrorCode::E508: return L"E508";
        default: return L"E000";
    }
}

inline const wchar_t* GetErrorDescription(ErrorCode code) {
    switch (code) {
        case ErrorCode::E101: return L"Single instance mutex creation failed.";
        case ErrorCode::E102: return L"Another instance of IdleDimmer is already running.";
        case ErrorCode::E103: return L"COM library initialization failed.";
        case ErrorCode::E104: return L"Setting the process DPI awareness context failed.";
        case ErrorCode::E105: return L"MainWindow instance creation failed.";
        case ErrorCode::E106: return L"Main window class registration failed.";
        case ErrorCode::E107: return L"Main window handle creation failed.";
        case ErrorCode::E108: return L"MainWindow initialization crash.";
        case ErrorCode::E201: return L"Direct2D factory creation failed.";
        case ErrorCode::E202: return L"DirectWrite factory creation failed.";
        case ErrorCode::E203: return L"Title text format creation failed.";
        case ErrorCode::E204: return L"Body text format creation failed.";
        case ErrorCode::E205: return L"Detail text format creation failed.";
        case ErrorCode::E206: return L"Hwnd Render Target creation failed.";
        case ErrorCode::E207: return L"Solid color brush creation failed.";
        case ErrorCode::E208: return L"Focus Mode cursor tracking timer registration failed.";
        case ErrorCode::E209: return L"System idle detection timer registration failed.";
        case ErrorCode::E213: return L"Tray icon addition failed.";
        case ErrorCode::E214: return L"RegisterClassExW failed for Add App Dialog.";
        case ErrorCode::E215: return L"CreateWindowExW failed for Add App Dialog.";
        case ErrorCode::E301: return L"Config file path resolution failed.";
        case ErrorCode::E302: return L"Failed to open config file for reading.";
        case ErrorCode::E303: return L"Hand-parsed JSON format invalid.";
        case ErrorCode::E304: return L"Failed to open config file for writing.";
        case ErrorCode::E305: return L"Registry key opening failed.";
        case ErrorCode::E306: return L"Registry value setting failed.";
        case ErrorCode::E307: return L"Registry value deletion failed.";
        case ErrorCode::E401: return L"Monitor enumeration failed.";
        case ErrorCode::E402: return L"Overlay window class registration failed.";
        case ErrorCode::E403: return L"CreateWindowExW failed for monitor overlay.";
        case ErrorCode::E404: return L"SetLayeredWindowAttributes failed for monitor overlay.";
        case ErrorCode::E405: return L"OpenProcess failed for foreground process query.";
        case ErrorCode::E406: return L"QueryFullProcessImageNameW failed.";
        case ErrorCode::E407: return L"CoCreateInstance failed for MMDeviceEnumerator.";
        case ErrorCode::E408: return L"GetDefaultAudioEndpoint failed.";
        case ErrorCode::E409: return L"Device Activate failed.";
        case ErrorCode::E410: return L"GetSessionEnumerator failed.";
        case ErrorCode::E411: return L"GetSession failed.";
        case ErrorCode::E412: return L"QueryInterface failed for IAudioSessionControl2.";
        case ErrorCode::E413: return L"QueryInterface failed for IAudioMeterInformation.";
        case ErrorCode::E414: return L"GetPeakValue failed.";
        case ErrorCode::E415: return L"OpenProcess failed during audio session PID query.";
        case ErrorCode::E416: return L"QueryFullProcessImageNameW failed during audio session.";
        case ErrorCode::E417: return L"OpenProcess failed in GetRealProcessId.";
        case ErrorCode::E418: return L"OpenProcess failed in GetProcessNameFromPid.";
        case ErrorCode::E419: return L"CreateWindowExW failed during setup.";
        case ErrorCode::E420: return L"SetWindowDisplayAffinity failed for overlay.";
        case ErrorCode::E501: return L"Resource lookup failed.";
        case ErrorCode::E502: return L"Resource loading failed.";
        case ErrorCode::E503: return L"Resource data access failed.";
        case ErrorCode::E504: return L"Destination directory creation failed.";
        case ErrorCode::E505: return L"Executable file creation failed.";
        case ErrorCode::E506: return L"Executable write failed.";
        case ErrorCode::E507: return L"Setup mutex creation failed.";
        case ErrorCode::E508: return L"Setup mutex already exists.";
        default: return L"Unknown error.";
    }
}

inline std::wstring g_lastAppError = L"SUCCESS";

inline std::wstring GetLastAppError() {
    return g_lastAppError;
}

inline void SetLastAppError(std::wstring_view err) {
    g_lastAppError = std::wstring(err);
}

inline std::wstring Utf8ToWide(const char* utf8) {
    if (!utf8) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &result[0], len);
    if (!result.empty()) result.pop_back();
    return result;
}

inline void LogError(ErrorCode code, HRESULT hr = S_OK,
                     const std::source_location& loc = std::source_location::current()) {
    SetLastAppError(GetErrorDescription(code));
    std::wstring msg;
    std::wstring fileName = Utf8ToWide(loc.file_name());
    std::wstring codeName = GetErrorCodeName(code);
    if (hr != S_OK) {
        msg = std::format(L"[{}] {}:{} {} (0x{:#010X})",
                           fileName, loc.line(), codeName, GetErrorDescription(code), static_cast<unsigned>(hr));
    } else {
        msg = std::format(L"[{}] {}:{} {}",
                           fileName, loc.line(), codeName, GetErrorDescription(code));
    }
    OutputDebugStringW(msg.c_str());
}
