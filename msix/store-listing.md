# WinDimmer64 — Microsoft Store Listing Copy

Use this content in Partner Center when creating the new app submission.

---

## App Name (reserved in Partner Center)
**WinDimmer64**

## Short Description (max 260 chars, shown in search results)
Per-monitor screen dimming for Windows 10/11 with idle detection, blocked-app bypass, and a 100KB native footprint. Zero telemetry, zero ads, zero bloat.

## Long Description (max 10,000 chars, shown on store page)

**A tiny, native screen dimmer that respects your eyes, your machine, and your privacy.**

WinDimmer64 is a single-file Windows utility that adds per-monitor brightness control beyond what your monitor offers. Each monitor gets its own toggle and slider, so you can dim your second screen to 30% for late-night reading while keeping your main display at 80% for work — without any DDC flicker or monitor OSD gymnastics.

### Why WinDimmer64

Most "screen dimmer" apps ship as 50MB Electron wrappers that pin your CPU and ship analytics to advertisers. WinDimmer64 is the opposite:

- **~150 KB native binary.** Written in C++17 with Direct2D and the raw Win32 API. No Electron, no .NET, no Chromium, no Java.
- **Zero telemetry, zero network calls, zero analytics SDKs.** The only outbound request the app ever makes is an optional manual "Check for Updates" call to GitHub.
- **100% offline.** No account, no sign-in, no subscription.
- **Open source.** Every line of code is on GitHub for inspection.

### Features

**Per-monitor dimming with independent controls**
Each connected display gets its own on/off toggle and 0–90% dim slider. Drag a slider to auto-enable dimming for that monitor. No "group dim" lock-in — disable one monitor and leave the rest at full brightness.

**Idle detection with smooth fade**
Set a timeout (1–60 minutes) and an idle dim level. When the system reports no keyboard or mouse input, the overlays fade in. When activity resumes, they fade out. The cursor auto-hides during idle dimming and reappears the moment you touch the mouse.

**Blocked-app bypass**
Add any process to the blocked list (Steam, MPC-HC, mpv, Plex, your TV app — whatever you use for video). When a blocked app is in the foreground OR is playing audio through the default audio endpoint, dimming is automatically suspended so you never get a darkened video.

**Fullscreen detection**
When Direct3D runs fullscreen (games) or a presentation app takes over the shell, dimming fades out. Detection is throttled to a 5-second cadence to keep CPU and Windows Defender happy.

**Hotkeys (work everywhere)**
- **Ctrl+Alt+↑** — Brighter (-5%)
- **Ctrl+Alt+↓** — Darker (+5%)
- **Ctrl+Alt+D** — Toggle dimming on/off

**Light and warm-tint overlays**
Two optional cosmetic overlays for low-light sessions: a soft warm tint and a monochrome "light mode" wash. Both are off by default; the default dim is a neutral black.

**Auto-start with Windows (optional)**
One-click toggle to register a Run-key entry. No scheduled tasks, no services, no startup folder pollution.

**In-app update check (optional)**
A single "Check for Updates" button pings GitHub's public releases API to compare your version against the latest. No background updater, no auto-install, no elevated-process tricks.

**Per-user installation**
Installs to `%LOCALAPPDATA%`, registers a per-user uninstall entry, requires no admin elevation, and respects your existing config from previous installs.

**Uninstall is clean**
Settings, Run-key entry, Start Menu shortcut — all removed. No leftover files, no leftover registry keys, no leftover services.

### Designed to stay out of your way

- **Tray-resident.** Closing the settings window leaves a single tray icon. No background window, no persistent notification.
- **One process, ~3 MB working set.** Verified idle.
- **No driver, no kernel component, no service.** Standard user-mode Win32 application.
- **Single-instance via a named mutex.** Launching the EXE twice just brings up the existing window.
- **Defender-friendly.** Heavy API calls (process enumeration, audio session polling, shell32 notification state) are throttled to a 5-second cadence. No `SetSystemCursor`, no `CreateToolhelp32Snapshot`, no `HWND_BROADCAST` power messages.

### Use cases

- **Late-night coding on a bright external monitor.** Dim just that one monitor to 25%, leave your laptop screen at 100%.
- **Watching video in a dark room.** Idle-dim kicks in when you stop touching the keyboard, but bypasses automatically the moment your video player takes focus.
- **Reading long-form articles.** Set a 2-minute idle timeout and a 40% dim level. Look away to think, the screen dims, your eyes relax.
- **Presentations.** Toggle dimming off globally with Ctrl+Alt+D before presenting. Toggle it back on after.
- **Battery saving on a laptop.** Dim the integrated display aggressively when on battery, leave the external monitor at a readable level.

### Technical details

- **Supported OS:** Windows 10 version 1809 (build 17763) and later, including Windows 11
- **Architecture:** x64
- **Dependencies:** None. Statically linked against the Windows API.
- **Disk footprint:** 150 KB executable + 1 KB config file
- **Memory footprint:** ~3 MB working set at idle
- **Permissions used:** None beyond standard user. No UAC prompts, no admin elevation.

### Privacy

Full policy at the project page. Summary: the app stores your preferences locally and never transmits them. No analytics, no crash reports, no fingerprinting, no advertising IDs.

### Source and releases

Source, releases, and the standalone installer for direct download are at:
https://github.com/UnDadFeated/WinDimmer64

## What's New in v1.4.7
- Fixed: Active dimming was fading out after 1 second on some systems. Caused by `QUNS_BUSY` being misclassified as a "fullscreen app" signal. Now only `QUNS_RUNNING_D3D_FULL_SCREEN` and `QUNS_PRESENTATION_MODE` trigger the bypass.
- Fixed: Setup installer now reports the correct installed version on upgrade.
- All prior fixes preserved: per-monitor controls, idle detection, blocked-app bypass, fullscreen detection, hotkeys, clean uninstall.

## Screenshots (4 required minimum, 8 max)
1. **Settings panel — main view** — Title bar, master toggle, per-monitor slider cards, hotkey hint
2. **Idle dimming engaged** — Overlay dimming active with "SYSTEM: IDLE" status indicator
3. **Per-monitor layout** — Two monitors with different dim levels side by side
4. **Light theme** — Same panel in light mode for daytime use
5. (Optional) **Tray icon menu** — Right-click context menu
6. (Optional) **Dark theme detail** — Close-up of the slider card and toggle switch

## Search Terms / Keywords (max 7, max 30 chars each)
- screen dimmer
- monitor dim
- brightness control
- eye care
- blue light
- night light
- per monitor

## Category
**Productivity** or **Utilities & tools**

## Age Rating
**3+** (no objectionable content, no interactivity beyond system controls, no user-generated content, no data sharing)

## Pricing
**$0.99 USD** (the lowest non-free tier — required by Partner Center for paid apps; free tier is $0)

## Privacy Policy URL
https://undadfeated.github.io/WinDimmer64/privacy.html

## Support Contact
https://github.com/UnDadFeated/WinDimmer64/issues

## Website
https://github.com/UnDadFeated/WinDimmer64

## Copyright
© 2026 UnDadFeated
