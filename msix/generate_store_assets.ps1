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

Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = "Stop"
$icoPath = Join-Path $PSScriptRoot "..\resources\app.ico"
$outDir  = Join-Path $PSScriptRoot "..\msix\Assets"

if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

# Load icon and convert to Bitmap at source resolution
$icon = New-Object System.Drawing.Icon $icoPath
$bmp = $icon.ToBitmap()
Write-Host "Source: $($bmp.Width)x$($bmp.Height)"

function Save-PngAtSize([System.Drawing.Bitmap]$source, [int]$size, [string]$outPath) {
    $dest = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($dest)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($source, 0, 0, $size, $size)
    $g.Dispose()
    $dest.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $dest.Dispose()
}

function Save-PngAtSizeWide([System.Drawing.Bitmap]$source, [int]$w, [int]$h, [string]$outPath) {
    $dest = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($dest)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($source, 0, 0, $w, $h)
    $g.Dispose()
    $dest.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $dest.Dispose()
}

# Square tiles
Save-PngAtSize       $bmp 44   (Join-Path $outDir "Square44x44Logo.png")
Save-PngAtSize       $bmp 71   (Join-Path $outDir "Square71x71Logo.png")
Save-PngAtSize       $bmp 150  (Join-Path $outDir "Square150x150Logo.png")
Save-PngAtSize       $bmp 310  (Join-Path $outDir "Square310x310Logo.png")

# Wide tile
Save-PngAtSizeWide   $bmp 310 150 (Join-Path $outDir "Wide310x150Logo.png")

# Store logo (50x50 in manifest, but Partner Center accepts 50x50)
Save-PngAtSize       $bmp 50   (Join-Path $outDir "StoreLogo.png")

# Splash screen (620x300) - render with our background color
$splash = New-Object System.Drawing.Bitmap 620, 300
$gs = [System.Drawing.Graphics]::FromImage($splash)
$gs.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$gs.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$gs.Clear([System.Drawing.Color]::FromArgb(255, 18, 18, 18))  # matches app bg #121212
# Centered icon at 200x200
$iconX = [int]((620 - 200) / 2)
$iconY = [int]((300 - 200) / 2)
$gs.DrawImage($bmp, $iconX, $iconY, 200, 200)
$gs.Dispose()
$splash.Save((Join-Path $outDir "SplashScreen.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$splash.Dispose()

$icon.Dispose()
$bmp.Dispose()

Write-Host "Generated all assets in ${outDir}:"
Get-ChildItem $outDir | ForEach-Object { Write-Host ("  {0,-30} {1,8} bytes" -f $_.Name, $_.Length) }
