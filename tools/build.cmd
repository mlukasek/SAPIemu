@echo off
rem Command-line build: tools\build.cmd [debug|release|<preset name>] [test [REGEX]|full]
rem   test        all tests in parallel (release: without the slow CPU exercisers ZEXDOC/ZEXALL/8080EXM)
rem   test REGEX  only the tests whose name matches (e.g. "zps3|rpd1")
rem   full        all tests including the slow ones (preset <preset>-full, release only)
rem Finds Visual Studio 2022 via vswhere, sets up the x64 environment and runs CMake presets.
rem debug/release map to windows-*; any other value is used as a preset name (e.g. local-debug
rem from the uncommitted CMakeUserPresets.json).
setlocal
set PRESET=%1
if "%PRESET%"=="" set PRESET=debug
if /i "%PRESET%"=="debug" set PRESET=windows-debug
if /i "%PRESET%"=="release" set PRESET=windows-release

rem Prefer Visual Studio 2022 (17.x), fall back to the newest installed version.
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -version [17.0^,18.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if "%VSDIR%"=="" for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if "%VSDIR%"=="" (
    echo Visual Studio with C++ tools not found.
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

pushd "%~dp0.."
cmake --preset %PRESET% || goto :fail
cmake --build --preset %PRESET% || goto :fail
if /i "%2"=="test" if "%~3"=="" ctest --preset %PRESET% || goto :fail
if /i "%2"=="test" if not "%~3"=="" ctest --preset %PRESET% -R "%~3" || goto :fail
if /i "%2"=="full" ctest --preset %PRESET%-full || goto :fail
popd
exit /b 0
:fail
popd
exit /b 1
