cmake_minimum_required(VERSION 3.20)
project(battletech_launcher LANGUAGES C CXX)
if(WIN32)
  enable_language(RC)  # the version resource and icon (tools/windows/version.rc.in)
endif()

# The BattleTech launcher on its own (docs/launcher-split.md, c2): no game code. Each game is its own program from
# its own repo, asked through the setup kit's contract (extern/re-setup-kit); the launcher fetches and starts them.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# A build for players (the package scripts): no path of this machine in the program.
option(MW3_PACKAGE "Build the launcher for players: its art beside it, no development paths" OFF)
if(MW3_PACKAGE)
  # A player's build: the family folder's one-time move from MicroProse (launcher_folder.cpp) happens only here.
  add_compile_definitions(MW3_PACKAGE_BUILD)
endif()
# The version --version, the update check and the Windows version resource give: git's describe on the hub's own
# tags (v1.5.0 onward, the same line as every launcher released) and the commit count.
execute_process(COMMAND git describe --tags --match "v[0-9]*" --always --dirty WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE MW3_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git rev-list --count HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE MW3_BUILD_NUMBER OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
# Before the hub's own first tag (v1.6.1) the nearest tag behind main is v1.2.2: the launchers released in between
# were tagged on mech3-re's commits. A build is never older than those: when the describe's tag is not the newest
# vX.Y.Z here, it is "<newest>-dev-g<commit>", which the update check counts as that release (update.h).
# A checkout with no vX.Y.Z tag at all (a bundle, a source tarball): a package refuses to build, as its version would
# rank below every release and be offered each one; a development build is "v0.0.0-dev-g<commit>".
execute_process(COMMAND git tag --list "v[0-9]*" --sort=-v:refname WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE MW3_TAGS OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
string(REGEX MATCH "^[^\n]+" MW3_NEWEST_TAG "${MW3_TAGS}")
if(NOT MW3_NEWEST_TAG)
  if(MW3_PACKAGE)
    message(FATAL_ERROR "No vX.Y.Z tag in this checkout: a package needs the repo's tags for its version (git fetch --tags)")
  endif()
  set(MW3_NEWEST_TAG "v0.0.0")
endif()
string(REGEX MATCH "^v[0-9][^-]*" MW3_DESCRIBED_TAG "${MW3_VERSION}")
if(NOT MW3_DESCRIBED_TAG STREQUAL MW3_NEWEST_TAG)
  execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE MW3_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  set(MW3_DIRTY "")
  if(MW3_VERSION MATCHES "-dirty$")
    set(MW3_DIRTY "-dirty")
  endif()
  if(MW3_COMMIT)
    set(MW3_VERSION "${MW3_NEWEST_TAG}-dev-g${MW3_COMMIT}${MW3_DIRTY}")
  else()  # not a git checkout
    set(MW3_VERSION "${MW3_NEWEST_TAG}-dev")
  endif()
endif()
# The package scripts name their files and the Mac app's version by this one (tools/macos, tools/windows, tools/linux).
file(WRITE ${CMAKE_BINARY_DIR}/mw3_version.txt "${MW3_VERSION}")
if(NOT MW3_BUILD_NUMBER)
  set(MW3_BUILD_NUMBER 0)
endif()

if(MSVC)
  add_compile_options(/W4 "$<$<COMPILE_LANGUAGE:CXX>:/EHsc>")
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The games catalog (docs/launcher-split.md b0): src/upgrades/update/games.json built into the launcher as text,
# written again when the file changes (a configure re-run). One raw literal: MSVC takes up to 16380 bytes in one.
set(MW3_CATALOG_JSON ${CMAKE_CURRENT_SOURCE_DIR}/src/upgrades/update/games.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MW3_CATALOG_JSON})
file(READ ${MW3_CATALOG_JSON} MW3_CATALOG_TEXT)
string(LENGTH "${MW3_CATALOG_TEXT}" MW3_CATALOG_SIZE)
if(MW3_CATALOG_SIZE GREATER 16000 OR MW3_CATALOG_TEXT MATCHES "\\)mw3catalog\"")
  message(FATAL_ERROR "games.json: ${MW3_CATALOG_SIZE} bytes, or it holds the literal's end: split the literal")
endif()
file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/games_catalog.cpp
     CONTENT "// Written by CMake from src/upgrades/update/games.json: do not edit.\nnamespace upgrades::update {\nextern const char kBuiltinCatalog[];\nconst char kBuiltinCatalog[] = R\"mw3catalog(@MW3_CATALOG_TEXT@)mw3catalog\";\n}  // namespace upgrades::update\n"
     @ONLY)
# Where the launcher looks (assets/launcher/sources.json, shipped beside the badges): the same file built in as the
# last resort, written from it here (docs/launcher.md, Where the launcher looks).
set(MW3_SOURCES_JSON ${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher/sources.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MW3_SOURCES_JSON})
file(READ ${MW3_SOURCES_JSON} MW3_SOURCES_TEXT)
if(MW3_SOURCES_TEXT MATCHES "\\)mw3sources\"")
  message(FATAL_ERROR "sources.json holds the literal's end")
endif()
if(NOT MW3_SOURCES_TEXT MATCHES "\"keys\": \\[[ \t\r\n]*\"[0-9a-fA-F]")
  message(WARNING "assets/launcher/sources.json lists no signing key: this build takes no fetched catalog (the built-in one only)")
endif()
file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/sources_builtin.cpp
     CONTENT "// Written by CMake from assets/launcher/sources.json: do not edit.\nnamespace upgrades::update {\nextern const char kBuiltinSources[];\nconst char kBuiltinSources[] = R\"mw3sources(@MW3_SOURCES_TEXT@)mw3sources\";\n}  // namespace upgrades::update\n"
     @ONLY)
# The update check and the games catalog (src/upgrades/update).
add_library(mw3update STATIC
  src/upgrades/update/update.cpp
  src/upgrades/update/games.cpp
  src/upgrades/update/sources.cpp
  ${CMAKE_BINARY_DIR}/games_catalog.cpp
  ${CMAKE_BINARY_DIR}/sources_builtin.cpp)
target_include_directories(mw3update PUBLIC src)

# The setup kit's archive reader takes libarchive (with zlib and liblzma) when found: the release's are static, from
# the deps scripts (deps/archive); a development Mac may use Homebrew's (brew install libarchive).
find_path(LIBARCHIVE_INCLUDE archive.h
          HINTS "${CMAKE_SOURCE_DIR}/deps/archive/include" /opt/homebrew/opt/libarchive/include /usr/local/opt/libarchive/include)
find_library(LIBARCHIVE_LIB NAMES archive archive_static
             HINTS "${CMAKE_SOURCE_DIR}/deps/archive/lib" /opt/homebrew/opt/libarchive/lib /usr/local/opt/libarchive/lib)
if(LIBARCHIVE_INCLUDE AND LIBARCHIVE_LIB)
  message(STATUS "libarchive: ${LIBARCHIVE_LIB}")
else()
  set(LIBARCHIVE_INCLUDE "")
  set(LIBARCHIVE_LIB "")
  message(STATUS "libarchive not found: the kit reads folders, drives, .iso and .cue only")
endif()
# The shared setup kit (extern/re-setup-kit, a submodule; DESIGN.md there): the contract, the child runner, the
# pickers, the data folder, hashes and archives.
set(REKIT_LIBARCHIVE_INCLUDE "${LIBARCHIVE_INCLUDE}" CACHE PATH "" FORCE)
set(REKIT_LIBARCHIVE_LIB "${LIBARCHIVE_LIB}" CACHE FILEPATH "" FORCE)
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/extern/re-setup-kit/CMakeLists.txt")
  message(FATAL_ERROR "extern/re-setup-kit is empty: run git submodule update --init")
endif()
add_subdirectory(extern/re-setup-kit EXCLUDE_FROM_ALL)
if(WIN32 AND LIBARCHIVE_LIB MATCHES "\\.(a|lib)$")
  target_link_libraries(rekit PUBLIC bcrypt)  # a static libarchive's BCryptGenRandom
endif()

enable_testing()

find_package(SDL3 CONFIG REQUIRED)
# RmlUi (third_party/rmlui, VERSION.txt): the launcher's screens (docs/launcher.md, How it is drawn); its fonts by
# FreeType.
find_package(Freetype REQUIRED)
file(GLOB_RECURSE rmlui_sources CONFIGURE_DEPENDS third_party/rmlui/Source/Core/*.cpp)
add_library(mw3rmlui STATIC ${rmlui_sources})
target_include_directories(mw3rmlui PUBLIC third_party/rmlui/Include)
target_compile_definitions(mw3rmlui PUBLIC RMLUI_STATIC_LIB PRIVATE RMLUI_FONT_ENGINE_FREETYPE RMLUI_VERSION="6.3")
target_compile_options(mw3rmlui PRIVATE "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")
target_link_libraries(mw3rmlui PRIVATE Freetype::Freetype)

# The launcher's sources, in the program and in its tests.
set(LAUNCHER_SOURCES
  src/frontend/sdl/launcher.cpp
  src/frontend/sdl/launcher_ui.cpp
  src/frontend/sdl/launcher_own.cpp
  src/frontend/sdl/launcher_job.cpp
  src/frontend/sdl/launcher_folder.cpp
  src/frontend/sdl/sdl_update.cpp
  src/frontend/sdl/sdl_games.cpp
  src/frontend/sdl/sdl_catalog.cpp
  src/frontend/sdl/run_log.cpp
  src/frontend/sdl/crash_report.cpp
  src/frontend/sdl/log_start.cpp
  src/frontend/sdl/sdl_child.cpp
  src/frontend/sdl/sdl_focus.cpp
  src/frontend/sdl/text_fit.cpp
  src/frontend/sdl/vulkan_check.cpp)
add_library(mw3launcherlib STATIC ${LAUNCHER_SOURCES})
target_include_directories(mw3launcherlib PUBLIC src)
target_link_libraries(mw3launcherlib PUBLIC mw3rmlui mw3update rekit_sdl SDL3::SDL3)
if(APPLE)
  target_link_libraries(mw3launcherlib PUBLIC "-framework Foundation")  # sdl_games' Dock and Finder calls (objc)
endif()
# Linux: the lettering's font from the system's when assets/launcher has none (launcher.cpp's font_path), its Latin
# letters checked through SDL3_ttf. The Mac and Windows name their fonts' paths.
find_package(SDL3_ttf CONFIG)
if(UNIX AND NOT APPLE)
  target_sources(mw3launcherlib PRIVATE src/frontend/sdl/font_search.cpp)
  target_link_libraries(mw3launcherlib PUBLIC SDL3_ttf::SDL3_ttf)
endif()
target_compile_definitions(mw3launcherlib PRIVATE MW3_VERSION="${MW3_VERSION}")
if(NOT MW3_PACKAGE)
  target_compile_definitions(mw3launcherlib PRIVATE MW3_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
endif()

# The launcher (docs/launcher.md, The two programs): the main icon, its own program.
add_executable(mw3launcher src/frontend/sdl/launcher_main.cpp)
target_link_libraries(mw3launcher PRIVATE mw3launcherlib)
target_compile_definitions(mw3launcher PRIVATE MW3_VERSION="${MW3_VERSION}")
if(NOT MW3_PACKAGE)
  target_compile_definitions(mw3launcher PRIVATE MW3_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
endif()
if(WIN32)
  set(MW3_RC_NAME "MW3 Rebuild")
  set(MW3_RC_FILE "mw3launcher.exe")
  set(MW3_RC_ICON "ICON_MAIN ICON \"${CMAKE_SOURCE_DIR}/tools/windows/mw3.ico\"")
  configure_file(tools/windows/version.rc.in ${CMAKE_BINARY_DIR}/mw3launcher.rc)
  target_sources(mw3launcher PRIVATE ${CMAKE_BINARY_DIR}/mw3launcher.rc)
  target_sources(mw3launcher PRIVATE tools/windows/utf8.manifest)
  # No console, in every build: it is only ever a window (its log is mw3launcher.log in the write dir).
  if(MSVC)
    target_link_options(mw3launcher PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
  else()
    target_link_options(mw3launcher PRIVATE -mwindows)
  endif()
  add_custom_command(TARGET mw3launcher POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:mw3launcher> $<TARGET_FILE_DIR:mw3launcher>
                     COMMAND_EXPAND_LISTS)
endif()

# The launcher's tests: stand-ins for the games only, no game files (docs/launcher-split.md, The tests that start
# mw3 beside the launcher). SDL3_ttf in the tests everywhere: launcher_narrow_spaces_widen measures a font's glyphs,
# and the font search is tested where the launcher does not use it.
if(NOT SDL3_ttf_FOUND)
  message(FATAL_ERROR "SDL3_ttf not found: the launcher's tests (and the launcher on Linux) need it (brew install sdl3_ttf)")
endif()
add_executable(launchertests
  tests/main.cpp
  tests/test_update.cpp
  tests/test_log_start.cpp
  tests/launcher/test_launcher.cpp)
if(NOT (UNIX AND NOT APPLE))
  target_sources(launchertests PRIVATE src/frontend/sdl/font_search.cpp)
endif()
# The kit's test keys (tests/test_keys.h, Monocypher's signer) sign the fixtures; never a real key.
target_include_directories(launchertests PRIVATE tests extern/re-setup-kit/tests extern/re-setup-kit/third_party)
target_link_libraries(launchertests PRIVATE mw3launcherlib SDL3_ttf::SDL3_ttf)
target_compile_definitions(launchertests PRIVATE MW3_TEST_TMP_ROOT="${CMAKE_BINARY_DIR}"
                                                 MW3_LAUNCHER_BIN="$<TARGET_FILE:mw3launcher>"
                                                 MW3_ASSETS_FOR_TESTS="${CMAKE_SOURCE_DIR}/assets"
                                                 MW3_TEST_FIXTURES="${CMAKE_SOURCE_DIR}/tests/fixtures")
add_dependencies(launchertests mw3launcher)
add_test(NAME launchertests COMMAND launchertests)
