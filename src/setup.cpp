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
#include <string>
#include <vector>
#include <winver.h>
#include "ErrorCodes.h"
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define IDI_APP 101
#define IDR_APP_BIN 102

static const wchar_t* APP_NAME = L"WinDimmer64";
static const wchar_t* INSTALL_DIR = L"WinDimmer64";
static const wchar_t* REG_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinDimmer64";
static const wchar_t* VER = L"1.2.9";

enum State { READY, INSTALLING, COMPLETE };
static State g_state = READY;
static wchar_t g_installPath[MAX_PATH];
static HWND g_hStatus, g_hLocation, g_hButton, g_hLaunchCheck;
static HFONT g_hFontTitle, g_hFontBody;

static void GetInstallPath(wchar_t* buf, DWORD size) {
    GetEnvironmentVariableW(L"LOCALAPPDATA", buf, size);
    wcscat_s(buf, size, L"\\");
    wcscat_s(buf, size, INSTALL_DIR);
}

static bool IsRunning() {
    return FindWindowW(L"WinDimmer64MainClass", NULL) != NULL;
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
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_APP_BIN), MAKEINTRESOURCEW(10));
    if (!hRes) {
        LogError(ErrorCode::E501, HRESULT_FROM_WIN32(GetLastError()));
        return false;
    }
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) {
        LogError(ErrorCode::E502, HRESULT_FROM_WIN32(GetLastError()));
        return false;
    }
    DWORD size = SizeofResource(NULL, hRes);
    void* data = LockResource(hData);
    if (!data || size == 0) {
        LogError(ErrorCode::E503, HRESULT_FROM_WIN32(GetLastError()));
        return false;
    }
    HANDLE hFile = CreateFileW(dest, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LogError(ErrorCode::E505, HRESULT_FROM_WIN32(GetLastError()));
        return false;
    }
    DWORD written;
    BOOL ok = WriteFile(hFile, data, size, &written, NULL);
    if (!ok || written != size) {
        LogError(ErrorCode::E506, HRESULT_FROM_WIN32(GetLastError()));
    }
    CloseHandle(hFile);
    return ok && written == size;
}

static void KillRunning() {
    HWND hwnd = FindWindowW(L"WinDimmer64MainClass", NULL);
    if (hwnd) {
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        for (int i = 0; i < 50; i++) {
            if (!IsRunning()) return;
            Sleep(100);
        }
        // Force kill if app didn't close (e.g. close-to-tray mode)
        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProc) {
            TerminateProcess(hProc, 1);
            CloseHandle(hProc);
            Sleep(300);
        }
    }
}

static void CreateShortcut(const wchar_t* target) {
    wchar_t path[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_STARTMENU, NULL, 0, path);
    wcscat_s(path, MAX_PATH, L"\\Programs\\WinDimmer64.lnk");

    IShellLinkW* psl;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl))) {
        psl->SetPath(target);
        psl->SetDescription(L"Monitor dimmer with per-monitor controls, hotkeys, and idle detection");
        IPersistFile* ppf;
        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
            ppf->Save(path, TRUE);
            ppf->Release();
        }
        psl->Release();
    }
}

static void RegisterUninstall() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return;
    wchar_t displayName[64], exePath[MAX_PATH], uninstallCmd[MAX_PATH + 32];
    swprintf(displayName, 64, L"%s", APP_NAME);
    swprintf(exePath, MAX_PATH, L"%s\\%s.exe", g_installPath, APP_NAME);
    swprintf(uninstallCmd, MAX_PATH + 32, L"%s\\%s.exe /uninstall", g_installPath, APP_NAME);
    RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (BYTE*)displayName, (DWORD)(sizeof(displayName)));
    RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, (BYTE*)exePath, (DWORD)(sizeof(exePath)));
    RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (BYTE*)uninstallCmd, (DWORD)(sizeof(uninstallCmd)));
    DWORD dummy = 1;
    RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (BYTE*)&dummy, sizeof(dummy));
    RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (BYTE*)&dummy, sizeof(dummy));
    RegCloseKey(hKey);
}

static void Uninstall() {
    GetInstallPath(g_installPath, MAX_PATH);
    KillRunning();
    wchar_t exePath[MAX_PATH];
    swprintf(exePath, MAX_PATH, L"%s\\%s.exe", g_installPath, APP_NAME);
    DeleteFileW(exePath);
    RemoveDirectoryW(g_installPath);

    wchar_t configDir[MAX_PATH];
    GetEnvironmentVariableW(L"APPDATA", configDir, MAX_PATH);
    wcscat_s(configDir, MAX_PATH, L"\\WinDimmer64");
    wchar_t iniPath[MAX_PATH];
    swprintf(iniPath, MAX_PATH, L"%s\\dimmer.ini", configDir);
    DeleteFileW(iniPath);
    RemoveDirectoryW(configDir);

    wchar_t shortcut[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_STARTMENU, NULL, 0, shortcut);
    wcscat_s(shortcut, MAX_PATH, L"\\Programs\\WinDimmer64.lnk");
    DeleteFileW(shortcut);

    RegDeleteKeyW(HKEY_CURRENT_USER, REG_PATH);
}

static LRESULT CALLBACK SetupWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCTW)lParam)->hInstance;
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right;
            int m = 25, cw = w - 50;
            int h = rc.bottom;

            GetInstallPath(g_installPath, MAX_PATH);
            wchar_t exePath[MAX_PATH];
            swprintf(exePath, MAX_PATH, L"%s\\%s.exe", g_installPath, APP_NAME);

            bool running = IsRunning();
            bool installed = GetFileAttributesW(exePath) != INVALID_FILE_ATTRIBUTES;

            std::wstring installedVer = L"";
            if (installed) {
                installedVer = GetExeVersion(exePath);
            }

            wchar_t status[196];
            if (running && installed)
                swprintf(status, 196, L"Running: v%s | Installed: v%s\r\nSetup will terminate it and overwrite.", installedVer.c_str(), installedVer.c_str());
            else if (running)
                swprintf(status, 196, L"Running: v%s\r\nSetup will terminate it and install.", installedVer.empty() ? L"unknown" : installedVer.c_str());
            else if (installed)
                swprintf(status, 196, L"Installed: v%s\r\nSetup will overwrite the existing installation.", installedVer.c_str());
            else
                swprintf(status, 196, L"Ready to install WinDimmer64 v%s", VER);

            wchar_t pathStr[MAX_PATH + 32];
            swprintf(pathStr, MAX_PATH + 32, L"Location: %s", g_installPath);

            // Title at top
            CreateWindowW(L"STATIC", L"WinDimmer64 Setup",
                WS_CHILD | WS_VISIBLE | SS_CENTER, m, 14, cw, 22, hwnd, NULL, hInst, NULL);
            // Status text — taller to avoid clipping multiline messages
            g_hStatus = CreateWindowW(L"STATIC", status,
                WS_CHILD | WS_VISIBLE | SS_CENTER, m, 44, cw, 50, hwnd, NULL, hInst, NULL);
            // Location — well separated from status (bottom 94 → top 102 = 8px gap)
            g_hLocation = CreateWindowW(L"STATIC", pathStr,
                WS_CHILD | WS_VISIBLE | SS_CENTER, m, 102, cw, 16, hwnd, NULL, hInst, NULL);
            // Launch checkbox — hidden until install completes
            g_hLaunchCheck = CreateWindowW(L"BUTTON", L"Launch WinDimmer64",
                WS_CHILD | BS_AUTOCHECKBOX | BS_LEFT, m, 126, cw, 18, hwnd, NULL, hInst, NULL);
            ShowWindow(g_hLaunchCheck, SW_HIDE);

            g_hButton = CreateWindowW(L"BUTTON", L"Install",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                (w - 120) / 2, h - 48, 120, 30, hwnd, (HMENU)IDOK, hInst, NULL);

            g_hFontTitle = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontBody = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hLocation, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hLaunchCheck, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                if (g_state == READY) {
                    g_state = INSTALLING;
                    SetWindowTextW(g_hButton, L"Installing...");
                    EnableWindow(g_hButton, FALSE);
                    SetWindowTextW(g_hStatus, L"Installing...");

                    KillRunning();
                    if (!CreateDirectoryW(g_installPath, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                        LogError(ErrorCode::E504, HRESULT_FROM_WIN32(GetLastError()));
                    }
                    wchar_t exePath[MAX_PATH];
                    swprintf(exePath, MAX_PATH, L"%s\\%s.exe", g_installPath, APP_NAME);

                    if (ExtractApp(exePath)) {
                        CreateShortcut(exePath);
                        RegisterUninstall();
                        SetWindowTextW(g_hStatus, L"Installation complete!");
                        ShowWindow(g_hLaunchCheck, SW_SHOW);
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
                        swprintf(exePath, MAX_PATH, L"%s\\%s.exe", g_installPath, APP_NAME);
                        Sleep(300); // let old mutex fully release
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
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR lpCmdLine, int) {
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\WinDimmer64SetupMutex");
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

    // Check for uninstall switch using wide command line
    wchar_t* cmdLine = GetCommandLineW();
    if (cmdLine && (wcsstr(cmdLine, L"/uninstall") || wcsstr(cmdLine, L"-uninstall"))) {
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        Uninstall();
        CoUninitialize();
        MessageBoxW(NULL, L"WinDimmer64 has been uninstalled.", APP_NAME, MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SetupWndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"WinDimmer64SetupClass";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, L"WinDimmer64SetupClass", L"WinDimmer64 Setup",
        WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 380, 230,
        NULL, NULL, hInst, NULL);
    if (!hwnd) return 1;

    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    // COM for shortcut creation
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
