#define WIN32_LEAN_AND_MEAN
#define _WIN32_IE 0x0600
#define UNICODE
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
#include "ErrorCodes.h"
#pragma comment(lib, "dwmapi.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define IDI_APP 101
#define IDR_APP_BIN 102

static const wchar_t* APP_NAME = L"IdleDimmer";
static const wchar_t* INSTALL_DIR = L"IdleDimmer";
static const wchar_t* REG_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\IdleDimmer";
static const wchar_t* VER = L"1.5.4";

enum State { READY, INSTALLING, COMPLETE };
static State g_state = READY;
static wchar_t g_installPath[MAX_PATH];
static HWND g_hStatus, g_hButton, g_hLaunchCheck, g_hLog;
static HFONT g_hFontTitle, g_hFontBody, g_hFontLog;
static HBRUSH g_hEditBgBrush;

static void Log(const wchar_t* fmt, ...) {
    wchar_t buf[1024];
    va_list args;
    va_start(args, fmt);
    vswprintf(buf, 1024, fmt, args);
    va_end(args);
    int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, len, len);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(buf));
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
        wchar_t buf[64];
        swprintf(buf, 64, L"%d.%d.%d",
            static_cast<int>(HIWORD(pFileInfo->dwProductVersionMS)),
            static_cast<int>(LOWORD(pFileInfo->dwProductVersionMS)),
            static_cast<int>(HIWORD(pFileInfo->dwProductVersionLS)));
        return buf;
    }
    return L"unknown";
}

static bool ExtractApp(const wchar_t* dest) {
    Log(L"  Extracting %s...\r\n", dest);
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_APP_BIN), MAKEINTRESOURCEW(10));
    if (!hRes) {
        LogError(ErrorCode::E501, HRESULT_FROM_WIN32(GetLastError()));
        Log(L"  [FAILED] FindResourceW error %lu\r\n", GetLastError());
        return false;
    }
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) {
        LogError(ErrorCode::E502, HRESULT_FROM_WIN32(GetLastError()));
        Log(L"  [FAILED] LoadResource error %lu\r\n", GetLastError());
        return false;
    }
    DWORD size = SizeofResource(NULL, hRes);
    void* data = LockResource(hData);
    if (!data || size == 0) {
        LogError(ErrorCode::E503, HRESULT_FROM_WIN32(GetLastError()));
        Log(L"  [FAILED] LockResource\r\n");
        return false;
    }
    Log(L"  Resource size: %lu bytes\r\n", size);
    HANDLE hFile = CreateFileW(dest, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LogError(ErrorCode::E505, HRESULT_FROM_WIN32(GetLastError()));
        Log(L"  [FAILED] CreateFileW error %lu\r\n", GetLastError());
        return false;
    }
    DWORD written;
    BOOL ok = WriteFile(hFile, data, size, &written, NULL);
    CloseHandle(hFile);
    if (!ok || written != size) {
        LogError(ErrorCode::E506, HRESULT_FROM_WIN32(GetLastError()));
        Log(L"  [FAILED] WriteFile wrote %lu of %lu bytes\r\n", written, size);
        DeleteFileW(dest);
        return false;
    }
    Log(L"  Extracted %lu bytes\r\n", written);
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
    Log(L"  Found running instance (PID %lu), sending WM_CLOSE...\r\n", pid);
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
            Log(L"  Wait returned %lu\r\n", waitResult);
        CloseHandle(hProc);
        Sleep(200);
    }
}

static void CreateShortcut(const wchar_t* target) {
    Log(L"  Creating Start Menu shortcut...\r\n");
    wchar_t path[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_STARTMENU, NULL, 0, path);
    wcscat_s(path, MAX_PATH, L"\\Programs\\IdleDimmer.lnk");
    IShellLinkW* psl;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl))) {
        psl->SetPath(target);
        psl->SetDescription(L"Monitor dimmer with per-monitor controls, hotkeys, and idle detection");
        IPersistFile* ppf;
        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
            ppf->Save(path, TRUE);
            ppf->Release();
            Log(L"  Shortcut saved\r\n");
        }
        psl->Release();
    }
}

static void RegisterUninstall() {
    Log(L"  Registering uninstall entry...\r\n");
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        Log(L"  [FAILED] RegCreateKeyExW\r\n");
        return;
    }
    wchar_t displayName[64], exePath[MAX_PATH], uninstallCmd[MAX_PATH + 32];
    swprintf(displayName, 64, L"%s", APP_NAME);
    swprintf(exePath, MAX_PATH, L"%s\\%s.exe", g_installPath, APP_NAME);
    swprintf(uninstallCmd, MAX_PATH + 32, L"%s\\%s.exe /uninstall", g_installPath, APP_NAME);
    RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (BYTE*)displayName, (DWORD)((wcslen(displayName) + 1) * sizeof(wchar_t)));
    RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, (BYTE*)exePath, (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t)));
    RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (BYTE*)uninstallCmd, (DWORD)((wcslen(uninstallCmd) + 1) * sizeof(wchar_t)));
    DWORD dummy = 1;
    RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (BYTE*)&dummy, sizeof(dummy));
    RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (BYTE*)&dummy, sizeof(dummy));
    RegCloseKey(hKey);
    Log(L"  Uninstall registered\r\n");
}

static void Uninstall() {
    GetInstallPath(g_installPath, MAX_PATH);
    Log(L"\r\n=== IdleDimmer Uninstall ===\r\n\r\n");
    Log(L"  Install path: %s\r\n", g_installPath);
    KillRunning();
    wchar_t exePath[MAX_PATH];
    swprintf(exePath, MAX_PATH, L"%s\\%s.exe", g_installPath, APP_NAME);
    DeleteFileW(exePath);
    Log(L"  Removed %s\r\n", exePath);
    RemoveDirectoryW(g_installPath);
    wchar_t configDir[MAX_PATH];
    GetEnvironmentVariableW(L"APPDATA", configDir, MAX_PATH);
    wcscat_s(configDir, MAX_PATH, L"\\IdleDimmer");
    wchar_t iniPath[MAX_PATH];
    swprintf(iniPath, MAX_PATH, L"%s\\dimmer.ini", configDir);
    DeleteFileW(iniPath);
    RemoveDirectoryW(configDir);
    wchar_t shortcut[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_STARTMENU, NULL, 0, shortcut);
    wcscat_s(shortcut, MAX_PATH, L"\\Programs\\IdleDimmer.lnk");
    DeleteFileW(shortcut);
    RegDeleteKeyW(HKEY_CURRENT_USER, REG_PATH);
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
            int m = 20;

            // Apply dark mode for title bar
            BOOL dark = TRUE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

            GetInstallPath(g_installPath, MAX_PATH);
            wchar_t exePath[MAX_PATH];
            swprintf(exePath, MAX_PATH, L"%s\\%s.exe", g_installPath, APP_NAME);

            bool running = IsRunning();
            bool installed = GetFileAttributesW(exePath) != INVALID_FILE_ATTRIBUTES;
            std::wstring installedVer;
            if (installed) installedVer = GetExeVersion(exePath);

            int leftW = 170;
            int logX = leftW + m * 2;
            int logW = cw - logX - m;

            g_hStatus = CreateWindowW(L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_CENTER, m, 44, leftW, 50, hwnd, NULL, hInst, NULL);

            wchar_t status[196];
            if (running && installed)
                swprintf(status, 196, L"Running: v%s | Installed: v%s\r\nSetup will terminate and overwrite.", installedVer.c_str(), installedVer.c_str());
            else if (running)
                swprintf(status, 196, L"Running: v%s\r\nSetup will terminate and install.", installedVer.empty() ? L"unknown" : installedVer.c_str());
            else if (installed)
                swprintf(status, 196, L"Installed: v%s\r\nSetup will overwrite.", installedVer.c_str());
            else
                swprintf(status, 196, L"Ready to install v%s", VER);
            SetWindowTextW(g_hStatus, status);

            g_hLaunchCheck = CreateWindowW(L"BUTTON", L"Launch after install",
                WS_CHILD | BS_AUTOCHECKBOX | BS_LEFT, m, 106, leftW, 18, hwnd, NULL, hInst, NULL);
            SendMessageW(g_hLaunchCheck, BM_SETCHECK, BST_CHECKED, 0);

            g_hButton = CreateWindowW(L"BUTTON", L"Install",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                m + (leftW - 100) / 2, ch - 48, 100, 30, hwnd, (HMENU)IDOK, hInst, NULL);

            // Log panel
            g_hLog = CreateWindowW(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_LEFT,
                logX, 14, logW, ch - 28, hwnd, NULL, hInst, NULL);

            g_hFontTitle = CreateFontW(20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Display");
            g_hFontBody = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
            g_hFontLog = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

            SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hLaunchCheck, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hLog, WM_SETFONT, (WPARAM)g_hFontLog, TRUE);

            Log(L">>> IdleDimmer Setup v%s\r\n", VER);
            Log(L">>> %s\r\n\r\n", g_installPath);
            if (installed) Log(L"  Installed: v%s\r\n", installedVer.c_str());
            if (running)  Log(L"  App is running\r\n");

            g_hEditBgBrush = CreateSolidBrush(RGB(30, 30, 30));

            break;
        }
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(200, 200, 200));
            SetBkColor(hdc, RGB(30, 30, 30));
            return (LRESULT)g_hEditBgBrush;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                if (g_state == READY) {
                    g_state = INSTALLING;
                    SetWindowTextW(g_hButton, L"Installing...");
                    EnableWindow(g_hButton, FALSE);
                    SetWindowTextW(g_hStatus, L"Installing...");

                    Log(L"\r\n=== Installing ===\r\n\r\n");
                    KillRunning();
                    Log(L"  Creating directory...\r\n");
                    if (!CreateDirectoryW(g_installPath, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                        LogError(ErrorCode::E504, HRESULT_FROM_WIN32(GetLastError()));
                        Log(L"  [FAILED] CreateDirectoryW error %lu\r\n", GetLastError());
                    }
                    wchar_t exePath[MAX_PATH];
                    swprintf(exePath, MAX_PATH, L"%s\\%s.exe", g_installPath, APP_NAME);

                    if (ExtractApp(exePath)) {
                        CreateShortcut(exePath);
                        RegisterUninstall();
                        Log(L"\r\n=== Installation complete! ===\r\n");
                        SetWindowTextW(g_hStatus, L"Installation complete!");
                        ShowWindow(g_hLaunchCheck, SW_SHOW);
                        SetWindowTextW(g_hButton, L"Close");
                        EnableWindow(g_hButton, TRUE);
                        g_state = COMPLETE;
                    } else {
                        Log(L"\r\n=== Installation FAILED ===\r\n");
                        SetWindowTextW(g_hStatus, L"Extraction failed.\r\nTry running as Administrator.");
                        SetWindowTextW(g_hButton, L"Close");
                        EnableWindow(g_hButton, TRUE);
                        g_state = COMPLETE;
                    }
                } else if (g_state == COMPLETE) {
                    if (SendMessageW(g_hLaunchCheck, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                        wchar_t exePath[MAX_PATH];
                        swprintf(exePath, MAX_PATH, L"%s\\%s.exe", g_installPath, APP_NAME);
                        Sleep(300);
                        STARTUPINFOW si = { sizeof(si) };
                        PROCESS_INFORMATION pi;
                        CreateProcessW(exePath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
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
            DeleteObject(g_hFontLog);
            DeleteObject(g_hEditBgBrush);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR lpCmdLine, int) {
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
        CloseHandle(hMutex);
        return 0;
    }

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
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
        CW_USEDEFAULT, CW_USEDEFAULT, 540, 240,
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
