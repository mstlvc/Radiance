@echo off
rem Builds Radiance.exe with the Visual Studio 2022 C++ tools (CMake + Ninja ship with them).
rem   build.bat            Release build into build\Release\Radiance.exe
rem   build.bat debug      Debug build into build\Debug\Radiance.exe
setlocal

set CONFIG=Release
if /I "%~1"=="debug" set CONFIG=Debug

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set VSINSTALL=
for /f "usebackq tokens=*" %%i in (`call "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
    echo Visual Studio 2022 with the C++ tools was not found.
    exit /b 1
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || exit /b 1

set ROOT=%~dp0
cmake -S "%ROOT%." -B "%ROOT%build\%CONFIG%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% || exit /b 1
cmake --build "%ROOT%build\%CONFIG%" || exit /b 1
echo.
echo Built %ROOT%build\%CONFIG%\Radiance.exe
