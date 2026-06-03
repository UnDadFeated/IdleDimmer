# WinDimmer64 — Agent Guide

## Build

- **MSVC**: `build.bat` (auto-detects VS 2022, runs `rc.exe` + `cl.exe`)
- **LLVM-MinGW** (always rebuild `resources.o` and `setup_res.o`):
  ```cmd
  llvm-windres resources\resources.rc -O coff -o resources\resources.o
  clang++ -O2 -std=c++17 -mwindows -Os -s -mguard=cf -o WinDimmer64.exe src\main.cpp src\MainWindow.cpp src\DimmerManager.cpp src\ConfigManager.cpp resources\resources.o -lgdi32 -ld2d1 -ldwrite -ldwmapi -lole32 -luuid -lwinhttp -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va
  ```
- **Setup** (requires `WinDimmer64.exe` in project root — rebuild resources *after* app exe to embed the correct version):
  ```cmd
  llvm-windres resources\setup.rc -O coff -o resources\setup_res.o
  clang++ -O2 -std=c++17 -mwindows -Os -s -mguard=cf -o WinDimmer64-Setup-v1.4.3.exe src\setup.cpp resources\setup_res.o -lole32 -lshell32 -ladvapi32 -luuid -lcomctl32 -lversion -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va

- Output: `WinDimmer64.exe` (~100 KB), `WinDimmer64-Setup-v1.4.3.exe` (~200 KB with embedded exe)

## Architecture

- **Singletons**: `MainWindow::Instance()` (settings panel), `DimmerManager::Instance()` (overlays)
- **4 source files** in `src/`:
  - `main.cpp` — entry, COM init, Per-Monitor DPI v2, single-instance mutex (`Global\WinDimmer64Mutex`), message loop
  - `MainWindow.*` — D2D-rendered settings panel with slider cards and toggle switches
  - `DimmerManager.*` — per-monitor layered overlay windows, fade animation (16ms timer)
  - `ConfigManager.*` — config I/O (simple JSON scanner, **not** a full JSON parser)
- **Window classes**: `WinDimmer64MainClass` / `WinDimmer64OverlayClass`
- **Overlay style**: `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`
- **Config file**: `%APPDATA%\WinDimmer64\dimmer.ini` (JSON-like, hand-parsed — no external parser library)

## Timer & Hotkey IDs

| ID | Purpose | Interval |
|---|---|---|
| 201 | Focus Mode cursor tracking | 150 ms |
| 202 | System idle detection (`GetLastInputInfo`) | 1000 ms |

| ID | Hotkey |
|---|---|
| 101 | Ctrl+Alt+ArrowUp (brighter -5%) |
| 102 | Ctrl+Alt+ArrowDown (darker +5%) |
| 103 | Ctrl+Alt+D (toggle dimming) |

## Value Ranges

- Monitor dim: `0`–`90` (slider `value / 90.0f` → float `0.0`–`1.0`)
- Idle dim level: `0`–`100` (slider `value / 100.0f`)
- Idle timeout: `1`–`60` minutes (slider `(value - 1) / 59.0f`)

## Layout Rules (preserve these)

- **Sequential Y**: `yOffset = last_element.rect.bottom + margin` — no hardcoded offsets like `+= 90`
- **Label X**: Monitor slider labels at `slider.rect.left + 60.0f` (X=80) to avoid 34×18 toggle switches cutting through text. Idle sliders (no toggle) use `+20.0f`.
- **Checkbox text clipping**: Column 1 clipped at `m_windowWidth / 2.0f - 10.0f`, Column 2 at `m_windowWidth - 20.0f`
- **Window height**: calculated dynamically in `UpdateLayout()` — `m_windowHeight = yOffset + 55`

## Theme (Premium Monochrome Slate)

| Token | Dark | Light |
|---|---|---|
| Bg | `#121212` | `#F0F0F2` |
| Card | `#1E1E1E` | `#FFFFFF` |
| Card border | `#2D2D2D` | `#E0E0E0` |
| Active track | `#8E8E93` | `#7A7A7E` |
| Active knob/text | `#E1E1E1` | `#1F1F1F` |
| Inactive track | `#2D2D2D` | `#E0E0E2` |
| Inactive knob/muted | `#808080` | `#757575` |
| Accent hover | `#C7C7CC` | `#555558` |

## Typography

- **Title**: `Segoe UI Variable Display`, semi-bold 20pt
- **Body**: `Segoe UI Variable Text`, 13pt
- **Detail**: `Segoe UI Variable Text`, 10.5pt

## Key Behaviors

- **Startup**: `dimmingEnabled` defaults to `false` — never auto-dim on launch
- **Per-monitor**: Each monitor has its own toggle and slider — no group mode (groupDim removed in v1.4.0)
- **Auto-enable**: Dragging/arrowing/scrolling a monitor slider auto-enables `dimmingEnabled` if it was off
- **Undo**: Session-start config snapshot (`m_backupConfig`); "Undo Changes" button in top-right header restores it
- **Device loss**: `D2DERR_RECREATED` → discard + recreate graphics resources (`MainWindow.cpp:626-629`)
- **Hot-plug**: `WM_DISPLAYCHANGE` triggers `RefreshMonitors()` → rebuild overlays, preserve saved per-monitor values

## Changelog

Write in natural human style in CHANGELOG.md. No AI boilerplate/buzzwords. Sections: **Updates**, **Bug Fixes**, **New Features**. Keep descriptions direct and punchy. Release notes go directly to `gh release create --notes "..."` — no separate whatsnew.txt file in the repo.

## Gotchas

- `std::wifstream` with `.c_str()` not `std::wstring` — MinGW may fail otherwise (`ConfigManager.cpp:38,121`)
- Resource manifest: `1 24 "manifest.xml"` — standard type 24 for MSVC + MinGW compat
- `m_pRenderTarget->SetColor()` is called each `OnPaint()` to switch theme — brushes are created once, colors swapped at render time

## Releases & Headless Publishing

To publish a release to the GitHub web interface when local authentication for the GitHub CLI (`gh`) is unavailable (e.g. inside a headless sandbox or build pipeline), you can leverage the local Git Credential Manager token:

1. **Retrieve the token**: Query the Git Credential helper to get your active GitHub OAuth/PAT token:
   ```cmd
   "protocol=https`nhost=github.com`n" | git credential fill
   ```
2. **Execute Headless Release**: Set the token to the `GH_TOKEN` environment variable so `gh` uses it directly, bypassing scope verification login limits, and run the release create command:
   ```cmd
   $env:GH_TOKEN="<retrieved_token>"
    gh release create v1.2.8 WinDimmer64-Setup-v1.2.8.exe --title "v1.2.8" --notes "Release notes go here"
   ```
