// C entry points over ported functions, loaded by tests/differential/*.py to compare
// them with the original x86 code running in an emulator.
#include <cstdint>

#include "gamez/zrender/zrndr_span.h"

extern "C" {

void probe_span_fill16(uint16_t* dest, uint32_t color, uint32_t count) {
  zrndr::span_state().dest = reinterpret_cast<uint8_t*>(dest);
  zrndr::span_fill16(color, count);
}

void probe_span_textured16(uint16_t* dest, const uint16_t* texture, uint32_t du, uint32_t dv,
                           uint32_t v_mask, uint32_t u, uint32_t v, int32_t count) {
  zrndr::SpanState& s = zrndr::span_state();
  s.dest = reinterpret_cast<uint8_t*>(dest);
  s.texture = texture;
  s.du = du;
  s.dv = dv;
  s.v_mask = v_mask;
  zrndr::span_textured16(u, v, count);
}

}  // extern "C"

#include "gamez/zutil/ztimer.h"
#include "platform/platform.h"

namespace {
struct ProbeClock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now; }
};
ProbeClock g_probe_clock;
}  // namespace

extern "C" {

// Runs ztimer::update() for each tick value; writes dt and raw_dt per frame.
void probe_ztimer(const uint32_t* ticks, int frames, float scale_on_frame, int scale_frame,
                  float* dt_out, float* raw_out) {
  ztimer::state() = ztimer::State{};
  ztimer::set_clock(&g_probe_clock);
  for (int i = 0; i < frames; ++i) {
    if (i == scale_frame) ztimer::state().scale = scale_on_frame;
    g_probe_clock.now = ticks[i];
    ztimer::update();
    dt_out[i] = ztimer::state().dt;
    raw_out[i] = ztimer::state().raw_dt;
  }
  ztimer::set_clock(nullptr);
}

}  // extern "C"

#include <cstdio>
#include <cstring>
#include <string>

#include "gamez/zreader/zreader.h"

namespace {
zrdr::Node g_probe_root;
bool g_probe_loaded = false;

void dump(const zrdr::Node& n, std::string& out) {
  char buf[32];
  switch (n.type) {
    case zrdr::Int: std::snprintf(buf, sizeof buf, "I%d ", n.i); out += buf; break;
    case zrdr::Float: {
      uint32_t bits;
      std::memcpy(&bits, &n.f, 4);
      std::snprintf(buf, sizeof buf, "F%08x ", bits);
      out += buf;
      break;
    }
    case zrdr::String:
      std::snprintf(buf, sizeof buf, "S%zu:", std::strlen(n.s));
      out += buf;
      out += n.s;
      out += ' ';
      break;
    case zrdr::List:
      std::snprintf(buf, sizeof buf, "L%d[", n.list[0].i);
      out += buf;
      for (int32_t i = 1; i < n.list[0].i; ++i) dump(n.list[i], out);
      out += "] ";
      break;
    default: out += "? ";
  }
}

// The index path from the root to `target`, e.g. "3/1/2", or "" if not inside.
bool path_to(zrdr::Node* n, const zrdr::Node* target, std::string& out) {
  if (n->type != zrdr::List) return false;
  for (int32_t i = 1; i <= n->list[0].i; ++i) {  // <= : one past the end is a valid answer
    if (&n->list[i] == target) { out = std::to_string(i); return true; }
    if (i < n->list[0].i) {
      std::string sub;
      if (path_to(&n->list[i], target, sub)) { out = std::to_string(i) + "/" + sub; return true; }
    }
  }
  return false;
}

int copy_out(const std::string& s, char* buf, int size) {
  int n = static_cast<int>(s.size());
  if (n >= size) return -1;
  std::memcpy(buf, s.data(), s.size() + 1);
  return n;
}
}  // namespace

extern "C" {

// Reads one node tree from `path` at `offset` with zrdr::read_node; returns bytes read.
int probe_zrdr_load(const char* path, long offset) {
  if (g_probe_loaded) zrdr::free_contents(&g_probe_root);
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return -1;
  std::fseek(f, offset, SEEK_SET);
  int n = static_cast<int>(zrdr::read_node(f, &g_probe_root));
  std::fclose(f);
  g_probe_loaded = true;
  return n;
}

int probe_zrdr_dump(char* buf, int size) {
  std::string s;
  dump(g_probe_root, s);
  return copy_out(s, buf, size);
}

// zrdr::find, get_float and get_int on the loaded tree. Writes the hit's index path.
int probe_zrdr_find(const char* key, char* buf, int size, int* got_float, float* f, int* got_int, int32_t* i) {
  *got_float = zrdr::get_float(&g_probe_root, key, f);
  *got_int = zrdr::get_int(&g_probe_root, key, i);
  std::string p;
  if (zrdr::Node* hit = zrdr::find(&g_probe_root, key)) path_to(&g_probe_root, hit, p);
  return copy_out(p, buf, size);
}

// The list and boolean getters by key, each into outputs prefilled with `fill`
// so a slot left alone shows. ok[] gets the six results in header order.
void probe_zrdr_lists(const char* key, uint32_t fill, float* v3, float* v2, int32_t* iv3,
                      int32_t* iv2, int32_t* bi, uint8_t* b, int* ok) {
  for (int k = 0; k < 3; ++k) std::memcpy(&v3[k], &fill, 4), std::memcpy(&iv3[k], &fill, 4);
  for (int k = 0; k < 2; ++k) std::memcpy(&v2[k], &fill, 4), std::memcpy(&iv2[k], &fill, 4);
  std::memcpy(bi, &fill, 4);
  *b = static_cast<uint8_t>(fill);
  ok[0] = zrdr::get_vec3(&g_probe_root, key, v3);
  ok[1] = zrdr::get_vec2(&g_probe_root, key, v2);
  ok[2] = zrdr::get_ivec3(&g_probe_root, key, iv3);
  ok[3] = zrdr::get_ivec2(&g_probe_root, key, iv2);
  ok[4] = zrdr::get_bool_int(&g_probe_root, key, bi);
  bool bb = *b != 0;
  ok[5] = zrdr::get_bool(&g_probe_root, key, &bb);
  if (ok[5]) *b = bb;
}

}  // extern "C"

#include <vector>

#include "crt/msvc_rand.h"
#include "mech3/mech.h"

namespace {
mech3::Mech g_mech;
mech3::Leg g_legs[2];
mech3::Foot g_feet[2];
struct {
  float controls = 0;
  int mission = 0, option = 1, screen = 0, ammo = 0;
  uintptr_t next_sound = 1;
} g_script;
std::vector<int32_t> g_log;

void log3(int32_t a, int32_t b, int32_t c) { g_log.insert(g_log.end(), {a, b, c}); }
int32_t fbits(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
float bitsf(int32_t i) {
  float f;
  std::memcpy(&f, &i, 4);
  return f;
}
int32_t snd(mech3::Sound s) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(s)); }
mech3::Sound snd(int32_t i) { return reinterpret_cast<mech3::Sound>(static_cast<uintptr_t>(static_cast<uint32_t>(i))); }
}  // namespace

extern "C" {

// Field order shared with tests/differential/test_heat.py (FIELDS).
void probe_heat_io(int32_t* v, int write) {
  mech3::Mech& m = g_mech;
  mech3::World& w = mech3::world();
  struct F { float* f = nullptr; int32_t* i = nullptr; bool* b = nullptr; };
  int32_t mission = g_script.mission, option = g_script.option, screen = g_script.screen,
          ammo = g_script.ammo, beep = snd(m.heat_beep), alarm = snd(m.heat_alarm),
          warn = snd(m.shutdown_warning), voice = snd(m.shutdown_voice);
  int32_t player = w.player == &m;
  const F fields[] = {
      {&m.height}, {&m.heat}, {&m.heat_added}, {&m.heat_dissipation}, {&m.masc_time},
      {&m.shutdown_time}, {&m.beep_time}, {&m.voice_time}, {&g_feet[0].water_depth},
      {&g_feet[1].water_depth}, {&w.time}, {&w.dt}, {&g_script.controls},
      {nullptr, nullptr, &m.power_down_pending}, {nullptr, nullptr, &m.heat_shutdown},
      {nullptr, nullptr, &m.powered_down}, {nullptr, nullptr, &m.heat_death_disabled},
      {nullptr, nullptr, &m.heat_sinks.doubles}, {nullptr, nullptr, &g_feet[0].in_water},
      {nullptr, nullptr, &g_feet[1].in_water}, {nullptr, nullptr, &w.full_heat_all_mechs},
      {nullptr, nullptr, &w.no_heat_haze},
      {nullptr, &m.jump_jets_firing}, {nullptr, &m.heat_sinks.count},
      {nullptr, reinterpret_cast<int32_t*>(&m.heat_state)}, {nullptr, &g_feet[0].soil},
      {nullptr, &g_feet[1].soil}, {nullptr, &beep}, {nullptr, &alarm}, {nullptr, &warn},
      {nullptr, &voice}, {nullptr, &player}, {nullptr, &mission}, {nullptr, &option},
      {nullptr, &screen}, {nullptr, &ammo},
  };
  int k = 0;
  for (const F& f : fields) {
    if (write) {
      if (f.f) *f.f = bitsf(v[k]);
      else if (f.i) *f.i = v[k];
      else *f.b = v[k] != 0;
    } else {
      v[k] = f.f ? fbits(*f.f) : f.i ? *f.i : (*f.b ? 1 : 0);
    }
    ++k;
  }
  if (write) {
    m.heat_beep = snd(beep), m.heat_alarm = snd(alarm);
    m.shutdown_warning = snd(warn), m.shutdown_voice = snd(voice);
    w.player = player ? &m : nullptr;
    g_script.mission = mission, g_script.option = option;
    g_script.screen = screen, g_script.ammo = ammo;
  }
}

void probe_heat_reset(uint32_t seed) {
  g_mech = mech3::Mech{};
  for (int i = 0; i < 2; ++i) g_legs[i].foot = &g_feet[i], g_feet[i] = {}, g_mech.legs[i] = &g_legs[i];
  g_script = {};
  crt::msvc_srand(seed);
  mech3::HeatCalls& c = mech3::heat_calls();
  c.heat_option = [] { return g_script.option != 0; };
  c.mission_mode = [] { return g_script.mission; };
  c.controls_value = [](mech3::Mech&) { return g_script.controls; };
  c.screen = [](mech3::Mech&) -> void* { return g_script.screen ? &g_script : nullptr; };
  c.heat_haze = [](void*, float a, int k, float dt) { log3(1, fbits(a), k), log3(1, fbits(dt), 0); };
  c.play_sound = [](mech3::Mech&, int id) {
    log3(2, id, static_cast<int32_t>(g_script.next_sound));
    return snd(static_cast<int32_t>(g_script.next_sound++));
  };
  c.stop_sound = [](mech3::Sound s) { log3(3, snd(s), 0); };
  c.play_mech_sound = [](int i) { log3(4, i, 0); };
  c.power = [](mech3::Mech&, int on) { log3(5, on, 0); };
  c.torso_hit = [](mech3::Mech&, int explode) { log3(6, explode, 1); };
  c.ammo_explosion = [](mech3::Mech&) { log3(7, g_script.ammo, 0); return g_script.ammo != 0; };
  c.hud_message = [](float s, int msg) { log3(8, fbits(s), msg); };
}

int probe_heat_step(int32_t* log, int cap) {
  g_log.clear();
  mech3::update_heat(g_mech, mech3::world().dt);
  const int n = static_cast<int>(g_log.size());
  for (int i = 0; i < n && i < cap; ++i) log[i] = g_log[i];
  return n;
}

}  // extern "C"

#include "gamez/zimage/zimg_library.h"

namespace {
zimg::Library* g_tex_lib = nullptr;
std::string g_tex_dir;
}  // namespace

extern "C" {

// Opens <dir>/<file> as a texture library (the name already resolved).
int probe_tex_open(const char* dir, const char* file, int display_format) {
  g_tex_dir = dir;
  zimg::state() = zimg::State{};
  zimg::state().dir = g_tex_dir.c_str();
  zimg::state().display_format = display_format;
  g_tex_lib = zimg::new_library(file, 0, false);
  g_tex_lib->needs_resolve = false;
  return zimg::open(*g_tex_lib) ? g_tex_lib->header.texture_count : -1;
}

// Loads one texture and serialises it: the 0x38-byte fields that matter, then
// pixels, alpha and palette. Returns the length, or -1.
int probe_tex_load(const char* name, uint8_t* out, int cap) {
  zimg::Texture* t = zimg::load(*g_tex_lib, name);
  if (!t) return -1;
  std::vector<uint8_t> b;
  auto put = [&](const void* p, size_t n) {
    const auto* c = static_cast<const uint8_t*>(p);
    b.insert(b.end(), c, c + n);
  };
  put(&t->pixel_count, 4), put(&t->width, 2), put(&t->height, 2), put(&t->unk_08, 1);
  put(&t->flags, 1), put(&t->stretch, 2), put(&t->palette_count, 2), put(&t->width_again, 4);
  const int bytes = zimg::bytes_per_pixel(*t);
  if (t->pixels) put(t->pixels, t->palette_count ? t->pixel_count : bytes * t->pixel_count);
  if (t->alpha) put(t->alpha, t->pixel_count);
  if (t->palette) put(t->palette, (t->flags & zimg::kGlobalPalette) ? 0x200 : 2 * t->palette_count);
  zimg::free_texture(t);
  if (static_cast<int>(b.size()) > cap) return -1;
  std::memcpy(out, b.data(), b.size());
  return static_cast<int>(b.size());
}

}  // extern "C"
