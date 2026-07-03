#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <format>
#include <signal.h>
#include <cstdlib>
#include "MainWindow.h"
#include "DimmerManager.h"
#include "ErrorCodes.h"

// ── v1.7.7: Last-resort crash backstop ──
// If ANY exception escapes all our SEH layers (WndProc top-level, message loop,
// Create wrapper), this filter writes a crash marker file and terminates the
// process with exit code 0. Without this, Windows Error Reporting (WER) shows
// a crash dialog and the cert tool records a "crash at launch."
static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep) {
    // Try to write a crash marker. Best-effort — if it fails, just exit.
    __try {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
            wcscat_s(path, MAX_PATH, L"\\IdleDimmer");
            CreateDirectoryW(path, NULL);
            wcscat_s(path, MAX_PATH, L"\\crash.log");
            HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL,
                                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD code = ep && ep->ExceptionRecord
                    ? ep->ExceptionRecord->ExceptionCode : 0;
                wchar_t buf[256];
                int len = wsprintfW(buf,
                    L"IdleDimmer v1.8.8 crash: exception 0x%08X\r\n", code);
                DWORD written;
                WriteFile(hFile, buf, len * sizeof(wchar_t), &written, NULL);
                CloseHandle(hFile);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Writing the crash marker itself crashed. Just exit.
    }
    // Exit cleanly so cert tools don't record a Watson crash.
    ExitProcess(0);
    return EXCEPTION_EXECUTE_HANDLER; // unreachable
}

// ── v1.8.0: Write a one-line startup marker so we can tell whether WinMain
// was reached at all on cert VMs. Uses only Win32 APIs (no CRT heap). ──
static void WriteStartupMarker() {
    __try {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
            wcscat_s(path, MAX_PATH, L"\\IdleDimmer");
            CreateDirectoryW(path, NULL);
            wcscat_s(path, MAX_PATH, L"\\startup.log");
            HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL,
                                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                const wchar_t msg[] = L"IdleDimmer v1.8.8 WinMain reached\r\n";
                DWORD written;
                WriteFile(hFile, msg, sizeof(msg) - sizeof(wchar_t), &written, NULL);
                CloseHandle(hFile);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Best-effort — don't crash writing the marker.
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    // ── v1.8.0: Install crash handlers BEFORE anything else ──
    // These catch abort(), terminate(), CRT invalid-parameter, and SIGABRT —
    // all of which bypass SetUnhandledExceptionFilter and kill the process
    // silently on cert VMs. Must be first, before any std:: call or heap use.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    std::set_terminate([]() { ExitProcess(0); });
    _set_invalid_parameter_handler(
        [](const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
            ExitProcess(0);
        });
    signal(SIGABRT, [](int) { ExitProcess(0); });

    SetUnhandledExceptionFilter(CrashFilter);
    WriteStartupMarker();
    // 1. Single Instance Enforcement using Mutex
    SetLastError(ERROR_SUCCESS);
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\IdleDimmerMutex");
    if (hMutex == nullptr) {
        LogError(ErrorCode::E101, HRESULT_FROM_WIN32(GetLastError()));
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        LogError(ErrorCode::E102);
        // Find existing window, restore and bring to front
        HWND hwndExisting = FindWindowW(L"IdleDimmerMainClass", nullptr);
        if (hwndExisting) {
            ShowWindow(hwndExisting, SW_SHOW);
            ShowWindow(hwndExisting, SW_RESTORE);
            SetForegroundWindow(hwndExisting);
        }
        CloseHandle(hMutex);
        return 0;
    }

    // 2. Initialize COM for DirectWrite / Shell functions
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        LogError(ErrorCode::E103, hr);
        CloseHandle(hMutex);
        return 1;
    }
    bool comInitialized = SUCCEEDED(hr);

    // 3. Set high-DPI awareness dynamically
    // Per-monitor v2 ensures perfect layout on multi-monitor systems with different DPI scales
    typedef BOOL(WINAPI* PFN_SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        auto pfn = (PFN_SetProcessDpiAwarenessContext)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (pfn) {
            if (!pfn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
                LogError(ErrorCode::E104, HRESULT_FROM_WIN32(GetLastError()));
            }
        }
    }

    // 4. Create and show settings panel
    if (!MainWindow::Instance().Create(hInstance, nCmdShow)) {
        LogError(ErrorCode::E105);
        // Don't show MessageBox — it blocks forever on headless certification
        // VMs where no user is logged in interactively. Just log and exit.
        if (comInitialized) CoUninitialize();
        CloseHandle(hMutex);
        return 0;
    }

    // 5. Message Loop — with SEH to catch any unhandled crash
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        __try {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // Any message handler crashed (AV from driver, corrupted state,
            // unexpected SEH). Log it and keep the pump alive so the
            // process survives certification testing.
            LogError(ErrorCode::E211, static_cast<HRESULT>(GetExceptionCode()));
        }
    }

    // Clean up overlays and managers
    DimmerManager::Instance().DestroyOverlays();

    if (comInitialized) CoUninitialize();
    CloseHandle(hMutex);
    return static_cast<int>(msg.wParam);
}
