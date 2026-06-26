# Microsoft Store Certification History

Track every certification submission, the devices Microsoft tested on, OS builds, and what was fixed.

## Submission #18 — v1.7.9 (submitted 2026-06-26)

**Status:** Pending

**Tested On:** TBD

**Fixes Applied:**
- **Fixed MSVC compiler error C2712 / MinGW runtime crash on audio check thread**: Separated the structured exception handling (`__try`/`__except`) wrapper from the C++ WASAPI/COM audio enumerator logic inside the background thread. Passing C++ objects like `std::vector` and using `ComPtr` within a function that contains structured exception handling blocks causes compiler errors under MSVC, and unstable unwinding behavior (leading to crashes at launch) under LLVM-MinGW.

## Submission #17 — v1.7.8 (submitted 2026-06-25)

**Status:** ❌ FAILED — Crash at launch on Dell Inspiron 5379 (build 26200.7840)

**Tested On:** Dell Inspiron 5379 (OS build 26200.7840)

**Fixes Applied:**
- **Moved EnumWindows off detached audio thread**: `GetMonitorsForProcessName` (which calls `EnumWindows`) was being called from a background thread with no message pump. `EnumWindows` from a pumpless thread can deadlock or crash if any enumerated window has a hung message handler. Now the background thread only does lightweight COM audio enumeration (get PIDs + peak values), storing process names. The main thread (timer 202) resolves process names → monitors via `EnumWindows`.
- **SEH around audio thread COM work**: The detached audio check thread had zero crash protection. Any SEH (access violation from COM/WASAPI calls, invalid audio device state) would propagate to `SetUnhandledExceptionFilter` → `ExitProcess(0)`, making the app vanish. Now wrapped in `__try/__except` via a static helper function (SEH can't coexist with C++ destructors in the same function).
- **try/catch around audio thread body**: C++ exceptions (`bad_alloc`, etc.) from vector/wstring operations in the thread are now caught.
- **OSD WndProc SEH protection**: The OSD window procedure (D2D rendering, fade timer) was the only WndProc without `__try/__except`. A D2D crash during OSD painting would miss `ValidateRect`, potentially causing an infinite repaint loop.

## Submission #16 — v1.7.7 (submitted 2026-06-25)

**Status:** ❌ FAILED — Crash "a while after launch" on Surface Laptop 5 (build 22631.4317)

**Tested On:** Microsoft Surface Laptop (OS build 22631.4317 — Windows 11 23H2)

**Root Cause for Failure:** Detached audio check thread (`CheckAudioPlaybackAsync`) had three problems:
1. Called `EnumWindows` from a thread without a message pump — can deadlock/crash
2. No SEH protection — COM/WASAPI access violations killed the process
3. No C++ exception handling — `bad_alloc` from vector operations propagated uncaught

**Fixes Applied (v1.7.7):**
- **Fixed stale binary FILEVERSION**: `FILEVERSION`/`PRODUCTVERSION` in `resources.rc` and `setup.rc` were stuck at `1,7,4,0` since v1.7.4. Only the string block was bumped each release. The MSIX manifest said `1.7.6.0` but the PE binary said `1.7.4.0` — version mismatch can cause Windows to reject/crash the app at launch.
- **Top-level SEH around entire WndProc**: Previously only `WM_PAINT` was wrapped in `__try/__except`. All other handlers (WM_APP+2 deferred init, WM_TIMER, WM_DISPLAYCHANGE, WM_POWERBROADCAST) propagated crashes uncaught. Now a single `__try/__except` covers the entire `WndProcImpl`.
- **C++ try/catch around WM_APP+2 deferred init**: `RefreshMonitors`, `SyncMonitorsWithConfig`, `SetWarmTint`, `SetDimmingEnabled` now have C++ exception safety. On failure, the minimal fallback ensures timers + tray icon are still set up.
- **OverlayWndProc SEH protection**: Overlay fade timer, paint, and positioning handlers wrapped in `__try/__except`.
- **SetUnhandledExceptionFilter backstop**: Last-resort crash handler in WinMain writes a crash marker to `%APPDATA%\IdleDimmer\crash.log` and exits with code 0 (no Watson dialog).

## Submission #15 — v1.7.6 (submitted 2026-06-24)

**Status:** ❌ FAILED — Crash at launch on Surface Laptop 5 (build 26200.8328)

**Fixes Applied (v1.7.5 → v1.7.6):**
- Message loop wrapped in `__try/__except` to catch any SEH from any message handler
- Removed blocking `MessageBoxW` on startup failure (blocks forever on headless cert VMs)

**Root Cause for Failure:** Two bugs found post-submission:
1. Stale binary `FILEVERSION 1,7,4,0` in resources.rc/setup.rc — MSIX manifest vs PE version mismatch
2. `WM_APP+2` deferred init handler had no crash protection — D2D/overlay crash during deferred startup killed the process

## Submission #14 — v1.7.5 (submitted 2026-06-23)

**Status:** ❌ FAILED — Crash at launch on Surface Laptop 5 (build 26200.8328)

## Submission #13 — v1.7.4 (submitted 2026-06-23)

**Status:** ❌ FAILED — Crash at launch on Surface Laptop 5 (build 26200.8328)
**Failure Mode Changed:** v1.7.2 and v1.7.3 failed with "hang" (Event ID 1002). v1.7.4's GDI-first approach fixed the hang but a D2D driver SEH access violation on Intel Iris Xe killed the process.

**Fixes Applied (v1.7.4 → v1.7.5):**
- The D2D driver crash is now caught by `__try/__except` — app falls back to permanent GDI mode instead of terminating.

## Submission #12 — v1.7.3 (submitted 2026-06-23)

**Status:** ❌ FAILED — Freeze at launch on Dell Inspiron 12-5280 (build 26200.8246)

**Tested On:** Dell Inspiron 12-5280 (OS build 26200.8246)

**Fixes Applied:**
- Deferred all blocking startup operations (`RefreshMonitors`, `AddTrayIcon`, `SetWarmTint`/`SetDimmingEnabled`, `SetTimer`, `ShowWindow`, `UpdateLayout`) to a `WM_APP+2` handler that runs on the first message-loop iteration. This prevents the GPU driver / DWM / Shell_NotifyIcon from hanging when called synchronously before the message pump on Win11 24H2+ (WDDM 3.2) certification VMs.

## Submission #11 — v1.7.2 (submitted 2026-06-22)

**Status:** ❌ FAILED — Freeze at launch on Dell Inspiron 12-5280 (build 26200.8246)

**Fixes Applied:**
- Moved synchronous WASAPI/COM audio session checks from the UI thread to a background worker thread to resolve startup hangs/DWM ghosting in VM environments with no default audio devices.

## Submission #9 — v1.7.0 (submitted 2026-06-18)

**Status:** ❌ FAILED — Hang after launch (DWM ghost window)  
**Tested On:** Microsoft Surface Pro 8

**Fixes Applied:**
- First-launch config init: set `m_config` fields directly instead of calling `ApplyPreset()` (triggers `DimmerManager` calls before init)
- Wrapped entire `SaveConfig` in `try/catch(...)` — prevents any unhandled C++ exception during startup file I/O
- Added top-level C++ `try/catch(...)` around `CreateImpl()` — `__try/__except` only catches SEH, not C++ exceptions
- `WinMain` returns 0 on failure + shows `MessageBox` (exit code 1 = crash to cert tool)
- Added `comdlg32.lib` link pragma for MSVC build compatibility

---

## Submission #7 — v1.6.6 (submitted 2026-06-18)

**Status:** ❌ FAILED — Crash at launch  
**Failure:** 10.1.2.10 Functionality — crashes at launch  
**OS Build:** 26200.8655  
**Tested On:**
- Microsoft Surface Laptop 5
- Dell Inspiron 13-5379

**Other device listed:** Lenovo Thinkpad 450s

---

## Submission #6 — v1.6.5 (submitted ~2026-06-18)

**Status:** ❌ FAILED (assumed, version bumped to 1.6.6 same day)

---

## Submission #5 — v1.6.4 (submitted ~2026-06-18)

**Status:** ❌ FAILED  
**Fixes Applied:**
- Wrapped `std::locale("")` in try/catch for `std::runtime_error` (stripped Windows images)
- Moved `UpdateLayout()` after `ShowWindow()`/`UpdateWindow()` in `CreateImpl()`

---

## Submission #4 — v1.6.3 (submitted 2026-06-17)

**Status:** ❌ FAILED  
**Fixes Applied:**
- Font fallback from `Segoe UI Variable` → `Segoe UI`
- Config file locale imbue (`std::locale("")`)

---

## Submission #3 — v1.6.2 (submitted 2026-06-16)

**Status:** ❌ FAILED  
**OS Build:** 22631.4317  
**Tested On:** Microsoft Surface Laptop 5  
**Fixes Applied:**
- `-static` linking (resolved missing DLL 0xc000007b)
- Dynamic `dwmapi.dll` load in setup.cpp
- SHGetFolderPathW return value checking

---

## Submission #2 — v1.6.1 (submitted 2026-06-16)

**Status:** ❌ FAILED  
**Fixes Applied:**
- Changed WaitForSingleObject from INFINITE to 3000ms timeout

---

## Submission #1 — v1.6.0 (submitted 2026-06-16)

**Status:** ❌ FAILED  
**OS Build:** 26200.8457  
**Fixes Applied:**
- PE subsystem version set to 10.0
- MaxVersionTested bumped to 10.0.26200.0
- Delayed update check by 15 seconds
- WinHTTP session handle race fix
- Removed legacy WinDimmer64 migration code

---

## Notes

- Microsoft tests on **different devices each time** — the test pool rotates.
- All failures have been "crash at launch" (10.1.2.10 Functionality).
- The cert VMs may be stripped-down Windows images with:
  - Missing optional fonts (Segoe UI Variable)
  - Missing locale DLLs
  - No audio devices
  - No external monitors (single-display Surface/Dell laptops)
  - Sandboxed MSIX container environment
  - Possible older or newer graphics drivers
