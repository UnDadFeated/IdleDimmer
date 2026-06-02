# Changelog

All notable changes to the WinDimmer64 project are documented here.

## [1.3.6] - 2026-06-02

### Bug Fixes
* **Idle dimming stuck when browser audio detected**: Twitch/YouTube in a browser tab kept a live audio session → `m_videoDetected = true` → idle timer's `&& !IsVideoDetected()` guard skipped the entire idle check → dimming never activated. Fixed: idle timer now runs regardless of video detection, and the overlay timer gives idle state priority over video detection.
* **Netflix minimized / silent browser tabs blocking dim**: Same root cause — any browser audio session with peak > 0.0001f blocked idle dimming entirely. Now only affects non-idle (active) dimming; idle always overrides.

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
* **Installer Directory Pre-creation**: Resolved a bug where launching the installer pre-emptively created an empty `%LOCALAPPDATA%\WinDimmer64` directory before the user clicked "Install".

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
* **Installer freeze**: Fixed 10-second freeze when "Launch WinDimmer64" checkbox is checked. Replaced `ShellExecuteW` with `CreateProcessW` to avoid COM/shell blocking.

## [1.1.0] - 2026-05-27

### Updates
* **Fixed Installer Layout**: Welcome screen status text no longer overlaps the location text. Increased spacing between controls and taller status box for multiline messages.
* **Launch on Finish**: The Launch WinDimmer64 checkbox on the completion screen now works — launches the app when checked before closing the installer.
* **Installer Source Tracked**: `src/setup.cpp` and `resources/setup.rc` are now in the repo for repeatable builds.
* **Fixed Installer Extraction**: App now extracts correctly with proper error messages. Shows install status on welcome screen — detects running instances, existing versions, and handles each case cleanly.
* **Installer Icon**: Professional icon and version info metadata on the installer executable.
* **Video Playback Detection**: Dimming pauses when a video is playing in a browser or media player (windowed or fullscreen).
* **Update Check**: Automatically checks GitHub for new releases on startup.

## [1.0.9] - 2026-05-27

### Updates
* **Installer**: Professional setup wizard with welcome screen, install location display, launch option, and clean uninstall via Settings > Apps & Features.
* **Update Check**: WinDimmer64 now checks GitHub for new releases on startup. Shows "Update Available" or "Up to Date" next to the version number in the footer.
* **Video Playback Detection**: WinDimmer64 now detects when a video is playing in a browser or media player (windowed or fullscreen) and halts all dimming until playback stops. Supports Chrome, Edge, Firefox, VLC, Plex, MPC, PotPlayer, Spotify, Discord, and more.
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
* **UI Cleanup**: Removed redundant "Group All Monitors" toggle (master slider already syncs all displays). Removed "Show in Taskbar" option. Removed "WinDimmer64" header title (already in the window titlebar). Moved "Undo" counter into the APPLICATION section for a cleaner top area. Tighter overall layout with fewer toggles.

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
