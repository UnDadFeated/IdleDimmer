@echo off
title WinDimmer64 MSVC Build Script
echo ===================================================
echo WinDimmer64 - C++ Windows 11 Build script
echo ===================================================
echo.

:: Detect Visual Studio
set "VS_PATH="
for /f "usebackq tokens=*" %%i in (`"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2^>nul`) do (
    set "VS_PATH=%%i"
)

if "%VS_PATH%"=="" (
    :: Try standard installations
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise"
    )
)

if "%VS_PATH%"=="" (
    echo [ERROR] Visual Studio 2022 installation could not be found automatically.
    echo Please run this script from a "Developer Command Prompt for VS 2022".
    echo.
    pause
    exit /b 1
)

:: Call vcvars64.bat to set environment if not already set
cl >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] Setting up 64-bit developer environment...
    call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
)

echo [INFO] Compiling resources...
rc.exe /nologo resources\resources.rc

echo [INFO] Compiling WinDimmer64 executable...
cl.exe /nologo /O2 /MT /std:c++17 /EHsc /Fe:WinDimmer64.exe src\main.cpp src\MainWindow.cpp src\DimmerManager.cpp src\ConfigManager.cpp resources\resources.res user32.lib gdi32.lib d2d1.lib dwrite.lib dwmapi.lib shell32.lib ole32.lib advapi.lib /link /SUBSYSTEM:WINDOWS

if %errorlevel% equ 0 (
    echo.
    echo ===================================================
    echo [SUCCESS] WinDimmer64.exe built successfully!
    echo ===================================================
) else (
    echo.
    echo [ERROR] Build failed!
)
pause
