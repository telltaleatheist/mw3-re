#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "gamez/zutil/zutl_msgdll.h"
#include "mech3/app_hooks.h"
#include "mech3/options.h"
#include "platform/platform.h"

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
  int window_width = 1280, window_height = 960;  // the window made for the 640x480 game
};

class GameEngine : public Engine {
 public:
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
  uint32_t install_options(uint32_t mask) override { return mask & 0x0101; }
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
  void settings_init(const char*, const char*, const char*) override {}
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
  void input_start(uint32_t window, uint32_t cmd_show) override;
  void input_devices_shutdown() override;
  void input_acquire() override;
  void input_unacquire() override;
  uint32_t joystick_present() override;
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
                          kTarget = 0x00010030, kScreenRect = 0x00010040;

 private:
  Platform p_;
  std::string command_line_;
  std::map<std::string, uint32_t> counts_;
  std::unordered_map<uint32_t, uint8_t> memory_;
  uint32_t hwnd_ = 0;
  int32_t device_ = -1;
  int width_ = 640, height_ = 480;
  std::vector<uint16_t> buffer_;
  uint32_t cursor_ = 1;
  zutl::MessageDll dll_;
  std::string formatted_;
  std::vector<void*> objects_;           // handle - 0x00100000 - 1
  std::unordered_map<void*, Handle> handles_;
};

}  // namespace mech3
