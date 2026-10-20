#include "mech3/sensors.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "crt/msvc_sort.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {

namespace {
SensorHooks g_default;
SensorHooks* g_hooks = &g_default;
std::vector<Handle> g_entities;  // [0x0073655c..0x00736560]
SortedAllies g_allies;           // [0x0060b550], [0x0060b548]
bool g_see_all = false;          // [0x0060b570]

// The keys of the virtual calls by slot.
const char* vkey(uint32_t slot) {
  switch (slot) {
    case 0x1c: return "veh.vt+0x1c";
    case 0x24: return "veh.vt+0x24";
    case 0x28: return "veh.vt+0x28";
    case 0x2c: return "veh.vt+0x2c";
    case 0x40: return "veh.vt+0x40";
    case 0x50: return "veh.vt+0x50";
    case 0x54: return "ctl.vt+0x54";
    case 0xa0: return "veh.vt+0xa0";
    case 0xb4: return "veh.vt+0xb4";
    case 0xc8: return "veh.vt+0xc8";
    case 0xcc: return "veh.vt+0xcc";
    case 0xd0: return "veh.vt+0xd0";
    case 0xec: return "veh.vt+0xec";
    case 0x140: return "veh.vt+0x140";
    default: return "vt+?";
  }
}
}  // namespace

uint32_t SensorHooks::vcall(Handle obj, uint32_t slot, std::initializer_list<zgame::Arg> args) {
  (void)args;
  return unported(vkey(slot), {obj});
}
std::vector<Handle> SensorHooks::targets() {
  std::vector<Handle> v;
  for (const CZAiTarget* t : target_list()) v.push_back(t->self);
  return v;
}

void set_sensor_hooks(SensorHooks* h) { g_hooks = h ? h : &g_default; }
SensorHooks& sensor_hooks() { return *g_hooks; }
std::vector<Handle>& entity_list() { return g_entities; }
SortedAllies& sorted_allies() { return g_allies; }
bool& see_all() { return g_see_all; }

namespace {

SensorHooks& H() { return *g_hooks; }
uint32_t vt(Handle o, uint32_t slot) { return H().vcall(o, slot, {}); }
// vt+0x2c, as the exe asks it of an entity or a vehicle (none for null).
Handle veh(Handle o) { return o ? vt(o, 0x2c) : 0; }
// Mech3.exe 1.2 @ 0x004e6fe0: the vehicle a contact saw.
Handle seen_vehicle(const Contact* c) { return c->seen ? vt(c->seen, 0x2c) : 0; }
AiTarget& F(Handle x) {
  static AiTarget none;  // a vehicle without fields reads zeros (the exe would crash)
  AiTarget* a = H().fields(x);
  return a ? *a : none;
}

// Whether an observer with flags `a` sees a target with flags `b`, whose
// vehicle `hidden` (vt+0xb4) may hide it: written out in each caller, the
// same test every time (docs/ai.md section 15, In sight).
uint8_t sees(uint8_t a, uint8_t b, Handle hidden) {
  if (g_see_all) return 1;
  if (vt(hidden, 0xb4) & 0xff) return 0;
  if (a & 2) return 0;                               // shut down: sees nothing
  if (!(a & 1) && (a & 4) && !(b & 0x40)) return 1;  // active BAP, no ECM on it
  if (b & 2) return 0;                               // a shut down mech hides
  if ((b & 1) && (a & 1)) return 0;                  // two passive radars miss each other
  return 1;
}
// The x87's "below": NaN counts as in range.
bool in_range(float d2, const AiTarget& t) { return g_see_all || !(d2 >= t.range2); }

struct Vec {
  float x, y, z;
};
Vec centroid(Handle x) {
  float p[3];
  H().centroid(x, p);
  return {p[0], p[1], p[2]};
}
// b - a and its square, summed (x + y) + z as the x87 does.
float delta(const Vec& a, const Vec& b, Vec& d) {
  d = {b.x - a.x, b.y - a.y, b.z - a.z};
  const float xx = d.x * d.x, yy = d.y * d.y, zz = d.z * d.z;
  const float xy = xx + yy;
  return xy + zz;
}
void negate(Vec& d) { d = {-d.x, -d.y, -d.z}; }

// The contact's fields as 0x004a9530 and its copies set them; the flags'
// other bits stay (0 in a new one).
void fill(Contact* c, Handle owner, Handle entity, float d2, const Vec& d, uint8_t vis) {
  c->owner = owner;
  c->entity = entity;
  c->seen = entity ? vt(entity, 0x24) : 0;
  c->dist2 = d2;
  c->delta[0] = d.x, c->delta[1] = d.y, c->delta[2] = d.z;
  c->x1c = -1.0f;
  c->flags = static_cast<uint8_t>((c->flags & ~1u) | (vis & 1));
}

// The frame's records ([esp+0x54] the vehicles', [esp+0x38] the targets')
// and the C3 lists ([esp+0x2c] the master, [esp+0x48] everyone on C3).
struct Ref {
  std::vector<SensorRecord>* v;
  size_t i;
  SensorRecord& r() const { return (*v)[i]; }
};
struct Frame {
  std::vector<SensorRecord> vehicles, targets;
  std::list<Ref> masters, c3;
  Handle player = 0, stealth = 0, view = 0;
  Handle master = 0;    // [0x00609a78]
  uint32_t merges = 0;  // [0x00609a7c]: counted, never read
};
Frame* g_frame = nullptr;

// Mech3.exe 1.2 @ 0x004a7fd0
SensorRecord make_record(Handle x, Handle player) {
  SensorRecord r;
  AiTarget& t = F(x);
  r.self = x;
  r.t = &t;
  r.target_was = t.target ? seen_vehicle(t.target) : 0;
  r.ally_was = t.ally_target ? seen_vehicle(t.ally_target) : 0;
  r.enemy_was = t.enemy_target ? seen_vehicle(t.enemy_target) : 0;
  r.saved_was = t.saved_target ? seen_vehicle(t.saved_target) : 0;
  for (Contact*& c : t.tracked)  // 0x004a7d00 with 0x004a7c70: the ones set
    if (c) r.refs.push_back({&c, seen_vehicle(c)});
  r.is_player = x == player;
  uint8_t f = (t.xbc >> 2) & 1;
  if (vt(x, 0xa0) == 1) f |= 2;
  if (const Handle m = x ? vt(x, 0x50) : 0) {
    SensorHooks& h = H();
    if (h.equipment(m, 4) > 0) f |= 4;     // bap
    if (h.equipment(m, 6) > 0) f |= 8;     // c3
    if (h.equipment(m, 7) > 0) f |= 0x10;  // c3slave
    if (h.equipment(m, 8) > 0) f |= 0x40;  // ecm
    if (t.xbc & 0x10) f |= 0x80;
    if (x != player && (f & 0x18)) {  // only the player leads; his allies' C3 joins his
      f &= ~8u;
      f = static_cast<uint8_t>((f & ~0x10u) | ((t.allies & h.team(vt(player, 0x28))) ? 0x10 : 0));
    }
  }
  r.flags = f;
  return r;
}

// The vehicle's frame state cleared and its contacts deleted (0x004a7be0):
// the target forgotten through vt+0xc0, or dropped.
void clear(Handle x, bool forget) {
  AiTarget& t = F(x);
  H().contact_delete(t.own_player);
  t.own_player = nullptr, t.player_contact = nullptr;
  H().contact_delete(t.own_lance);
  t.own_lance = nullptr, t.nearest_lance = nullptr;
  if (forget) H().set_target(x, nullptr, false);
  else t.target = nullptr;
  t.ally_target = t.enemy_target = t.saved_target = nullptr;
  t.clear_contacts();
  t.touching = nullptr;
  t.xbc &= 0xfc;
}

// Mech3.exe 1.2 @ 0x004a8b70(e, 0), and 0x004a91b0's and 0x004a8710's copies.
void forget(Handle e) {
  const Handle x = veh(e);
  if (e) H().set_sensed(e, 0);
  if (!x) return;
  clear(x, true);
  vt(x, 0xc8);
}

void push(std::vector<SensorRecord>& v, SensorRecord&& r) {
  Frame& f = *g_frame;
  v.push_back(std::move(r));
  const Ref ref{&v, v.size() - 1};
  if (v.back().flags & 8) {
    f.masters.push_back(ref);
    f.c3.push_back(ref);
  } else if (v.back().flags & 0x10) {
    f.c3.push_back(ref);
  }
}

// Mech3.exe 1.2 @ 0x004a8990: the record made from the state before, then
// the state cleared for this frame.
void add(Handle e) {
  Frame& f = *g_frame;
  const Handle x = veh(e);
  if (!x) return forget(e);
  H().entity_refresh(x);  // 0x005a85c0
  push(f.vehicles, make_record(x, f.player));
  const Handle x2 = veh(e);
  if (e) H().set_sensed(e, 1);
  if (!x2) return;
  clear(x2, false);
  vt(x2, 0xc8);
}

// Mech3.exe 1.2 @ 0x004a8530: a target, if active (+0x08).
void add_target(Handle t) {
  Frame& f = *g_frame;
  if (!t || !H().active(t)) return;
  H().entity_refresh(t);
  push(f.targets, make_record(t, f.player));
  H().set_sensed(t, 1);
  clear(t, false);  // no vt+0xc8 for a target
}

// Mech3.exe 1.2 @ 0x004a9530: a contact for r's vehicle on `target`, put in
// its list (the record's vt+0x00, 0x004a81f0), the target's flags, the
// picks found again.
Contact* make_contact(SensorRecord& r, Handle target, const Vec& d, float d2, uint8_t vis) {
  Contact* c = new Contact;
  fill(c, r.self, target, d2, d, vis);
  r.t->contacts.push_back(c);
  const Handle w = veh(target);
  AiTarget* wt = w ? &F(w) : nullptr;
  // FIX (bug): a target with no vehicle is not written through (the exe
  // would crash; every vehicle and target answers itself).
  if (r.is_player && vis && wt && !(wt->xbc & 8)) wt->xbc |= 8;
  if (wt && vis && (wt->enemies & H().team(vt(r.self, 0x28)))) wt->xbc |= 2;
  if (target == r.target_was) {
    r.target = c;
    if (wt) wt->xbc |= 1;
  }
  if (target == r.ally_was) r.ally = c;
  if (target == r.enemy_was) r.enemy = c;
  if (target == r.saved_was) r.saved = c;
  // 0x004a7f30: a tracked contact on the same vehicle becomes this one; its ref goes.
  auto match = [c](SensorRecord::Ref& p) {
    if (!p.slot || seen_vehicle(c) != p.was) return false;  // 0x004a7cd0
    *p.slot = c;
    return true;
  };
  auto it = r.refs.begin();
  while (it != r.refs.end() && !match(*it)) ++it;
  if (it != r.refs.end()) {
    auto out = it;
    for (++it; it != r.refs.end(); ++it)
      if (!match(*it)) *out++ = *it;
    r.refs.erase(out, r.refs.end());
  }
  return c;
}

// Mech3.exe 1.2 @ 0x004a9980: b's vehicle senses a's when in its range (d
// turned to point at a); none otherwise, d left.
Contact* senses_back(SensorRecord& b, SensorRecord& a, Vec& d, float d2) {
  if (!in_range(d2, *b.t)) return nullptr;
  negate(d);
  const Handle v = veh(a.self);
  const uint8_t vis = g_see_all ? 1 : sees(b.flags, a.flags, v);
  return make_contact(b, a.self, d, d2, vis);
}

// Mech3.exe 1.2 @ 0x004a9680: the player (the first record) and another.
// The other's contact on the player is kept at +0x98 and +0x80, made and
// owned (+0x94) when the player is out of its range.
void player_pair(SensorRecord& a, SensorRecord& b) {
  const Vec pa = centroid(a.self);  // 0x00448600
  const Vec pb = centroid(b.self);  // 0x004a98c0
  Vec d;
  const float d2 = delta(pa, pb, d);
  if (in_range(d2, *a.t)) {
    const Handle v = veh(b.self);  // 0x004a9910
    const uint8_t vis = g_see_all ? 1 : sees(a.flags, b.flags, v);
    make_contact(a, b.self, d, d2, vis);
  }
  Contact* c = senses_back(b, a, d, d2);
  if (!c) {
    negate(d);
    c = new Contact;
    const uint8_t vis = sees(b.flags, a.flags, a.self);
    fill(c, b.self, a.self, d2, d, vis);
    b.t->own_player = c;
  }
  b.t->player_contact = c;
  b.t->nearest_lance = c;
}

// Mech3.exe 1.2 @ 0x004a9cf0: a's vehicle senses b's when in its range.
Contact* senses(SensorRecord& a, SensorRecord& b, Vec& d, float& d2) {
  const Vec pa = centroid(a.self);
  const Vec pb = centroid(b.self);
  d2 = delta(pa, pb, d);
  if (!in_range(d2, *a.t)) return nullptr;
  const Handle v = veh(b.self);
  const uint8_t vis = g_see_all ? 1 : sees(a.flags, b.flags, v);
  return make_contact(a, b.self, d, d2, vis);
}

// Mech3.exe 1.2 @ 0x004a9a60: a lancemate under the player's orders and
// another; the other's +0x80 becomes the nearer of what it had and this
// lancemate, made and owned (+0x7c, reused) when out of its range.
void lance_pair(SensorRecord& a, SensorRecord& b) {
  Vec d;
  float d2;
  senses(a, b, d, d2);
  Contact* c = senses_back(b, a, d, d2);
  const Contact* old = b.t->nearest_lance;
  const bool closer = !old || !(d2 >= old->dist2);
  if (!c) {
    if (!closer) return;
    negate(d);
    c = b.t->own_lance ? b.t->own_lance : new Contact;
    const uint8_t vis = sees(b.flags, a.flags, a.self);
    fill(c, b.self, a.self, d2, d, vis);
    b.t->own_lance = c;
  }
  if (closer) b.t->nearest_lance = c;
}

// Mech3.exe 1.2 @ 0x004a92c0: two others, each sensing the other in its range.
void pair(SensorRecord& a, SensorRecord& b) {
  const Vec pa = centroid(a.self);
  const Vec pb = centroid(b.self);
  Vec d;
  const float d2 = delta(pa, pb, d);
  if (in_range(d2, *a.t)) {
    const Handle v = veh(b.self);
    const uint8_t vis = g_see_all ? 1 : sees(a.flags, b.flags, v);
    make_contact(a, b.self, d, d2, vis);
  }
  if (in_range(d2, *b.t)) {
    negate(d);
    const Handle v = veh(a.self);
    const uint8_t vis = g_see_all ? 1 : sees(b.flags, a.flags, v);
    make_contact(b, a.self, d, d2, vis);
  }
}

// Mech3.exe 1.2 @ 0x004aa270: a contact of the C3 network as the master's:
// none on the master itself, its own as it is, another's made anew from
// the master's position, in sight.
Contact* c3_contact(Contact* c) {
  const Handle master = g_frame->master;
  if (seen_vehicle(c) == master) return nullptr;
  if (c->owner == master) return c;
  const Vec pm = centroid(master);
  const Vec ps = centroid(c->seen);
  Vec d;
  const float d2 = delta(pm, ps, d);
  Contact* n = new Contact;
  fill(n, master, c->seen, d2, d, 1);
  return n;
}

// By what they saw (0x004aa260: the pointers' order).
bool by_seen(const Contact* a, const Contact* b) { return a->seen < b->seen; }

// Mech3.exe 1.2 @ 0x004a9fd0: the master's list becomes the union of its
// own and every ally's on C3 (by what each saw), when that holds more.
void c3_merge(SensorRecord& m) {
  Frame& f = *g_frame;
  for (const Ref& ref : f.c3) {
    SensorRecord& x = ref.r();
    if (m.self == x.self || (x.flags & 0x20) || !(x.flags & 0x18)) continue;
    if (!(x.t->allies & H().team(vt(m.self, 0x28)))) continue;
    ++f.merges;
    if (m.sorted_by_seen != 1) {
      crt::msvc_sort(m.t->contacts.begin(), m.t->contacts.end(), by_seen);  // 0x004ab1f0
      m.sorted_by_seen = 1;
    }
    if (x.sorted_by_seen != 1) {
      crt::msvc_sort(x.t->contacts.begin(), x.t->contacts.end(), by_seen);
      x.sorted_by_seen = 1;
    }
    f.master = m.self;
    // FIX (bug): the exe merges into [0x00609a5c], sized to the entity list
    // (at least 32), and writes past it when the union is longer (targets
    // count too); the port's grows.
    std::vector<Contact*> out;
    auto a = m.t->contacts.begin(), ae = m.t->contacts.end();
    auto b = x.t->contacts.begin(), be = x.t->contacts.end();
    while (a != ae && b != be) {
      if ((*a)->seen < (*b)->seen) {
        out.push_back(c3_contact(*a++));
      } else if ((*b)->seen < (*a)->seen) {
        out.push_back(c3_contact(*b++));
      } else {
        out.push_back(c3_contact(*a++));
        ++b;
      }
    }
    for (; a != ae; ++a) out.push_back(c3_contact(*a));  // 0x004ab470
    for (; b != be; ++b) out.push_back(c3_contact(*b));
    size_t n = 0;  // 0x004ab4a0, 0x004ab4c0: the nulls out
    for (Contact* c : out)
      if (c) out[n++] = c;
    if (n > m.t->contacts.size()) {
      m.t->contacts.resize(n);
      std::copy(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(n), m.t->contacts.begin());
    }
  }
}

// By distance (0x004a9fb0: the x87's below, NaN first).
bool by_distance(const Contact* a, const Contact* b) { return !(a->dist2 >= b->dist2); }

// Mech3.exe 1.2 @ 0x004a9e70: the list nearest first (0x004ab500,
// 0x004ab620), the one within 8 m, the picks put back; a target lost
// makes way for the nearest ally (when it was the last ally) or the
// nearest of the rest.
void finish(SensorRecord& r) {
  AiTarget& t = *r.t;
  if (!t.contacts.empty()) {
    crt::msvc_sort(t.contacts.begin(), t.contacts.end(), by_distance);
    if (!(t.contacts.front()->dist2 >= 64.0f)) t.touching = t.contacts.front();
  }
  t.ally_target = r.ally;
  t.enemy_target = r.enemy;
  t.saved_target = r.saved;
  if (r.target) H().set_target(r.self, r.target, true);
  else if (r.target_was == r.ally_was && r.target_was) t.nearest_ally(0);
  else if (r.target_was) t.nearest_enemy(0);
  if (!(vt(r.self, 0xd0) & 0xff)) H().set_sensed(r.self, 0);
}

// Mech3.exe 1.2 @ 0x004aa400 over the view's vehicle's contacts, nearest
// first: the first in its range whose vehicle's vt+0x40 has a ping (vt+0xec)
// plays it at that vehicle; stops there or at the first out of range.
void radar_ping(Handle view) {
  AiTarget& t = F(view);
  const float r2 = t.range2;
  for (Contact* c : t.contacts) {
    if (c->dist2 > r2) break;
    const Handle v = seen_vehicle(c);
    const Handle o = v ? vt(v, 0x40) : 0;
    if (!o) continue;
    const Handle s = vt(o, 0xec);
    if (!s) continue;
    float p[3];
    H().node_position(o, p);  // 0x005a85e0
    H().ping(s, p);
    break;
  }
}

}  // namespace

// Mech3.exe 1.2 @ 0x004a8cd0 with 0x004a8710 and 0x004a83c0 (the frame's
// set-up), 0x004a91b0 per entity, 0x004a8530 per target. Order: the player
// with every other (0x004a9680), each lancemate under orders with those
// after it (0x004a9a60), then every pair of the rest (0x004a92c0), each
// with the targets; the C3 merge; every vehicle's list finished.
void sensors_update() {
  SensorHooks& h = H();
  Frame f;
  g_frame = &f;
  f.player = h.player();
  f.view = h.view_target();
  f.stealth = h.stealth();
  const std::vector<Handle> list = g_entities;
  f.vehicles.reserve(list.size() + 2);
  f.targets.reserve(list.size());
  // 0x004a8710
  if (const Handle x = veh(f.player)) vt(x, 0xcc);  // asked, not used
  add(f.player);
  if (f.player) h.set_sensed(f.player, 0);
  if (f.stealth) {
    const Handle x = veh(f.stealth);
    h.set_sensed(f.stealth, 0);
    if (x) {
      clear(x, true);
      vt(x, 0xc8);
    }
  }
  if (f.view != f.player && f.view != f.stealth) {
    if (const Handle x = veh(f.view)) vt(x, 0xcc);  // 0x004a8960, not used
    add(f.view);
  }
  for (Handle e : list) {  // 0x004a91b0
    if (e == f.view || e == f.player || e == f.stealth) continue;
    const Handle x = veh(e);
    if (x && (vt(x, 0xcc) & 0xff)) add(e);
    else forget(e);
  }
  for (Handle t : h.targets()) add_target(t);

  std::vector<SensorRecord>& v = f.vehicles;
  std::vector<SensorRecord>& tg = f.targets;
  size_t i = 1;
  if (!v.empty()) {
    for (size_t j = 1; j < v.size(); ++j) player_pair(v[0], v[j]);
    for (SensorRecord& t : tg) player_pair(v[0], t);
    for (; i < v.size() && (v[i].flags & 0x80); ++i) {
      for (size_t j = i + 1; j < v.size(); ++j) lance_pair(v[i], v[j]);
      for (SensorRecord& t : tg) lance_pair(v[i], t);
    }
    for (; i < v.size(); ++i) {
      for (size_t j = i + 1; j < v.size(); ++j) pair(v[i], v[j]);
      for (SensorRecord& t : tg) pair(v[i], t);
    }
  }
  if (!f.masters.empty())
    for (const Ref& m : f.masters) c3_merge(m.r());
  for (SensorRecord& r : v) finish(r);
  if (const Handle view = h.view_target()) radar_ping(view);
  g_frame = nullptr;
}

// --- The entity sort (after ZAiTarget.cpp) --------------------------------------------

namespace {

// The brain behind a vehicle's controls (vt+0x140, then vt+0x54).
Handle brain_of(Handle m) {
  const Handle c = vt(m, 0x140);
  return c ? vt(c, 0x54) : 0;
}

// Mech3.exe 1.2 @ 0x004d7650, and inline in the partition 0x004d7bc0. Ties
// go by the objects' addresses.
struct EntityLess {
  Handle player, stealth;
  bool operator()(Handle a, Handle b) const {
    if (a == b || a == stealth) return false;
    if (b == stealth || a == player) return true;
    if (b == player) return false;
    const Handle va = veh(a), vb = veh(b);
    if (!va) return vb ? false : a < b;
    if (!vb) return true;
    const uint32_t allies = F(player).allies;
    const bool aa = (allies & H().team(vt(va, 0x28))) != 0;
    const bool ab = (allies & H().team(vt(vb, 0x28))) != 0;
    if (!aa) return ab ? false : a < b;
    if (!ab) return true;
    const bool la = F(va).xbc & 0x10, lb = F(vb).xbc & 0x10;
    if (!la) return lb ? false : va < vb;
    if (!lb) return true;
    const Handle ma = vt(va, 0x50), mb = vt(vb, 0x50);
    if (!ma) return mb ? false : a < b;
    if (!mb) return true;
    const Handle ba = brain_of(ma), bb = brain_of(mb);
    if (!ba) return bb ? true : a < b;  // as written: no brain sorts first
    if (!bb) return false;
    // The lance: Dominic Paine, Epona Rhi, Alan Mattila, then the rest.
    const char* s[3] = {H().message(0xe14), H().message(0xe15), H().message(0xe01)};
    auto rank = [&s](const char* p) {
      // FIX (bug): a lancemate without a pilot name ranks last (the exe reads through null).
      if (!p) return 3;
      for (int k = 0; k < 3; ++k)
        if (s[k] && std::strcmp(p, s[k]) == 0) return k;
      return 3;
    };
    const int ra = rank(H().brain_pilot(ba)), rb = rank(H().brain_pilot(bb));
    return ra != rb ? ra < rb : a < b;
  }
};

}  // namespace

// Mech3.exe 1.2 @ 0x004d5be0 (ECX the vehicle): the brain's lancemate
// setting (+0xcd), bit 1 "default": an ally of the player.
void lance_flag(Handle x) {
  AiTarget& t = F(x);
  const Handle m = vt(x, 0x40);
  const Handle b = m ? brain_of(m) : 0;
  bool on;
  if (!b) {
    on = false;
  } else if (!(H().brain_lancemate(b) & 2)) {
    on = H().brain_lancemate(b) & 1;
  } else {
    const Handle w = vt(x, 0x50);
    const Handle p = H().player();
    on = w && w != p && (F(p).allies & H().team(vt(w, 0x28)));
  }
  t.xbc = static_cast<uint8_t>((t.xbc & ~0x10u) | (on ? 0x10 : 0));
}

// Mech3.exe 1.2 @ 0x004d5ca0 (with 0x004d5bc0 per entity, the sort 0x004d7360).
void entities_sort() {
  SensorHooks& h = H();
  std::vector<Handle>& l = g_entities;
  for (Handle e : l)
    if (const Handle x = veh(e)) lance_flag(x);
  if (l.size() > 1) {
    crt::msvc_sort(l.begin(), l.end(), EntityLess{h.player(), h.stealth()});
    const Handle p = h.player();
    size_t i = 1;
    for (; i < l.size(); ++i) {
      const Handle x = veh(l[i]);
      const Handle m = x ? vt(x, 0x50) : 0;
      if (!m || !(F(p).allies & h.team(vt(m, 0x28)))) break;
    }
    g_allies = {1, i};
  } else {
    g_allies = {l.size(), l.size()};
  }
  h.set_sorted();
}

// Mech3.exe 1.2 @ 0x004d5dc0, from the AI controls' damage handler
// (0x004ea520) once the player's friendly fire passes 3: every ally after
// him in the sorted list takes his team as an enemy and stops counting it
// an ally; bit 31 (team 31) goes to their allies and off every enemy mask.
void allies_turn() {
  SensorHooks& h = H();
  AiTarget& p = F(h.player());
  const uint32_t team = p.team;
  p.enemies &= 0x7fffffff;
  for (size_t i = g_allies.first; i < g_allies.end && i < g_entities.size(); ++i) {
    const Handle x = veh(g_entities[i]);
    if (!x) continue;
    AiTarget& t = F(x);
    t.enemies = (team | t.enemies) & 0x7fffffff;
    t.allies = (t.allies | 0x80000000u) & ~team;
  }
}

}  // namespace mech3::ai
