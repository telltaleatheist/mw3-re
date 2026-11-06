@echo off
rem Installs the weapon patch's stand-in IFORCE2.dll in a game folder. See HOW-TO-INSTALL.txt.
rem No parenthesised blocks here, and paths only echoed in quotes: a game folder may be "Program Files (x86)".
setlocal EnableExtensions DisableDelayedExpansion
set "OURS=%~dp0install-files\IFORCE2-standin.dll"
set "ROWS=%~dp0install-files\weapon-rows.txt"
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

if not exist "%GAME%\Mech3.exe" if not exist "%GAME%\Mech3fixup.exe" goto :no_exe
if exist "%GAME%\IFORCE2o.dll" goto :already
if not exist "%GAME%\IFORCE2.dll" goto :no_dll
"%FC%" /b "%GAME%\IFORCE2.dll" "%OURS%" >nul 2>&1
if not errorlevel 1 goto :overwritten

ren "%GAME%\IFORCE2.dll" IFORCE2o.dll >nul 2>&1
if errorlevel 1 goto :cant_rename
copy /b "%OURS%" "%GAME%\IFORCE2.dll" >nul 2>&1
if errorlevel 1 goto :cant_copy
"%FC%" /b "%GAME%\IFORCE2.dll" "%OURS%" >nul 2>&1
if errorlevel 1 goto :cant_copy

echo Installed in "%GAME%":
echo   Your original IFORCE2.dll is now IFORCE2o.dll. Keep it there: the game needs it.
echo   IFORCE2.dll is now the weapon patch. The game's exe was not changed.
if exist "%GAME%\weapon-rows.txt" goto :rows_kept
copy /b "%ROWS%" "%GAME%\weapon-rows.txt" >nul 2>&1
if errorlevel 1 goto :cant_rows
echo   weapon-rows.txt is the list of weapons to add. It adds the plain PPC.
goto :done
:rows_kept
echo   weapon-rows.txt was already there, so it was kept as it is.
:done
echo.
echo Start the game the way you always do. Each start writes one line to weapon-patch.log
echo in the game folder, saying how many weapons it added or why it added none.
echo To undo this, use restore.cmd.
echo.
pause
exit /b 0

:not_unzipped
echo STOPPED: install-files\IFORCE2-standin.dll isn't beside this script.
echo Unzip the whole zip to a folder first, and run install.cmd from there.
goto :failed
:usage
echo Drop your game folder onto install.cmd, or run:
echo   install.cmd "C:\path\to\your\game folder"
echo Nothing was changed.
goto :failed
:no_exe
echo STOPPED: there is no Mech3.exe or Mech3fixup.exe in "%GAME%".
echo That isn't a MechWarrior 3 folder. Nothing was changed.
goto :failed
:already
echo STOPPED: IFORCE2o.dll is already in "%GAME%".
echo The patch looks installed already. To undo it, use restore.cmd. Nothing was changed.
goto :failed
:no_dll
echo STOPPED: there is no IFORCE2.dll in "%GAME%".
echo The game needs its own IFORCE2.dll: put it back first, then run this again. Nothing was changed.
goto :failed
:overwritten
echo STOPPED: the IFORCE2.dll in "%GAME%" is already this patch's file,
echo and there is no IFORCE2o.dll, so the game's original has been overwritten.
echo The game can't start like this. Put the original back first, from a backup, your disc or a
echo fresh copy of the game: it is 61,440 bytes. Then run this again. Nothing was changed.
goto :failed
:cant_rename
echo STOPPED: couldn't rename IFORCE2.dll in "%GAME%".
echo Close the game if it's running. A folder under Program Files may need this run as administrator.
echo Nothing was changed.
goto :failed
:cant_copy
del "%GAME%\IFORCE2.dll" >nul 2>&1
ren "%GAME%\IFORCE2o.dll" IFORCE2.dll >nul 2>&1
echo STOPPED: couldn't copy the patch into "%GAME%".
echo Your original IFORCE2.dll has been put back. Check that it is there before starting the game.
goto :failed
:cant_rows
echo.
echo PROBLEM: the patch is in, but weapon-rows.txt couldn't be copied, so no weapons will be added.
echo Copy install-files\weapon-rows.txt to "%GAME%" by hand.
goto :failed
:failed
echo.
pause
exit /b 1
