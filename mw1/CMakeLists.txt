# MW1: Critical Hit (VISION.md, The code base): the program `mw1` and its
# tests, on Pirate's Moon's game code the way mw3pm is. This is mw3-re's branch
# `mw1` (pm plus MW1), added by the root CMakeLists.txt (MW3_BUILD_MW1); mw1
# takes mw3pm's sources less its main, so a source added to mw3pm comes along
# without an edit here.

# The MechWarrior (1989) folder the development builds and the tests read: the repo's ignored game/
# (MW3_GAME_DIR and MW3PM_GAME_DIR are the root's).
set(MW1_DOS_GAME_DIR "${CMAKE_SOURCE_DIR}/game/mw1-dos/mwarrior/MECHWAR" CACHE PATH "The MechWarrior (1989) DOS folder the development builds read")
# The tests' exe checks and the narration need it: without MW_MAIN.EXE there (any case), say so now, not only as
# skipped tests at the end of a run (tests/mw1_install.h).
file(GLOB mw1_dos_files LIST_DIRECTORIES false "${MW1_DOS_GAME_DIR}/*")
set(mw1_dos_main FALSE)
foreach(f IN LISTS mw1_dos_files)
  get_filename_component(n "${f}" NAME)
  string(TOUPPER "${n}" n)
  if(n STREQUAL "MW_MAIN.EXE")
    set(mw1_dos_main TRUE)
  endif()
endforeach()
if(NOT mw1_dos_main)
  message(WARNING "No MW_MAIN.EXE in MW1_DOS_GAME_DIR (${MW1_DOS_GAME_DIR}): the tests that check MW1's exe will be "
                  "skipped, and a development mw1 has no narration, pitches or titles. Set -DMW1_DOS_GAME_DIR to the "
                  "MechWarrior (1989) folder.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(MW3RE_DIR "${CMAKE_SOURCE_DIR}")
# MSVC and clang-cl: C++ exceptions on, as the engine does (mw3-re's build.cmd replaces CMake's default /EHsc).
if(MSVC)
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/EHsc>")
endif()

# The worlds' scenery (data-src/art/scenery.csv, tools/mw1art.py's: MW1's planet number and our scenery type only)
# as a table for src/mw1_scenery.cpp, written at configure time and again when the table changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/data-src/art/scenery.csv)
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/data-src/art/scenery.csv mw1_scenery_rows)
set(mw1_scenery_inc "// Written by mw1/CMakeLists.txt from data-src/art/scenery.csv.\n")
foreach(row IN LISTS mw1_scenery_rows)
  if(row MATCHES "^([0-9]+),([a-z_]+)$")
    string(APPEND mw1_scenery_inc "{${CMAKE_MATCH_1}, \"${CMAKE_MATCH_2}\"},\n")
  endif()
endforeach()
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mw1gen-src/mw1_scenery.inc CONTENT "${mw1_scenery_inc}")

# What MW1 adds that a test can reach without SDL.
add_library(mw1core STATIC src/mw1_start.cpp src/mw1_campaign.cpp src/mw1_game.cpp src/mw1_save.cpp src/mw1_worlds.cpp src/mw1_arctic.cpp src/mw1_fade.cpp src/mw1_hub.cpp src/mw1_market.cpp src/mw1_travel.cpp src/mw1_pilots.cpp src/mw1_repairs.cpp src/mw1_missions.cpp src/mw1_images.cpp src/mw1_menu.cpp src/mw1_screen.cpp src/mw1_market_screen.cpp src/mw1_lab_pilot.cpp src/mw1_lab_fit.cpp src/mw1_mech_icons.cpp src/mw1_mech_renders.cpp src/mw1_travel_screen.cpp src/mw1_bar_screen.cpp src/mw1_office.cpp src/mw1_story.cpp src/mw1_ladder.cpp src/mw1_event_screen.cpp src/mw1_loading.cpp src/mw1_results.cpp src/mw1_travel_map.cpp src/mw1_map_image.cpp src/mw1_chrome.cpp src/mw1_scenery.cpp src/mw1_art.cpp src/mw1_stb_image.cpp)
target_include_directories(mw1core PUBLIC src ${MW3RE_DIR}/src PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/mw1gen-src third_party)
# stb_image (third_party/stb): as written, its warnings not ours.
set_source_files_properties(src/mw1_stb_image.cpp PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")
target_link_libraries(mw1core PUBLIC pmcore mw3core zipkitlib)

add_executable(mw1tests ${MW3RE_DIR}/tests/main.cpp tests/test_mw1_start.cpp tests/test_mw1_campaign.cpp tests/test_mw1_loop.cpp tests/test_mw1_save.cpp tests/test_mw1_worlds.cpp tests/test_mw1_market.cpp tests/test_mw1_travel.cpp tests/test_mw1_pilots.cpp tests/test_mw1_repairs.cpp tests/test_mw1_missions.cpp tests/test_mw1_menu.cpp tests/test_mw1_office.cpp tests/test_mw1_story.cpp tests/test_mw1_fade.cpp tests/test_mw1_travel_map.cpp tests/test_mw1_map_image.cpp tests/test_mw1_chrome.cpp tests/test_mw1_look.cpp tests/test_mw1_rooms.cpp tests/test_mw1_renders.cpp tests/test_mw1_art.cpp tests/test_mw1_results.cpp)
target_include_directories(mw1tests PRIVATE ${MW3RE_DIR}/tests)
target_link_libraries(mw1tests PRIVATE mw1core pmcore mw3core)
target_compile_definitions(mw1tests PRIVATE MW3_GAME_DIR="${MW3_GAME_DIR}" MW3PM_GAME_DIR="${MW3PM_GAME_DIR}" MW1_MW3_GAME_DIR="${MW3_GAME_DIR}"
                                            MW1_DOS_GAME_DIR="${MW1_DOS_GAME_DIR}"
                                            MW1_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
                                            MW3_TEST_TMP_ROOT="${CMAKE_CURRENT_BINARY_DIR}"
                                            MW3_SOURCE_DIR="${MW3RE_DIR}")
set_target_properties(mw1tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})  # beside mw3pmtests
enable_testing()
add_test(NAME mw1tests COMMAND mw1tests)

if(TARGET mw3pm)
  find_package(SDL3 CONFIG REQUIRED)  # its imported target is mw3-re's directory's alone
  # mw3pm's sources, its main left out, made absolute: they are mw3-re's paths.
  get_target_property(mw1_from_pm mw3pm SOURCES)
  set(mw1_sources)
  foreach(f IN LISTS mw1_from_pm)
    if(f MATCHES "mw3pm_main\\.cpp$|\\.rc$|\\.manifest$")
      continue()
    endif()
    if(NOT IS_ABSOLUTE "${f}")
      set(f "${MW3RE_DIR}/${f}")
    endif()
    list(APPEND mw1_sources "${f}")
  endforeach()
  add_executable(mw1 src/mw1_main.cpp ${mw1_sources})
  target_include_directories(mw1 PRIVATE ${MW3RE_DIR}/src)
  get_target_property(mw1_libs mw3pm LINK_LIBRARIES)
  target_link_libraries(mw1 PRIVATE mw1core ${mw1_libs})
  get_target_property(mw1_defs mw3pm COMPILE_DEFINITIONS)
  target_compile_definitions(mw1 PRIVATE ${mw1_defs} MW1=1)
  if(NOT MW3_PACKAGE)
    target_compile_definitions(mw1 PRIVATE MW1_PM_GAME_DIR="${MW3PM_GAME_DIR}" MW1_MW3_GAME_DIR="${MW3_GAME_DIR}"
                                           MW1_DOS_GAME_DIR="${MW1_DOS_GAME_DIR}")
    # The lab's text fit (FIXES.md item 17): every lab text inside its panel, its four pages and load box, both shapes.
    add_test(NAME mw1labfit COMMAND ${CMAKE_COMMAND} -DMW1_BIN=$<TARGET_FILE:mw1> -DDIR=${CMAKE_CURRENT_BINARY_DIR}/mw1labfit
                                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lab_fit.cmake)
    # MW1's own screens' text fit (FIXES.md items 3 and 5): every text inside the canvas, both shapes.
    add_test(NAME mw1screenfit COMMAND ${CMAKE_COMMAND} -DMW1_BIN=$<TARGET_FILE:mw1> -DDIR=${CMAKE_CURRENT_BINARY_DIR}/mw1screenfit
                                       -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/screen_fit.cmake)
    # The high-resolution 2D (FIXES.md item 12, research/hires-2d.md B3 and B4). Off, every frame byte-identical to
    # the recorded one (PM's options, MW1 with MW1_HIRES=0); on, MW1's 2D at 4K has detail inside the canvas's texels.
    add_executable(mw1hirescheck tools/mw1hirescheck.cpp)
    target_link_libraries(mw1hirescheck PRIVATE SDL3::SDL3)
    add_test(NAME mw1hiresoff COMMAND ${CMAKE_COMMAND} -DPM_BIN=$<TARGET_FILE:mw3pm> -DMW1_BIN=$<TARGET_FILE:mw1>
                                      -DDIR=${CMAKE_CURRENT_BINARY_DIR}/mw1hiresoff
                                      -DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/tests/hires_golden.txt
                                      -DPLATFORM=${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}
                                      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/hires_off.cmake)
    add_test(NAME mw1hiressmooth COMMAND ${CMAKE_COMMAND} -DMW1_BIN=$<TARGET_FILE:mw1> -DCHECK_BIN=$<TARGET_FILE:mw1hirescheck>
                                         -DDIR=${CMAKE_CURRENT_BINARY_DIR}/mw1hiressmooth
                                         -DEXPECT=smooth  # B1's fourth piece; MW1_HIRES=0 still blocky
                                         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/hires_smooth.cmake)
  endif()
  if(WIN32)
    # The version resource's numbers, MW1's own (the engine's are in its own scope): git's describe and commit count.
    set(MW3_VERSION dev)
    set(MW3_BUILD_NUMBER 0)
    find_package(Git QUIET)
    if(GIT_FOUND)
      execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                      OUTPUT_VARIABLE mw1_describe OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE mw1_git_r)
      if(mw1_git_r EQUAL 0 AND mw1_describe)
        set(MW3_VERSION ${mw1_describe})
      endif()
      execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                      OUTPUT_VARIABLE mw1_count OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE mw1_git_r)
      if(mw1_git_r EQUAL 0 AND mw1_count MATCHES "^[0-9]+$")
        math(EXPR MW3_BUILD_NUMBER "${mw1_count} % 65536")
      endif()
    endif()
    set(MW3_RC_NAME "MechWarrior 1: Critical Hit")
    set(MW3_RC_FILE "mw1.exe")
    set(MW3_RC_ICON "ICON_MAIN ICON \"${MW3RE_DIR}/tools/windows/mw3.ico\"")
    configure_file(${MW3RE_DIR}/tools/windows/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/mw1.rc)
    target_sources(mw1 PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/mw1.rc ${MW3RE_DIR}/tools/windows/utf8.manifest)
    if(MW3_PACKAGE)
      if(MSVC)
        target_link_options(mw1 PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
      else()
        target_link_options(mw1 PRIVATE -mwindows)
      endif()
    endif()
    if(NOT CMAKE_VERSION VERSION_LESS 3.21)
      add_custom_command(TARGET mw1 POST_BUILD
                         COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:mw1> $<TARGET_FILE_DIR:mw1>
                         COMMAND_EXPAND_LISTS)
    endif()
  endif()
  # Beside mw3pm and the launcher, whose MECHWARRIOR 1 entry starts mw1 from its own folder (docs/launcher.md,
  # MechWarrior 1).
  set_target_properties(mw1 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  # MW1's own data (the layouts, the fonts, data/), beside the program as mw1data/: mounted at the start
  # (mw1_main.cpp). Copied on every build (a layout changed alone relinks nothing).
  add_custom_target(mw1data ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data $<TARGET_FILE_DIR:mw1>/mw1data)
  add_dependencies(mw1 mw1data)
  # MW1's own images (src/mw1_images.h), packed from data-src/ at build time into PM's image format: mw1data/zbd/.
  # The travel map's backdrops are drawn from the worlds' data (src/mw1_map_image.h): mw1core.
  add_executable(mw1imgpack tools/mw1imgpack.cpp)
  target_link_libraries(mw1imgpack PRIVATE mw1core SDL3::SDL3)
  # Windows: the SDL3 DLL beside the tool, which runs during the build.
  if(WIN32 AND NOT CMAKE_VERSION VERSION_LESS 3.21)
    add_custom_command(TARGET mw1imgpack POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:mw1imgpack> $<TARGET_FILE_DIR:mw1imgpack>
                       COMMAND_EXPAND_LISTS)
  endif()
  set(mw1_image_zbd ${CMAKE_CURRENT_BINARY_DIR}/mw1gen/zbd/mw1image.zbd)
  set(mw1_art_zbd ${CMAKE_CURRENT_BINARY_DIR}/mw1gen/zbd/mw1art.zbd)
  # The screens' art (tools/mw1art.py's output, src/mw1_art.h; a picture added or removed is seen at the next
  # configure).
  file(GLOB mw1_art CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/data-src/art/*.png ${CMAKE_CURRENT_SOURCE_DIR}/data-src/art/*.jpg
       ${CMAKE_CURRENT_SOURCE_DIR}/data-src/art/art.csv)
  # The art's list, rewritten only when it changes (a picture added or removed; the glob above reconfigures): the pack
  # depends on it, so a removed picture repacks too (the remaining inputs alone are older than the pack).
  list(JOIN mw1_art "\n" mw1_art_lines)
  file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mw1gen-art-list.txt CONTENT "${mw1_art_lines}\n" @ONLY)
  add_custom_command(OUTPUT ${mw1_image_zbd} ${mw1_art_zbd}
                     COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/mw1gen/zbd
                     COMMAND mw1imgpack ${mw1_image_zbd} ${mw1_art_zbd} ${CMAKE_CURRENT_SOURCE_DIR}/data-src/intro.bmp ${CMAKE_CURRENT_SOURCE_DIR}/data-src/art
                     # Beside the program at once: a repack alone (the art or the chrome changed, the program not
                     # relinked) reached mw1data/ only at the program's next link before.
                     COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/mw1gen $<TARGET_FILE_DIR:mw1>/mw1data
                     DEPENDS mw1imgpack ${CMAKE_CURRENT_SOURCE_DIR}/data-src/intro.bmp ${CMAKE_CURRENT_SOURCE_DIR}/src/mw1_map_image.cpp
                             ${CMAKE_CURRENT_SOURCE_DIR}/src/mw1_map_image.h ${CMAKE_CURRENT_SOURCE_DIR}/src/mw1_travel_map.cpp
                             ${CMAKE_CURRENT_SOURCE_DIR}/src/mw1_chrome.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/mw1_chrome.h ${mw1_art}
                             ${CMAKE_CURRENT_SOURCE_DIR}/src/mw1_art.cpp ${CMAKE_CURRENT_BINARY_DIR}/mw1gen-art-list.txt
                     COMMENT "MW1's image archives")
  add_custom_target(mw1images DEPENDS ${mw1_image_zbd} ${mw1_art_zbd})
  add_dependencies(mw1 mw1images)
  add_custom_command(TARGET mw1 POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/mw1gen $<TARGET_FILE_DIR:mw1>/mw1data)
endif()
