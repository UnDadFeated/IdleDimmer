# Changelog

All notable changes to the WinDimmer64 project are documented here.

## [1.0.3] - 2026-05-27

### New Features
* **Visual Undo Changes Engine**: Added an interactive "Undo Changes" button in the top-right header of the settings panel. If you modify any sliders or checkboxes, clicking it instantly reverts all monitors, theme settings, timeouts, and options back to your starting session preferences in real time.
* **Dynamic Binding Architecture**: Re-engineered checkbox settings to bind directly to configuration variables via pointers, removing all hardcoded mapping loops from the rendering and event systems.

### Updates
* **Visual Spacing & Alignment**: Solved slider label overlapping with monitor selection checkmarks by dynamically shifting text columns (+48px offset). Shifted column spacing of settings checkmarks to X=260 to provide a symmetrical, professional layout grid.
* **Safe Launch Defaults**: Set the initial startup dimming level to 0% to ensure that first-time launches do not modify active display brightness levels before you decide to move the master slider.

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
