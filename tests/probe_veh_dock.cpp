// Probes for test_veh_dock.py (docs/vehicles.md, The repair dock): the dock's
// states (0x0045a740..0x0045b6f0) on the registry and the field bases'
// drive (0x004598d0..0x0045b9c0), mech3/mech_fieldbase_dock. The registry is
// the port's (mech3::field_bases()), set up as the original's image at
// 0x00605ae0; its bases are the probe's, named by the original's addresses
// in the calls; the other entities, valves, sounds and the dock point are
// the original's addresses as opaque handles. Memory the original's stubs
// set up is read through peek.
#include <cstdint>
#include <cstring>

#include "gamez/zgame/unported.h"
#include "mech3/mech_fieldbase.h"
#include "probe_app.h"

extern "C" void probe_vb_hooks(int on);

namespace {
namespace m3 = mech3;

uint32_t lo(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
m3::Handle hd(uint32_t v) { return reinterpret_cast<m3::Handle>(static_cast<uintptr_t>(v)); }
uint32_t peek(uint32_t a) { return probe_app::call("peek32", {a}); }
float peekf(uint32_t a) { return zgame::from_bits(peek(a)); }

struct Base {
  m3::FieldBase b;
  m3::FieldBaseArms arms;
  m3::VehicleControls ctl;
  uint32_t addr = 0;
  float in[3] = {};
};
Base g_base[3];
uint32_t g_valves[3] = {};

int base_index(const void* p) {
  for (int i = 0; i < 3; ++i)
    if (p == static_cast<const void*>(&g_base[i].b) || p == static_cast<const m3::BasicVehicle*>(&g_base[i].b)) return i;
  return -1;
}

const float* peek3(uint32_t a) {
  static float t[4][3];
  static int k = 0;
  float* r = t[k++ & 3];
  for (int i = 0; i < 3; ++i) r[i] = peekf(a + 4 * static_cast<uint32_t>(i));
  return r;
}

struct Hooks : m3::FieldBaseHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return probe_app::call(key, args);
  }
  uint32_t ref(const void* p) override {
    const int i = base_index(p);
    return i >= 0 ? g_base[i].addr : lo(p);
  }
  float game_time() override { return peekf(0x60991c); }
  uint32_t frame() override { return peek(0x72f338); }
  int32_t valve_value(m3::ai::Valve* v) override {
    for (int i = 0; i < 3; ++i)
      if (lo(v) == g_valves[i] && g_valves[i]) return static_cast<int32_t>(unported(i == 0 ? "vv0.vt+0x4" : i == 1 ? "vv1.vt+0x4" : "vv2.vt+0x4", {}));
    return 0;
  }
  void valve_set(m3::ai::Valve* v, int32_t value) override { unported("0x004d3c00", {lo(v), value}); }
  const float* entity_position(m3::Handle e) override { return peek3(unported("0x005a85e0", {ref(e)})); }
  bool powered_down(m3::Handle e) override { return unported("req.vt+0xa0", {lo(e)}) != 0; }
  const float* dock_point_position(m3::Handle p) override { return peek3(unported("point.vt+0x18", {lo(p)})); }
  m3::Handle view_vehicle() override { return hd(peek(peek(0x5f4c8c) + 0x64)); }
  int32_t view_camera() override {
    const uint32_t c = peek(peek(0x5f4c8c) + 0x58);
    return c ? static_cast<int32_t>(peek(c + 4)) : -1;
  }
  void view_set_camera(int32_t mode) override { unported("0x0040ae70", {peek(0x5f4c8c), mode}); }
  void mech_rest_at(m3::Handle mech, const float* pos) override {
    unported("0x00418e90", {lo(mech), zgame::bits(pos[0]), zgame::bits(pos[1]), zgame::bits(pos[2])});
  }
  void mech_repair(m3::Handle mech, float dt) override { unported("0x004154e0", {lo(mech), zgame::bits(dt)}); }
  m3::Handle effects_start(m3::Handle def, m3::Handle node) override {
    return hd(unported("0x00508c10", {lo(def), lo(node), 0, 0, 0}));
  }
  m3::Handle dock_request(m3::DockIdle&, m3::Handle m) override { return hd(unported("0x0045a780", {lo(m)})); }
  void say(m3::Handle sound) override { unported("0x004e6d50", {lo(sound), 0x3f800000}); }
  void marker_clear() override { unported("0x0047a720", {peek(0x60b544)}); }
  float stopping_now(m3::BasicVehicle& v) override { return zgame::from_bits(unported("veh.vt+0x124", {ref(&v)})); }
};
Hooks g_hooks;

m3::DockState* state(int i) {
  m3::FieldBases& r = m3::field_bases();
  m3::DockState* s[7] = {&r.idle, &r.contact, &r.places, &r.deploy, &r.repair, &r.close, &r.reset};
  return i >= 0 && i < 7 ? s[i] : nullptr;
}
int state_index(const m3::DockStep* p) {
  for (int i = 0; i < 7; ++i)
    if (p == state(i)) return i;
  return 99;
}
uint32_t step_id(const m3::DockStep* p) {
  if (const auto* d = dynamic_cast<const m3::FieldBaseDrive*>(p))
    return 0x100u | (d->open ? 0x10u : 0u) | static_cast<uint32_t>(base_index(d->base) & 0xf);
  return 0xff;  // the run's own placeholder
}
m3::DockStep g_placeholder;
}  // namespace

extern "C" {
void probe_dk_hooks(int on) {
  probe_vb_hooks(on);
  m3::set_field_base_hooks(on ? &g_hooks : nullptr);
}

// The bases (b: addr, active, node, actions placed before, begin_next; f:
// dock_at, in, arms now and target, the controls' x70.z, throttle) and the
// registry (u, see the test's REG_WORDS).
void probe_dk_setup(const uint32_t* b, const float* f, const uint32_t* u) {
  m3::FieldBases& r = m3::field_bases();
  r.clear();
  for (int i = 0; i < 3; ++i) {
    Base& x = g_base[i];
    const uint32_t* w = b + 5 * i;
    const float* g = f + 10 * i;
    x.b.actions.clear();
    x.addr = w[0];
    x.b.active = w[1] != 0;
    x.b.node = hd(w[2]);
    for (uint32_t k = 0; k < w[3]; ++k) x.b.actions.push_back(&g_placeholder);
    x.b.begin_next = w[4] != 0;
    for (int k = 0; k < 3; ++k) x.b.dock_at[k] = g[k], x.in[k] = g[3 + k];
    x.b.dock_in = x.in;
    x.arms.now = g[6], x.arms.target = g[7];
    x.b.arms = &x.arms;
    x.ctl.x70.z = g[8], x.ctl.throttle = g[9];
    x.b.controls = &x.ctl;
    x.b.anim = nullptr;
    r.bases[i] = w[0] ? &x.b : nullptr;
  }
  int k = 0;
  r.count = static_cast<int32_t>(u[k++]);
  for (int i = 0; i < 3; ++i) g_valves[i] = u[k], r.valves[i] = static_cast<m3::ai::Valve*>(hd(u[k])), ++k;
  r.requester = hd(u[k++]);
  r.busy = u[k++] != 0;
  r.dock_point = hd(u[k++]);
  r.camera = static_cast<int32_t>(u[k++]);
  r.deploy_valve = static_cast<m3::ai::Valve*>(hd(u[k++]));
  for (m3::Handle& s : r.sounds) s = hd(u[k++]);
  r.effects = hd(u[k++]);
  r.idle_on = u[k++] != 0;
  r.begin_next = u[k++] != 0;
  r.states.clear();
  const uint32_t n = u[k++];
  for (uint32_t i = 0; i < 8; ++i, ++k)
    if (i < n) r.states.push_back(state(static_cast<int>(u[k])));
  for (int i = 0; i < 7; ++i) {
    m3::DockState* s = state(i);
    s->finished = u[k++] != 0;
    s->sound = hd(u[k++]);
  }
  r.idle.asked = u[k++] != 0;
  r.places.until = u[k++];
  r.repair.timer = zgame::from_bits(u[k++]);
  m3::field_bases_quiet_until() = zgame::from_bits(u[k++]);
  m3::field_bases_contact() = u[k++] != 0;
  m3::field_bases_drive() = u[k++] != 0;
  // The deploy's and close's drives, when the run starts with them made.
  for (m3::FieldBaseDrive** d : {r.deploy.drives, r.close.drives}) {
    const uint32_t mask = u[k++];
    for (int i = 0; i < 3; ++i) {
      delete d[i];
      d[i] = (mask >> i) & 1 ? new m3::FieldBaseDrive(r.bases[i], d == r.deploy.drives) : nullptr;
      if (d[i]) d[i]->finished = (mask >> (4 + i)) & 1;
    }
  }
}

// One state's slot: 0 begin, 1 step, 2 end, 3 message, 4 abort, 5 done.
uint32_t probe_dk_call(int32_t which, int32_t slot, float dt, uint32_t m) {
  m3::DockState* s = state(which);
  switch (slot) {
    case 0: s->begin(); return 0;
    case 1: return s->step(dt);
    case 2: s->end(); return 0;
    case 3: return lo(s->message(hd(m)));
    case 4: s->abort(); return 0;
    default: return s->done();
  }
}
// The registry's helpers: 0 requester_ok, 1 set_requester, 2 message, 3 abort_front.
uint32_t probe_dk_helper(int32_t which, uint32_t m) {
  m3::FieldBases& r = m3::field_bases();
  switch (which) {
    case 0: return r.requester_ok();
    case 1: r.set_requester(hd(m)); return 0;
    case 2: r.message(hd(m)); return 0;
    default: r.abort_front(); return 0;
  }
}

int probe_dk_image(uint32_t* out) {
  const m3::FieldBases& r = m3::field_bases();
  int k = 0;
  out[k++] = static_cast<uint32_t>(r.states.size());
  int n = 0;
  for (const m3::DockStep* s : r.states)
    if (n++ < 8) out[k++] = static_cast<uint32_t>(state_index(s));
  for (; n < 8; ++n) out[k++] = 0xffffffff;
  out[k++] = r.begin_next, out[k++] = lo(r.requester), out[k++] = r.busy, out[k++] = static_cast<uint32_t>(r.camera);
  out[k++] = r.idle_on;
  for (int i = 0; i < 7; ++i) out[k++] = state(i)->finished, out[k++] = lo(state(i)->sound);
  out[k++] = r.idle.asked, out[k++] = r.places.until, out[k++] = zgame::bits(r.repair.timer);
  for (const m3::FieldBaseDrive* const* d : {r.deploy.drives, r.close.drives}) {
    uint32_t mask = 0;
    for (int i = 0; i < 3; ++i)
      if (d[i]) mask |= (1u << i) | (d[i]->base ? static_cast<uint32_t>(base_index(d[i]->base) + 1) << (4 + 2 * i) : 0);
    out[k++] = mask;
  }
  out[k++] = zgame::bits(m3::field_bases_quiet_until()), out[k++] = m3::field_bases_contact(),
  out[k++] = m3::field_bases_drive();
  for (const Base& x : g_base) {
    out[k++] = static_cast<uint32_t>(x.b.actions.size()), out[k++] = x.b.begin_next, out[k++] = lo(x.b.anim);
    int j = 0;
    for (const m3::DockStep* s : x.b.actions)
      if (j++ < 4) out[k++] = step_id(s);
    for (; j < 4; ++j) out[k++] = 0xffffffff;
  }
  return k;
}

// The drive on base `i` (set up by probe_dk_setup): its fields, then a slot
// (0 begin, 1 step, 2 after, 4 abort, 5 done).
m3::FieldBaseDrive* g_drive = nullptr;
void probe_dv_make(int32_t i, int32_t open, int32_t finished, int32_t reached, float way, float travel) {
  delete g_drive;
  g_drive = new m3::FieldBaseDrive(&g_base[i].b, open != 0);
  g_drive->finished = finished != 0, g_drive->reached = reached != 0, g_drive->way = way, g_drive->travel = travel;
  g_base[i].b.actions.push_front(g_drive);
}
uint32_t probe_dv_call(int32_t slot, float dt) {
  switch (slot) {
    case 0: g_drive->begin(); return 0;
    case 1: return g_drive->step(dt);
    case 2: g_drive->after(dt); return 0;
    case 4: g_drive->abort(); return 0;
    default: return g_drive->done();
  }
}
void probe_dv_speed(int32_t i, float vz) { g_base[i].ctl.x70.z = vz; }
void probe_dv_arms(int32_t i, float now) { g_base[i].arms.now = now; }
// finished, reached, way, travel (bits), the base's arms target (bits),
// throttle (bits), its steps and begin_next.
void probe_dv_image(int32_t i, uint32_t* out) {
  const Base& x = g_base[i];
  out[0] = g_drive->finished, out[1] = g_drive->reached, out[2] = zgame::bits(g_drive->way);
  out[3] = zgame::bits(g_drive->travel), out[4] = zgame::bits(x.arms.target), out[5] = zgame::bits(x.ctl.throttle);
  out[6] = static_cast<uint32_t>(x.b.actions.size()), out[7] = x.b.begin_next;
}
}  // extern "C"
