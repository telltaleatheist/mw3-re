@echo off
rem Builds the 32-bit Indeo helper mw3indeo.exe (docs/fmv.md, The Indeo
rem helper) into build-win\, beside mw3.exe: clang-cl -m32 against Visual
rem Studio's x86 libraries and the Windows SDK's (the default C++ workload
rem has both). One file, Windows' own libraries only, the CRT linked in; a
rem GUI program, so no console opens when mw3 starts it.
rem Usage: tools\windows\build_indeo.cmd [output folder, default build-win]
setlocal
cd /d "%~dp0\..\.."
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS=%%i
if not defined VS (echo No Visual Studio with the C++ tools found. & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul || exit /b 1
set OUT=%~1
if "%OUT%"=="" set OUT=build-win
if not exist "%OUT%" mkdir "%OUT%"
clang-cl -m32 /nologo /std:c++17 /O2 /MT /EHsc /W3 /Isrc tools\indeo\mw3indeo.cpp /Fo"%OUT%\mw3indeo.obj" /Fe:"%OUT%\mw3indeo.exe" ^
  /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup vfw32.lib msvfw32.lib avifil32.lib psapi.lib user32.lib || exit /b 1
echo Built %OUT%\mw3indeo.exe
