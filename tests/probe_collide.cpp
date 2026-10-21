// Mech collision (mech3/mech_collide) for tests/differential/test_collide.py.
// The mech goes back and forth as a byte image; other mechs, vehicles,
// nodes and contacts are small tables the test fills; the segment query
// answers with the records the test scripted. Every call out is logged as
// (count, code, args...) on both sides. Handles: node i is 0x100 + i,
// vehicle i 0x200 + i, contact i 0x300 + i.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zclass/cls_node.h"
#include "mech3/mech.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_move.h"

using namespace mech3;

namespace {
template <typename T> T rd(const uint8_t* b, int off) {
  T v;
  std::memcpy(&v, b + off, sizeof v);
  return v;
}
template <typename T> void wr(uint8_t* b, int off, T v) { std::memcpy(b + off, &v, sizeof v); }
Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
uint32_t u(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
int32_t bits(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}

std::vector<int32_t> g_log;
void log(std::initializer_list<int32_t> v) {
  g_log.push_back(static_cast<int32_t>(v.size()));
  g_log.insert(g_log.end(), v);
}

struct LogPart {
  int code;
};
struct LTorso : Torso {
  int code = 1;
  void apply_damage(float d, int s) override { log({20, code, bits(d), s}); }
};
struct LHead : Head {
  void apply_damage(float d, int s) override { log({20, 2, bits(d), s}); }
};
struct LLeg : Leg {
  int code = 3;
  void apply_damage(float d, int s) override { log({20, code, bits(d), s}); }
};

// The test's tables, filled by probe_collide_setup.
struct Node {
  uint32_t kind = 0;      // 0 plain, 1 mech (other = idx), 2 vehicle, 3 touchable
  int32_t idx = 0;
  float touch = 0;        // what its damage handler returns
  int32_t has_box = 0;
  float box[6] = {};
};
struct Vehicle {
  int32_t alive = 1, target = 1, body = 1, killed = 0;
  uint32_t team = 0;
  float velocity[3] = {}, spin[3] = {}, mass = 0, matrix[12] = {};
};
struct Contact {
  int32_t mech = -1;      // -1 none, 0 self, 1..2 others (index + 1)
  int32_t target = 0;
  float dist2 = 0, dist = 0;
  Vec3 dir{};
};

struct State {
  Mech m;
  MechControls c;
  MechData pilot;
  Skis skis;
  LTorso torso;
  LHead head;
  Hip hip;
  LLeg legs[2];
  Foot feet[2];
  Vec3 foot_pos;
  Mech others[2];
  MechData other_pilot[2];
  LTorso other_torso[2];
  int32_t other_remote[2] = {};
  Node nodes[12];
  Vehicle veh[2];
  Contact contacts[4];
  int32_t ncontacts = 0;
  int32_t ai = 0, ai_reverses = 0;
  Vec3 pos;
  float transform[12];
  NetCollision net;
  // two scripted query answers: per segment a count and hits (node index or -1)
  zcls::SegmentHits recs[2][12];
  int32_t calls = 0;
  uint32_t mech_node_flags = 0;
};
State* g = nullptr;

int32_t node_of(Handle n) {
  const uint32_t v = u(n);
  return v >= 0x100 && v < 0x10c ? static_cast<int32_t>(v - 0x100) : -1;
}
}  // namespace

extern "C" {

// The tables: nodes (kind, idx, touch, has_box, box[6]) x 12; vehicles x 2
// (alive, target, body, killed, team, velocity[3], spin[3], mass,
// matrix[12]); contacts x 4 (mech, target, dist2, dist, dir[3]).
void probe_collide_setup(const float* nodes, const float* veh, const float* contacts, int32_t ncontacts,
                         const int32_t* others_roster, const uint32_t* others_team, const float* others_vel,
                         const int32_t* others_remote, int32_t ai, int32_t ai_rev) {
  static State state;
  g = &state;
  for (int i = 0; i < 12; ++i) {
    const float* n = nodes + 10 * i;
    Node& d = g->nodes[i];
    d.kind = static_cast<uint32_t>(n[0]), d.idx = static_cast<int32_t>(n[1]), d.touch = n[2];
    d.has_box = static_cast<int32_t>(n[3]);
    std::memcpy(d.box, n + 4, 24);
  }
  for (int i = 0; i < 2; ++i) {
    const float* v = veh + 24 * i;
    Vehicle& d = g->veh[i];
    d.alive = static_cast<int32_t>(v[0]), d.target = static_cast<int32_t>(v[1]), d.body = static_cast<int32_t>(v[2]);
    d.killed = static_cast<int32_t>(v[3]);
    std::memcpy(&d.team, v + 4, 4);
    std::memcpy(d.velocity, v + 5, 12), std::memcpy(d.spin, v + 8, 12);
    d.mass = v[11];
    std::memcpy(d.matrix, v + 12, 48);
  }
  g->ncontacts = ncontacts;
  for (int i = 0; i < ncontacts; ++i) {
    const float* c = contacts + 8 * i;
    Contact& d = g->contacts[i];
    d.mech = static_cast<int32_t>(c[0]), d.target = static_cast<int32_t>(c[1]), d.dist2 = c[2], d.dist = c[3];
    d.dir = {c[4], c[5], c[6]};
  }
  for (int i = 0; i < 2; ++i) {
    Mech& o = g->others[i];
    o = Mech{};
    g->other_pilot[i].roster = others_roster[i];
    o.pilot = &g->other_pilot[i];
    o.team = others_team[i];
    o.velocity = {others_vel[3 * i], others_vel[3 * i + 1], others_vel[3 * i + 2]};
    g->other_torso[i] = LTorso{};
    g->other_torso[i].code = 5 + i;
    o.torso = &g->other_torso[i];
    o.attacker = nullptr;
    g->other_remote[i] = others_remote[i];
  }
  g->ai = ai, g->ai_reverses = ai_rev;
}

// One query answer: call index, segment, its hits as (node index or -1,
// normal[3], point[3]) x count.
void probe_collide_record(int32_t call, int32_t seg, int32_t count, const float* hits) {
  zcls::SegmentHits& r = g->recs[call][seg];
  r.count = static_cast<uint8_t>(count);
  for (int i = 0; i < count; ++i) {
    const float* x = hits + 7 * i;
    zcls::SegmentHit& s = r.hit[i];
    std::memset(&s, 0, sizeof s);
    const int n = static_cast<int>(x[0]);
    s.node = n < 0 ? nullptr : static_cast<zcls::Node*>(h(0x100 + n));
    std::memcpy(s.normal, x + 1, 12);
    std::memcpy(s.point, x + 4, 12);
  }
}

// Mech image fields (offsets as in the exe) in and out; returns Collide's result.
// `flags`: 1 viewed player, 2 mission player, 4 stats, 8 a network record.
int32_t probe_collide(uint8_t* mech, int32_t roster, float half, float* transform, float* pos, uint32_t flags,
                      const uint8_t* net, uint32_t frame, float* others_out, float* veh_out, float* skis_vy,
                      int32_t* out, int32_t* n) {
  Mech& m = g->m;
  m = Mech{};
  auto f = [&](int off) { return rd<float>(mech, off); };
  auto v3 = [&](int off) { return Vec3{f(off), f(off + 4), f(off + 8)}; };
  m.node = h(0x100 + 11);
  m.velocity = v3(0x30), m.vec_3c = v3(0x3c), m.vec_58 = v3(0x58);
  m.allies = rd<uint32_t>(mech, 0xc4);
  m.no_power_down = mech[0x584] != 0;
  m.push = v3(0x5a0), m.impulse = v3(0x594);
  m.max_speed = f(0x518), m.turn_rate = f(0x520), m.frame_dt = f(0x6b0);
  Collider& c = m.collider;
  c.restitution = f(0x118), c.yaw_kick = f(0x11c), c.count = rd<int32_t>(mech, 0x120);
  for (int i = 0; i < 15; ++i) c.local[i] = v3(0x124 + 12 * i), c.world[i] = v3(0x1d8 + 12 * i), c.last[i] = v3(0x28c + 12 * i);
  g->c = MechControls{};
  g->c.throttle = rd<float>(mech, 0x6d0);  // the controls' current throttle, carried in a spare slot
  m.controls = &g->c;
  g->c.owner = &m;
  g->pilot.roster = roster;
  m.pilot = &g->pilot;
  g->skis = Skis{};
  g->skis.half = half;
  std::memcpy(g->skis.vy, skis_vy, 28);
  m.skis = &g->skis;
  g->torso = LTorso{};
  m.torso = &g->torso;
  m.head = &g->head;
  m.hip = &g->hip;
  for (int i = 0; i < 2; ++i) {
    g->legs[i] = LLeg{};
    g->legs[i].code = 3 + i;
    g->legs[i].foot = &g->feet[i];
    m.legs[i] = &g->legs[i];
  }
  g->foot_pos = v3(0x6c0);
  g->feet[0].translation = &g->foot_pos;
  std::memcpy(g->transform, transform, 48);
  m.transform = g->transform;
  g->pos = {pos[0], pos[1], pos[2]};
  m.sounds[13] = h(rd<uint32_t>(mech, 0x5ec));
  m.attacker = nullptr;
  if (flags & 8) {
    std::memcpy(&g->net.frame, net, 4), std::memcpy(&g->net.sent, net + 4, 4);
    g->net.on = net[8], g->net.active = net[0xb];
    std::memcpy(&g->net.normal, net + 0x24, 12), std::memcpy(&g->net.point, net + 0x30, 12);
    m.net_collision = &g->net;
  }
  zcls::frames().frame = frame;
  World& w = world();
  w.player = flags & 1 ? &m : nullptr;
  w.mission_player = flags & 2 ? &m : nullptr;
  w.stats = flags & 4;
  g->calls = 0;
  g->mech_node_flags = rd<uint32_t>(mech, 0x6d4);
  g_log.clear();

  skis_calls().position = [](Mech&) -> const Vec3* { return &g->pos; };
  MoveCalls& mc = move_calls();
  mc = MoveCalls{};
  mc.pilot_sound = [](Mech&, int id, float vol, float pitch) -> Sound {
    log({9, id, bits(vol), bits(pitch)});
    return h(0x9000);
  };
  CollideCalls& cc = collide_calls();
  cc = CollideCalls{};
  cc.segments = [](float* segs, int32_t points, zcls::SegmentHits* outr) {
    std::vector<int32_t> e{13, points};
    for (int i = 0; i < 3 * points; ++i) e.push_back(bits(segs[i]));
    g_log.push_back(static_cast<int32_t>(e.size()));
    g_log.insert(g_log.end(), e.begin(), e.end());
    const int k = g->calls < 2 ? g->calls : 1;
    for (int i = 0; i < points / 2; ++i) outr[i] = g->recs[k][i];
    ++g->calls;
  };
  cc.node_intersect = [](Handle, int on) {
    log({14, on});
    g->mech_node_flags = on ? g->mech_node_flags | 0x10 : g->mech_node_flags & ~0x10u;
  };
  cc.node_mech = [](Handle nd) -> Mech* {
    const int i = node_of(nd);
    return i >= 0 && g->nodes[i].kind == 1 ? &g->others[g->nodes[i].idx] : nullptr;
  };
  cc.node_entity = [](Handle nd) -> Handle {
    const int i = node_of(nd);
    return i >= 0 && g->nodes[i].kind == 2 ? h(0x200 + g->nodes[i].idx) : nullptr;
  };
  cc.node_touchable = [](Handle nd) {
    const int i = node_of(nd);
    return i >= 0 && g->nodes[i].kind == 3;
  };
  cc.touch = [](Handle nd, const Vec3* from, const CollisionHit&, float dmg) {
    log({12, node_of(nd), bits(from->x), bits(from->y), bits(from->z), bits(dmg)});
    return g->nodes[node_of(nd)].touch;
  };
  cc.hit_box = [](Handle nd) -> const float* {
    const int i = node_of(nd);
    return i >= 0 && g->nodes[i].has_box ? g->nodes[i].box : nullptr;
  };
  cc.set_position = [](Mech&, const Vec3* p) {
    log({15, bits(p->x), bits(p->y), bits(p->z)});
    g->pos = *p;
  };
  cc.contact_count = [](Mech&) { return g->ncontacts; };
  cc.contact = [](Mech&, int32_t i) { return h(0x300 + i); };
  cc.contact_mech = [](Handle ct) -> Mech* {
    const Contact& k = g->contacts[u(ct) - 0x300];
    return k.mech < 0 ? nullptr : k.mech == 0 ? &g->m : &g->others[k.mech - 1];
  };
  cc.contact_target = [](Handle ct) { return g->contacts[u(ct) - 0x300].target != 0; };
  cc.contact_dist2 = [](Handle ct) { return g->contacts[u(ct) - 0x300].dist2; };
  cc.contact_dist = [](Handle ct) { return g->contacts[u(ct) - 0x300].dist; };
  cc.contact_dir = [](Handle ct) -> const Vec3* { return &g->contacts[u(ct) - 0x300].dir; };
  cc.ai_reverses = [](Mech&) { return g->ai ? g->ai_reverses : 0; };
  cc.entity_dead = [](Handle e) { return g->veh[u(e) - 0x200].alive == 0; };
  cc.entity_target = [](Handle e) -> Handle { return g->veh[u(e) - 0x200].target ? h(0x400 + u(e) - 0x200) : nullptr; };
  cc.entity_body = [](Handle e) -> Handle { return g->veh[u(e) - 0x200].body ? h(0x500 + u(e) - 0x200) : nullptr; };
  cc.entity_team = [](Handle e) { return g->veh[u(e) - 0x200].team; };
  cc.entity_damage = [](Handle t, float d) {
    log({11, static_cast<int32_t>(u(t) - 0x400), bits(d)});
    return g->veh[u(t) - 0x400].killed;
  };
  cc.body_velocity = [](Handle b) { return g->veh[u(b) - 0x500].velocity; };
  cc.body_spin = [](Handle b) { return g->veh[u(b) - 0x500].spin; };
  cc.body_mass = [](Handle b) { return g->veh[u(b) - 0x500].mass; };
  cc.entity_matrix = [](Handle e) -> const float* { return g->veh[u(e) - 0x200].matrix; };
  cc.remote = [](Mech& o) { return g->other_remote[&o - g->others] != 0; };
  cc.net_shove = [](Mech&, Mech& o, float d, float vx, float vz) {
    log({10, static_cast<int32_t>(&o - g->others), bits(d), bits(vx), bits(vz)});
  };
  cc.ff_jolt = [](const float nf[3], float a) { log({8, bits(nf[0]), bits(nf[1]), bits(nf[2]), bits(a)}); };
  cc.crouch = [](Hip&, float v) { log({7, bits(v)}); };

  const int32_t r = m.collide() ? 1 : 0;

  auto put = [&](int off, float v) { wr(mech, off, v); };
  auto p3 = [&](int off, const Vec3& v) { put(off, v.x), put(off + 4, v.y), put(off + 8, v.z); };
  p3(0x30, m.velocity), p3(0x3c, m.vec_3c), p3(0x58, m.vec_58), p3(0x5a0, m.push), p3(0x594, m.impulse);
  for (int i = 0; i < 15; ++i) p3(0x1d8 + 12 * i, c.world[i]), p3(0x28c + 12 * i, c.last[i]);
  for (int k = 0; k < 4; ++k) mech[0x374 + k] = m.blocked[k];
  wr<int32_t>(mech, 0xfc, c.clear);
  wr<uint32_t>(mech, 0x5ec, u(m.sounds[13]));
  const uint32_t att = m.attacker == &g->others[0] ? 1 : m.attacker == &g->others[1] ? 2 : 0;
  wr<uint32_t>(mech, 0x664, att);
  put(0x6d0, g->c.throttle);
  wr<uint32_t>(mech, 0x6d4, g->mech_node_flags);
  std::memcpy(skis_vy, g->skis.vy, 28);
  pos[0] = g->pos.x, pos[1] = g->pos.y, pos[2] = g->pos.z;
  for (int i = 0; i < 2; ++i) {
    const Mech& o = g->others[i];
    float* x = others_out + 4 * i;
    x[0] = o.velocity.x, x[1] = o.velocity.y, x[2] = o.velocity.z;
    const uint32_t a = o.attacker == &g->m ? 1 : 0;
    std::memcpy(x + 3, &a, 4);
  }
  for (int i = 0; i < 2; ++i) std::memcpy(veh_out + 6 * i, g->veh[i].velocity, 12), std::memcpy(veh_out + 6 * i + 3, g->veh[i].spin, 12);
  w.player = w.mission_player = nullptr;
  cc = CollideCalls{};
  mc = MoveCalls{};
  skis_calls().position = nullptr;
  const int32_t len = static_cast<int32_t>(g_log.size());
  std::memcpy(out, g_log.data(), 4 * (len < *n ? len : *n));
  *n = len;
  return r;
}

}  // extern "C"

extern "C" {
// The probe setup 0x00422360 on a node box and a foot position; writes the
// local probes into the image at +0x124.
int32_t probe_collider_setup(uint8_t* mech, int32_t roster, float half, float, const float* foot, const float* box) {
  static Mech m;
  static Skis skis;
  static MechData pilot;
  static Leg leg;
  static Foot ft;
  static Vec3 fpos;
  static float b[6];
  m = Mech{};
  skis = Skis{};
  skis.half = half;
  m.skis = &skis;
  pilot.roster = roster;
  m.pilot = &pilot;
  fpos = {foot[0], foot[1], foot[2]};
  ft.translation = &fpos;
  leg.foot = &ft;
  m.legs[0] = &leg;
  m.legs[1] = &leg;
  std::memcpy(b, box, 24);
  collide_calls() = CollideCalls{};
  collide_calls().node_box = [](Handle) -> const float* { return b; };
  m.setup_collider();
  collide_calls() = CollideCalls{};
  for (int i = 0; i < 11; ++i) std::memcpy(mech + 0x124 + 12 * i, &m.collider.local[i], 12);
  return 0;
}
}
