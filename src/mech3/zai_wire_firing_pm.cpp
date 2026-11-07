#include "mech3/zai_wire_firing_pm.h"

#include <map>

#include "mech3/zai_target.h"
#include "mech3/zai_wire_pm.h"

namespace mech3::ai {

namespace {
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
bool grouped(zgame::Handle) {
  pm_hook_missing("wm+0x2c.group");
  return false;
}

class WireAim : public AimPmHooks {
 public:
  Contact* plan_contact(zgame::Handle plan) override { return plan_of(plan) ? *plan_of(plan)->slot : nullptr; }
  const float* plan_position(zgame::Handle plan) override { pm_hook_missing("WireAim.plan_position"); return nullptr; }
  zgame::Handle contact_entity(const Contact* c) override { return brain_hooks().contact_entity(c); }
  bool entity_is_vehicle(zgame::Handle e) override { pm_hook_missing("WireAim.entity_is_vehicle"); return {}; }
  uint32_t entity_pilot_kind(zgame::Handle e) override { pm_hook_missing("WireAim.entity_pilot_kind"); return {}; }
  float difficulty_error() override { pm_hook_missing("WireAim.difficulty_error"); return {}; }
  float pilot_error(Brain* b) override { return b->params.get<float>(0xb8); }
  float pilot_aid(Brain* b) override { return b->params.get<float>(0xc0); }
  bool targeting_aid(Brain* b) override { return brain_hooks().targeting_aid(vehicle_of(b)); }
  void vehicle_position(Brain* b, float out[3]) override { brain_hooks().position(vehicle_of(b), out); }
  void weapon_line(zgame::Handle w, const float at[3], float muzzle[3], float dir[3]) override { pm_hook_missing("WireAim.weapon_line"); }
  int32_t rand() override { return brain_hooks().rand(); }
  float now() override { return brain_hooks().now(); }
  void wm_aim_at(zgame::Handle wm, const float* at) override { pm_hook_missing("WireAim.wm_aim_at"); }
  void wm_has_target(zgame::Handle wm, bool on) override { pm_hook_missing("WireAim.wm_has_target"); }
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
  bool faces(zgame::Handle v, const float dir[3]) override { pm_hook_missing("WirePick.faces"); return {}; }
  bool in_sight(zgame::Handle v, zgame::Handle e) override { pm_hook_missing("WirePick.in_sight"); return {}; }
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
  zgame::Handle group(zgame::Handle e) override { pm_hook_missing("WirePick.group"); return {}; }
  uint32_t group_marker(zgame::Handle g) override { pm_hook_missing("WirePick.group_marker"); return {}; }
  uint32_t group_xe8(zgame::Handle g) override { pm_hook_missing("WirePick.group_xe8"); return {}; }
  zgame::Handle vehicle(zgame::Handle e) override { pm_hook_missing("WirePick.vehicle"); return {}; }
  zgame::Handle vehicle_x58(zgame::Handle m) override { pm_hook_missing("WirePick.vehicle_x58"); return {}; }
  uint32_t vehicle_x100(zgame::Handle m) override { pm_hook_missing("WirePick.vehicle_x100"); return {}; }
  uint32_t entity_x50(zgame::Handle e) override { pm_hook_missing("WirePick.entity_x50"); return {}; }
  float health(zgame::Handle e) override { return brain_hooks().health(e); }
  zgame::Handle mech(zgame::Handle e) override { return brain_hooks().vehicle_mech(e); }
  float heat(zgame::Handle m) override { pm_hook_missing("WirePick.heat"); return {}; }
  zgame::Handle part(zgame::Handle m, int32_t offset) override { pm_hook_missing("WirePick.part"); return {}; }
  uint32_t part_child(zgame::Handle m, int32_t leg) override { pm_hook_missing("WirePick.part_child"); return {}; }
  float part_health(zgame::Handle p) override { pm_hook_missing("WirePick.part_health"); return {}; }
  const float* position(zgame::Handle e) override {
    brain_hooks().position(e, buf_);
    return buf_;
  }
  zgame::Handle player() override { return brain_hooks().player_vehicle(); }
  zgame::Handle threat(Brain* b, int32_t list) override { pm_hook_missing("WirePick.threat"); return {}; }
  int32_t rand() override { return brain_hooks().rand(); }
  float now() override { return brain_hooks().now(); }
 private:
  float buf_[3] = {};
};

class WireShot : public ShotPmHooks {
 public:
  ShotShooter shooter(const Contact* c) override { pm_hook_missing("WireShot.shooter"); return {}; }
  const std::vector<zgame::Handle>& weapons(zgame::Handle wm) override { pm_hook_missing("WireShot.weapons"); return no_handles(); }
  bool ready(zgame::Handle w) override { pm_hook_missing("WireShot.ready"); return {}; }
  bool can_hit(zgame::Handle w, const float* at) override { pm_hook_missing("WireShot.can_hit"); return {}; }
  double reach_least(zgame::Handle w) override { pm_hook_missing("WireShot.reach_least"); return {}; }
  double reach_most(zgame::Handle w) override { pm_hook_missing("WireShot.reach_most"); return {}; }
  float cost(zgame::Handle w) override { pm_hook_missing("WireShot.cost"); return {}; }
  float value(zgame::Handle w) override { pm_hook_missing("WireShot.value"); return {}; }
  double dps(zgame::Handle w) override { pm_hook_missing("WireShot.dps"); return {}; }
  int32_t ammo(zgame::Handle w) override { pm_hook_missing("WireShot.ammo"); return {}; }
  bool bin(zgame::Handle w, int32_t* full) override { pm_hook_missing("WireShot.bin"); return {}; }
  void group_clear(zgame::Handle wm) override { pm_hook_missing("WireShot.group_clear"); }
  void group_add(zgame::Handle wm, zgame::Handle w, float rate) override { pm_hook_missing("WireShot.group_add"); }
  void set_mode(zgame::Handle wm, int32_t mode) override { pm_hook_missing("WireShot.set_mode"); }
 private:
  float buf_[3] = {};
};

class WirePlan : public PlanPmHooks {
 public:
  zgame::Handle contact_entity(const Contact* c) override { return brain_hooks().contact_entity(c); }
  zgame::Handle mech(zgame::Handle e) override { return brain_hooks().vehicle_mech(e); }
  void refresh(zgame::Handle m) override { pm_hook_missing("WirePlan.refresh"); }
  zgame::Handle wm(zgame::Handle v) override { return brain_hooks().weapons(v); }
  zgame::Handle group_weapon(zgame::Handle wm) override { pm_hook_missing("WirePlan.group_weapon"); return {}; }
  bool aim_point(zgame::Handle v, Contact* c, zgame::Handle weapon, float out[3]) override { pm_hook_missing("WirePlan.aim_point"); return {}; }
  void weapon_place(zgame::Handle weapon, float out[3]) override { pm_hook_missing("WirePlan.weapon_place"); }
  const float* position(zgame::Handle e) override {
    brain_hooks().position(e, buf_);
    return buf_;
  }
  const float* matrix(zgame::Handle v) override { pm_hook_missing("WirePlan.matrix"); return nullptr; }
  bool turn(zgame::Handle v, const float dir[3], float out[3]) override { pm_hook_missing("WirePlan.turn"); return {}; }
  void face(zgame::Handle v, const float dir[3]) override { pm_hook_missing("WirePlan.face"); }
  void wm_aim_at(zgame::Handle wm, const float at[3]) override { pm_hook_missing("WirePlan.wm_aim_at"); }
  const std::vector<zgame::Handle>& weapons(zgame::Handle wm) override { pm_hook_missing("WirePlan.weapons"); return no_handles(); }
  const std::vector<zgame::Handle>& group(zgame::Handle g) override { pm_hook_missing("WirePlan.group"); return no_handles(); }
  bool weapon_cos(zgame::Handle w, zgame::Handle wm, float* cos) override { pm_hook_missing("WirePlan.weapon_cos"); return {}; }
  bool in_sight(zgame::Handle v, zgame::Handle e) override { pm_hook_missing("WirePlan.in_sight"); return {}; }
  void part_place(uint32_t part, float out[3]) override { pm_hook_missing("WirePlan.part_place"); }
  const float* entity_place(zgame::Handle e) override {
    brain_hooks().position(e, buf_);
    return buf_;
  }
  bool has_type(zgame::Handle w) override { pm_hook_missing("WirePlan.has_type"); return {}; }
  int32_t ammo(zgame::Handle w) override { pm_hook_missing("WirePlan.ammo"); return {}; }
  float blocked(zgame::Handle w) override { pm_hook_missing("WirePlan.blocked"); return {}; }
  double reach(zgame::Handle w) override { pm_hook_missing("WirePlan.reach"); return {}; }
  zgame::Handle holder_mech(zgame::Handle w) override { pm_hook_missing("WirePlan.holder_mech"); return {}; }
  bool mounted(zgame::Handle w) override { pm_hook_missing("WirePlan.mounted"); return {}; }
  zgame::Handle muzzle_node(zgame::Handle w) override { pm_hook_missing("WirePlan.muzzle_node"); return {}; }
  const float* mount_place(zgame::Handle w) override { pm_hook_missing("WirePlan.mount_place"); return nullptr; }
  bool spreads(zgame::Handle w) override { pm_hook_missing("WirePlan.spreads"); return {}; }
  float spread(zgame::Handle w) override { pm_hook_missing("WirePlan.spread"); return {}; }
  const float* offset(zgame::Handle w) override { pm_hook_missing("WirePlan.offset"); return nullptr; }
  bool buried(zgame::Handle node, const float at[3]) override { pm_hook_missing("WirePlan.buried"); return {}; }
  bool has_mount(zgame::Handle w) override { pm_hook_missing("WirePlan.has_mount"); return {}; }
  bool mount_reaches(zgame::Handle w, const float dir[3]) override { pm_hook_missing("WirePlan.mount_reaches"); return {}; }
  bool mech_reaches(zgame::Handle m, const float dir[3]) override { pm_hook_missing("WirePlan.mech_reaches"); return {}; }
  int32_t rand() override { return brain_hooks().rand(); }
 private:
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
  double heat(zgame::Handle m) override { pm_hook_missing("WireStep.heat"); return {}; }
  float coolant(zgame::Handle m) override { pm_hook_missing("WireStep.coolant"); return {}; }
  void flush(zgame::Handle m) override { pm_hook_missing("WireStep.flush"); }
  float now() override { return brain_hooks().now(); }
  float hold(Brain* b) override { return b->params.get<float>(0x34); }
  float rest(Brain* b) override { return b->params.get<float>(0x38); }
  float range(Brain* b) override { return b->params.get<float>(0x30); }
  float rate(Brain* b) override { pm_hook_missing("WireStep.rate"); return {}; }
  uint32_t xd0(Brain* b) override { pm_hook_missing("WireStep.xd0"); return {}; }
  void seed(uint32_t v) override { pm_hook_missing("WireStep.seed"); }
  void start_x50(uint32_t v) override { pm_hook_missing("WireStep.start_x50"); }
  void set_range(zgame::Handle v, float r) override { pm_hook_missing("WireStep.set_range"); }
  void look_start() override { pm_hook_missing("WireStep.look_start"); }
  zgame::Handle best_weapon(zgame::Handle wm) override { pm_hook_missing("WireStep.best_weapon"); return {}; }
  float weapon_reach(zgame::Handle w) override { pm_hook_missing("WireStep.weapon_reach"); return {}; }
  zgame::Handle pick(zgame::Handle v) override {
    FiringRecord* r = record_of(v);
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
  bool plan_quick(zgame::Handle plan) override { return plan_of(plan)->faces != 0; }
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
  void trigger(Brain* b, bool on) override { pm_hook_missing("WireStep.trigger"); }
  zgame::Handle group(zgame::Handle wm) override { pm_hook_missing("WireStep.group"); return {}; }
  double group_soonest(zgame::Handle g) override { pm_hook_missing("WireStep.group_soonest"); return {}; }
  bool clear_of_friends(zgame::Handle plan) override { pm_hook_missing("WireStep.clear_of_friends"); return {}; }
  void aim_track(zgame::Handle plan, Brain* b) override { if (FiringRecord* r = record_of(plan_of(plan)->vehicle)) r->aim.track(plan, 0, b); }
  void aim_lead(float dt, zgame::Handle plan, zgame::Handle wm, Brain* b) override { pm_hook_missing("WireStep.aim_lead"); }
  void stand_down(zgame::Handle v) override { pm_hook_missing("WireStep.stand_down"); }
  void brain_x20(Brain* b) override { pm_hook_missing("WireStep.brain_x20"); }
 private:
  float buf_[3] = {};
};

}  // namespace

FiringRecord& pm_firing(Brain& b, zgame::Handle vehicle) {
  FiringRecord& r = records()[vehicle];
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
