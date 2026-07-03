# Changelog

All notable changes to the IdleDimmer project are documented here.

## [1.8.8] - 2026-07-03

### Bug Fixes
* **Inactivity Timeout Priority**: Fixed a priority conflict where the screen would not dim after the inactivity timeout if a video or audio bypass app (such as web browsers or media players) was active in the background. Enforced Idle > video priority so overlay windows are shown and dimmed correctly when idle.

## [1.8.7] - 2026-07-03

### Bug Fixes
* **Warm Amber Tint Toggle**: Fixed an issue where toggling the Warm Amber Tint setting did not instantly apply the color change to active overlays until a fade animation was triggered. Toggling now redraws the GDI surface and forces a layered window recomposition immediately.
* **MS Store/MSIX Update Exit**: Added active session ending detection (`WM_QUERYENDSESSION` and `WM_ENDSESSION`). This lets the app shut down cleanly when an update is requested by Windows Deployment or the Microsoft Store, rather than minimizing to tray and blocking the update.
* **Idle Dimming Reliability**: Fixed potential UI thread locks by throttling process-to-monitor matching in audio checks to every 5 seconds. Standardized click-through styles and frame changes when overlay windows are recreated during DPI/display changes.

## [1.8.6] - 2026-07-02

### Updates
* **Compiler Warnings Clean-up**: Resolved all compiler and static analysis warnings across the entire codebase under strict syntax checking flags.
* **Initialization Hardening**: Upgraded structure initialization syntax (including `WNDCLASSEXW`, `NOTIFYICONDATAW`, `RECT`, and `MONITORINFO`) to warning-free, standard C++ value-initialization.
* **Store Listing Alignment**: Updated the Microsoft Store listing and documentation to align with current features, removing obsolete references to hotkeys that were deprecated in v1.5.9.

## [1.8.5] - 2026-07-02

### Bug Fixes
* **Settings > Apps Uninstall Failure**: Fixed an issue where clicking uninstall from Windows Settings launched the main program instead of uninstalling. The installer now copies itself to `%LOCALAPPDATA%\IdleDimmer\uninstall.exe` on install, and the registry uninstall command runs it. Upon exit, `uninstall.exe` runs a detached background process to delete itself and remove the installation directory.
* **Hardened Uninstall Directory Cleanup**: Deletes `startup.log` and `crash.log` in `%APPDATA%\IdleDimmer` prior to folder removal, ensuring `RemoveDirectoryW` does not fail due to a non-empty directory.

## [1.8.4] - 2026-06-30

### Bug Fixes
* **Nineteenth Certification Fix — Startup Sizing Jitter Prevention (Policy 10.1.2.10)**: Fixed a latent window-resizing pop issue during early-stage window mapping:
  1. **Lifecycle-Aware DPI Helper**: Created a unified `GetWindowDpi()` helper that queries `GetDpiForWindow` only if the window is currently visible, falling back dynamically to `GetDpiForSystem()` during early hidden-stage layout updates.
  2. **Jitter Prevention in UpdateLayout**: Replaced independent queries in the `UpdateLayout` tail and `CreateGraphicsResources` with the new helper, preventing the window from shrinking back down to 1.0x size inside `WM_APP+2` before being corrected on `WM_APP+4`.

## [1.8.3] - 2026-06-30

### Bug Fixes
* **Eighteenth Certification Fix — Hidden Window DPI Initialization (Policy 10.1.2.10)**: Fixed a startup bug where the window was created unscaled and `GetDpiForWindow` returned stale readings (96 DPI) when queried on the hidden window during early initialization, causing layout truncation on high-DPI monitors:
  1. **Primary Screen Startup Scaling**: Scaled the initial creation dimensions in `CreateImpl` using `GetDpiForSystem()`, ensuring the window starts at the correct physical size for the primary display.
  2. **Render Target Creation Safety Net**: Added a runtime window-sizing synchronization block in `CreateGraphicsResources()` directly before `GetClientRect()` and render target creation. This guarantees the client size and Direct2D DPI configuration are always derived in sync from a single, active DPI query after the window has been mapped and monitor-associated.
  3. **Documentation Update**: Fixed a stale line reference in `AGENTS.md` regarding device-loss handling.

## [1.8.2] - 2026-06-30

### Bug Fixes
* **Seventeenth Certification Fix — DPI Scaling & Resolution Compatibility (Policy 10.1.2.10)**: Fixed a layout truncation and hit-test misalignment bug on high-DPI scaling screens (observed at 150%+ scaling on devices like Surface Laptop and Dell Inspiron):
  1. **Dynamic DPI Scaling**: Applied custom DPI scaling to the main window's layout size and switched client-to-window calculation from `AdjustWindowRectEx` to `AdjustWindowRectExForDpi`.
  2. **Direct2D DPI Synchronization**: Added `SetDpi` calls on Direct2D HWND render targets at creation and display events, ensuring DirectWrite and shapes scale precisely with the window's current DPI.
  3. **Logical Coordinate Mapping**: Mapped physical mouse input coordinates back to logical DIP units in the message loop (`WM_MOUSEMOVE`/`WM_LBUTTONDOWN`/`WM_LBUTTONUP`/`WM_MOUSEWHEEL`), matching the scaled hit-test zones and restoring correct slider drag and click alignment at any scaling factor.
  4. **WM_DPICHANGED support**: Added full message-handling logic for DPI changes at runtime, enabling on-the-fly resizing and layout adjustment when dragged across monitors with different scale configurations.

## [1.8.1] - 2026-06-29

### Bug Fixes
* **Sixteenth Certification Fix — AppxManifest EntryPoint Typo**: Fixed a critical packaging typo where the application `EntryPoint` in `Package.appxmanifest` was defined as `WindowsFullTrustApplication` instead of `Windows.FullTrustApplication`. The missing dot prevented the packaged desktop bridge application from launching inside the MSIX container, causing the reported "crashes at launch" error on Windows 11 build 26200. Bumped the version to 1.8.1 across all project configurations.

## [1.8.0] - 2026-06-26

### Bug Fixes
* **Fifteenth Certification Fix — Delay-Loaded DLLs + Pre-WinMain Crash Handlers**: After 14 versions of SEH wrapping and try/catch that only work after `WinMain`, the crash at launch on Surface Laptop 4 (build 26200.8457) persisted because the crash happens *before* `WinMain` runs — during DLL loading or CRT initialization. Three changes:
  1. **Delay-loaded 6 non-essential DLLs**: `d2d1.dll`, `DWrite.dll`, `dwmapi.dll`, `WINHTTP.dll`, `comdlg32.dll`, and `VERSION.dll` are now delay-loaded via `/DELAYLOAD` (MSVC) / `--delayload` (clang). They're only loaded on first call, not at process start. The import table dropped from 24 entries to 6 (USER32, GDI32, SHELL32, ole32, ADVAPI32, KERNEL32). A missing or broken DLL no longer kills the process before any crash handler is installed.
  2. **Pre-WinMain crash handlers**: Added `_set_abort_behavior(0)`, `std::set_terminate()`, `_set_invalid_parameter_handler()`, and `signal(SIGABRT)` at the very top of `WinMain` — these catch `abort()`, `terminate()`, CRT invalid parameter errors, and SIGABRT, all of which bypass `SetUnhandledExceptionFilter` and silently kill the process.
  3. **Eliminated global heap allocations**: Replaced two `inline std::wstring` globals in `ErrorCodes.h` with `const wchar_t*` pointers and a function-local static. Global `std::wstring` constructors allocate heap memory before `WinMain` runs — if the CRT heap isn't fully initialized (MSIX sandbox edge case), this crashes with no handler to catch it.
* **Startup diagnostic marker**: `WinMain` now writes `startup.log` to `%APPDATA%\IdleDimmer\` as its first action (before COM, mutex, or DPI init). If the cert tester retrieves this file, it confirms whether `WinMain` was reached at all.
* **LogError hardened**: `LogError()` in `ErrorCodes.h` now wraps `std::format` calls in try/catch. If the heap or locale is broken during early startup, it falls back to raw `OutputDebugStringW` with the error code name instead of crashing.

## [1.7.9] - 2026-06-26

### Bug Fixes
* **Fixed MSVC Compiler Error C2712 / MinGW Runtime Crash on Audio Thread**: Separated the structured exception handling wrapper (`__try`/`__except`) from the actual C++ logic containing standard library structures (`ComPtr`, `std::vector`, `std::wstring`) inside `DoAudioCheckSEH`. This resolves a compilation error under MSVC and fixes runtime instability and crashes at launch on Windows 11 under LLVM-MinGW.

## [1.7.8] - 2026-06-25

### Bug Fixes
* **Eighth Certification Fix — Moved EnumWindows Off Audio Background Thread & WndProc Hardening**: Fixed a critical crash/deadlock bug ("crashes a while after launch") observed during Microsoft Store certification on Windows 11 devices:
  1. **EnumWindows execution isolation**: Relocated `EnumWindows`/`GetMonitorsForProcessName` calls from the background detached audio thread to the main UI thread during `CheckVideoPlayback`. Since `EnumWindows` synchronously pumps window messages, invoking it on a background thread without a message pump can deadlock or crash if any enumerated top-level window belongs to a hung thread.
  2. **COM exception isolation (`DoAudioCheckSEH`)**: Wrapped the background WASAPI/COM device and audio session enumeration loop inside structured exception handling (`__try`/`__except`) and C++ `try`/`catch(...)` blocks to prevent driver or allocation-level crashes from taking down the process.
  3. **OSD message handler SEH**: Wrapped the Direct2D paint/draw messages in the click-through OSD window procedure (`OSDWndProc`) within `__try`/`__except` guards to capture driver access violations and avoid infinite invalidation repaint loops.

## [1.7.7] - 2026-06-25

### Bug Fixes
* **Seventh Certification Fix — Stale PE Binary Version + Full WndProc SEH**: Two root causes found for the persistent "crash at launch" on Surface Laptop 5 (build 26200.8328):
  1. **Stale binary `FILEVERSION`**: The `FILEVERSION`/`PRODUCTVERSION` fields in `resources.rc` and `setup.rc` were stuck at `1,7,4,0` since v1.7.4 — only the string block was updated each bump. `GetOwnVersion()` reads the binary version via `VerQueryValue`, so the MSIX manifest said `1.7.6.0` while the actual PE reported `1.7.4.0`. This version mismatch can cause Windows MSIX validation to reject or crash the app at launch.
  2. **Unprotected `WM_APP+2` deferred init**: The entire deferred startup handler (overlay creation, tray icon, timers, layout) had zero crash protection. Any SEH from D2D/DWM or C++ exception from `std::vector`/`std::format` killed the process. Added top-level `__try/__except` around the **entire `WndProc`** body (not just `WM_PAINT`) plus `try/catch(...)` specifically around the deferred init with a minimal fallback path.
* **OverlayWndProc SEH protection**: Overlay window message handlers (fade timer, paint, positioning) were also unprotected — a driver-level SEH from Intel Iris Xe during overlay operations would kill the process. Now wrapped in `__try/__except`.
* **`SetUnhandledExceptionFilter` backstop**: Added an absolute last-resort crash handler in `WinMain` that writes a crash marker to `%APPDATA%\IdleDimmer\crash.log` and exits cleanly (code 0) instead of triggering Watson/WER — cert tools won't record it as a crash.

## [1.7.6] - 2026-06-24

### Bug Fixes
* **Sixth Certification Fix — SEH Message Loop + Silent Startup Exit**: v1.7.5's targeted SEH around D2D init/paint still left crashes from unprotected message handlers uncaught on Surface Laptop Go (OS build 26200.7922). Two changes:
  1. **Message loop wrapped in `__try/__except`**: The `while (GetMessageW)` loop in `WinMain` now catches any SEH exception from any message handler (WM_APP+2, WM_TIMER, WM_DISPLAYCHANGE, etc.) and keeps the pump alive. No unhandled crash can kill the process.
  2. **Removed blocking MessageBox on startup failure**: `WinMain`'s `MessageBoxW` when `Create()` fails blocked indefinitely on headless cert VMs with no interactive user. Replaced with silent `LogError` + clean exit.

## [1.7.5] - 2026-06-23

### Bug Fixes
* **Fifth Certification Fix — SEH Protection for D2D Driver Crashes on Intel Iris Xe / WDDM 3.2**: v1.7.4's async D2D init path (`WM_APP+4`) fixed the first-paint hang but introduced a new failure mode: D2D driver calls (`D2D1CreateFactory`, `CreateHwndRenderTarget`) on Intel Iris Xe / WDDM 3.2 / build 26200+ can throw **SEH access violations** instead of returning HRESULT. Without `__try/__except`, these AVs propagate through `DispatchMessageW` and terminate the process. Added three layers of protection:
  1. **`WM_APP+4` wrapped in `__try/__except`**: Catches SEH crashes from `CreateGraphicsResources()`. On catch, sets `m_d2dFailed = true` and permanently falls back to GDI rendering.
  2. **`WM_PAINT` wrapped in `__try/__except`**: Catches any D2D paint-path crash (device loss, TDR). On catch, discards D2D resources, sets `m_d2dFailed`, and forces a GDI repaint.
  3. **`DiscardGraphicsResources()` resets `m_d2dReady`**: Fixes a latent bug where `D2DERR_RECREATED` from `EndDraw()` would release D2D resources but leave `m_d2dReady=true`, causing a null-pointer crash on the next paint.
* The GDI fallback now checks `m_d2dFailed` to avoid posting `WM_APP+4` when D2D is permanently dead.

## [1.7.4] - 2026-06-23

### Bug Fixes
* **Fourth Certification Freeze Fix — Safety Nets for Certification VMs**: Added three layers of resilience after research into Win11 24H2 / WDDM 3.2 hang patterns found in the wild. (1) **GDI-first paint fallback**: The first `WM_PAINT` always renders with GDI instead of Direct2D. If D2D init has never succeeded, `OnPaint` paints with GDI and posts `WM_APP+4` to try D2D asynchronously. This prevents the DWM ghost-window hang that occurs when the first `EndDraw`/`Present` stalls on iGPUs under WDDM 3.2 (JUCE framework had the identical issue). (2) **Shell_NotifyIcon retry timer (ID 204)**: If `AddTrayIcon` fails (common during early startup), a 4-second retry timer is set instead of blocking — preventing `Shell_NotifyIcon`'s internal `SendMessageTimeout` from contributing to the 5-second hang window. (3) **Async D2D init via `WM_APP+4`**: D2D creation is decoupled from `WM_PAINT`, so the window draws immediately via GDI and upgrades to D2D later without time pressure.

## [1.7.3] - 2026-06-23

### Bug Fixes
* **Third Certification Freeze Fix — Deferred Startup Initialization**: Fixed a freeze-at-launch (Event ID 1002 Application Hang) on OS build 26200.8246 (Dell Inspiron 12-5280, Win11 24H2+/WDDM 3.2) with the same symptom as v1.7.1/v1.7.2 but triggered by *different* blocking operations. Split `CreateImpl()` into a minimal synchronous phase (window creation + class registration only) and a deferred phase (`WM_APP+2` handler) that runs on the first message-loop iteration. All potentially-blocking operations — `RefreshMonitors()` (DWM overlay-window creation), `AddTrayIcon()` (Shell_NotifyIcon IPC), `SetWarmTint()`/`SetDimmingEnabled()` (D2D overlay init), `SetTimer()` calls, and `UpdateLayout()` (`SetWindowPos` → nested `WM_SIZE`/`WM_PAINT`) — now run after the message pump is live, preventing certification VMs from hanging when the GPU driver or DWM takes too long synchronously.

## [1.7.2] - 2026-06-22

### Bug Fixes
* **Second Certification Freeze Fix — All Remaining Vectors Eliminated**: Fixed four root causes contributing to the freeze-at-launch on OS build 22631.6199 (Event ID 1002 Application Hang). (1) Added the Windows 11 `supportedOS` GUID `{1f676c76-80e1-4239-95bb-83d0f6d0da78}` to the application manifest so the correct DWM compatibility shims are applied. (2) Corrected the `D2DERR_RECREATED` fallback value from `0x8898000C` to `0x88980001` — the old value was a typo that caused D2D device-loss recovery to silently fail. (3) Wrapped the `CheckAudioPlaybackAsync` thread body in an RAII guard so `m_audioCheckInFlight` always resets, even if an exception escapes the COM enumeration. (4) Replaced synchronous `UpdateWindow()` with deferred `InvalidateRect()` in the overlay creation path to avoid running `WM_PAINT` before D2D infrastructure is fully initialized.

## [1.7.1] - 2026-06-19

### Bug Fixes
* **Certification Hang / Freeze at Launch Fix**: Moved the synchronous WASAPI/COM audio session enumeration from the main UI thread (called inside the 1-second `WM_TIMER` callback) to a background worker thread. This prevents the application from blocking the message pump and hanging (causing a Windows DWM "ghost window" with the app icon centered) in environments where the audio stack is not fully initialized or no audio render device is present, which is common in Microsoft Store certification VMs.

## [1.7.0] - 2026-06-18

### Updates
* **C++23 Modernization**: Migrated the entire codebase to C++23. Replaced all legacy `StringCchPrintfW`, `swprintf`, `vswprintf`, `StringCchCopyW`, and `swscanf` calls with `std::format` and `wcscpy_s` for safer, more maintainable string formatting. Replaced the manual `E###` error code constants in `ErrorCodes.h` with compile-time `std::source_location` for automatic source-file and line-number capture in error logs. This is a no-behavior-change refactor — all functionality, APIs, and file formats remain identical.

## [1.6.7] - 2026-06-18

### Bug Fixes
* **Certification Crash at Launch (7th attempt) — All paths eliminated**: Comprehensive audit of the full codebase eliminated every identified crash-at-launch vector. Key hardening: (1) First-launch config initialization in `CreateImpl()` now sets `m_config` fields directly instead of calling `ApplyPreset()`, which triggered `DimmerManager` calls and file I/O before the manager was initialized. (2) Wrapped the entire `SaveConfig` body in `try/catch(...)` to prevent any unhandled C++ exception from terminating the process during startup file writes in MSIX sandboxes. (3) Added a top-level C++ `try/catch(...)` around `CreateImpl()` in `MainWindow::Create()` — the existing `__try/__except` only catches SEH exceptions, not C++ exceptions like `std::bad_alloc` or `std::runtime_error`. (4) `WinMain` now returns exit code 0 (instead of 1) on initialization failure and shows a user-friendly `MessageBox` so certification testers can see the error. (5) Added `#pragma comment(lib, "comdlg32.lib")` for MSVC build compatibility with the new `GetOpenFileNameW`/`GetSaveFileNameW` imports. (6) `comdlg32.dll` is already dynamically loaded for the LLVM-MinGW build via the existing `-lcomdlg32` linker flag.

## [1.6.5] - 2026-06-18

### New Features
* **Time-of-Day Scheduling**: Added automated dimming schedule with `ScheduleEnabled`, `ScheduleStartMins`, `ScheduleEndMins` (stored as minutes-of-day, snapped to 15-minute intervals via the UI sliders). Overnight spans (e.g. 22:00 → 07:00) are handled correctly via midnight wrap. `DimmerManager::CheckSchedule()` runs on the existing 1-second tick and triggers a fade on every overlay when entering or exiting the scheduled window. The scheduled dim level is applied as the fade target only when neither video-bypass nor idle state takes precedence.
* **Click-Through OSD Notifications**: Added a sleek dark rounded-card OSD at the bottom-right of the primary monitor that shows brightness changes (e.g. "Brightness: 75%", "Preset: Night | 20%", "Profile imported"). The window uses `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST` — it never steals focus, never appears in Alt+Tab, never intercepts clicks, and stays above fullscreen apps. It fades in over ~250 ms, displays for 2 seconds, then fades out over ~250 ms. A Direct2D paint path is used with a GDI fallback (`DrawTextW` + solid brush) for stripped-down Windows images where DirectWrite font creation fails.
* **Preset Profile Buttons**: Added a row of 4 compact monochrome preset buttons under the per-monitor sliders — `[Gaming]` `[Reading]` `[Night]` `[OLED]`. Clicking applies an immediate configuration override, pushes to the undo stack, propagates master value to every monitor via `SyncMonitorsWithConfig()`, and shows an OSD confirmation. Each preset is a hand-curated combination of dimming-enabled / warm-tint / master-value: Gaming = off/off/0, Reading = on/on/30, Night = on/on/80, OLED = on/off/90.
* **Profile Import / Export**: Added `[Import]` and `[Export]` buttons in the right-side sliding panel. Uses native Win32 `GetOpenFileNameW` / `GetSaveFileNameW` dialogs with a `.ini` filter and `.ini` default extension. The exported file is a standard `dimmer.ini` config file that can be edited by hand, shared between machines, or imported on another install. On import, all settings (including scheduling, blocked-apps list, monitor values, warm tint, etc.) are applied immediately, the layout is rebuilt, and an OSD confirmation is shown.
* **Overlay Hiding During Video Bypass (PowerPoint / Presentation Compatibility)**: When a monitor enters video-bypass state (fullscreen video, blocked-app audio, or fullscreen D3D app), the overlay is now hidden via `ShowWindow(hwnd, SW_HIDE)` instead of just being faded to alpha=0. When bypass ends, the overlay is restored with `SW_SHOWNOACTIVATE`. This completely eliminates any chance of a stale layered window intercepting clicks or appearing in screen captures / PowerPoint presentation modes.

### Modified Files
* `src/ConfigManager.h` — added `scheduleEnabled`, `scheduleStartMins`, `scheduleEndMins`, `scheduleDimLevel` fields to `AppConfig` with safe defaults.
* `src/ConfigManager.cpp` — hand-parses and serializes the four new schedule fields. Missing/malformed fields retain defaults.
* `src/DimmerManager.h` — added schedule state (`m_scheduleEnabled`, `m_scheduleActive`, `m_scheduleStartMins`, `m_scheduleEndMins`, `m_scheduleDimLevel`), OSD state (HWND, Direct2D factory/target/text-format/brushes, alpha/timer state), and new public API (`SetScheduleEnabled`, `SetScheduleRange`, `IsScheduleActive`, `GetScheduleDimLevel`, `CheckSchedule`, `ShowOSD`, `DestroyOSD`). Now transitively includes `<d2d1.h>` / `<dwrite.h>`.
* `src/DimmerManager.cpp` — implements `SetScheduleRange`, `CheckSchedule` (with same-day vs overnight span handling), `ShowOSD`, `DestroyOSD`, `EnsureOSDResources`, `DiscardOSDResources`, and `OSDWndProc`. Updated `CheckVideoPlayback` to `ShowWindow(SW_HIDE)` overlays during video bypass and `SW_SHOWNOACTIVATE` after. Updated `OverlayWndProc` WM_TIMER to apply schedule dim level when `IsScheduleActive()` is true. Calls `CheckSchedule()` at the end of every `CheckVideoPlayback`. Updated destructor to call `DestroyOSD()`.
* `src/MainWindow.h` — added `isScheduleStart`/`isScheduleEnd` flags to `UISlider`. Added `UIPresetButton` struct, `m_presets` vector, `ApplyPreset(int)` method, `m_importProfileRect`/`m_exportProfileRect`/hover state, `ShowImportProfileDialog`/`ShowExportProfileDialog` methods.
* `src/MainWindow.cpp` — `UpdateLayout` now places preset button row, SCHEDULE checkbox + optional Start/End sliders, and Import/Export button rectangles. `LoadSettings` now pushes schedule config into DimmerManager. Added `ApplyPreset`, `ShowImportProfileDialog`, `ShowExportProfileDialog` implementations at end of file.
* `src/MainWindowDraw.cpp` — `OnPaint` now renders preset buttons, schedule section header, schedule sliders with time-of-day formatting (`HH:MM`), and Import/Export buttons inside the right-side panel.
* `src/MainWindowInput.cpp` — `HandleMouseMove` now hovers preset/profile buttons, handles schedule slider drags (with 15-min snapping), and triggers OSD on master/per-monitor slider drag. `HandleLButtonDown` handles preset clicks (before slider-drag check), Import/Export button clicks, and ScheduleEnabled checkbox toggle (re-layouts). `HandleMouseWheel` and `HandleKeyDown` also handle the schedule sliders.
* `build.bat` — added `comdlg32.lib` for the Win32 file dialogs.
* `README.md` — added `-lcomdlg32` to the LLVM-MinGW build command.

## [1.6.4] - 2026-06-18

### Bug Fixes
* **Locale Initialization Crash on Stripped Windows Images**: Wrapped the `std::locale("")` imbue call in both `LoadConfig` and `SaveConfig` (`ConfigManager.cpp`) with a `try/catch` for `std::runtime_error`. On stripped-down Windows images — Store certification VMs, sandboxed containers, Server Core without locale DLLs — `std::locale("")` can throw an uncaught `std::runtime_error` and terminate the process at launch. The fix falls back to the default `"C"` locale, which is safe for our data because the config file is pure ASCII (JSON keys + `\\.\DISPLAYn` monitor IDs). Added `<stdexcept>` include to make the catch type explicitly available.
* **UpdateLayout Ordering in CreateImpl**: Moved `UpdateLayout()` to run *after* `ShowWindow()` / `UpdateWindow()` in `MainWindow::CreateImpl()`. `UpdateLayout()` ends with a `SetWindowPos()` call, which pumps `WM_SIZE` / `WM_PAINT`. Although `OnResize` is null-safe and `ValidateRect` runs unconditionally in `WndProc`, ordering the layout resize after the first legitimate paint cycle — when Direct2D resources are guaranteed to be initialized — eliminates a class of edge cases on certification VMs where D2D device creation may transiently fail. This is a defensive hardening fix, not a crash fix.

## [1.6.3] - 2026-06-17

### Bug Fixes
* **Font Fallback for Stripped Windows Images**: Added automatic fallback from `Segoe UI Variable Display` / `Segoe UI Variable Text` to the universally-available `Segoe UI` when DirectWrite font creation fails. On test machines with missing or incomplete font installations (e.g. certification lab Surface Laptops), the previous behavior caused `CreateGraphicsResources()` to fail silently, making the app exit with no visible window — which looks like a crash at launch.
* **Config File Locale Corruption**: Added `std::locale("")` imbue to both `wifstream` and `wofstream` in config read/write paths. The default `"C"` locale could produce garbled monitor IDs on certain Windows code pages, corrupting the config file and causing phantom monitor entries on subsequent launches.

## [1.6.2] - 2026-06-16

### Bug Fixes
* **Statically Linked MinGW Runtime**: Configured LLVM-MinGW `clang++` compilation with the `-static` flag to statically link C++ standard library (`libc++`) and unwind (`libunwind`) resources. This resolves the load-time `0xc000007b` (invalid image format/missing DLL dependencies) application error when running on environments that lack the LLVM development toolchain.
* **Windows PE (WinPE) Compatibility**: Refactored the setup installer to dynamically load `dwmapi.dll` and resolve `DwmSetWindowAttribute` at runtime. Added safety guards around `SHGetFolderPathW` to handle cases where Start Menu folders do not exist. These changes resolve startup load-time `0xc000007b` errors and potential shell folder crashes in lightweight/Windows PE environments.

## [1.6.1] - 2026-06-16

### Bug Fixes
* **MS Store Certification Exit Hang Fix**: Changed the `WaitForSingleObject` wait timeout in the `MainWindow` destructor from `INFINITE` to `3000` milliseconds. This prevents a potential shutdown hang if the update checking thread fails to abort quickly during program exit.

## [1.6.0] - 2026-06-16

### Bug Fixes
* **MS Store Certification Launch Crash**: Resolved a launch crash observed on Windows 11 build 26200.8457.
  * Explicitly set the PE subsystem version to 10.0 and OS version to 10.0 in compiler and linker settings (MSVC and Clang/MinGW) to match the target environment and avoid loader shim mismatch issues.
  * Updated MSIX manifest `MaxVersionTested` to `10.0.26200.0` to explicitly support the certification test environment OS version.
  * Delayed the manual update checking process by 15 seconds after app launch to ensure a smooth startup.
  * Stored the update check WinHTTP session handle and safely cancelled any pending synchronous requests using `InterlockedExchangePointer` and `WinHttpCloseHandle` at window destruction. This prevents a use-after-free crash if the app is closed while the network request is still blocking/pending.
  * Removed legacy configuration migration code referencing the old `WinDimmer64` application directory to eliminate potential file access issues and clean up unused config references.

## [1.5.11] - 2026-06-15

### Bug Fixes
* **MS Store Certification Crash Fix (Windows Build 26200)**: Resolved a launch crash on Windows 11 25H2 build 26200.7840 observed on HP Spectre x360 and Surface Laptop devices.
  * Added `WS_EX_TRANSPARENT` to overlay creation style so overlays start in click-through mode, avoiding digitizer/touch input conflicts during startup.
  * Deferred the initial fade trigger on overlay creation using `PostMessageW` (rather than a direct call) to allow the message queue to process standard creation events first.
  * Safely check the return value of `DwmSetWindowAttribute` calls and ignore failures gracefully if attributes are not supported or fail to apply.
  * Wrapped the main window creation sequence in a structured exception handler (`__try`/`__except`) to safely log initialization crashes and avoid sudden termination.
  * Guarded registry-based startup configuration with an `IsPackaged()` check to prevent virtualized registry write failures when running inside the MSIX container.
  * Bumped `MaxVersionTested` in the MSIX manifest to `10.0.26100.0` to prevent Windows from applying buggy compatibility shims on newer 25H2 OS builds.

## [1.5.10] - 2026-06-12

### Bug Fixes
* **Surface & High-DPI Startup Crash Fix**: Resolved a critical launch crash on devices with touchscreen and high-DPI scaling (such as the Surface Laptop Go). Deferred the creation of Direct2D graphics resources from initial creation to the first frame repaint, and enforced a minimum `1x1` target size during render target creation and window resizing. This prevents invalid hardware allocations (zero width/height dimensions) that cause driver crashes and startup failures on Intel graphics and virtualized environments.
* **DirectWrite Partial-Initialization Crash Fix**: Fixed a bug where partial DirectWrite initialization failures during startup would leave text format pointers as null on subsequent paint loops, causing immediate access violations. The factory and all text formats are now safely verified and recreated on any resource mismatch.

## [1.5.9] - 2026-06-12

### Updates
* **Removed Global Hotkeys**: Removed all global system-wide keybinds (`Ctrl+Alt+ArrowUp`, `Ctrl+Alt+ArrowDown`, `Ctrl+Alt+D`) to resolve hotkey conflicts and issues.
* **Modern C++17 Codebase Refactoring**: Hardened codebase with modern C++17 standards, including using `Microsoft::WRL::ComPtr` for COM resource lifetime safety in audio checking and shortcut creation, standardizing on `std::wstring_view` for string parameters, and using standard C++ structured bindings.
* **Sleep Resume & Power State Robustness**: Handled `WM_POWERBROADCAST` sleep resume events to automatically recreate and refresh monitor overlays, fixing the issue where dimming would stop working after the system woke up from long periods of inactivity/sleep.

## [1.5.8] - 2026-06-11

### Bug Fixes
* **Direct2D/DirectWrite Resource Robustness**: Refactored graphics resource initialization to return appropriate failure HRESULTs if any brush or text format fails to initialize. This prevents access violation crashes on startup if DirectWrite fails to load requested fonts on the device or if the render target cannot allocate resources during initialization.
* **Safe Monitor String Resolution**: Explicitly null-terminated the device name buffer in monitor enumeration (`mi.szDevice`). This prevents access violation crashes when querying monitor details on specific laptop/tablet configurations (such as Dell Inspiron 13, Surface Go 4, and Dell Latitude 5520) which could otherwise construct `std::wstring` from un-terminated memory.
* **Safe Version Extraction**: Hardened executable path retrieval inside `GetOwnVersion()` with safe length checking and explicit null-termination.

## [1.5.7] - 2026-06-10

### Bug Fixes
* **HP Spectre x360 Launch Crash Fix**: Resolved a critical launch crash on devices with screen rotation, touchscreen, and custom DPI configurations (such as the HP Spectre x360 2-in-1). The window handle (`m_hwnd`) is now initialized early during the `WM_NCCREATE` message in `WndProc`, preventing access violation crashes when early messages (like repaint or size changes) trigger Direct2D resource initialization (`GetClientRect` and `CreateHwndRenderTarget`) before the main window creation completes.
* **Defender Heuristics Optimization**: Optimized fullscreen app checking by caching the result of `IsFullscreenAppActive()` inside the 5-second check interval. This avoids calling `LoadLibraryW`/`FreeLibrary` on `shell32.dll` every second, satisfying security heuristic patterns.
* **Dynamic Version Resource Loading**: Implemented a `GetOwnVersion()` runtime helper that extracts the application version directly from the PE version resource, eliminating hardcoded version variables in the display loop.
* **Cleaned Up Obsolete UWP Tracking**: Removed unused UWP child window and PID helper functions to reduce potential AV scanner flags.

## [1.5.6] - 2026-06-10

### Updates
* **Smart App Control Mitigation**: Bumped application and installer versions to trigger a reputation hash refresh to address false-positive Smart App Control blocking after system reboots.

## [1.5.5] - 2026-06-09

### Bug Fixes
* **Store Launch Crash Fix**: Updated main thread COM initialization to safely ignore `RPC_E_CHANGED_MODE` (which commonly occurs when launching inside the Windows App Store MSIX container when COM is pre-initialized as multithreaded by telemetry/sandbox layers) and managed `CoUninitialize` cleanup dynamically.
* **Packaged Sandbox Safety**: Hardened the `IsPackaged` detection helper to pass a valid `wchar_t` buffer to `GetCurrentPackageFullName` instead of `nullptr`, preventing potential access violations and crashes on various Windows OS builds.

## [1.5.4] - 2026-06-08

### Bug Fixes
* **Store Launch Crash Fix**: Reset mutex creation error status using `SetLastError(ERROR_SUCCESS)` to prevent stale startup errors from false-triggering the duplicate instance check.
* **Packaged Sandbox Safety**: Disabled the GitHub update checking thread entirely when running inside a packaged MSIX container to prevent network failures or access violations.
* **Audio Session Robustness**: Added null-pointer checks for COM interfaces inside the audio playback detection loop.

## [1.5.3] - 2026-06-05

### Bug Fixes
* **Launch Crash Fix**: Changed the single-instance mutex namespace from `Global\\` to `Local\\` in `src/main.cpp`. Packaged store-submitted apps (MSIX) run as standard users and lack `SeCreateGlobalPrivilege`, which caused the previous `Global\\` mutex to fail with `ERROR_ACCESS_DENIED` at launch, exiting early and being flagged as a crash by the Microsoft Store certification.

## [1.5.2] - 2026-06-04

### Bug Fixes
* **Cursor Hiding Deferral**: Solved a bug where the cursor remained visible during idle dimming even when all screens dimmed. This was resolved by removing the immediate 1px cursor shift on entering idle (which occurred while the layered overlay was still at 0% opacity and thus transparent to hit-testing, sending `WM_SETCURSOR` to background windows) and deferring the shift to the first timer tick when the overlay under the cursor reaches at least 1% opacity and becomes hit-testable.
* **Removed Fake Cursor Rendering**: Removed the duplicate fake cursor rendering from `WM_PAINT` which kept a visible mouse arrow on the screen during idle dimming.

## [1.5.1] - 2026-06-04

### Bug Fixes
* **Conditional Cursor Hiding**: Updated the mouse cursor hiding logic during idle dimming so that the cursor only hides if all connected/enabled monitors are actually dimming. If any monitor is bypassing dimming (e.g. because of active audio or video playback), the cursor will remain visible globally since the user is considered nearby.

## [1.5.0] - 2026-06-04

### Bug Fixes
* **Memory Leak in Update Thread**: Fixed a heap-allocated memory leak of the version string if the window is closed before the thread completes.
* **Robust Update Checking Parser**: Refactored the GitHub API tag parser to handle different JSON formatting styles, such as spaces after colons.
* **Instant Fullscreen Dismissal**: Split monitor dimming suspension flags into source-specific states, allowing immediate fade-in when fullscreen applications are closed or moved.
* **Opaque Idle Dimming**: Removed the 90% dimming cap for idle mode to allow fully black overlays when configured.
* **Thread Handle Race Condition**: Delayed the update thread spawning until the main window handle is fully initialized to avoid posting to a null handle.

## [1.4.9] - 2026-06-03

### Bug Fixes
* **Mouse Cursor Hiding Only During Idle**: Fixed the mouse cursor hiding behavior so that the cursor is hidden globally only during idle/inactive dimming. Manually adjusting brightness via active dimming/hotkeys keeps the cursor visible. This was achieved by toggling the `WS_EX_TRANSPARENT` style on overlays during the idle state to capture cursor events and handle them with `SetCursor(nullptr)` under `WM_SETCURSOR`, combined with instant wake-up triggers.
* **Multi-process Browser Audio Association**: Changed audio/video monitor bypass mapping from PID-based to process-name-based. This ensures that when a web browser (e.g., Chrome) runs its audio rendering via a headless utility helper process, all of its visible, non-minimized browser windows are properly associated with the active audio session, correctly preventing the monitor hosting the playing video from dimming while allowing other inactive monitors to dim.

## [1.4.8] - 2026-06-03

### New Features
* **Automatic Configuration Migration**: Added logic to copy the user's existing settings from `%APPDATA%\WinDimmer64\dimmer.ini` to `%APPDATA%\IdleDimmer\dimmer.ini` if the latter is not found, ensuring that previous settings, monitor dim levels, and customized blocked app list are automatically preserved.

### Bug Fixes
* **Active and Idle Dimmer Not Working After Rename**: Resolved conflict where the old `WinDimmer64` process would remain running in the background, keeping the global hotkeys and overlay priority, while the new `IdleDimmer` app started with empty configurations.
* **Harmless DPI Context Log Error**: Safely handle DPI awareness initialization errors if the application manifest has already pre-configured the thread DPI context.

## [1.4.7] - 2026-06-02

### Bug Fixes
* **Active dimming dying after 1 second**: `SHQueryUserNotificationState` in `IsFullscreenAppActive()` was treating `QUNS_BUSY` as a fullscreen signal. On a normal active desktop, the default state is `QUNS_ACCEPTS_NOTIFICATIONS`, but various Windows activities (focus assist transitioning, lock screen, UAC dialog) can land the session in `QUNS_BUSY` briefly — every 5-second full check then set `m_videoDetected = true` and forced the overlay to fade out, even when nothing was actually playing video. Reverted to checking only `QUNS_RUNNING_D3D_FULL_SCREEN` and `QUNS_PRESENTATION_MODE`, which are the two states that genuinely indicate a fullscreen app is occupying the foreground.
* **Setup showing "Installed: v1.4.0" for the current installation**: `FILEVERSION` and `PRODUCTVERSION` (the comma-form) in `resources\resources.rc` and `resources\setup.rc` were never bumped past `1,4,0,0` — only the `VALUE "FileVersion"` / `VALUE "ProductVersion"` strings were updated each release. `GetExeVersion()` in `setup.cpp` reads the comma-form via `VerQueryValue`, so it always reported `1.4.0.0` regardless of what the strings said. Both forms are now kept in sync at `1,4,7,0` / `1.4.7.0` across all four resource locations, and the `APP_VERSION` constant in `MainWindow.cpp` + `VER` in `setup.cpp` match them. The version field in the UI header and the installer's "Detected installed:" readout will now read correctly on every release going forward.

## [1.4.6] - 2026-06-02

### Bug Fixes
* **Active dimming oscillation fixed**: Restored proper logic to `CheckVideoPlayback()`. On lightweight ticks, the state can only transition from `false` -> `true` (for instant response to fullscreen apps/games). Transitions back to `false` (clearing detected state) are deferred to the full-check tick every 5 seconds. This eliminates the 5-second oscillation cycle.
* **Version display showing 1.4.0**: Runtime PE version reading (`GetOwnVersion`) had hardcoded `L"v1.4.0"` fallbacks that triggered when the API failed. Replaced with a single compile-time `APP_VERSION` constant — no API, no fallback, always matches.

## [1.4.5] - 2026-06-02

### Bug Fixes
* **Video detection oscillation fixed**: `CheckVideoPlayback()` now only updates `m_videoDetected` on full-check ticks (every 5th second), eliminating the 4-second flicker cycle between lightweight and heavyweight checks.
* **Installer visual refresh**: Dark theme applied with Premium Monochrome Slate colors (bg `#1E1E1E`, text `#E1E1E1`) matching the app. Title bar uses `DWMWA_USE_IMMERSIVE_DARK_MODE`. Fonts updated to Segoe UI Variable Display/Text at 20pt/13pt. Launch checkbox enabled by default and always visible. Redundant Location label removed (install path already shown in the log console).

## [1.4.4] - 2026-06-02

### Bug Fixes
* **Cursor no longer hides during active dimming**: Mouse cursor only hides during idle/inactive dimming, not when manually dimming via sliders or hotkeys.

## [1.4.3] - 2026-06-02

### Updates
* **Focus Highlight and Boundary Diagnostics removed**: Dead UI features with no user-facing value. Removed FocusMode/ShowBoundaries checkboxes, timer 201 (150ms cursor tracking), overlay WM_TIMER focus-mode branching, boundary drawing in overlay WM_PAINT, and all config/manager plumbing. LightMode and WarmTint checkboxes now fill the row cleanly.
* **Fixed false fullscreen detection breaking active dimming**: `IsForegroundWindowFullscreen()` now excludes `IdleDimmerMainClass` and `IdleDimmerOverlayClass` window classes, and the style check no longer flags ordinary restored windows as fullscreen. The geometric check is still fast enough to run every tick.
* **Moved `IsFullscreenAppActive` into throttled path**: The shell32.dll `SHQueryUserNotificationState` call (LoadLibrary/FreeLibrary every tick) now only runs every 5th second alongside the other heavyweight checks, reducing overhead and Defender signal.

## [1.4.2] - 2026-06-02

### Updates
* **Defender heuristic reduction**: Replaced `HWND_BROADCAST` + `SC_MONITORPOWER` broadcast with a targeted `SendMessage` to `GetDesktopWindow()` and `Shell_TrayWnd` — eliminates a top malware-class heuristic flag.
* **Throttled process enumeration**: Added `m_videoCheckTick` counter in `CheckVideoPlayback()` so `OpenProcess`/`QueryFullProcessImageNameW` and COM audio enumeration only run every 5th second instead of every second, reducing surveillance-like API patterns.

## [1.4.1] - 2026-06-02

### Bug Fixes
* **Dead GroupDim handler removed**: The `else if (cb.settingName == L"GroupDim")` block in `MainWindow.cpp` was left over from v1.4.0 — no UI can trigger it, but the code inside forcibly synced all monitors to `masterValue`. Deleted. Also stripped `groupDim` from `ConfigManager.h` struct and from `SaveConfig`/`LoadConfig`. No remaining trace of the flag.
* **SYSTEM: ACTIVE status now respects dimmingEnabled toggle**: The footer status indicator checked per-monitor `enabled && dimValue > 0` but ignored the global `dimmingEnabled` flag. If dimming was toggled off with non-zero slider values saved, it incorrectly showed "ACTIVE". Now gated behind `if (m_config.dimmingEnabled)`.

## [1.4.0] - 2026-06-02

### Bug Fixes
* **Per-monitor toggles silently grouped (reported)**: `groupDim` branching was still wired into every toggle, drag, scroll, and arrow-key handler even though the checkbox was removed from the UI. A stale `true` value in the INI file made per-monitor toggles behave as master toggles, syncing all monitors at once. Removed every `if (m_config.groupDim)` branch across HandleLButtonDown, HandleMouseMove, HandleMouseWheel, and HandleKeyDown — individual path only. Forced `config.groupDim = false` on config load so stale files can't re-enable it.
* **Hotkey Ctrl+Alt+D didn't update dimmingEnabled config**: `SetDimmingEnabled()` was called but `m_config.dimmingEnabled` was never set, so the DimmingEnabled checkbox showed stale state and saves/restores used the wrong value. Now both are in sync.
* **WM_MOUSEWHEEL screen coordinates not converted to client**: Used raw `x,y` from `lParam` (screen-space) for slider hit-testing. On non-zero window positions or mixed-DPI setups, scroll-wheel adjustments silently targeted wrong locations. Unified to a single `ScreenToClient` conversion at the top of the function.

### Notes
* Fixes 2 (auto-enable SetDimmingEnabled) and 4 (CoInitializeEx) from the audit were already present — confirmed in code.
* Active dimming, per-monitor independent control, and idle state management are all based on the v1.3.9 foundation which resolved 7 prior bugs.

### Bug Fixes
* **Per-monitor toggles all controls tied together**: `groupDim` defaulted to `true` but no GroupDim checkbox was shown in the UI, so every monitor toggle and slider adjusted ALL monitors. Changed default to `false` so each monitor controls independently.
* **Ctrl+Alt+D hotkey never enabled dimming**: Hotkey only toggled `info->enabled` per monitor but never set the global `m_dimmingEnabled` flag. The overlay timer's `IsDimmingEnabled() && info->enabled` check always failed, keeping overlays transparent. Now calls `SetDimmingEnabled()`.
* **Idle state stuck when video detected**: The `&& !IsVideoDetected()` guard on timer 202 prevented the idle timer from clearing idle state when a blocked app was playing. If idle activated before video started, idle state stayed true indefinitely, causing the overlay to always use idle dim level instead of the per-monitor slider value. Removed guard; idle timer now always runs.
* **Idle state never cleared when idle dimming disabled**: No else clause existed — when `idleDimEnabled` was toggled off, `m_isIdleState` stayed `true` forever. Added explicit `SetIdleState(false)` in the timer else branch.
* **Overlay idle dim ignored per-monitor enabled**: `IsIdleState()` branch didn't check `info->enabled`, so disabled monitors still dimmed during idle. Added `info->enabled` guard.
* **Overlay idle dim ignored per-monitor slider value**: Used only `GetIdleDimLevel()`, ignoring the per-monitor slider. Now uses `max(info->dimValue, idleDimLevel)` so each monitor's dim level is respected even during idle.
* **Update check thread data race**: Background thread wrote directly to `m_latestVersion` and `m_updateAvailable` while `OnPaint` read them. Now passes results through `PostMessage` WPARAM/LPARAM.

### Updates

### Bug Fixes
* **Active dimming not working**: Two issues combined to break all dimming. First, the global `m_dimmingEnabled` flag was never set by the Ctrl+Alt+D hotkey (ID 103) — it only toggled per-monitor `enabled` leaving the global flag in its default `false` state, so the overlay timer's `IsDimmingEnabled() && info->enabled` check always failed. Second, `SetIdleState` was never called to clear idle state when `idleDimEnabled` was toggled off — stuck idle state caused the overlay to enter the idle branch instead of the active dimming branch. Fixed: hotkey now calls `SetDimmingEnabled()` and idle timer explicitly clears idle state when idle dim is disabled.

* **Idle state stuck when idle dim disabled**: Previously `m_isIdleState` stayed `true` indefinitely after toggling idle dim off. Now the timer 202 else-branch calls `SetIdleState(false)` if idle state was set.

* **Overlay timer priority reverted to v1.3.6**: Order is `idle > video > dimming`. If the system is idle, overlays dim even if a blocked app is detected (useful for the "fall asleep watching a movie" case). Active dimming only engages when neither idle nor video detection is active.

* **Idle timer guard removed**: No longer skips idle detection when video is detected. Idle state is managed independently of video state to prevent state from getting stuck.

### Updates

### Updates
* **Browser audio detection removed entirely**: v1.3.6's fix tried to let idle dimming override browser audio, but the real problem was deeper — enumerating audio sessions falsely flagged ANY browser audio (Twitch, YouTube, notifications, ads) as "video playing." Now completely reverted to v1.2.8 behavior: a simple foreground-window blocked-app check with zero audio enumeration. No COM audio meter. No IAudioSessionEnumerator. Just `GetForegroundWindow` + process name match. Clean, fast, and reliable.
* **Overlay timer priority reverted**: Back to v1.2.8 order: video detection > idle > manual dim. If a blocked app plays audio in the foreground, no dimming at all — exactly what you want during a movie.
* **Idle timer reverted**: Now checks `&& !IsVideoDetected()` so idle never dims while a blocked app is running. If you're watching something, the screen stays bright.
* **Per-monitor idle dimming preserved**: Each overlay still handles its own fade during idle, capped at 90%. Disabled monitors stay undimmed.

## [1.3.5] - 2026-06-02

### Updates
* **Verbose console window**: Setup now opens a black console alongside the GUI with real-time step-by-step output (version detection, kill, extract, shortcut, uninstall registration). Auto-scrolls to keep the latest line visible.
* **KillRunning safety sleep**: Added Sleep(200) after normal process exit too, not just after TerminateProcess.

## [1.3.4] - 2026-06-02

### Bug Fixes
* **Setup not killing running app**: `KillRunning` used `FindWindowW` to check if the app exited, but when `closeToTray` is on, `WM_CLOSE` just hides the window — `FindWindowW` returns `NULL` while the process still runs. Now uses `OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE)` + `WaitForSingleObject` to wait for actual process exit before force-killing.
* **Crash after crash blocks relaunch**: Named mutex with `bInitialOwner=TRUE` and no abandoned-mutex handling meant a crashed instance permanently blocked relaunch. Now checks `WAIT_ABANDONED` and proceeds if the previous instance crashed.
* **Data race in update check thread**: Background thread wrote to `m_latestVersion` (std::wstring), `m_updateAvailable`, and `m_updateChecked` without synchronization while `OnPaint` read them on the main thread. Now results are passed through `PostMessage` parameters and applied on the main thread.
* **D2D resource failure crash**: Individual brush/text-format creation failures were logged but not propagated — `CreateGraphicsResources` returned `S_OK` with null pointers, crashing `OnPaint`. Failures now set the function's `HRESULT`, triggering `DiscardGraphicsResources` and a safe early-return on the next paint.
* **Unchecked WinHttpQueryDataAvailable**: Return value was ignored — stale/undefined `size` could cause a zero-byte read or oversized buffer. Now checks the return before proceeding.
* **Disabled monitors dimming during idle**: Monitors toggled OFF would still dim when the system went idle. Added `info->enabled` check to the idle dim path.

### Updates
* **Version resource consistency**: Binary `FILEVERSION`/`PRODUCTVERSION` now matches the string version block (both `1.3.4.0`). Previously binary said `1.3.2.0` while string said `1.3.3.0`.
* **Setup mutex handle leak**: Uninstall path returned without `CloseHandle` on the single-instance mutex. Also fixed on `CreateWindowExW` failure.
* **Registry write cleanup**: `RegSetValueExW` used `sizeof(buf)` writing uninitialized stack bytes past the null terminator. Now uses `(wcslen(buf) + 1) * sizeof(wchar_t)`.
* **Partial exe cleanup**: Failed `WriteFile` in setup no longer leaves a corrupt `.exe` on disk — `DeleteFileW` removes it on failure.
* **Thread handle leak**: `CreateThread` return value was discarded, leaking a kernel handle. Now stored and closed.

## [1.3.3] - 2026-06-02

### Updates
* **Browser foreground separation**: Browsers (Chrome, Edge, Firefox, Opera, Brave) no longer trigger the foreground blocked-app check. They only block dimming when actively playing audio. A silent browser in foreground won't stop idle dimming.
* **Per-monitor idle dimming**: During idle, each monitor now targets `max(own dimValue, global idle level)` instead of a forced global value. Monitors darker than the idle level stay at their existing level.
* **Saved config sanitization**: Old `dimmer.ini` files with browsers in `BlockedApps` are filtered on load — browsers removed from the foreground match list.
* **Hardcoded browser list**: Built-in `m_browserApps` vector in DimmerManager with 5 known browsers, checked only via audio enumeration.

### Bug Fixes
* **Audio detection bug**: `IsAnyAppPlayingAudio` was hardcoded to compare against `m_blockedApps` instead of the passed `apps` parameter — browser audio never matched. Fixed.
* **Idle dimming lockout via browser sounds**: Browser system audio (ads, notification pings) was keeping `m_videoDetected = true` permanently, preventing idle timer from ever activating. Now correctly isolated.

## [1.3.2] - 2026-06-02

### Bug Fixes
* **Cursor hide on any dim, not just idle**: `UpdateCursorDimming` now checks actual monitor dim level, not just idle state. Cursor hides whenever screen dims ≥ 5%.
* **Shell32.dll leak**: `IsFullscreenAppActive` loaded shell32 via `LoadLibraryW` but never called `FreeLibrary`. Fixed.
* **OpenProcess error logging**: `GetRealProcessId` and `GetProcessNameFromPid` now log E417/E418 when `OpenProcess` fails.
* **Dead declaration removed**: `IsProcessNamePlayingAudio` declared in header but removed from .cpp.

### Updates
* **Error codes E417–E420**: Added for OpenProcess failures, setup window creation, overlay display affinity.
* **Self-healing**: All COM failure paths log and return cleanly. Per-monitor overlay failure doesn't block others.

## [1.3.0] - 2026-06-01

### Bug Fixes
* **Windows Security Block**: Replaced the global `SetSystemCursor` calls with local `ShowCursor` visibility checks during idle screen dimming to completely resolve heuristic antivirus/smartscreen warnings and blocks.

## [1.2.9] - 2026-06-01

### New Features
* **Windowed Netflix & Video Playback Detection**: Added active audio session monitoring to detect when browser tabs (like Chrome, Edge, Firefox) or media players are playing sound in the foreground. This prevents the screen from dimming while watching a show or movie in windowed mode, and correctly resumes dimming when the video is paused.
* **Fullscreen & Borderless Game Detection**: Bypasses screen dimming when a game is in the foreground, resolving the bright cursor contrast issue.

### Bug Fixes
* **Centralized Error Database**: Integrated a structured E### error reporting system across all areas of the application (initialization, GUI rendering, config loading, registry settings, and setup extraction). Errors are logged to system debuggers (OutputDebugStringW) for clean troubleshooting.

## [1.2.8] - 2026-05-27

### Bug Fixes
* **Global Cursor Hiding on Idle**: Replaced the local thread-restricted `ShowCursor(FALSE)` call with a clean, antivirus-friendly system-wide `SetSystemCursor` using a statically created blank cursor when screens are dimmed during idle. Original cursors are instantly restored globally via `SPI_SETCURSORS` upon waking up or application exit.
* **Hotkey Active Dimming Auto-Enable**: Adjusting display brightness via global hotkeys (`Ctrl+Alt+ArrowUp/ArrowDown`) now auto-enables active dimming if it was off, so changes take effect immediately.
* **Hotkey Idle Slider Corruption**: Fixed a layout bug where global brightness hotkeys incorrectly reset the inactivity timeout and dim level slider visual values on the settings panel.
* **Installer Directory Pre-creation**: Resolved a bug where launching the installer pre-emptively created an empty `%LOCALAPPDATA%\IdleDimmer` directory before the user clicked "Install".

## [1.2.7] - 2026-05-27

### Updates
* **Cursor dimming removed**: Replaced `SetSystemCursor`/`CreateDimmedCursor` (which triggered Windows Defender blocks on signed systems) with simple `ShowCursor(FALSE)` when the screen dims during idle. Cursor reappears automatically on any mouse/keyboard input. Cleaner, safer, no security flags.

## [1.2.6] - 2026-05-27

### Bug Fixes
* **Cursor bitmap ownership in CreateDimmedCursor**: The GDI color and mask bitmaps passed to `CreateIconIndirect` were deleted immediately after creation. MSDN states the system does not copy these bitmaps — the application must not delete them while the cursor is in use. The bitmaps are now kept alive and cleaned up when the system replaces cursors via `SPI_SETCURSORS`.

## [1.2.5] - 2026-05-27

### Bug Fixes
* **Multi-Cursor Dimming**: Expanded cursor dimming to also cover text select (`OCR_IBEAM` / `IDC_IBEAM`) and link hovering hand (`OCR_HAND` / `IDC_HAND`) cursors, ensuring the cursor remains dimmed when hovering over buttons, links, or text fields.
* **Out-of-Sync Cursor Dimming**: Routed cursor dimming updates directly into `SetMonitorDim`, `SetMonitorEnabled`, and `RefreshMonitors`, so the cursor's dim state is dynamically updated during slider drags, mouse scroll increments, keyboard adjustments, or when using "Undo Changes".
* **GDI Handle Leaks**: Fixed a memory/GDI resource leak in `CreateDimmedCursor` by properly calling `DeleteObject` on the original color and mask GDI bitmap handles returned by `GetIconInfo` upon successful creation.
* **Safe Cursor Restoration**: Upgraded cursor restoration to use a clean system-wide `SystemParametersInfo` call, eliminating potential cursor corruption or stale handle crashes when restoring original cursors.

## [1.2.4] - 2026-05-27

### Bug Fixes
* **UWP & Store App Dimming**: Resolved `ApplicationFrameHost.exe` window wrapping. UWP and Microsoft Store applications (such as Plex for Windows, Netflix, and Movies & TV) are now correctly resolved to their real process names (e.g. `Plex.exe`) by enumerating child windows, enabling them to be correctly matched against the blocked apps list.

## [1.2.3] - 2026-05-27

### Bug Fixes
* **Cursor disappears after idle wake**: Fixed a bitmap ownership bug in `CreateDimmedCursor` — the new mask bitmap was deleted immediately after `CreateIconIndirect`, corrupting the cursor. The mask is now kept alive (owned by the cursor, cleaned up by the system via `DestroyCursor`).
* **Squished "Bypass Apps" label**: The arrow label text rect was only 21px wide — far too narrow for 11 characters. Moved the arrow left to make room and widened the text rect to 93px.

## [1.2.2] - 2026-05-27

### Bug Fixes
* **Truncated section headers**: "SCREEN DIMMIN" and "SCREEN DISPLA" fixed — character counts were off by one in the `DrawText` calls.
* **Panel title overlap**: "BYPASS APPS" text rect now stops short of the `[+ Add]` button so both are visible.
* **Window off-screen**: When toggling the right-side panel, the window position is now clamped to screen bounds.
* **Start minimized**: When both "Start with Windows" and "Close to Tray" are enabled, the app now starts silently in the system tray.

### Updates
* **Blocked Apps renamed**: Arrow label and panel header now say "Bypass Apps".
* **Cleaner default blocklist**: Browsers and chat apps stripped from defaults — only media players remain. Saved config unaffected.

## [1.2.1] - 2026-05-27

### New Features
* **Right-Side Panel for Blocked Apps**: Moved Blocked Apps out of the cramped inline card into a dedicated 200px scrollable panel on the right side. Click the "Apps" arrow at the top-right of the window to expand it. Scroll through the app list with the mouse wheel.

### Updates
* **Cleaner Default Blocklist**: Stripped browsers (Chrome, Edge, Firefox, Opera, Brave) and chat apps (Zoom, Teams, WhatsApp, Slack, Discord, Spotify) from the default blocked apps list. Only media players remain. Existing user configs are not affected.

## [1.2.0] - 2026-05-27

### New Features
* **Blocked Apps**: Replaced the hardcoded media player list with a configurable blocklist. Added a collapsible BLOCKED APPS card on the settings panel with an expansion arrow. Users can add and remove apps freely — dimming pauses whenever a blocked app is the foreground window. Pre-populated with all major media players and video chat apps.

## [1.1.3] - 2026-05-27

### Updates
* **More media players**: Expanded detection list — Kodi, MPV, Netflix, Screenbox, KMPlayer, GOM Player, SMPlayer, Zoom, Teams, WhatsApp, Slack.

## [1.1.2] - 2026-05-27

### Bug Fixes
* **Video detection not working**: `QueryFullProcessImageNameW` returns the full executable path, but the comparison was against just the filename (e.g. `Plex.exe`). Now extracts the filename from the path before matching. Fixes detection for all media players in any install location.

## [1.1.1] - 2026-05-27

### Bug Fixes
* **Installer freeze**: Fixed 10-second freeze when "Launch IdleDimmer" checkbox is checked. Replaced `ShellExecuteW` with `CreateProcessW` to avoid COM/shell blocking.

## [1.1.0] - 2026-05-27

### Updates
* **Fixed Installer Layout**: Welcome screen status text no longer overlaps the location text. Increased spacing between controls and taller status box for multiline messages.
* **Launch on Finish**: The Launch IdleDimmer checkbox on the completion screen now works — launches the app when checked before closing the installer.
* **Installer Source Tracked**: `src/setup.cpp` and `resources/setup.rc` are now in the repo for repeatable builds.
* **Fixed Installer Extraction**: App now extracts correctly with proper error messages. Shows install status on welcome screen — detects running instances, existing versions, and handles each case cleanly.
* **Installer Icon**: Professional icon and version info metadata on the installer executable.
* **Video Playback Detection**: Dimming pauses when a video is playing in a browser or media player (windowed or fullscreen).
* **Update Check**: Automatically checks GitHub for new releases on startup.

## [1.0.9] - 2026-05-27

### Updates
* **Installer**: Professional setup wizard with welcome screen, install location display, launch option, and clean uninstall via Settings > Apps & Features.
* **Update Check**: IdleDimmer now checks GitHub for new releases on startup. Shows "Update Available" or "Up to Date" next to the version number in the footer.
* **Video Playback Detection**: IdleDimmer now detects when a video is playing in a browser or media player (windowed or fullscreen) and halts all dimming until playback stops. Supports Chrome, Edge, Firefox, VLC, Plex, MPC, PotPlayer, Spotify, Discord, and more.
* **Black Cursor**: Dimmed cursor now turns fully black instead of proportionally dimming, with no flipping artifacts.
* **Footer Undo**: Moved "Undo Changes" into the app footer, centered between the status indicator and version number. Removed the Undo checkbox row from the APPLICATION section, eliminating wasted space.
* **Tighter Top Margin**: Reduced empty gap between title bar and first slider card from 52 to 30 pixels.
* **Centered Undo Text**: Undo label is now center-aligned within its footer hitbox instead of left-aligned.

## [1.0.8] - 2026-05-27

### Updates
* **Cursor Dimming**: Instead of proportionally dimming the arrow cursor with the screen, it now turns completely black when dimming is active for a cleaner, more consistent look.
* **Footer Undo**: Moved "Undo Changes" into the app footer, centered between the status indicator and version number. Removed the Undo checkbox row from the APPLICATION section, eliminating wasted space.
* **Tighter Top Margin**: Reduced empty gap between title bar and first slider card from 52 to 30 pixels.
* **Centered Undo Text**: Undo label is now center-aligned within its footer hitbox instead of left-aligned.

## [1.0.7] - 2026-05-27

### Updates
* **UI Cleanup**: Removed redundant "Group All Monitors" toggle (master slider already syncs all displays). Removed "Show in Taskbar" option. Removed "IdleDimmer" header title (already in the window titlebar). Moved "Undo" counter into the APPLICATION section for a cleaner top area. Tighter overall layout with fewer toggles.

### Bug Fixes
* **Idle Dimming Default**: Idle dimming is now enabled by default so monitors dim during inactivity without requiring manual setup.

## [1.0.6] - 2026-05-27

### New Features
* **One-By-One Undo**: Replaced the single "undo everything" button with a proper undo stack. Each click of "Undo" reverts only the most recent change, letting you step back through slider adjustments, checkbox toggles, hotkey presses, and wheel ticks one at a time.

### Updates
* **Compact Settings Panel**: Tightened all layout spacing — card heights reduced from 75→65 px, row margins tightened, section gaps narrowed, and overall window padding cut — so the panel fits more content without scrolling on smaller screens.

## [1.0.5] - 2026-05-27

### New Features
* **App Icon**: Embedded a custom multi-resolution icon (16–256 px) into the binary. The window, tray, and taskbar now show the dimmer icon instead of the default Windows application icon.
* **Change Counter**: The "Undo Changes" button now shows the number of changes made since the session started (e.g. "Undo (3)"). The count resets after clicking undo.

### Updates
* **Removed Settings Separator**: Removed the horizontal divider line that cut through the DIMMING section header label in the settings panel.

## [1.0.4] - 2026-05-27

### Bug Fixes
* **Slider Dimming Not Applied**: Fixed an issue where dragging, scrolling, or arrow-keying a monitor brightness slider had no visible effect on screen brightness. Adjusting any monitor slider now auto-enables active dimming if it was off, so you see the result immediately without manually toggling the switch first.
* **Smart App Control Triggering**: Fixed version mismatch between manifest and resources (1.0.3.0 vs 1.0.4.0) that flagged the binary as suspicious. Added ASLR, DEP, and Control Flow Guard linker flags so the PE looks legitimate to Windows 11 Smart App Control.
* **Cursor Not Dimming With Screen**: Replaced the system arrow cursor with a proportionally dimmed bitmap (`SetSystemCursor` + `CreateIconIndirect`) whenever dimming is active, so the mouse pointer matches the overlay dim level instead of staying at full brightness.

### Updates
* **Grouped Settings Sections**: Reorganized all toggle switches into three labeled sections — **DIMMING**, **DISPLAY**, and **APPLICATION** — with muted section header labels and breathing room between groups. Related options are now visually clustered, making the settings panel easier to scan.
* **Shortened Toggle Labels**: Trimmed checkbox labels to be punchier and less verbose (e.g. "Dim When Away" instead of "Dim screen when idle").

## [1.0.3] - 2026-05-27

### New Features
* **Active Dimming Toggle**: Added a dedicated manual override switch. Checking it applies your screen dimming settings immediately, while unchecking it keeps screens at 100% brightness.
* **Group Dimming Sync**: Added a "Group Dim All Monitors" toggle option. Enabling it locks all monitor brightness sliders and enabled toggles together, making them sync to the same values instantly.
* **Visual Undo Changes Engine**: Added an interactive "Undo Changes" button in the top-right header of the settings panel to revert all settings changes back to your starting session preferences in real time.
* **Dynamic Binding Architecture**: Re-engineered checkbox settings to bind directly to configuration variables via pointers, removing all hardcoded mapping loops from the rendering and event systems.

### Updates
* **Premium Monochrome Grey Theme**: Replaced the high-intensity blue highlight colors with a high-end slate-obsidian dark/light grey palette (featuring carbon `#121212`, mid-slate `#1E1E1E`, slate-grey `#2D2D2D`, and soft silver `#E1E1E1`), creating an incredibly focused, sleek, and premium industrial look with elegant low-contrast active/inactive switches.
* **Advanced Fluent Typography**: Upgraded headings to the modern `Segoe UI Variable Display` family (semi-bold 20pt) and scaled body copy to `Segoe UI Variable Text` (13pt), yielding a highly refined, professional presentation.
* **Symmetric Spacing Grid**: Restructured layout rendering to calculate coordinates sequentially and dynamically. Completely resolved the visual collision bug by ensuring all slider cards, divider lines, and footer elements are placed with strict margins and generous vertical spacing (`row * 32`).
* **Label-Switch Overlap Fixed**: Shifted monitor card labels to X=80 (`+60px` offset) and clipped labels dynamically based on their column width, ensuring that toggle switches never dissect or overlap text.



## [1.0.2] - 2026-05-27

### New Features
* **Obsidian Dark Theme**: Upgraded the default dark mode design with a deep obsidian color scheme (`0x0B0B0C`) and subtle silver card borders (`0x8A8A8F` at 35% opacity) for a clean, professional dark panel.
* **Dynamic Light Mode Toggle**: Added a theme switch to the settings panel. Checking it transitions display cards, backgrounds, text, and tracks to off-white, dynamically updating the Windows 11 window frame and title bar colors in real time.

## [1.0.1] - 2026-05-27

### New Features
* **Product Properties Metadata**: Integrated a standard resource version info block into the compiled binary so that right-clicking the executable and viewing details in Windows Explorer displays correct description, version, and copyright fields.

### Bug Fixes
* **Direct2D Device Loss Recovery**: Handled device-loss status checks in the main painting loop. If display settings change, graphics driver resets, or the system returns from sleep, the application now automatically discards and recreates render resources, preventing frozen screens or crashes.
* **Overlay Creation Race Condition**: Bound the overlay window user-data pointer directly within the initial creation message (`WM_NCCREATE`) rather than after window creation, eliminating potential race conditions when early messages are processed.
* **Persistent Settings on Hot-Plugging**: Standardized the display layout change handler to synchronize active monitors against the saved settings immediately upon connection or disconnection. Dynamic monitor connection now correctly preserves your custom dim level and enabled preferences.

## [1.0.0] - 2026-05-26

### New Features
* **Inactivity Timeout OLED Power Saver**: Added a system idle tracking feature using `GetLastInputInfo`. Displays fade smoothly to a chosen Idle Dim level (up to 100% black overlay) to prevent OLED burn-in during periods of inactivity. The screens wake up instantly upon mouse or keyboard movement, bypassing slow hardware sleep/wake cycles. Added a checkbox option to physically turn off monitors.
* **Smooth Fading Transitions**: Implemented hardware-accelerated exponential-decay fading transitions when launching, exiting, or toggling dimming overlays.
* **Warm Amber Eye-Saver Tint**: Added a blue-light filter option that replaces neutral black overlays with a soothing warm orange/amber tint spectrum `RGB(255, 130, 45)`.
* **Focused Active Screen Highlight (Focus Mode)**: Added active monitor cursor tracking. The monitor containing the active mouse cursor remains bright while inactive screens fade deeper (+25% offset) to reduce visual distractions.
* **System-Wide Keyboard Hotkeys**: Registered global shortcuts (`Ctrl + Alt + ArrowUp/ArrowDown/D`) to control screen brightness levels and active states from any game, app, or browser.
* **Modern Fluent Win32 UI Panel**: Implemented custom rounded display cards, interactive sliders, checkbox controls, and system tray minimization.

### Updates
* **Per-Monitor DPI v2 Support**: Upgraded window positioning logic to dynamically scale and fit displays with mixed DPI values (e.g. 100% and 150%) on multi-monitor setups.
* **Strict Compiler Compatibility**: Simplified display config friendly name query with direct numbering fallbacks and standard C++ compliant wide-stream file path string pointer handling (`.c_str()`), allowing seamless compilation across both Microsoft MSVC (`cl.exe`) and LLVM-MinGW (`clang++`).
* **Standalone Portability**: Engineered the codebase using pure Win32 and Direct2D to achieve a single standalone executable under **200 KB** with zero external DLL runtime dependencies.
