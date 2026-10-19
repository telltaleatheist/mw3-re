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
      {nullptr, nullptr, &m.powered_down}, {nullptr, nullptr, &m.invincible},
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
  c.game_type = [] { return g_script.mission; };
  c.throttle = [](mech3::Mech&) { return g_script.controls; };
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

extern "C" {

// Power on/off: fields in, call log out. f: [vec x,y,z]; i: pending, heat_shutdown,
// powered_down, anim_state, no_power_down, hud_saved, loops[3], up, down,
// sound_flags, player, screen, screen_byte. op 0/1 = power(op), 2 = finish.
int probe_power(int op, float* f, int32_t* i, int32_t* log, int cap) {
  mech3::Mech& m = g_mech;
  m = mech3::Mech{};
  m.vec_3c = {f[0], f[1], f[2]};
  m.power_down_pending = i[0], m.heat_shutdown = i[1], m.powered_down = i[2];
  m.anim_state = i[3], m.no_power_down = i[4], m.hud_saved = static_cast<uint8_t>(i[5]);
  for (int k = 0; k < 3; ++k) m.loops[k] = snd(i[6 + k]);
  m.power_up_sound = snd(i[9]), m.power_down_sound = snd(i[10]);
  m.sound_flags = static_cast<uint32_t>(i[11]);
  mech3::world().player = i[12] ? &m : nullptr;
  g_script.screen = i[13];
  static int32_t screen_byte;
  screen_byte = i[14];
  g_script.next_sound = 100;
  g_log.clear();
  mech3::HeatCalls& c = mech3::heat_calls();
  c.stop_sound = [](mech3::Sound s) { log3(3, snd(s), 0); };
  c.screen = [](mech3::Mech&) -> void* { return g_script.screen ? &g_script : nullptr; };
  c.stop_controls = [](mech3::Mech&, int a) { log3(9, a, 0); };
  c.screen_power_down = [](void*) { log3(10, screen_byte, 0); return static_cast<uint8_t>(screen_byte); };
  c.screen_power_up = [](void*, uint8_t b) { log3(11, b, 0); };
  c.play_sound_at = [](mech3::Mech&, int id) {
    log3(2, id, static_cast<int32_t>(g_script.next_sound));
    return snd(static_cast<int32_t>(g_script.next_sound++));
  };
  if (op == 2) m.finish_power_down();
  else m.power(op);
  f[0] = m.vec_3c.x, f[1] = m.vec_3c.y, f[2] = m.vec_3c.z;
  i[0] = m.power_down_pending, i[1] = m.heat_shutdown, i[2] = m.powered_down;
  i[3] = m.anim_state, i[4] = m.no_power_down, i[5] = m.hud_saved;
  for (int k = 0; k < 3; ++k) i[6 + k] = snd(m.loops[k]);
  i[9] = snd(m.power_up_sound), i[10] = snd(m.power_down_sound);
  const int n = static_cast<int>(g_log.size());
  for (int k = 0; k < n && k < cap; ++k) log[k] = g_log[k];
  return n;
}

}  // extern "C"

#include "gamez/zsiscript/si_script.h"

extern "C" {

// Reads one motion from <path> at <offset> and serialises the script: header
// fields, then each part's name, translations, rotations and scales.
int probe_motion(const char* path, long offset, uint8_t* out, int cap) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return -1;
  std::fseek(f, offset, SEEK_SET);
  zsis::Script s;
  const bool ok = s.read(f);
  std::fclose(f);
  std::vector<uint8_t> b;
  auto put = [&](const void* p, size_t n) {
    const auto* c = static_cast<const uint8_t*>(p);
    b.insert(b.end(), c, c + n);
  };
  const uint8_t okb = ok, act = s.active;
  put(&okb, 1), put(&s.unk_04, 4), put(&s.unk_08, 4), put(&s.unk_0c, 4), put(&act, 1);
  put(&s.loop_time, 4), put(&s.inv_loop_time, 4), put(&s.frame_time, 4), put(&s.inv_frame_time, 4);
  put(&s.frame_count, 4), put(&s.part_count, 4), put(&s.time, 4);
  const uint32_t n = s.frame_count + 1;
  for (uint32_t i = 0; ok && i < s.part_count; ++i) {
    const zsis::Part* p = s.parts[i];
    put(p->name, std::strlen(p->name) + 1);
    put(p->translations, n * 12);
    put(p->rotations, n * 16);
    if (p->scales) put(p->scales, n * 12);
  }
  if (static_cast<int>(b.size()) > cap) return -1;
  std::memcpy(out, b.data(), b.size());
  return static_cast<int>(b.size());
}

}  // extern "C"

#include "gamez/zclass/cls_mechlib.h"

namespace {
void put_bytes(std::vector<uint8_t>& b, const void* p, size_t n) {
  const auto* c = static_cast<const uint8_t*>(p);
  b.insert(b.end(), c, c + n);
}
int finish(const std::vector<uint8_t>& b, uint8_t* out, int cap) {
  if (static_cast<int>(b.size()) > cap) return -1;
  std::memcpy(out, b.data(), b.size());
  return static_cast<int>(b.size());
}
}  // namespace

extern "C" {

// One model at <offset>: the header (pointer fields as present/absent), the
// arrays, then each polygon's fields and arrays. Same layout as test_mechlib.py.
int probe_mechlib_model(const char* path, long offset, uint8_t* out, int cap) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return -1;
  std::fseek(f, offset, SEEK_SET);
  zcls::Model* m = zcls::read_model(f);
  const long end = std::ftell(f);
  std::fclose(f);
  std::vector<uint8_t> b;
  const int32_t used = static_cast<int32_t>(end - offset);
  put_bytes(b, &used, 4);
  put_bytes(b, m->header, 0x34);
  put_bytes(b, m->header + 0x48, 0x5c - 0x48);
  if (m->vertices) put_bytes(b, m->vertices, m->vertex_count * 12);
  if (m->normals) put_bytes(b, m->normals, m->normal_count * 12);
  if (m->morphs) put_bytes(b, m->morphs, m->morph_count * 12);
  for (uint32_t i = 0; i < m->polygon_count; ++i) {
    const zcls::Polygon& p = m->polygons[i];
    const uint32_t k = p.flags & 0xff;
    put_bytes(b, &p.flags, 4), put_bytes(b, &p.priority, 4);
    put_bytes(b, &p.material, 4), put_bytes(b, &p.zone_set, 4);
    const uint8_t has[4] = {p.vertex_indices != nullptr, p.normal_indices != nullptr,
                            p.uvs != nullptr, p.colors != nullptr};
    put_bytes(b, has, 4);
    if (p.vertex_indices) put_bytes(b, p.vertex_indices, k * 4);
    if (p.normal_indices) put_bytes(b, p.normal_indices, k * 4);
    if (p.uvs) put_bytes(b, p.uvs, k * 8);
    if (p.colors) put_bytes(b, p.colors, k * 12);
  }
  zcls::free_model(m);
  return finish(b, out, cap);
}

// One material at <offset>: the 0x28 bytes, then the texture name.
int probe_mechlib_material(const char* path, long offset, uint8_t* out, int cap) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return -1;
  std::fseek(f, offset, SEEK_SET);
  zcls::Material m{};
  zcls::read_material(f, m);
  const int32_t used = static_cast<int32_t>(std::ftell(f) - offset);
  std::fclose(f);
  std::vector<uint8_t> b;
  put_bytes(b, &used, 4);
  put_bytes(b, m.raw, 0x28);
  if (m.texture) put_bytes(b, m.texture, std::strlen(m.texture) + 1);
  std::free(m.raw);
  std::free(m.texture);
  return finish(b, out, cap);
}

}  // extern "C"

#include "gamez/zimage/zimg_texture.h"

extern "C" {

// Reads a world's texture directory: count at offset, group set first. Output:
// result, table count, then per entry name[20], state, group, mip index.
int probe_texture_directory(const char* path, long offset, int count, int group, uint8_t* out, int cap) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return -1;
  std::fseek(f, offset, SEEK_SET);
  zimg::TextureTable& t = zimg::textures();
  t.count = -7;
  t.group = group;
  const int32_t r = zimg::read_texture_directory(count, f);
  std::fclose(f);
  std::vector<uint8_t> b;
  put_bytes(b, &r, 4), put_bytes(b, &t.count, 4);
  for (int i = 0; i < (r > 0 ? r : 0); ++i) {
    const zimg::TextureEntry& e = t.entries[i];
    const int32_t mip = e.mip ? static_cast<int32_t>(e.mip - t.entries) : -1;
    put_bytes(b, e.name, 20), put_bytes(b, &e.state, 4), put_bytes(b, &e.group, 4), put_bytes(b, &mip, 4);
  }
  return finish(b, out, cap);
}

}  // extern "C"

#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zvideo/zvid_format.h"

extern "C" {

// Reads a world's materials at offset with the display set to r/g/b bits.
// Output: result, count, free head, used head, then every slot (texture as an
// index), then each used cycled material's cycle, in used-list order.
int probe_materials(const char* path, long offset, int rb, int gb, int bb, uint8_t* out, int cap) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return -1;
  std::fseek(f, offset, SEEK_SET);
  zvid::set_pixel_format(rb, gb, bb);
  zmod::MaterialTable& t = zmod::materials();
  t = zmod::MaterialTable{};
  const int32_t r = zmod::read_materials(f);
  std::fclose(f);
  std::vector<uint8_t> b;
  put_bytes(b, &r, 4), put_bytes(b, &t.count, 4), put_bytes(b, &t.free_head, 4), put_bytes(b, &t.used_head, 4);
  std::vector<bool> used(t.slots.size());
  for (int i = t.used_head; i != -1; i = t.slots[i].next) used[i] = true;
  for (int i = 0; i < r; ++i) {
    const zmod::Material& m = t.slots[i];
    put_bytes(b, &m.alpha, 1), put_bytes(b, &m.flags, 1), put_bytes(b, &m.rgb, 2);
    put_bytes(b, &m.r, 4), put_bytes(b, &m.g, 4), put_bytes(b, &m.b, 4);
    const int32_t tex = (used[i] && (m.flags & 1))
                            ? (m.texture ? static_cast<int32_t>(m.texture - zimg::textures().entries) : -1)
                            : static_cast<int32_t>(m.texture_index);
    put_bytes(b, &tex, 4), put_bytes(b, m.unk_14, 12), put_bytes(b, &m.soil, 4);
    put_bytes(b, &m.prev, 2), put_bytes(b, &m.next, 2);
  }
  for (int i = t.used_head; i != -1; i = t.slots[i].next) {
    const zmod::Material& m = t.slots[i];
    if (!(m.flags & 4)) continue;
    const zmod::Cycle* c = m.cycle;
    put_bytes(b, &c->looping, 4), put_bytes(b, &c->frame, 4), put_bytes(b, &c->index, 4);
    put_bytes(b, &c->speed, 4), put_bytes(b, &c->texture_count, 4), put_bytes(b, &c->unk_14, 4);
    for (uint32_t k = 0; k < c->texture_count; ++k) {
      const int32_t idx = c->textures[k] ? static_cast<int32_t>(c->textures[k] - zimg::textures().entries) : -1;
      put_bytes(b, &idx, 4);
    }
  }
  return finish(b, out, cap);
}

}  // extern "C"

extern "C" {

int probe_pack_rgb(int rb, int gb, int bb, float r, float g, float b) {
  zvid::set_pixel_format(rb, gb, bb);
  const float rgb[3] = {r, g, b};
  return zvid::pack_rgb(rgb);
}

}  // extern "C"

#include "gamez/zmodel/gmod_const.h"

extern "C" {

// Reads materials at moff then models at off (display 5-6-5). Output: result,
// count, free head, then per slot the header (pointer fields skipped), link,
// arrays, lights and polygons; materials as indices.
int probe_models(const char* path, long moff, long off, uint8_t* out, int cap) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return -1;
  zvid::set_pixel_format(5, 6, 5);
  zmod::materials() = zmod::MaterialTable{};
  std::fseek(f, moff, SEEK_SET);
  zmod::read_materials(f);
  zmod::models() = zmod::ModelPool{};
  std::fseek(f, off, SEEK_SET);
  const int32_t r = zmod::read_models(f);
  std::fclose(f);
  const zmod::ModelPool& p = zmod::models();
  std::vector<uint8_t> b;
  put_bytes(b, &r, 4), put_bytes(b, &p.count, 4), put_bytes(b, &p.free_head, 4);
  for (int i = 0; i < r; ++i) {
    const zmod::Model& m = p.slots[i];
    put_bytes(b, m.header, 0x34), put_bytes(b, m.header + 0x48, 0x14), put_bytes(b, &m.link, 4);
    const auto n = [&](int o) { return static_cast<int32_t>(m.count(o)); };
    if (n(0x14) > 0) put_bytes(b, m.vertices, n(0x14) * 12);
    if (n(0x18) > 0) put_bytes(b, m.normals, n(0x18) * 12);
    if (n(0x1c) > 0) put_bytes(b, m.morphs, n(0x1c) * 12);
    for (int k = 0; k < n(0x20); ++k) {
      const zmod::Light& l = m.lights[k];
      put_bytes(b, l.raw, 0x28), put_bytes(b, &l.color, 2), put_bytes(b, l.raw + 0x30, 0x1c);
      int32_t vc;
      std::memcpy(&vc, l.raw + 0x0c, 4);
      if (vc > 0) put_bytes(b, l.vertices, vc * 12);
    }
    for (int k = 0; k < n(0x10); ++k) {
      const zmod::Polygon& q = m.polygons[k];
      const uint32_t c = q.flags & 0xff;
      const int32_t mat = q.material ? static_cast<int32_t>(q.material - zmod::materials().slots.data()) : -1;
      put_bytes(b, &q.flags, 4), put_bytes(b, &q.priority, 4), put_bytes(b, &mat, 4), put_bytes(b, &q.zone_set, 4);
      if (q.vertex_indices) put_bytes(b, q.vertex_indices, c * 4);
      if (q.normal_indices) put_bytes(b, q.normal_indices, c * 4);
      if (q.uvs) put_bytes(b, q.uvs, c * 8);
      if (q.colors) put_bytes(b, q.colors, c * 12);
    }
  }
  return finish(b, out, cap);
}

}  // extern "C"

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zrender/zrndr_fog.h"

namespace {
std::vector<int32_t> g_reports;

void record_report(int level, const char*, int line, const char*) {
  g_reports.push_back(level), g_reports.push_back(line);
}

int32_t node_index(const zcls::Node* n) {
  if (!n) return -1;
  const zcls::NodePool& p = zcls::nodes();
  for (size_t b = 0; b < p.blocks.size(); ++b) {
    const zcls::Node* base = p.blocks[b].get();
    if (n >= base && n < base + p.block_size) return static_cast<int32_t>(b * p.block_size + (n - base));
  }
  return -2;
}

void put_i32(std::vector<uint8_t>& b, int32_t v) { put_bytes(b, &v, 4); }

void put_nodes(std::vector<uint8_t>& b, zcls::Node* const* list, int32_t count) {
  put_i32(b, list != nullptr);
  if (list)
    for (int32_t i = 0; i < count; ++i) put_i32(b, node_index(list[i]));
}

// Bytes as read with the given 4-byte pointer fields zeroed.
void put_raw(std::vector<uint8_t>& b, const uint8_t* raw, size_t n, std::initializer_list<int> ptrs) {
  std::vector<uint8_t> copy(raw, raw + n);
  for (int off : ptrs) std::memset(copy.data() + off, 0, 4);
  put_bytes(b, copy.data(), n);
}

void put_class_data(std::vector<uint8_t>& b, zcls::Node* n) {
  using namespace zcls;
  switch (n->cls()) {
    case 1: {
      auto* c = static_cast<Camera*>(n->data);
      put_raw(b, c->raw, sizeof c->raw, {0, 4, 8, 0xc});
      put_nodes(b, c->nodes, 4);
      break;
    }
    case 2: {
      auto* w = static_cast<World*>(n->data);
      put_raw(b, w->raw, sizeof w->raw, {0x90, 0xa4, 0xa8, 0xb0, 0xb4});
      const int32_t lights = w->get<int32_t>(0xa0), sounds = w->get<int32_t>(0xac);
      put_nodes(b, w->lights, lights);
      for (int32_t i = 0; w->lights && i < lights; ++i) put_i32(b, w->light_data[i] == w->lights[i]->data);
      put_nodes(b, w->sounds, sounds);
      for (int32_t i = 0; w->sounds && i < sounds; ++i) put_i32(b, w->sound_data[i] == w->sounds[i]->data);
      for (int32_t z = 0; z < w->get<int32_t>(0x8c); ++z)
        for (int32_t x = 0; x < w->get<int32_t>(0x88); ++x) {
          const Partition& p = w->rows[z][x];
          put_raw(b, p.raw, sizeof p.raw, {0x3c});
          put_nodes(b, p.nodes, p.get<int16_t>(0x3a));
        }
      break;
    }
    case 3: put_bytes(b, static_cast<Window*>(n->data)->raw, 0xf8); break;
    case 4: put_bytes(b, static_cast<Display*>(n->data)->raw, 0x1c); break;
    case 5: put_bytes(b, static_cast<Object3d*>(n->data)->raw, 0x90); break;
    case 6: put_bytes(b, static_cast<Lod*>(n->data)->raw, 0x50); break;
    case 9: {
      auto* l = static_cast<Light*>(n->data);
      put_raw(b, l->raw, sizeof l->raw, {0xcc});
      put_nodes(b, l->nodes, l->get<int32_t>(0xc8));
      break;
    }
    case 10: {
      auto* s = static_cast<Sound*>(n->data);
      put_raw(b, s->raw, sizeof s->raw, {0x90});
      put_nodes(b, s->nodes, s->get<int32_t>(0x8c));
      break;
    }
    default: break;
  }
}

void put_item(std::vector<uint8_t>& b, const zcls::ListItem* item) {
  const zcls::Lists& l = zcls::lists();
  int32_t chunk = -1, slot = -1;
  for (size_t c = 0; item && c < l.chunks.size(); ++c) {
    const zcls::ListItem* base = l.chunks[c].get();
    if (item >= base && item < base + l.chunk_size) chunk = static_cast<int32_t>(c), slot = static_cast<int32_t>(item - base);
  }
  put_i32(b, chunk), put_i32(b, slot);
}
}  // namespace

extern "C" {

// The node pool after reading one world's textures-to-nodes (test_gamez.py).
int probe_nodes(const char* path, long moff, long mdl_off, long node_off, int size, int free_head,
                uint8_t* out, int cap) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return -1;
  zvid::set_pixel_format(5, 6, 5);
  zmod::materials() = zmod::MaterialTable{};
  std::fseek(f, moff, SEEK_SET);
  zmod::read_materials(f);
  zmod::models() = zmod::ModelPool{};
  std::fseek(f, mdl_off, SEEK_SET);
  zmod::read_models(f);
  zcls::nodes() = zcls::NodePool{};
  zcls::nodes().free_head = free_head;
  zcls::lists() = zcls::Lists{};
  zmod::fog() = zmod::Fog{};
  zrndr::fog_color() = zrndr::FogColor{};
  zrndr::software_format() = zrndr::SoftwareFormat{0xf800, 0x07e0, 8, 3, 3};
  zvid::clear_color() = 0;
  g_reports.clear();
  zerr::set_sink(record_report);
  std::fseek(f, node_off, SEEK_SET);
  const int32_t r = zcls::read_nodes(size, f);
  std::fclose(f);
  zerr::set_sink(nullptr);

  const zcls::NodePool& p = zcls::nodes();
  std::vector<uint8_t> b;
  put_i32(b, r), put_i32(b, p.capacity), put_i32(b, p.live), put_i32(b, p.free_head);
  put_i32(b, static_cast<int32_t>(p.blocks.size()));
  for (int32_t i = 0; i < p.capacity; ++i) {
    zcls::Node* n = zcls::node_at(i);
    put_raw(b, n->raw, 0xd0, {0x38, 0x3c, 0x48, 0x58, 0x60});
    put_bytes(b, &n->link, 4);
    put_i32(b, n->model ? static_cast<int32_t>(n->model - zmod::models().slots.data()) : -1);
    put_i32(b, n->action != nullptr);
    if (i >= size) continue;
    put_nodes(b, n->parents, n->parent_count());
    put_nodes(b, n->children, n->child_count());
    if (n->data) put_class_data(b, n);
  }
  const zcls::Lists& l = zcls::lists();
  for (const zcls::List& list : l.list) {
    int32_t count = 0;
    for (zcls::ListItem* i = list.head; i; i = i->next) ++count;
    put_i32(b, count);
    for (zcls::ListItem* i = list.head; i; i = i->next) {
      put_i32(b, node_index(i->node)), put_item(b, i), put_i32(b, i->removable);
      put_item(b, i->prev);
    }
    put_item(b, list.tail), put_i32(b, list.needs_purge);
  }
  int32_t free_count = 0;
  for (zcls::ListItem* i = l.free; i; i = i->next) ++free_count;
  put_i32(b, free_count), put_item(b, l.free), put_i32(b, static_cast<int32_t>(l.chunks.size()));
  put_bytes(b, &zmod::fog(), sizeof(zmod::Fog));
  const zrndr::FogColor& fc = zrndr::fog_color();
  put_bytes(b, fc.rgb, 12), put_bytes(b, &fc.r, 4), put_bytes(b, &fc.g, 4), put_bytes(b, &fc.b, 4);
  put_bytes(b, &fc.packed, 2), put_bytes(b, &fc.doubled, 4), put_bytes(b, fc.ramp, 128);
  put_bytes(b, &zvid::clear_color(), 4);
  put_i32(b, static_cast<int32_t>(g_reports.size() / 2));
  for (int32_t v : g_reports) put_i32(b, v);
  return finish(b, out, cap);
}

}  // extern "C"
