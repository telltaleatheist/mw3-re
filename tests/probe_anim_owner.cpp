// Probes for test_anim_owner.py (docs/ai.md section 18): the animation-owner
// registry (mech3/zai_anim_owner). The animations are the probe's own (a
// callback kind and a record each); owners' messages, valves, the inout
// tests and viaNode's calls go to the shared callback, the node's and the
// vehicle's floats read from a byte memory model the test fills as it fills
// the emulator's.
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gamez/zclass/cls_node.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zeffect/zeff_anim_save.h"
#include "mech3/zai_anim_owner.h"
#include "probe_app.h"

namespace {
using mech3::ai::AnimCallback;
using mech3::ai::AnimOwnerRec;
using probe_app::call;
using zgame::Handle;

std::map<uint32_t, uint32_t> g_mem;
uint32_t peek(uint32_t a) {
  auto it = g_mem.find(a);
  return it == g_mem.end() ? 0 : it->second;
}

struct Anim {
  AnimCallback cb = AnimCallback::none;
  AnimOwnerRec* rec = nullptr;
};
std::map<Handle, Anim> g_anims;
std::map<Handle, std::string> g_names;
std::vector<std::unique_ptr<AnimOwnerRec>> g_strays;  // records no list holds
struct Def {
  uint32_t flags = 0;
  int32_t status = 0;
  Handle node = 0;
  std::vector<int32_t> values;
};
std::vector<Handle> g_order;  // zEffect's def array, index 0 first
std::map<Handle, Def> g_defs;
struct NodeRec {
  uint32_t count = 0;
  Handle first = 0;
  std::string name;
};
std::map<Handle, NodeRec> g_nodes;
float g_place[3];

struct Hooks : mech3::ai::AnimOwnerHooks {
  uint32_t unported(const char* k, std::initializer_list<zgame::Arg> args) override { return call(k, args); }
  AnimCallback anim_callback(Handle a) override { return g_anims[a].cb; }
  AnimOwnerRec* anim_record(Handle a) override { return g_anims[a].rec; }
  void anim_set_record(Handle a, AnimOwnerRec* r) override { g_anims[a] = {AnimCallback::ours, r}; }
  void entity_message(Handle o, Handle a, int32_t msg) override {
    const auto it = g_names.find(o);
    const std::string k = (it == g_names.end() ? std::string("?") : it->second) + ".vt+0x74";
    call(k.c_str(), {a, msg});
  }
  Handle player_vehicle() override { return peek(peek(0x60861c) + 0x14); }
  void node_translation(Handle n, float out[3]) override {
    call("0x004fc020", {n});
    for (int i = 0; i < 3; ++i) out[i] = zgame::from_bits(peek(n + 0x10 + 4 * i));
  }
  void node_rotation(Handle n, float out[3]) override {
    call("0x004fbe90", {n});
    for (int i = 0; i < 3; ++i) out[i] = zgame::from_bits(peek(n + 0x20 + 4 * i));
  }
  const float* entity_position(Handle v) override {
    call("0x005a85e0", {v});
    for (int i = 0; i < 3; ++i) g_place[i] = zgame::from_bits(peek(v + 0x30 + 4 * i));
    return g_place;
  }
  void place(Handle v, const float pos[3], float yaw) override {
    call("player.vt+0xdc", {v, zgame::bits(pos[0]), zgame::bits(pos[1]), zgame::bits(pos[2]), zgame::bits(yaw)});
  }
  // 0x0050b570 and 0x00508f40 on the test's defs, as zEffect's ports do.
  Handle anims_next(Handle after) override {
    size_t i = 0;
    if (after)
      while (i < g_order.size() && g_order[i] != after) ++i;
    while (++i < g_order.size()) {
      const Def& d = g_defs[g_order[i]];
      if (d.status != 5 && (d.flags & 0x10)) return g_order[i];
    }
    return 0;
  }
  void anim_callback_values(Handle a, std::vector<int32_t>& out) override {
    const Def& d = g_defs[a];
    if (d.flags & 0x10) out.insert(out.end(), d.values.begin(), d.values.end());
  }
  Handle anim_node(Handle a) override { return g_defs[a].status == 5 ? 0 : g_defs[a].node; }
  Handle node_entity(Handle n) override { return call("0x0040eee0", {n}); }
  Handle node_parent(Handle n) override {
    const NodeRec& r = g_nodes[n];
    return r.count == 1 ? r.first : 0;
  }
  Handle valve_of_node(Handle n) override { return n ? call("0x004d3c40", {g_nodes[n].name.c_str()}) : 0; }
};
Hooks g_hooks;

int32_t index_of(const AnimOwnerRec* r) {
  int32_t i = 0;
  for (const AnimOwnerRec* x : mech3::ai::anim_owner_records()) {
    if (x == r) return i;
    ++i;
  }
  return -1;
}
bool stray(const AnimOwnerRec* r) {
  for (const auto& s : g_strays)
    if (s.get() == r) return true;
  return false;
}
// The binder on a shipped world (test_anim_binder.py), on the defs
// test_anim_run's probe loaded: defs by index, nodes by their pool index + 1;
// the node's entity and the valve go to the shared callback.
void world_callback(zeff::AnimDef*, void*, int32_t) {}
std::map<Handle, zcls::Node*> g_world_nodes;
struct WorldHooks : mech3::ai::AnimOwnerHooks {
  uint32_t unported(const char* k, std::initializer_list<zgame::Arg> args) override { return call(k, args); }
  static zeff::AnimDef* def(Handle a) { return a ? &zeff::anim().defs[a] : nullptr; }
  static Handle hn(zcls::Node* n) {
    if (!n) return 0;
    const Handle h = static_cast<Handle>(zeff::node_index(n) + 1);
    g_world_nodes[h] = n;
    return h;
  }
  AnimCallback anim_callback(Handle a) override {
    const zeff::AnimDef* d = def(a);
    if (!d->callback) return AnimCallback::none;
    return d->callback == &world_callback ? AnimCallback::ours : AnimCallback::other;
  }
  AnimOwnerRec* anim_record(Handle a) override { return static_cast<AnimOwnerRec*>(def(a)->callback_arg); }
  void anim_set_record(Handle a, AnimOwnerRec* r) override { zeff::anim_set_callback(def(a), &world_callback, r); }
  Handle anims_next(Handle a) override {
    const zeff::AnimDef* d = zeff::anim_next_with_callbacks(def(a));
    return d ? static_cast<Handle>(d - zeff::anim().defs) : 0;
  }
  void anim_callback_values(Handle a, std::vector<int32_t>& out) override {
    zeff::anim_each_callback(def(a), [](zeff::AnimDef*, void* v, int32_t x) { static_cast<std::vector<int32_t>*>(v)->push_back(x); }, &out);
  }
  Handle anim_node(Handle a) override { return hn(zeff::anim_node(def(a))); }
  Handle node_entity(Handle n) override { return call("0x0040eee0", {n - 1}); }
  Handle node_parent(Handle n) override {
    const zcls::Node* nd = g_world_nodes[n];
    return nd && nd->parent_count() == 1 && nd->parents ? hn(nd->parents[0]) : 0;
  }
  Handle valve_of_node(Handle n) override { return n ? call("0x004d3c40", {g_world_nodes[n]->name()}) : 0; }
};
WorldHooks g_world;
}  // namespace

extern "C" {

void probe_aown_world_reset() {
  mech3::ai::anim_owner_free();
  mech3::ai::set_anim_owner_hooks(&g_world);
  g_world_nodes.clear();
}
void probe_aown_world_bind() { mech3::ai::anim_owner_bind_all(); }
// The made-up round: def i's status and flags, and def 0 given def k's sequences.
void probe_aown_world_def(int32_t i, int32_t status, uint32_t flags) {
  zeff::AnimDef& d = zeff::anim().defs[i];
  d.raw[0x98] = static_cast<uint8_t>(status);
  d.put<uint32_t>(0x94, flags);
}
void probe_aown_world_seqs(int32_t to, int32_t from) {
  zeff::AnimDef& a = zeff::anim().defs[to];
  const zeff::AnimDef& b = zeff::anim().defs[from];
  a.seqs = b.seqs;
  a.raw[0x108] = b.raw[0x108];
}
// Per def from 1: its callback kind, record's list place (-1 none), owner, valve.
int32_t probe_aown_world_dump(int32_t* out, int32_t cap) {
  const auto& list = mech3::ai::anim_owner_records();
  int32_t n = 0;
  for (int32_t i = 1; i < zeff::anim().def_count() && 4 * (n + 1) <= cap; ++i, ++n) {
    const zeff::AnimDef& d = zeff::anim().defs[i];
    const auto* r = static_cast<const AnimOwnerRec*>(d.callback_arg);
    const int32_t kind = !d.callback ? 0 : d.callback == &world_callback ? 1 : 2;
    out[4 * n] = kind;
    out[4 * n + 1] = kind == 1 ? index_of(r) : -1;
    out[4 * n + 2] = kind == 1 && r ? static_cast<int32_t>(r->owner) : 0;
    out[4 * n + 3] = kind == 1 && r ? static_cast<int32_t>(r->valve) : 0;
  }
  return n;
}

void probe_aown_reset() {
  mech3::ai::anim_owner_free();
  mech3::ai::set_anim_owner_hooks(&g_hooks);
  g_mem.clear(), g_anims.clear(), g_names.clear(), g_strays.clear();
  g_order.clear(), g_defs.clear(), g_nodes.clear();
}
void probe_aown_node(uint32_t n, uint32_t count, uint32_t first, const char* name) { g_nodes[n] = {count, first, name}; }
void probe_aown_def(uint32_t a, uint32_t flags, int32_t status, uint32_t node, const int32_t* values, int32_t n) {
  g_order.push_back(a);
  g_defs[a] = {flags, status, node, std::vector<int32_t>(values, values + n)};
}
void probe_aown_bind() { mech3::ai::anim_owner_bind_all(); }
void probe_aown_poke(uint32_t a, uint32_t v) { g_mem[a] = v; }
void probe_aown_name(uint32_t o, const char* name) { g_names[o] = name; }
// kind 0 none, 1 ours (with a record no list holds, or none), 2 another's.
void probe_aown_anim(uint32_t a, int32_t kind, int32_t has_rec, uint32_t owner, uint32_t valve) {
  Anim& x = g_anims[a];
  x.cb = static_cast<AnimCallback>(kind);
  x.rec = nullptr;
  if (kind == 1 && has_rec) {
    g_strays.push_back(std::make_unique<AnimOwnerRec>(AnimOwnerRec{owner, valve}));
    x.rec = g_strays.back().get();
  }
}
int32_t probe_aown_claim(uint32_t owner, uint32_t a) { return mech3::ai::anim_owner_claim(owner, a) ? 1 : 0; }
void probe_aown_release(uint32_t owner) { mech3::ai::anim_owner_release(owner); }
void probe_aown_message(uint32_t a, int32_t null_rec, int32_t msg) {
  mech3::ai::anim_owner_message(a, null_rec ? nullptr : g_anims[a].rec, msg);
}
void probe_aown_end() { mech3::ai::anim_owner_mission_end(); }
void probe_aown_free() { mech3::ai::anim_owner_free(); }
void probe_aown_set_valve(int32_t i, uint32_t valve) {
  for (AnimOwnerRec* r : mech3::ai::anim_owner_records())
    if (i-- == 0) r->valve = valve;
}
// kind, the record (its list index, -1 none, -2 a stray, -3 freed), owner, valve.
void probe_aown_state(uint32_t a, int32_t out[4]) {
  const Anim& x = g_anims[a];
  out[0] = static_cast<int32_t>(x.cb);
  const int32_t i = x.rec ? index_of(x.rec) : -1;
  const bool alive = x.rec && (i >= 0 || stray(x.rec));
  out[1] = !x.rec ? -1 : i >= 0 ? i : alive ? -2 : -3;
  out[2] = alive ? static_cast<int32_t>(x.rec->owner) : 0;
  out[3] = alive ? static_cast<int32_t>(x.rec->valve) : 0;
}
int32_t probe_aown_list(uint32_t* out, int32_t cap) {
  int32_t n = 0;
  for (const AnimOwnerRec* r : mech3::ai::anim_owner_records()) {
    if (n < cap) out[2 * n] = r->owner, out[2 * n + 1] = r->valve;
    ++n;
  }
  return n;
}

}  // extern "C"
