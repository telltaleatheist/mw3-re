#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "gamez/zgame/zgameapp.h"

namespace zcls { struct Node; }
namespace zimg { struct Texture; }

// What the game's app and states call in code not ported yet (docs/game-loop.md).
// One method per original function, its address in the key; by default each
// reports to unported() and does nothing. Ported functions default to the port.
//
// Objects of modules not ported are opaque handles. Their fields are read and
// written through peek/poke with the original's addresses and offsets, and their
// virtual methods through vcall(object, vtable slot), so the code stays the
// original's and a module, once ported, replaces the generic access.
namespace mech3 {

using zgame::Arg;
using zgame::Handle;

class MenuScreen;

class Engine : public zgame::AppHooks {
 public:
  // Memory of modules not ported yet, by the original's address.
  virtual uint32_t peek32(uint32_t addr) { return unported("peek32", {addr}); }
  virtual uint8_t peek8(uint32_t addr) { return static_cast<uint8_t>(unported("peek8", {addr})); }
  virtual void poke32(uint32_t addr, uint32_t v) { unported("poke32", {addr, v}); }
  virtual void poke8(uint32_t addr, uint8_t v) { unported("poke8", {addr, v}); }
  uint32_t field32(Handle obj, uint32_t off) { return peek32(obj + off); }
  uint8_t field8(Handle obj, uint32_t off) { return peek8(obj + off); }
  void set_field32(Handle obj, uint32_t off, uint32_t v) { poke32(obj + off, v); }
  void set_field8(Handle obj, uint32_t off, uint8_t v) { poke8(obj + off, v); }
  // A virtual call on an object not ported: "vt+<slot>" (object, arguments...).
  virtual uint32_t vcall(Handle obj, uint32_t slot, std::initializer_list<Arg> args);

  // The globals these files read.
  Handle pilot() { return peek32(0x60861c); }          // the current pilot ([0x0060861c])
  void set_pilot(Handle p) { poke32(0x60861c, p); }
  Handle player_view() { return peek32(0x5f4c8c); }    // the player's view ([0x005f4c8c])
  Handle network_game() { return peek32(0x608620); }  // [0x00608620]
  void set_network_game(Handle g) { poke32(0x608620, g); }
  int32_t video_mode() { return static_cast<int32_t>(peek32(0x72f348)); }  // 0 software, 1 Direct3D
  int32_t fatal_error() { return static_cast<int32_t>(peek32(0x608d5c)); }
  virtual const char* pilot_name();                    // [0x0060861c]+0x18, null without a pilot
  virtual float frame_dt();                            // [0x0072c52c], ported: ztimer
  // The options ([0x005f4d10], [0x005f4d2c], [0x005f4d30]), ported.
  virtual int32_t in_game_vmode();
  virtual float mouse_accel_x();
  virtual float mouse_accel_y();

  // The options' functions (mech3/options), ported.
  virtual int32_t hw_card_flag();                      // 0x0040d4d0
  virtual int32_t hw_card_dev();                       // 0x0040cc10
  virtual void select_hw(int32_t hw);                  // 0x0040d4c0
  virtual void set_hw_card_dev(int32_t dev);           // 0x0040cbf0
  virtual int32_t vmode();                             // 0x0040d9d0
  virtual void set_vmode(int32_t mode);                // 0x0040d810
  virtual int32_t gfx_flags();                         // 0x0040d480
  virtual int32_t pixel_double();                      // 0x0040d4b0
  virtual Handle render_rect();                        // 0x0040d6a0 (1)
  virtual Handle display_rect();                       // 0x0040d750 (2)
  virtual Handle window_rect();                        // 0x0040d7b0 (3)
  virtual Handle display_depth();                      // 0x0040d790
  virtual Handle vstride();                            // 0x0040d7a0
  virtual int32_t window_h();                          // 0x0040d7c0
  virtual void set_use_joystick(int32_t on);           // 0x0040d070
  virtual void set_fast_menu(int32_t on);              // 0x0040ce90
  virtual int32_t fast_menu();                         // 0x0040cec0
  virtual int32_t display_width();                     // 0x0040d760
  virtual int32_t display_height();                    // 0x0040d770
  virtual void options_apply_all();                    // 0x0040cb30
  // The game options screen's (docs/menus.md), ported.
  virtual int32_t game_intensity();                    // 0x0040cdf0
  virtual void set_game_intensity(int32_t v);          // 0x0040cc20
  virtual int32_t camera_mode();                       // 0x0040ce70
  virtual void set_camera_mode(int32_t v);             // 0x0040ce10
  virtual int32_t invincible();                        // 0x0040cf20
  virtual void set_invincible(int32_t v);              // 0x0040ced0
  virtual int32_t infinite_ammo();                     // 0x0040cf80
  virtual void set_infinite_ammo(int32_t v);           // 0x0040cf30
  virtual int32_t heat();                              // 0x0040cfc0
  virtual void set_heat(int32_t v);                    // 0x0040cf90
  virtual int32_t torso_auto_return();                 // 0x0040d010
  virtual void set_torso_auto_return(int32_t v);       // 0x0040cfd0
  virtual int32_t throttle_const();                    // 0x0040d060
  virtual void set_throttle_const(int32_t v);          // 0x0040d020
  virtual int32_t mw2_jump_jets();                     // 0x0040d1e0
  virtual void set_mw2_jump_jets(int32_t v);           // 0x0040d1c0
  virtual void set_mouse_accel_y(float v);             // *[0x005f4d30] = v (no setter)
  virtual void settings_write();                       // 0x00597a60, ported: zreg::write_all
  // The graphics and audio screen's, ported.
  virtual float cd_volume();                           // 0x0040dae0
  virtual void set_cd_volume(float v);                 // 0x0040dab0
  virtual float sound_volume();                        // 0x0040daa0
  virtual void set_sound_volume(float v);              // 0x0040da80
  virtual int32_t texture_memory();                    // 0x0040d410
  virtual void set_texture_memory(int32_t v);          // 0x0040d3f0
  virtual int32_t sound_lod();                         // 0x0040d3e0
  virtual void set_sound_lod(int32_t v);               // 0x0040d3d0
  virtual int32_t object_lod();                        // 0x0040d3b0
  virtual void set_object_lod(int32_t v);              // 0x0040d310
  virtual int32_t effects_level();                     // 0x0040d2f0
  virtual void set_effects_level(int32_t v);           // 0x0040d2a0
  virtual int32_t shadow_parts();                      // 0x0040d220
  virtual void set_shadow_parts(int32_t v);            // 0x0040d1f0
  virtual int32_t shadow();                            // 0x0040d280
  virtual void set_shadow(int32_t v);                  // 0x0040d240
  virtual void set_gfx_flags(int32_t v);               // 0x0040d430
  virtual void set_in_game_vmode(int32_t v);           // *[0x005f4d10] = v (no setter)
  // 0x004ab860 (ECX device, EDX the lowest mode out): the screen modes a
  // device can run, the highest by its video memory. Ported, on zVideo's
  // memory counts below.
  virtual void video_mode_range(int32_t device, int32_t* lo, int32_t* hi);
  virtual int32_t video_memory_total() { return static_cast<int32_t>(unported("0x00579a10", {})); }
  virtual int32_t video_memory_used() { return static_cast<int32_t>(unported("0x00579a40", {})); }
  virtual uint32_t video_has_cap(int32_t cap) { return unported("0x00579b70", {cap}); }
  virtual uint32_t video_device_count() { return unported("0x00579aa0", {}); }
  virtual const char* video_device_name(uint32_t i) {  // 0x00579a70
    unported("0x00579a70", {i});
    return "";
  }
  // zModel's and zRender's GfxFlags: the settings entry `name` if the store
  // has it, else a copy of `flags` (0x00526750, 0x00557ae0).
  virtual void model_use_gfx_flags(const char* name, int32_t flags) { unported("0x00526750", {name, flags}); }
  virtual void render_use_gfx_flags(const char* name, int32_t flags) { unported("0x00557ae0", {name, flags}); }
  virtual void pop_state(uint32_t arg);                // 0x005905a0, ported: zGameApp::Pop
  // Replace(state(4), 0) (0x00482970, 0x00590250): the Exit state, so the game
  // restarts with a new video device. Ported.
  virtual void replace_with_exit();
  virtual void effects_set_level(int32_t v) { unported("0x00508fb0", {v}); }
  virtual int32_t options_load(const char* name) {     // 0x0040c420, detail.zrd + registry
    return static_cast<int32_t>(unported("0x0040c420", {name}));
  }

  // Windows, through the front end (FIX (A)).
  virtual uint32_t lobby_connection() { return unported("0x005899b0", {}); }  // DirectPlay lobby
  virtual uint32_t activate_running_copy() { return unported("0x00480340", {}); }  // 0: one runs
  virtual void close_running_copy() { unported("0x004803a0", {}); }
  virtual uint32_t winsock_check(int32_t* major, int32_t* minor) {
    return unported("0x00575e30", {*major, *minor});
  }
  // 0x00575fd0: message `id` of Mech3Msg.dll formatted with one string argument.
  virtual const char* message_format(int32_t id, const char* arg);
  // sprintf, for the "%d.%d" Winsock version.
  virtual void print_version(char* out, int size, int32_t major, int32_t minor);
  virtual uint32_t lobby_connect() { return unported("0x00589a50", {0x5c3f28}); }
  virtual uint32_t register_window_class() { return unported("AfxRegisterClass", {}); }
  virtual void unregister_window_class() { unported("UnregisterClassA", {"Mech3Class"}); }
  virtual void update_window(uint32_t hwnd) { unported("UpdateWindow", {hwnd}); }
  virtual void message_beep(uint32_t type) { unported("MessageBeep", {type}); }
  virtual uint32_t message_box_text(uint32_t hwnd, const char* text, const char* caption, uint32_t type) {
    return unported("MessageBoxA", {hwnd, text, caption, type});
  }
  virtual uint32_t focus_window() { return unported("GetFocus", {}); }
  virtual void exit_process(uint32_t code);            // ExitProcess
  virtual void lock_system_keys(uint32_t on) {         // SystemParametersInfoA(0x61, on, ..)
    unported("SystemParametersInfoA", {0x61, on});
  }
  virtual void sleep(uint32_t ms) { unported("Sleep", {ms}); }
  virtual void cursor_pos(int32_t* x, int32_t* y);     // GetCursorPos
  virtual uint32_t rand();                             // MSVCRT rand, ported: crt
  virtual void remove_debug_menu(uint32_t hwnd) { unported("SetMenu", {hwnd, 0}); }
  // The debug menu (MENU resource 0x93, gametypes.md) on the window: SetMenu
  // with the loaded menu, whose handle the MFC wrapper leaves 0 here.
  virtual void attach_debug_menu(uint32_t hwnd) { unported("SetMenu", {hwnd, 0}); }
  virtual const char* command_line();                  // GetCommandLineA
  virtual void print(const char* format, const char* arg);  // printf
  virtual void flush_stdout() { unported("fflush", {}); }
  virtual void run_dir() { unported("0x004828d0", {}); }    // RunDir from the registry
  virtual uint32_t message_dll_load(const char* name) { return unported("0x00575f30", {name}); }
  virtual uint32_t directx_version() { return unported("0x0057a3d0", {}); }
  virtual uint32_t install_options(uint32_t mask) {    // 0x00597d80, InstallOptions & mask
    return unported("0x00597d80", {mask});
  }

  // The app's own objects.
  virtual Handle script_new() { return unported("0x00410700", {}); }  // new(0xc0) "interp.zbd"
  virtual void script_delete(Handle s) { unported("0x0047faa0", {s}); }
  virtual void free_block(Handle p) { unported("delete", {p}); }  // operator delete
  virtual Handle frame_new() { return unported("0x00590920", {"mech3"}); }  // new(0xd0) frame
  virtual Handle frame_create(Handle frame) { return unported("CWnd::CreateEx", {frame}); }
  virtual uint32_t frame_hwnd(Handle frame) { return field32(frame, 0x20); }
  virtual void frame_set_title(Handle frame, const char* title) {
    unported("CWnd::SetWindowTextA", {frame, title});
  }
  virtual void error_log_open(uint32_t hwnd, uint32_t a, const char* name) {
    unported("0x0050da50", {hwnd, a, name});
  }
  virtual const char* message(int32_t id);             // 0x00576040, Mech3Msg.dll's text
  virtual const char* version_string();                // 0x004af280
  virtual const char* version_number();                // 0x004af290
  // 0x004ac150 (cdecl): a message box from the message table; the fourth
  // argument (a format argument) is pushed only by some callers.
  virtual uint32_t message_box(int32_t a, int32_t caption, int32_t text, int32_t arg = 0) {
    (void)arg;
    return unported("0x004ac150", {a, caption, text});
  }
  virtual int32_t pick_video_device(int32_t a, int32_t b) {  // 0x004eaab0
    return static_cast<int32_t>(unported("0x004eaab0", {a, b}));
  }

  // Engine modules at start and stop.
  virtual void settings_init(const char* company, const char* product, const char* version) {
    unported("0x00598210", {company, product, version});
  }
  virtual void input_init(int32_t commands, const char* map) { unported("0x0051c630", {commands, map}); }
  virtual void list_pool_init(int32_t links) { unported("0x00555cd0", {links}); }
  virtual void reader_init(const char* dir) { unported("0x00556170", {dir}); }
  virtual void reader_open_archive(const char* path, int32_t current) {
    unported("0x00557320", {path, current});
  }
  virtual void entity_init() { unported("0x005a8b30", {}); }
  virtual void anim_enable(int32_t on) { unported("0x005090d0", {on}); }
  virtual void effects_set_a(int32_t v) { unported("0x0050d1e0", {v}); }
  virtual void effects_set_b(int32_t v) { unported("0x0050d1f0", {v}); }
  virtual void weapons_set_mode(int32_t v) { unported("0x00583740", {v}); }
  virtual void decals_set_mode(int32_t v) { unported("0x00501f90", {v}); }
  virtual void classes_set_mode(int32_t v) { unported("0x004f8f60", {v}); }
  virtual void geometry_init(const char* dir) { unported("0x00516f50", {dir}); }
  virtual int32_t effects_init() { return static_cast<int32_t>(unported("0x0050d270", {})); }
  virtual int32_t classes_init() { return static_cast<int32_t>(unported("0x004f8f20", {})); }
  virtual int32_t models_init() { return static_cast<int32_t>(unported("0x00526510", {})); }
  virtual int32_t render_init() { return static_cast<int32_t>(unported("0x005578f0", {})); }
  virtual int32_t sound_init(uint32_t window) { return static_cast<int32_t>(unported("0x005700f0", {window})); }
  virtual int32_t util_init() { return static_cast<int32_t>(unported("0x00576c50", {})); }
  virtual int32_t weapons_init() { return static_cast<int32_t>(unported("0x00583750", {})); }
  virtual void textures_set_mode(int32_t v) { unported("0x00518790", {v}); }
  virtual void sound_set_mode(int32_t v) { unported("0x0056f4b0", {v}); }
  virtual void models_load(const char* flags_name, int32_t flags) { unported("0x00526750", {flags_name, flags}); }
  virtual void render_load(const char* flags_name, int32_t flags) { unported("0x00557ae0", {flags_name, flags}); }
  virtual void light_init();                           // 0x00542bb0, ported: zmod::shadows_init
  virtual void fonts_init(const char* name, int32_t a) { unported("0x00516e90", {name, a}); }
  virtual void sound_load(uint32_t window, const char* name, uint32_t callback, uint32_t a) {
    unported("0x005705e0", {window, name, callback, a});
  }
  virtual void input_start(uint32_t window, uint32_t cmd_show) { unported("0x0051b0a0", {window, cmd_show}); }
  virtual uint32_t joystick_present() { return unported("0x0051dc10", {}); }
  virtual uint32_t force_feedback_present() { return unported("0x0051ddb0", {}); }
  virtual Handle network_game_new() { return unported("0x00433090", {}); }  // new(0x34)
  virtual void network_game_delete(Handle g) { unported("0x004333f0", {g}); }
  virtual void network_game_start(Handle g, int32_t a) { unported("0x004338a0", {g, a}); }
  virtual void mechs_init(Handle display, Handle window) { unported("0x0040f410", {display, window}); }
  virtual void maps_init(const char* name) { unported("0x004583b0", {name}); }
  virtual void clock_init() { unported("0x005759a0", {}); }
  virtual void cd_init(int32_t a) { unported("0x00575d00", {a}); }
  virtual void video_select_device(int32_t dev) { unported("0x00579ab0", {dev}); }
  virtual int32_t video_open(uint32_t window, int32_t hw, int32_t restore, int32_t vmode) {
    return static_cast<int32_t>(unported("0x00579c60", {window, hw, restore, vmode}));
  }
  virtual int32_t span_init(int32_t height) { return static_cast<int32_t>(unported("0x00564370", {height})); }
  virtual uint32_t video_card_fn_390(int32_t dev) { return unported("[0x0072f390]", {dev}); }
  virtual uint32_t video_card_memory(int32_t dev, uint32_t* total) {
    const uint32_t r = unported("[0x0072f38c]", {dev});
    *total = unported("@video_total", {});
    return r;
  }
  virtual void pilots_delete() { unported("0x004785f0", {}); }
  virtual void mechs_shutdown() { unported("0x00414000", {}); }
  virtual void missions_shutdown() { unported("0x004844b0", {}); }
  virtual void mech_data_shutdown() { unported("0x004584e0", {}); }
  virtual void world_shutdown() { unported("0x0040f7d0", {}); }
  virtual void span_shutdown() { unported("0x005646d0", {}); }
  virtual void network_shutdown() { unported("0x00586c20", {}); }
  virtual void decals_shutdown() { unported("0x00502020", {}); }
  virtual void geometry_shutdown() { unported("0x00516f00", {}); }
  virtual void weapons_shutdown() { unported("0x00583870", {}); }
  virtual void effects_shutdown() { unported("0x0050d2b0", {}); }
  virtual void classes_shutdown() { unported("0x004f8f40", {}); }
  virtual void models_shutdown() { unported("0x00526780", {}); }
  virtual void sound_shutdown_groups(uint32_t callback) { unported("0x005715d0", {callback}); }
  virtual void reader_text_shutdown() { unported("0x00557b10", {}); }
  virtual void sound_shutdown() { unported("0x00570510", {}); }
  virtual void archives_shutdown() { unported("0x00576c60", {}); }
  virtual void input_devices_shutdown() { unported("0x0051b170", {}); }
  virtual void reader_shutdown() { unported("0x00556210", {}); }
  virtual void video_shutdown() { unported("0x00579ef0", {}); }
  virtual void ai_targets_shutdown() { unported("0x004d72a0", {}); }
  virtual void list_pool_free() { unported("0x00555d90", {}); }
  virtual void entity_shutdown() { unported("0x005a8bb0", {}); }
  // The app's destructor (0x0047fab0).
  virtual void message_dll_free() { unported("0x00575f70", {}); }
  virtual void input_maps_free() { unported("0x0051d0c0", {}); }
  virtual void options_save();                         // 0x0040cb20, ported
  virtual void settings_free();                        // 0x00598270, ported
  virtual void input_set_mode(uint32_t mode) { unported("0x0051bae0", {mode}); }

  // zVideo's buffers and frame (zvid_buff.c and neighbours).
  virtual Handle video_back_buffer() { return unported("0x00576db0", {}); }
  virtual Handle video_front_buffer() { return unported("0x00576c70", {}); }
  virtual Handle video_screen_rect() { return unported("0x00579880", {}); }
  virtual Handle video_target() { return unported("0x00576d80", {}); }
  virtual Handle video_pitch() { return unported("0x00576dc0", {}); }
  virtual Handle video_buffer_da0() { return unported("0x00576da0", {}); }
  virtual Handle video_buffer_d90() { return unported("0x00576d90", {}); }
  virtual Handle video_buffer_cd0() { return unported("0x00576cd0", {}); }
  virtual Handle video_buffer_ca0() { return unported("0x00576ca0", {}); }
  virtual void video_clear(Handle rect) { unported("0x00576dd0", {rect}); }
  virtual void video_clear_a(Handle rect) { unported("0x00576ce0", {rect}); }
  virtual void video_clear_b(Handle rect) { unported("0x00576cf0", {rect}); }
  virtual void video_begin_3d() { unported("0x00576d00", {}); }
  virtual void video_end_3d() { unported("0x00576d60", {}); }
  virtual void video_lock() { unported("0x00576de0", {}); }
  virtual void video_unlock() { unported("0x00576e70", {}); }
  virtual void video_flip(int32_t a, int32_t b, int32_t c, int32_t d) { unported("0x00576ea0", {a, b, c, d}); }
  virtual void video_set_frame_mode(int32_t a) { unported("0x00579250", {a}); }
  virtual uint32_t video_show_cursor(uint32_t on) { return unported("0x00579840", {on}); }
  virtual void video_set_gamma(int32_t a) { unported("0x0057a3c0", {a}); }
  virtual void video_restore() { unported("0x00577cb0", {}); }
  virtual void video_begin_scene() { unported("0x00579af0", {}); }
  virtual void video_draw_3d() { unported("[0x0072f3ec]", {}); }
  virtual void video_end_scene() { unported("0x00579b10", {}); }
  virtual void video_double(Handle a, Handle b) { unported("[0x0072f358]", {a, b}); }
  virtual void video_apply_fog() { unported("[0x0072f3f0]", {}); }
  virtual void render_set_target(Handle target, Handle rect, Handle b, Handle a) {
    unported("0x00557f90", {target, rect, b, a});
  }
  virtual void render_set_target2(Handle target, Handle rect, Handle b, Handle a) {
    unported("0x00558290", {target, rect, b, a});
  }
  virtual void render_set_pitch(Handle p) { unported("0x00558070", {p}); }
  virtual void render_reset() { unported("0x00557b20", {}); }
  virtual void render_flush(Handle rect) { unported("0x00558010", {rect}); }
  virtual void render_finish() { unported("0x00558610", {}); }

  // The 2D front end (zTwoD) and the shell screens.
  virtual void twod_set_cursor(uint32_t v) { unported("0x0059fb80", {v}); }
  virtual void twod_reset(Handle h) { unported("0x0059fa10", {h}); }
  virtual void screen_refresh(Handle s) { unported("0x0059fa60", {s}); }
  virtual void screen_hide(Handle s) { unported("0x0059fa40", {s}); }
  virtual void shell_clear(uint32_t state) { unported("0x0048ee00", {state}); }
  // zGameApp::Push (0x00590400) of a state the port does not have (a menu
  // object of 0x00609858..0x00609878, by its handle).
  virtual void push_state(Handle state, uint32_t arg) { unported("0x00590400", {state, arg}); }
  // The menu objects' fields the screens read: a shell state's screen (+0x0c).
  virtual Handle state_screen(Handle state) { return peek32(state + 0xc); }
  // The main menu's BACKGROUND image and its scrolling ([0x00609854] +0x110ec,
  // +0x111e8); false without a main menu. Ported: mech3::main_menu().
  virtual bool main_menu_background(zimg::Texture** image, uint8_t* moving);
  virtual uint8_t mission_running();                   // 0x0047fb70: the Core state's +0x19, ported
  // The credits state's reset of its screen's clock ([screen+0x11adc] = 0).
  virtual void credits_restart(Handle screen) { poke32(screen + 0x11adc, 0); }
  virtual void confirm_quit();                         // 0x00482a20, ported: Mech3App::confirm_quit
  virtual Handle construct(uint32_t ctor, std::initializer_list<Arg> args) {  // new + ctor
    char key[16];
    format_key(key, ctor);
    return unported(key, args);
  }
  virtual Handle wait_new(int32_t a, int32_t b) { return unported("0x00599f70", {a, b}); }
  virtual void wait_start() { unported("wait.vt+0x8", {0, 0}); }
  virtual uint32_t wait_poll() { return unported("wait.vt+0x4", {0, 0}); }
  virtual void wait_end() { unported("wait.vt+0xc", {}); }

  // Movies (zFmv).
  virtual Handle movie_new() { return unported("0x00598cc0", {0, 0, 0}); }  // new(0x20)
  virtual void movie_set_window(Handle m, uint32_t hwnd) { poke32(m + 4, hwnd); }
  virtual int32_t movie_load(Handle m, const char* file, const char* name) {
    return static_cast<int32_t>(unported("0x00598d80", {m, file, name}));
  }
  virtual void movie_play(Handle m) { unported("0x005996d0", {m}); }
  virtual uint32_t movie_playing(Handle m) { return unported("0x00599710", {m}) & 0xff; }
  virtual void movie_stop(Handle m, int32_t a) { unported("0x00599750", {m, a}); }
  virtual void movie_delete(Handle m) { unported("0x00598d00", {m}); }  // + operator delete

  // Input (zInput).
  virtual void input_poll(uint8_t a) { unported("0x0051b360", {a}); }
  virtual uint32_t key_state(int32_t key) { return unported("0x0051a9b0", {key}); }  // a DIK code's state, zin::key_state
  virtual void input_clear() { unported("0x0051a300", {}); }
  virtual void input_reset() { unported("0x0051b1c0", {}); }
  virtual void input_capture() { unported("0x0051b310", {}); }
  virtual void input_release() { unported("0x0051b340", {}); }
  virtual void input_center_mouse() { unported("0x0051b560", {}); }
  virtual void mouse_set_pos(int32_t x, int32_t y) { unported("0x0051b4f0", {x, y}); }
  virtual void mouse_set_accel(float x, float y) { unported("0x0051b790", {zgame::bits(x), zgame::bits(y)}); }
  virtual void set_screen_mode(int32_t mode) { unported("0x004ab6b0", {mode}); }
  // What the controls screen calls (docs/menus.md, The controls screen): zInput's
  // key and button capture and its command map (the current map's 0x0051d2xx
  // wrappers), the stick's reading and the controls setup (controls_cfg).
  // Ported: the defaults are the ports.
  virtual uint32_t input_read_key(int32_t wait);                // 0x0051aa00
  virtual int32_t joy_wait_button(int32_t wait);                // 0x0051dd20
  virtual int32_t mouse_wait_button(int32_t wait);              // 0x0051baf0
  virtual Handle joy_state() { return unported("0x0051dce0", {}); }  // the stick's last read, 0 none
  // 0x0042fea0 (src, joy, mouse 0): a source's reading off a joy_state() read.
  virtual float source_value(int32_t src, Handle joy) {
    return zgame::from_bits(unported("0x0042fea0", {src, joy, 0}));
  }
  virtual void joy_use();                                       // 0x0051b2c0
  virtual void joy_unuse();                                     // 0x0051b2f0
  virtual uint32_t key1_of(int32_t cmd);                        // 0x0051d200
  virtual uint32_t key2_of(int32_t cmd);                        // 0x0051d210
  virtual uint32_t joy_of(int32_t cmd);                         // 0x0051d220
  virtual uint32_t mouse_of(int32_t cmd);                       // 0x0051d230
  virtual int32_t key1_command(uint32_t key);                   // 0x0051d240
  virtual int32_t key2_command(uint32_t key);                   // 0x0051d250
  virtual int32_t joy_command(uint32_t b);                      // 0x0051d260
  virtual int32_t mouse_command(uint32_t b);                    // 0x0051d270
  virtual void bind_key1(uint32_t key, int32_t cmd);            // 0x0051d280
  virtual void bind_key2(uint32_t key, int32_t cmd);            // 0x0051d290
  virtual void bind_joy(uint32_t b, int32_t cmd);               // 0x0051d2a0
  virtual void bind_mouse(uint32_t b, int32_t cmd);             // 0x0051d2b0
  virtual const char* command_name(int32_t cmd, char* out, int32_t n);      // 0x0051d310
  virtual const char* key_name(uint32_t key, char* out, int32_t n);         // 0x0051d330
  virtual const char* joy_button_name(uint32_t b, char* out, int32_t n);    // 0x0051d350
  virtual const char* mouse_button_name(uint32_t b, char* out, int32_t n);  // 0x0051d370
  virtual void input_map_refresh();                             // 0x0051d1c0
  virtual int32_t controls_defaults();                          // 0x00411150
  virtual int32_t controls_group_count();                       // 0x00410f80
  virtual const char* controls_group_title(int32_t group);      // 0x00410fa0
  virtual int32_t controls_group_size(int32_t group);           // 0x00410fb0
  virtual int32_t controls_group_command(int32_t group, int32_t i);  // 0x00410fd0
  virtual void controls_read(const char* path);                 // 0x004120a0
  virtual void controls_write(const char* path);                // 0x00411e60
  virtual char* active_ctl_cfg();                               // *[0x005f4d40], ActiveCtlCfg (0x104 bytes)
  // FindFirstFileA / FindNextFileA's files for a pattern ("keys\\*.zrd"), in
  // the listing's order. FIX (A): the write dir's and the mounts' (zutl::list).
  virtual std::vector<std::string> list_files(const char* pattern);
  // 0x004ac150 with three strings for the message's %1 %2 %3.
  virtual uint32_t message_box3(int32_t a, int32_t caption, int32_t text, const char* s1, const char* s2,
                                const char* s3) {
    return unported("0x004ac150", {a, caption, text, s1, s2, s3});
  }
  // A ported screen made by a ported state rather than by construct(): its
  // handle (the program registers it as construct's screens; 0 elsewhere).
  virtual Handle adopt_screen(MenuScreen* screen, uint32_t size) {
    (void)screen, (void)size;
    return 0;
  }

  // Sound and music.
  virtual void cd_pause();                             // 0x00572c90, ported: zsnd_cd
  virtual void cd_resume(int32_t mode);                // 0x00572d70, ported
  virtual void cd_stop();                              // 0x00572c30, ported
  virtual int32_t cd_track_count();                    // 0x00573040, ported
  virtual void music_play(int32_t track, int32_t mode);  // 0x004ac290, ported: mech3_music
  virtual void sound_update(int32_t a, int32_t b) { unported("0x0056dfb0", {a, b}); }
  virtual Handle sound_find(const char* name) { return unported("0x0056f6f0", {name}); }
  virtual void sound_play(Handle s, float v) { unported("0x0056e3d0", {s, zgame::bits(v)}); }
  virtual Handle sound_listener() { return unported("0x0056eb10", {}); }
  virtual void sound_stop_all(Handle s) { unported("0x0056f0a0", {s}); }
  virtual void sound_release_all(Handle s) { unported("0x0056f190", {s}); }
  virtual void sound_reset() { unported("0x005716a0", {}); }
  virtual void voice_start(const char* name) { unported("0x0056f520", {name}); }
  virtual void voice_stop(Handle s) { unported("0x0056e9f0", {s}); }
  virtual void voice_release(const char* name) { unported("0x0056f530", {name}); }

  // The mission: world, pilots, mechs, views.
  virtual Handle pilot_get(const char* name) { return unported("0x004788b0", {name}); }
  virtual void pilot_select_game(Handle p, int32_t type) { unported("0x00478250", {p, type}); }
  virtual Handle pilot_game(Handle p) { return unported("0x0040a0f0", {p}); }
  virtual void pilot_set_mech(Handle p, uint32_t id) { unported("0x004783c0", {p, id}); }
  virtual void pilot_update(Handle p, float dt) { unported("0x004783a0", {p, zgame::bits(dt)}); }
  virtual void pilot_record_mission(Handle p, int32_t a) { unported("0x00478f10", {p, a}); }
  virtual Handle pilot_campaign(Handle p) { return unported("0x00479440", {p}); }
  virtual uint32_t pilot_campaign_number(Handle p) { return unported("0x00479420", {p}); }
  virtual void pilot_save_all(const char* name) { unported("0x00477f50", {name}); }
  virtual void lab_start(Handle cfg, int32_t a) { unported("0x005a2ba0", {cfg, a}); }
  virtual void mech_apply_config(Handle mech, uint32_t cfg) { unported("0x00414430", {mech, cfg}); }
  virtual void mech_set_mode_a(Handle mech, int32_t a) { unported("0x0041ca30", {mech, a}); }
  virtual void mech_set_mode_b(Handle mech, int32_t a) { unported("0x0041cc30", {mech, a}); }
  virtual void mech_draw_cockpit(Handle mech) { unported("0x0041ab90", {mech}); }
  virtual Handle mech_ai(Handle data) { return unported("0x00430aa0", {data}); }
  virtual void ai_radar_update(Handle ai, float dt) { unported("0x0043cd20", {ai, zgame::bits(dt)}); }
  virtual void ai_reset(uint32_t list) { unported("0x004d8610", {list}); }
  virtual void entities_sort() { unported("0x004d5ca0", {}); }  // ported: mech3/sensors
  virtual void mission_results(uint8_t a, uint8_t b) { unported("0x0047ae10", {a, b}); }
  virtual void missions_update(uint32_t list, float dt) { unported("0x0048f470", {list, zgame::bits(dt)}); }
  virtual void missions_reset(uint32_t list) { unported("0x00490720", {list}); }
  virtual void missions_start(uint32_t list) { unported("0x00490880", {list}); }
  virtual const char* mission_dir(uint32_t n);         // 0x00484c50, ported: mech3/mission
  virtual void sensors_update() { unported("0x004a8cd0", {}); }  // ported: mech3/sensors
  virtual void camera_update() { unported("0x0040f8d0", {}); }
  virtual void effects_draw(int32_t a, Handle rect) { unported("0x005a6790", {a, rect}); }
  virtual void particles_draw(float dt) { unported("0x005a67c0", {zgame::bits(dt)}); }
  virtual void lists_update() { unported("0x004f7d80", {}); }
  virtual void view_reset(Handle view) { unported("0x0040ad90", {view}); }
  virtual void view_draw_node(Handle cam, Handle node, Handle world) { unported("0x004f73f0", {cam, node, world}); }
  virtual void view_draw_hud(Handle view) { unported("0x004b51f0", {view}); }
  virtual void node_set_active(Handle node, int32_t on) { unported("0x004eb170", {node, on}); }
  virtual void node_set_shadow(Handle node, int32_t on) { unported("0x004eb6d0", {node, on}); }
  virtual void camera_set_near(int32_t a) { unported("0x004f3540", {a}); }
  virtual void camera_set_far(int32_t a) { unported("0x004f3550", {a}); }
  virtual void camera_draw_scene(Handle cam, Handle scene) { unported("0x004f7350", {cam, scene}); }
  virtual void window_set_size(Handle w, int32_t width, int32_t height) {
    unported("0x004f9fa0", {w, width, height});
  }
  virtual void window_set_origin(Handle w, int32_t x, int32_t y) { unported("0x004fa0b0", {w, x, y}); }
  virtual void window_apply(Handle w) { unported("0x00526e10", {w}); }
  virtual int32_t model_global_light() { return static_cast<int32_t>(unported("0x00526a80", {})); }
  virtual void model_set_global_light(int32_t on) { unported("0x00526a70", {on}); }
  virtual Handle scene_begin() { return unported("0x005674d0", {}); }
  virtual void scene_begin_3d() { unported("0x00567570", {}); }
  virtual void scene_draw(Handle s) { unported("0x00567580", {s}); }
  virtual void scene_end(Handle s) { unported("0x005675e0", {s}); }
  virtual void scene_set_scale(int32_t a, float v) { unported("0x005674e0", {a, zgame::bits(v)}); }
  virtual void scene_set_scale_b(float v) { unported("0x00567530", {zgame::bits(v)}); }
  virtual void scene_present() { unported("0x00567dc0", {}); }
  virtual void scene_reset() { unported("0x00566f10", {}); }
  virtual void update_shadows();                       // 0x004814e0, ported: app_states
  // The projected-shadow casters (zmod, ported; docs/render-shadows.md). A
  // caster is a handle the defaults map to the port's records; the node,
  // position and light are the original's objects (node_of, peek).
  virtual Handle shadow_new();                                  // 0x00542c20
  virtual void shadow_set_size(Handle c, int32_t w, int32_t h); // 0x00542d30
  virtual void shadow_set_node(Handle c, Handle node);          // 0x00542dd0
  virtual void shadow_enable(Handle c, int32_t on);             // 0x00542e60
  virtual void shadow_set_color(Handle c, uint32_t color);      // 0x00542e00
  virtual void shadow_set_position(Handle c, Handle pos);       // 0x00542de0, 3 floats at pos
  virtual int32_t shadow_draw(Handle c, Handle light_dir, float ground);  // 0x00542e70
  virtual void shadow_upload(Handle c);                         // 0x00542e10
  virtual void shadow_release_texture(Handle c);                // 0x00542e40
  virtual void shadow_delete(Handle c);                         // 0x00542ca0
  virtual float clouds_at(float x, float z);                    // 0x00543ac0, ported: gmod_light
  virtual Handle entity_position(Handle entity) { return unported("0x005a85e0", {entity}); }  // 3 floats
  // A node handle as the port's node: none until the entities are ported.
  virtual zcls::Node* node_of(Handle node) {
    unported("node_of", {node});
    return nullptr;
  }
  virtual void hud_update() { unported("0x0040fac0", {}); }
  virtual void hud_draw() { unported("0x0040e4b0", {}); }
  virtual void shadows_reset() { unported("0x004e6900", {}); }
  virtual void classes_reset() { unported("0x004ea740", {}); }
  virtual void reader_close_archives(int32_t all) { unported("0x005573d0", {all}); }
  virtual void anim_stop(int32_t a) { unported("0x0050ace0", {a}); }
  virtual void debug_mark(const char* text) { unported("0x0040fef0", {text}); }
  virtual uint32_t network_lobby_active() { return unported("0x00586ce0", {}) & 0xff; }
  virtual Handle message_box_new(const char* file, const char* name) {  // new(0x12688)
    return unported("0x0048dfe0", {file, name});
  }
  virtual void message_box_quit_prompt(Handle box) { unported("0x0048e2d0", {box}); }
  virtual void fade(int32_t a, int32_t b, int32_t c, uint32_t d, int32_t e, int32_t f, int32_t g) {
    unported("0x0048ee70", {a, b, c, d, e, f, g});
  }
  virtual Handle key_list_build() { return unported("key_list_build", {}); }  // 0x00482570's body
  virtual void hud_text_clear(Handle text) { unported("vt+0x7c", {text, ""}); }  // cdecl (this, "")

  // The app's small accessors called from the briefing (0x0040a7d0..0x0040a830).
  virtual uint32_t app_window() { return unported("0x0040a7d0", {}); }
  virtual Handle app_sounds() { return unported("0x0040a800", {}); }
  virtual uint8_t app_flag_1c3() { return static_cast<uint8_t>(unported("0x0040a830", {})); }
  virtual uint8_t briefing_has_movie() { return static_cast<uint8_t>(unported("0x0040a820", {})); }
  virtual void sounds_nop(Handle s, uint32_t a) { unported("0x004a5bc0", {s, a}); }
  virtual void sounds_load(Handle s, const char* name, int32_t a) { unported("0x0040baf0", {s, name, a}); }
  virtual void briefing_play_movie(uint32_t state) { unported("0x00409320", {state}); }
  virtual void debrief_voice_play(uint32_t state) { unported("0x0047abe0", {state}); }
  virtual void briefing_init(Handle s) { unported("0x00409ec0", {s}); }
  virtual void briefing_start(Handle s) { unported("0x00409eb0", {s}); }
  virtual void briefing_refresh(Handle s) { unported("0x00408bd0", {s}); }
  virtual void briefing_show(Handle s, int32_t a) { unported("0x00404ae0", {s, a}); }
  virtual uint32_t list_head(Handle list) { return unported("0x0049ccb0", {list}); }
  virtual void format(const char* fmt, std::initializer_list<Arg> args, char* out, int size);  // CString::Format

  static void format_key(char* out, uint32_t addr);
};

}  // namespace mech3
