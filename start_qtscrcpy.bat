@echo off
title QtScrcpy
set "ROOT_DIR=%~dp0"
cd /d "%ROOT_DIR%"

taskkill /F /IM scrcpy.exe >nul 2>&1
taskkill /F /IM QtScrcpy.exe >nul 2>&1

set "OUT_DIR=%ROOT_DIR%output\x64\RelWithDebInfo"
set "OUTPUT_EXE=%OUT_DIR%\QtScrcpy.exe"
set "CMAKE_BIN=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "QT_DIR=D:\Qt\5.15.2\msvc2019_64"

if /i "%1"=="build" goto do_build
if /i "%1"=="rebuild" goto do_rebuild
if not exist "%OUTPUT_EXE%" goto do_build

echo ===============================================================
echo  [INFO] Launching QtScrcpy ...
echo  Target: %OUTPUT_EXE%
echo ===============================================================
start "" /D "%OUT_DIR%" "%OUTPUT_EXE%"
ping 127.0.0.1 -n 3 >nul
exit /b 0

:do_rebuild
echo [INFO] Cleaning build cache for rebuild...
if exist "build_temp" rmdir /s /q "build_temp"

:do_build
echo ===============================================================
echo  Building QtScrcpy (Incremental Build)
echo ===============================================================

if not exist "build_temp" md "build_temp"

if not exist "%CMAKE_BIN%" (
    where cmake >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] Cannot find cmake.exe. Check Visual Studio BuildTools.
        pause
        exit /b 1
    )
    set "CMAKE_BIN=cmake"
)

if not exist "%QT_DIR%" (
    echo [ERROR] Cannot find Qt SDK at %QT_DIR%
    pause
    exit /b 1
)

echo [1/3] Configuring CMake...
"%CMAKE_BIN%" -G "Visual Studio 16 2019" -A x64 -DCMAKE_PREFIX_PATH="%QT_DIR%" -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build_temp -S .
if errorlevel 1 (
    echo [ERROR] CMake configuration failed!
    pause
    exit /b 1
)

echo [2/3] Compiling project...
"%CMAKE_BIN%" --build build_temp --config RelWithDebInfo -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo [ERROR] Compilation failed!
    pause
    exit /b 1
)

echo [3/3] Syncing runtime dependencies...
if not exist "%OUT_DIR%\adb.exe" (
    copy /y "QtScrcpy\QtScrcpyCore\src\third_party\adb\win\*" "%OUT_DIR%\" >nul
)
if not exist "%OUT_DIR%\scrcpy-server" (
    copy /y "QtScrcpy\QtScrcpyCore\src\third_party\scrcpy-server" "%OUT_DIR%\" >nul
)
if not exist "%OUT_DIR%\keymap" (
    xcopy /y /e /i "keymap" "%OUT_DIR%\keymap" >nul
)
if not exist "%OUT_DIR%\config" (
    xcopy /y /e /i "config" "%OUT_DIR%\config" >nul
)

if not exist "%OUT_DIR%\Qt5Core.dll" (
    echo [INFO] Deploying Qt runtime libraries (windeployqt)...
    "%QT_DIR%\bin\windeployqt.exe" --release --no-translations "%OUTPUT_EXE%" >nul
)

echo [INFO] Build completed. Launching QtScrcpy...
start "" /D "%OUT_DIR%" "%OUTPUT_EXE%"
ping 127.0.0.1 -n 3 >nul
exit /b 0
