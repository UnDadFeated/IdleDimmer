# WinDimmer64

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/MIT-6B7280?style=flat-square&label=License" alt="License: MIT"></a>
  <img src="https://img.shields.io/badge/1.4.1-10B981?style=flat-square&label=Version" alt="Version 1.4.1">

  <img src="https://img.shields.io/badge/Windows-0078D4?style=flat-square&logo=windows&logoColor=white&label=OS" alt="Windows">
  <img src="https://img.shields.io/badge/C%2B%2B17-00599C?style=flat-square&logo=c%2B%2B&logoColor=white&label=Language" alt="C++17">
</p>

A lightweight, modern, and high-performance screen dimming and OLED burn-in prevention utility designed specifically for Windows 11 (x64) in native C++.

Unlike the default Windows display options that only support physically turning off external monitors after idle periods, WinDimmer64 provides beautiful layered transparency overlays that dynamically dim screens to save energy and protect OLED displays, waking up instantly when user activity is detected.

---

## Installation

Download the latest installer from the [releases page](https://github.com/UnDadFeated/WinDimmer64/releases):

[**WinDimmer64-Setup-v1.4.1.exe**](https://github.com/UnDadFeated/WinDimmer64/releases/download/v1.4.1/WinDimmer64-Setup-v1.4.1.exe)

Run the installer — it will install WinDimmer64, create a Start Menu shortcut, and register with Settings > Apps & Features for clean uninstallation.

---

## Key Features

* **OLED Inactivity Power Saver (Idle Dimming)**: Detects system-wide inactivity using `GetLastInputInfo`. After a user-defined timeout (1 to 60 min), all displays smoothly fade to a selected Idle Dim Level (0% to 100% black overlay) to prevent OLED burn-in. Wakes up instantly on mouse or keyboard movement, bypassing slow hardware power-on delay cycles.
* **Luxurious Fading Transitions**: Features high-performance, hardware-accelerated exponential-decay animations that fade overlays smoothly during startup, exit, monitor changes, or settings toggles.
* **Eye-Saver Warm Amber Tint (Blue-Light Filter)**: Transforms standard neutral black dimming overlays into a curated orange/amber tint spectrum `RGB(255, 130, 45)` to actively block eye-straining high-energy blue light for comfortable night use.
* **Focused Active Screen Highlight (Focus Mode)**: Multi-monitor visual helper that tracks your mouse cursor and highlights your active screen by smoothly dimming inactive screens deeper (+25% dimming offset) to minimize distractions.
* **System-Wide Keyboard Hotkeys**: Adjust active brightness globally without opening the visual control panel:
  * `Ctrl + Alt + ArrowUp`   : Brighter (decreases dimming by 5%)
  * `Ctrl + Alt + ArrowDown` : Darker (increases dimming by 5%)
  * `Ctrl + Alt + D`         : Toggle all active screen dimmers on/off
* **Fluent Win32 UI & System Tray**: Custom-drawn Direct2D rounded monitor cards, checkbox toggles, and system tray integration (minimizes to tray, dynamic right-click settings menu).

---

## Specifications

* **Standalone Portable Executable**: Under **200 KB** in size.
* **Zero Runtime Dependencies**: No frameworks (WinUI, Electron, or ImGui), no bloated visual libraries. Built on pure Win32 and Direct2D/DirectWrite.
* **Mixed-DPI Multi-Monitor Support**: Perfectly scales and positions overlays across multiple monitors with different DPI scales using Per-Monitor DPI Awareness v2.

---

## Compilation

The project can be built using standard Windows compiler toolchains. An automated compilation script is provided:

### Prerequisites
* Windows 10 or 11 (x64)
* Visual Studio 2022 (MSVC compiler `cl.exe`) or **LLVM-MinGW** (based on `clang++`)

### Build Steps
1. Navigate to the root directory in a console.
2. Run the automated MSVC compilation script:
   ```cmd
   build.bat
   ```
3. If using LLVM-MinGW/Clang:
   ```cmd
   llvm-windres resources\resources.rc -O coff -o resources\resources.o
   clang++ -O2 -std=c++17 -mwindows -o WinDimmer64.exe src\main.cpp src\MainWindow.cpp src\DimmerManager.cpp src\ConfigManager.cpp resources\resources.o -lgdi32 -ld2d1 -ldwrite -ldwmapi -lole32 -luuid
   ```

---

## License

This project is licensed under the MIT License - see the LICENSE details for info.
