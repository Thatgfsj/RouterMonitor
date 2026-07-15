@echo off
REM ============================================================
REM RouterMonitor - one-click build script
REM Tries MinGW (O:\llvm-mingw\mingw64\bin) first, falls back to VS2022.
REM Usage: build.bat [Release|Debug]  (default Release)
REM ============================================================
setlocal enabledelayedexpansion

set CFG=%1
if "%CFG%"=="" set CFG=Release

set ROOT=%~dp0
pushd "%ROOT%"

REM ----- Pick a compiler toolchain -----
set CMAKE=
set GENERATOR=
set TOOLCHAIN_DESC=

REM 1) Try MinGW (clang/gcc + bundled cmake).
if exist "O:\llvm-mingw\mingw64\bin\cmake.exe" (
    set "PATH=O:\llvm-mingw\mingw64\bin;%PATH%"
    set CMAKE=O:\llvm-mingw\mingw64\bin\cmake.exe
    set GENERATOR=-G "MinGW Makefiles"
    set TOOLCHAIN_DESC=MinGW (O:\llvm-mingw\mingw64)
    goto :configure
)

REM 2) Fall back to Visual Studio 2022 via vswhere + vcvars.
set VCVARS=
for /f "tokens=*" %%i in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul') do (
    set VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat
)

if "%VCVARS%"=="" (
    echo [ERROR] No compiler found.
    echo         Install either MinGW at O:\llvm-mingw\mingw64\ ^<https://winlibs.com/^>
    echo         or Visual Studio 2022 with "Desktop development with C++".
    popd & exit /b 1
)

if not exist "%VCVARS%" (
    echo [ERROR] vcvars64.bat not found at "%VCVARS%"
    popd & exit /b 1
)

call "%VCVARS%" >nul
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC environment
    popd & exit /b 1
)
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] cmake not found in PATH (after vcvars). Install CMake or add it to PATH.
    popd & exit /b 1
)
set CMAKE=cmake
set GENERATOR=-G "Visual Studio 17 2022" -A x64
set TOOLCHAIN_DESC=Visual Studio 2022

:configure
echo Using: %TOOLCHAIN_DESC%

if not exist build mkdir build
echo.
echo === CMake configure (%CFG%) ===
%CMAKE% -S . -B build %GENERATOR% -DCMAKE_BUILD_TYPE=%CFG%
if errorlevel 1 ( popd & exit /b 1 )

echo.
echo === CMake build ===
%CMAKE% --build build --config %CFG% --parallel
if errorlevel 1 ( popd & exit /b 1 )

echo.
echo === Done ===
echo Executable: %ROOT%build\bin\%CFG%\RouterMonitor.exe
popd
endlocal