#define WIN32_LEAN_AND_MEAN
#define _WIN32_IE 0x0600
#define UNICODE
#define NOMINMAX
#include <windows.h>
#include <wchar.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <shellapi.h>
#include <objbase.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#include <winver.h>
#include <wrl/client.h>
#include <format>
#include "ErrorCodes.h"
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using Microsoft::WRL::ComPtr;

#define IDI_APP 101
#define IDR_APP_BIN 102

static const wchar_t* APP_NAME = L"IdleDimmer";
static const wchar_t* INSTALL_DIR = L"IdleDimmer";
static const wchar_t* REG_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\IdleDimmer";

enum State { READY, INSTALLING, COMPLETE };
static State g_state = READY;
static wchar_t g_installPath[MAX_PATH];
static HWND g_hStatus, g_hButton, g_hLaunchCheck;
static HFONT g_hFontTitle, g_hFontBody;

static void Log(const wchar_t* msg) {
    (void)msg;
}

static void GetInstallPath(wchar_t* buf, DWORD size) {
    GetEnvironmentVariableW(L"LOCALAPPDATA", buf, size);
    wcscat_s(buf, size, L"\\");
    wcscat_s(buf, size, INSTALL_DIR);
}

static bool IsRunning() {
    return FindWindowW(L"IdleDimmerMainClass", NULL) != NULL;
}

static std::wstring GetExeVersion(const wchar_t* filepath) {
    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeW(filepath, &dummy);
    if (size == 0) return L"unknown";
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(filepath, 0, size, data.data())) return L"unknown";
    VS_FIXEDFILEINFO* pFileInfo = nullptr;
    UINT len = 0;
    if (VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&pFileInfo), &len) && len > 0 && pFileInfo) {
        return std::format(L"{}.{}.{}",
            static_cast<int>(HIWORD(pFileInfo->dwProductVersionMS)),
            static_cast<int>(LOWORD(pFileInfo->dwProductVersionMS)),
            static_cast<int>(HIWORD(pFileInfo->dwProductVersionLS)));
    }
    return L"unknown";
}

static bool ExtractApp(const wchar_t* dest) {
    Log(std::format(L"  Extracting {}...\r\n", dest).c_str());
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_APP_BIN), MAKEINTRESOURCEW(10));
    if (!hRes) {
        LogError(ErrorCode::E501, HRESULT_FROM_WIN32(GetLastError()));
        Log(std::format(L"  [FAILED] FindResourceW error {}\r\n", GetLastError()).c_str());
        return false;
    }
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) {
        LogError(ErrorCode::E502, HRESULT_FROM_WIN32(GetLastError()));
        Log(std::format(L"  [FAILED] LoadResource error {}\r\n", GetLastError()).c_str());
        return false;
    }
    DWORD size = SizeofResource(NULL, hRes);
    void* data = LockResource(hData);
    if (!data || size == 0) {
        LogError(ErrorCode::E503, HRESULT_FROM_WIN32(GetLastError()));
        Log(L"  [FAILED] LockResource\r\n");
        return false;
    }
    Log(std::format(L"  Resource size: {} bytes\r\n", size).c_str());
    HANDLE hFile = CreateFileW(dest, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LogError(ErrorCode::E505, HRESULT_FROM_WIN32(GetLastError()));
        Log(std::format(L"  [FAILED] CreateFileW error {}\r\n", GetLastError()).c_str());
        return false;
    }
    DWORD written;
    BOOL ok = WriteFile(hFile, data, size, &written, NULL);
    CloseHandle(hFile);
    if (!ok || written != size) {
        LogError(ErrorCode::E506, HRESULT_FROM_WIN32(GetLastError()));
        Log(std::format(L"  [FAILED] WriteFile wrote {} of {} bytes\r\n", written, size).c_str());
        DeleteFileW(dest);
        return false;
    }
    Log(std::format(L"  Extracted {} bytes\r\n", written).c_str());
    return true;
}

static void KillRunning() {
    HWND hwnd = FindWindowW(L"IdleDimmerMainClass", NULL);
    if (!hwnd) {
        Log(L"  No running instance found\r\n");
        return;
    }
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    Log(std::format(L"  Found running instance (PID {}), sending WM_CLOSE...\r\n", pid).c_str());
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    HANDLE hProc = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid);
    if (hProc) {
        DWORD waitResult = WaitForSingleObject(hProc, 5000);
        if (waitResult == WAIT_OBJECT_0)
            Log(L"  Process exited gracefully\r\n");
        else if (waitResult == WAIT_TIMEOUT) {
            Log(L"  Timeout, force-terminating...\r\n");
            TerminateProcess(hProc, 1);
        } else
            Log(std::format(L"  Wait returned {}\r\n", waitResult).c_str());
        CloseHandle(hProc);
        Sleep(200);
    }
}

static void CreateShortcut(const wchar_t* target) {
    Log(L"  Creating Start Menu shortcut...\r\n");
    wchar_t path[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_STARTMENU, nullptr, 0, path))) {
        wcscat_s(path, MAX_PATH, L"\\Programs\\IdleDimmer.lnk");
        ComPtr<IShellLinkW> psl;
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl)))) {
            psl->SetPath(target);
            psl->SetDescription(L"Monitor dimmer with per-monitor controls, hotkeys, and idle detection");
            ComPtr<IPersistFile> ppf;
            if (SUCCEEDED(psl.As(&ppf))) {
                ppf->Save(path, TRUE);
                Log(L"  Shortcut saved\r\n");
            }
        }
    } else {
        Log(L"  [WARNING] Could not retrieve Start Menu folder path\r\n");
    }
}

static void RegisterUninstall() {
    Log(L"  Registering uninstall entry...\r\n");
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        Log(L"  [FAILED] RegCreateKeyExW\r\n");
        return;
    }
    wchar_t displayName[64], exePath[MAX_PATH];
    wcscpy_s(displayName, ARRAYSIZE(displayName), APP_NAME);
    wcscpy_s(exePath, ARRAYSIZE(exePath), std::format(L"{}\\{}.exe", g_installPath, APP_NAME).c_str());
    std::wstring uninstallCmd = std::format(L"\"{}\\uninstall.exe\" /uninstall", g_installPath);
    RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (BYTE*)displayName, (DWORD)((wcslen(displayName) + 1) * sizeof(wchar_t)));
    RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, (BYTE*)exePath, (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t)));
    RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (BYTE*)uninstallCmd.c_str(), (DWORD)((uninstallCmd.length() + 1) * sizeof(wchar_t)));
    DWORD dummy = 1;
    RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (BYTE*)&dummy, sizeof(dummy));
    RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (BYTE*)&dummy, sizeof(dummy));
    RegCloseKey(hKey);
    Log(L"  Uninstall registered\r\n");
}

static void SelfDelete() {
    wchar_t installPath[MAX_PATH];
    GetInstallPath(installPath, MAX_PATH);
    wchar_t uninstallExePath[MAX_PATH];
    swprintf_s(uninstallExePath, ARRAYSIZE(uninstallExePath), L"%s\\uninstall.exe", installPath);

    wchar_t cmdLine[MAX_PATH * 3];
    swprintf_s(cmdLine, ARRAYSIZE(cmdLine),
        L"cmd.exe /c ping 127.0.0.1 -n 2 > nul & del /f /q \"%s\" & rd /s /q \"%s\"",
        uninstallExePath, installPath);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

static void Uninstall() {
    GetInstallPath(g_installPath, MAX_PATH);
    Log(L"\r\n=== IdleDimmer Uninstall ===\r\n\r\n");
    Log(std::format(L"  Install path: {}\r\n", g_installPath).c_str());
    KillRunning();
    wchar_t exePath[MAX_PATH];
    wcscpy_s(exePath, ARRAYSIZE(exePath), std::format(L"{}\\{}.exe", g_installPath, APP_NAME).c_str());
    DeleteFileW(exePath);
    Log(std::format(L"  Removed {}\r\n", exePath).c_str());
    RemoveDirectoryW(g_installPath);
    
    wchar_t configDir[MAX_PATH];
    GetEnvironmentVariableW(L"APPDATA", configDir, MAX_PATH);
    wcscat_s(configDir, MAX_PATH, L"\\IdleDimmer");
    
    wchar_t iniPath[MAX_PATH];
    wcscpy_s(iniPath, ARRAYSIZE(iniPath), std::format(L"{}\\dimmer.ini", configDir).c_str());
    DeleteFileW(iniPath);

    wchar_t startupLogPath[MAX_PATH];
    wcscpy_s(startupLogPath, ARRAYSIZE(startupLogPath), std::format(L"{}\\startup.log", configDir).c_str());
    DeleteFileW(startupLogPath);

    wchar_t crashLogPath[MAX_PATH];
    wcscpy_s(crashLogPath, ARRAYSIZE(crashLogPath), std::format(L"{}\\crash.log", configDir).c_str());
    DeleteFileW(crashLogPath);

    wchar_t dimmerLogPath[MAX_PATH];
    wcscpy_s(dimmerLogPath, ARRAYSIZE(dimmerLogPath), std::format(L"{}\\dimmer.log", configDir).c_str());
    DeleteFileW(dimmerLogPath);
    wcscpy_s(dimmerLogPath, ARRAYSIZE(dimmerLogPath), std::format(L"{}\\dimmer_0.log", configDir).c_str());
    DeleteFileW(dimmerLogPath);
    wcscpy_s(dimmerLogPath, ARRAYSIZE(dimmerLogPath), std::format(L"{}\\dimmer_1.log", configDir).c_str());
    DeleteFileW(dimmerLogPath);
    wcscpy_s(dimmerLogPath, ARRAYSIZE(dimmerLogPath), std::format(L"{}\\dimmer_2.log", configDir).c_str());
    DeleteFileW(dimmerLogPath);

    RemoveDirectoryW(configDir);
    Log(L"  Removed config and logs\r\n");

    wchar_t shortcut[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_STARTMENU, NULL, 0, shortcut))) {
        wcscat_s(shortcut, MAX_PATH, L"\\Programs\\IdleDimmer.lnk");
        DeleteFileW(shortcut);
        Log(L"  Removed Start Menu shortcut\r\n");
    }
    RegDeleteKeyW(HKEY_CURRENT_USER, REG_PATH);
    Log(L"  Removed registry settings\r\n");
    Log(L"  Uninstall complete\r\n");
}

static LRESULT CALLBACK SetupWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCTW)lParam)->hInstance;
            RECT rc;
            GetClientRect(hwnd, &rc);
            int cw = rc.right;
            int ch = rc.bottom;
            int m = 24;

            // Apply dark mode for title bar dynamically to allow running on environments without dwmapi.dll (e.g. WinPE)
            HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
            if (hDwm) {
                typedef HRESULT (WINAPI *DwmSetWindowAttributePtr)(HWND, DWORD, LPCVOID, DWORD);
                DwmSetWindowAttributePtr fnDwmSetWindowAttribute = (DwmSetWindowAttributePtr)GetProcAddress(hDwm, "DwmSetWindowAttribute");
                if (fnDwmSetWindowAttribute) {
                    BOOL dark = TRUE;
                    fnDwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
                }
                FreeLibrary(hDwm);
            }

            // Get setup exe's version dynamically from PE resources
            wchar_t setupPath[MAX_PATH];
            GetModuleFileNameW(NULL, setupPath, MAX_PATH);
            std::wstring setupVer = GetExeVersion(setupPath);

            GetInstallPath(g_installPath, MAX_PATH);
            wchar_t exePath[MAX_PATH];
            wcscpy_s(exePath, ARRAYSIZE(exePath), std::format(L"{}\\{}.exe", g_installPath, APP_NAME).c_str());

            bool running = IsRunning();
            bool installed = GetFileAttributesW(exePath) != INVALID_FILE_ATTRIBUTES;
            std::wstring installedVer;
            if (installed) installedVer = GetExeVersion(exePath);

            // Icon static
            CreateWindowW(L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_ICON,
                m, 32, 64, 64, hwnd, (HMENU)1001, hInst, NULL);
            SendMessageW(GetDlgItem(hwnd, 1001), STM_SETIMAGE, IMAGE_ICON, 
                (LPARAM)LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP)));

            // App name static
            int appNameX = m + 64 + 16;
            HWND hAppName = CreateWindowW(L"STATIC", APP_NAME,
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                appNameX, 36, cw - appNameX - m, 24, hwnd, (HMENU)1002, hInst, NULL);

            // Publisher static
            HWND hPublisher = CreateWindowW(L"STATIC", L"UnDadFeated",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                appNameX, 62, cw - appNameX - m, 18, hwnd, (HMENU)1003, hInst, NULL);

            // Version static
            HWND hVersion = CreateWindowW(L"STATIC", std::format(L"Version {}", setupVer).c_str(),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                appNameX, 82, cw - appNameX - m, 18, hwnd, (HMENU)1004, hInst, NULL);

            // Status static
            g_hStatus = CreateWindowW(L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                m, 102, cw - m * 2, 36, hwnd, (HMENU)1005, hInst, NULL);

            wchar_t status[256];
            if (running && installed)
                wcscpy_s(status, ARRAYSIZE(status), std::format(L"Running: v{} | Installed: v{}\r\nSetup will terminate and overwrite.", installedVer, installedVer).c_str());
            else if (running)
                wcscpy_s(status, ARRAYSIZE(status), std::format(L"Running: v{}\r\nSetup will terminate and install.", installedVer.empty() ? L"unknown" : installedVer).c_str());
            else if (installed)
                wcscpy_s(status, ARRAYSIZE(status), std::format(L"Installed: v{}\r\nSetup will overwrite.", installedVer).c_str());
            else
                wcscpy_s(status, ARRAYSIZE(status), std::format(L"Ready to install v{}", setupVer).c_str());
            SetWindowTextW(g_hStatus, status);

            // Launch checkbox
            g_hLaunchCheck = CreateWindowW(L"BUTTON", L"Launch after install",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_LEFT,
                m, 140, cw - m * 2, 18, hwnd, (HMENU)1006, hInst, NULL);
            SendMessageW(g_hLaunchCheck, BM_SETCHECK, BST_CHECKED, 0);

            // Install/Close button
            int buttonX = cw - 120 - m;
            g_hButton = CreateWindowW(L"BUTTON", L"Install",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                buttonX, 164, 120, 30, hwnd, (HMENU)IDOK, hInst, NULL);

            g_hFontTitle = CreateFontW(20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Display");
            g_hFontBody = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");

            SendMessageW(hAppName, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);
            SendMessageW(hPublisher, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(hVersion, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hLaunchCheck, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);

            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hwndCtl = (HWND)lParam;
            // Gray color for publisher and version statics
            if (hwndCtl == GetDlgItem(hwnd, 1003) || hwndCtl == GetDlgItem(hwnd, 1004)) {
                SetTextColor(hdc, RGB(120, 120, 120));
                SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
                return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
            }
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                if (g_state == READY) {
                    g_state = INSTALLING;
                    SetWindowTextW(g_hButton, L"Installing...");
                    EnableWindow(g_hButton, FALSE);
                    SetWindowTextW(g_hStatus, L"Installing...");

                    KillRunning();

                    wchar_t exePath[MAX_PATH];
                    wcscpy_s(exePath, ARRAYSIZE(exePath), std::format(L"{}\\{}.exe", g_installPath, APP_NAME).c_str());

                    if (ExtractApp(exePath)) {
                        // Copy setup.exe to uninstall.exe
                        wchar_t setupPath[MAX_PATH];
                        GetModuleFileNameW(NULL, setupPath, MAX_PATH);
                        wchar_t uninstallExePath[MAX_PATH];
                        swprintf_s(uninstallExePath, ARRAYSIZE(uninstallExePath), L"%s\\uninstall.exe", g_installPath);
                        if (CopyFileW(setupPath, uninstallExePath, FALSE)) {
                        } else {
                        }
                        CreateShortcut(exePath);
                        RegisterUninstall();
                        SetWindowTextW(g_hStatus, L"Installation complete!");
                        SetWindowTextW(g_hButton, L"Close");
                        EnableWindow(g_hButton, TRUE);
                        g_state = COMPLETE;
                    } else {
                        SetWindowTextW(g_hStatus, L"Extraction failed.\r\nTry running as Administrator.");
                        SetWindowTextW(g_hButton, L"Close");
                        EnableWindow(g_hButton, TRUE);
                        g_state = COMPLETE;
                    }
                } else if (g_state == COMPLETE) {
                    if (SendMessageW(g_hLaunchCheck, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                        wchar_t exePath[MAX_PATH];
                        wcscpy_s(exePath, ARRAYSIZE(exePath), std::format(L"{}\\{}.exe", g_installPath, APP_NAME).c_str());
                        Sleep(300);
                        ShellExecuteW(nullptr, L"open", exePath, nullptr, nullptr, SW_SHOWNORMAL);
                    }
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                }
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            DeleteObject(g_hFontTitle);
            DeleteObject(g_hFontBody);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR lpCmdLine, int) {
    (void)lpCmdLine;
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\IdleDimmerSetupMutex");
    if (hMutex == nullptr) {
        LogError(ErrorCode::E507, HRESULT_FROM_WIN32(GetLastError()));
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        LogError(ErrorCode::E508);
        CloseHandle(hMutex);
        return 0;
    }

    InitCommonControls();

    wchar_t* cmdLine = GetCommandLineW();
    if (cmdLine && (wcsstr(cmdLine, L"/uninstall") || wcsstr(cmdLine, L"-uninstall"))) {
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        Uninstall();
        CoUninitialize();
        MessageBoxW(NULL, L"IdleDimmer has been uninstalled.", APP_NAME, MB_OK | MB_ICONINFORMATION);
        SelfDelete();
        CloseHandle(hMutex);
        return 0;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SetupWndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"IdleDimmerSetupClass";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, L"IdleDimmerSetupClass", L"IdleDimmer Setup",
        WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 440, 250,
        NULL, NULL, hInst, NULL);
    if (!hwnd) { CloseHandle(hMutex); return 1; }

    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CloseHandle(hMutex);
    CoUninitialize();
    return (int)msg.wParam;
}
