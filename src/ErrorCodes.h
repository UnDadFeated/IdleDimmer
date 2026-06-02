#pragma once
#include <windows.h>
#include <string>
#include <sstream>

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
    E210 = 210, // Hotkey Ctrl+Alt+Up registration failed
    E211 = 211, // Hotkey Ctrl+Alt+Down registration failed
    E212 = 212, // Hotkey Ctrl+Alt+D registration failed
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

struct ErrorInfo {
    const wchar_t* code;
    const wchar_t* description;
};

inline ErrorInfo GetErrorInfo(ErrorCode code) {
    switch (code) {
        case ErrorCode::E101: return { L"E101", L"Single instance mutex creation failed. The operating system could not create the mutex handle." };
        case ErrorCode::E102: return { L"E102", L"Another instance of WinDimmer64 is already running." };
        case ErrorCode::E103: return { L"E103", L"COM library initialization failed. CoInitializeEx returned a failure HRESULT." };
        case ErrorCode::E104: return { L"E104", L"Setting the process DPI awareness context failed." };
        case ErrorCode::E105: return { L"E105", L"MainWindow instance creation failed. The class singleton could not be initialized." };
        case ErrorCode::E106: return { L"E106", L"Main window class registration failed (RegisterClassExW)." };
        case ErrorCode::E107: return { L"E107", L"Main window handle creation failed (CreateWindowExW)." };
        
        case ErrorCode::E201: return { L"E201", L"Direct2D factory creation failed (D2D1CreateFactory)." };
        case ErrorCode::E202: return { L"E202", L"DirectWrite factory creation failed (DWriteCreateFactory)." };
        case ErrorCode::E203: return { L"E203", L"Title text format creation failed (CreateTextFormat)." };
        case ErrorCode::E204: return { L"E204", L"Body text format creation failed (CreateTextFormat)." };
        case ErrorCode::E205: return { L"E205", L"Detail text format creation failed (CreateTextFormat)." };
        case ErrorCode::E206: return { L"E206", L"Hwnd Render Target creation failed (CreateHwndRenderTarget)." };
        case ErrorCode::E207: return { L"E207", L"Solid color brush creation failed (CreateSolidColorBrush)." };
        case ErrorCode::E208: return { L"E208", L"Focus Mode cursor tracking timer registration failed (SetTimer)." };
        case ErrorCode::E209: return { L"E209", L"System idle detection timer registration failed (SetTimer)." };
        case ErrorCode::E210: return { L"E210", L"Hotkey Ctrl+Alt+ArrowUp registration failed (RegisterHotKey)." };
        case ErrorCode::E211: return { L"E211", L"Hotkey Ctrl+Alt+ArrowDown registration failed (RegisterHotKey)." };
        case ErrorCode::E212: return { L"E212", L"Hotkey Ctrl+Alt+D registration failed (RegisterHotKey)." };
        case ErrorCode::E213: return { L"E213", L"Tray icon addition failed (Shell_NotifyIconW)." };
        case ErrorCode::E214: return { L"E214", L"RegisterClassExW failed for Add App Dialog." };
        case ErrorCode::E215: return { L"E215", L"CreateWindowExW failed for Add App Dialog." };
        
        case ErrorCode::E301: return { L"E301", L"Config file path resolution failed. Unable to query APPDATA environment variable." };
        case ErrorCode::E302: return { L"E302", L"Failed to open config file for reading. The file may not exist or is locked." };
        case ErrorCode::E303: return { L"E303", L"Hand-parsed JSON format invalid. The config file structure is corrupted." };
        case ErrorCode::E304: return { L"E304", L"Failed to open config file for writing. The file could not be created or written to." };
        case ErrorCode::E305: return { L"E305", L"Registry key opening failed for startup registry entry (RegOpenKeyExW)." };
        case ErrorCode::E306: return { L"E306", L"Registry value setting failed for startup registry entry (RegSetValueExW)." };
        case ErrorCode::E307: return { L"E307", L"Registry value deletion failed for startup registry entry (RegDeleteValueW)." };
        
        case ErrorCode::E401: return { L"E401", L"Monitor enumeration failed (EnumDisplayMonitors returned false)." };
        case ErrorCode::E402: return { L"E402", L"Overlay window class registration failed (RegisterClassExW)." };
        case ErrorCode::E403: return { L"E403", L"CreateWindowExW failed for monitor overlay window." };
        case ErrorCode::E404: return { L"E404", L"SetLayeredWindowAttributes failed for monitor overlay window." };
        case ErrorCode::E405: return { L"E405", L"OpenProcess failed for foreground process query during video check." };
        case ErrorCode::E406: return { L"E406", L"QueryFullProcessImageNameW failed for foreground process path during video check." };
        case ErrorCode::E407: return { L"E407", L"CoCreateInstance failed for MMDeviceEnumerator (audio check)." };
        case ErrorCode::E408: return { L"E408", L"GetDefaultAudioEndpoint failed for default rendering device (audio check)." };
        case ErrorCode::E409: return { L"E409", L"Device Activate failed for IAudioSessionManager2 (audio check)." };
        case ErrorCode::E410: return { L"E410", L"GetSessionEnumerator failed from session manager (audio check)." };
        case ErrorCode::E411: return { L"E411", L"GetSession failed from session enumerator (audio check)." };
        case ErrorCode::E412: return { L"E412", L"QueryInterface failed for IAudioSessionControl2 (audio check)." };
        case ErrorCode::E413: return { L"E413", L"QueryInterface failed for IAudioMeterInformation (audio check)." };
        case ErrorCode::E414: return { L"E414", L"GetPeakValue failed from audio meter (audio check)." };
        case ErrorCode::E415: return { L"E415", L"OpenProcess failed during audio session PID query." };
        case ErrorCode::E416: return { L"E416", L"QueryFullProcessImageNameW failed during audio session PID query." };
        case ErrorCode::E417: return { L"E417", L"OpenProcess failed in GetRealProcessId for UWP process resolution." };
        case ErrorCode::E418: return { L"E418", L"OpenProcess failed in GetProcessNameFromPid during video/audio check." };
        case ErrorCode::E419: return { L"E419", L"CreateWindowExW failed during setup window creation." };
        case ErrorCode::E420: return { L"E420", L"SetWindowDisplayAffinity failed for overlay window." };
        
        case ErrorCode::E501: return { L"E501", L"Resource lookup failed during setup (FindResourceW)." };
        case ErrorCode::E502: return { L"E502", L"Resource loading failed during setup (LoadResource)." };
        case ErrorCode::E503: return { L"E503", L"Resource data access failed during setup (LockResource)." };
        case ErrorCode::E504: return { L"E504", L"Destination directory creation failed during setup." };
        case ErrorCode::E505: return { L"E505", L"Executable file creation failed during setup." };
        case ErrorCode::E506: return { L"E506", L"Executable write failed during setup." };
        case ErrorCode::E507: return { L"E507", L"Setup mutex creation failed." };
        case ErrorCode::E508: return { L"E508", L"Setup mutex already exists. Another setup instance is running." };
        
        default: return { L"E000", L"Unknown error code." };
    }
}

inline ErrorCode g_lastAppError = ErrorCode::SUCCESS;

inline ErrorCode GetLastAppError() {
    return g_lastAppError;
}

inline void SetLastAppError(ErrorCode code) {
    g_lastAppError = code;
}

inline void LogError(ErrorCode code, HRESULT hr = S_OK) {
    SetLastAppError(code);
    ErrorInfo info = GetErrorInfo(code);
    std::wstringstream wss;
    wss << L"[WinDimmer64 Error Code]: " << info.code << L" - " << info.description;
    if (hr != S_OK) {
        wss << L" (System HRESULT/Code: 0x" << std::hex << hr << L")";
    }
    wss << L"\n";
    OutputDebugStringW(wss.str().c_str());
}
