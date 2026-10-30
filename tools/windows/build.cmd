@echo off
rem Builds mw3 on Windows with clang-cl (the Mac's compiler family, so the
rem floats round alike) and Ninja, against Visual Studio's headers and
rem libraries. SDL3 and SDL3_ttf: their VC development zips unpacked into
rem deps\ (docs/windows.md). Usage: tools\windows\build.cmd [target...]
setlocal
cd /d "%~dp0\..\.."
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS=%%i
if not defined VS (echo No Visual Studio with the C++ tools found. & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
for /d %%d in (deps\SDL3-3*) do set SDL3_DIR=%CD%\%%d\cmake
for /d %%d in (deps\SDL3_ttf-*) do set SDL3_ttf_DIR=%CD%\%%d\cmake
if not exist build-win\build.ninja (
  cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ^
    "-DSDL3_DIR=%SDL3_DIR%" "-DSDL3_ttf_DIR=%SDL3_ttf_DIR%" || exit /b 1
)
set TARGETS=%*
if "%TARGETS%"=="" set TARGETS=mw3
cmake --build build-win --target %TARGETS%
