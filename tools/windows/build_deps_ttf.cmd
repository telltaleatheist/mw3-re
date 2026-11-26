@echo off
rem SDL3_ttf on Windows (docs/deps.md): SDL3_ttf 3.2.2 with FreeType 2.14.3 built in, from the same pinned sources
rem as tools/deps/build_deps.sh, each checked by its sha256, built with clang-cl, CMake and Ninja into deps\ttf
rem (bin\SDL3_ttf.dll, lib, cmake), where package.cmd finds it. SDL3_ttf's own Windows release carries FreeType
rem 2.13.2, which draws TTF text a few pixels apart from the Mac and Linux (2.14.3). No HarfBuzz or PlutoSVG, as on
rem the Mac and Linux. It links SDL3 from deps\SDL3-3*. Usage:
rem   tools\windows\build_deps_ttf.cmd
setlocal
cd /d "%~dp0\..\.."
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS=%%i
if not defined VS (echo No Visual Studio with the C++ tools found. & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
for /d %%d in (deps\SDL3-3*) do set SDL3_DIR=%CD%\%%d\cmake
if not defined SDL3_DIR (echo No deps\SDL3-3*: unpack SDL3's -VC.zip release there first ^(docs/windows.md^). & exit /b 1)

set DL=%CD%\build-deps\downloads
set WORK=%CD%\build-deps\win\work-ttf
set PREFIX=%CD%\deps\ttf
rem the same with forward slashes: CMake reads a backslash in a path as an escape
set PREFIXF=%PREFIX:\=/%
set SDL3_DIRF=%SDL3_DIR:\=/%
if not exist "%DL%" mkdir "%DL%"

call :fetch freetype-2.14.3.tar.xz https://download.savannah.gnu.org/releases/freetype/freetype-2.14.3.tar.xz 36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f || exit /b 1
call :fetch SDL3_ttf-3.2.2.tar.gz https://github.com/libsdl-org/SDL_ttf/releases/download/release-3.2.2/SDL3_ttf-3.2.2.tar.gz 63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d || exit /b 1
echo sources: both match their pins

if exist "%WORK%" rmdir /s /q "%WORK%"
if exist "%PREFIX%" rmdir /s /q "%PREFIX%"
mkdir "%WORK%" "%PREFIX%"
for %%f in (freetype-2.14.3.tar.xz SDL3_ttf-3.2.2.tar.gz) do tar -xf "%DL%\%%f" -C "%WORK%" || exit /b 1

set COMMON=-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl "-DCMAKE_INSTALL_PREFIX=%PREFIXF%" "-DCMAKE_PREFIX_PATH=%PREFIXF%" -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL

rem FreeType static, inside SDL3_ttf.dll, with none of its optional libraries (the game's fonts need none).
set OPTS=-DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
call :build "%WORK%\freetype-2.14.3" || exit /b 1
echo built FreeType

set OPTS=-DBUILD_SHARED_LIBS=ON -DSDLTTF_VENDORED=OFF -DSDLTTF_HARFBUZZ=OFF -DSDLTTF_PLUTOSVG=OFF -DSDLTTF_SAMPLES=OFF "-DSDL3_DIR=%SDL3_DIRF%"
call :build "%WORK%\SDL3_ttf-3.2.2" || exit /b 1
if not exist "%PREFIX%\bin\SDL3_ttf.dll" (echo SDL3_ttf.dll was not installed. & exit /b 1)
echo built SDL3_ttf

rem Their licences, for the package (FreeType's inside SDL3_ttf.dll), one folder each (SDL3_ttf's install puts
rem its own in share\licenses\SDL3_ttf, a second copy).
rmdir /s /q "%PREFIX%\share\licenses\SDL3_ttf" 2>nul
call :licences SDL3_ttf-3.2.2 LICENSE.txt || exit /b 1
call :licences freetype-2.14.3 LICENSE.TXT docs\FTL.TXT docs\GPLv2.TXT || exit /b 1
echo prefix: %PREFIX%
exit /b 0

:fetch
if not exist "%DL%\%1" curl -sSfL -o "%DL%\%1" %2 || exit /b 1
for /f "usebackq delims=" %%h in (`powershell -NoProfile -Command "(Get-FileHash -Algorithm SHA256 '%DL%\%1').Hash.ToLower()"`) do set GOT=%%h
if not "%GOT%"=="%3" (echo %1: sha256 %GOT%, pinned %3: refused & exit /b 1)
exit /b 0

:licences
rem %1 a source folder, then its licence files
set LIC=%PREFIX%\share\licenses\%1
mkdir "%LIC%" 2>nul
set SRCDIR=%WORK%\%1
:licences_next
shift
if "%~1"=="" exit /b 0
copy /y "%SRCDIR%\%~1" "%LIC%\" >nul || exit /b 1
goto licences_next

:build
rem one library: %1 its source folder, OPTS its options
cmake -S "%~1" -B "%~1\_build" %COMMON% %OPTS% >nul || exit /b 1
cmake --build "%~1\_build" >nul || exit /b 1
cmake --install "%~1\_build" >nul || exit /b 1
exit /b 0
