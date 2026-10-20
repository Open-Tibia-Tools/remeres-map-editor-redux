@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM  RME Ninja Build Script
REM  Builds x64 Release with Ninja and generates compile_commands.json
REM ============================================================

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR:~0,-1%"
set "BUILD_DIR=%PROJECT_ROOT%\build-ninja"

REM Preserve any user-set VCPKG_ROOT before MSVC environment setup
set "PRE_VCPKG_ROOT=%VCPKG_ROOT%"

REM Parse optional command-line flags
set "CLEAN_BUILD=0"
set "FULL_CLEAN=0"
set "SHOW_HELP=0"

for %%A in (%*) do (
    if /i "%%~A"=="help" set "SHOW_HELP=1"
    if /i "%%~A"=="--help" set "SHOW_HELP=1"
    if /i "%%~A"=="-help" set "SHOW_HELP=1"
    if /i "%%~A"=="/help" set "SHOW_HELP=1"
    if /i "%%~A"=="-h" set "SHOW_HELP=1"
    if /i "%%~A"=="/?" set "SHOW_HELP=1"
    if /i "%%~A"=="clean" set "CLEAN_BUILD=1"
    if /i "%%~A"=="--clean" set "CLEAN_BUILD=1"
    if /i "%%~A"=="-clean" set "CLEAN_BUILD=1"
    if /i "%%~A"=="/clean" set "CLEAN_BUILD=1"
    if /i "%%~A"=="clean-all" set "FULL_CLEAN=1"
    if /i "%%~A"=="--clean-all" set "FULL_CLEAN=1"
    if /i "%%~A"=="-clean-all" set "FULL_CLEAN=1"
    if /i "%%~A"=="/clean-all" set "FULL_CLEAN=1"
)

if "!SHOW_HELP!"=="1" (
    echo.
    echo ========================================================
    echo   RME Ninja Build Script
    echo ========================================================
    echo.
    echo Usage: build_ninja.bat [options]
    echo.
    echo Options:
    echo   clean       Clear CMake cache and ninja build files before building.
    echo   clean-all   Remove the entire build-ninja directory before building.
    echo   help        Show this help message.
    echo.
    exit /b 0
)

echo.
echo ========================================================
echo   RME Ninja Build Script (x64 Release)
echo ========================================================
echo.

if "!FULL_CLEAN!"=="1" (
    echo [Clean] Removing entire build directory: %BUILD_DIR%...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    echo OK - Clean complete.
    echo.
) else if "!CLEAN_BUILD!"=="1" (
    echo [Clean] Clearing CMake cache and build files in: %BUILD_DIR%...
    if exist "%BUILD_DIR%\CMakeCache.txt" del /f /q "%BUILD_DIR%\CMakeCache.txt"
    if exist "%BUILD_DIR%\build.ninja" del /f /q "%BUILD_DIR%\build.ninja"
    if exist "%BUILD_DIR%\CMakeFiles" rmdir /s /q "%BUILD_DIR%\CMakeFiles"
    if exist "%BUILD_DIR%\rme.exe" del /f /q "%BUILD_DIR%\rme.exe"
    echo OK - Clean complete.
    echo.
)

echo [1/6] Checking for Ninja...
ninja --version >nul 2>&1
if !ERRORLEVEL! neq 0 (
    echo ERROR: Ninja was not found on your PATH.
    echo Please install Ninja and ensure it is available on your PATH.
    exit /b 1
)
echo OK - Ninja found.

echo [2/6] Finding Visual C++ toolchain...
set "VW=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VW%" set "VW=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VW%" (
    echo ERROR: vswhere.exe not found.
    exit /b 1
)

set "VSP="
for /f "usebackq tokens=*" %%i in (`"%VW%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
    set "VSP=%%i"
)
if "%VSP%"=="" (
    for /f "usebackq tokens=*" %%i in (`"%VW%" -latest -products * -property installationPath 2^>nul`) do (
        set "VSP=%%i"
    )
)

if "%VSP%"=="" (
    echo ERROR: Visual Studio installation not found.
    exit /b 1
)

echo Found VS at: %VSP%
echo Initializing x64 environment...

set "VV=%VSP%\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VV%" (
    echo ERROR: vcvarsall.bat not found.
    exit /b 1
)

call "%VV%" x64 >nul
if !ERRORLEVEL! neq 0 (
    echo ERROR: Failed to initialize x64 environment.
    exit /b 1
)
echo OK - Environment initialized.

echo [3/6] Locating vcpkg...
set "VKD="
if defined PRE_VCPKG_ROOT (
    if exist "%PRE_VCPKG_ROOT%\.vcpkg-root" set "VKD=%PRE_VCPKG_ROOT%"
)
if not defined VKD (
    for %%p in ("c:\vcpkg" "c:\src\vcpkg" "%USERPROFILE%\vcpkg" "%USERPROFILE%\source\repos\vcpkg") do (
        if exist "%%~p\.vcpkg-root" if not defined VKD set "VKD=%%~p"
    )
)
if not defined VKD (
    if defined VCPKG_ROOT (
        if exist "%VCPKG_ROOT%\.vcpkg-root" set "VKD=%VCPKG_ROOT%"
    )
)
if not defined VKD (
    echo ERROR: vcpkg not found. Set VCPKG_ROOT environment variable.
    exit /b 1
)
set "VCPKG_ROOT=%VKD%"
echo OK - vcpkg found at: %VKD%

echo [4/6] Configuring CMake (Ninja)...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Validate existing CMakeCache.txt for missing libraries or changed toolchains
if exist "%BUILD_DIR%\CMakeCache.txt" (
    set "STALE_CACHE=0"
    for /f "tokens=1,2 delims==" %%A in ('findstr /i "FILEPATH=" "%BUILD_DIR%\CMakeCache.txt" 2^>nul ^| findstr /i "\.lib" ^| findstr /v /i "NOTFOUND"') do (
        if not exist "%%B" (
            set "STALE_CACHE=1"
            echo Detected missing cached library: %%B
        )
    )
    for /f "tokens=2 delims==" %%R in ('findstr /i "Z_VCPKG_ROOT_DIR:INTERNAL=" "%BUILD_DIR%\CMakeCache.txt" 2^>nul') do (
        set "CACHED_VKD=%%R"
        set "CACHED_VKD=!CACHED_VKD:/=\!"
        set "CURR_VKD=%VKD:/=\%"
        if /i not "!CACHED_VKD!"=="!CURR_VKD!" (
            set "STALE_CACHE=1"
            echo Detected vcpkg root change from !CACHED_VKD! to !CURR_VKD!.
        )
    )
    if "!STALE_CACHE!"=="1" (
        echo WARNING: Stale CMake cache detected.
        echo Clearing CMakeCache.txt and build.ninja to force re-detection...
        if exist "%BUILD_DIR%\CMakeCache.txt" del /f /q "%BUILD_DIR%\CMakeCache.txt"
        if exist "%BUILD_DIR%\build.ninja" del /f /q "%BUILD_DIR%\build.ninja"
        if exist "%BUILD_DIR%\CMakeFiles" rmdir /s /q "%BUILD_DIR%\CMakeFiles"
    )
)

cmake -G Ninja ^
      -S "%PROJECT_ROOT%" ^
      -B "%BUILD_DIR%" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
      -DCMAKE_TOOLCHAIN_FILE="%VKD%\scripts\buildsystems\vcpkg.cmake" ^
      -DVCPKG_TARGET_TRIPLET=x64-windows

if !ERRORLEVEL! neq 0 (
    echo ERROR: CMake configuration failed.
    exit /b 1
)
echo OK - Configuration complete.

echo [5/6] Building rme...
cmake --build "%BUILD_DIR%" --target rme --parallel
if !ERRORLEVEL! neq 0 (
    echo.
    echo ERROR: Build failed.
    echo If this is due to stale dependencies or cached build artifacts, try:
    echo   .\build_ninja.bat clean
    echo   .\build_ninja.bat clean-all
    exit /b 1
)
echo OK - Build complete.

echo [6/6] Updating compile_commands.json...
if exist "%BUILD_DIR%\compile_commands.json" (
    copy /Y "%BUILD_DIR%\compile_commands.json" "%PROJECT_ROOT%\compile_commands.json" >nul
    echo OK - compile_commands.json copied to root.
) else (
    echo WARNING: compile_commands.json not found.
)

echo.
echo ========================================================
echo   BUILD SUCCESSFUL!
echo ========================================================
echo.
echo   Output:    %BUILD_DIR%\rme.exe
echo   Linter:    %PROJECT_ROOT%\compile_commands.json
echo.

endlocal
