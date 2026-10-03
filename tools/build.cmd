@echo off
rem Command-line build: tools\build.cmd [debug|release] [test]
rem Finds Visual Studio 2022 via vswhere, sets up the x64 environment and runs CMake presets.
setlocal
set CFG=%1
if "%CFG%"=="" set CFG=debug
set PRESET=windows-%CFG%

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
if /i "%2"=="test" ctest --preset %PRESET% || goto :fail
popd
exit /b 0
:fail
popd
exit /b 1
