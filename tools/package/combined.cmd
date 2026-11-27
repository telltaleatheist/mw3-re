@echo off
rem The launcher's package on Windows (DEPLOY.md): from a vX.Y.Z tag on the hub's main, built in a clean worktree
rem of its own; then, in build-pkg\dist\windows\:
rem   MW3-Rebuild-Setup-<v>.exe and MW3-Rebuild-<v>-win64.zip   the launcher (the updater takes the Setup); it
rem                                                              downloads each game (docs\launcher.md, Games on
rem                                                              demand). Each game's package is built in its repo.
rem and their SHA256SUMS (tools\package\sums.py; again over every platform's files once collected).
rem Usage: tools\package\combined.cmd <ref>   e.g. v1.6.1
rem The worktree is build-pkg\main (ours, replaced each run); deps\ is joined into it.
setlocal
cd /d "%~dp0\..\.."
set ROOT=%CD%
if "%~1"=="" (echo Usage: tools\package\combined.cmd ^<ref^> & exit /b 2)
set W=%ROOT%\build-pkg
set OUT=%W%\dist\windows
if not exist "%W%" mkdir "%W%"
set DIR=%W%\main
if exist "%DIR%" (
  if exist "%DIR%\deps" rmdir "%DIR%\deps"
  git worktree remove --force "%DIR%" || (echo %DIR% is not a worktree of ours: remove it yourself. & exit /b 1)
)
git worktree prune
git worktree add --detach "%DIR%" %1 >nul || exit /b 1
git -C "%DIR%" submodule update --init --quiet || exit /b 1
if exist "%ROOT%\deps" mklink /J "%DIR%\deps" "%ROOT%\deps" >nul || exit /b 1
for /f "usebackq delims=" %%v in (`git -C "%DIR%" describe --tags --always --dirty`) do echo main: %%v
rem The built-in games catalog is the live one, its badges shipped (DEPLOY.md, step 1): refused otherwise.
python tools\package\check_catalog.py "%DIR%" || exit /b 1
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%" || exit /b 1

pushd "%DIR%" || exit /b 1
call tools\windows\package.cmd || (popd & exit /b 1)
popd
copy /y "%DIR%\build-win\dist\MW3-Rebuild-Setup-*.exe" "%OUT%\" >nul || exit /b 1
copy /y "%DIR%\build-win\dist\MW3-Rebuild-*-win64.zip" "%OUT%\" >nul || exit /b 1
python tools\package\sums.py "%OUT%" || exit /b 1
dir /b "%OUT%"
exit /b 0
