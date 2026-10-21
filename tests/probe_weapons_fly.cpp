// zWeapon's projectiles (gamez/zweapon/zwep_shot) for
// tests/differential/test_weapons_fly.py, on the definitions probe_weapons.cpp
// loaded. Every hook goes to one Python callback, which keeps the port's side
// of the fake scene, answers the queries the same way it answers the
// emulator's, and logs the calls; handles are the emulator's addresses.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "crt/msvc_rand.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zweapon/zweapon.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/weapon.h"

namespace {

using zwep::Handle;
using zwep::HitList;
using zwep::HitRecord;
using zwep::Projectile;
using zwep::Vec3f3;

using PyFn = int32_t (*)(int32_t kind, const int32_t* args, int32_t n, int32_t* out);
PyFn g_py = nullptr;
int32_t g_lock_flag;
zwep::LockRef g_locks[8];
Vec3f3 g_lock_at[8], g_lock_vel[8];
// A lock pointer as a token: 0x30001 + 2i target i's position, 0x30002 + 2i its velocity.
int32_t ltok(const Vec3f3* v) {
  if (!v) return 0;
  for (int i = 0; i < 8; ++i) {
    if (v == &g_lock_at[i]) return 0x30001 + 2 * i;
    if (v == &g_lock_vel[i]) return 0x30002 + 2 * i;
  }
  return -2;
}
// The beams the test made, by index; their muzzles and directions.
zwep::Beam* g_beams[16];
Vec3f3 g_beam_at[16], g_beam_dir[16];
uint32_t g_beam_zone[16];
int32_t btok(const zwep::Beam* b) {
  if (!b) return 0;
  for (int i = 0; i < 16; ++i)
    if (g_beams[i] == b) return 0x50000 + i;
  return -2;
}

int32_t fb(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
float bf(int32_t i) {
  float f;
  std::memcpy(&f, &i, 4);
  return f;
}
int32_t u(const void* p) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(p)); }
Handle h(int32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(static_cast<uint32_t>(v))); }

// A projectile as a token: 0x10000 + its pool index; 0 for none.
int32_t ptok(const Projectile* p) {
  if (!p) return 0;
  const auto& pool = zwep::state().pool;
  if (!pool.empty() && p >= pool.data() && p < pool.data() + pool.size())
    return 0x10000 + static_cast<int32_t>(p - pool.data());
  return 0x20000;  // a stand-in on the stack
}
Projectile* pfrom(int32_t t) {
  auto& pool = zwep::state().pool;
  if (t >= 0x10000 && t < 0x10000 + static_cast<int32_t>(pool.size())) return &pool[static_cast<size_t>(t - 0x10000)];
  return nullptr;
}
int32_t dtok(const zwep::WeaponDef* d) { return d ? d->index : -1; }

int32_t call(int32_t kind, std::initializer_list<int32_t> a, int32_t* out = nullptr) {
  static int32_t dummy[400];
  std::vector<int32_t> v(a);
  return g_py(kind, v.data(), static_cast<int32_t>(v.size()), out ? out : dummy);
}
void v3(std::vector<int32_t>& o, const Vec3f3& v) { o.insert(o.end(), {fb(v.x), fb(v.y), fb(v.z)}); }
int32_t callv(int32_t kind, std::vector<int32_t> v, int32_t* out = nullptr) {
  static int32_t dummy[400];
  return g_py(kind, v.data(), static_cast<int32_t>(v.size()), out ? out : dummy);
}
std::vector<int32_t> rec_args(const HitRecord* r) {
  std::vector<int32_t> o;
  v3(o, r->point);
  o.push_back(u(r->node));
  return o;
}
// A query's answer: count byte, total byte, 31 x 10 ints of records, how
// many records were written (the rest stay as they were, as in the exe).
void read_list(const int32_t* a, HitList* out) {
  out->count = static_cast<uint8_t>(a[0]);
  out->total = static_cast<uint8_t>(a[1]);
  for (int i = 0; i < a[312]; ++i) {
    const int32_t* r = a + 2 + 10 * i;
    HitRecord& x = out->rec[i];
    x.normal = {bf(r[0]), bf(r[1]), bf(r[2])};
    x.point = {bf(r[3]), bf(r[4]), bf(r[5])};
    x.x18 = static_cast<uint32_t>(r[6]);
    x.distance_sq = bf(r[7]);
    x.material = h(r[8]);
    x.node = h(r[9]);
  }
}

void tick_cb(Projectile* p) { call(51, {ptok(p)}); }

// The handler tables the port made (to dump them), and two touch handlers.
std::vector<zwep::NodeHandlers*> g_tables;
HitRecord g_touch_rec;
float touch_a(Handle obj, float dmg) { return call(83, {1, u(obj), fb(dmg)}), 0.0f; }
float touch_b(Handle obj, float dmg) { return call(83, {2, u(obj), fb(dmg)}), 0.0f; }
uint32_t damage_a(Handle, zwep::WeaponDef*, HitRecord*, float) { return 0; }
int32_t fn_tok(void (*f)()) {
  if (!f) return 0;
  if (f == reinterpret_cast<void (*)()>(touch_a)) return 1;
  if (f == reinterpret_cast<void (*)()>(touch_b)) return 2;
  if (f == reinterpret_cast<void (*)()>(damage_a)) return 3;
  return -2;
}

int32_t net_gate(zwep::WeaponDef* d, int32_t* id) {
    int32_t out[2];
    const int32_t r = call(41, {dtok(d), *id}, out);
    *id = out[0];
    return r;
}
void net_detonated(zwep::WeaponDef* d, const Vec3f3* at, Handle owner) {
    std::vector<int32_t> v{dtok(d), at ? 1 : 0};
    if (at) v3(v, *at);
    v.push_back(u(owner));
    callv(42, v);
}


void set_hooks() {
  zwep::RunHooks& k = zwep::run_hooks();
  k = zwep::RunHooks{};
  k.parent_count = [](Handle n) { return call(60, {u(n)}); };
  k.first_parent = [](Handle n) { return h(call(61, {u(n)})); };
  k.child_count = [](Handle n) { return call(62, {u(n)}); };
  k.first_child = [](Handle n) { return h(call(63, {u(n)})); };
  k.add_child = [](Handle p, Handle c) { call(1, {u(p), u(c)}); };
  k.remove_child = [](Handle p, Handle c) { call(2, {u(p), u(c)}); };
  k.obj_add_child = [](Handle p, Handle c) { call(3, {u(p), u(c)}); };
  k.obj_remove_child = [](Handle p, Handle c) { call(4, {u(p), u(c)}); };
  k.clone = [](Handle n) { return h(call(5, {u(n)})); };
  k.link = [](Handle n) { return h(call(64, {u(n)})); };
  k.set_link = [](Handle n, Handle v) { call(65, {u(n), u(v)}); };
  k.set_shootable = [](Handle n, Projectile* p) { call(66, {u(n), ptok(p)}); };
  k.clear_handler = [](Handle n) { call(67, {u(n)}); };
  k.node_frame = [](Handle n) { return call(68, {u(n)}); };
  k.node_flags = [](Handle n) { return static_cast<uint32_t>(call(74, {u(n)})); };
  k.parent = [](Handle n, int32_t i) { return h(call(80, {u(n), i})); };
  k.child = [](Handle n, int32_t i) { return h(call(81, {u(n), i})); };
  k.handler = [](Handle n) -> uintptr_t {
    const uint32_t v = static_cast<uint32_t>(call(69, {u(n)}));
    for (zwep::NodeHandlers* t : g_tables)
      if (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t)) == v) return reinterpret_cast<uintptr_t>(t);
    return v;
  };
  k.set_handler = [](Handle n, uintptr_t v) {
    auto* t = reinterpret_cast<zwep::NodeHandlers*>(v);
    if (v > 1 && std::find(g_tables.begin(), g_tables.end(), t) == g_tables.end()) g_tables.push_back(t);
    call(82, {u(n), static_cast<int32_t>(static_cast<uint32_t>(v))});
  };
  k.set_flag_4eb790 = [](Handle n, int32_t on) { call(79, {u(n), on}); };
  k.crater_defaults = [](zwep::CraterOrder* c) {
    int32_t o[11];
    call(75, {}, o);
    std::memcpy(&c->x00, &o[0], 4), std::memcpy(&c->points, &o[1], 4), c->material = h(o[2]);
    std::memcpy(&c->x0c, &o[3], 4), c->depth = bf(o[4]), c->radius = bf(o[5]);
    c->at = {bf(o[6]), bf(o[7]), bf(o[8])};
    c->owner = h(o[9]);
  };
  k.crater_make = [](const zwep::CraterOrder* c) {
    return call(76, {static_cast<int32_t>(c->x00), c->points, u(c->material), static_cast<int32_t>(c->x0c), fb(c->depth),
                     fb(c->radius), fb(c->at.x), fb(c->at.y), fb(c->at.z), u(c->owner)});
  };
  k.quicksand_defaults = [](zwep::QuicksandOrder* q) {
    int32_t o[11];
    call(77, {}, o);
    std::memcpy(&q->x00, &o[0], 4), std::memcpy(&q->points, &o[1], 4);
    q->cycle = h(o[2]), q->still = h(o[3]);
    std::memcpy(&q->x10, &o[4], 4);
    q->depth = bf(o[5]), q->radius = bf(o[6]);
    q->at = {bf(o[7]), bf(o[8]), bf(o[9])};
    q->owner = h(o[10]);
  };
  k.quicksand_make = [](const zwep::QuicksandOrder* q) {
    call(78, {static_cast<int32_t>(q->x00), q->points, u(q->cycle), u(q->still),
              static_cast<int32_t>(q->x10), fb(q->depth), fb(q->radius), fb(q->at.x), fb(q->at.y), fb(q->at.z),
              u(q->owner)});
  };
  k.set_active = [](Handle n, int32_t on) { call(52, {u(n), on}); };
  k.set_name = [](Handle n, const char* s) {
    uint32_t x = 2166136261u;
    for (; s && *s; ++s) x = (x ^ static_cast<uint8_t>(*s)) * 16777619u;
    call(14, {u(n), static_cast<int32_t>(x)});
  };
  k.set_flag_4eb620 = [](Handle n, int32_t on) { call(15, {u(n), on}); };
  k.set_flag_4eb750 = [](Handle n, int32_t on) { call(16, {u(n), on}); };
  k.node_free = [](Handle n) { call(17, {u(n)}); };
  k.node_destroy = [](Handle n) { call(18, {u(n)}); };
  k.pool_live = [] { return call(19, {}) != 0; };
  k.object3d_create = [] { return h(call(13, {})); };
  k.set_scale = [](Handle n, float x, float y, float z) { call(9, {u(n), fb(x), fb(y), fb(z)}); };
  k.set_rotation = [](Handle n, float x, float y, float z) { call(10, {u(n), fb(x), fb(y), fb(z)}); };
  k.set_translation = [](Handle n, float x, float y, float z) { call(11, {u(n), fb(x), fb(y), fb(z)}); };
  k.rotate = [](Handle n, float x, float y, float z) { call(12, {u(n), fb(x), fb(y), fb(z)}); };
  k.light_create = [] { return h(call(20, {})); };
  k.light_position = [](Handle l, float r, float g, float b) { call(21, {u(l), fb(r), fb(g), fb(b)}); };
  k.light_range = [](Handle l, float a, float b) { call(22, {u(l), fb(a), fb(b)}); };
  k.light_color = [](Handle l, float x, float y, float z) { call(23, {u(l), fb(x), fb(y), fb(z)}); };
  k.world_add_light = [](Handle l) { call(24, {u(l)}); };
  k.world_remove_light = [](Handle l) { call(25, {u(l)}); };
  k.anim_attached = [](Handle a, Handle n, const void* off) { return h(call(26, {u(a), u(n), u(off)})); };
  k.anim_follow = [](Handle a, Handle n) { return h(call(27, {u(a), u(n)})); };
  k.anim_model = [](Handle a, Handle m) { return h(call(28, {u(a), u(m)})); };
  k.anim_on_done = [](Handle a, Projectile* p) { call(29, {u(a), ptok(p)}); };
  k.anim_stop = [](Handle a) { call(30, {u(a)}); };
  k.sound_at = [](Handle s, const Vec3f3* at, float vol) {
    return h(call(33, {u(s), fb(at->x), fb(at->y), fb(at->z), fb(vol)}));
  };
  k.impact_type = [](int32_t t) { call(34, {t}); };
  k.effect_handle = [](Handle a, const float* x) {
    return h(call(31, {u(a), fb(x[0]), fb(x[1]), fb(x[2]), fb(x[3]), fb(x[4]), fb(x[5]), fb(x[6]), fb(x[7]), fb(x[8])}));
  };
  k.set_flag_4fbb90 = [](Handle n, int32_t on) { call(53, {u(n), on}); };
  k.set_uniform_scale = [](Handle n, float s) { call(54, {u(n), fb(s)}); };
  k.sound_stop = [](Handle s) { call(55, {u(s)}); };
  k.sound_frequency = [](Handle s, float v) { call(56, {u(s), fb(v)}); };
  k.anim_free = [](Handle a) { call(57, {u(a)}); };
  k.query_static = [](int32_t on) { call(35, {on}); };
  k.query_since = [](int32_t on, uint32_t f) { call(36, {on, static_cast<int32_t>(f)}); };
  k.query_mask = [](uint32_t m) { call(37, {static_cast<int32_t>(m)}); };
  k.segment_list = [](const Vec3f3* a, const Vec3f3* b, HitList* out) {
    int32_t buf[400];
    std::vector<int32_t> v;
    v3(v, *a);
    v3(v, *b);
    v.push_back(static_cast<int32_t>(zwep::run_state().zone));
    const int32_t r = callv(38, v, buf);
    const uint8_t total = out->total;  // 0x004eed90 writes only the count byte
    read_list(buf, out);
    out->total = total;
    return r;
  };
  k.segment = [](const Vec3f3* a, const Vec3f3* b, HitList* out) {
    int32_t buf[400];
    std::vector<int32_t> v;
    v3(v, *a);
    v3(v, *b);
    v.push_back(static_cast<int32_t>(zwep::run_state().zone));
    const int32_t r = callv(39, v, buf);
    read_list(buf, out);
    return r;
  };
  k.ground = [](const Vec3f3* at, HitList* out) {
    int32_t buf[400];
    std::vector<int32_t> v;
    v3(v, *at);
    callv(40, v, buf);
    read_list(buf, out);
  };
  zwep::HitHooks& t = zwep::hit_hooks();
  t = zwep::HitHooks{};
  t.surface_mark = [](HitRecord* r) { callv(45, rec_args(r)); };
  t.node_handler = [](Handle n) -> uintptr_t { return static_cast<uint32_t>(call(69, {u(n)})); };
  t.node_projectile = [](Handle n) { return pfrom(call(70, {u(n)})); };
  t.handler_live = [](uintptr_t hd) { return call(72, {static_cast<int32_t>(hd)}) != 0; };
  t.call_handler = [](uintptr_t hd, zwep::WeaponDef* d, HitRecord* r, float dmg) -> uint32_t {
    std::vector<int32_t> v{static_cast<int32_t>(hd), dtok(d)};
    const auto ra = rec_args(r);
    v.insert(v.end(), ra.begin(), ra.end());
    v.push_back(fb(dmg));
    int32_t out[2];
    const uint32_t res = static_cast<uint32_t>(callv(44, v, out));
    if (out[0]) zwep::note_kill(h(out[0]), r);
    zwep::set_health_after(bf(out[1]));
    return res;
  };
  t.soil = [](Handle m) { return call(73, {u(m)}); };
  t.effect = [](Handle a, const float* x) {
    call(31, {u(a), fb(x[0]), fb(x[1]), fb(x[2]), fb(x[3]), fb(x[4]), fb(x[5]), fb(x[6]), fb(x[7]), fb(x[8])});
  };
  t.impact_effect = [](int32_t e, const Vec3f3* at) { call(32, {e, fb(at->x), fb(at->y), fb(at->z)}); };
  t.surface_angles = [](const Vec3f3* n, float out[3]) {
    int32_t o[3];
    call(49, {fb(n->x), fb(n->y), fb(n->z)}, o);
    for (int i = 0; i < 3; ++i) out[i] = bf(o[i]);
  };
  t.node_intersect = [](Handle n) { return call(71, {u(n)}) != 0; };
  t.set_intersect = [](Handle n, int32_t on) { call(6, {u(n), on}); };
  t.query = [](Handle n, const Vec3f3* at, float radius, HitList* out) {
    int32_t buf[400];
    const int32_t r = call(43, {u(n), fb(at->x), fb(at->y), fb(at->z), fb(radius)}, buf);
    read_list(buf, out);
    return r;
  };

  mech3::WeaponCalls& c = mech3::weapon_calls();
  c = mech3::WeaponCalls{};
  c.spawn = [](zwep::WeaponDef* d, Handle node, Handle x64, const mech3::Vec3f* m, const mech3::Vec3f* dir,
               const mech3::Vec3f* v, int32_t p5, int32_t p6, bool) -> Handle {
    const Vec3f3 mm{m->x, m->y, m->z}, dd{dir->x, dir->y, dir->z}, vv{v->x, v->y, v->z};
    return zwep::spawn(d, node, static_cast<const uint32_t*>(x64), &mm, &dd, &vv, p5,
                       reinterpret_cast<Projectile*>(static_cast<uintptr_t>(p6)));
  };
}

}  // namespace

extern "C" {

void probe_fly_setup(PyFn py) {
  g_py = py;
  set_hooks();
  zwep::run_state() = zwep::RunState{};
  zwep::hit_state() = zwep::HitState{};
  zwep::fire_state() = zwep::FireState{};
  mech3::radar_clear();
  for (auto& b : g_beams) b = nullptr;
  g_tables.clear();
}

// The globals an op starts from: dt, frame, net clock, zone, init arg, the
// fire state (damage, track x2, terrain, lock flag set, lock values), the hit
// state (record_shot, splash, counted, hits), the tether ceiling, the clock.
void probe_fly_globals(const int32_t* g) {
  zwep::HitState& hs = zwep::hit_state();
  zwep::RunState& rs = zwep::run_state();
  zwep::FireState& f = zwep::fire_state();
  hs.dt = bf(g[0]);
  rs.frame = static_cast<uint32_t>(g[1]);
  rs.net_clock = bf(g[2]);
  rs.zone = static_cast<uint32_t>(g[3]);
  zwep::state().init_arg = g[4];
  f.damage_scale = bf(g[5]);
  f.track_scale[0] = bf(g[6]);
  f.track_scale[1] = bf(g[7]);
  f.terrain_follow = g[8];
  g_lock_flag = g[10];
  for (int i = 0; i < 8; ++i) {
    const int32_t* e = i == 0 ? g + 11 : g + 24 + 7 * (i - 1);
    g_lock_at[i] = {bf(e[1]), bf(e[2]), bf(e[3])};
    g_lock_vel[i] = {bf(e[4]), bf(e[5]), bf(e[6])};
    g_locks[i] = {h(e[0]), &g_lock_at[i], &g_lock_vel[i]};
  }
  if (g[9]) {
    f.lock_flag = &g_lock_flag;
    f.lock_target = g_locks;
  } else {
    f.lock_flag = nullptr;
    f.lock_target = nullptr;
  }
  hs.record_shot = g[18];
  hs.splash_enabled = g[19];
  hs.counted = h(g[20]);
  hs.hits = g[21];
  hs.tether_ceiling = bf(g[22]);
  mech3::weapon_world().clock = bf(g[23]);
  f.beam_range_scale = bf(g[73]);
  f.beam_ground_fx = g[74];
  hs.killed = nullptr;
  hs.killed_at = nullptr;
}

// op 0 spawn (def, owner, zone set?, zone, muzzle3, dir3, vel3, net id, slot),
// 1 tick, 2 detonate (def, at?, at3, owner), 3 burst_at (def, at3), 4 kill_all,
// 5 pool_objects(arg), 6 aim_slope (def, from3, to3), 7 pool_stats, 8 a
// shot's tick callback on (slot), 9 light_take (at3), 10 light_return (light),
// 11 kill (def, slot), 12 mine_idle-free: sound (def, soil, rec3, vol),
// 13 bounce_sound. Returns a result word; `out` gets extra results.
int32_t probe_fly_op(int op, const int32_t* a, uint32_t seed, int32_t* out) {
  crt::msvc_srand(seed);
  auto def = [](int32_t i) { return &zwep::state().defs[static_cast<size_t>(i)]; };
  switch (op) {
    case 0: {
      const uint32_t zone = static_cast<uint32_t>(a[3]);
      const Vec3f3 m{bf(a[4]), bf(a[5]), bf(a[6])}, d{bf(a[7]), bf(a[8]), bf(a[9])}, v{bf(a[10]), bf(a[11]), bf(a[12])};
      return ptok(zwep::spawn(def(a[0]), h(a[1]), a[2] ? &zone : nullptr, &m, &d, &v, a[13], pfrom(a[14])));
    }
    case 1: zwep::tick(); return 0;
    case 2: {
      const Vec3f3 at{bf(a[2]), bf(a[3]), bf(a[4])};
      return zwep::detonate(def(a[0]), a[1] ? &at : nullptr, h(a[5]));
    }
    case 3: {
      const Vec3f3 at{bf(a[1]), bf(a[2]), bf(a[3])};
      return zwep::burst_at(def(a[0]), &at);
    }
    case 4: zwep::kill_all(); return 0;
    case 5: return zwep::pool_objects(a[0]);
    case 6: {
      const Vec3f3 f{bf(a[1]), bf(a[2]), bf(a[3])}, t{bf(a[4]), bf(a[5]), bf(a[6])};
      float dist = 0;
      const float r = zwep::aim_slope(def(a[0]), &f, &t, &dist);
      out[0] = fb(dist);
      return fb(r);
    }
    case 7: {
      zwep::PoolStats s;
      zwep::pool_stats(&s);
      out[0] = s.size, out[1] = s.free, out[2] = s.flying, out[3] = s.x0c;
      return 0;
    }
    case 8: {  // a shot's tick: 0 the test's, 1 the radar's friend, 2 foe
      Projectile* p = pfrom(a[0]);
      p->tick = a[1] == 1 ? mech3::radar_friend : a[1] == 2 ? mech3::radar_foe : tick_cb;
      p->tick_arg = reinterpret_cast<uintptr_t>(p);
      return 0;
    }
    case 9: {
      const Vec3f3 at{bf(a[0]), bf(a[1]), bf(a[2])};
      return u(zwep::light_take(&at));
    }
    case 10: zwep::light_return(h(a[0])); return 0;
    case 11: {
      zwep::WeaponDef* d = def(a[0]);
      Projectile* p = pfrom(a[1]);
      d->active = p->next;  // off the list first, as the callers do
      zwep::kill(d, p);
      return 0;
    }
    case 12:
    case 13: {
      HitRecord r;
      r.point = {bf(a[2]), bf(a[3]), bf(a[4])};
      if (op == 12) zwep::sound(def(a[0]), &r, a[1], bf(a[5]));
      else zwep::bounce_sound(def(a[0]), &r, a[1], bf(a[5]));
      return 0;
    }
    case 15: {  // the intercept 0x00524247
      const zmth::Vec3 f{bf(a[0]), bf(a[1]), bf(a[2])}, t{bf(a[3]), bf(a[4]), bf(a[5])}, v{bf(a[7]), bf(a[8]), bf(a[9])};
      zmth::Vec3 o{0, 0, 0};
      const int32_t r = zmth::intercept(f, t, bf(a[6]), v, &o);
      out[0] = fb(o.x), out[1] = fb(o.y), out[2] = fb(o.z);
      return r;
    }
    case 16: {  // the slerp 0x0051e460
      const zmth::Vec3 x{bf(a[0]), bf(a[1]), bf(a[2])}, y{bf(a[3]), bf(a[4]), bf(a[5])};
      zmth::Vec3 o;
      zmth::slerp(x, y, bf(a[6]), &o);
      out[0] = fb(o.x), out[1] = fb(o.y), out[2] = fb(o.z);
      return 0;
    }
    default: return -1;
  }
}

// The free list cut to its first n slots (the rest are lost, both sides).
void probe_fly_trim(int n) {
  Projectile* p = zwep::state().free_list;
  for (int i = 1; p && i < n; ++i) p = p->next;
  if (p) p->next = nullptr;
}

// Data no shipped weapon has, put into a def: 0 its flags, 1 lead min, 2 lead
// max, 3 lead span, 4 gravity, 5 flyout model, 6 flyout model animation,
// 7 flyout animation, 8 flyout attached, 9 destroy animation, 10 fire effect,
// 11 fire animation choice, 12 fire attach node, 13 flyout health, 14 range,
// 15 impact proximity (and its square), 16 acceleration.
void probe_fly_poke(int i, int which, int32_t v) {
  zwep::WeaponDef& d = zwep::state().defs[static_cast<size_t>(i)];
  switch (which) {
    case 0: d.flags = static_cast<uint32_t>(v); break;
    case 1: d.lock_on_lead_min = bf(v); break;
    case 2: d.lock_on_lead_max = bf(v); break;
    case 3: d.lock_on_lead_span = bf(v); break;
    case 4: d.gravity = bf(v); break;
    case 5: d.flyout.model = h(v); break;
    case 6: d.flyout.model_animation = h(v); break;
    case 7: d.flyout.animation = h(v); break;
    case 8: d.flyout.animation_attached = h(v); break;
    case 9: d.destroy_animation = h(v); break;
    case 10: d.fire.effect = v; break;
    case 11: d.fire.animation_choice = v; break;
    case 12: d.fire.attach_node = h(v); break;
    case 13: d.flyout_health = bf(v); break;
    case 14: d.range = bf(v); break;
    case 15: d.impact_proximity = bf(v), d.impact_proximity_sq = bf(v) * bf(v); break;
    case 16: d.acceleration = bf(v); break;
    case 19: d.fire.flags = static_cast<uint32_t>(v); break;
    case 20: d.velocity = bf(v); break;
    case 21: d.crater_span.points = v; break;
    case 22: d.crater.points = v; break;
    case 23: d.detonation_distance_sq = bf(v); break;
    case 17:
      for (int k = 0; k < zwep::kSoilCount; ++k) {
        d.impacts[k].bounce_sound_count = 2;
        d.impacts[k].bounce_sounds[0] = h(v);
        d.impacts[k].bounce_sounds[1] = h(v + 1);
      }
      break;
    case 18: d.turn_suspend_time = bf(v); break;
    default: break;
  }
}

// Beams: 0 make (def, owner, zone set, zone, slot, segments) -> index,
// 1 start, 2 stop, 3 free (index).
int32_t probe_fly_beam(int op, const int32_t* a) {
  switch (op) {
    case 0: {
      const int slot = a[4];
      g_beam_zone[slot] = static_cast<uint32_t>(a[3]);
      g_beams[slot] = zwep::beam_make(&zwep::state().defs[static_cast<size_t>(a[0])], h(a[1]),
                                      a[2] ? &g_beam_zone[slot] : nullptr, &g_beam_at[slot], &g_beam_dir[slot], a[5]);
      return 0x50000 + slot;
    }
    case 1: return zwep::beam_start(g_beams[a[0]]);
    case 2: return zwep::beam_stop(g_beams[a[0]]);
    case 4:  // set a beam's length (by its reach less a[1]) and designate timer
      g_beams[a[0]]->length = g_beams[a[0]]->range_scale * g_beams[a[0]]->def->range - bf(a[1]);
      g_beams[a[0]]->timer = bf(a[2]);
      return 0;
    case 3: {
      const int32_t r = zwep::beam_free(g_beams[a[0]]);
      g_beams[a[0]] = nullptr;
      return r;
    }
    default: return -1;
  }
}

void probe_fly_beam_at(int slot, const int32_t* v) {
  g_beam_at[slot] = {bf(v[0]), bf(v[1]), bf(v[2])};
  g_beam_dir[slot] = {bf(v[3]), bf(v[4]), bf(v[5])};
}

// A beam's fields (test_weapons_fly.py, beam_fields).
int probe_fly_beam_dump(int slot, int32_t* o) {
  const zwep::Beam* b = g_beams[slot];
  int n = 0;
  auto put = [&](int32_t v) { o[n++] = v; };
  put(btok(b->next));
  put(btok(b->prev));
  put(b->def->index);
  put(u(b->owner));
  put(b->zone ? 1 : 0);
  put(u(b->sound));
  put(u(b->light));
  for (float x : {b->length, b->timer, b->age, b->damage_scale, b->range_scale}) put(fb(x));
  put(b->segment_count);
  put(b->active);
  for (const zwep::BeamSegment& sg : b->segments) {
    put(u(sg.node));
    for (float x : {sg.start.x, sg.start.y, sg.start.z, sg.dir.x, sg.dir.y, sg.dir.z, sg.length}) put(fb(x));
  }
  put(b->lock_count == &g_lock_flag ? 1 : b->lock_count ? -2 : 0);
  put(b->targets == g_locks ? 1 : b->targets ? -2 : 0);
  put(static_cast<int32_t>(b->flags));
  return n;
}

// The lock array after an op (a MULTI_TARGET beam sorts it): entity, at token.
void probe_fly_locks(int32_t* o) {
  for (int i = 0; i < 8; ++i) {
    o[2 * i] = u(g_locks[i].entity);
    o[2 * i + 1] = ltok(g_locks[i].at);
  }
}

// The radar list: shot token, friendly.
int probe_fly_radar(int32_t* o) {
  int n = 0;
  for (const mech3::RadarBlip* b : mech3::radar_blips()) {
    o[n++] = ptok(b->shot);
    o[n++] = b->friendly ? 1 : 0;
  }
  return n;
}

// The handler ops: 0 damage handler (obj, node, fn 1/2/3, flag), 1 touch
// handler (obj, node, fn), 2 remove (node), 3 owns (node), 4 touch (from3,
// rec point3, rec node, damage).
int32_t probe_fly_handler(int op, const int32_t* a) {
  auto fn = [](int32_t t) -> void (*)() {
    return t == 1 ? reinterpret_cast<void (*)()>(touch_a) : t == 2 ? reinterpret_cast<void (*)()>(touch_b)
                                                                   : reinterpret_cast<void (*)()>(damage_a);
  };
  switch (op) {
    case 0: return zwep::set_damage_handler(h(a[0]), h(a[1]), reinterpret_cast<zwep::DamageFn>(fn(a[2])), a[3]);
    case 1: return zwep::set_touch_handler(h(a[0]), h(a[1]), reinterpret_cast<zwep::TouchFn>(fn(a[2])));
    case 2: return zwep::remove_handlers(h(a[0]));
    case 3: return zwep::own_handler(h(a[0])) ? 1 : 0;
    case 4: {
      HitRecord& r = g_touch_rec;
      r = HitRecord{};
      const Vec3f3 from{bf(a[0]), bf(a[1]), bf(a[2])};
      r.point = {bf(a[3]), bf(a[4]), bf(a[5])};
      r.node = h(a[6]);
      zwep::touch(&from, &r, bf(a[7]));
      return 0;
    }
    default: return -1;
  }
}

// The AMS: 0 make (range, period, rounds), 1 find (side, at3), 2 fire (count).
// out: on, range, next, period, target token, rounds, target position token.
int32_t probe_fly_ams(int op, const int32_t* a, int32_t* out) {
  static mech3::Ams* ams = nullptr;
  int32_t r = 0;
  if (op == 0) {
    delete ams;
    ams = new mech3::Ams(bf(a[0]), bf(a[1]), a[2]);
  } else if (op == 1) {
    const Vec3f3 at{bf(a[1]), bf(a[2]), bf(a[3])};
    r = ams->find(a[0], &at) ? 1 : 0;
  } else if (op == 2) {
    ams->fire(a[0]);
  }
  const Vec3f3* t = ams->target_at();
  const int32_t v[] = {ams->on ? 1 : 0, fb(ams->range), fb(ams->next), fb(ams->period), ptok(ams->target), ams->rounds,
                       t ? ptok(ams->target) : 0};
  std::memcpy(out, v, sizeof v);
  return r;
}

// A table the port made, by its low 32 bits: obj, fn token for both slots; -1 unknown.
int probe_fly_table(int32_t v, int32_t* o) {
  for (const zwep::NodeHandlers* t : g_tables) {
    if (static_cast<int32_t>(reinterpret_cast<uintptr_t>(t)) != v) continue;
    for (int i = 0; i < 2; ++i) o[2 * i] = u(t->slot[i].object), o[2 * i + 1] = fn_tok(t->slot[i].fn);
    return 4;
  }
  return -1;
}

// The network callbacks [0x0072fcac] and [0x0072fcb4] on or off.
void probe_fly_net(int gate, int det) {
  zwep::run_hooks().net_gate = gate ? net_gate : nullptr;
  zwep::run_hooks().net_detonated = det ? net_detonated : nullptr;
}

void probe_fly_seed(uint32_t seed) { crt::msvc_srand(seed); }

void probe_fly_choice(int i, int32_t sound, int32_t flyout) {
  zwep::WeaponDef& d = zwep::state().defs[static_cast<size_t>(i)];
  d.fire.sound_choice = sound;
  d.flyout.animation_choice = flyout;
}

// A slot's fields, as the test reads them from the emulator (pointers as
// tokens or handles): see test_weapons_fly.py, slot_fields.
int probe_fly_slot(int i, int32_t* o) {
  const Projectile& p = zwep::state().pool[static_cast<size_t>(i)];
  int n = 0;
  auto put = [&](int32_t v) { o[n++] = v; };
  auto putv = [&](const Vec3f3& v) { put(fb(v.x)), put(fb(v.y)), put(fb(v.z)); };
  put(ptok(p.next));
  put(u(p.owner));
  put(static_cast<int32_t>(p.zone));
  put(u(p.node));
  put(u(p.flyout_anim));
  put(u(p.flyout_attached));
  put(u(p.model_anim));
  put(u(p.model));
  put(p.state);
  putv(p.spawn_at);
  putv(p.base_velocity);
  putv(p.direction);
  putv(p.position);
  putv(p.next_position);
  putv(p.velocity);
  put(u(p.lock_entity));
  put(ltok(p.target_at));
  put(ltok(p.target_velocity));
  put(p.static_hits);
  put(static_cast<int32_t>(p.static_frame));
  put(p.statics.count);
  put(p.statics.total);
  for (int k = 0; k < p.statics.count && k < 31; ++k) {
    const HitRecord& r = p.statics.rec[k];
    putv(r.point);
    put(fb(r.distance_sq));
    put(u(r.node));
  }
  put(p.static_next);
  put(fb(p.homing_time));
  put(fb(p.speed));
  put(fb(p.max_speed));
  putv(p.deviation);
  putv(p.drift);
  put(fb(p.travelled));
  put(fb(p.age));
  put(fb(p.health));
  put(fb(p.damage_scale));
  put(fb(p.track_scale[0]));
  put(fb(p.track_scale[1]));
  put(p.terrain_follow);
  put(p.net_id);
  put(p.tick == tick_cb ? 1 : p.tick == mech3::radar_friend ? 2 : p.tick == mech3::radar_foe ? 3 : p.tick ? -2 : 0);
  put(ptok(reinterpret_cast<const Projectile*>(p.tick_arg)));
  return n;
}

// The weapon-wide state: free list, each def's active list and model pool,
// fire/flyout choices, attach node, terrain height; the fire, hit and run
// state.
int probe_fly_world(int32_t* o) {
  int n = 0;
  auto put = [&](int32_t v) { o[n++] = v; };
  const zwep::State& s = zwep::state();
  for (const Projectile* p = s.free_list; p; p = p->next) put(ptok(p));
  put(-1);
  for (const zwep::WeaponDef& d : s.defs) {
    if (!(d.flags & zwep::kBeam))  // a beam def's list holds Beams: dumped apart
      for (const Projectile* p = d.active; p; p = p->next) put(ptok(p));
    put(-1);
    put(u(d.model_pool));
    put(d.fire.sound_choice);
    put(d.fire.animation_choice);
    put(d.flyout.animation_choice);
    put(u(d.fire.attach_node));
    put(fb(d.terrain_height));
  }
  const zwep::FireState& f = zwep::fire_state();
  put(f.lock_flag ? 1 : 0);
  put(fb(f.damage_scale));
  put(fb(f.track_scale[0]));
  put(fb(f.track_scale[1]));
  put(f.terrain_follow);
  const zwep::HitState& hs = zwep::hit_state();
  for (float x : {hs.shot_from.x, hs.shot_from.y, hs.shot_from.z, hs.shot_to.x, hs.shot_to.y, hs.shot_to.z})
    put(fb(x));
  put(u(hs.attacker));
  put(hs.record_shot);
  put(hs.splash_enabled);
  put(u(hs.killed));
  put(fb(hs.health_after));
  put(hs.hits);
  const zwep::RunState& rs = zwep::run_state();
  put(static_cast<int32_t>(rs.zone));
  put(static_cast<int32_t>(rs.tick_zone));
  put(rs.owner_hidden);
  put(rs.pool_objects);
  put(u(rs.lights));
  return n;
}

}  // extern "C"
