#include "mech3/veh_basic_controls.h"

#include <cmath>
#include <cstring>
#include <utility>

#include "crt/msvc_rand.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zreader/zreader.h"
#include "mech3/veh_basic_object.h"
#include "mech3/veh_stealth.h"
#include "mech3/zai_x87.h"

namespace mech3 {

namespace {
BasicControlsHooks g_default;
BasicControlsHooks* g_hooks = &g_default;
BasicControlsHooks& hk() { return *g_hooks; }
// The exe's fcomp against 0 tested on C3: equal, or unordered (a NaN counts as 0).
bool zero(float f) { return !(f < 0.0f || f > 0.0f); }
}  // namespace

BasicControlsHooks& basic_controls_hooks() { return *g_hooks; }
void set_basic_controls_hooks(BasicControlsHooks* h) { g_hooks = h ? h : &g_default; }

void BasicControlsHooks::base_init(BasicControls& c) { c.VehicleControls::init(); }
void BasicControlsHooks::base_setup(BasicControls& c, zrdr::Node* def, uint32_t) { c.VehicleControls::setup(def); }

// Mech3.exe 1.2 @ 0x004022c0: the record whose +0x18 is the number.
Net105Record* BasicControlsHooks::record(int32_t id) {
  for (Net105Record* r = ai::nets105(); r; r = r->next)
    if (r->x18 == id) return r;
  return nullptr;
}

float BasicControlsHooks::reader_float(zrdr::Node* n, int32_t i) { return n->list[i].f; }

// Mech3.exe 1.2 @ 0x004ae270
BasicControls::BasicControls(void* vehicle, int32_t net_number) {
  owner = vehicle;
  net = net_number;
  init();
}

// Mech3.exe 1.2 @ 0x004ae3c0: with an owner and its node, the base's init; a net number that names a record
// takes its mode from the record's kind and its node nearest the vehicle, the vehicle's "healthy" node shown
// to the box test (and its flag 8 off), and the ranges from the record (each 0 its default); else stopped (4).
void BasicControls::init() {
  BasicControlsHooks& h = hk();
  auto* v = static_cast<BasicVehicle*>(owner);
  if (!v || !v->node) return;
  h.base_init(*this);
  state = 4;
  if (net > 0 && (record = h.record(net))) state = 2;  // no positive number: +0xa08 as it was
  if (state != 2) return;
  if (record->kind == 0) mode = 0;
  else if (record->kind == 1) mode = 2;
  else if (record->kind == 2) mode = 3;
  const Handle e = h.nearest_element(record, h.owner_position(owner));
  net_node = e ? h.element_node(e) : nullptr;
  if (const Handle healthy = h.node_find(v->node, "healthy")) {
    h.node_set_intersect_bbox(healthy, 1);
    h.node_set_flag8(healthy, 0);
  }
  const auto sq = [](float f, float otherwise) { return zero(f) ? otherwise : f * f; };
  activate_rad = sq(record->activate_rad, 102400.0f);   // [0x005c8c90]
  attack_rad = sq(record->attack_rad, 1500.0f);                   // [0x005c8c98]
  attack_dwell = zero(record->attack_dwell) ? 10.0f : record->attack_dwell;
  pursuit_near = sq(record->pursuit_near, 4624.0f);
  pursuit_far = sq(record->pursuit_far, 4900.0f);
  not_pursuit_dwell = zero(record->not_pursuit_dwell) ? 3.0f : record->not_pursuit_dwell;
  return_range = sq(record->return_range, 62500.0f);
  xa18 = 0;
  v->xe0 = v;
}

// Mech3.exe 1.2 @ 0x004ae6b0: the net's number kept, the base's setup; with a record, each of its ranges left 0
// taken from the definition ("activation", "pursuit_range"'s two, "not_pursuit_dwell", "return_range"), the
// pursuit pair put in order.
void BasicControls::setup_with(zrdr::Node* def, uint32_t arg2, int32_t net_number) {
  BasicControlsHooks& h = hk();
  auto* v = static_cast<BasicVehicle*>(owner);
  if (!v || !v->node) return;
  net = net_number;
  h.base_setup(*this, def, arg2);
  if (state != 2) return;
  zrdr::Node* n = h.reader_find(def, "activation");
  if (zero(record->activate_rad) && n) {
    const float f = h.reader_float(n, 1);
    activate_rad = f * f;
  }
  if ((n = h.reader_find(def, "pursuit_range"))) {
    if (zero(record->pursuit_near)) {
      const float f = h.reader_float(n, 1);
      pursuit_near = f * f;
    }
    if (zero(record->pursuit_far)) {
      const float f = h.reader_float(n, 2);
      pursuit_far = f * f;
    }
  }
  if (pursuit_far < pursuit_near) std::swap(pursuit_near, pursuit_far);
  if ((n = h.reader_find(def, "not_pursuit_dwell")) && zero(record->not_pursuit_dwell)) not_pursuit_dwell = h.reader_float(n, 1);
  if ((n = h.reader_find(def, "return_range")) && zero(record->return_range)) {
    const float f = h.reader_float(n, 1);
    return_range = f * f;
  }
}

// Mech3.exe 1.2 @ 0x004ae350
int32_t BasicControls::path_goal() { return hk().node_goal(net_node); }

}  // namespace mech3

// ---------------------------------------------------------------------------------------------------------------
// The path follower (0x00401050..0x00401f50; docs/vehicles.md, The basic vehicle's own controls). Each runs on the
// vehicle (ECX), its controls by +0xe4; the target (EDX) is the player's mech.

namespace mech3 {

namespace {
using Node = ai::Net105Node;
BasicControls& ctl(BasicVehicle& v) { return *static_cast<BasicControls*>(v.controls); }
Node* node_of(BasicControls& c) { return static_cast<Node*>(c.net_node); }
float from_bits_f(uint32_t b) { return zgame::from_bits(b); }
uint32_t bits_of(float f) { return zgame::bits(f); }

// Mech3.exe 1.2 @ 0x00401620: a link of `n` other than `exclude`: one link the first; two the first; else a random
// one of the count, the next after it when that is the excluded.
int32_t pick_link(Node& n, int32_t exclude) {
  int32_t count = 0;
  for (Node* l : n.links)
    if (l) ++count;
  if (count == 1) return 0;
  int32_t i = count == 2 ? 0 : hk().rand() % count;
  if (i == exclude) i = (i + 1) % count;
  return i;
}

// Mech3.exe 1.2 @ 0x00401500 (EDX the node in hand, kept updated): on to the followed link's node. A node made at
// run time left behind is freed; at one, its link 0 (a kind-1 net's vehicle back in mode 2) unless it is made too;
// else the link back to where it came from avoided. `*rec` the chosen link's record, `off` the vehicle's offset
// from the new node.
void next_node(BasicVehicle& v, Node** in_hand, ai::Net105Link** rec, float off[3]) {
  BasicControls& c = ctl(v);
  Node* from = *in_hand;
  Node* to = from->links[c.link];
  c.net_node = to;
  const int32_t from_number = from->number;
  if (from_number < 0) {
    hk().node_free(from);
    to = node_of(c);
    *in_hand = to;
    if (to->number < 0) {
      c.link = 0;
    } else {
      c.link = pick_link(*to, -1);
      if (c.record->kind == 1) c.mode = 2;
    }
  } else {
    *in_hand = to;
    int32_t back = 4;
    for (int32_t i = 0; i < 3; ++i)
      if (to->links[i] && to->links[i]->number == from_number) {
        back = i;
        break;
      }
    c.link = pick_link(*to, back);
  }
  *rec = (*in_hand)->link_rec[c.link];
  const float* p = hk().entity_position(&v);
  const float* at = (*in_hand)->place;
  off[0] = p[0] - at[0], off[1] = p[1] - at[1], off[2] = p[2] - at[2];
}

// Mech3.exe 1.2 @ 0x00401400: blocked when the last frame hit an object or a wall, else when a sweep from the place
// to the place ahead by the velocity x 0.7143 hits a wall or an object.
bool blocked(BasicVehicle& v) {
  BasicControls& c = ctl(v);
  if (c.hit_object || c.hit_wall) return true;
  const float* p = hk().entity_position(&v);
  float pts[6] = {p[0], p[1], p[2], 0, 0, 0};
  const float ahead[3] = {c.velocity.x * 0.7143f, c.velocity.y * 0.7143f, c.velocity.z * 0.7143f};
  const float* q = hk().entity_position(&v);
  pts[3] = ahead[0] + q[0], pts[4] = ahead[1] + q[1], pts[5] = ahead[2] + q[2];
  hk().sweep(&v, pts);
  const bool hit = hk().sweep_hit(c);
  hk().sweep_clear(&v);
  return hit;
}

// Mech3.exe 1.2 @ 0x004020d0 in float (the vehicle's frame runs it at the game's x87 setting): normalised, the length.
float norm(float d[3]) {
  zmth::Vec3 w{d[0], d[1], d[2]};
  const float len = zmth::normalize(w);
  d[0] = w.x, d[1] = w.y, d[2] = w.z;
  return len;
}

// The way to `to` on the flat, normalised (0x004020d0: its length returned), and the throttle and turn toward it.
float flat_to(BasicVehicle& v, const float* to, float d[3]) {
  const float* p = hk().entity_position(&v);
  d[0] = to[0] - p[0], d[1] = to[1] - p[1], d[2] = to[2] - p[2];
  d[1] = 0.0f;
  return norm(d);
}
}  // namespace

// Mech3.exe 1.2 @ 0x00401100 (mode 0): along the links. Blocked: on to the next node, stopped (mode 4), turning to
// face the link's node (+0x760, the heading +0x748). Else toward the link's node: ahead, the throttle 1 - |cross|
// (no less than 0.25) and the turn the cross; behind, once it was ahead, on to the next node, again; else stopped,
// turning. A submarine pitches toward the node's height. Within 10, on to the next node.
void basic_follow_waypoints(BasicVehicle& v) {
  BasicControls& c = ctl(v);
  Node* in_hand = node_of(c);
  ai::Net105Link* rec = in_hand->link_rec[c.link];
  Node* to = in_hand->links[c.link];
  float d[3];
  if (!blocked(v)) {
    const float len = flat_to(v, to->place, d);
    const float fx = v.forward.x, fz = v.forward.z;
    const float dot = fz * d[2] + fx * d[0];
    const float cross = d[0] * fz - d[2] * fx;
    float throttle, turn;
    if (0.0f <= dot) {
      throttle = 1.0f - std::fabs(cross);
      if (throttle <= 0.25f) throttle = 0.25f;
      c.ahead = 1;
      turn = cross;
    } else {
      if (c.ahead) {
        next_node(v, &in_hand, &rec, d);
        c.ahead = 0;
        basic_follow_waypoints(v);
        return;
      }
      throttle = 0.0f;
      turn = static_cast<float>(0.0f <= cross ? 1 : -1);
    }
    c.throttle = throttle;
    c.turn = turn;
    if (c.motion.mode == VehicleMotion::kSub) {
      const float y = to->place[1];
      const float* p = hk().entity_position(&v);
      const float pitch = (((y - p[1]) + c.x7ec) * from_bits_f(0x3c8ef34d) - c.angles.x) * from_bits_f(0x40b66666);  // [0x005e0250], [0x005e0254]
      c.pitch_in = pitch;
      c.pitch_out = input_curve(pitch, 0.5f, 1.0f);
    }
    if (len < 10.0f) {
      next_node(v, &in_hand, &rec, d);
      c.ahead = 0;
    }
    return;
  }
  next_node(v, &in_hand, &rec, d);
  const Node* face = in_hand->links[c.link];  // the new node's link (0x00401500 updates the node in hand)
  c.mode = 4;
  c.x760 = 1;
  const float* p = hk().entity_position(&v);
  float w[3] = {face->place[0] - p[0], face->place[1] - p[1], face->place[2] - p[2]};
  w[1] = 0.0f;
  zmth::Vec3 out{c.heading.x, c.heading.y, c.heading.z};
  zmth::flat_normalize(zmth::Vec3{w[0], w[1], w[2]}, out);  // 0x0051e210 into +0x748
  c.heading.x = out.x, c.heading.y = out.y, c.heading.z = out.z;
  c.throttle = 0.0f;
  c.turn = 0.0f;
}


namespace {
// Mech3.exe 1.2 @ 0x00401ab0 (EDX `toward`): 400 or more from the net node in hand, a breadcrumb: a node made at the
// vehicle's place, linked to `toward`, its links' records zeroed; the old node's link record filled toward it
// (0x004023d0, width 10); made the node in hand, link 0, the next breadcrumb a second on.
void drop_crumb(BasicVehicle& v, Node* toward) {
  BasicControls& c = ctl(v);
  const float* p = hk().entity_position(&v);
  const zmth::Vec3 a{node_of(c)->place[0], node_of(c)->place[1], node_of(c)->place[2]}, b{p[0], p[1], p[2]};
  if (!(400.0f <= zmth::distance_sq(a, b))) return;
  Node* n = hk().node_make(c);
  n->links[0] = toward;
  const float* q = hk().entity_position(&v);
  n->place[0] = q[0], n->place[1] = q[1], n->place[2] = q[2];
  n->number = -1;
  hk().link_fill(n->link_rec[0], node_of(c)->place, n->place, 10.0f);
  c.net_node = n;
  c.link = 0;
  c.crumb_at = hk().now() - -1.0f;
}

// Mech3.exe 1.2 @ 0x00401940: armed and past the dwell, both matrices fresh: within attack_rad of the target with a
// clear line, the pursuit (mode 1): the weapon's next shot now, a breadcrumb toward the link's node, its place kept,
// +0x350 set, the dwell's end = now + attack_dwell; true. Not clear: +0x350 off.
bool engage(BasicVehicle& v, Handle target) {
  BasicControls& c = ctl(v);
  if (!c.first || !(c.xa18 < hk().now())) return false;
  hk().entity_refresh(&v);
  hk().entity_refresh(target);
  if (!(hk().centroid_distance_sq(&v, target) < c.attack_rad)) return false;
  if (!hk().line_clear(&v, target)) {
    c.x350 = 0;
    return false;
  }
  c.mode = 1;
  c.first->x24 = bits_of(hk().now());
  drop_crumb(v, node_of(c)->links[c.link]);
  const Node* to = node_of(c)->links[c.link];
  c.chase[0] = to->place[0], c.chase[1] = to->place[1], c.chase[2] = to->place[2];
  c.x350 = 1;
  c.xa18 = c.attack_dwell + hk().now();
  return true;
}

// Mech3.exe 1.2 @ 0x00401f50 (EDX the target; the squared distance, the facing): the first weapon. Not beaming: the
// target active, the shot due, facing within 0.75 and inside its range: fire (+0x390), the next shot an interval
// on; a beam weapon (its def's +0x6c bit 1) starts its beam; a locking one (bit 15) locks on the target. Beaming:
// past the time, off the facing or range, or the target gone, the fire off and the next shot an interval on.
void fire_check(BasicVehicle& v, Handle target, float d2, float facing) {
  BasicControls& c = ctl(v);
  WeaponSlot* w = c.first;
  if (!w) return;  // not in the exe, which reads through it: every vehicle that pursues or stands is armed
  const float now = hk().now();
  const float next = from_bits_f(w->x24), every = from_bits_f(w->x28);
  const float lo = from_bits_f(w->x2c), hi = from_bits_f(w->x30);
  if (c.beam_on == 0) {
    if (hk().entity_active(target) && next < now && 0.75f < facing && d2 < hi && lo < d2) {
      c.fire_on = 1;
      w->x24 = bits_of(now + every);
      const uint32_t flags = hk().def_flags(w->def);
      if (flags >> 1 & 1) {
        c.beam_on = 1;
        hk().beam_start(w->beam, c.serial);
        w->x24 = bits_of(hk().now() + every);
        return;
      }
      if (flags & 0x8000) {
        c.x65c = 1;
        c.x664 = const_cast<float*>(hk().entity_lock(target));
        c.x668 = hk().entity_velocity(target);
        return;
      }
      c.x65c = 0, c.x664 = nullptr, c.x668 = nullptr;
    }
    return;
  }
  if (next < now || facing < 0.75f || hi < d2 || !hk().entity_active(target)) {
    c.fire_on = 0;
    w->x24 = bits_of(every + hk().now());
  }
}

// Turn toward a flat way (the facing's sign when behind): the dot and the cross with the vehicle's forward.
void face(BasicVehicle& v, const float d[3], float* dot, float* turn) {
  const float fx = v.forward.x, fz = v.forward.z;
  *turn = d[0] * fz - fx * d[2];
  *dot = fx * d[0] + d[2] * fz;
}
}  // namespace

// Mech3.exe 1.2 @ 0x00401bf0: a breadcrumb linked to the node in hand, then the waypoints (mode 0).
void basic_back_to_path(BasicVehicle& v) {
  drop_crumb(v, node_of(ctl(v)));
  ctl(v).mode = 0;
}

// Mech3.exe 1.2 @ 0x00401680 (mode 1): the pursuit. A breadcrumb (linked to the node in hand) when due; toward the target: ahead, the throttle 1
// past pursuit_far, -1 inside pursuit_near, else 0; behind, 0 and the turn +-1. A submarine pitches and banks by the
// way's height. The lock point aimed at (+0x6f0), the fire check. Past the dwell with the target active, within the
// return range at 0.8 of full health: on; else back to the path, the next engage not_pursuit_dwell on.
void basic_pursue(BasicVehicle& v, Handle target) {
  BasicControls& c = ctl(v);
  if (c.crumb_at <= hk().now()) drop_crumb(v, node_of(c));  // EDX the node in hand
  const float* p = hk().entity_position(&v);
  const float* t = hk().entity_position(target);
  float d[3] = {t[0] - p[0], t[1] - p[1], t[2] - p[2]};
  const float dy = d[1];
  const float d2 = d[2] * d[2] + d[0] * d[0];
  float rise = dy;
  d[1] = 0.0f;
  const float len = norm(d);
  rise = len == 0.0f ? 0.0f : dy / len;
  float dot, turn;
  face(v, d, &dot, &turn);
  float throttle;
  if (0.0f < dot) {
    throttle = d2 <= c.pursuit_far ? (c.pursuit_near <= d2 ? 0.0f : -1.0f) : 1.0f;
  } else {
    throttle = 0.0f;
    turn = static_cast<float>(0.0f <= turn ? 1 : -1);
  }
  c.throttle = throttle;
  c.turn = turn;
  if (c.motion.mode == VehicleMotion::kSub) {
    const float pitch = (from_bits_f(0x3f4ccccd) * rise - c.angles.x) * from_bits_f(0x40b66666);  // [0x005e0258], [0x005e025c]
    c.pitch_in = pitch;
    c.pitch_out = input_curve(pitch, 0.5f, 1.0f);
    const float bank = from_bits_f(0x3dcccccd) * dy;  // [0x005e0260]
    c.in40 = bank;
    c.out48 = input_curve(bank, 0.5f, 1.0f);
  }
  const float* lock = hk().entity_lock(target);
  c.player_at.x = lock[0], c.player_at.y = lock[1], c.player_at.z = lock[2];
  fire_check(v, target, d2, dot);
  if (hk().now() <= c.xa18 && hk().entity_active(target)) {
    const float* q = hk().entity_position(&v);
    const zmth::Vec3 a{q[0], q[1], q[2]}, b{c.chase[0], c.chase[1], c.chase[2]};
    if (zmth::distance_sq(a, b) <= c.return_range && c.health_max * 0.8f <= c.health) return;
  }
  basic_back_to_path(v);
  c.xa18 = c.not_pursuit_dwell + hk().now();
}

// Mech3.exe 1.2 @ 0x00401c30 (mode 2, a kind-1 net's): standing, turning to face the target (+-1 when behind), the
// lock point aimed at; once engaged, every vehicle on its +0xe0 ring still in mode 2 joins the pursuit.
void basic_hold(BasicVehicle& v, Handle target) {
  BasicControls& c = ctl(v);
  const float* p = hk().entity_position(&v);
  const float* t = hk().entity_position(target);
  float d[3] = {t[0] - p[0], 0.0f, t[2] - p[2]};
  norm(d);
  float dot, turn;
  face(v, d, &dot, &turn);
  if (dot <= 0.0f) turn = static_cast<float>(0.0f <= turn ? 1 : -1);
  c.throttle = 0.0f;
  c.turn = turn;
  const float* lock = hk().entity_lock(target);
  c.player_at.x = lock[0], c.player_at.y = lock[1], c.player_at.z = lock[2];
  if (!engage(v, target)) return;
  for (auto* o = static_cast<BasicVehicle*>(v.xe0); o && o != &v; o = static_cast<BasicVehicle*>(o->xe0)) {
    BasicControls& oc = ctl(*o);
    if (oc.mode != 2) continue;
    oc.mode = 1;
    drop_crumb(*o, node_of(oc)->links[oc.link]);
    oc.xa18 = oc.attack_dwell + hk().now();
  }
}

// Mech3.exe 1.2 @ 0x00401e30 (mode 3, a kind-2 net's): turning to face the target, the lock point aimed at, the
// fire check.
void basic_stand(BasicVehicle& v, Handle target) {
  BasicControls& c = ctl(v);
  const float* p = hk().entity_position(&v);
  const float* t = hk().entity_position(target);
  float d[3] = {t[0] - p[0], 0.0f, t[2] - p[2]};
  const float d2 = d[2] * d[2] + d[0] * d[0];
  norm(d);
  float dot, turn;
  face(v, d, &dot, &turn);
  if (dot <= 0.0f) turn = static_cast<float>(0.0f <= turn ? 1 : -1);
  c.turn = turn;
  const float* lock = hk().entity_lock(target);
  c.player_at.x = lock[0], c.player_at.y = lock[1], c.player_at.z = lock[2];
  fire_check(v, target, d2, dot);
}

// Mech3.exe 1.2 @ 0x00401050
void BasicControls::follow(Handle target) {
  auto& v = *static_cast<BasicVehicle*>(owner);
  switch (mode) {
    case 0:
      basic_follow_waypoints(v);
      engage(v, target);
      return;
    case 1:
      basic_pursue(v, target);
      return;
    case 2:
      basic_hold(v, target);
      return;
    case 3:
      basic_stand(v, target);
      return;
    case 4:
      turn = 0.0f;  // IControls vt+0x28(0)
      if (x760 == 0) mode = 0;
      engage(v, target);
      return;
    default:
      return;
  }
}

bool BasicControlsHooks::sweep_hit(BasicControls& c) { return !c.walls.empty() || !c.objects.empty(); }
int32_t BasicControlsHooks::rand() { return crt::msvc_rand(); }
void BasicControlsHooks::node_free(ai::Net105Node* n) {
  for (ai::Net105Link*& r : n->link_rec) std::free(r), r = nullptr;
  for (ai::Net105* net = ai::nets105(); net; net = net->next)
    for (auto it = net->made.begin(); it != net->made.end(); ++it)
      if (it->get() == n) {
        net->made.erase(it);
        return;
      }
}
ai::Net105Node* BasicControlsHooks::node_make(BasicControls& c) {
  ai::Net105Node* n = c.record->make_node();
  n->link_rec[0] = static_cast<ai::Net105Link*>(std::calloc(1, 0x3c));  // malloc(0x3c) zeroed; the others none
  return n;
}

// Mech3.exe 1.2 @ 0x004023d0 (ECX the record, EDX `from`): the offset to `to`; the flat length less the width, or
// half the width when that is less (a NaN: the half); the offset flat-normalised, the side across it, the two
// turned 45 either way (the exe passes 45 as is); the width. Also the v105 nets' measure (ai::net105_link_fill).
void basic_link_fill(ai::Net105Link& r, const float from[3], const float to[3], float width) {
  r.offset[0] = to[0] - from[0], r.offset[1] = to[1] - from[1], r.offset[2] = to[2] - from[2];
  const float flat = std::sqrt(r.offset[0] * r.offset[0] + r.offset[2] * r.offset[2]);
  const float half = width * 0.5f;  // [0x005bb920]
  r.length = flat - width > half ? flat - width : half;
  zmth::Vec3 o{r.offset[0], r.offset[1], r.offset[2]};
  zmth::flat_normalize(o, o);
  r.offset[0] = o.x, r.offset[1] = o.y, r.offset[2] = o.z;
  r.side[0] = -r.offset[2], r.side[1] = 0.0f, r.side[2] = r.offset[0];  // 0x00522460
  zmth::rotate_y(r.left, r.side, 45.0f);
  zmth::rotate_y(r.right, r.side, -45.0f);
  r.width = width;
}
void BasicControlsHooks::link_fill(ai::Net105Link* r, const float* to, const float* from, float width) {
  if (r) basic_link_fill(*r, from, to, width);
}

}  // namespace mech3
