#include "mech3/zai_wire_firing_pm.h"

#include <cstdio>
#include <cstdlib>

#include <map>

#include "mech3/mech.h"
#include "mech3/mech_parts.h"
#include "mech3/weapon.h"
#include "mech3/zai_brain_pm.h"
#include "mech3/zai_target.h"
#include "mech3/zai_wire_pm.h"
#include "mech3/zai_wire_walk_pm.h"

namespace mech3::ai {

namespace {
// What a counted hook hands back for a place or matrix: its callers read through it, as the exe's never null.
constexpr float kZeros[12] = {};
PlanPm* plan_of(zgame::Handle plan) { return static_cast<PlanPm*>(pm_ptr(plan)); }
zgame::Handle vehicle_of(Brain* b) { return b->controls->vehicle(); }
const std::vector<zgame::Handle>& no_handles() {
  static const std::vector<zgame::Handle> none;
  return none;
}
std::vector<Contact*>& no_contacts() {
  static std::vector<Contact*> none;
  return none;
}
std::map<zgame::Handle, FiringRecord>& records() {
  static std::map<zgame::Handle, FiringRecord> m;
  return m;
}
FiringRecord* record_of(zgame::Handle v) {
  const auto it = records().find(v);
  return it == records().end() ? nullptr : &it->second;
}
WeaponManager* wm_of(zgame::Handle wm) { return static_cast<WeaponManager*>(brain_hooks().object_of(wm)); }
Weapon* weapon_of(zgame::Handle w) { return static_cast<Weapon*>(brain_hooks().object_of(w)); }
WeaponGroup* group_of(zgame::Handle g) { return static_cast<WeaponGroup*>(brain_hooks().object_of(g)); }
// By reference in the exe; here rebuilt on each ask.
const std::vector<zgame::Handle>& list_of(Weapon* const* from, uint32_t n) {
  static std::vector<zgame::Handle> out;
  out.clear();
  for (uint32_t i = 0; i < n; ++i) out.push_back(brain_hooks().handle_of(from[i]));
  return out;
}
const std::vector<zgame::Handle>& manager_list(zgame::Handle wm) {
  const WeaponManager* m = wm_of(wm);
  return m ? list_of(m->weapons, m->count) : no_handles();
}
zgame::Handle manager_group(zgame::Handle wm) {
  const WeaponManager* m = wm_of(wm);
  return m && m->group_ptr() ? brain_hooks().handle_of(m->group_ptr()) : 0;
}
// The lead point on what the port has; the exe's slot on a mech (vt+0x90).
struct LeadHooks : LeadPmHooks {
  float v_[2][3] = {}, m_[3] = {};
  int n_ = 0;
  const float* velocity(zgame::Handle e) override {
    float* o = v_[n_++ & 1];
    brain_hooks().velocity(e, o);
    return o;
  }
  const float* smoothed_velocity(zgame::Handle v) override {
    const zgame::Handle m = brain_hooks().vehicle_mech(v);
    return m ? &static_cast<Mech*>(brain_hooks().object_of(m))->target_velocity.x : nullptr;
  }
  void aim_point(zgame::Handle e, float out[3]) override { brain_hooks().entity_lock_position(e, out); }
  uint32_t weapon_flags(zgame::Handle w) override { return brain_hooks().weapon_flags(w); }
  float weapon_speed(zgame::Handle w) override { return brain_hooks().weapon_speed(w); }
  const float* muzzle(zgame::Handle w) override {
    brain_hooks().weapon_node_position(w, m_);
    return m_;
  }
};
// The weapon's line, 0x004b7800 (weapon_line), on what the port's weapon holds.
struct LineHooks : WeaponLinePmHooks {
  bool node_place(zgame::Handle node, float io[3]) override {
    const auto& k = mech3::weapon_calls();
    void* n = brain_hooks().object_of(node);
    if (!n) return true;
    return k.node_position && k.node_position(n, reinterpret_cast<Vec3f*>(io)) != 0;
  }
  const float* node_axis(zgame::Handle node) override {
    const auto& k = mech3::weapon_calls();
    const Vec3f* a = k.node_axis ? k.node_axis(brain_hooks().object_of(node)) : nullptr;
    return a ? &a->x : kZeros;
  }
  int32_t rand() override { return brain_hooks().rand(); }
  bool quiet() override { return false; }  // [0x0062f86c], a debug switch, off
};
WeaponLinePm line_of(const Weapon& w) {
  WeaponLinePm o;
  const auto& k = mech3::weapon_calls();
  o.damage = w.damage;
  o.part_node = w.part && k.part_node ? brain_hooks().handle_of(k.part_node(w.part)) : 0;
  o.node = brain_hooks().handle_of(w.node);
  if (w.firepoint) o.fp_place = w.firepoint->position, o.fp_radius = w.firepoint->radius, o.fp_tolerance = &w.firepoint->aim[0];
  o.salvo_left = w.salvo_left;
  o.spread[0] = w.spread.x, o.spread[1] = w.spread.y, o.spread[2] = w.spread.z;
  return o;
}
bool weapon_line_cos(Weapon* w, WeaponManager* m, float* cos) {
  LineHooks h;
  float mz[3], dir[3], kept;
  return weapon_line(h, line_of(*w), &m->aim.x, mz, dir, cos, &kept);
}

// What a weapon heats by the second, kept on the exe's weapon at +0x1c: 0x004b8ed0, for a beam 0x004b8f10.
float heat_rate(const Weapon& w) {
  const WeaponDef* d = w.def;
  if (!d) return 0.0f;
  const WeaponType* t = d->type;
  if (w.is_beam()) {
    double cycle = double{d->period} * t->fire_time / t->recharge_rate + t->fire_time;
    if (cycle != 0.0) cycle = 1.0 / cycle;
    return static_cast<float>(cycle * t->heat);
  }
  double rate = d->period;
  if (rate != 0.0) rate = 1.0 / rate;
  double heat = t->heat;
  if (w.salvo != 0) heat = heat * t->salvo_truecount;
  return static_cast<float>(heat * rate);
}

// A body part by the exe's offset on PM's mech.
void* part_at(zgame::Handle m, int32_t offset) {
  const Mech* k = static_cast<Mech*>(brain_hooks().object_of(m));
  switch (offset) {
    case 0x39c: return k->legs[0];
    case 0x3a0: return k->legs[1];
    case 0x3a4: return k->arms[0];
    case 0x3a8: return k->arms[1];
    case 0x3ac: return k->torso;
    case 0x3b4: return k->head;
    default: return nullptr;
  }
}

// 0x004bc930's top: the firing's own numbers (the exe's brain +0x124), and the contact's owner's heat and health.
ShotShooter shooter_of(const Contact* c) {
  ShotShooter o;
  const zgame::Handle e = c->owner;
  const FiringRecord* r = record_of(e);
  o.heat = brain_hooks().heat(e), o.health = brain_hooks().health(e);
  o.vehicle = brain_hooks().vehicle_mech(e) != 0;
  o.pilot = r != nullptr;
  if (r) {
    o.heat_most = r->fire.too_hot, o.rate_most = r->fire.x3c, o.cost_least = r->fire.x40;
    o.health_past = r->fire.x44, o.ammo_keep = r->fire.x48;
  }
  return o;
}
// A mech (vtable 0x005dfaac) or a basic vehicle (0x005eaeac): both answer 0 at vt+0x50, +0x68 and +0x100. Other
// classes' slots are unread, and counted.
bool known_vehicle(zgame::Handle e) { return brain_hooks().vehicle_mech(e) || brain_hooks().targetable(e); }
bool grouped(zgame::Handle v) { return manager_group(brain_hooks().weapons(v)) != 0; }

class WireAim : public AimPmHooks {
 public:
  Contact* plan_contact(zgame::Handle plan) override { return plan_of(plan) ? *plan_of(plan)->slot : nullptr; }
  const float* plan_position(zgame::Handle plan) override {
    PlanPm* p = plan_of(plan);
    return ai::plan_place(*p, *p->slot);
  }
  zgame::Handle contact_entity(const Contact* c) override { return brain_hooks().contact_entity(c); }
  bool entity_is_vehicle(zgame::Handle e) override {
    return brain_hooks().vehicle_mech(e) != 0;  // vt+0x48 on a mech is itself; other classes unread
  }
  uint32_t entity_pilot_kind(zgame::Handle e) override {
    const zgame::Handle m = brain_hooks().vehicle_mech(e);  // PM's mech +0x6a8 is its MechData, +0x0c the roster
    const Mech* k = m ? static_cast<const Mech*>(brain_hooks().object_of(m)) : nullptr;
    return k && k->pilot ? static_cast<uint32_t>(k->pilot->roster) : 0;
  }
  float difficulty_error() override {
    return brain_hooks().aim_intensity();  // [0x0062cc64], taken as MW3's [0x00608e58]
  }
  float pilot_error(Brain* b) override { return b->params.get<float>(0xb8); }
  float pilot_aid(Brain* b) override { return b->params.get<float>(0xc0); }
  bool targeting_aid(Brain* b) override { return brain_hooks().targeting_aid(vehicle_of(b)); }
  void vehicle_position(Brain* b, float out[3]) override { brain_hooks().position(vehicle_of(b), out); }
  bool weapon_line(zgame::Handle w, const float at[3], float muzzle[3], float dir[3]) override {
    LineHooks h;
    float c, kept;
    return ai::weapon_line(h, line_of(*weapon_of(w)), at, muzzle, dir, &c, &kept);
  }
  int32_t rand() override { return brain_hooks().rand(); }
  float now() override { return brain_hooks().now(); }
  void wm_aim_at(zgame::Handle wm, const float* at) override {
    if (at) brain_hooks().wm_aim_at(wm, at);
    else brain_hooks().wm_clear_aim(wm);
  }
  void wm_has_target(zgame::Handle wm, bool on) override {
    if (WeaponManager* m = wm_of(wm)) m->has_target = on;
  }
 private:
  float buf_[3] = {};
};

class WirePick : public PickPmHooks {
 public:
  std::vector<Contact*>& contacts(zgame::Handle v) override {
    AiTarget* t = brain_hooks().targetable(v);
    return t ? t->contacts : no_contacts();
  }
  Contact** target_slot(zgame::Handle v) override {
    AiTarget* t = brain_hooks().targetable(v);
    return t ? &t->target : nullptr;
  }
  uint32_t allies(zgame::Handle v) override {
    const AiTarget* t = brain_hooks().targetable(v);
    return t ? t->allies : 0;
  }
  const float* forward(zgame::Handle v) override {
    brain_hooks().forward(v, &buf_[0], &buf_[2]);
    buf_[1] = 0.0f;
    return buf_;
  }
  bool faces(zgame::Handle v, const float dir[3]) override {
    return brain_hooks().can_face(v, dir);  // vt+0x98 is MW3's vt+0x8c
  }
  bool in_sight(zgame::Handle v, zgame::Handle e) override {
    return brain_hooks().clear_line(v, e);
  }
  bool select(zgame::Handle v, Contact* c) override {
    AiTarget* t = brain_hooks().targetable(v);
    return t && t->set_target(c, false);
  }
  void select_none(zgame::Handle v) override { if (AiTarget* t = brain_hooks().targetable(v)) t->set_target(nullptr, false); }
  zgame::Handle contact_entity(const Contact* c) override { return brain_hooks().contact_entity(c); }
  zgame::Handle contact_mech(const Contact* c) override { return brain_hooks().vehicle_mech(brain_hooks().contact_entity(c)); }
  bool active(zgame::Handle e) override { return brain_hooks().entity_active(e); }
  uint32_t team(zgame::Handle e) override {
    const AiTarget* t = brain_hooks().targetable(e);
    return t ? t->team : 0;
  }
  uint8_t flags(zgame::Handle e) override { return brain_hooks().vehicle_flags(e); }
  zgame::Handle group(zgame::Handle e) override {
    return known_vehicle(e) ? 0 : (pm_hook_missing("WirePick.group"), zgame::Handle{0});  // 0x00406ce0 on both
  }
  uint32_t group_marker(zgame::Handle g) override { pm_hook_missing("WirePick.group_marker"); return {}; }
  uint32_t group_xe8(zgame::Handle g) override { pm_hook_missing("WirePick.group_xe8"); return {}; }
  zgame::Handle vehicle(zgame::Handle e) override {
    return e;  // 0x00402250 on a mech; dev 2 has the other classes
  }
  zgame::Handle vehicle_x58(zgame::Handle m) override {
    return brain_hooks().vehicle_mech(m);
  }
  uint32_t vehicle_x100(zgame::Handle m) override {
    return known_vehicle(m) ? 0 : (pm_hook_missing("WirePick.vehicle_x100"), 0u);  // 0x00406ce0 on both
  }
  uint32_t entity_x50(zgame::Handle e) override {
    return known_vehicle(e) ? 0 : (pm_hook_missing("WirePick.entity_x50"), 0u);  // 0x00406ce0 on both
  }
  float health(zgame::Handle e) override { return brain_hooks().health(e); }
  zgame::Handle mech(zgame::Handle e) override { return brain_hooks().vehicle_mech(e); }
  float heat(zgame::Handle m) override {
    return brain_hooks().heat(m);
  }
  zgame::Handle part(zgame::Handle m, int32_t offset) override {
    return brain_hooks().handle_of(part_at(m, offset));
  }
  uint32_t part_child(zgame::Handle m, int32_t leg) override {
    const Mech* k = static_cast<const Mech*>(brain_hooks().object_of(m));  // 0x0041e290: the leg's calf
    const Leg* l = k && (leg == 0 || leg == 1) ? k->legs[leg] : nullptr;
    return l && l->calf ? brain_hooks().handle_of(l->calf) : 0;
  }
  float part_health(zgame::Handle p) override {
    return static_cast<MechPart*>(brain_hooks().object_of(p))->get_internal(0);
  }
  const float* position(zgame::Handle e) override {
    brain_hooks().position(e, buf_);
    return buf_;
  }
  zgame::Handle player() override { return brain_hooks().player_vehicle(); }
  zgame::Handle threat(Brain* b, int32_t list) override {
    return list == 0 ? pm_latest_attacker(*b) : pm_hardest_attacker(*b);  // each changes the list, as the exe's
  }
  int32_t rand() override { return brain_hooks().rand(); }
  float now() override { return brain_hooks().now(); }
 private:
  float buf_[3] = {};
};

class WireShot : public ShotPmHooks {
 public:
  ShotShooter shooter(const Contact* c) override {
    return shooter_of(c);
  }
  const std::vector<zgame::Handle>& weapons(zgame::Handle wm) override {
    return manager_list(wm);
  }
  bool ready(zgame::Handle w) override {
    return weapon_of(w)->can_fire();
  }
  bool can_hit(zgame::Handle w, const float* at) override {
    return ai::can_hit(w, at);
  }
  double reach_least(zgame::Handle w) override {
    return weapon_of(w)->min_range_sq();
  }
  double reach_most(zgame::Handle w) override {
    return weapon_of(w)->max_range_sq();  // 0x004b7b90 is the square, 0x004b7b50 the range
  }
  float cost(zgame::Handle w) override {
    return heat_rate(*weapon_of(w));
  }
  float value(zgame::Handle w) override {
    return weapon_of(w)->rating;
  }
  double dps(zgame::Handle w) override {
    const Weapon* k = weapon_of(w);  // 0x004bc8f0, a beam's 0x004bc910: an eighth of its heat by the second
    if (k->is_beam()) return 0.125 * heat_rate(*k);
    return k->def ? k->def->type->heat : 0.0f;
  }
  int32_t ammo(zgame::Handle w) override {
    return weapon_of(w)->ammo();
  }
  bool bin(zgame::Handle w, int32_t* full) override {
    const Weapon* k = weapon_of(w);
    if (!k->bin) return false;
    *full = k->bin->capacity();
    return true;
  }
  void group_clear(zgame::Handle wm) override {
    if (WeaponGroup* g = group_of(manager_group(wm))) {
      while (g->count) g->remove(g->weapons[g->count - 1]);
    }
  }
  void group_add(zgame::Handle wm, zgame::Handle w, float rate) override {
    (void)rate;  // PM's +0x98 on the weapon: its readers aren't ported
    pm_hook_missing("fired: group_add");  // as the trigger's
    if (WeaponGroup* g = group_of(manager_group(wm))) g->add(weapon_of(w));
  }
  void set_mode(zgame::Handle wm, int32_t mode) override {
    if (WeaponManager* m = wm_of(wm)) m->mode = mode;
  }
 private:
  float buf_[3] = {};
};

class WirePlan : public PlanPmHooks {
 public:
  zgame::Handle contact_entity(const Contact* c) override { return brain_hooks().contact_entity(c); }
  zgame::Handle mech(zgame::Handle e) override { return brain_hooks().vehicle_mech(e); }
  void refresh(zgame::Handle m) override {
    brain_hooks().mech_aim_at_last_hit(m);  // 0x0041df20 is MW3's 0x0041d130
  }
  zgame::Handle wm(zgame::Handle v) override { return brain_hooks().weapons(v); }
  zgame::Handle group_weapon(zgame::Handle wm) override {
    return brain_hooks().wm_group_weapon(wm);
  }
  bool aim_point(zgame::Handle v, Contact* c, zgame::Handle weapon, float out[3]) override {
    LeadHooks h;
    return lead_point_pm(h, v, brain_hooks().contact_entity(c), weapon, out);
  }
  void weapon_place(zgame::Handle weapon, float out[3]) override {
    brain_hooks().weapon_node_position(weapon, out);
  }
  const float* position(zgame::Handle e) override {
    brain_hooks().position(e, buf_);
    return buf_;
  }
  const float* matrix(zgame::Handle v) override {
    brain_hooks().vehicle_matrix(v, m_);
    return m_;
  }
  bool turn(zgame::Handle v, const float dir[3], float out[3]) override {
    return brain_hooks().bearer_aim_clamp(v, dir, out);
  }
  void face(zgame::Handle v, const float dir[3]) override {
    brain_hooks().bearer_set_aim(v, dir);
  }
  void wm_aim_at(zgame::Handle wm, const float at[3]) override {
    if (at) brain_hooks().wm_aim_at(wm, at);
    else brain_hooks().wm_clear_aim(wm);
  }
  const std::vector<zgame::Handle>& weapons(zgame::Handle wm) override {
    return manager_list(wm);
  }
  const std::vector<zgame::Handle>& group(zgame::Handle g) override {
    const WeaponGroup* k = group_of(g);
    return k ? list_of(k->weapons, k->count) : no_handles();
  }
  bool weapon_cos(zgame::Handle w, zgame::Handle wm, float* cos) override {
    return weapon_line_cos(weapon_of(w), wm_of(wm), cos);
  }
  bool in_sight(zgame::Handle v, zgame::Handle e) override {
    return brain_hooks().clear_line(v, e);  // 0x004e5d00 is MW3's 0x004d5980
  }
  void part_place(uint32_t part, float out[3]) override {
    Vec3f at{0, 0, 0};  // 0x004ff250: the node's place in the world
    const auto& k = mech3::weapon_calls();
    void* node = k.part_node ? k.part_node(brain_hooks().object_of(part)) : nullptr;
    if (node && k.node_position) k.node_position(node, &at);
    out[0] = at.x, out[1] = at.y, out[2] = at.z;
  }
  const float* entity_place(zgame::Handle e) override {
    brain_hooks().position(e, buf_);
    return buf_;
  }
  bool has_type(zgame::Handle w) override {
    return weapon_of(w)->def != nullptr;
  }
  int32_t ammo(zgame::Handle w) override {
    return weapon_of(w)->ammo();
  }
  float blocked(zgame::Handle w) override {
    return weapon_of(w)->damage;
  }
  double reach(zgame::Handle w) override {
    return weapon_of(w)->max_range_sq();
  }
  zgame::Handle holder_mech(zgame::Handle w) override {
    return brain_hooks().handle_of(weapon_of(w)->manager->owner);
  }
  bool mounted(zgame::Handle w) override {
    const Weapon* k = weapon_of(w);
    return k->part && k->firepoint;
  }
  zgame::Handle muzzle_node(zgame::Handle w) override {
    return brain_hooks().handle_of(weapon_of(w)->node);
  }
  const float* mount_place(zgame::Handle w) override {
    return weapon_of(w)->firepoint->position;
  }
  bool spreads(zgame::Handle w) override {
    const Weapon* k = weapon_of(w);
    return k->salvo_left && k->firepoint;
  }
  float spread(zgame::Handle w) override {
    return weapon_of(w)->firepoint->radius;
  }
  const float* offset(zgame::Handle w) override {
    return &weapon_of(w)->spread.x;
  }
  bool buried(zgame::Handle node, float at[3]) override {
    if (!node) return true;
    return mech3::weapon_calls().node_position(brain_hooks().object_of(node), reinterpret_cast<Vec3f*>(at)) != 0;
  }
  bool has_mount(zgame::Handle w) override {
    return weapon_of(w)->firepoint != nullptr;
  }
  bool mount_reaches(zgame::Handle w, const float dir[3]) override {
    return firepoint_reaches(weapon_of(w)->firepoint->cone, dir);
  }
  bool mech_reaches(zgame::Handle m, const float dir[3]) override {
    const Torso* t = static_cast<Mech*>(brain_hooks().object_of(m))->torso;
    return torso_reaches(t->min_pitch, t->max_pitch, t->max_yaw, dir);
  }
  int32_t rand() override { return brain_hooks().rand(); }
 private:
  float m_[9] = {};
  float buf_[3] = {};
};

class WireStep : public StepPmHooks {
 public:
  zgame::Handle wm(zgame::Handle v) override { return brain_hooks().weapons(v); }
  zgame::Handle mech(zgame::Handle v) override { return brain_hooks().vehicle_mech(v); }
  Contact* target(zgame::Handle v) override {
    const AiTarget* t = brain_hooks().targetable(v);
    return t ? t->target : nullptr;
  }
  double heat(zgame::Handle m) override {
    return brain_hooks().heat(m);
  }
  float coolant(zgame::Handle m) override {  // PM's +0x590, the port's coolant
    const Mech* k = static_cast<const Mech*>(brain_hooks().object_of(m));
    return k ? k->coolant : 0.0f;
  }
  void flush(zgame::Handle m) override {  // 0x0041ad50 is MW3's 0x00419e60
    if (Mech* k = static_cast<Mech*>(brain_hooks().object_of(m))) k->flush_coolant();
  }
  float now() override { return brain_hooks().now(); }
  float hold(Brain* b) override { return b->params.get<float>(0x34); }
  float rest(Brain* b) override { return b->params.get<float>(0x38); }
  float range(Brain* b) override { return b->params.get<float>(0x30); }
  float rate(Brain* b) override {
    FiringRecord* r = record_of(vehicle_of(b));
    return r ? r->fire.x54 : 0.0f;
  }
  uint32_t xd0(Brain* b) override {
    return b->params.target_words[1];
  }
  void seed(uint32_t v) override { pm_hook_missing("WireStep.seed"); }
  void start_x50(uint32_t v) override { pm_hook_missing("WireStep.start_x50"); }
  void set_range(zgame::Handle v, float r) override {
    if (AiTarget* t = brain_hooks().targetable(v)) {  // 0x004e7150, with no cap
      t->range = r;
      t->range2 = static_cast<float>(double{r} * r);
    }
  }
  void look_start() override { pm_hook_missing("WireStep.look_start"); }
  zgame::Handle best_weapon(zgame::Handle wm) override {
    return ai::best_weapon(wm);
  }
  float weapon_reach(zgame::Handle w) override {
    return weapon_of(w)->max_range();
  }
  zgame::Handle pick(zgame::Handle v) override {
    FiringRecord* r = record_of(v);
    if (r) r->pick.reach = r->fire.range;
    PlanPm* p = r ? plan_pick(r->pick) : nullptr;
    if (p) p->seen = p->in_sight;  // one byte in the exe, +0x0d
    return pm_id(p);
  }
  void plan_free(zgame::Handle plan) override {
    PlanPm* p = plan_of(plan);
    pm_ids_forget(p);
    delete p;
  }
  bool plan_turn(zgame::Handle plan, float cone) override {
    PlanPm* p = plan_of(plan);
    return ai::plan_turn(*p, *p->slot, grouped(p->vehicle), cone);
  }
  bool plan_cone(zgame::Handle plan, float cone) override {
    PlanPm* p = plan_of(plan);
    const zgame::Handle wm = brain_hooks().weapons(p->vehicle);
    return ai::plan_cone(*p, *p->slot, wm, grouped(p->vehicle), group(wm), cone);
  }
  bool plan_sight(zgame::Handle plan) override {
    PlanPm* p = plan_of(plan);
    ai::plan_sight(*p, *p->slot);
    return p->seen;
  }
  bool plan_quick(zgame::Handle plan) override {
    const bool f = plan_of(plan)->faces != 0;
    pm_hook_missing(f ? "picked: facing" : "picked: not facing");  // for the first runs: 0x004bd1c0 follows the second
    return f;
  }
  bool plan_seen(zgame::Handle plan) override { return plan_of(plan)->seen; }
  Contact* plan_contact(zgame::Handle plan) override { return *plan_of(plan)->slot; }
  const float* plan_place(zgame::Handle plan, float hold, float rate) override {
    PlanPm* p = plan_of(plan);
    return ai::plan_place(*p, *p->slot);
  }
  bool plan_fired(zgame::Handle plan) override { return plan_of(plan)->fired; }
  void plan_set_fired(zgame::Handle plan, bool on) override { plan_of(plan)->fired = on; }
  bool shot(zgame::Handle wm, Contact* c, const float* at, float hold, float rate) override { return shot_pick(wm, c, at, hold, rate); }
  bool any_can_hit(zgame::Handle plan) override {
    PlanPm* p = plan_of(plan);
    return ai::any_can_hit(brain_hooks().weapons(p->vehicle), ai::plan_place(*p, *p->slot));
  }
  void too_far(Brain* b) override { pm_hook_missing("WireStep.too_far"); }
  void trigger(Brain* b, bool on) override {
    if (on) pm_hook_missing("fired: trigger");  // a count of what the first run did, not a gap
    static const bool trace = std::getenv("PM_TRACE") != nullptr;
    if (trace && on) std::printf("pm trace: %a fire v%u\n", brain_hooks().now(), b->controls->vehicle());
    b->controls->fire(on);
  }
  zgame::Handle group(zgame::Handle wm) override {
    return manager_group(wm);
  }
  double group_soonest(zgame::Handle g) override {
    const WeaponGroup* k = group_of(g);  // 0x004b5bf0: the least reload among those that can fire
    float least = 100000.0f;
    for (uint32_t i = 0; k && i < k->count; ++i) {
      const Weapon* w = k->weapons[i];
      if (!(w->reload < least) || !w->def || !w->enabled) continue;
      const int32_t n = w->ammo();
      if ((n > 0 || n == -1) && w->damage < 1.0f) least = w->reload;
    }
    return least;
  }
  bool clear_of_friends(zgame::Handle plan) override {
    PlanPm* p = plan_of(plan);  // the first contact in line, nearest first, is the target or none
    Contact* t = *p->slot;
    const AiTarget* me = brain_hooks().targetable(p->vehicle);
    if (!me || !t) return true;
    for (Contact* c : me->contacts)
      if (in_line(*t, *c)) return c == t;
    return true;
  }
  void aim_track(zgame::Handle plan, Brain* b) override { if (FiringRecord* r = record_of(plan_of(plan)->vehicle)) r->aim.track(plan, 0, b); }
  void aim_lead(float dt, zgame::Handle plan, zgame::Handle wm, Brain* b) override {
    if (FiringRecord* r = record_of(plan_of(plan)->vehicle)) r->aim.lead(dt, plan, brain_hooks().wm_current(wm), wm, b);
  }
  void stand_down(zgame::Handle v) override {
    // vt+0x164(0, &o, 1), the port's Mech::aim_at; then the torso turned to straight ahead (0x004677e0, 0x005eb958).
    const zgame::Handle m = brain_hooks().vehicle_mech(v);
    Mech* k = m ? static_cast<Mech*>(brain_hooks().object_of(m)) : nullptr;
    if (!k) {
      const zgame::Handle wm = brain_hooks().weapons(v);
      if (wm) brain_hooks().wm_clear_aim(wm);
      return;
    }
    uint8_t out[16] = {};
    k->aim_at(nullptr, out, true);
    static const Vec3 ahead{0.0f, 0.0f, -1.0f};
    if (k->torso && aim_calls().torso_aim) aim_calls().torso_aim(*k, &ahead, out, 0);
  }
  void brain_x20(Brain* b) override {
    pm_walk(*b, *b->controls).set_fought(false);  // vt+0x20 0x004faa30: the brain's +0x118 to 0
  }
 private:
  float buf_[3] = {};
};

}  // namespace

FiringRecord& pm_firing(Brain& b, zgame::Handle vehicle) {
  const bool fresh = !records().count(vehicle);
  FiringRecord& r = records()[vehicle];
  if (fresh) r.fire.seed();
  {  // the pilot's numbers, read where they live as the exe does (PM's allies_turn sets a filter bit in play)
    const AiParams& p = b.params;
    r.pick.pilot.repick = zgame::from_bits(p.target_words[0]);
    r.pick.pilot.pick_order = static_cast<int32_t>(p.target_words[2]);
    r.pick.pilot.part_mode = static_cast<int32_t>(p.target_words[3]);
    for (int i = 0; i < 4; ++i) r.pick.pilot.weights[i] = zgame::from_bits(p.target_words[4 + i]);
    r.pick.pilot.range = p.get<float>(0x30);
    r.pick.pilot.filter[0] = p.raw[0xcd], r.pick.pilot.filter[1] = p.raw[0xce];
    r.pick.pilot.marker = b.own_marker;
  }
  r.fire.vehicle = r.pick.vehicle = vehicle;
  r.fire.brain = r.pick.brain = &b;
  return r;
}

void pm_firing_forget(zgame::Handle vehicle) { records().erase(vehicle); }

void pm_firing_install() {
  static WireAim aim;
  static WirePick pick;
  static WireShot shot;
  static WirePlan plan;
  static WireStep step;
  set_aim_pm_hooks(&aim);
  set_pick_pm_hooks(&pick);
  set_shot_pm_hooks(&shot);
  set_plan_pm_hooks(&plan);
  set_step_pm_hooks(&step);
}

}  // namespace mech3::ai
