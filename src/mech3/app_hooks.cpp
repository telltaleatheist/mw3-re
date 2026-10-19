#include "mech3/app_hooks.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "crt/msvc_rand.h"
#include "gamez/zsound/zsnd_cd.h"
#include "gamez/zutil/ztimer.h"
#include "gamez/zutil/zutl_msg.h"
#include "mech3/music.h"
#include "mech3/options.h"

namespace mech3 {

// The object, then up to three arguments (the most any call here passes).
uint32_t Engine::vcall(Handle obj, uint32_t slot, std::initializer_list<Arg> args) {
  char key[16];
  std::snprintf(key, sizeof key, "vt+0x%x", slot);
  const Arg* a = args.begin();
  switch (args.size()) {
    case 0: return unported(key, {obj});
    case 1: return unported(key, {obj, a[0]});
    case 2: return unported(key, {obj, a[0], a[1]});
    default: return unported(key, {obj, a[0], a[1], a[2]});
  }
}

void Engine::format_key(char* out, uint32_t addr) { std::snprintf(out, 16, "0x%08x", addr); }

// [0x0060861c]+0x18, a string inside the pilot: until the pilot module is
// ported there is no name, so a lobby launch goes to the main menu.
const char* Engine::pilot_name() { return nullptr; }

float Engine::frame_dt() { return ztimer::state().dt; }
int32_t Engine::in_game_vmode() { return options().in_game_vmode; }
float Engine::mouse_accel_x() { return options().mouse_accel[0]; }
float Engine::mouse_accel_y() { return options().mouse_accel[1]; }

int32_t Engine::hw_card_flag() { return mech3::hw_card_flag(); }
int32_t Engine::hw_card_dev() { return mech3::hw_card_dev(); }
void Engine::select_hw(int32_t hw) { mech3::select_hw(hw); }
void Engine::set_hw_card_dev(int32_t dev) { mech3::set_hw_card_dev(dev); }
int32_t Engine::vmode() { return mech3::vmode(); }
void Engine::set_vmode(int32_t mode) { mech3::set_vmode(mode); }
int32_t Engine::gfx_flags() { return mech3::gfx_flags(); }
int32_t Engine::pixel_double() { return mech3::pixel_double(); }
// The rectangles are named by number for the hooks that take them.
Handle Engine::render_rect() { return 1; }
Handle Engine::display_rect() { return 2; }
Handle Engine::window_rect() { return 3; }
Handle Engine::display_depth() { return static_cast<Handle>(mech3::display_depth()); }
Handle Engine::vstride() { return static_cast<Handle>(mech3::vstride()); }
int32_t Engine::window_h() { return mech3::window_h(); }
void Engine::set_use_joystick(int32_t on) { mech3::set_use_joystick(on); }
void Engine::set_fast_menu(int32_t on) { mech3::set_fast_menu(on); }
void Engine::options_apply_all() { mech3::apply_all(); }

void Engine::exit_process(uint32_t code) { std::exit(static_cast<int>(code)); }
void Engine::cursor_pos(int32_t* x, int32_t* y) {
  unported("GetCursorPos", {});
  *x = 0;
  *y = 0;
}
uint32_t Engine::rand() { return static_cast<uint32_t>(crt::msvc_rand()); }
const char* Engine::command_line() { return ""; }
void Engine::print(const char* format, const char* arg) {
  if (arg) std::printf(format, arg);
  else std::fputs(format, stdout);
}

const char* Engine::message(int32_t id) {
  const zutl::Messages& m = zutl::messages();
  const char* s = m.get_text ? m.get_text(id) : nullptr;
  return s ? s : "";
}
const char* Engine::version_string() { return "1.0"; }
const char* Engine::message_format(int32_t id, const char* arg) {
  unported("0x00575fd0", {id, arg});
  return message(id);
}
void Engine::print_version(char* out, int size, int32_t major, int32_t minor) {
  std::snprintf(out, static_cast<size_t>(size), "%d.%d", static_cast<int>(major), static_cast<int>(minor));
  unported("sprintf", {out});
}
const char* Engine::version_number() { return "1.2"; }
const char* Engine::mission_dir(uint32_t n) {
  unported("0x00484c50", {n});
  return "";
}

void Engine::cd_pause() { zsnd_cd::pause(); }
void Engine::cd_resume(int32_t mode) { zsnd_cd::resume(mode); }
void Engine::cd_stop() { zsnd_cd::stop(); }
int32_t Engine::cd_track_count() {
  const zsnd_cd::State& s = zsnd_cd::state();
  return (s.flags & 2) ? s.num_tracks : 0;
}
void Engine::music_play(int32_t track, int32_t mode) { mech3_music::play(track, mode); }

// MFC's CString::Format for the formats these files use: %s and %d.
void Engine::format(const char* fmt, std::initializer_list<Arg> args, char* out, int size) {
  std::string s;
  const Arg* a = args.begin();
  for (const char* p = fmt; *p; ++p) {
    if (p[0] == '%' && (p[1] == 's' || p[1] == 'd') && a != args.end()) {
      if (p[1] == 's') s += a->s ? a->s : "";
      else s += std::to_string(static_cast<int32_t>(a->v));
      ++a;
      ++p;
    } else {
      s += *p;
    }
  }
  std::snprintf(out, static_cast<size_t>(size), "%s", s.c_str());
  unported("CString::Format", {out});
}

}  // namespace mech3
