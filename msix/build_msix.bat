@echo off
setlocal EnableDelayedExpansion

REM ====================================================================
REM  IdleDimmer MSIX build script
REM
REM  Usage:
REM    build_msix.bat
REM
REM  Prerequisites:
REM    - IdleDimmer.exe built (run build.bat or LLVM-MinGW commands)
REM    - Windows 10 SDK 10.0.22621 or later installed
REM    - Package.appxmanifest Publisher matches your Partner Center ID
REM    - For Store submission: skip signing (Microsoft signs the MSIX)
REM    - For sideloading: self-sign with a generated cert
REM
REM  Outputs:
REM     - dist\IdleDimmer_1.8.1.0_x64.msix       (unsigned, for Store)
REM     - dist\IdleDimmer_1.8.1.0_x64.msix      (self-signed, for sideload)
REM     - dist\IdleDimmer_1.8.1.0_x64.cer       (signing cert, for sideload)
REM ====================================================================

set VERSION=1.8.1.0
set ARCH=x64
set CONFIG=Release
set PROJECT_ROOT=%~dp0..
set DIST_DIR=%PROJECT_ROOT%\dist
set MSIX_DIR=%PROJECT_ROOT%\msix
set STAGE_DIR=%DIST_DIR%\msix-stage
set SDK_BIN="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64"

REM --------------------------------------------------------------------
REM  Step 0: Verify the app exe exists
REM --------------------------------------------------------------------
if not exist "%PROJECT_ROOT%\IdleDimmer.exe" (
    echo [ERROR] IdleDimmer.exe not found in project root.
    echo         Run build.bat first, or copy your release exe here.
    exit /b 1
)

REM --------------------------------------------------------------------
REM  Step 1: Verify the manifest, assets, and tools
REM --------------------------------------------------------------------
if not exist "%MSIX_DIR%\Package.appxmanifest" (
    echo [ERROR] Package.appxmanifest not found at %MSIX_DIR%
    exit /b 1
)
if not exist "%MSIX_DIR%\Assets\StoreLogo.png" (
    echo [ERROR] Store assets not found at %MSIX_DIR%\Assets
    echo         Run powershell -ExecutionPolicy Bypass -File msix\generate_store_assets.ps1
    exit /b 1
)
if not exist %SDK_BIN%\makeappx.exe (
    echo [ERROR] makeappx.exe not found at %SDK_BIN%
    echo         Install Windows 10 SDK 10.0.22621 from:
    echo         https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
    exit /b 1
)

if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"
if exist "%STAGE_DIR%" rmdir /s /q "%STAGE_DIR%"
mkdir "%STAGE_DIR%"

echo.
echo === IdleDimmer MSIX build ===
echo Version: %VERSION%
echo Arch:    %ARCH%
echo.

REM --------------------------------------------------------------------
REM  Step 2: Stage files for makeappx.exe
REM --------------------------------------------------------------------
echo [1/5] Staging files...
copy /y "%PROJECT_ROOT%\IdleDimmer.exe" "%STAGE_DIR%\IdleDimmer.exe" >nul
xcopy /y /e /i "%MSIX_DIR%\Assets"    "%STAGE_DIR%\Assets"     >nul
copy /y "%MSIX_DIR%\Package.appxmanifest" "%STAGE_DIR%\AppxManifest.xml" >nul

echo       Staged:
dir /b "%STAGE_DIR%"
echo.

REM --------------------------------------------------------------------
REM  Step 3: Pack the MSIX (unsigned) ??? /v enables manifest validation
REM --------------------------------------------------------------------
echo [2/5] Packing MSIX (unsigned, with manifest validation)...
%SDK_BIN%\makeappx.exe pack /v /o /d "%STAGE_DIR%" /p "%DIST_DIR%\IdleDimmer_%VERSION%_%ARCH%.msix"
if errorlevel 1 (
    echo [ERROR] makeappx pack failed.
    exit /b 1
)
echo       Created: %DIST_DIR%\IdleDimmer_%VERSION%_%ARCH%.msix
echo.

REM --------------------------------------------------------------------
REM  Step 5: Generate self-signed cert and sign (for sideloading only)
REM --------------------------------------------------------------------
set CERT_PFX=%DIST_DIR%\IdleDimmer_SelfSign.pfx
set CERT_CER=%DIST_DIR%\IdleDimmer_SelfSign.cer
set CERT_SUBJECT="CN=A69EF8C6-AD1B-4296-A157-C3CC0B4A89A8"

if exist "%CERT_PFX%" (
    echo [3/5] Reusing existing self-signed cert...
) else (
    echo [3/5] Generating self-signed cert for sideloading tests...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0generate_signing_cert.ps1"
    if errorlevel 1 (
        echo [ERROR] Cert generation failed.
        exit /b 1
    )
)

echo.
echo [4/5] Signing MSIX (for sideloading)...
%SDK_BIN%\signtool.exe sign /f "%CERT_PFX%" /p wddim64 /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 "%DIST_DIR%\IdleDimmer_%VERSION%_%ARCH%.msix"
if errorlevel 1 (
    echo [WARN] signtool signing failed. The unsigned MSIX is still valid for Store submission.
) else (
    echo       Signed: %DIST_DIR%\IdleDimmer_%VERSION%_%ARCH%.msix
)

echo.
echo === Build complete ===
echo.
echo   Store submission (unsigned):  %DIST_DIR%\IdleDimmer_%VERSION%_%ARCH%.msix
echo   Sideloading (signed):         %DIST_DIR%\IdleDimmer_%VERSION%_%ARCH%.msix
echo   Signing cert:                 %CERT_CER%
echo.
echo   For Store upload: use the MSIX file in Partner Center ^> Packages ^> Upload.
echo   For local sideloading: double-click the .cer to install the cert into
echo     'Trusted People', then double-click the .msix to install.
echo.
endlocal
