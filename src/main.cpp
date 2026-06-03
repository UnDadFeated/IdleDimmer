#include <windows.h>
#include <objbase.h>
#include "MainWindow.h"
#include "DimmerManager.h"
#include "ErrorCodes.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. Single Instance Enforcement using Mutex
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\IdleDimmerMutex");
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
    if (FAILED(hr)) {
        LogError(ErrorCode::E103, hr);
        CloseHandle(hMutex);
        return 1;
    }

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
        CoUninitialize();
        CloseHandle(hMutex);
        return 1;
    }

    // 5. Message Loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Clean up overlays and managers
    DimmerManager::Instance().DestroyOverlays();

    CoUninitialize();
    CloseHandle(hMutex);
    return static_cast<int>(msg.wParam);
}
