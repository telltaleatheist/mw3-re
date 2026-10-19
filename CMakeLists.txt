cmake_minimum_required(VERSION 3.20)
project(mw3re LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The rebuilt game and engine. No platform or engine types in here: the front end
# (SDL3, with Godot as the fallback) talks to it through src/platform.
add_library(mw3core STATIC
  src/crt/msvc_math.cpp
  src/crt/msvc_rand.cpp
  src/crt/msvc_string.cpp
  src/gamez/zerror/zerr.cpp
  src/gamez/zvideo/zvid_ddd3d.cpp
  src/gamez/zvideo/zvid_format.cpp
  src/gamez/zutil/ztimer.cpp
  src/gamez/zutil/zutl_msg.cpp
  src/gamez/zutil/zutl_path.cpp
  src/gamez/zutil/zutl_zar.cpp
  src/gamez/zclass/camera.cpp
  src/gamez/zclass/class.cpp
  src/gamez/zclass/cls_mechlib.cpp
  src/gamez/zclass/cls_node.cpp
  src/gamez/zclass/cls_util.cpp
  src/gamez/zclass/cls_world.cpp
  src/gamez/zclass/cls_zbd.cpp
  src/gamez/zclass/display.cpp
  src/gamez/zclass/light.cpp
  src/gamez/zclass/list.cpp
  src/gamez/zclass/object3d.cpp
  src/gamez/zclass/sound.cpp
  src/gamez/zclass/window.cpp
  src/gamez/zeffect/zeff_anim_init.cpp
  src/gamez/zimage/zimg_library.cpp
  src/gamez/zinterp/zinterp.cpp
  src/gamez/zinterp/zinterp_parse.cpp
  src/gamez/zimage/zimg_texture.cpp
  src/gamez/zmath/zmth_matrix.cpp
  src/gamez/zmodel/gmod_bake.cpp
  src/gamez/zmodel/gmod_clip2d.cpp
  src/gamez/zmodel/gmod_clip3d.cpp
  src/gamez/zmodel/gmod_const.cpp
  src/gamez/zmodel/gmod_cull.cpp
  src/gamez/zmodel/gmod_draw.cpp
  src/gamez/zmodel/gmod_fogcalc.cpp
  src/gamez/zmodel/gmod_fog.cpp
  src/gamez/zmodel/gmod_init.cpp
  src/gamez/zmodel/gmod_light.cpp
  src/gamez/zmodel/gmod_matl.cpp
  src/gamez/zreader/zreader.cpp
  src/gamez/zsiscript/si_script.cpp
  src/gamez/zgame/zgameapp.cpp
  src/gamez/zsound/zsnd_cd.cpp
  src/gamez/zrender/zrndr_fog.cpp
  src/gamez/zrender/zrndr_span.cpp
  src/mech3/mech.cpp
  src/mech3/mech3_interp.cpp
  src/mech3/mech_config.cpp
  src/mech3/mech_damage.cpp
  src/mech3/mech_names.cpp
  src/mech3/mech_parts.cpp
  src/mech3/mech_reader.cpp
  src/mech3/mech_skin.cpp
  src/mech3/mission_world.cpp
  src/mech3/music.cpp
)
target_include_directories(mw3core PUBLIC src)
# The x87 never fuses a multiply and an add; clang does by default on arm64,
# which rounds differently from the original (CLAUDE.md, FPU precision).
if(NOT MSVC)
  target_compile_options(mw3core PRIVATE -ffp-contract=off)
endif()

# A C interface over a few ported functions, for the differential tests that run
# the original x86 code in an emulator and compare (tests/differential).
add_library(mw3probe SHARED
  tests/probe.cpp
  tests/probe_clip3d.cpp
  tests/probe_clip2d.cpp
  tests/probe_fogsort.cpp
  tests/probe_lighting.cpp
  tests/probe_drawtable.cpp
  tests/probe_matrix.cpp
  tests/probe_world.cpp
  tests/probe_modeldraw.cpp
  tests/probe_bake.cpp
  tests/probe_texload.cpp
  tests/probe_scenewalk.cpp
  tests/probe_damage.cpp
  tests/probe_interp.cpp
)
target_link_libraries(mw3probe PRIVATE mw3core)

enable_testing()
add_executable(mw3tests
  tests/main.cpp
  tests/test_mech.cpp
  tests/test_mech_reader.cpp
  tests/test_msvc_rand.cpp
  tests/test_ztimer.cpp
  tests/test_zgameapp.cpp
  tests/test_zsnd_cd.cpp
  tests/test_zrndr_span.cpp
  tests/test_zreader.cpp
)
target_link_libraries(mw3tests PRIVATE mw3core)
# Tests that read the player's install skip themselves when it is not there.
target_compile_definitions(mw3tests PRIVATE MW3_GAME_DIR="${CMAKE_SOURCE_DIR}/game/mw3-1.2"
                                          MW3_TEST_TMP="${CMAKE_BINARY_DIR}")
add_test(NAME mw3tests COMMAND mw3tests)

# Development tools on SDL3 (not part of the port): the world viewer.
find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
  # The SDL3 front end: platform implementations on SDL (render3d so far).
  add_library(mw3sdl STATIC src/frontend/sdl/sdl_render3d.cpp)
  target_link_libraries(mw3sdl PUBLIC mw3core SDL3::SDL3)
  add_executable(mw3view tools/viewer/viewer.cpp tools/viewer/scene.cpp tools/viewer/realframe.cpp)
  target_link_libraries(mw3view PRIVATE mw3core mw3sdl SDL3::SDL3)
  target_include_directories(mw3view PRIVATE tools/viewer)
  target_compile_definitions(mw3view PRIVATE MW3_GAME_DIR="${CMAKE_SOURCE_DIR}/game/mw3-1.2")
endif()
