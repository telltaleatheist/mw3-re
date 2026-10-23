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
#include "gamez/zreader/zreader.h"
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

}  // extern "C"
