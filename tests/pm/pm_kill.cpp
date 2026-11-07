// test_pm_mission.cpp's kill test, the parts and weapons it reaches: these headers don't sit with options.h.
#include "mech3/mech.h"
#include "mech3/mech_parts.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/class.h"
#include "gamez/zweapon/zweapon.h"
#include "mech3/app_mech.h"
#include "mech3/mech3_interp.h"
#include "mech3/mission_world.h"
#include "mech3/weapon.h"

#include <cstdio>
#include <string>

using namespace mech3;

namespace {
MechPart* (*g_node_part)(Handle) = nullptr;
}
int pm_kill_on_part = 0, pm_kill_off_part = 0;
static bool pm_leg_log = false;
static void* pm_leg_target = nullptr;
static float pm_leg_damage = 0;
static const char* pm_leg_def = "-";
static std::string pm_leg_line;
static int pm_leg_off_leg = 0;
static void* me_node = nullptr;

void pm_kill_begin(void* player) {
  auto& d = damage_calls();
  g_node_part = d.node_part;
  static decltype(d.controls_hit) old_hit;
  old_hit = d.controls_hit;
  d.controls_hit = [](Mech& m, WeaponDef* def, Collision* coll, float damage, Handle attacker) {
    pm_leg_damage = damage, pm_leg_def = def && def->desc ? def->desc : "-";
    if (old_hit) old_hit(m, def, coll, damage, attacker);
    else if (m.controls) m.controls->on_hit(attacker);
  };
  d.node_part = [](Handle n) -> MechPart* {
    MechPart* part = g_node_part(n);
    ++(part ? pm_kill_on_part : pm_kill_off_part);
    if (pm_leg_log && damage_calls().node_owner(n) == pm_leg_target) {
      auto* z = static_cast<zcls::Node*>(n);
      const auto& hs = zwep::hit_state();
      auto* by = static_cast<zcls::Node*>(hs.attacker);
      char line[240];
      std::snprintf(line, sizeof line, "%s id %x flags %x, %s, %.3f from %s by %s, scale %.5f, shot end %.1f %.1f %.1f", z ? z->name() : "-",
                    z ? z->get<uint32_t>(0x28) : 0, z ? z->flags() : 0, part ? "part" : "SPREAD", pm_leg_damage, pm_leg_def, by ? by->name() : "-",
                    interp_globals().damage_scale, hs.shot_to.x, hs.shot_to.y, hs.shot_to.z);
      pm_leg_line = line;
      auto* m = static_cast<Mech*>(pm_leg_target);
      if (by && by == static_cast<zcls::Node*>(me_node) && (!part || (part != m->legs[0] && part != m->legs[1] && part != m->legs[0]->calf && part != m->legs[1]->calf &&
                                                                     part != m->legs[0]->foot && part != m->legs[1]->foot)))
        ++pm_leg_off_leg;
    }
    static int shown = 0;
    if (!part && shown++ < 12) {
      auto* z = static_cast<zcls::Node*>(n);
      Handle owner = damage_calls().node_owner ? damage_calls().node_owner(n) : nullptr;
      std::printf("  off a part: node %s id %x flags %x class %d children %d, owner %p, node_owner %p\n", z ? z->name() : "-", z ? z->get<uint32_t>(0x28) : 0,
                  z ? z->flags() : 0, z ? z->cls() : -1, z ? z->child_count() : -1, z ? z->owner : nullptr, owner);
    }
    return part;
  };
  static_cast<Mech*>(player)->invincible = true;
  me_node = static_cast<Mech*>(player)->node;
}

int pm_kill_fire(void* player, const float at[3]) {
  auto* me = static_cast<Mech*>(player);
  auto* wm = static_cast<WeaponManager*>(me->ammo);
  me->heat = 0.0f;
  const Vec3f aim{at[0], at[1], at[2]};
  int shots = 0;
  for (uint32_t i = 0; i < wm->count; ++i) {
    Weapon* w = wm->weapons[i];
    if (!w) continue;
    w->reload = 0.0f;
    shots += w->aim_and_fire(&aim, true) ? 1 : 0;
  }
  return shots;
}

void pm_kill_left(void* mech, char* out, size_t n) {
  const auto& m = *static_cast<Mech*>(mech);
  std::snprintf(out, n, "torso %.3f/%.3f sides %.3f %.3f rear %.3f head %.3f/%.3f legs %.3f %.3f arms %.3f %.3f",
                m.torso->get_internal(0), m.torso->get_armor(0), m.torso->get_internal(1), m.torso->get_internal(2),
                m.torso->get_armor(3), m.head->internal, m.head->armor, m.legs[0]->internal, m.legs[1]->internal,
                m.arms[0] ? m.arms[0]->internal : -1.0f, m.arms[1] ? m.arms[1]->internal : -1.0f);
}

// What a straight line finds on its way to the target, and how many hits the mechs have taken.
void pm_kill_ray(const float from[3], const float to[3]) {
  void* world = world_hooks().world;
  static zcls::LineHits r;
  const int32_t res = zcls::segment_nearest(static_cast<zcls::Node*>(world), from, to, &r);
  std::printf("  ray: result %d, %d hits", res, r.count);
  for (int i = 0; i < r.count && i < 6; ++i) {
    auto* n = static_cast<zcls::Node*>(r.hit[i].node);
    std::printf("; node %s id %x", n ? n->name() : "-", n ? n->get<uint32_t>(0x28) : 0);
  }
  void* who = nullptr;
  std::printf("; hits taken %d\n", mech_hits_taken(&who));
}

uint32_t pm_kill_root_flags(void* mech) { return static_cast<zcls::Node*>(static_cast<Mech*>(mech)->node)->flags(); }

static void tree(zcls::Node* n, int depth) {
  if (depth > 3) return;
  std::printf("  %*s%s class %d id %x flags %x model %p pm_model %p children %d\n", depth * 2, "", n->name(), n->cls(), n->get<uint32_t>(0x28), n->flags(),
              static_cast<const void*>(n->model), static_cast<const void*>(n->pm_model), n->child_count());
  for (int32_t i = 0; i < n->child_count(); ++i) tree(n->children[i], depth + 1);
}
void pm_kill_tree(void* mech) { tree(static_cast<zcls::Node*>(static_cast<Mech*>(mech)->node), 0); }

void pm_kill_weapons(void* mech) {
  auto* wm = static_cast<WeaponManager*>(static_cast<Mech*>(mech)->ammo);
  for (uint32_t i = 0; i < wm->count; ++i) {
    Weapon* w = wm->weapons[i];
    if (!w || !w->def) continue;
    std::printf("  weapon %u: %s damage %.2f splash^2 %.1f flags %x\n", i, w->def->desc, w->def->damage, w->def->impact_proximity_sq, static_cast<uint32_t>(w->def->flags));
  }
}
void pm_kill_scales(void* mech) {
  const auto& m = *static_cast<Mech*>(mech);
  std::printf("  scales, armor/internal: torso %.5f/%.5f rear %.5f sides %.5f/%.5f head %.5f/%.5f leg %.5f/%.5f arm %.5f/%.5f\n", m.torso->get_armor_scale(0),
              m.torso->get_internal_scale(0), m.torso->get_armor_scale(3), m.torso->get_armor_scale(1), m.torso->get_internal_scale(1), m.head->get_armor_scale(0),
              m.head->get_internal_scale(0), m.legs[0]->get_armor_scale(0), m.legs[0]->get_internal_scale(0), m.arms[0] ? m.arms[0]->get_armor_scale(0) : 0.f,
              m.arms[0] ? m.arms[0]->get_internal_scale(0) : 0.f);
}

// One weapon, fired only when it's ready, at `leg` (0, 1) of `mech`: its calf's node, or the thigh's. Prints each hit: the node the query
// reported, the path, then every section's health. Returns the shots fired so far by this weapon.
static int leg_hits = 0;
int pm_leg_fire(void* player, void* mech, int weapon, int leg, int shown) {
  auto* me = static_cast<Mech*>(player);
  auto* m = static_cast<Mech*>(mech);
  auto* wm = static_cast<WeaponManager*>(me->ammo);
  me->heat = 0.0f;
  pm_leg_log = true;
  pm_leg_target = mech;
  Weapon* w = wm->weapons[weapon];
  float at[3] = {};  // in and out: an offset from the node
  zcls::node_world_point(static_cast<zcls::Node*>(m->legs[leg]->calf ? m->legs[leg]->calf->node : m->legs[leg]->node), at);
  const Vec3f aim{at[0], at[1], at[2]};
  int fired = 0;
  wm->aim = aim;
  if (w && w->can_fire()) fired = w->trigger() ? 1 : 0;  // the real route, a beam's too
  if (!pm_leg_line.empty()) {
    if (leg_hits++ < shown) {
      char left[200];
      pm_kill_left(mech, left, sizeof left);
      std::printf("  hit %d, aimed %.1f %.1f %.1f: %s\n     %s\n", leg_hits, at[0], at[1], at[2], pm_leg_line.c_str(), left);
    }
    pm_leg_line.clear();
  }
  return fired;
}
void pm_leg_reset() { leg_hits = 0, pm_leg_off_leg = 0, pm_leg_line.clear(); }
const char* pm_weapon_name(void* player, int i) {
  auto* wm = static_cast<WeaponManager*>(static_cast<Mech*>(player)->ammo);
  return wm->weapons[i] && wm->weapons[i]->def ? wm->weapons[i]->def->desc : "?";
}

// The leg's gone, and everything the player landed on this mech landed on a leg (the rest of the mission shoots it too).
bool pm_kill_only_leg_lost(void* mech, int leg) { return static_cast<Mech*>(mech)->legs[leg]->internal == 0.0f && pm_leg_off_leg == 0; }
