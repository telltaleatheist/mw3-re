#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "gamez/zutil/zutl_msgdll.h"
#include "mech3/app_hooks.h"
#include "mech3/options.h"
#include "platform/platform.h"

namespace zvid { struct Surface; }

namespace platform {
class Render3D;
class SoundOutput;
}  // namespace platform

// The program's mech3::Engine (docs/program.md): every hook of the app and its
// states that has a port is wired to it, the Windows calls go to the platform
// (FIX (A), each named below), and the rest stay the base's unported() calls,
// logged once each by the original address. Our addition, not original code:
// it stands where the exe's other modules will be.
namespace mech3 {

// The CD's table of contents over the mounts: music/trackNN.* in the write dir
// or any mounted folder (CLAUDE.md, Music).
struct MountFiles : platform::Files {
  bool exists(const std::string& path) override;
};

struct Platform {
  platform::Window* window = nullptr;
  platform::Alert* alert = nullptr;
  platform::Clock* clock = nullptr;
  platform::Input* input = nullptr;
  platform::SoundOutput* sound = nullptr;
  platform::MusicOutput* music = nullptr;
  platform::Files* files = nullptr;     // what music/trackNN.* the "disc" holds
  platform::Render3D* render = nullptr;
  platform::Screen2D* screen = nullptr;
  platform::Settings* settings = nullptr;  // the registry's stand-in (settings.md); none: nothing saved
  uint32_t ram_mb = 4096;               // the machine's memory, for detail.zrd's RAM_KB
  int window_width = 1280, window_height = 960;  // the window made for the 640x480 game
};

class GameEngine;
class MenuScreen;
class MenuState;
void wire_mission_hooks(GameEngine* e);  // mech3/mission's hooks to the ports (app_mission.cpp)
void wire_pilot_hooks(GameEngine* e);    // the pilot module's calls out (app_menus.cpp)

// The briefing screen open ([0x005f4c80]) hears the mission load's fraction
// (0x004050d0) and lines (0x00405120); none open, nothing (app_menus.cpp).
void program_briefing_progress(float f);
void program_briefing_line(const char* s);
uint32_t program_slot_count();  // the chosen pilot's campaign's slot count (0x00435180), 0 without one

class GameEngine : public Engine {
 public:
  // A mission started from the command line (our addition, docs/program.md):
  // the pilot's game answers these, and the load goes on to the Core state.
  struct Launch {
    int32_t index = 0;   // the mission directory (0x00484c50): c1 c2 c3 c4 c4b t1
    int32_t type = 1;    // the game type: 1 a campaign mission (gametypes.md)
    int32_t number = 1;  // the mission number (world.zrd's m<n>, the start animation's)
    int32_t mech = 9;    // the player's mech by roster index (9 the Mad Cat), its first configuration
    bool have_pos = false;  // the camera placed (--pos/--look): there is no mech to follow yet
    float eye[3] = {}, yaw = 0, pitch = 0;
    bool active = false;
    // A debug start goes on to the Core state as soon as the mission is
    // loaded (in place of the briefing's LAUNCH); from the menus the
    // briefing's LAUNCH does (campaign.md section 7, item 5).
    bool auto_launch = true;
    // The slot's loadout and skin (the pilot's game's; the Config is the
    // slot's own, alive while the pilot's game is), else the mech's first.
    Handle config = 0;
    int32_t skin = 0;
    int32_t op = 1;  // a campaign's operation (the game's vt+0x48)
    // The chosen pilot's handle stays in [0x0060861c] (a debug start puts a
    // stand-in there).
    bool keep_pilot = false;
  };
  void set_launch(const Launch& l);
  const Launch& launch() const { return launch_; }
  void missions_start(uint32_t list) override;   // 0x00490880: mech3::missions().start()
  void missions_update(uint32_t list, float dt) override;  // 0x0048f470: mech3::missions().update()
  void missions_reset(uint32_t list) override;   // 0x00490720: only the mission number is kept
  void sensors_update() override;                // 0x004a8cd0: mech3::ai::sensors_update
  void entities_sort() override;                 // 0x004d5ca0: mech3::ai::entities_sort
  void ai_on() override;                         // 0x004ea740: mech3::ai::ai_switch_on
  // Core's resume(7) on the pilot's mech: the HUD's fall flag, then the
  // power-up (the spawn left it powered down).
  void mech_set_mode_a(Handle mech, int32_t a) override;  // 0x0041ca30
  void timer_update() override;                  // 0x00575a00, then the step's copies (sync_frame_step)
  void weapons_shutdown() override;               // 0x00583870: zwep::shutdown, no per-def callback (0x00584920)
  void mech_set_mode_b(Handle mech, int32_t a) override;  // 0x0041cc30 (Mech::power)
  void fade(int32_t a, int32_t b, int32_t c, uint32_t d, int32_t e, int32_t f, int32_t g) override;
  Handle pilot_game(Handle p) override;
  uint32_t pilot_campaign_number(Handle p) override;  // 0x00479420: the launch's mission
  // The briefing (mech3/briefing, campaign.md section 7, item 5): the Load
  // state's game from the pilot the pilot screen chose, the ported screen's
  // calls into the ported mission, launch, states and zSound.
  void load_state_enter() override;
  Handle pilot_campaign(Handle p) override;      // 0x00479440: the launch's game
  void mission_results(uint8_t a, uint8_t b) override;  // 0x0047ae10
  Handle mission_salvage_store() override;              // [0x00609990], the ported store
  std::vector<LanceMech> debrief_lance() override;     // the ported mechs
  uint32_t player_allies() override;
  void win_keep_scores(Handle screen, int32_t n) override;  // into the ported campaign
  void briefing_script_start(Handle s) override;
  void briefing_init(Handle s) override;
  void briefing_start(Handle s) override;
  void briefing_map_image(ztwod::Dialog* screen, zrdr::Node* section) override;
  bool missions_loaded() override;
  bool mission_launch() override;
  void mission_cleanup(int32_t unload) override;
  void replace_state(int32_t n) override;
  Handle voice_play(Handle s, float v) override;
  int32_t voice_status(Handle v) override;
  void sound_cue_briefing(Handle s) override;
  uint8_t app_flag_1ba() override;
  Handle mission_navs() override;
  uint32_t vcall(Handle obj, uint32_t slot, std::initializer_list<Arg> args) override;
  void view_reset(Handle view) override;  // 0x0040ad90, the port's on the program's view
  // The menu screens that are ported (app_menus.cpp): the shell states hold a
  // handle, a block of the module memory standing for the object, whose
  // offsets lead to its parts (the screen, its buttons).
  Handle construct(uint32_t ctor, std::initializer_list<Arg> args) override;
  void screen_refresh(Handle s) override;
  void screen_hide(Handle s) override;
  void twod_reset(Handle h) override;
  void twod_set_cursor(uint32_t v) override;  // 0x0059fb80: ztwod::set_page_flip
  void shell_clear(uint32_t state) override;  // 0x0048ee00: ShellState::clear_screen
  void push_state(Handle state, uint32_t arg) override;
  bool menu_vcall(Handle obj, uint32_t slot, std::initializer_list<Arg> args, uint32_t* out);
  MenuScreen* screen_of(Handle h, uint32_t* offset = nullptr) const;
  void delete_screens();  // the ones still open at exit, and the menu objects
  MenuState* menu_state_of(Handle h) const;
  MenuState* popup_state_of(Handle h) const;  // the controls screen's popups (+0x16760, +0x1677c)
  Handle adopt_screen(MenuScreen* screen, uint32_t size) override;
  Handle state_screen(Handle state) override;
  Handle joy_state() override;                 // 0x0051dce0: zin::joy_state
  float source_value(int32_t src, Handle joy) override;  // 0x0042fea0: mech3::source_value
  uint32_t message_box3(int32_t a, int32_t caption, int32_t text, const char* s1, const char* s2,
                        const char* s3) override;
  void credits_restart(Handle screen) override;
  void pilot_state_set_mode(Handle state, int32_t mode) override;
  void pilot_chosen(Pilot* p) override;
  void pilot_record_mission(Handle p, int32_t a) override;
  void pilots_delete() override;
  void pilot_delete(const char* name) override;
  uint32_t message_box1(int32_t a, int32_t caption, int32_t text, const char* s) override;
  void sync_view();  // the player view's mode and target into its block (app_view.cpp)
  void set_mission_view(Handle view, Handle camera);  // the view 0x0040aca0 makes, and its camera
  Handle mission_camera() const { return camera_; }
  Handle alloc_block(uint32_t size);  // a block of the modules' memory (the peek/poke map)
  void video_clear_b(Handle rect) override;
  void lists_update() override;  // 0x004f7d80: zcls::run_action_lists
  void classes_shutdown() override;   // 0x004f8f40: zcls::classes_reset
  int32_t effects_init() override;    // 0x0050d270: zEffect's table and animations started
  void effects_shutdown() override;   // 0x0050d2b0: freed
  void decals_set_mode(int32_t v) override;  // 0x00501f90
  void decals_shutdown() override;           // 0x00502020
  void missions_shutdown() override;  // 0x004844b0: mech3::close_mechlib
  std::function<void(int frame)> on_present;  // after each frame shown (the 2D or the 3D)
  bool drew_scene() const { return last_drew_scene_; }  // the frame just shown was the 3D scene
  int frames_presented = 0;

  GameEngine(const Platform& p, std::string command_line);
  ~GameEngine() override;

  // The not-ported calls: logged the first time each key is reached.
  uint32_t unported(const char* key, std::initializer_list<Arg> args) override;
  const std::map<std::string, uint32_t>& unported_counts() const { return counts_; }
  void print_unported_summary() const;
  bool quiet = false;  // no log lines (tests)

  // The memory of modules not ported: what is poked reads back.
  uint32_t peek32(uint32_t addr) override;
  uint8_t peek8(uint32_t addr) override;
  void poke32(uint32_t addr, uint32_t v) override;
  void poke8(uint32_t addr, uint8_t v) override;

  // Windows (FIX (A)).
  uint32_t lobby_connection() override { return 0; }
  uint32_t activate_running_copy() override { return 1; }
  uint32_t register_window_class() override { return 1; }
  void unregister_window_class() override {}
  void update_window(uint32_t hwnd) override;
  void message_beep(uint32_t) override {}
  uint32_t message_box_text(uint32_t hwnd, const char* text, const char* caption, uint32_t type) override;
  uint32_t focus_window() override { return hwnd_; }
  void lock_system_keys(uint32_t) override {}
  void sleep(uint32_t ms) override;
  void remove_debug_menu(uint32_t) override {}
  void attach_debug_menu(uint32_t) override {}
  const char* command_line() override { return command_line_.c_str(); }
  void run_dir() override {}
  uint32_t message_dll_load(const char* name) override;
  uint32_t directx_version() override { return 0x700; }
  uint32_t install_options(uint32_t mask) override { return mask & install_options_; }  // 0x00597d80
  int32_t pick_video_device(int32_t, int32_t) override { return 0; }
  const char* message_format(int32_t id, const char* arg) override;
  uint32_t message_box(int32_t a, int32_t caption, int32_t text, int32_t arg = 0) override;

  // The app's objects.
  Handle script_new() override;
  void script_delete(Handle s) override;
  void free_block(Handle) override {}
  Handle frame_new() override { return kFrame; }
  Handle frame_create(Handle frame) override;
  uint32_t frame_hwnd(Handle) override { return hwnd_; }
  void frame_set_title(Handle frame, const char* title) override;
  void error_log_open(uint32_t hwnd, uint32_t a, const char* name) override;

  // Engine modules.
  void settings_init(const char* company, const char* product, const char* version) override;
  void list_pool_init(int32_t) override {}  // the rebuild's containers stand in (startup.md step 6)
  void list_pool_free() override {}
  void reader_init(const char* dir) override;
  void reader_open_archive(const char* path, int32_t current) override;
  void reader_close_archives(int32_t all) override;
  int32_t options_load(const char* name) override;
  int32_t sound_init(uint32_t window) override;
  void sound_set_mode(int32_t v) override;
  void sound_load(uint32_t window, const char* name, uint32_t callback, uint32_t a) override;
  void sound_shutdown_groups(uint32_t callback) override;
  void sound_shutdown() override;
  void light_init() override;
  // zInput (gamez/zinput, docs/input.md).
  void input_init(int32_t commands, const char* map) override;
  void input_start(uint32_t window, uint32_t cmd_show) override;
  void input_devices_shutdown() override;
  void input_acquire() override;
  void input_unacquire() override;
  uint32_t joystick_present() override;
  uint32_t force_feedback_present() override;
  void input_set_mode(uint32_t mode) override;
  void input_poll(uint8_t callbacks) override;
  uint32_t key_state(int32_t key) override;
  void input_clear() override;
  void input_reset() override;
  void input_capture() override;
  void input_release() override;
  void input_center_mouse() override;
  void mouse_set_pos(int32_t x, int32_t y) override;
  void mouse_set_accel(float x, float y) override;
  void clock_init() override;
  void cd_init(int32_t) override {}  // the MCI notify window: the pump stands in
  void video_select_device(int32_t dev) override { device_ = dev; }
  int32_t video_open(uint32_t window, int32_t hw, int32_t restore, int32_t vmode) override;
  int32_t span_init(int32_t) override { return 0; }
  uint32_t video_card_fn_390(int32_t) override { return 0; }
  uint32_t video_card_memory(int32_t dev, uint32_t* total) override;
  void video_shutdown() override {}
  void video_restore_window() override {}

  // zVideo's frame: the 16-bit buffer the 2D draws into, shown by the flip.
  Handle video_back_buffer() override { return kBackBuffer; }
  Handle video_front_buffer() override { return kFrontBuffer; }
  Handle video_target() override { return kTarget; }
  Handle video_screen_rect() override { return kScreenRect; }
  void video_lock() override;
  void video_unlock() override {}
  void video_clear(Handle rect) override;
  void video_flip(int32_t a, int32_t b, int32_t c, int32_t d) override;
  uint32_t video_show_cursor(uint32_t on) override;
  // The fade's stills (0x00579500, 0x0055b1f0, 0x00517cc0) on the 16-bit buffer;
  // the colour is the ported tint (0x00558540).
  Handle video_grab(int32_t buffer) override;
  void image_draw(Handle image, int32_t x, int32_t y, int32_t key, Handle rect) override;
  void screen_tint(uint16_t colour, Handle rect, double alpha) override;
  void image_free(Handle image) override;
  // The Direct3D frame: zvid's lock (0x00576d00 / 0x00576d60), scene nesting
  // (0x00579af0 / 0x00579b10), the quad ([0x0072f3ec]) and the finish (0x00558610).
  // 0x0040f8d0: the message panel's slide, then the view target's HUD data update.
  void camera_update() override;
  void message_panel_init(Handle, Handle) override;  // 0x0040f410
  void message_panel_free() override;                // 0x0040f7d0
  void video_begin_3d() override;
  void video_end_3d() override;
  void video_begin_scene() override;
  void video_draw_3d() override;
  void video_end_scene() override;
  void render_finish() override;
  int32_t lock_back(zvid::Surface* s);
  int32_t unlock_back(zvid::Surface* s);
  // The 2D layer's untouched pixels (FIX (A)): a magenta no 2D draw in the game uses as a colour.
  static constexpr uint16_t kLayerKey = 0xf81f;
  uint16_t* buffer16() { return buffer_.data(); }
  int buffer_width() const { return width_; }
  int buffer_height() const { return height_; }

  // Sound: zsnd objects by handle.
  void sound_update(int32_t a, int32_t b) override;
  Handle sound_find(const char* name) override;
  void sound_play(Handle s, float v) override;
  Handle sound_listener() override;
  void sound_stop_all(Handle s) override;
  void sound_release_all(Handle s) override;
  void sound_reset() override;
  void voice_start(const char* name) override;   // 0x0056f520: zsnd::load_set
  void voice_stop(Handle s) override;            // 0x0056e9f0: zsnd::stop
  void voice_release(const char* name) override; // 0x0056f530: zsnd::unload_set

  int32_t model_global_light() override;
  void model_set_global_light(int32_t on) override;

  Handle handle_of(void* p);  // a 32-bit handle for a port object (0 for null)
  void* object_of(Handle h) const;

  const Platform& platform_parts() const { return p_; }
  zutl::MessageDll& messages() { return dll_; }

  static constexpr Handle kFrame = 0x00010000, kBackBuffer = 0x00010010, kFrontBuffer = 0x00010020,
                          kTarget = 0x00010030, kScreenRect = 0x00010040,
                          kGrabs = 0x00010100;  // the fade's stills, 0x10 apart

 private:
  Platform p_;
  std::string command_line_;
  std::map<std::string, uint32_t> counts_;
  std::unordered_map<uint32_t, uint8_t> memory_;
  uint32_t hwnd_ = 0;
  int32_t device_ = -1;
  uint32_t install_options_ = 0x0101;  // HKLM's InstallOptions: a 3D card (FIX (A), fixes.md)
  int width_ = 640, height_ = 480;
  std::vector<uint16_t> buffer_;
  std::vector<uint8_t> cover_alpha_;   // the layer's blends (zrndr::LayerCover), FIX (A)
  std::vector<uint16_t> cover_colour_;
  std::map<Handle, std::vector<uint16_t>> grabs_;  // the fade's stills, by handle
  uint32_t cursor_ = 1;
  zutl::MessageDll dll_;
  std::string formatted_;
  std::vector<void*> objects_;           // handle - 0x00100000 - 1
  Launch launch_;
  Handle pilot_ = 0, game_ = 0, view_ = 0, camera_ = 0;
  std::map<int32_t, Handle> view_modes_;  // the view's camera ids' blocks (sync_view)
  Handle stub_pilot_ = 0, mech_block_ = 0;  // the stand-ins' blocks, made once
  Handle slot_ = 0;  // the debug launch's game slot (vt+0x34): the mech, its configuration, its skin
  Handle next_block_ = 0x01000000;
  bool scene_drawn_ = false, last_drew_scene_ = false, layer_drawn_ = false;
  std::unordered_map<void*, Handle> handles_;
  struct OpenScreen {
    MenuScreen* screen;
    uint32_t size;  // the original object's, for the offsets into it
    bool owned = false;  // made by another screen's part (adopt_screen), deleted with it
  };
  std::map<Handle, OpenScreen> screens_;
  std::map<Handle, MenuState*> menu_states_;  // the heap menu objects, by handle (menu_states.h)
};

// The frame's surface and tint hooks to `e`'s buffer and Render3D (null: none).
void wire_frame_hooks(GameEngine* e);

}  // namespace mech3
