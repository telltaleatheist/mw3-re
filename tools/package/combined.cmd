@echo off
rem The combined package for a release on Windows (docs/release.md, The combined package): MechWarrior 3 and the
rem launcher from a vX.Y.Z tag on main, Pirate's Moon from a pm-vX.Y.Z tag on pm, each built in a clean worktree of its
rem own; then, in build-pkg\dist\windows\:
rem   MW3-Rebuild-Setup-<v>.exe and MW3-Rebuild-<v>-win64.zip   the launcher, mw3 and mw3pm (the updater takes the Setup)
rem   MW3-alone-Setup-<v>.exe and MW3-alone-<v>-win64.zip       the launcher and mw3 (the updater never takes them)
rem   PM-Rebuild-Setup-<pm v>.exe and mw3pm-<pm v>-win64.zip     mw3pm alone (branch pm's own package)
rem and their SHA256SUMS (tools\package\sums.py; again over every platform's files once collected).
rem Usage: tools\package\combined.cmd <main ref> <pm ref>     e.g. v1.1.0 pm-v1.0.2 (origin/main origin/pm for a check)
rem The worktrees are build-pkg\main and build-pkg\pm (ours, replaced each run); deps\ is joined into each.
setlocal
cd /d "%~dp0\..\.."
set ROOT=%CD%
if "%~2"=="" (echo Usage: tools\package\combined.cmd ^<main ref^> ^<pm ref^> & exit /b 2)
set W=%ROOT%\build-pkg
set OUT=%W%\dist\windows
if not exist "%W%" mkdir "%W%"
call :worktree main %1 || exit /b 1
call :worktree pm %2 || exit /b 1
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%" || exit /b 1

rem Pirate's Moon alone (pm's own script), which also leaves mw3pm.exe built.
pushd "%W%\pm" || exit /b 1
call tools\windows\package.cmd || (popd & exit /b 1)
popd
copy /y "%W%\pm\build-win\dist\PM-Rebuild-Setup-*.exe" "%OUT%\" >nul || exit /b 1
copy /y "%W%\pm\build-win\dist\mw3pm-*-win64.zip" "%OUT%\" >nul || exit /b 1

rem MechWarrior 3 alone, then the combined one from the same build.
pushd "%W%\main" || exit /b 1
set MW3_PKG_STEM=MW3-alone
call tools\windows\package.cmd || (popd & exit /b 1)
set MW3_PKG_STEM=
set MW3_PKG_KEEP_BUILD=1
set MW3_PKG_PM_BIN=%W%\pm\build-win-pkg\mw3pm.exe
call tools\windows\package.cmd || (popd & exit /b 1)
set MW3_PKG_KEEP_BUILD=
set MW3_PKG_PM_BIN=
popd
copy /y "%W%\main\build-win\dist\MW3-alone-Setup-*.exe" "%OUT%\" >nul || exit /b 1
copy /y "%W%\main\build-win\dist\MW3-alone-*-win64.zip" "%OUT%\" >nul || exit /b 1
copy /y "%W%\main\build-win\dist\MW3-Rebuild-Setup-*.exe" "%OUT%\" >nul || exit /b 1
copy /y "%W%\main\build-win\dist\MW3-Rebuild-*-win64.zip" "%OUT%\" >nul || exit /b 1
python tools\package\sums.py "%OUT%" || exit /b 1
dir /b "%OUT%"
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
