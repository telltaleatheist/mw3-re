// Probes for test_mech_data_load.py (docs/mech-build.md section 14): the mech
// types' data, mech3/mech_data.cpp's cache 0x00430d10, constructor 0x004303d0,
// loader 0x004308a0 and its steps, the refresh 0x004319a0 and the clear
// 0x004318e0. Every call out goes to the shared callback (a calltrace.Replay
// of the original's run); the definitions, trees and reader nodes are fake
// objects in the emulator's memory, read through peek. The port's data and
// soils objects get the addresses of the original's `new`s, so their images
// can be laid over the emulator's blocks.
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "mech3/mech.h"
#include "mech3/mech_data.h"
#include "mech3/mech_reader.h"
#include "probe_app.h"

namespace {
using probe_app::call;
using mech3::Handle;
namespace m3 = mech3;

uint32_t lo(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
Handle hd(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
uint32_t peek32(uint32_t a) { return call("peek32", {a}); }
uint8_t peek8(uint32_t a) { return static_cast<uint8_t>(call("peek8", {a})); }

std::map<const void*, uint32_t> g_addr;
std::map<uint32_t, const void*> g_obj;
std::map<const void*, int> g_kind;  // 0 data, 1 soils
std::map<uint32_t, m3::MechDfn*> g_dfns;
std::map<uint32_t, std::string> g_names;

uint32_t addr_of(const void* p) {
  if (!p) return 0;
  auto it = g_addr.find(p);
  return it != g_addr.end() ? it->second : lo(p);
}

const char* cstr(uint32_t a) {
  if (!a) return nullptr;
  auto it = g_names.find(a);
  if (it != g_names.end()) return it->second.c_str();
  std::string s;
  for (uint32_t i = 0; i < 256; ++i) {
    const char c = static_cast<char>(peek8(a + i));
    if (!c) break;
    s.push_back(c);
  }
  return g_names.emplace(a, s).first->second.c_str();
}

int g_new_kind = -1;  // what the next `new` is for

struct Hooks : m3::MechDataHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return call(key, args); }
  uint32_t ref(const void* p) override { return addr_of(p); }
  void on_new(uint32_t size, const void* p) override {
    const uint32_t a = call("new", {size});
    if (!p) return;
    g_addr[p] = a, g_obj[a] = p;
    g_kind[p] = size == 0x234 ? 0 : 1;
  }
  Handle mech_model(int32_t roster, int32_t variant) override { return hd(call("0x00484760", {roster, variant})); }
  void node_set_intersect(Handle node, int32_t on) override { call("0x004eb6d0", {lo(node), on}); }
  const m3::MechDfn* dfn(int32_t roster) override {
    const uint32_t a = call("0x004846a0", {roster});
    auto it = g_dfns.find(a);
    return it != g_dfns.end() ? it->second : nullptr;
  }
  void skins_apply(const void* skins, Handle node) override { call("0x00475780", {lo(skins), lo(node)}); }
  Handle cameras(m3::Pilot& p) override { return hd(call("0x00430e70", {addr_of(&p)})); }
  void cameras_free(Handle list) override { call("0x0040abf0", {lo(list)}); }
  void hud_free(Handle hud) override { call("0x0043b860", {lo(hud)}); }
  Handle sound_find(const char* name) override { return hd(call("0x0056f6f0", {name})); }
  Handle anim_find(const char* name) override { return hd(call("0x0050b450", {name})); }
  Handle node_named(int32_t list, const char* name) override { return hd(call("0x004f7fe0", {list, name})); }
  void animations(m3::Pilot& p) override {
    if (call("0x00431a70", {addr_of(&p)}) == 2) p.x20 = 0;  // the stub's way to fail the step
  }
  Handle reader_find(Handle node, const char* key) override { return hd(call("0x005569e0", {lo(node), key})); }
  bool reader_vec3(Handle node, const char* key, float out[3]) override {
    const bool r = call("0x005570b0", {lo(node), key}) != 0;
    if (r)
      for (uint32_t i = 0; i < 3; ++i) out[i] = zgame::from_bits(call("@vec", {i}));
    return r;
  }
  const char* reader_string(Handle node, const char* key) override {
    return cstr(call("0x005569f0", {lo(node), key}));
  }
  const char* reader_value(Handle node) override { return cstr(peek32(peek32(lo(node) + 4) + 0xc)); }
};
Hooks g_hooks;

// --- images -------------------------------------------------------------------
struct Img {
  uint8_t* buf;
  uint8_t* mask;
  uint32_t size;
  void put(uint32_t off, uint32_t v, uint32_t n = 4) {
    for (uint32_t i = 0; i < n && off + i < size; ++i) buf[off + i] = static_cast<uint8_t>(v >> (8 * i)), mask[off + i] = 1;
  }
  void f(uint32_t off, float v) { put(off, zgame::bits(v)); }
  void h(uint32_t off, const void* p) { put(off, addr_of(p)); }
};

void image_data(Img& o, const m3::Pilot& p) {
  o.f(0, p.range_sq[0]), o.f(4, p.range_sq[1]);
  o.put(8, static_cast<uint32_t>(p.refs)), o.put(0xc, static_cast<uint32_t>(p.roster));
  // The name is the roster table's (0x005e6078 + 0x3c i, "notmech" 0x005e6ec4).
  o.put(0x10, p.roster >= 0 && p.roster < 19 ? 0x5e6078u + 0x3cu * static_cast<uint32_t>(p.roster) : 0x5e6ec4u);
  o.h(0x14, p.model), o.h(0x18, p.models[0]), o.h(0x1c, p.models[1]);
  o.put(0x20, p.x20, 1);
  o.h(0x24, p.cameras);
  for (int i = 0; i < 25; ++i) o.h(0x28 + 4 * i, p.sounds[i]);
  for (int i = 0; i < 13; ++i) o.h(0x8c + 4 * i, p.anims[i]);
  for (int i = 0; i < 13; ++i) o.f(0xc0 + 8 * i, p.trans[i][0]), o.f(0xc4 + 8 * i, p.trans[i][1]);
  for (int i = 0; i < 9; ++i) o.h(0x128 + 4 * i, p.actions[i]);
  for (int i = 0; i < 5; ++i) {
    const uint32_t b = 0x14c + 0x14 * i;
    o.h(b, p.canned[i].effect);
    for (int k = 0; k < 3; ++k) o.f(b + 4 + 4 * k, p.canned[i].offset[k]);
    o.h(b + 0x10, p.canned[i].node);
  }
  for (int id = 1; id < 6; ++id)
    for (int k = 0; k < 3; ++k) o.h(0x1a4 + 0xc * id + 4 * k, p.part_effects[id][k]);
  for (int i = 0; i < 4; ++i) o.h(0x1ec + 4 * i, p.death_effects[i]);
  o.h(0x1fc, p.x1fc), o.h(0x200, p.cockpit_explosion);
  o.h(0x204, p.landing_dust), o.h(0x208, p.landing_splash), o.h(0x20c, p.water_death), o.h(0x210, p.x210);
  for (int i = 0; i < 4; ++i) o.h(0x214 + 4 * i, p.stubs[i]);
  o.h(0x224, p.hud), o.h(0x228, p.x228);
  o.f(0x22c, p.limp_speed);
  o.h(0x230, p.soils);
}

void image_soils(Img& o, const m3::Pilot::Soils& s) {
  for (int i = 0; i < 11; ++i) {
    o.h(4 * i, s.step_sound[i]);
    o.h(0x2c + 4 * i, s.footprint[i]);
    o.f(0x58 + 4 * i, s.scale[i]);
  }
}
}  // namespace

extern "C" {

void probe_md_reset() {
  {
    probe_app::Mute mute;
    m3::set_mech_data_hooks(&g_hooks);
    m3::mech_data_clear();
  }
  for (auto& [a, d] : g_dfns) d->skins = nullptr, d->reader = nullptr;  // fakes
  g_addr.clear(), g_obj.clear(), g_kind.clear(), g_names.clear();
}

// A fake definition at `addr`: its reader, skins and the two ranges.
void probe_md_dfn(uint32_t addr, uint32_t reader, uint32_t skins, float a, float b) {
  auto*& d = g_dfns[addr];
  if (!d) d = new m3::MechDfn(0, nullptr);
  d->reader = reinterpret_cast<zrdr::Node*>(static_cast<uintptr_t>(reader));
  d->skins = reinterpret_cast<m3::SkinSet*>(static_cast<uintptr_t>(skins));
  d->put(0x14c, a), d->put(0x150, b);
}

uint32_t probe_md_get(int32_t roster, int32_t create) {
  m3::set_mech_data_hooks(&g_hooks);
  return addr_of(m3::mech_data(roster, create));
}
void probe_md_refresh() { m3::mech_data_refresh(); }
void probe_md_clear() { m3::mech_data_clear(); }

// The cache's entries by address, in order; returns the count.
int probe_md_cache(uint32_t* out, int cap) {
  int n = 0;
  for (const m3::Pilot* p : m3::mech_data_cache())
    if (n < cap) out[n++] = addr_of(p);
  return static_cast<int>(m3::mech_data_cache().size());
}

// The port's object at `addr` as bytes over `size`: 0 data, 1 soils, -1 none.
int probe_md_image(uint32_t addr, uint32_t size, uint8_t* buf, uint8_t* mask) {
  auto it = g_obj.find(addr);
  if (it == g_obj.end()) return -1;
  Img o{buf, mask, size};
  const int kind = g_kind[it->second];
  bool live = false;  // freed objects stay in the maps: only the cache's count
  for (const m3::Pilot* p : m3::mech_data_cache()) live |= p == it->second || p->soils == it->second;
  if (!live) return -2;
  if (kind == 0) image_data(o, *static_cast<const m3::Pilot*>(it->second));
  else image_soils(o, *static_cast<const m3::Pilot::Soils*>(it->second));
  return kind;
}

}  // extern "C"
