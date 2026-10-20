// Probe for test_menus.py (docs/menus.md, Testing): the credits, the game
// options and the graphics and audio options built from their dialogs (or
// synthetic ones) on the ported zTwoD, then a plan of frames, presses and
// screen calls; every call out recorded through the calltrace callback
// (zTwoD's through the shared RecordingHost, the game's through a recording
// mech3::Engine), the screen dumped as text.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "mech3/app.h"
#include "mech3/credits.h"
#include "mech3/options_menu.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;
using mech3::Arg;

std::deque<std::string> g_text;

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

int32_t i32(const char* key) { return static_cast<int32_t>(call(key, {})); }
float f32(const char* key) { return zgame::from_bits(call(key, {})); }

struct MenuEngine : mech3::Engine {
  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  int32_t fast_menu() override { return static_cast<int32_t>(call("0x0040cec0", {}) & 0xff); }
  int32_t display_width() override { return i32("0x0040d760"); }
  int32_t display_height() override { return i32("0x0040d770"); }
  // The stub's strings are made during the run: the text is the id's.
  const char* message(int32_t id) override {
    if (!call("0x00576040", {id})) return nullptr;
    char buf[16];
    std::snprintf(buf, sizeof buf, "M%x", static_cast<uint32_t>(id));
    return keep(buf);
  }
  bool main_menu_background(zimg::Texture** image, uint8_t* moving) override {
    const uint32_t m = call("peek32", {0x609854});
    if (!m) return false;
    *image = tex(call("peek32", {m + 0x110ec}));
    *moving = static_cast<uint8_t>(call("peek8", {m + 0x111e8}));
    return true;
  }
  uint8_t mission_running() override { return static_cast<uint8_t>(call("peek8", {0x608ed9})); }
  int32_t game_intensity() override { return i32("0x0040cdf0"); }
  void set_game_intensity(int32_t v) override { call("0x0040cc20", {v}); }
  int32_t camera_mode() override { return i32("0x0040ce70"); }
  void set_camera_mode(int32_t v) override { call("0x0040ce10", {v}); }
  int32_t invincible() override { return i32("0x0040cf20"); }
  void set_invincible(int32_t v) override { call("0x0040ced0", {v}); }
  int32_t infinite_ammo() override { return i32("0x0040cf80"); }
  void set_infinite_ammo(int32_t v) override { call("0x0040cf30", {v}); }
  int32_t heat() override { return i32("0x0040cfc0"); }
  void set_heat(int32_t v) override { call("0x0040cf90", {v}); }
  int32_t torso_auto_return() override { return i32("0x0040d010"); }
  void set_torso_auto_return(int32_t v) override { call("0x0040cfd0", {v}); }
  int32_t throttle_const() override { return i32("0x0040d060"); }
  void set_throttle_const(int32_t v) override { call("0x0040d020", {v}); }
  int32_t mw2_jump_jets() override { return i32("0x0040d1e0"); }
  void set_mw2_jump_jets(int32_t v) override { call("0x0040d1c0", {v}); }
  // The entries the screens read and write without a function.
  float mouse_accel_y() override { return zgame::from_bits(call("peek32", {call("peek32", {0x5f4d30})})); }
  void set_mouse_accel_y(float v) override { call("poke32", {call("peek32", {0x5f4d30}), zgame::bits(v)}); }
  int32_t in_game_vmode() override { return static_cast<int32_t>(call("peek32", {call("peek32", {0x5f4d10})})); }
  void set_in_game_vmode(int32_t v) override { call("poke32", {call("peek32", {0x5f4d10}), static_cast<uint32_t>(v)}); }
  void settings_write() override { call("0x00597a60", {}); }
  float cd_volume() override { return f32("0x0040dae0"); }
  void set_cd_volume(float v) override { call("0x0040dab0", {zgame::bits(v)}); }
  float sound_volume() override { return f32("0x0040daa0"); }
  void set_sound_volume(float v) override { call("0x0040da80", {zgame::bits(v)}); }
  int32_t texture_memory() override { return i32("0x0040d410"); }
  void set_texture_memory(int32_t v) override { call("0x0040d3f0", {v}); }
  int32_t sound_lod() override { return i32("0x0040d3e0"); }
  void set_sound_lod(int32_t v) override { call("0x0040d3d0", {v}); }
  int32_t object_lod() override { return i32("0x0040d3b0"); }
  void set_object_lod(int32_t v) override { call("0x0040d310", {v}); }
  int32_t effects_level() override { return i32("0x0040d2f0"); }
  void set_effects_level(int32_t v) override { call("0x0040d2a0", {v}); }
  int32_t shadow_parts() override { return i32("0x0040d220"); }
  void set_shadow_parts(int32_t v) override { call("0x0040d1f0", {v}); }
  int32_t shadow() override { return i32("0x0040d280"); }
  void set_shadow(int32_t v) override { call("0x0040d240", {v}); }
  int32_t gfx_flags() override { return i32("0x0040d480"); }
  void set_gfx_flags(int32_t v) override { call("0x0040d430", {v}); }
  int32_t hw_card_flag() override { return i32("0x0040d4d0"); }
  int32_t hw_card_dev() override { return i32("0x0040cc10"); }
  void set_hw_card_dev(int32_t v) override { call("0x0040cbf0", {v}); }
  const char* video_device_name(uint32_t i) override { return text_at(call("0x00579a70", {i})); }
  void pop_state(uint32_t arg) override { call("0x005905a0", {arg}); }
  void replace_with_exit() override {
    const uint32_t s = call("0x00482970", {4});
    call("0x00590250", {s, 0});
  }
};

// The screen's parts the plan names by number, as test_menus.py lists them.
struct Parts {
  std::vector<mech3::SlideButton*> slides;
  std::vector<mech3::CycleButton*> cycles;
  std::vector<ztwod::Control*> others;  // the sliders, the credits' names
};

void dump_slide(mech3::SlideButton& k, Dump& d) {
  d.kv("reading", k.reading);
  d.kv("id", k.id);
  d.kv("borrowed", k.borrowed);
  d.t("stashed", k.stashed);
  d.kv("rx", k.ripple.x);
  d.kv("ry", k.ripple.y);
  d.kv("inner", k.ripple.inner);
  d.kv("outer", k.ripple.outer);
  d.kv("ramp", k.ripple.amplitude);
  d.kv("rippling", k.rippling);
  d.f("amp", k.amplitude);
  d.f("phase", k.phase);
  d.kv("radius", k.radius);
  d.kv("maxr", k.max_radius);
  d.f("rr", k.reach_radius);
}

void dump_cycle(mech3::CycleButton& c, Dump& d) {
  d.kv("reading", c.reading);
  d.kv("cid", c.cycle_id);
  d.kv("index", c.index);
  d.kv("click", c.click_steps);
  d.kv("wraps", c.wraps);
  d.kv("hover", c.hover);
  d.kv("pdx", c.prev_dx);
  d.kv("pdy", c.prev_dy);
  d.kv("ndx", c.next_dx);
  d.kv("ndy", c.next_dy);
  d.kv("n", c.count());
  for (mech3::CycleButton::Item* it : c.items) {
    d.s("item", it->text ? it->text : "(null)");
    d.kv("v", it->value);
  }
  if (auto* a = dynamic_cast<mech3::AlertCycle*>(&c)) {
    d.kv("fnorm", a->font_normal);
    d.kv("falert", a->font_alert);
    d.kv("alert", a->alert);
  }
  d.o << "\n  prev ";
  d.control(&c.prev);
  d.o << "\n  next ";
  d.control(&c.next);
}

void dump_screen(mech3::MenuScreen& m, Parts& p, Dump& d) {
  d.o << "menu ";
  d.kv("state", m.state);
  d.kv("busy", m.busy_);
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
  for (size_t i = 0; i < p.slides.size(); ++i) {
    d.o << "\nslide" << i << ' ';
    dump_slide(*p.slides[i], d);
  }
  for (size_t i = 0; i < p.cycles.size(); ++i) {
    d.o << "\ncycle" << i << ' ';
    dump_cycle(*p.cycles[i], d);
  }
  for (size_t i = 0; i < p.others.size(); ++i) {
    d.o << "\nother" << i << ' ';
    if (auto* v = dynamic_cast<mech3::VolumeSlider*>(p.others[i])) {
      d.kv("reading", v->reading);
      d.f("value", v->value);
    }
  }
  if (auto* c = dynamic_cast<mech3::CreditsScreen*>(&m)) {
    mech3::CreditsScroll& s = c->names;
    d.o << "\ncredits ";
    d.f("clock", c->clock);
    d.f("unused", c->unused_11ad8);
    d.kv("left", s.left);
    d.kv("top", s.top);
    d.kv("right", s.right);
    d.kv("bottom", s.bottom);
    d.kv("total", s.total);
    d.f("rate", s.rate);
    d.kv("nsec", static_cast<int64_t>(s.sections.size()));
    int n = 0;
    for (mech3::CreditSection& sec : s.sections) {
      d.o << "\n  sec" << n++ << ' ';
      d.kv("nlines", static_cast<int64_t>(sec.lines.size()));
      for (mech3::CreditLine& l : sec.lines) {
        d.o << "\n    line ";
        d.fonttext(&l.text);
        d.kv("dx", l.dx);
        d.kv("dy", l.dy);
      }
    }
  }
  if (auto* g = dynamic_cast<mech3::GfxAudioScreen*>(&m)) {
    d.o << "\ngfx ";
    d.kv("dev", g->device_on_entry);
  }
  d.o << "\nentries ";
  d.kv("accel_y", call("peek32", {call("peek32", {0x5f4d30})}));
  d.kv("vmode", call("peek32", {call("peek32", {0x5f4d10})}));
  d.o << '\n';
}

}  // namespace

extern "C" {

// Opens `reader` as zReader's archive, builds screen `kind` (0 credits, 1 game
// options, 2 graphics and audio) for `state`, runs `plan` (lines, as
// test_menus.py writes them), dumps it into `out`, then destroys it. Returns
// the dump's length, or -1.
int probe_menus_run(const char* reader, int kind, uint32_t state, const char* plan, char* out, int size) {
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
  const bool per_second = zfix::options().per_second;
  const bool bugs = zfix::options().bugs;
  zfix::options().per_second = false;  // the original's path (zfix.h)
  zfix::options().bugs = false;
  zrdr::init();
  zrdr::close_archives(true);
  zrdr::open_archive(reader, true);
  ztwod::set_page_flip(true);
  std::unique_ptr<mech3::MenuScreen> menu;
  Parts parts;
  std::string dump;
  std::istringstream in(plan);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    char op;
    ls >> op;
    if (op == 'N') {
      parts = Parts{};
      if (kind == 0) {
        auto* c = new mech3::CreditsScreen(engine, state);
        menu.reset(c);
        parts.slides = {&c->cancel};
        parts.others = {&c->names};
      } else if (kind == 1) {
        auto* g = new mech3::GameOptionsScreen(engine, state);
        menu.reset(g);
        parts.slides = {&g->accept, &g->back, &g->gfx_audio, &g->controls};
        parts.cycles = {&g->difficulty, &g->camera, &g->invincible, &g->infinite_ammo, &g->heat,
                        &g->torso, &g->throttle, &g->jump_jets, &g->invert_y};
      } else {
        auto* g = new mech3::GfxAudioScreen(engine, state);
        menu.reset(g);
        parts.slides = {&g->accept, &g->back};
        parts.cycles = {&g->shadows, &g->self_shadows, &g->lighting, &g->resolution, &g->device,
                        &g->textures, &g->sound, &g->detail, &g->effects};
        parts.others = {&g->music, &g->sfx};
        g_kind[&g->music] = 'L';
        g_kind[&g->sfx] = 'L';
      }
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
    } else if (op == 'R') {  // +0x10: the options read (0) or written (1)
      int v;
      ls >> v;
      menu->apply(v);
    } else if (op == 'P') {  // a slide button's +0x30
      int i;
      ls >> i;
      parts.slides[static_cast<size_t>(i)]->on_left_up();
    } else if (op == 'C') {  // a cycle's +0x30
      int i;
      ls >> i;
      parts.cycles[static_cast<size_t>(i)]->on_left_up();
    } else if (op == 'O') {  // another control's +0x30
      int i;
      ls >> i;
      parts.others[static_cast<size_t>(i)]->on_left_up();
    } else if (op == 'S') {  // a cycle's +0xa4
      int i, v;
      ls >> i >> v;
      parts.cycles[static_cast<size_t>(i)]->step(v);
    } else if (op == 'Q') {  // a cycle's +0xa0
      int i, v;
      ls >> i >> v;
      parts.cycles[static_cast<size_t>(i)]->select(v);
    } else if (op == 'E') {  // a cycle's +0x90
      int i, v;
      ls >> i >> v;
      parts.cycles[static_cast<size_t>(i)]->set_enabled(v);
    } else if (op == 'W') {  // a cycle's +0x94
      int i, a, b, c;
      ls >> i >> a >> b >> c;
      parts.cycles[static_cast<size_t>(i)]->set_arrows(static_cast<uint8_t>(a), static_cast<uint8_t>(b),
                                                       static_cast<uint8_t>(c));
    } else if (op == 'B') {
      int v;
      ls >> v;
      call("busy", {menu->set_busy(static_cast<uint8_t>(v)), menu->busy()});
    } else if (op == 'D') {
      menu->dirty_all();
    } else if (op == 'H') {
      menu->restore_behind();
    } else if (op == 'K') {
      int i, v;
      ls >> i >> v;
      parts.slides[static_cast<size_t>(i)]->set_baked(v != 0);
    } else if (op == 'G') {  // the graphics screen's 0x00439a90
      int v;
      ls >> v;
      if (auto* g = dynamic_cast<mech3::GfxAudioScreen*>(menu.get())) g->device_changed(v);
    } else if (op == 'X') {
      Dump d;
      dump_dialog(*menu, d);
      dump_screen(*menu, parts, d);
      dump = d.o.str();
      menu.reset();
    }
  }
  zrdr::close_archives(true);
  zfix::options().per_second = per_second;
  zfix::options().bugs = bugs;
  mech3::set_app(nullptr);
  ztwod::set_host(nullptr);
  if (static_cast<int>(dump.size()) >= size) return -1;
  std::memcpy(out, dump.data(), dump.size() + 1);
  return static_cast<int>(dump.size());
}

}  // extern "C"
