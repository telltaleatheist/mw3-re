// Probes for test_veh_fieldbase.py (docs/vehicles.md, The mobile field
// base): the field base's create 0x004589d0 and constructor 0x00458a70 with
// the registry 0x0045a030, the setup 0x00458d40, the frame 0x00459640 and the
// arms 0x004597d0, the damage 0x00458f50, the death 0x004590b0 and the
// destructor 0x00458bc0 (mech3/mech_fieldbase). Every call out goes to the
// shared callback (the basic vehicle's under probe_veh_basic.cpp's hooks,
// bound to this base); memory the original's stubs set up is read through
// peek. The base, its controls, nodes and the registry's other bases are
// the original's addresses where the calls name them.
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "gamez/zgame/unported.h"
#include "mech3/mech_fieldbase.h"
#include "mech3/zai_params.h"
#include "probe_app.h"

extern "C" void probe_vb_hooks(int on);
extern "C" void probe_vb_bind(void* v, uint32_t addr);

namespace {
namespace m3 = mech3;

uint32_t lo(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
m3::Handle hd(uint32_t v) { return reinterpret_cast<m3::Handle>(static_cast<uintptr_t>(v)); }
uint32_t peek(uint32_t a) { return probe_app::call("peek32", {a}); }
float peekf(uint32_t a) { return zgame::from_bits(peek(a)); }

m3::FieldBase* g_b = nullptr;
uint32_t g_addr = 0, g_ctl = 0, g_ictl = 0;
uint32_t g_kill = 0;    // the stubbed 0x004ad3f0 leaves the base inactive
uint32_t g_loaded = 0;  // the stubbed 0x005a9990 loads the script (its frames)
m3::ai::AiParams g_params;
std::map<uint32_t, std::vector<float>> g_mats;  // the original's matrix blocks, the rebuild's copies

// The poses' stubs give a fixed function of the call's index and time.
void pose(uint32_t n, float t, float* out, int k) {
  for (int i = 0; i < k; ++i) out[i] = static_cast<float>(((n * 7 + static_cast<uint32_t>(i) * 3) % 11)) * 0.25f - 1.2f + t * 0.5f;
}
uint32_t g_tn = 0, g_rn = 0;

struct Controls : m3::VehicleControls {
  int update(float dt) override { return static_cast<int>(probe_app::call("ctl.ictl.vt+0xc", {zgame::bits(dt)})); }
  bool destroyed() override { return probe_app::call("ctl.vt+0x1c", {}) != 0; }
};

struct Action : m3::FieldBaseAction {
  std::string name;
  uint32_t addr = 0;
  std::string k(const char* slot) const { return name + slot; }
  void begin() override { probe_app::call(k(".vt+0x0").c_str(), {}); }
  bool step(float dt) override { return (probe_app::call(k(".vt+0x4").c_str(), {zgame::bits(dt)}) & 0xff) != 0; }
  void after(float dt) override { probe_app::call(k(".vt+0x8").c_str(), {zgame::bits(dt)}); }
  void end() override { probe_app::call(k(".vt+0xc").c_str(), {}); }
  bool done() override { return (probe_app::call(k(".vt+0x18").c_str(), {}) & 0xff) != 0; }
};
std::vector<Action*> g_actions;

struct Hooks : m3::FieldBaseHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return probe_app::call(key, args);
  }
  void on_new(uint32_t, const void* p) override {
    g_b = static_cast<m3::FieldBase*>(const_cast<void*>(p));
    probe_vb_bind(g_b, g_addr);
  }
  uint32_t ref(const void* p) override {
    if (g_b && p == static_cast<const m3::BasicVehicle*>(g_b)) return g_addr;
    if (g_b && p == g_b->controls) return g_ctl;
    return lo(p);
  }
  void base_construct(m3::BasicVehicle& v, m3::Handle ctl, m3::Handle node) override {
    unported("0x004ac530", {ref(&v), lo(ctl), lo(node)});
    v.active = true;
    v.node = nullptr;
    v.engine.entry = reinterpret_cast<m3::EngineSoundEntry*>(static_cast<uintptr_t>(0x5151));
  }
  void base_setup(m3::BasicVehicle& v, zrdr::Node* def, const char* name, int32_t kind) override {
    unported("0x0049ec00", {ref(&v), lo(def), name, kind});
  }
  int32_t base_damage(m3::BasicVehicle& v, m3::Handle w, uint32_t x, float amount) override {
    const int32_t r = static_cast<int32_t>(unported("0x004ad3f0", {ref(&v), lo(w), x, zgame::bits(amount)}));
    if (g_kill) v.active = false;
    return r;
  }
  void base_die(m3::BasicVehicle& v) override {
    unported("0x004ad2d0", {ref(&v)});
    v.active = false;
  }
  void base_destroy(m3::BasicVehicle& v) override { unported("0x004ac650", {ref(&v)}); }
  void motion_step(m3::BasicVehicle& v, float throttle) override {
    unported("0x004addc0", {ref(&v) + 0xf0, lo(v.node), zgame::bits(throttle)});
  }
  void part_translation(zsis::Part* p, float t, float out[3]) override {
    unported("0x005aafe0", {lo(p), zgame::bits(t)});
    pose(g_tn++, t, out, 3);
  }
  void part_rotation(zsis::Part* p, float t, float out[4]) override {
    unported("0x005ab1e0", {lo(p), zgame::bits(t)});
    pose(g_rn++ + 100, t, out, 4);
  }
  zsis::Part* arms_part(m3::FieldBaseArms&, const char* name) override {
    return static_cast<zsis::Part*>(hd(unported("0x005aa270", {name, 1})));
  }
  m3::FieldBaseArms* arms_make() override {
    unported("0x005a9990", {"mobilehq"});
    auto* a = new m3::FieldBaseArms;
    a->active = g_loaded != 0;
    a->frame_count = g_loaded;
    a->set_loop_time(1.0f);
    a->wraps = false;
    return a;
  }
  void arms_free(m3::FieldBaseArms* a) override { unported("0x005a9dc0", {lo(a)}); }
  m3::Handle node_find(m3::Handle root, const char* name) override {
    return hd(unported("0x004f9d60", {lo(root), name}));
  }
  void script_path_add(const char* path, bool first) override {
    if (first) unported("0x00576100", {path});
    else unported("0x00576140", {peek(0x736584), path});
  }
  m3::ai::AiParams* brain_params(m3::BasicVehicle&) override {
    return unported("ctl.ictl.vt+0x54", {g_ictl}) ? &g_params : nullptr;
  }
  m3::ai::Valve* valve_get(const char* name) override {
    return static_cast<m3::ai::Valve*>(hd(unported("0x004d3c40", {name})));
  }
  void valve_set(m3::ai::Valve* v, int32_t value) override { unported("0x004d3c00", {lo(v), value}); }
  int32_t game_type() override { return static_cast<int32_t>(unported("game.vt+0x2c", {})); }
  int32_t campaign_bases() override { return static_cast<int32_t>(peek(unported("0x00479440", {}) + 0xa8)); }
  m3::Handle campaign() override { return hd(unported("0x00479440", {})); }
  uint32_t player_allies() override { return peek(peek(peek(0x60861c) + 0x14) + 0xc4); }
  float difficulty() override { return peekf(0x608e5c); }
  void say(m3::Handle sound) override { unported("0x004e6d50", {lo(sound), 0x3f800000}); }
  void all_lost() override { unported("0x0047a720", {peek(0x60b544)}); }
  uint32_t entity_team(m3::Handle e) override { return peek(lo(e) + 0xc0); }
  void controls_hit(m3::BasicVehicle&, m3::Handle w, uint32_t x, float amount, m3::Handle e) override {
    unported("ctl.ictl.vt+0x94", {g_ictl, lo(w), x, zgame::bits(amount), lo(e)});
  }
  float* node_matrix(m3::Handle node) override {
    const uint32_t a = unported("0x004fc3f0", {lo(node)});
    auto& m = g_mats[a];
    if (m.empty())
      for (uint32_t i = 0; i < 12; ++i) m.push_back(peekf(a + 4 * i));
    return m.data();
  }
  void node_set_matrix(m3::Handle node, const float* m) override {
    uint32_t a = 0;
    for (auto& [k, v] : g_mats)
      if (v.data() == m) a = k;
    unported("0x004fc300", {lo(node), a});
  }
  bool node_in_world(m3::Handle node) override { return peek(lo(node) + 0x54) != 0; }
  void node_unparent(m3::Handle node) override { unported("0x004ebb40", {peek(peek(lo(node) + 0x58)), lo(node)}); }
  void anims_stop_on(m3::Handle node) override { unported("0x00506bc0", {lo(node), 1, 1}); }
};
Hooks g_hooks;

// A base as the original's image at `addr` has it, without its constructor.
m3::FieldBase* fresh(uint32_t addr, uint32_t ctl, uint32_t ictl) {
  for (Action* a : g_actions) delete a;
  g_actions.clear();
  if (g_b) {
    if (g_b->arms) delete g_b->arms;
    g_b->arms = nullptr;
    g_b->controls = nullptr;
    delete g_b;
  }
  g_b = new m3::FieldBase;
  g_b->field_base = true;
  g_addr = addr, g_ctl = ctl, g_ictl = ictl;
  probe_vb_bind(g_b, addr);
  g_mats.clear();
  g_tn = g_rn = 0;
  return g_b;
}

std::vector<m3::FieldBase*> g_others;  // the registry's other bases (damage runs)
}  // namespace

extern "C" {
void probe_fb_hooks(int on) {
  probe_vb_hooks(on);
  m3::set_field_base_hooks(on ? &g_hooks : nullptr);
}

// The registry before a run: count, the bases (the original's addresses;
// `active` their +0x08 for the damage's count), the valves.
void probe_fb_registry(int32_t count, const uint32_t* bases, const uint32_t* active, const uint32_t* valves) {
  for (m3::FieldBase* b : g_others) delete b;
  g_others.clear();
  m3::FieldBases& r = m3::field_bases();
  r = m3::FieldBases{};
  r.count = count;
  for (int i = 0; i < 3; ++i) {
    auto* o = new m3::FieldBase;
    o->active = active[i] != 0;
    g_others.push_back(o);
    r.bases[i] = bases[i] ? o : nullptr;
    r.valves[i] = static_cast<m3::ai::Valve*>(hd(valves[i]));
  }
}
// The registry after: count, then each base as 1 + its index in `bases`
// (the run's base 4, none 0), then the valves.
void probe_fb_registry_image(uint32_t* out) {
  const m3::FieldBases& r = m3::field_bases();
  out[0] = static_cast<uint32_t>(r.count);
  for (int i = 0; i < 3; ++i) {
    uint32_t id = 0;
    if (r.bases[i] == g_b) id = 4;
    for (int j = 0; j < 3; ++j)
      if (r.bases[i] && r.bases[i] == g_others[static_cast<size_t>(j)] && !id) id = static_cast<uint32_t>(j + 1);
    out[1 + i] = id;
    out[4 + i] = lo(r.valves[i]);
  }
}
void probe_fb_sounds(const uint32_t* s) {
  for (int i = 0; i < 3; ++i) m3::field_bases().sounds[i] = hd(s[i]);
}

// 0x004589d0: the base at `addr` (the original's) if made.
int probe_fb_create(uint32_t addr, uint32_t ctl, const char* type, const char* name) {
  fresh(addr, ctl, 0);
  delete g_b;
  g_b = nullptr;
  return m3::field_base_create(hd(ctl), type, name) != nullptr;  // on_new binds it
}
// +0x08 active, +0x68 height (bits), +0xec engine entry, +0x104 the steps, +0x108, +0x15c.
void probe_fb_image(uint32_t* out) {
  out[0] = g_b->active, out[1] = zgame::bits(g_b->height), out[2] = lo(g_b->engine.entry);
  out[3] = static_cast<uint32_t>(g_b->actions.size()), out[4] = g_b->begin_next, out[5] = lo(g_b->anim);
}

// 0x00458d40 on a base at `v` with its node; controls at `ctl` (0 none),
// their IControls at `ictl`; the script loaded with `frames` frames (0 not).
void probe_fb_setup(uint32_t v, uint32_t node, uint32_t ctl, uint32_t ictl, uint32_t frames, int32_t path_made,
                    uint32_t def, const char* name, int32_t kind) {
  m3::FieldBase* b = fresh(v, ctl, ictl);
  b->node = hd(node);
  b->x09 = false;
  static Controls c;
  b->controls = ctl ? &c : nullptr;
  g_loaded = frames;
  m3::field_bases_path_made() = path_made != 0;
  g_params = m3::ai::AiParams{};
  for (int off = 0x3c; off < 0xa4; off += 4) g_params.put<uint32_t>(off, 0xdeadbeef);
  m3::field_base_setup(*b, static_cast<zrdr::Node*>(hd(def)), name, kind);
}
// nodes (7), parts (7), +0x09, arms: active, +0x54..+0x64 (5), +0x70, +0x88,
// +0x8c (bits); the brain's eight words; the path made.
void probe_fb_setup_image(uint32_t* out) {
  const m3::FieldBase& b = *g_b;
  int k = 0;
  for (m3::Handle n : b.nodes) out[k++] = lo(n);
  for (zsis::Part* p : b.parts) out[k++] = lo(p);
  out[k++] = b.x09;
  const m3::FieldBaseArms* a = b.arms;
  out[k++] = a ? a->active : 0xffffffff;
  out[k++] = a ? zgame::bits(a->loop_time) : 0;
  out[k++] = a ? zgame::bits(a->inv_loop_time) : 0;
  out[k++] = a ? zgame::bits(a->frame_time) : 0;
  out[k++] = a ? zgame::bits(a->inv_frame_time) : 0;
  out[k++] = a ? a->frame_count : 0;
  out[k++] = a ? a->wraps : 0;
  out[k++] = a ? zgame::bits(a->target) : 0;
  out[k++] = a ? zgame::bits(a->now) : 0;
  for (int off : {0x60, 0x3c, 0x80, 0x5c, 0x88, 0x98, 0x9c, 0xa0}) out[k++] = g_params.get<uint32_t>(off);
  out[k++] = m3::field_bases_path_made();
}

// The frame's base: steps (their names act0.. and the original's
// addresses), begin_next; controls at `ctl`, their throttle; the node, the
// engine entry and the motion's animation; drive; the arms (0 none: else
// `target`, `now`, the nodes and parts).
void probe_fb_frame_setup(uint32_t v, uint32_t ctl, uint32_t node, int32_t steps, const uint32_t* addrs,
                          int32_t begin_next, float throttle, uint32_t entry, uint32_t anim, int32_t drive,
                          int32_t arms, float target, float now, const uint32_t* nodes, const uint32_t* parts) {
  m3::FieldBase* b = fresh(v, ctl, 0);
  static Controls c;
  c.throttle = throttle;
  b->controls = &c;
  b->node = hd(node);
  b->engine.entry = entry ? reinterpret_cast<m3::EngineSoundEntry*>(static_cast<uintptr_t>(entry)) : nullptr;
  b->motion.anim = hd(anim);
  b->begin_next = begin_next != 0;
  for (int i = 0; i < steps; ++i) {
    auto* a = new Action;
    a->name = "act" + std::to_string(i);
    a->addr = addrs[i];
    g_actions.push_back(a);
    b->actions.push_back(a);
  }
  m3::field_bases_drive() = drive != 0;
  if (arms) {
    b->arms = new m3::FieldBaseArms;
    b->arms->target = target, b->arms->now = now, b->arms->rate = 0.5f;
  }
  for (int i = 0; i < 7; ++i) b->nodes[i] = hd(nodes[i]), b->parts[i] = static_cast<zsis::Part*>(hd(parts[i]));
}
void probe_fb_frame(float dt) { m3::field_base_frame(*g_b, dt); }
void probe_fb_arms(float dt) { m3::field_base_arms_step(*g_b, dt); }
// active, the engine entry, the motion's animation, the throttle (bits), the
// arms' now and rate (bits), begin_next, the steps left (as indices) and
// each step's running.
int probe_fb_frame_image(uint32_t* out) {
  const m3::FieldBase& b = *g_b;
  int k = 0;
  out[k++] = b.active, out[k++] = lo(b.engine.entry), out[k++] = lo(b.motion.anim);
  out[k++] = zgame::bits(b.controls->throttle);
  out[k++] = b.arms ? zgame::bits(b.arms->now) : 0, out[k++] = b.arms ? zgame::bits(b.arms->rate) : 0;
  out[k++] = b.begin_next;
  out[k++] = static_cast<uint32_t>(b.actions.size());
  for (const m3::FieldBaseAction* a : b.actions)
    for (size_t i = 0; i < g_actions.size(); ++i)
      if (g_actions[i] == a) out[k++] = static_cast<uint32_t>(i);
  for (const Action* a : g_actions) out[k++] = a->running;
  return k;
}
// The matrices written: block address, then 12 floats' bits, for each.
int probe_fb_mats(uint32_t* out) {
  int k = 0;
  for (auto& [a, m] : g_mats) {
    out[k++] = a;
    for (float f : m) out[k++] = zgame::bits(f);
  }
  return k;
}

// 0x00458f50 on a base at `v`: active, team, allies, controls (0 none) with
// their IControls; the stubbed base damage kills it or not.
int32_t probe_fb_damage(uint32_t v, int32_t active, uint32_t team, uint32_t allies, uint32_t ctl, uint32_t ictl,
                        int32_t kill, uint32_t weapon, uint32_t x, float amount) {
  m3::FieldBase* b = fresh(v, ctl, ictl);
  static Controls c;
  b->controls = ctl ? &c : nullptr;
  b->active = active != 0;
  b->team = team, b->allies = allies;
  g_kill = static_cast<uint32_t>(kill);
  return m3::field_base_damage(*b, hd(weapon), x, amount);
}
int probe_fb_active() { return g_b->active; }
// 0x004590b0 on a base at `v`.
void probe_fb_die(uint32_t v) {
  fresh(v, 0, 0)->active = true;
  m3::field_base_die(*g_b);
}
// 0x00458bc0 on a base at `v`: its anim, node, arms (the original's
// addresses, 0 none) and `steps` queued.
void probe_fb_destroy(uint32_t v, uint32_t anim, uint32_t node, uint32_t arms, int32_t steps) {
  m3::FieldBase* b = fresh(v, 0, 0);
  b->anim = hd(anim);
  b->node = hd(node);
  b->arms = reinterpret_cast<m3::FieldBaseArms*>(static_cast<uintptr_t>(arms));
  for (int i = 0; i < steps; ++i) b->actions.push_back(nullptr);
  m3::field_base_destroy(*b);
  b->arms = nullptr;
}
// +0x04 node, +0x15c, the steps left.
void probe_fb_destroy_image(uint32_t* out) {
  out[0] = lo(g_b->node), out[1] = lo(g_b->anim), out[2] = static_cast<uint32_t>(g_b->actions.size());
}
}  // extern "C"
