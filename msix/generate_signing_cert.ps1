# generate_signing_cert.ps1
# Creates a self-signed code-signing cert for MSIX sideloading tests.
# PFX password is fixed at "wddim64" for build reproducibility.

$ErrorActionPreference = "Stop"
$distDir = Join-Path $PSScriptRoot "..\dist"
$pfxPath = Join-Path $distDir "WinDimmer64_SelfSign.pfx"
$cerPath = Join-Path $distDir "WinDimmer64_SelfSign.cer"
$password = ConvertTo-SecureString -String "wddim64" -Force -AsPlainText

if (-not (Test-Path $distDir)) {
    New-Item -ItemType Directory -Path $distDir -Force | Out-Null
}

if ((Test-Path $pfxPath) -and (Test-Path $cerPath)) {
    Write-Host "Reusing existing cert files at $distDir"
    Write-Host "  PFX: $pfxPath"
    Write-Host "  CER: $cerPath"
    exit 0
}

# Look for an existing cert with this subject first (idempotent)
$existing = Get-ChildItem "Cert:\CurrentUser\My" -ErrorAction SilentlyContinue |
    Where-Object { $_.Subject -eq "CN=UnDadFeated" -and $_.HasPrivateKey }

if ($existing) {
    Write-Host "Found existing cert, exporting..."
    $cert = $existing
} else {
    Write-Host "Generating new self-signed cert..."
    $cert = New-SelfSignedCertificate `
        -Subject "CN=UnDadFeated" `
        -Type CodeSigningCert `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -NotAfter (Get-Date).AddYears(5) `
        -KeyAlgorithm RSA `
        -KeyLength 2048 `
        -KeyUsage DigitalSignature `
        -FriendlyName "WinDimmer64 MSIX Signing"
}

Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $password | Out-Null
Export-Certificate -Cert $cert -FilePath $cerPath -Type CERT | Out-Null

Write-Host ""
Write-Host "Cert files written:"
Write-Host "  PFX: $pfxPath"
Write-Host "  CER: $cerPath"
Write-Host "  Thumbprint: $($cert.Thumbprint)"
Write-Host ""
Write-Host "For Store submission: ignore these files. Microsoft signs your MSIX."
Write-Host "For local sideloading tests: double-click the .cer to add it to"
Write-Host "'Trusted People', then double-click the .msix to install."
