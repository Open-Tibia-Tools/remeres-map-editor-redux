@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM  RME Test Build & Run Script
REM  Builds every test target with the same Ninja setup as
REM  build_ninja.bat (MSVC x64 environment required) and runs
REM  the full CTest suite.
REM
REM  Usage:   run_tests.bat
REM  Requires: build_ninja.bat to have configured build-ninja
REM            at least once (this script does not reconfigure).
REM ============================================================

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR:~0,-1%"
set "BUILD_DIR=%PROJECT_ROOT%\build-ninja"

if not exist "%BUILD_DIR%\build.ninja" (
    echo ERROR: %BUILD_DIR% has no build.ninja.
    echo        Run build_ninja.bat once to configure the build directory.
    exit /b 1
)

echo.
echo ========================================================
echo   RME Test Build ^& Run (x64 Release)
echo ========================================================
echo.

echo [1/4] Checking for Ninja...
ninja --version >nul 2>&1
if !ERRORLEVEL! neq 0 (
    echo ERROR: Ninja was not found on your PATH.
    exit /b 1
)
echo OK - Ninja found.

echo [2/4] Finding Visual C++ toolchain...
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

set "VV=%VSP%\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VV%" (
    echo ERROR: vcvarsall.bat not found.
    exit /b 1
)

echo Found VS at: %VSP%
echo Initializing x64 environment...
call "%VV%" x64 >nul
if !ERRORLEVEL! neq 0 (
    echo ERROR: Failed to initialize x64 environment.
    exit /b 1
)
echo OK - Environment initialized.

echo [3/4] Building all targets (rme + tests)...
cmake --build "%BUILD_DIR%" --parallel
if !ERRORLEVEL! neq 0 (
    echo ERROR: Build failed. Fix the compile errors above.
    exit /b 1
)
echo OK - Build complete.

echo [4/4] Running CTest...
ctest --test-dir "%BUILD_DIR%" --output-on-failure
if !ERRORLEVEL! neq 0 (
    echo ERROR: Tests failed.
    exit /b 1
)

echo.
echo ========================================================
echo   ALL TESTS PASSED
echo ========================================================
echo.

endlocal
