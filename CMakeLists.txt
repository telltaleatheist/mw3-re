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
  src/gamez/zvideo/zvid_dd.cpp
  src/gamez/zvideo/zvid_ddd3d.cpp
  src/gamez/zvideo/zvid_format.cpp
  src/gamez/zutil/ztimer.cpp
  src/gamez/zinput/zin_init.cpp
  src/gamez/zinput/zin_kbd.cpp
  src/gamez/zinput/zin_mouse.cpp
  src/gamez/zinput/zin_cmdmap.cpp
  src/gamez/zinput/zin_joy.cpp
  src/gamez/znetwork/znet_object.cpp
  src/gamez/zutil/zutl_msg.cpp
  src/gamez/zutil/zutl_msgdll.cpp
  src/gamez/zutil/zutl_path.cpp
  src/gamez/zutil/zutl_reg.cpp
  src/platform/reg_file.cpp
  src/gamez/zutil/zutl_zar.cpp
  src/gamez/zclass/camera.cpp
  src/gamez/zclass/class.cpp
  src/gamez/zclass/cls_di.cpp
  src/gamez/zclass/cls_mechlib.cpp
  src/gamez/zclass/cls_node.cpp
  src/gamez/zclass/cls_util.cpp
  src/gamez/zclass/cls_world.cpp
  src/gamez/zclass/cls_zbd.cpp
  src/gamez/zclass/display.cpp
  src/gamez/zclass/light.cpp
  src/gamez/zclass/lod.cpp
  src/gamez/zclass/list.cpp
  src/gamez/zclass/seq.cpp
  src/gamez/zclass/object3d.cpp
  src/gamez/zclass/sound.cpp
  src/gamez/zclass/window.cpp
  src/gamez/zeffect/zeff_anim_init.cpp
  src/gamez/zentity/zent_manager.cpp
  src/gamez/zeffect/zeff_anim_puffer.cpp
  src/gamez/zeffect/zeff_anim_events.cpp
  src/gamez/zeffect/zeff_anim_run.cpp
  src/gamez/zeffect/zeff_anim_save.cpp
  src/gamez/zweapon/zwep_damage.cpp
  src/gamez/zimage/zimg_library.cpp
  src/gamez/zinterp/zinterp.cpp
  src/gamez/zinterp/zinterp_parse.cpp
  src/gamez/zimage/zimg_texture.cpp
  src/gamez/zmath/zmth_matrix.cpp
  src/gamez/zmath/zmth_vector.cpp
  src/gamez/zmodel/gmod_bake.cpp
  src/gamez/zmodel/gmod_clip2d.cpp
  src/gamez/zmodel/gmod_clip3d.cpp
  src/gamez/zmodel/gmod_const.cpp
  src/gamez/zmodel/gmod_cull.cpp
  src/gamez/zmodel/gmod_draw.cpp
  src/gamez/zmodel/gmod_fogcalc.cpp
  src/gamez/zmodel/gmod_fog.cpp
  src/gamez/zmodel/gmod_hit.cpp
  src/gamez/zmodel/gmod_init.cpp
  src/gamez/zmodel/gmod_light.cpp
  src/gamez/zmodel/gmod_matl.cpp
  src/gamez/zmodel/gmod_puff.cpp
  src/gamez/zreader/zreader.cpp
  src/gamez/zsiscript/si_script.cpp
  src/gamez/zgame/zgameapp.cpp
  src/gamez/zsound/zsnd_cd.cpp
  src/gamez/zrender/zrndr_draw.cpp
  src/gamez/zsound/zsnd_3d.cpp
  src/gamez/zsound/zsnd_create.cpp
  src/gamez/zsound/zsnd_grp.cpp
  src/gamez/zsound/zsnd_init.cpp
  src/gamez/zsound/zsnd_parm.cpp
  src/gamez/zsound/zsnd_play.cpp
  src/gamez/zweapon/zweapon.cpp
  src/gamez/zweapon/zwep_shot.cpp
  src/gamez/zweapon/zwep_beam.cpp
  src/gamez/zweapon/zwep_init.cpp
  src/gamez/zrender/zrndr_fog.cpp
  src/gamez/zrender/zrndr_span.cpp
  src/gamez/zrender/zrndr_tint.cpp
  src/gamez/zrender/zrndr_blit.cpp
  src/gamez/ztwod/twod_control.cpp
  src/gamez/ztwod/twod_dialog.cpp
  src/gamez/ztwod/twod_editbar.cpp
  src/gamez/ztwod/twod_shapes.cpp
  src/gamez/ztwod/twod_host.cpp
  src/gamez/ztwod/twod_list.cpp
  src/gamez/ztwod/twod_msgbox.cpp
  src/gamez/ztwod/twod_object.cpp
  src/gamez/ztwod/twod_textbox.cpp
  src/gamez/ztwod/twod_wintext.cpp
  src/mech3/mech.cpp
  src/mech3/mech3_interp.cpp
  src/mech3/controls_cfg.cpp
  src/mech3/mech_config.cpp
  src/mech3/mech_control.cpp
  src/mech3/mech_damage.cpp
  src/mech3/mech_data.cpp
  src/mech3/mech_lib.cpp
  src/mech3/mech_names.cpp
  src/mech3/mech_parts.cpp
  src/mech3/mech_part_update.cpp
  src/mech3/mech_pose.cpp
  src/mech3/mech_move.cpp
  src/mech3/mech_skis.cpp
  src/mech3/mech_build.cpp
  src/mech3/mech_part_build.cpp
  src/mech3/mech_update.cpp
  src/mech3/mech_collide.cpp
  src/mech3/mech_place.cpp
  src/mech3/mech_command.cpp
  src/mech3/veh_basic.cpp
  src/mech3/mech_anim.cpp
  src/mech3/mech_reader.cpp
  src/mech3/mech_skin.cpp
  src/mech3/mission.cpp
  src/mech3/mission_launch.cpp
  src/mech3/player.cpp
  src/mech3/player_view.cpp
  src/mech3/hud.cpp
  src/mech3/mech_hud.cpp
  src/mech3/app_hud.cpp
  src/mech3/pilot.cpp
  src/mech3/pilot_games.cpp
  src/mech3/pilot_menu.cpp
  src/mech3/mission_world.cpp
  src/mech3/options.cpp
  src/mech3/weapon.cpp
  src/mech3/zai_params.cpp
  src/mech3/zai_target.cpp
  src/mech3/sensors.cpp
  src/mech3/zai_valve.cpp
  src/mech3/zai_brain.cpp
  src/mech3/zai_steps.cpp
  src/mech3/zai_path.cpp
  src/mech3/zainet.cpp
  src/mech3/zainetedit.cpp
  src/mech3/zainet_region.cpp
  src/mech3/zainet_node.cpp
  src/mech3/zrdrainet.cpp
  src/mech3/zrdraiv.cpp
  src/mech3/zrdrvalve.cpp
  src/mech3/app.cpp
  src/mech3/app_hooks.cpp
  src/mech3/app_states.cpp
  src/mech3/app_engine.cpp
  src/mech3/app_mission.cpp
  src/mech3/app_view.cpp
  src/mech3/app_mech.cpp
  src/mech3/app_weapons.cpp
  src/mech3/app_sensors.cpp
  src/mech3/app_menus.cpp
  src/mech3/mainmenu.cpp
  src/mech3/menu_widgets.cpp
  src/mech3/menu_states.cpp
  src/mech3/credits.cpp
  src/mech3/options_menu.cpp
  src/mech3/controls_menu.cpp
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
  tests/probe_app.cpp
  tests/probe_options.cpp
  tests/probe_states.cpp
  tests/probe_shadows.cpp
  tests/probe_shadow_image.cpp
  tests/probe_puffers.cpp
  tests/probe_move.cpp
  tests/probe_skis.cpp
  tests/probe_jets.cpp
  tests/probe_actions.cpp
  tests/probe_sound.cpp
  tests/probe_weapons.cpp
  tests/probe_weapons_run.cpp
  tests/probe_weapons_hit.cpp
  tests/probe_weapons_fly.cpp
  tests/probe_twod.cpp
  tests/probe_mainmenu.cpp
  tests/probe_menus.cpp
  tests/probe_ctlscreen.cpp
  tests/probe_loadsave.cpp
  tests/probe_pilot.cpp
  tests/probe_pilotscreen.cpp
  tests/probe_update.cpp
  tests/probe_di.cpp
  tests/probe_place.cpp
  tests/probe_shotq.cpp
  tests/probe_command.cpp
  tests/probe_collide.cpp
  tests/probe_anim_run.cpp
  tests/probe_action_lists.cpp
  tests/probe_mechanim.cpp
  tests/probe_pose.cpp
  tests/probe_parts.cpp
  tests/probe_controls.cpp
  tests/probe_ctlcfg.cpp
  tests/probe_ai.cpp
  tests/probe_ai_net.cpp
  tests/probe_zinput.cpp
  tests/probe_settings.cpp
  tests/probe_mission.cpp
  tests/probe_player.cpp
  tests/probe_mech_build.cpp
  tests/probe_ai_brain.cpp
  tests/probe_ai_valve.cpp
  tests/probe_ai_target.cpp
  tests/probe_sensors.cpp
  tests/probe_fade.cpp
  tests/probe_frame.cpp
  tests/probe_view.cpp
  tests/probe_hud.cpp
  tests/probe_mech_data.cpp
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
  tests/test_app.cpp
  tests/test_mainmenu.cpp
  tests/test_menus.cpp
  tests/test_zsnd_cd.cpp
  tests/test_zrndr_span.cpp
  tests/test_fixes.cpp
  tests/test_zreader.cpp
  tests/test_controls_cfg.cpp
  tests/test_pilot_names.cpp
  tests/test_program.cpp
)
target_link_libraries(mw3tests PRIVATE mw3core)
# Tests that read the player's install skip themselves when it is not there.
target_compile_definitions(mw3tests PRIVATE MW3_GAME_DIR="${CMAKE_SOURCE_DIR}/game/mw3-1.2"
                                          MW3_TEST_TMP="${CMAKE_BINARY_DIR}")
add_test(NAME mw3tests COMMAND mw3tests)

# The SDL3 front end and the programs on it.
find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
  # Platform implementations on SDL (docs/program.md): window and message
  # pump, input, render3d, sound, music; the debug world loader.
  add_library(mw3sdl STATIC
    src/frontend/debug_world.cpp
    src/frontend/sdl/sdl_input.cpp
    src/frontend/sdl/sdl_music.cpp
    src/frontend/sdl/sdl_platform.cpp
    src/frontend/sdl/sdl_render3d.cpp
    src/frontend/sdl/sdl_sound.cpp
  )
  target_link_libraries(mw3sdl PUBLIC mw3core SDL3::SDL3)
  # Music files other than .wav decode with SDL3_sound (brew install sdl3_sound).
  find_package(SDL3_sound CONFIG QUIET)
  if(SDL3_sound_FOUND)
    target_link_libraries(mw3sdl PUBLIC SDL3_sound::SDL3_sound)
    target_compile_definitions(mw3sdl PRIVATE MW3_HAVE_SDL_SOUND=1)
  else()
    message(STATUS "SDL3_sound not found: music plays .wav tracks only")
  endif()
  # The game, and its front end's tests: the 2D text needs SDL3_ttf.
  find_package(SDL3_ttf CONFIG QUIET)
  if(SDL3_ttf_FOUND)
    add_library(mw3text STATIC src/frontend/sdl/sdl_twod_host.cpp)
    target_link_libraries(mw3text PUBLIC mw3sdl SDL3_ttf::SDL3_ttf)
    add_executable(mw3
      src/frontend/sdl/mw3_main.cpp
      src/frontend/sdl/debug_states.cpp
    )
    target_link_libraries(mw3 PRIVATE mw3text mw3sdl mw3core SDL3::SDL3)
    target_compile_definitions(mw3 PRIVATE MW3_GAME_DIR="${CMAKE_SOURCE_DIR}/game/mw3-1.2")
    add_executable(mw3sdltests
      tests/main.cpp
      tests/test_sdl_frontend.cpp
    )
    target_link_libraries(mw3sdltests PRIVATE mw3text mw3sdl mw3core SDL3::SDL3)
    target_compile_definitions(mw3sdltests PRIVATE MW3_GAME_DIR="${CMAKE_SOURCE_DIR}/game/mw3-1.2"
                                                 MW3_TEST_TMP="${CMAKE_BINARY_DIR}")
    add_test(NAME mw3sdltests COMMAND mw3sdltests)
  else()
    message(STATUS "SDL3_ttf not found: mw3 is not built (brew install sdl3_ttf)")
  endif()
  # Development tools (not part of the port): the world viewer.
  add_executable(mw3view tools/viewer/viewer.cpp tools/viewer/scene.cpp)
  target_link_libraries(mw3view PRIVATE mw3core mw3sdl SDL3::SDL3)
  target_include_directories(mw3view PRIVATE tools/viewer)
  target_compile_definitions(mw3view PRIVATE MW3_GAME_DIR="${CMAKE_SOURCE_DIR}/game/mw3-1.2")
  # Plays one game sound through zSound and SDL audio (docs/sound.md).
  add_executable(mw3soundcheck tools/soundcheck/soundcheck.cpp)
  target_link_libraries(mw3soundcheck PRIVATE mw3core mw3sdl SDL3::SDL3)
  target_compile_definitions(mw3soundcheck PRIVATE MW3_GAME_DIR="${CMAKE_SOURCE_DIR}/game/mw3-1.2")
endif()
