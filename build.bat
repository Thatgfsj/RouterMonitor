@echo off
REM ============================================================
REM RouterMonitor - one-click build script (Windows / MSVC)
REM Usage: build.bat [Release|Debug]  (default Release)
REM ============================================================
setlocal enabledelayedexpansion

set CFG=%1
if "%CFG%"=="" set CFG=Release

set ROOT=%~dp0
pushd "%ROOT%"

REM Locate Visual Studio
set VCVARS=
for /f "tokens=*" %%i in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul') do (
    set VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat
)

if "%VCVARS%"=="" (
    echo [ERROR] Visual Studio 2022 not found. Install with "Desktop development with C++" workload.
    popd & exit /b 1
)

if not exist "%VCVARS%" (
    echo [ERROR] vcvars64.bat not found at "%VCVARS%"
    popd & exit /b 1
)

echo Using: %VCVARS%
call "%VCVARS%" >nul
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC environment
    popd & exit /b 1
)

REM Configure
echo.
echo === CMake configure (%CFG%) ===
if not exist build mkdir build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%CFG%
if errorlevel 1 ( popd & exit /b 1 )

REM Build
echo.
echo === CMake build ===
cmake --build build --config %CFG% --parallel
if errorlevel 1 ( popd & exit /b 1 )

echo.
echo === Done ===
echo Executable: %ROOT%build\bin\%CFG%\RouterMonitor.exe
popd
endlocal