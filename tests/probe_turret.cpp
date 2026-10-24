// Probes for test_turret.py (docs/vehicles.md, The turrets): the loader
// 0x004a76b0, the turret's constructor 0x004a5c60 and setup 0x004a6270,
// its slots and destructor, the turn limits 0x004a4b30 (mech3/turret).
// Every call out goes to the shared callback; what the code reads of other
// modules' objects (node flags and names, matrices, weapon definitions,
// the player) is a memory model the test fills, as the emulator's memory
// on the original's side. Out-parameters come from silent "@" calls the
// test's stubs also answer.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "mech3/turret.h"
#include "mech3/weapon.h"
#include "probe_app.h"

namespace {
using mech3::tur::Turret;
using probe_app::call;
using zgame::Handle;

std::map<uint32_t, uint32_t> g_mem;  // dwords by address
uint32_t peek(uint32_t a) {
  auto it = g_mem.find(a);
  return it == g_mem.end() ? 0 : it->second;
}
std::map<uint32_t, std::string> g_names;  // nodes' names
std::vector<uint32_t> g_handles;
size_t g_next = 0;
zrdr::Node* g_tree = nullptr;
uint32_t g_root_handle = 0;  // the original's tree
std::vector<std::unique_ptr<mech3::ai::Contact>> g_contacts;

void fetch(const char* key, Handle node, float* out, int n) {
  for (int i = 0; i < n; ++i) out[i] = zgame::from_bits(call(key, {node, i}));
}

struct Recording : mech3::tur::TurretHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return call(key, args); }
  Handle turret_handle() override { return g_next < g_handles.size() ? g_handles[g_next++] : 0; }
  void turret_free(Turret* t) override {
    call("turret.vt+0x0", {t->self, 1});
    delete t;
  }
  zrdr::Node* read(const char* file) override {
    g_root_handle = call("0x00556740", {file, 0});
    return g_root_handle ? g_tree : nullptr;
  }
  void free_tree(zrdr::Node* root) override {
    call("0x00556820", {g_root_handle});
    (void)root;  // the probe frees it
  }
  const char* node_name(Handle node) override {
    auto it = g_names.find(node);
    return it == g_names.end() ? nullptr : it->second.c_str();
  }
  uint32_t node_flags(Handle node) override { return peek(node + 0x24); }
  Handle node_model(Handle node) override { return peek(node + 0x3c); }
  float matrix_y(Handle m) override { return zgame::from_bits(peek(m + 0x28)); }
  float weapon_range(Handle def) override { return zgame::from_bits(peek(def + 0x1c)); }
  uint32_t weapon_flags(Handle def) override { return peek(def + 0x6c); }
  Handle player_vehicle() override { return peek(peek(0x60861c) + 0x14); }
  Handle world_node() override { return peek(peek(0x608c84) + 4); }
  void node_world_position(Handle node, float out[3]) override {
    call("0x004ed650", {node});
    fetch("@pos", node, out, 3);
  }
  void node_rotation(Handle node, float out[3]) override {
    call("0x004fbe90", {node});
    fetch("@rot", node, out, 3);
  }
  void node_translation(Handle node, float out[3]) override {
    call("0x004fc020", {node});
    fetch("@tr", node, out, 3);
  }
  int32_t node_box(Handle node, float box[6]) override {
    const int32_t r = static_cast<int32_t>(call("0x004ebd50", {node}));
    if (r == 0) fetch("@box", node, box, 6);
    return r;
  }
  void entity_set_node(Handle self, Handle node, Handle* target_node, Handle* centroid_node) override {
    call("0x004d6c90", {self, node, 0, 0, 0});
    *target_node = node ? call("@tnode", {node}) : 0;
    *centroid_node = node ? call("@cnode", {node}) : 0;
  }
};
Recording g_hooks;

uint32_t f2u(float f) { return zgame::bits(f); }
void put32(uint8_t* img, uint32_t off, uint32_t v) { std::memcpy(img + off, &v, 4); }
void putf(uint8_t* img, uint32_t off, float f) { put32(img, off, f2u(f)); }

// The turret as the original's 0x284 bytes (heap pointers left 0: the
// lists, the AiTarget's vectors, the AMS; the test compares those apart).
void image(const Turret& t, uint32_t node_arg, uint8_t* img) {
  std::memset(img, 0, 0x284);
  put32(img, 0x00, 0x5c8770);
  put32(img, 0x04, t.node);
  img[0x08] = t.active;
  put32(img, 0x10, t.target_node);
  put32(img, 0x14, t.centroid_node);
  putf(img, 0x68, t.height);
  put32(img, 0x98, t.player_contact ? 1 : 0);
  putf(img, 0x9c, t.sensor_range);
  putf(img, 0xa0, t.range);
  putf(img, 0xa4, t.range2);
  img[0xbc] = t.xbc;
  put32(img, 0xc0, t.team);
  put32(img, 0xc4, t.allies);
  put32(img, 0xc8, t.enemies);
  put32(img, 0xd4, t.attacker);
  put32(img, 0xd8, static_cast<uint32_t>(t.id));
  put32(img, 0xdc, t.healthy);
  for (int i = 0; i < 3; ++i) {
    putf(img, 0xe0 + 4 * i, t.pos[i]);
    putf(img, 0xec + 4 * i, t.aim_from[i]);
    putf(img, 0x108 + 4 * i, t.muzzle[0][i]);
    putf(img, 0x114 + 4 * i, t.muzzle[1][i]);
    putf(img, 0x124 + 4 * i, t.facing[i]);
    putf(img, 0x160 + 4 * i, t.fire_dir[i]);
    putf(img, 0x16c + 4 * i, t.fire_from[i]);
    putf(img, 0x178 + 4 * i, t.x178[i]);
  }
  put32(img, 0xf8, t.off_node);
  put32(img, 0xfc, t.turret);
  put32(img, 0x100, t.turret_matrix);
  put32(img, 0x104, t.gun);
  put32(img, 0x120, t.gun_matrix);
  put32(img, 0x130, t.effect);
  putf(img, 0x134, t.effect_time);
  img[0x138] = t.ams_on;
  img[0x139] = t.ams_found;
  put32(img, 0x13c, t.ams ? 1 : 0);
  put32(img, 0x140, t.weapon);
  put32(img, 0x144, static_cast<uint32_t>(t.ammo));
  putf(img, 0x148, t.detect_range);
  img[0x14c] = t.aimed;
  put32(img, 0x150, static_cast<uint32_t>(t.firepoint));
  put32(img, 0x154, static_cast<uint32_t>(t.firepoints));
  put32(img, 0x158, t.fire_node[0]);
  put32(img, 0x15c, t.fire_node[1]);
  putf(img, 0x184, t.rate);
  putf(img, 0x188, t.next_fire);
  putf(img, 0x18c, t.burst_end);
  putf(img, 0x190, t.burst[0]);
  putf(img, 0x194, t.burst[1]);
  put32(img, 0x198, t.beam);
  img[0x19c] = t.beam_on;
  put32(img, 0x1a0, static_cast<uint32_t>(t.x1a0));
  putf(img, 0x204, t.yaw_lo);
  putf(img, 0x208, t.yaw_hi);
  put32(img, 0x20c, t.self + 0x210);
  put32(img, 0x210, static_cast<uint32_t>(t.limits.kind));
  putf(img, 0x214, t.limits.lo_sin);
  putf(img, 0x218, t.limits.lo_cos);
  putf(img, 0x21c, t.limits.hi_sin);
  putf(img, 0x220, t.limits.hi_cos);
  for (int i = 0; i < 3; ++i) {
    putf(img, 0x224 + 4 * i, t.limits.lo_edge[i]);
    putf(img, 0x230 + 4 * i, t.limits.hi_edge[i]);
  }
  put32(img, 0x23c, t.destroy_anim);
  putf(img, 0x240, t.health);
  putf(img, 0x244, t.per_damage);
  img[0x248] = t.picked;
  for (uint32_t o : {0x24cu, 0x258u, 0x264u, 0x270u}) img[o] = static_cast<uint8_t>(node_arg);  // the lists' allocator bytes
  put32(img, 0x254, static_cast<uint32_t>(t.wakeup.size()));
  put32(img, 0x260, static_cast<uint32_t>(t.onfire.size()));
  put32(img, 0x26c, static_cast<uint32_t>(t.aux.size()));
  put32(img, 0x278, static_cast<uint32_t>(t.sleep.size()));
  img[0x27c] = t.idle;
  put32(img, 0x280, t.fire_sound);
}

std::vector<uint32_t> g_node_args;  // each turret's node, for the allocator bytes
}  // namespace

extern "C" {

void probe_turret_reset() {
  mech3::tur::set_turret_hooks(&g_hooks);
  mech3::tur::turrets_delete_all();
  mech3::tur::turret_count() = 0;
  g_mem.clear();
  g_names.clear();
  g_handles.clear();
  g_node_args.clear();
  g_contacts.clear();
  g_next = 0;
  if (g_tree) zrdr::free_node(g_tree), g_tree = nullptr;
}

void probe_turret_poke(uint32_t a, uint32_t v) { g_mem[a] = v; }
void probe_turret_name(uint32_t node, const char* name) { g_names[node] = name; }
void probe_turret_handles(const uint32_t* h, int32_t n) { g_handles.assign(h, h + n); }

// aiTurrets.zrd's bytes (one binary node) as the file the loader reads.
int32_t probe_turret_load(const uint8_t* zrd, int32_t len, uint32_t at) {
  std::FILE* f = std::tmpfile();
  std::fwrite(zrd, 1, static_cast<size_t>(len), f);
  std::rewind(f);
  g_tree = static_cast<zrdr::Node*>(std::calloc(1, sizeof(zrdr::Node)));
  zrdr::read_node(f, g_tree);
  std::fclose(f);
  const bool ok = mech3::tur::turrets_load("aiTurrets.zrd", at);
  return ok ? static_cast<int32_t>(mech3::tur::turret_list().size()) : -1;
}

void probe_turret_node_args(const uint32_t* a, int32_t n) { g_node_args.assign(a, a + n); }

// Turret i's image, then its four lists (wakeup, onfire, aux, sleep: count
// then items) and its AMS (on, range, next, period, rounds) into `extra`.
int32_t probe_turret_state(int32_t i, uint8_t* img, uint32_t* extra) {
  auto& l = mech3::tur::turret_list();
  if (i < 0 || static_cast<size_t>(i) >= l.size()) return 0;
  const Turret& t = *l[i];
  image(t, i < static_cast<int32_t>(g_node_args.size()) ? g_node_args[i] : 0, img);
  int32_t k = 0;
  for (const std::list<Handle>* v : {&t.wakeup, &t.onfire, &t.aux, &t.sleep}) {
    extra[k++] = static_cast<uint32_t>(v->size());
    for (Handle a : *v) extra[k++] = a;
  }
  if (t.ams) {
    extra[k++] = t.ams->on;
    extra[k++] = f2u(t.ams->range);
    extra[k++] = f2u(t.ams->next);
    extra[k++] = f2u(t.ams->period);
    extra[k++] = static_cast<uint32_t>(t.ams->rounds);
  }
  return k;
}

// One of the turret's slots: 0 hit (a, b, damage bits), 1 kill, 2
// deactivate (a), 3 message (a from, b msg), 4 tint (a), 5 awake, 6
// in_range, 7 alive (bits), 8 the player's contact at dist2 (a bits; 0
// none), 9 marker (a), 10 aux_reset.
uint32_t probe_turret_op(int32_t i, int32_t op, uint32_t a, uint32_t b, uint32_t c) {
  auto& l = mech3::tur::turret_list();
  Turret* t = i >= 0 && static_cast<size_t>(i) < l.size() ? l[i] : nullptr;
  if (op == 9) return mech3::tur::turret_marker(static_cast<int32_t>(a));
  if (!t) return 0xffffffff;
  switch (op) {
    case 0: return t->hit(a, b, zgame::from_bits(c));
    case 1: t->kill(); return 0;
    case 2: t->deactivate(a != 0); return 0;
    case 3: t->message(a, static_cast<int32_t>(b)); return 0;
    case 4: t->tint(a != 0); return 0;
    case 5: return t->awake() ? 1 : 0;
    case 6: return t->in_range() ? 1 : 0;
    case 7: return f2u(t->alive());
    case 8:
      if (b) {
        g_contacts.push_back(std::make_unique<mech3::ai::Contact>());
        g_contacts.back()->dist2 = zgame::from_bits(a);
        t->player_contact = g_contacts.back().get();
      } else {
        t->player_contact = nullptr;
      }
      return 0;
    case 10: t->aux_reset(); return 0;
  }
  return 0;
}

void probe_turret_delete_all() { mech3::tur::turrets_delete_all(); }
void probe_turret_end() { mech3::tur::turrets_end(); }

}  // extern "C"
