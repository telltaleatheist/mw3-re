@echo off
rem Builds the launcher's package for players (docs/launcher.md, Games on demand: the launcher downloads each game,
rem so no game is in it): build-win\dist\MW3-Rebuild-<git describe>-win64\ and its .zip, with mw3launcher.exe, the
rem DLLs it loads (SDL3, the Visual C++ runtime), README.txt, THIRD-PARTY.txt and the libraries' licence files, and
rem the installer MW3-Rebuild-Setup-<git describe>.exe (Inno Setup, tools\windows\installer.iss). The names and the
rem AppId are every earlier launcher's, so the updater takes it. Its own build tree, build-win-pkg, configured with
rem MW3_PACKAGE.
rem Usage: tools\windows\package.cmd
setlocal
cd /d "%~dp0\..\.."
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS=%%i
if not defined VS (echo No Visual Studio with the C++ tools found. & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
for /d %%d in (deps\SDL3-3*) do set SDL3_DIR=%CD%\%%d\cmake
rem SDL3_ttf with FreeType 2.14.3, as on the Mac and Linux (tools\windows\build_deps_ttf.cmd; its -VC.zip release's
rem FreeType 2.13.2 draws TTF text a few pixels apart, docs/deps.md).
if not exist deps\ttf\cmake\SDL3_ttfConfig.cmake (echo No deps\ttf: run tools\windows\build_deps_ttf.cmd first. & exit /b 1)
if not exist deps\ttf\share\licenses\freetype-2.14.3\FTL.TXT (echo deps\ttf has no licences: run tools\windows\build_deps_ttf.cmd again. & exit /b 1)
set SDL3_ttf_DIR=%CD%\deps\ttf\cmake
rem The same FreeType for the launcher's RmlUi (linked in, third_party/rmlui).
set Freetype_ROOT=%CD%\deps\ttf
rem The setup kit's archives: a package always reads them.
if not exist deps\archive\lib\archive.lib (echo No deps\archive: run tools\windows\build_deps_archive.cmd first. & exit /b 1)
if not exist deps\archive\share\licenses\libarchive-3.7.9\COPYING (echo deps\archive has no licences: run tools\windows\build_deps_archive.cmd again. & exit /b 1)

set MW3_PKG_STEM=MW3-Rebuild

rem The build, from scratch each time so the version and the switches are this checkout's.
if exist build-win-pkg rmdir /s /q build-win-pkg
cmake -S . -B build-win-pkg -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ^
  -DMW3_PACKAGE=ON "-DSDL3_DIR=%SDL3_DIR%" "-DSDL3_ttf_DIR=%SDL3_ttf_DIR%" || exit /b 1
cmake --build build-win-pkg --target mw3launcher || exit /b 1

rem The program's own version (CMakeLists.txt: never older than a release).
set /p VER=<build-win-pkg\mw3_version.txt
if not defined VER set VER=dev
set NAME=%MW3_PKG_STEM%-%VER%-win64
set DIST=build-win\dist\%NAME%
if exist "%DIST%" rmdir /s /q "%DIST%"
if exist "build-win\dist\%NAME%.zip" del "build-win\dist\%NAME%.zip"
mkdir "%DIST%\licenses" || exit /b 1

rem The launcher and the DLLs its build copied beside it.
copy /y build-win-pkg\mw3launcher.exe "%DIST%\" >nul || exit /b 1
copy /y build-win-pkg\*.dll "%DIST%\" >nul || exit /b 1
rem The Visual C++ runtime, app-local (a player may not have the redistributable).
for /d %%d in ("%VCToolsRedistDir%x64\Microsoft.VC*.CRT") do (
  for %%f in (msvcp140.dll vcruntime140.dll vcruntime140_1.dll) do if exist "%%d\%%f" copy /y "%%d\%%f" "%DIST%\" >nul
)

rem The read-me, the notices, and each library's own licence files.
copy /y tools\windows\dist\README.txt "%DIST%\" >nul || exit /b 1
rem The launcher's art (ours, assets/launcher: generated, with its prompts).
mkdir "%DIST%\assets\launcher\ui"
copy /y assets\launcher\ui\* "%DIST%\assets\launcher\ui\" >nul || exit /b 1
copy /y assets\launcher\*.png "%DIST%\assets\launcher\" >nul || exit /b 1
copy /y assets\launcher\font.ttf "%DIST%\assets\launcher\" >nul || exit /b 1
copy /y assets\launcher\font_body.ttf "%DIST%\assets\launcher\" >nul || exit /b 1
rem The menu's two sounds (ours: assets\launcher\README.md).
copy /y assets\launcher\click.wav "%DIST%\assets\launcher\" >nul || exit /b 1
copy /y assets\launcher\hover.wav "%DIST%\assets\launcher\" >nul || exit /b 1
copy /y tools\windows\dist\THIRD-PARTY.txt "%DIST%\" >nul || exit /b 1
if exist LICENSE copy /y LICENSE "%DIST%\LICENSE.txt" >nul
rem libarchive, zlib and liblzma: linked into mw3launcher.exe, their licences from
rem tools\windows\build_deps_archive.cmd's prefix; SDL3_ttf's and its FreeType's from build_deps_ttf.cmd's.
for /d %%d in (deps\archive\share\licenses\* deps\ttf\share\licenses\*) do (
  mkdir "%DIST%\licenses\%%~nxd" 2>nul
  copy /y "%%d\*" "%DIST%\licenses\%%~nxd\" >nul || exit /b 1
)
rem The other libraries' release folders (not a -VC.zip SDL3_ttf left in deps\: the package's is deps\ttf's).
for /d %%d in (deps\*) do echo %%~nxd| findstr /i /r /c:"^archive$" /c:"^ttf$" /c:"^SDL3_ttf-" >nul || (
  mkdir "%DIST%\licenses\%%~nxd" 2>nul
  for %%f in ("%%d\LICENSE*" "%%d\COPYING*" "%%d\README-SDL.txt") do copy /y "%%f" "%DIST%\licenses\%%~nxd\" >nul
)
for /d %%d in ("%DIST%\licenses\*") do dir /b /a-d "%%d" >nul 2>nul || echo Warning: no licence file found for %%~nxd: copy it into licenses\%%~nxd by hand.
mkdir "%DIST%\licenses\saira-condensed"
copy /y assets\launcher\font-OFL.txt "%DIST%\licenses\saira-condensed\OFL.txt" >nul || exit /b 1
mkdir "%DIST%\licenses\rmlui"
copy /y third_party\rmlui\LICENSE.txt "%DIST%\licenses\rmlui\" >nul || exit /b 1
copy /y third_party\rmlui\Include\RmlUi\Core\Containers\LICENSE.txt "%DIST%\licenses\rmlui\LICENSE-containers.txt" >nul || exit /b 1
mkdir "%DIST%\licenses\miniz"
copy /y extern\re-setup-kit\third_party\miniz\LICENSE "%DIST%\licenses\miniz\" >nul || exit /b 1

rem Every DLL the launcher loads is in the folder or part of Windows; no game file or game program is.
powershell -NoProfile -ExecutionPolicy Bypass -File tools\windows\check_dist.ps1 "%DIST%" || exit /b 1

tar -a -c -f "build-win\dist\%NAME%.zip" -C build-win\dist "%NAME%" || exit /b 1

rem The installer, MW3-Rebuild-Setup-<version>.exe (tools\windows\installer.iss),
rem with Inno Setup's compiler: iscc on the PATH (scoop's shim), else its usual folders.
set ISCC=
for %%i in (iscc.exe) do if not "%%~$PATH:i"=="" set "ISCC=%%~$PATH:i"
for %%d in ("%LOCALAPPDATA%\Programs\Inno Setup 7" "%LOCALAPPDATA%\Programs\Inno Setup 6" "%ProgramFiles(x86)%\Inno Setup 7" "%ProgramFiles(x86)%\Inno Setup 6" "%ProgramFiles%\Inno Setup 7") do (
  if not defined ISCC if exist "%%~d\ISCC.exe" set "ISCC=%%~d\ISCC.exe"
)
if not defined ISCC (
  echo Warning: Inno Setup's ISCC.exe not found, so no installer was made ^(docs/windows.md, Packaging^).
  goto :done
)
"%ISCC%" /Qp "/DAppVersion=%VER%" "/DDist=%CD%\%DIST%" "/DStem=%MW3_PKG_STEM%" "/O%CD%\build-win\dist" tools\windows\installer.iss || exit /b 1
echo Setup:   build-win\dist\%MW3_PKG_STEM%-Setup-%VER%.exe

:done
echo Package: %DIST%
echo Zip:     build-win\dist\%NAME%.zip
