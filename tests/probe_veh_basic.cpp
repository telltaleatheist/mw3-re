// Probes for test_veh_basic.py (docs/vehicles.md): the basic vehicle's entry
// 0x004ac320, constructor 0x004ac530, destructor 0x004ac650 and the HUD
// markers 0x004ac3d0 (mech3/veh_basic_object). Every call out goes to the
// shared callback; the vehicle takes the original's block address for the
// arguments that name it, and its image is laid out as the original's 0xfc
// bytes.
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

#include "gamez/zgame/unported.h"
#include "mech3/veh_basic_object.h"
#include "mech3/player_view.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zmodel/gmod_cull.h"
#include "probe_app.h"

namespace {
namespace m3 = mech3;

uint32_t lo(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
m3::Handle hd(uint32_t v) { return reinterpret_cast<m3::Handle>(static_cast<uintptr_t>(v)); }

uint32_t g_addr = 0;  // the original's block for the next vehicle
m3::BasicVehicle* g_v = nullptr;

struct Hooks : m3::VehBasicHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return probe_app::call(key, args);
  }
  uint32_t ref(const void* p) override { return p == g_v ? g_addr : lo(p); }
  void on_new(uint32_t size, const void* p) override {
    (void)size;
    g_v = static_cast<m3::BasicVehicle*>(const_cast<void*>(p));
  }
  // The stand-in keeps the node only (the test's 0x004d6c90 writes +0x04).
  void entity_set_node(m3::Vehicle& v, m3::Handle node) override {
    unported("0x004d6c90", {ref(&v), low(node), 0, 0, 0});
    v.node = node;
  }
  void weapons_free(void* w) override { unported("0x004b01d0", {low(w)}); }
  void cameras_once() override { unported("0x004ace00", {}); }  // the ctor's tests stub it
  void view_attach(m3::BasicVehicle& v, m3::CameraMode* mode) override;
  // 1d: the damage's, the controls and their glow block by the original's
  // addresses, reads through peek32.
  bool controls_destroyed(m3::VehicleControls&) override { return unported("ctl.vt+0x1c", {}) == 1; }
  void controls_died(m3::VehicleControls&) override { unported("ctl.ictl.vt+0xa8", {}); }
  void path_drop_temporary(m3::BasicVehicle& v) override { unported("0x00402510", {ref(&v)}); }  // the test stubs it
  int32_t glow_hit(m3::Block398&, m3::Handle w, float amount) override;
  void glow_end(m3::Block398&) override;
  uint32_t weapon_flags(m3::Handle w) override { return probe_app::call("peek32", {low(w) + 0x6c}); }
  const float* node_matrix(m3::Handle node) override {
    static float t[12];
    peekf(unported("0x004fc3f0", {low(node)}), t, 12);
    return t;
  }
  const float* shot_line() override {
    static float t[6];
    peekf(unported("0x00581e30", {}), t, 6);
    return t;
  }
  int32_t game_intensity() override { return static_cast<int32_t>(unported("0x0040cdf0", {})); }
  // The setup's zone (0x0049f710): the hits mirrored from the record the
  // test's 0x004edab0 also leaves at g_hits_addr, their materials' soil
  // copied; the node's flag 8 and zone byte by their calls.
  int32_t ground(float x, float y, float z, zcls::GroundHits* out) override;
  int32_t node_flag8(m3::Handle node) override { return static_cast<int32_t>(unported("0x004eb660", {low(node)})); }
  void node_set_flag8(m3::Handle node, int32_t on) override { unported("0x004eb620", {low(node), on}); }
  void node_set_zone(m3::Handle node, uint8_t zone) override { unported("0x004eb810", {low(node), zone}); }
  // The readout's: the controls' slots keyless (as the test's stubs), the
  // node's name and the camera's zones read back, the position by its call.
  int32_t controls_dynamics(m3::VehicleControls&) override { return static_cast<int32_t>(unported("ctl.vt+0x20", {})); }
  bool controls_on_path(m3::VehicleControls&) override { return unported("ctl.ictl.vt+0x70", {}) == 1; }
  int32_t controls_mode(m3::VehicleControls&) override { return static_cast<int32_t>(unported("ctl.vt+0x4", {})); }
  int32_t controls_goal(m3::VehicleControls&) override { return static_cast<int32_t>(unported("ctl.vt+0x8", {})); }
  const char* node_name(m3::Handle node) override { return text(low(node)); }
  const float* entity_position(m3::Vehicle& v) override {
    static float t[3];
    peekf(unported("0x005a85e0", {ref(&v)}), t, 3);
    return t;
  }
  const uint8_t* camera_zones() override {
    static uint8_t z[4];
    const uint32_t view = probe_app::call("peek32", {0x5f4c8c});
    const uint32_t cam = probe_app::call("peek32", {view + 4});
    const uint32_t rec = probe_app::call("peek32", {cam + 0x38}) + 0x1e4;
    for (uint32_t i = 0; i < 4; ++i) z[i] = static_cast<uint8_t>(probe_app::call("peek8", {rec + i}));
    return z;
  }
  static void peekf(uint32_t a, float* out, int n) {
    for (int i = 0; i < n; ++i) {
      const uint32_t b = probe_app::call("peek32", {a + 4u * static_cast<uint32_t>(i)});
      std::memcpy(out + i, &b, 4);
    }
  }
  // 1b: the emulator's strings read back through peek8.
  m3::Handle sound_find(const char* name) override { return hd(unported("0x0056f6f0", {name})); }
  const char* sound_name(m3::Handle sound) override { return text(unported("0x0056f740", {low(sound)})); }
  void voice_unlock(m3::Handle v) override { unported("0x0056f410", {low(v)}); }
  void voice_stop(m3::Handle v) override { unported("0x0056e9f0", {low(v)}); }
  m3::Handle anim_find(const char* name) override { return hd(unported("0x0050b450", {name})); }
  void report(int32_t level, const char* file, int32_t line, const char* message) override {
    unported("0x0049bfd0", {level, file, line, message});
  }
  zrdr::Node* reader_find(zrdr::Node* node, const char* key) override {
    return reinterpret_cast<zrdr::Node*>(hd(unported("0x005569e0", {low(node), key})));
  }
  int32_t reader_type(zrdr::Node* node) override {
    return static_cast<int32_t>(probe_app::call("peek32", {low(node)}));
  }
  const char* reader_string(zrdr::Node* node, const char* key) override {
    return text(unported("0x005569f0", {low(node), key}));
  }
  bool reader_string_value(zrdr::Node* node, const char* key, const char** out) override {
    const bool ok = unported("0x00557090", {low(node), key}) != 0;
    const uint32_t s = unported("@0x00557090.out", {});
    if (ok) *out = s ? text(s) : nullptr;
    return ok;
  }

  static const char* text(uint32_t a) {
    if (!a) return nullptr;
    static std::map<uint32_t, std::string> seen;
    std::string& t = seen[a];
    t.clear();
    for (uint32_t i = 0; i < 256; ++i) {
      const char c = static_cast<char>(probe_app::call("peek8", {a + i}));
      if (!c) break;
      t.push_back(c);
    }
    return t.c_str();
  }
};
Hooks g_hooks;

void put(uint8_t* b, uint32_t off, uint32_t v) { std::memcpy(b + off, &v, 4); }
void putf(uint8_t* b, uint32_t off, float v) { std::memcpy(b + off, &v, 4); }
void put3(uint8_t* b, uint32_t off, const m3::Vec3& v) { putf(b, off, v.x), putf(b, off + 4, v.y), putf(b, off + 8, v.z); }

}  // namespace

extern "C" {

void probe_vb_hooks(int on) { m3::set_veh_basic_hooks(on ? &g_hooks : nullptr); }

// 0x004ac320(ctl, type, name) as the original's block `addr` would be; 1 when
// a vehicle was made.
int probe_vb_create(uint32_t addr, uint32_t ctl, const char* type, const char* name) {
  g_addr = addr;
  g_v = nullptr;
  return m3::vehicle_create(hd(ctl), type, name) != nullptr;
}

// The fields the destructor reads, set as the test set the original's.
void probe_vb_set(uint32_t ammo, uint32_t net_collision, uint32_t anim, uint32_t engine) {
  g_v->ammo = hd(ammo);
  g_v->net_collision = reinterpret_cast<m3::NetCollision*>(hd(net_collision));
  g_v->motion.anim = hd(anim);
  g_v->engine.entry = reinterpret_cast<m3::EngineSoundEntry*>(hd(engine));
}

void probe_vb_destroy() {
  m3::vehicle_destroy(*g_v);
  g_v->ammo = nullptr, g_v->net_collision = nullptr;  // fakes: not the port's to free
  delete g_v;
  g_v = nullptr;
}

// The vehicle as the original's 0xfc bytes (the vtables and the tracked list's
// pointers, +0x88..+0x93, left as the test fills them).
void probe_vb_image(uint8_t* b) {
  const m3::BasicVehicle& v = *g_v;
  put(b, 4, lo(v.node)), b[8] = v.active, b[9] = v.x09, put(b, 0xc, lo(v.transform));
  put(b, 0x10, lo(v.target_node)), put(b, 0x14, lo(v.centroid));
  put3(b, 0x18, v.x18), put3(b, 0x24, v.forward), put3(b, 0x30, v.velocity), put3(b, 0x3c, v.vec_3c);
  for (int k = 0; k < 4; ++k) putf(b, 0x48 + 4 * k, v.orientation[k]);
  put3(b, 0x58, v.vec_58), b[0x64] = v.x64;
  putf(b, 0x68, v.height);
  put(b, 0x6c, lo(v.target)), put(b, 0x70, lo(v.ally_target)), put(b, 0x74, lo(v.enemy_target));
  put(b, 0x78, lo(v.saved_target)), put(b, 0x7c, lo(v.own_lance)), put(b, 0x80, lo(v.nearest_lance));
  b[0x84] = v.x84;
  put(b, 0x94, lo(v.own_player)), put(b, 0x98, lo(v.player_contact));
  putf(b, 0x9c, v.sensor_range), putf(b, 0xa0, v.range), putf(b, 0xa4, v.range2);
  b[0xa8] = v.xa8;
  for (int k = 0; k < 3; ++k) put(b, 0xac + 4 * k, 0);  // the contacts: none
  put(b, 0xb8, lo(v.touching));
  b[0xbc] = v.xbc;
  put(b, 0xc0, v.team), put(b, 0xc4, v.allies), put(b, 0xc8, v.enemies), put(b, 0xcc, v.shadow ? 1u : 0u);
  put(b, 0xd0, lo(v.net)), put(b, 0xd4, lo(v.ammo)), put(b, 0xd8, lo(v.net_collision));
  put(b, 0xe0, v.xe0), put(b, 0xe4, lo(v.controls));
  // +0xec, the engine sound: compared as an index (probe_vb_engine_index).
  put(b, 0xf0, lo(v.motion.def)), put(b, 0xf4, lo(v.motion.anim)), put(b, 0xf8, v.motion.x08);
}

// The engine sounds: 0x004ad930 (the entry's index, -1 for null), the
// entries (x00, sound, voice), a voice set, 0x004add10.
int probe_vb_sound_find(const char* name) {
  const m3::EngineSoundEntry* e = m3::engine_sound_find(name);
  return e ? static_cast<int>(e - m3::engine_sounds().entries.data()) : -1;
}
int probe_vb_sound_count() { return static_cast<int>(m3::engine_sounds().entries.size()); }
void probe_vb_sound_entry(int i, uint32_t* out) {
  const m3::EngineSoundEntry& e = m3::engine_sounds().entries[static_cast<size_t>(i)];
  out[0] = e.x00, out[1] = lo(e.sound), out[2] = lo(e.voice);
}
void probe_vb_sound_voice(int i, uint32_t v) { m3::engine_sounds().entries[static_cast<size_t>(i)].voice = hd(v); }
void probe_vb_sounds_clear() { m3::engine_sounds_clear(); }
void probe_vb_sounds_reset() { m3::engine_sounds().entries.clear(); }  // silent, between runs
int probe_vb_engine_index() {
  const m3::EngineSoundEntry* e = g_v->engine.entry;
  return e ? static_cast<int>(e - m3::engine_sounds().entries.data()) : -1;
}

// 0x0049ec00 on the vehicle made last.
void probe_vb_setup(uint32_t def, const char* name, int32_t kind) {
  m3::vehicle_setup(*g_v, reinterpret_cast<zrdr::Node*>(hd(def)), name, kind);
}

void probe_vb_markers(uint32_t* out) {
  m3::vehicle_markers_create();
  const m3::VehicleMarkers& m = m3::vehicle_markers();
  out[0] = lo(m.ally), out[1] = lo(m.enemy), out[2] = lo(m.other);
}

// The cameras: 0x004ace00 (twice), each mode's +0x04..+0x43 as the
// original's 0x44 bytes, the lookups by id (index in the list, -1 none).
void probe_vb_cameras_reset() { m3::vehicle_cameras_reset(); }
void probe_vb_cameras_make() { m3::vehicle_cameras_make(); }
int probe_vb_camera_count() { return static_cast<int>(m3::vehicle_cameras().modes.size()); }
void probe_vb_camera_image(int i, uint32_t* out) {
  const m3::CameraMode* c = m3::vehicle_cameras().modes[static_cast<size_t>(i)];
  std::memcpy(out, &c->id, 4);
  std::memcpy(out + 1, c->delta_rot, 16);
  std::memcpy(out + 5, c->desired_pos, 12);
  const float f[7] = {c->dist_vary_factor, c->dist, c->dist_factor, c->dist_vary,
                      c->dist_catch_up, c->turn_rate, c->look_rate};
  std::memcpy(out + 8, f, 28);
  std::memcpy(out + 15, &c->look_, 4);
}
static int probe_vb_camera_index(const m3::CameraMode* c) {
  const auto& v = m3::vehicle_cameras().modes;
  for (size_t i = 0; i < v.size(); ++i)
    if (v[i] == c) return static_cast<int>(i);
  return -1;
}
int probe_vb_camera_find(int32_t id) { return probe_vb_camera_index(m3::vehicle_camera(id)); }
int probe_vb_camera_current() { return probe_vb_camera_index(m3::vehicle_camera_current()); }
// 0x004acd90: which vehicle and which mode the view was given.
uint32_t g_attach[2];
void probe_vb_view_attach(uint32_t v, uint32_t* out) {
  alignas(16) static unsigned char stand_in[sizeof(m3::BasicVehicle)];
  g_v = reinterpret_cast<m3::BasicVehicle*>(stand_in);
  g_addr = v;
  m3::vehicle_view_attach(*g_v);
  out[0] = g_attach[0], out[1] = g_attach[1];
}

}  // extern "C"

uint32_t g_ctl = 0;  // the original's controls block
uint32_t g_hits_addr = 0;
zmod::Material g_mats[32];
int32_t Hooks::ground(float x, float y, float z, zcls::GroundHits* out) {
  const int32_t r = static_cast<int32_t>(unported("0x004edab0", {zgame::bits(x), zgame::bits(y), zgame::bits(z)}));
  auto p32 = [](uint32_t a) { return probe_app::call("peek32", {a}); };
  out->count = static_cast<uint8_t>(p32(g_hits_addr));
  for (int i = 0; i < out->count && i < 32; ++i) {
    const uint32_t h = g_hits_addr + 4 + 0x28u * static_cast<uint32_t>(i);
    zcls::GroundHit& g = out->hit[i];
    g.x = zgame::from_bits(p32(h + 0xc)), g.height = zgame::from_bits(p32(h + 0x10)), g.z = zgame::from_bits(p32(h + 0x14));
    g.zone[0] = p32(h + 0x18), g.zone[1] = p32(h + 0x1c);
    const uint32_t m = p32(h + 0x20);
    g_mats[i].soil = m ? p32(m + 0x20) : 0;
    g.material = m ? &g_mats[i] : nullptr;
  }
  return r;
}
m3::BasicVehicle* g_dv = nullptr;
int32_t Hooks::glow_hit(m3::Block398&, m3::Handle w, float amount) {
  return static_cast<int32_t>(unported("0x00586a30", {low(w), g_ctl + 0x398, zgame::bits(amount)}));
}
void Hooks::glow_end(m3::Block398&) { unported("0x00586af0", {g_ctl + 0x398}); }

void Hooks::view_attach(m3::BasicVehicle& v, m3::CameraMode* mode) {
  g_attach[0] = ref(&v);
  g_attach[1] = static_cast<uint32_t>(probe_vb_camera_index(mode));
}

// 1d: a vehicle for the damage (vals: node, active, ammo, engine set,
// frozen, state, health_max, health, glow x08, beam_on, beam, inv_mass,
// destroy_anim, angles x, z, +0x70 x, z), then one call (0 damage, 1 the
// hit callback, 2 the tint on, 3 off), then the same fields back.
extern "C" {
void probe_vb_dmg_setup(uint32_t v, uint32_t ctl, const uint32_t* a) {
  static m3::EngineSoundEntry entry;
  delete g_dv;
  g_dv = new m3::BasicVehicle;
  auto* c = new m3::VehicleControls;  // leaked: the probe's
  g_v = g_dv, g_addr = v, g_ctl = ctl;
  g_dv->controls = c;
  g_dv->node = hd(a[0]);
  g_dv->active = a[1] != 0;
  g_dv->ammo = hd(a[2]);
  g_dv->engine.entry = a[3] ? &entry : nullptr;
  c->frozen = a[4];
  c->state = static_cast<int32_t>(a[5]);
  c->health_max = zgame::from_bits(a[6]);
  c->health = zgame::from_bits(a[7]);
  c->x398.x08 = zgame::from_bits(a[8]);
  c->beam_on = a[9];
  c->first = &c->slots[0];
  c->slots[0].beam = hd(a[10]);
  c->motion.inv_mass = zgame::from_bits(a[11]);
  c->motion.destroy_anim = hd(a[12]);
  c->angles.x = zgame::from_bits(a[13]), c->angles.z = zgame::from_bits(a[14]);
  c->x70.x = zgame::from_bits(a[15]), c->x70.z = zgame::from_bits(a[16]);
}
// The readout's extra state: motion mode, +0x20, angles.y, zone count and ids.
void probe_vb_dbg_setup(int32_t mode, uint32_t x20, uint32_t yaw, uint32_t zones) {
  m3::VehicleControls& c = *g_dv->controls;
  c.motion.mode = static_cast<m3::VehicleMotion::Mode>(mode);
  c.x20 = x20;
  c.angles.y = zgame::from_bits(yaw);
  g_dv->x64 = static_cast<uint8_t>(zones);
  for (int i = 0; i < 3; ++i) g_dv->zones[i] = static_cast<uint8_t>(zones >> (8 * (i + 1)));
}
void probe_vb_dbg(int null) { m3::vehicle_debug_lines(null ? nullptr : g_dv); }
int32_t probe_vb_dmg_call(int mode, uint32_t weapon, uint32_t x, uint32_t amount) {
  switch (mode) {
    case 0: return m3::vehicle_damage(*g_dv, hd(weapon), x, zgame::from_bits(amount));
    case 1: m3::vehicle_hit(*g_dv, hd(weapon), x, zgame::from_bits(amount)); return 0;
    default: m3::vehicle_view_tint(*g_dv, static_cast<uint8_t>(x)); return 0;
  }
}
void probe_vb_dmg_image(uint32_t* a) {
  const m3::VehicleControls& c = *g_dv->controls;
  auto b = [](float f) { return zgame::bits(f); };
  a[0] = lo(g_dv->node), a[1] = g_dv->active, a[2] = lo(g_dv->ammo), a[3] = g_dv->engine.entry != nullptr;
  a[4] = c.frozen, a[5] = static_cast<uint32_t>(c.state), a[6] = b(c.health_max), a[7] = b(c.health);
  a[8] = b(c.x398.x08), a[9] = c.beam_on, a[10] = lo(c.slots[0].beam), a[11] = b(c.motion.inv_mass);
  a[12] = lo(c.motion.destroy_anim), a[13] = b(c.angles.x), a[14] = b(c.angles.z), a[15] = b(c.x70.x);
  a[16] = b(c.x70.z);
}
}  // extern "C"

// The setup's points and zone (0x0049f660): a vehicle at `v` with base
// controls at `ctl`: `count` points, the motion's collision points.
extern "C" {
void probe_vb_pts_setup(uint32_t v, uint32_t ctl, uint32_t node, int32_t count, const float* coll, uint32_t hits) {
  delete g_dv;
  g_dv = new m3::BasicVehicle;
  auto* c = new m3::VehicleControls;  // leaked: the probe's
  g_v = g_dv, g_addr = v, g_ctl = ctl, g_hits_addr = hits;
  g_dv->controls = c;
  g_dv->node = hd(node);
  c->point_count = count;
  for (int i = 0; i < 12; ++i) c->motion.collision[i] = {coll[3 * i], coll[3 * i + 1], coll[3 * i + 2]};
}
void probe_vb_pts_run() { m3::vehicle_points_place(*g_dv); }
void probe_vb_pts_image(float* pts, uint8_t* zones) {
  const m3::VehicleControls& c = *g_dv->controls;
  for (int i = 0; i < 15; ++i) pts[3 * i] = c.points[i].x, pts[3 * i + 1] = c.points[i].y, pts[3 * i + 2] = c.points[i].z;
  zones[0] = g_dv->x64, zones[1] = g_dv->zones[0], zones[2] = g_dv->zones[1], zones[3] = g_dv->zones[2];
}
float probe_vb_gravity_x1c() { return g_dv->controls->x1c; }
}  // extern "C"

extern "C" void probe_zones_mode(int32_t on) { zmod::zones().mode = on; }  // [0x005ea1d0]
