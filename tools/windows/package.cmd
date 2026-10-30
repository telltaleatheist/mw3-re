@echo off
rem Builds the package for other players (docs/windows.md, Packaging):
rem build-win\dist\mw3-<git describe>-win64\ and its .zip, with mw3.exe, the
rem 32-bit Indeo helper mw3indeo.exe (the movies through the player's own
rem codec: no FFmpeg, no importer), every DLL mw3 loads (SDL3, SDL3_ttf,
rem SDL3_sound, the Visual C++ runtime), README.txt, THIRD-PARTY.txt and the
rem libraries' licence files. Nothing from the game. Its own build tree, build-win-pkg,
rem configured with MW3_PACKAGE (no development copy of the game in the exes).
rem Usage: tools\windows\package.cmd
setlocal
cd /d "%~dp0\..\.."
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS=%%i
if not defined VS (echo No Visual Studio with the C++ tools found. & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
for /d %%d in (deps\SDL3-3*) do set SDL3_DIR=%CD%\%%d\cmake
for /d %%d in (deps\SDL3_ttf-*) do set SDL3_ttf_DIR=%CD%\%%d\cmake
for /d %%d in (deps\SDL3_sound-*) do set SDL3_sound_ROOT=%CD%\%%d
if not defined SDL3_sound_ROOT echo Warning: no deps\SDL3_sound-*: the package's music plays .wav only.

rem The build, from scratch each time so the version and the switches are this checkout's.
if exist build-win-pkg rmdir /s /q build-win-pkg
cmake -S . -B build-win-pkg -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ^
  -DMW3_PACKAGE=ON "-DSDL3_DIR=%SDL3_DIR%" "-DSDL3_ttf_DIR=%SDL3_ttf_DIR%" || exit /b 1
cmake --build build-win-pkg --target mw3 || exit /b 1
rem The helper, 32-bit (its own vcvarsall x86, in a child shell).
cmd /c tools\windows\build_indeo.cmd build-win-pkg || exit /b 1

for /f "usebackq delims=" %%v in (`git describe --tags --always --dirty`) do set VER=%%v
set NAME=mw3-%VER%-win64
set DIST=build-win\dist\%NAME%
if exist "%DIST%" rmdir /s /q "%DIST%"
if exist "build-win\dist\%NAME%.zip" del "build-win\dist\%NAME%.zip"
mkdir "%DIST%\licenses" || exit /b 1

rem The two programs and the DLLs mw3's build copied beside it.
copy /y build-win-pkg\mw3.exe "%DIST%\" >nul || exit /b 1
copy /y build-win-pkg\mw3indeo.exe "%DIST%\" >nul || exit /b 1
copy /y build-win-pkg\*.dll "%DIST%\" >nul || exit /b 1
rem The Visual C++ runtime, app-local (a player may not have the redistributable).
for /d %%d in ("%VCToolsRedistDir%x64\Microsoft.VC*.CRT") do (
  for %%f in (msvcp140.dll vcruntime140.dll vcruntime140_1.dll) do if exist "%%d\%%f" copy /y "%%d\%%f" "%DIST%\" >nul
)

rem The read-me, the notices, and each library's own licence files.
copy /y tools\windows\dist\README.txt "%DIST%\" >nul || exit /b 1
copy /y tools\windows\dist\THIRD-PARTY.txt "%DIST%\" >nul || exit /b 1
for /d %%d in (deps\SDL3*) do (
  mkdir "%DIST%\licenses\%%~nxd" 2>nul
  for %%f in ("%%d\LICENSE*" "%%d\COPYING*" "%%d\README-SDL.txt") do copy /y "%%f" "%DIST%\licenses\%%~nxd\" >nul
)
for /d %%d in ("%DIST%\licenses\*") do dir /b /a-d "%%d" >nul 2>nul || echo Warning: no licence file found for %%~nxd: copy it into licenses\%%~nxd by hand.
mkdir "%DIST%\licenses\pl_mpeg"
copy /y third_party\pl_mpeg\README.md "%DIST%\licenses\pl_mpeg\" >nul || exit /b 1
mkdir "%DIST%\licenses\miniz"
copy /y third_party\miniz\LICENSE "%DIST%\licenses\miniz\" >nul || exit /b 1

rem Every DLL the programs load is in the folder or part of Windows; no game file is.
powershell -NoProfile -ExecutionPolicy Bypass -File tools\windows\check_dist.ps1 "%DIST%" || exit /b 1

tar -a -c -f "build-win\dist\%NAME%.zip" -C build-win\dist "%NAME%" || exit /b 1
echo Package: %DIST%
echo Zip:     build-win\dist\%NAME%.zip
