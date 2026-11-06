@echo off
rem Takes the weapon patch out of a game folder and puts the game's own IFORCE2.dll back. See INSTALL.txt.
rem It deletes only our own file, compared byte for byte, and only with the original there to put back.
setlocal EnableExtensions DisableDelayedExpansion
set "OURS=%~dp0install-files\IFORCE2-standin.dll"
set "FC=%SystemRoot%\System32\fc.exe"

if not exist "%OURS%" goto :not_unzipped
if not "%~1"=="" goto :from_argument
set "GAME=%~dp0"
if exist "%~dp0Mech3.exe" goto :have_game
if exist "%~dp0Mech3fixup.exe" goto :have_game
goto :usage
:from_argument
set "GAME=%~f1"
:have_game
if "%GAME:~-1%"=="\" set "GAME=%GAME:~0,-1%"

if not exist "%GAME%\IFORCE2o.dll" goto :no_original
if not exist "%GAME%\IFORCE2.dll" goto :no_ours
"%FC%" /b "%GAME%\IFORCE2.dll" "%OURS%" >nul 2>&1
if errorlevel 1 goto :not_ours

del "%GAME%\IFORCE2.dll" >nul 2>&1
if exist "%GAME%\IFORCE2.dll" goto :cant_delete
ren "%GAME%\IFORCE2o.dll" IFORCE2.dll >nul 2>&1
if errorlevel 1 goto :cant_rename

echo Restored in "%GAME%":
echo   The patch's IFORCE2.dll is gone, and your original is IFORCE2.dll again.
echo   weapon-rows.txt and weapon-patch.log were left there; they do nothing now.
echo.
echo A loadout saved with a new weapon loses that weapon without the patch, with no message.
echo.
pause
exit /b 0

:not_unzipped
echo STOPPED: install-files\IFORCE2-standin.dll isn't beside this script.
echo Unzip the whole zip to a folder first, and run restore.cmd from there.
goto :failed
:usage
echo Drop your game folder onto restore.cmd, or run:
echo   restore.cmd "C:\path\to\your\game folder"
echo Nothing was changed.
goto :failed
:no_original
echo STOPPED: there is no IFORCE2o.dll in "%GAME%", so there is nothing to put back.
echo The patch isn't installed there, or its original is gone. Nothing was changed.
goto :failed
:no_ours
echo STOPPED: "%GAME%" has IFORCE2o.dll but no IFORCE2.dll.
echo Rename IFORCE2o.dll to IFORCE2.dll by hand to put your original back. Nothing was changed.
goto :failed
:not_ours
echo STOPPED: the IFORCE2.dll in "%GAME%" isn't this patch's file, so it was left alone.
echo It may be from another version of this patch or from something else. Nothing was changed.
goto :failed
:cant_delete
echo STOPPED: couldn't remove the patch's IFORCE2.dll from "%GAME%".
echo Close the game if it's running. A folder under Program Files may need this run as administrator.
echo Nothing was changed.
goto :failed
:cant_rename
echo STOPPED: the patch's file is gone, but IFORCE2o.dll couldn't be renamed in "%GAME%".
echo The game can't start like this. Rename IFORCE2o.dll to IFORCE2.dll by hand.
goto :failed
:failed
echo.
pause
exit /b 1
