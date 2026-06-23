# IdleDimmer

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-6B7280?style=flat-square" alt="MIT License"></a>
  <img src="https://img.shields.io/badge/Version-1.7.4-10B981?style=flat-square" alt="Version 1.7.4">
  <img src="https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D4?style=flat-square&logo=windows&logoColor=white" alt="Windows 10/11">
  <img src="https://img.shields.io/badge/C%2B%2B23-00599C?style=flat-square&logo=c%2B%2B&logoColor=white" alt="C++23">
</p>

**A tiny, native Windows screen dimmer that protects OLED screens from burn-in, respects your eyes, and values your privacy.**

IdleDimmer is a per-monitor screen dimming utility for Windows 10 and 11. It overlays a transparent black layer on each display you choose to reduce eye strain and protect OLED panels from static burn-in. You can dim a second screen to 30% for late-night reading while keeping your main monitor at 80% — without flickering the OSD, fighting DDC, or installing a 50 MB Electron wrapper.

Built in native C++23 with Direct2D and the raw Win32 API. **~150 KB on disk. Zero runtime dependencies. Zero telemetry. Zero ads.**

---

## Download

<table>
<tr>
<td width="50%" valign="top">

### Direct installer (GitHub Releases)

[**IdleDimmer-Setup-v1.7.4.exe**](https://github.com/UnDadFeated/IdleDimmer/releases/download/v1.7.4/IdleDimmer-Setup-v1.7.4.exe) — 280 KB

Per-user install to `%LOCALAPPDATA%`. No admin elevation. Clean uninstall from Settings > Apps. Self-extracting installer with dark theme UI.

</td>
<td width="50%" valign="top">

### Microsoft Store

[**Get it from Microsoft Store →**](https://apps.microsoft.com/)

Desktop Bridge MSIX package. Auto-updates. Sandbox-friendly. Free.

</td>
</tr>
</table>

---

---

## Features

### Per-monitor dimming with independent controls
Each connected display gets its own on/off toggle and a 0–90% dim slider. Drag a slider to auto-enable dimming for that monitor. Disable one monitor and leave the rest at full brightness — no group lock-in, no "all-or-nothing" mode.

### Idle detection with smooth fade (OLED Protection)
Set a timeout (1–60 minutes) and an idle dim level (up to 100% black). When the system reports no keyboard or mouse input, the overlays fade in to prevent static element burn-in on OLED displays. When activity resumes, they fade out. The cursor auto-hides during idle dimming and reappears the moment you touch the mouse.

### Blocked-app bypass
Add any process to the blocked list (Steam, MPC-HC, mpv, Plex, your TV app — whatever you use for video). When a blocked app is in the foreground **or** is playing audio through the default audio endpoint, dimming is automatically suspended so you never get a darkened video.

### Fullscreen detection
When Direct3D runs fullscreen (games) or a presentation app takes over the shell, dimming fades out. Detection is throttled to a 5-second cadence to keep CPU and Windows Defender happy.

### Warm tint and light mode overlays
Two optional cosmetic overlays for low-light sessions: a soft warm tint (`RGB(255, 130, 45)`) that filters blue light, and a monochrome "light mode" wash. Both are off by default — the default dim is a neutral black.

### Auto-start with Windows (optional)
One-click toggle in the settings panel to register a `HKCU\...\Run` entry. No scheduled tasks, no services, no startup folder pollution.

### In-app update check (optional)
A single "Check for Updates" button pings GitHub's public releases API to compare your version against the latest. No background updater, no auto-install, no elevated-process tricks. Off by default.

### Clean uninstall
Settings, Run-key entry, Start Menu shortcut — all removed. No leftover files, no leftover registry keys, no leftover services.

### Time-of-day scheduling
Set a start and end time for automatic dimming. The scheduler uses two compact sliders (15-minute increments) for quick setup — default is 22:00 to 07:00 for overnight dimming. Toggle it on and forget it.

### On-screen display (OSD) feedback
Every config change triggers a sleek, click-through notification at the bottom-right of your primary monitor — showing what changed without stealing focus or intercepting input.

### Preset profiles — customize and save
Four preset buttons on the settings panel: **[OLED]**, **[Gaming]**, **[Reading]**, **[Night]**. Click any preset to instantly apply its configuration:
- **OLED**: True-black dimming (master 90%), no warm tint — perfect for OLED panels
- **Gaming**: No dimming, vibrant display — ready for gaming
- **Reading**: Low dim + warm amber tint — easy on the eyes
- **Night**: Heavy dim + warm amber — late-night sessions

**All presets are saved on exit.** Customize any preset by clicking it, tweaking the settings, and the app saves your configuration automatically — every slider change, toggle, and checkbox is written to disk immediately.

### Profile import / export
Save your entire config as a portable `.ini` file and share it across machines. Import a previously exported profile with one click — no manual file editing needed.

---

## Designed to stay out of your way

- **Tray-resident.** Closing the settings window leaves a single tray icon. No background window, no persistent notification.
- **One process, ~3 MB working set.** Verified idle.
- **No driver, no kernel component, no service.** Standard user-mode Win32 application.
- **Single-instance via a named mutex.** Launching the EXE twice just brings up the existing window.
- **Defender-friendly.** Heavy API calls (process enumeration, audio session polling, shell32 notification state) are throttled to a 5-second cadence. No `SetSystemCursor`, no `CreateToolhelp32Snapshot`, no `HWND_BROADCAST` power messages.

---

## Specifications

| | |
|---|---|
| **Executable size** | 150 KB (standalone), 280 KB (installer), 140 KB (MSIX) |
| **Config file size** | < 1 KB at `%APPDATA%\IdleDimmer\dimmer.ini` |
| **Memory footprint** | ~3 MB working set at idle |
| **Disk footprint** | 150 KB exe + 1 KB config |
| **Runtime dependencies** | None. Statically linked against Windows API. |
| **Install location** | `%LOCALAPPDATA%\Programs\IdleDimmer\` (or per-user MSIX location) |
| **Permissions used** | Standard user. No UAC prompts, no admin elevation. |
| **Supported OS** | Windows 10 1809+ and Windows 11 |
| **Architecture** | x64 |

---

## Privacy

The app collects **no personal data, no telemetry, and no analytics**. The only outbound network request is an optional manual "Check for Updates" call to GitHub's public API, which contains no identifying information.

Full policy: [docs/privacy.html](https://undadfeated.github.io/IdleDimmer/privacy.html)

Source is open. You can audit every line: [src/](src/)

---

## Building from source

### Prerequisites
- Windows 10 or 11 (x64)
- **Visual Studio 2022** (MSVC `cl.exe` + `rc.exe`), or
- **LLVM-MinGW** (`clang++` + `llvm-windres`)

### MSVC (one command)
```cmd
build.bat
```
Auto-detects VS 2022, compiles resources, links the app.

### LLVM-MinGW
```cmd
llvm-windres resources\resources.rc -O coff -o resources\resources.o
clang++ -O2 -std=c++23 -mwindows -Os -s -mguard=cf -fms-extensions -static ^
    -o IdleDimmer.exe ^
    src\main.cpp src\MainWindow.cpp src\MainWindowDraw.cpp src\MainWindowInput.cpp src\DimmerManager.cpp src\ConfigManager.cpp ^
    resources\resources.o ^
    -lgdi32 -ld2d1 -ldwrite -ldwmapi -lole32 -luuid -lwinhttp -lversion -lcomdlg32 ^
    -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va -Wl,--subsystem,windows
```

### Installer
```cmd
llvm-windres resources\setup.rc -O coff -o resources\setup_res.o
clang++ -O2 -std=c++23 -mwindows -Os -s -mguard=cf -static ^
    -o IdleDimmer-Setup-v1.7.4.exe ^
    src\setup.cpp resources\setup_res.o ^
    -lole32 -lshell32 -ladvapi32 -luuid -lcomctl32 -lversion ^
    -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va -Wl,--subsystem,windows
```

### Microsoft Store MSIX
```cmd
msix\build_msix.bat
```
Requires Windows 10 SDK 10.0.22621 (`winget install --id=Microsoft.WindowsSDK.10.0.22621 -e --silent`).

---

## Architecture

| Component | File | Purpose |
|---|---|---|
| Entry point | `src/main.cpp` | COM init, Per-Monitor DPI v2, single-instance mutex, message loop |
| Settings UI | `src/MainWindow.cpp` | D2D-rendered settings panel, slider cards, toggle switches |
| Overlay manager | `src/DimmerManager.cpp` | Per-monitor layered overlay windows, fade animation, video detection |
| Config I/O | `src/ConfigManager.cpp` | JSON-like config at `%APPDATA%\IdleDimmer\dimmer.ini` |
| Installer | `src/setup.cpp` | Self-extracting installer with dark DWM theme |

Window classes: `IdleDimmerMainClass`, `IdleDimmerOverlayClass`, `IdleDimmerSetupClass`.

Overlay style: `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`.

---

## For AI agents and contributors

See [AGENTS.md](AGENTS.md) for the full project rules, build commands, removed features, Defender heuristics, theme tokens, layout rules, and headless release-publishing procedure.

---

## License

MIT License. See [LICENSE](LICENSE).

Copyright © 2026 UnDadFeated.
