@echo off
setlocal enabledelayedexpansion

:: Build and Run Script for huahua-player
:: Usage: double-click or run in cmd: build_and_run.bat

:: Default values
set "DEFAULT_QT_BIN=D:\ide\Qt\Qt5.8.0\5.8\mingw53_32\bin"
set "DEFAULT_MINGW_BIN=D:\ide\Qt\Qt5.8.0\Tools\mingw530_32\bin"
set "DEFAULT_PROJECT_DIR=D:\code\suhuamo\huahua-player"
set "DEFAULT_BUILD_DIR=D:\code\suhuamo\build-huahua-player-Desktop_Qt_5_8_0_MinGW_32bit-Debug"

:: Read config from .env/run.config (overrides defaults)
set "CONFIG_FILE=%~dp0..\.env\run.config"
if exist "%CONFIG_FILE%" (
    for /f "usebackq tokens=1,* delims==" %%a in ("%CONFIG_FILE%") do (
        set "_line=%%a"
        if "!_line:~0,1!" neq "#" if "%%a" neq "" (
            if "%%a"=="QT_BIN" set "QT_BIN=%%b"
            if "%%a"=="MINGW_BIN" set "MINGW_BIN=%%b"
            if "%%a"=="PROJECT_DIR" set "PROJECT_DIR=%%b"
            if "%%a"=="BUILD_DIR" set "BUILD_DIR=%%b"
        )
    )
)

:: Fall back to defaults if not set in config
if not defined QT_BIN set "QT_BIN=%DEFAULT_QT_BIN%"
if not defined MINGW_BIN set "MINGW_BIN=%DEFAULT_MINGW_BIN%"
if not defined PROJECT_DIR set "PROJECT_DIR=%DEFAULT_PROJECT_DIR%"
if not defined BUILD_DIR set "BUILD_DIR=%DEFAULT_BUILD_DIR%"

:: Set PATH
set PATH=%QT_BIN%;%MINGW_BIN%;%PATH%

echo ==========================================
echo Building huahua-player...
echo ==========================================

:: Create build directory if not exists
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
    echo Created build directory: %BUILD_DIR%
)

:: Change to build directory
cd /d "%BUILD_DIR%"

:: Run qmake
echo.
echo [1/3] Running qmake...
"%QT_BIN%\qmake.exe" -o Makefile "%PROJECT_DIR%\huahua-player.pro" -spec win32-g++ "CONFIG+=debug" "CONFIG+=qml_debug"

if errorlevel 1 (
    echo Error: qmake failed!
    pause
    exit /b 1
)

:: Run make
echo.
echo [2/3] Running mingw32-make...
"%MINGW_BIN%\mingw32-make.exe" -f Makefile.Debug

if errorlevel 1 (
    echo Error: mingw32-make failed!
    pause
    exit /b 1
)

echo.
echo Build completed successfully!

:: Deploy DLLs
echo.
echo [3/3] Deploying DLLs...

set DEBUG_DIR=%BUILD_DIR%\debug

:: Copy Qt DLLs
echo   Copying Qt DLLs...
if not exist "%DEBUG_DIR%\Qt5Cored.dll" copy /Y "%QT_BIN%\Qt5Cored.dll" "%DEBUG_DIR%\" >nul
if not exist "%DEBUG_DIR%\Qt5Guid.dll" copy /Y "%QT_BIN%\Qt5Guid.dll" "%DEBUG_DIR%\" >nul
if not exist "%DEBUG_DIR%\Qt5Widgetsd.dll" copy /Y "%QT_BIN%\Qt5Widgetsd.dll" "%DEBUG_DIR%\" >nul
if not exist "%DEBUG_DIR%\Qt5Networkd.dll" copy /Y "%QT_BIN%\Qt5Networkd.dll" "%DEBUG_DIR%\" >nul
if not exist "%DEBUG_DIR%\libgcc_s_dw2-1.dll" copy /Y "%QT_BIN%\libgcc_s_dw2-1.dll" "%DEBUG_DIR%\" >nul
if not exist "%DEBUG_DIR%\libstdc++-6.dll" copy /Y "%QT_BIN%\libstdc++-6.dll" "%DEBUG_DIR%\" >nul
if not exist "%DEBUG_DIR%\libwinpthread-1.dll" copy /Y "%QT_BIN%\libwinpthread-1.dll" "%DEBUG_DIR%\" >nul

:: Copy project DLLs
echo   Copying project DLLs...
if exist "%PROJECT_DIR%\dll\x86" (
    for %%f in ("%PROJECT_DIR%\dll\x86\*.dll") do (
        if not exist "%DEBUG_DIR%\%%~nxf" copy /Y "%%f" "%DEBUG_DIR%\" >nul 2>&1
    )
)

:: Copy platform plugin
echo   Copying platform plugin...
if not exist "%DEBUG_DIR%\platforms" mkdir "%DEBUG_DIR%\platforms"
if not exist "%DEBUG_DIR%\platforms\qwindowsd.dll" copy /Y "%QT_BIN%\..\plugins\platforms\qwindowsd.dll" "%DEBUG_DIR%\platforms\" >nul

echo.
echo ==========================================
echo Build and Deploy completed!
echo ==========================================
echo.
echo Starting huahua-player...
echo.

:: Run the executable
start "" "%DEBUG_DIR%\huahua-player.exe"