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
}  // namespace

extern "C" {

void probe_aown_reset() {
  mech3::ai::anim_owner_free();
  mech3::ai::set_anim_owner_hooks(&g_hooks);
  g_mem.clear(), g_anims.clear(), g_names.clear(), g_strays.clear();
}
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
