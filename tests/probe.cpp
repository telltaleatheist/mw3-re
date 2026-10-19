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
