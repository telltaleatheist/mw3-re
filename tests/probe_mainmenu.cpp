// Probe for test_mainmenu.py (docs/mainmenu.md, Testing): the main menu built
// from dialog.zrd's MAINMENU (or a synthetic one) on the ported zTwoD, then a
// plan of frames, presses and screen calls; every call out recorded through
// the calltrace callback (zTwoD's through the shared RecordingHost, the
// game's through a recording mech3::Engine), the screen dumped as text.
#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "mech3/app.h"
#include "mech3/mainmenu.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;
using mech3::Arg;

std::deque<std::string> g_text;

// A C string of the original's memory (the stubs' answers point into it).
const char* text_at(uint32_t a) {
  if (!a) return nullptr;
  std::string s;
  for (uint32_t i = 0; i < 256; ++i) {
    const char c = static_cast<char>(call("peek8", {a + i}));
    if (!c) break;
    s += c;
  }
  g_text.push_back(s);
  if (g_text.size() > 64) g_text.pop_front();
  return g_text.back().c_str();
}

struct MenuEngine : mech3::Engine {
  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  int32_t fast_menu() override { return static_cast<int32_t>(call("0x0040cec0", {}) & 0xff); }
  int32_t display_width() override { return static_cast<int32_t>(call("0x0040d760", {})); }
  int32_t display_height() override { return static_cast<int32_t>(call("0x0040d770", {})); }
  void music_play(int32_t track, int32_t mode) override { call("0x004ac290", {track, mode}); }
  void cd_stop() override { call("0x00572c30", {}); }
  const char* message(int32_t id) override { return text_at(call("0x00576040", {id})); }
  const char* version_number() override { return text_at(call("0x004af290", {})); }
  void confirm_quit() override { call("0x00482a20", {}); }
};

void dump_menu(mech3::MainMenu& m, Dump& d) {
  d.o << "menu ";
  d.kv("state", m.state);
  d.kv("busy", m.busy_);
  d.kv("def", m.definition ? 1 : 0);
  d.o << "\nstill ";
  d.image(&m.still);
  const mech3::ScrollImage& b = m.background;
  d.o << "\nbackground ";
  d.image(&m.background);
  d.kv("step", b.step_x);
  d.kv("stepy", b.step_y);
  for (int i = 0; i < 4; ++i) {
    d.kv("pc", b.piece[i]);
    d.kv("px", b.piece_pos[i][0]);
    d.kv("py", b.piece_pos[i][1]);
    d.r("psrc", b.piece_src[i]);
  }
  d.r("view", b.view);
  d.kv("moving", b.moving);
  d.o << "\ntitle ";
  d.image(&m.title);
  d.f("speed", m.title.speed);
  for (int i = 0; i < 7; ++i) {
    mech3::MainMenuButton& k = m.buttons[i];
    d.o << "\nbutton" << i << ' ';
    d.kv("reading", k.reading);
    d.kv("id", k.id);
    d.kv("borrowed", k.borrowed);
    d.t("stashed", k.stashed);
    d.r("reach", k.reach);
    d.kv("rx", k.ripple.x);
    d.kv("ry", k.ripple.y);
    d.kv("rflags", k.ripple.flags);
    d.kv("inner", k.ripple.inner);
    d.kv("outer", k.ripple.outer);
    d.kv("ramp", k.ripple.amplitude);
    d.f("rwl", k.ripple.wavelength);
    d.f("rph", k.ripple.phase);
    d.kv("rippling", k.rippling);
    d.f("speed", k.speed);
    d.f("amp", k.amplitude);
    d.f("wl", k.wavelength);
    d.f("phase", k.phase);
    d.kv("radius", k.radius);
    d.kv("maxr", k.max_radius);
    d.f("rr", k.reach_radius);
    d.f("rsq", k.reach_sq);
    d.kv("nowned", static_cast<int64_t>(k.owned.size()));
    d.o << "\n  line ";
    d.image(&k.line);
  }
  d.o << '\n';
}

}  // namespace

extern "C" {

// Opens `reader` as zReader's archive, builds the main menu for `state`, runs
// `plan` (lines, as test_mainmenu.py writes them), dumps it into `out`, then
// destroys it. Returns the dump's length, or -1.
int probe_mainmenu_run(const char* reader, uint32_t state, int no_voices, const char* plan, char* out, int size) {
  RecordingHost host;
  ztwod::set_host(&host);
  reset();
  MenuEngine engine;
  std::unique_ptr<mech3::Mech3App> app;
  {
    probe_app::Mute mute;
    app = std::make_unique<mech3::Mech3App>(&engine);
  }
  mech3::set_app(app.get());
  app->no_voices() = static_cast<uint8_t>(no_voices);
  const bool per_second = zfix::options().per_second;
  zfix::options().per_second = false;  // the original's path (zfix.h)
  zrdr::init();
  zrdr::close_archives(true);
  zrdr::open_archive(reader, true);
  ztwod::set_page_flip(true);
  std::unique_ptr<mech3::MainMenu> menu;
  std::string dump;
  std::istringstream in(plan);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    char op;
    ls >> op;
    if (op == 'N') {
      menu = std::make_unique<mech3::MainMenu>(engine, state);
    } else if (op == 'F') {
      uint32_t dt;
      ztwod::MouseState m{};
      uint32_t* w = reinterpret_cast<uint32_t*>(&m);
      ls >> std::hex >> dt;
      for (int i = 0; i < 15; ++i) ls >> w[i];
      g_mouse.push_back(m);
      menu->update(zgame::from_bits(dt));
      ++g_frame;
    } else if (op == 'A') {
      int v;
      ls >> v;
      menu->set_active(v);
    } else if (op == 'P') {
      int i;
      ls >> i;
      menu->buttons[i].on_left_up();
    } else if (op == 'B') {
      int v;
      ls >> v;
      call("busy", {menu->set_busy(static_cast<uint8_t>(v)), menu->busy()});
    } else if (op == 'D') {
      menu->dirty_all();
    } else if (op == 'H') {
      menu->restore_behind();
    } else if (op == 'U') {
      menu->unload();
    } else if (op == 'K') {
      int i, v;
      ls >> i >> v;
      menu->buttons[i].set_baked(v != 0);
    } else if (op == 'R') {
      int i;
      ls >> i;
      ztwod::Rect* r = menu->buttons[i].get_rect();
      if (r) call("rect", {r->left, r->top, r->right, r->bottom});
      else call("rect", {});
    } else if (op == 'E') {
      int i, v;
      ls >> i >> v;
      menu->buttons[i].enabled = v;
      menu->buttons[i].refresh();
    } else if (op == 'X') {  // dumped, then destroyed (its calls compared too)
      Dump d;
      dump_dialog(*menu, d);
      dump_menu(*menu, d);
      dump = d.o.str();
      menu.reset();
    }
  }
  zrdr::close_archives(true);
  zfix::options().per_second = per_second;
  mech3::set_app(nullptr);
  ztwod::set_host(nullptr);
  if (static_cast<int>(dump.size()) >= size) return -1;
  std::memcpy(out, dump.data(), dump.size() + 1);
  return static_cast<int>(dump.size());
}

}  // extern "C"
