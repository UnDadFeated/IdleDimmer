# Microsoft Store Certification History

Track every certification submission, the devices Microsoft tested on, OS builds, and what was fixed.

## Submission #10 — v1.7.1 (submitted 2026-06-19)

**Status:** Pending

**Fixes Applied:**
- Moved synchronous WASAPI/COM audio session checks from the UI thread to a background worker thread to resolve startup hangs/DWM ghosting in VM environments with no default audio devices.

## Submission #9 — v1.7.0 (submitted 2026-06-18)

**Status:** Pending

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
