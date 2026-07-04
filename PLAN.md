# Plan: Simplify Video Detection to Primary Monitor Only

## Problem
User was watching Netflix in windowed mode on Chrome, and it dimmed after 5 mins while the video was playing. The current video detection logic checks all monitors for audio/video association, but this is complex and may not correctly identify windowed video apps (Netflix, YouTube, Plex, Amazon Prime Video in Chrome/Edge).

## Solution
Simplify the app to:
1. **Idle Timer**: Continue using system-wide `GetLastInputInfo` (already global, no per-monitor idle detection needed).
2. **Video Detection**: Only monitor the **primary monitor** for video playing inside browser windows or media apps. 
   - Still show and dim all other monitors normally.
   - Only check the primary monitor's windows for video playback (Netflix, YouTube, Plex, Amazon Prime Video in Chrome, Edge, Firefox, etc.).
   - If a video-playing app is detected on the primary monitor, bypass dimming for that monitor (or globally if video is active on primary).

## Changes Required

### 1. `DimmerManager.h`
- Add method to get primary monitor info or monitor ID for primary display.
- Ensure video detection logic only checks windows on the primary monitor.

### 2. `DimmerManager.cpp`
- In `CheckAudioPlaybackAsync()` or video detection logic, filter windows to only those on the primary monitor.
- Use `MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)` and compare with primary monitor handle or ID.
- Simplify the per-monitor video bypass logic to focus only on primary monitor for video detection.

### 3. Version Bump
- Update version from 1.9.8 to 1.9.9
- Update `resources/resources.rc`, `resources/manifest.xml`, `msix/Package.appxmanifest`, `msix/build_msix.bat`

### 4. Build & Release
- Build `IdleDimmer.exe`
- Build `dist\IdleDimmer_1.9.9.0_x64.msix`
- Commit changes, push to git
- Create GitHub release v1.9.9 (MSIX only via Store, no setup exe)

## Implementation Steps

1. **Identify Primary Monitor**: Add helper to get primary monitor ID or `HMONITOR`.
2. **Filter Video Detection**: In audio/video playback check, only associate video playback with windows on the primary monitor.
3. **Update Dimming Logic**: Ensure if video is detected on primary monitor, primary monitor bypasses dimming (or all monitors if that's the intended behavior based on "primary monitor is the only one we need to look at for video playing").
4. **Test**: Verify windowed Netflix/YouTube on Chrome doesn't trigger dimming on primary monitor.
5. **Bump & Release**: Version 1.9.9, build MSIX, push to git.
