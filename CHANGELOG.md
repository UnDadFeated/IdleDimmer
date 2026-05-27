# Changelog

All notable changes to the WinDimmer64 project are documented here.

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
