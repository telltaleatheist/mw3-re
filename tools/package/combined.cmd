@echo off
rem The combined package for a release on Windows (docs/release.md, The combined package): MechWarrior 3 and the
rem launcher from a vX.Y.Z tag on main, Pirate's Moon from a pm-vX.Y.Z tag on pm, each built in a clean worktree of its
rem own; then, in build-pkg\dist\windows\:
rem   MW3-Rebuild-Setup-<v>.exe and MW3-Rebuild-<v>-win64.zip   the launcher, mw3 and mw3pm (the updater takes the Setup)
rem   MW3-alone-Setup-<v>.exe and MW3-alone-<v>-win64.zip       the launcher and mw3 (the updater never takes them)
rem   PM-Rebuild-Setup-<pm v>.exe and mw3pm-<pm v>-win64.zip     mw3pm alone (branch pm's own package)
rem and their SHA256SUMS (tools\package\sums.py; again over every platform's files once collected).
rem Usage: tools\package\combined.cmd <main ref> <pm ref> [<mw1 ref>]   e.g. v1.1.0 pm-v1.0.2 (origin/main origin/pm for a check)
rem With a third ref, MechWarrior 1 Remake (branch mw1) is built too and goes beside the launcher in the
rem MW3-Rebuild package (mw1.exe and mw1data\); MW3 alone and PM alone never carry it. With MW3_PKG_WITH_PORT=1 it
rem also brings Elthial's MechWarrior (1989) SDL port for PLAY ORIGINAL: tools\package\mw1port\build_port.cmd at its
rem pins with the proposed patches, staged in build-pkg\mw1port and put in MechWarrior1989\ beside the launcher.
rem Off: cut for now (2026-11-20).
rem The worktrees are build-pkg\main, build-pkg\pm and build-pkg\mw1 (ours, replaced each run); deps\ is joined into each.
setlocal
cd /d "%~dp0\..\.."
set ROOT=%CD%
if "%~2"=="" (echo Usage: tools\package\combined.cmd ^<main ref^> ^<pm ref^> & exit /b 2)
set W=%ROOT%\build-pkg
set OUT=%W%\dist\windows
if not exist "%W%" mkdir "%W%"
call :worktree main %1 || exit /b 1
call :worktree pm %2 || exit /b 1
if not "%~3"=="" call :worktree mw1 %3 || exit /b 1
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%" || exit /b 1

rem Pirate's Moon alone (pm's own script), which also leaves mw3pm.exe built.
pushd "%W%\pm" || exit /b 1
call tools\windows\package.cmd || (popd & exit /b 1)
popd
copy /y "%W%\pm\build-win\dist\PM-Rebuild-Setup-*.exe" "%OUT%\" >nul || exit /b 1
copy /y "%W%\pm\build-win\dist\mw3pm-*-win64.zip" "%OUT%\" >nul || exit /b 1

rem MechWarrior 1 Remake, when asked for: mw1.exe and mw1data\ in its build tree, for the combined package.
if not "%~3"=="" call :mw1 || exit /b 1
if not "%~3"=="" if "%MW3_PKG_WITH_PORT%"=="1" call :mw1port || exit /b 1

rem MechWarrior 3 alone, then the combined one from the same build.
pushd "%W%\main" || exit /b 1
set MW3_PKG_STEM=MW3-alone
call tools\windows\package.cmd || (popd & exit /b 1)
set MW3_PKG_STEM=
set MW3_PKG_KEEP_BUILD=1
set MW3_PKG_PM_BIN=%W%\pm\build-win-pkg\mw3pm.exe
if not "%~3"=="" set MW3_PKG_MW1_DIR=%W%\mw1\build-win-pkg
if not "%~3"=="" if "%MW3_PKG_WITH_PORT%"=="1" set MW3_PKG_MW1_PORT_DIR=%W%\mw1port
call tools\windows\package.cmd || (popd & exit /b 1)
set MW3_PKG_KEEP_BUILD=
set MW3_PKG_PM_BIN=
set MW3_PKG_MW1_DIR=
set MW3_PKG_MW1_PORT_DIR=
popd
copy /y "%W%\main\build-win\dist\MW3-alone-Setup-*.exe" "%OUT%\" >nul || exit /b 1
copy /y "%W%\main\build-win\dist\MW3-alone-*-win64.zip" "%OUT%\" >nul || exit /b 1
copy /y "%W%\main\build-win\dist\MW3-Rebuild-Setup-*.exe" "%OUT%\" >nul || exit /b 1
copy /y "%W%\main\build-win\dist\MW3-Rebuild-*-win64.zip" "%OUT%\" >nul || exit /b 1
python tools\package\sums.py "%OUT%" || exit /b 1
dir /b "%OUT%"
exit /b 0

rem mw1 (its branch's root CMakeLists adds mw1/ with MW3_BUILD_MW1): a package build, MW3_PACKAGE on, so no
rem development game paths and a windowed program; the build copies its DLLs and mw1data\ beside mw1.exe.
:mw1
setlocal
pushd "%W%\mw1" || exit /b 1
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS=%%i
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || (popd & exit /b 1)
for /d %%d in (deps\SDL3-3*) do set SDL3_DIR=%CD%\%%d\cmake
for /d %%d in (deps\SDL3_ttf-*) do set SDL3_ttf_DIR=%CD%\%%d\cmake
for /d %%d in (deps\SDL3_sound-*) do set SDL3_sound_ROOT=%CD%\%%d
if exist build-win-pkg rmdir /s /q build-win-pkg
cmake -S . -B build-win-pkg -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ^
  -DMW3_PACKAGE=ON -DMW3_BUILD_MW1=ON "-DSDL3_DIR=%SDL3_DIR%" "-DSDL3_ttf_DIR=%SDL3_ttf_DIR%" || (popd & exit /b 1)
cmake --build build-win-pkg --target mw1 || (popd & exit /b 1)
if not exist build-win-pkg\mw1.exe (echo mw1.exe was not built. & popd & exit /b 1)
if not exist build-win-pkg\mw1data\zbd\mw1image.zbd (echo mw1data\ was not made beside mw1.exe. & popd & exit /b 1)
popd
endlocal
exit /b 0

rem Elthial's port (the main ref's kit, as its package.cmd expects), its sources fetched once into build-deps.
:mw1port
setlocal
set "MW1PORT_SRC=%ROOT%\build-deps\mw1port\src"
for /d %%d in ("%ROOT%\deps\SDL3-3*") do set "SDL3_DIR=%%~fd\cmake"
if not exist "%MW1PORT_SRC%\port\.git" call "%W%\main\tools\package\mw1port\build_port.cmd" fetch || exit /b 1
call "%W%\main\tools\package\mw1port\build_port.cmd" "%W%\mw1port" --proposed >nul || exit /b 1
if not exist "%W%\mw1port\mechwarrior.exe" (echo the port was not staged in %W%\mw1port & exit /b 1)
for /f "tokens=2,4" %%a in ('findstr /c:"  port  " "%W%\mw1port\licences\SOURCES.txt"') do echo mw1port: %%b
endlocal
exit /b 0

rem A clean worktree at the ref in build-pkg\<name>: an earlier one of ours removed first (only these two paths),
rem deps\ (the SDL, FFmpeg and libarchive builds package.cmd reads) joined in.
:worktree
set DIR=%W%\%1
if exist "%DIR%" (
  if exist "%DIR%\deps" rmdir "%DIR%\deps"
  git worktree remove --force "%DIR%" || (echo %DIR% is not a worktree of ours: remove it yourself. & exit /b 1)
)
git worktree prune
git worktree add --detach "%DIR%" %2 >nul || exit /b 1
if exist "%ROOT%\deps" mklink /J "%DIR%\deps" "%ROOT%\deps" >nul || exit /b 1
for /f "usebackq delims=" %%v in (`git -C "%DIR%" describe --tags --always --dirty`) do echo %1: %%v
exit /b 0
