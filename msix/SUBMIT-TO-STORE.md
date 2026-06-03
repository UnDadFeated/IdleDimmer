# Microsoft Store Submission Guide — WinDimmer64

This walks you through uploading `WinDimmer64_1.4.7.0_x64.msix` to the
Microsoft Store for $0.99.

## Prerequisites

- [x] Microsoft Partner Center account (you have this)
- [x] App name "WinDimmer64" reserved in Partner Center (see Step 1)
- [x] Built MSIX at `dist/WinDimmer64_1.4.7.0_x64.msix`
- [ ] Privacy policy URL live on a public domain (see Step 2)
- [ ] Store listing screenshots (1080x1080 or 1920x1080 PNGs)

---

## Step 1: Reserve the app name in Partner Center

1. Go to https://partner.microsoft.com/dashboard
2. Sign in with the account that owns the dev registration
3. Click **Apps and games** in the left nav
4. Click **+ New product** → **Desktop application** (NOT "Game" — we are a utility)
5. Type `WinDimmer64` as the product name
6. Microsoft will check availability. If `WinDimmer64` is taken, you'll need
   a name like `WinDimmer64 Dimmer` or `Win Dimmer 64` — the search result
   will tell you what's available.
7. Once reserved, you'll be assigned a **Publisher ID**. It looks like
   `CN=12345678-90AB-CDEF-1234-567890ABCDEF`. **Copy this.**
8. Open `msix/Package.appxmanifest` and replace the `Publisher="CN=UnDadFeated"`
   attribute on the `<Identity>` element with the ID you just copied.
9. Re-run `msix\build_msix.bat` to repack with the correct Publisher.

> **Important:** The Publisher string must EXACTLY match your Partner Center
> ID. If they don't match, Partner Center will reject the upload with
> "Identity Publisher Mismatch" (error code 0x80080204).

---

## Step 2: Host the privacy policy

You have two options.

### Option A: GitHub Pages (free, recommended)

The privacy policy is already at `docs/privacy.html`. To publish it:

1. Push the `docs/` folder to GitHub (the `docs/privacy.html` file)
2. Go to https://github.com/UnDadFeated/WinDimmer64/settings/pages
3. Under "Source", select **Deploy from a branch**
4. Branch: `master`, folder: `/docs`
5. Click **Save**
6. Wait ~60 seconds. The policy will be live at:
   `https://undadfeated.github.io/WinDimmer64/privacy.html`
7. Use this URL in Partner Center → Property → Privacy policy URL

### Option B: Any other static host

Upload `docs/privacy.html` to Netlify, Vercel, Cloudflare Pages, your own
server, etc. Any HTTPS URL works.

---

## Step 3: Take the screenshots

The Store requires 4-8 screenshots, at least one of which must be
1920x1080 (or 1080x1080 square) PNG.

Screenshots to capture (suggested order):

1. **Main settings panel (dark theme)** — Default view, "SYSTEM: ACTIVE" status
2. **Per-monitor controls** — Two monitors each with their own slider/toggle
3. **Idle dimming in action** — Dimmed overlay + "SYSTEM: IDLE" status
4. **Light theme** — Same panel with the light theme active
5. (Optional) **Tray icon menu** — Right-click on the tray icon
6. (Optional) **Update check dialog** — The "Check for Updates" UI

**How to take good screenshots:**

1. Run `dist\WinDimmer64_1.4.7.0_x64.msix` after installing it (or just run
   the standalone `WinDimmer64.exe`)
2. Press `Win+Shift+S` to open the Snipping Tool
3. Capture the relevant area
4. Save as PNG
5. Recommended size: 1920x1080. If your screen is smaller, the
   screenshot tool will scale up correctly.

**Where to save them:** Anywhere — you'll upload them in Step 5.

---

## Step 4: Start a new submission in Partner Center

1. Go to https://partner.microsoft.com/dashboard
2. Click **Apps and games** → **WinDimmer64**
3. Click **Start a new submission**
4. Pricing and availability:
   - Price: **$0.99 USD** (lowest paid tier — Microsoft requires this;
     the "free" tier is $0)
   - Free trial: **No free trial**
   - Markets: **All markets** (or specific countries if you prefer)
   - Release date: **Make this app available as soon as it passes certification**
5. Properties:
   - Category: **Productivity** (or **Utilities & tools**)
   - Subcategory: leave default
   - Privacy policy URL: paste the URL from Step 2
   - Website: `https://github.com/UnDadFeated/WinDimmer64`
   - Support contact: `https://github.com/UnDadFeated/WinDimmer64/issues`
   - Copyright: `© 2026 UnDadFeated`
   - Requires a 3D printer, headset, etc.: **No** to all
6. Age rating:
   - Click **Start age rating questionnaire**
   - Answer all questions (most will be "No" for a dimmer utility)
   - Result: **3+** (or whatever it lands on; should be 3+ or 7+)
7. Click **Save draft**, then go to the next section

---

## Step 5: Fill in the Packages section

1. Click **Packages** in the left nav
2. Click **Upload your package**
3. Select `dist\WinDimmer64_1.4.7.0_x64.msix` (the unsigned one is fine —
   Microsoft re-signs everything with their own cert)
4. Wait for upload + analysis (usually under 60 seconds)
5. You should see:
   - Supported architectures: **x64**
   - Minimum Windows version: **10.0.17763.0**
   - Target Windows version: **10.0.22621.0**
   - No capability warnings (we declare `runFullTrust` legitimately)
6. If you see any warnings/errors, fix and rebuild. Common issues:
   - **"Identity Publisher Mismatch"** → Step 1, fix the Publisher string
   - **"Manifest validation error"** → run `makeappx pack /v` for detail
   - **"Missing icon"** → regenerate assets with the PowerShell script
7. Click **Save**

---

## Step 6: Fill in the Store listing

This is what users see in the Store. Use the content from
`msix/store-listing.md`.

1. Click **Store listing** in the left nav
2. **Languages:** add **English (United States)** → click **Manage**
3. **Product name:** `WinDimmer64`
4. **Short description:** (paste from `store-listing.md`)
5. **Long description:** (paste from `store-listing.md`)
6. **What's new in this version:** v1.4.7 release notes
7. **Screenshots:** upload the 4-8 PNGs from Step 3
   - You need at least 4. The 4th slot and beyond is for marketing
     "hero" images — make them look good.
8. **Store logo:** upload `msix/Assets/StoreLogo.png` (50x50)
9. **App icon:** upload `msix/Assets/Square150x150Logo.png`
10. **Promotional images** (optional): 414x180 + 414x468 for special placements
11. **Search terms:** paste from `store-listing.md`
12. Click **Save**

---

## Step 7: Submit for certification

1. Click **Submit for certification** in the top-right
2. Read the certification checklist — make sure you've answered "Yes" to all
3. Click **Submit**
4. Microsoft will review the app. Typical review time:
   - New app: 1-3 business days
   - Updates to existing app: a few hours to 1 day
5. You'll get an email when it's approved (or rejected with reasons)
6. If rejected, fix the listed issues, rebuild, and resubmit

---

## Step 8: After approval

- The app appears in the Microsoft Store at `apps.microsoft.com`
- You can monitor downloads/ratings in Partner Center
- To update the app: build a new MSIX with a bumped version number in the
  manifest, then start a new submission with the new MSIX
- The `WinDimmer64-Setup-v1.4.7.exe` on GitHub still works for users who
  don't use the Store

---

## Common certification rejections and fixes

| Rejection reason | Fix |
|---|---|
| Privacy policy URL unreachable | Make sure the URL returns 200 OK and is HTTPS |
| No screenshots in required slot | All 4 required slots must be filled |
| Misleading icon | The app icon must match the actual app, not a generic icon |
| Identity Publisher Mismatch | Re-check Step 1; the manifest Publisher must match Partner Center |
| Manifest validation error | Run `makeappx pack /v` locally to see the exact schema error |
| Capability not justified | If you add a capability (broadFileSystemAccess, etc.), explain why in Property notes |

---

## Payment

Microsoft takes 30% of the $0.99 sale, so you receive ~$0.69 per copy.
You must pass $100 in lifetime sales to trigger a payout.
Payouts go to the bank/account you configure in Partner Center → Payouts.

---

## Questions?

- Microsoft Store policies: https://learn.microsoft.com/en-us/windows/uwp/publish/
- App submission FAQ: https://learn.microsoft.com/en-us/windows/uwp/publish/app-submissions
- For the v1.4.7 app, open an issue at https://github.com/UnDadFeated/WinDimmer64/issues
