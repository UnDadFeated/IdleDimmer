# generate_store_assets.ps1
# Generates all Microsoft Store icon assets from resources\app.ico
# Outputs PNGs to msix\Assets\
#
# Store requirements:
#   Square44x44Logo.png        44x44   Taskbar small
#   Square71x71Logo.png        71x71   Start menu small
#   Square150x150Logo.png     150x150  Start menu medium / Tile
#   Square310x310Logo.png     310x310  Start menu large
#   Wide310x150Logo.png       310x150  Start menu wide
#   StoreLogo.png              50x50   Partner Center thumbnail
#   SplashScreen.png          620x300  Splash screen
#
# The .ico file is a multi-frame container. Modern Windows .ico files
# (Vista+) can embed PNG data directly (PNG-in-ICO). We enumerate the
# frames, pick the largest one (typically 256x256), and render it at
# each required Store size with high-quality bicubic downsampling.

Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = "Stop"
$icoPath = Join-Path $PSScriptRoot "..\resources\app.ico"
$outDir  = Join-Path $PSScriptRoot "..\msix\Assets"

if (-not (Test-Path $icoPath)) {
    throw "Source icon not found: $icoPath"
}
if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

# --------------------------------------------------------------------
# 1. Parse the .ico and find the largest frame
# --------------------------------------------------------------------
# ICONDIR:    2 (reserved) + 2 (type=1) + 2 (count)
# ICONDIRENTRY: 1 (width) + 1 (height) + 1 (colors) + 1 (reserved) +
#              2 (planes) + 2 (bpp) + 4 (size) + 4 (offset)
$bytes = [System.IO.File]::ReadAllBytes($icoPath)
if ($bytes.Length -lt 6) { throw "ICO too small: $icoPath" }
$count = [BitConverter]::ToUInt16($bytes, 4)
if ($count -le 0) { throw "ICO has no frames: $icoPath" }

$bestW = 0
$bestH = 0
$bestOff = 0
$bestSize = 0
$bestIsPng = $false

for ($i = 0; $i -lt $count; $i++) {
    $e = 6 + ($i * 16)
    $w  = $bytes[$e]
    $h  = $bytes[$e + 1]
    if ($w -eq 0)  { $w = 256 }
    if ($h -eq 0)  { $h = 256 }
    $size = [BitConverter]::ToUInt32($bytes, $e + 8)
    $off  = [BitConverter]::ToUInt32($bytes, $e + 12)
    $isPng = ($bytes[$off] -eq 0x89) -and ($bytes[$off + 1] -eq 0x50) `
          -and ($bytes[$off + 2] -eq 0x4E) -and ($bytes[$off + 3] -eq 0x47)
    Write-Host ("  Frame $i : {0}x{1}  {2} bytes  [{3}]" -f $w, $h, $size, $(if ($isPng) { "PNG" } else { "BMP" }))
    if ($w * $h -gt $bestW * $bestH) {
        $bestW = $w; $bestH = $h
        $bestOff = $off
        $bestSize = $size
        $bestIsPng = $isPng
    }
}

Write-Host ""
Write-Host ("Using largest frame: {0}x{1}  ({2} bytes, {3})" -f $bestW, $bestH, $bestSize, $(if ($bestIsPng) { "PNG" } else { "BMP" }))

# --------------------------------------------------------------------
# 2. Extract the largest frame into a Bitmap
# --------------------------------------------------------------------
# Write the raw frame to a temp file and load it. If PNG, .NET's
# Bitmap decoder reads it directly. If BMP (32-bpp with AND mask), we
# need to strip the AND mask.
$tmp = [System.IO.Path]::GetTempFileName()
try {
    if ($bestIsPng) {
        # Just write the PNG bytes
        [System.IO.File]::WriteAllBytes($tmp, $bytes[$bestOff..($bestOff + $bestSize - 1)])
    } else {
        # Build a clean BMP without the AND mask
        $andMaskBytes = [int]($bestH * [Math]::Ceiling($bestW / 32.0) * 4)
        $ms = New-Object System.IO.MemoryStream
        $ms.Write($bytes, $bestOff, 40)               # BITMAPINFOHEADER
        $ms.Position = 8                                # patch biHeight
        $ms.Write([System.BitConverter]::GetBytes([int32]$bestH), 0, 4)
        $ms.Write($bytes, $bestOff + 40, ($bestW * $bestH * 4))   # XOR data
        $ms.Position = 0
        [System.IO.File]::WriteAllBytes($tmp, $ms.ToArray())
        $ms.Dispose()
    }
    $source = [System.Drawing.Image]::FromFile($tmp)
    Write-Host ("Decoded source: {0}x{1}  format={2}" -f $source.Width, $source.Height, $source.RawFormat)
} finally {
    Remove-Item $tmp -ErrorAction SilentlyContinue
}

# --------------------------------------------------------------------
# 3. Render at each required size
# --------------------------------------------------------------------
function Save-PngAtSize([System.Drawing.Image]$src, [int]$size, [string]$outPath) {
    $dest = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($dest)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($src, 0, 0, $size, $size)
    $g.Dispose()
    $dest.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $dest.Dispose()
}

function Save-PngAtSizeWide([System.Drawing.Image]$src, [int]$w, [int]$h, [string]$outPath) {
    $dest = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($dest)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($src, 0, 0, $w, $h)
    $g.Dispose()
    $dest.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $dest.Dispose()
}

# Square tiles
Save-PngAtSize       $source 44   (Join-Path $outDir "Square44x44Logo.png")
Save-PngAtSize       $source 71   (Join-Path $outDir "Square71x71Logo.png")
Save-PngAtSize       $source 150  (Join-Path $outDir "Square150x150Logo.png")
Save-PngAtSize       $source 310  (Join-Path $outDir "Square310x310Logo.png")

# Wide tile
Save-PngAtSizeWide   $source 310 150 (Join-Path $outDir "Wide310x150Logo.png")

# Store logo (50x50 in manifest, but Partner Center accepts 50x50)
Save-PngAtSize       $source 50   (Join-Path $outDir "StoreLogo.png")

# Splash screen (620x300) - render with our background color
$splash = New-Object System.Drawing.Bitmap 620, 300
$gs = [System.Drawing.Graphics]::FromImage($splash)
$gs.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$gs.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$gs.Clear([System.Drawing.Color]::FromArgb(255, 18, 18, 18))  # matches app bg #121212
# Centered icon at 200x200
$iconX = [int]((620 - 200) / 2)
$iconY = [int]((300 - 200) / 2)
$gs.DrawImage($source, $iconX, $iconY, 200, 200)
$gs.Dispose()
$splash.Save((Join-Path $outDir "SplashScreen.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$splash.Dispose()

$source.Dispose()

Write-Host ""
Write-Host "Generated all assets in ${outDir}:"
Get-ChildItem $outDir | ForEach-Object {
    Write-Host ("  {0,-30} {1,8} bytes" -f $_.Name, $_.Length)
}
