#include "gamez/zsound/zsnd.h"
// test_pm_mission.cpp's kill test, the parts and weapons it reaches: these headers don't sit with options.h.
#include "mech3/mech.h"
#include "mech3/mech_build.h"
#include "mech3/mech_parts.h"
#include "mech3/mech_headlight.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/list.h"
#include "gamez/zweapon/zweapon.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/app_hud.h"
#include "mech3/app_mech.h"
#include "mech3/app_turret.h"
#include "mech3/turret.h"
#include "mech3/mech3_interp.h"
#include "mech3/mission_world.h"
#include "mech3/weapon.h"
#include "mech3/mech_ids.h"
#include "gamez/zclass/object3d.h"
#include "mech3/mech_reader.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_steps.h"
#include "mech3/mech_control.h"
#include "mech3/cheats.h"
#include <typeinfo>

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <map>
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
      if (!pm_leg_line.empty()) pm_leg_line += "\n      and ";
      pm_leg_line += line;
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
int pm_leg_fire(void* player, void* mech, int weapon, int leg, int shown, bool ready) {
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
  if (w && ready) {  // no waiting on the reload or, between beams, the recharge
    w->reload = 0.0f;
    if (auto* b = dynamic_cast<BeamWeapon*>(w); b && !b->firing) b->bin->amount = b->bin->capacity();
  }
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

// One missile weapon at a mech at range, a shot at a time: every missile's nearest pass, its last distance and how
// many reached the target's damage (PM_MISSILE=weapon,lock,artemis,range).
namespace {
struct Flight {
  float nearest = 1e9f, last = 0, age = 0, start = 0;
  int frames = 0;
  bool locked = false, seen = false;
  float track = 0;
  int follow = 0;
};
std::map<zwep::Projectile*, Flight> g_flights;
int g_missile_hits = 0;
void* g_missile_target = nullptr;
}  // namespace

int pm_missile_step(void* player, void* mech, int weapon, bool lock, bool artemis) {
  auto* me = static_cast<Mech*>(player);
  auto* m = static_cast<Mech*>(mech);
  auto* wm = static_cast<WeaponManager*>(me->ammo);
  me->heat = 0.0f;
  Weapon* w = wm->weapons[weapon];
  static decltype(damage_calls().controls_hit) old_hit;
  if (g_missile_target != mech) {
    if (!g_missile_target) {
      old_hit = damage_calls().controls_hit;
      damage_calls().controls_hit = [](Mech& t, WeaponDef* def, Collision* coll, float damage, Handle attacker) {
        if (&t == g_missile_target && attacker == static_cast<Handle>(mech_mission_player())) ++g_missile_hits;
        if (old_hit) old_hit(t, def, coll, damage, attacker);
        else if (t.controls) t.controls->on_hit(attacker);
      };
    }
    g_missile_target = mech;
    bool set = false;
    if (lock) std::printf("  lock group %d", program_hud_lock_on(mech, &set)), std::printf(", target set %d\n", set ? 1 : 0);
    if (artemis) {
      weapon_calls().fcs_count = [](Handle) { return 1; };
      weapon_calls().equipment_in_part = [](Handle, int32_t, int32_t) { return true; };
    }
  }
  float at[3] = {};
  zcls::node_world_point(static_cast<zcls::Node*>(m->torso->node), at);
  // The real route: the reticle's aim and the HUD's lock, the shot fired when the lock shows
  int fired = 0;
  static int lock_frames = 0;
  if (wm->has_target) ++lock_frames;
  if (w && w->can_fire() && g_flights.empty() && (!lock || wm->has_target)) fired = w->trigger() ? 1 : 0;
  static int salvo_left_max = 0;
  salvo_left_max = std::max(salvo_left_max, static_cast<int>(w->salvo_left));
  if (fired) std::printf("  fired: salvo %d left %d, reload %.2f, locked %d, lock frames so far %d\n", static_cast<int>(w->salvo), static_cast<int>(w->salvo_left), w->reload, w->locked ? 1 : 0, lock_frames);
  // The flights
  std::map<zwep::Projectile*, bool> live;
  for (auto* p = static_cast<zwep::Projectile*>(w->def->active); p; p = p->next) {
    live[p] = true;
    Flight& f = g_flights[p];
    const float d = std::sqrt((p->position.x - at[0]) * (p->position.x - at[0]) + (p->position.y - at[1]) * (p->position.y - at[1]) +
                              (p->position.z - at[2]) * (p->position.z - at[2]));
    if (!f.seen) f.seen = true, f.start = d, f.locked = p->target_at != nullptr, f.track = p->track_scale[0], f.follow = p->terrain_follow;
    f.nearest = std::min(f.nearest, d), f.last = d, f.age = p->age, ++f.frames;
  }
  for (auto it = g_flights.begin(); it != g_flights.end();) {
    if (live.count(it->first)) { ++it; continue; }
    const Flight& f = it->second;
    std::printf("  missile: start %.0f nearest %.1f last %.1f age %.2f frames %d locked %d track %.1f follow %d\n", f.start, f.nearest, f.last, f.age, f.frames, f.locked ? 1 : 0, f.track, f.follow);
    it = g_flights.erase(it);
  }
  return fired;
}
int pm_missile_hits() { return g_missile_hits; }
// A mech held powered down (a test's scenario without its fire): power(1) unless down or on its way down.
void pm_hold_down(void* mech) {
  auto* m = static_cast<Mech*>(mech);
  if (!m->is_powered_down() && !m->power_down_pending) m->power(1);
}

// pm-tests, a shot's flight under the real loop (PM_SHOT_FLIGHT): the player's first weapon that fires rounds fired
// once, its newest round followed while it lives (at most 30 frames): the distance it covered against its speed times
// its age, and its frame stamp against the counter. 0 while following, 1 measured, -1 no such weapon.
int pm_shot_flight(void* player, int since, float* travelled, float* expected, int* seen, char* name, size_t n) {
  static zwep::Projectile* shot = nullptr;
  static float from[3], speed = 0;
  static int frames = 0;
  static Weapon* w = nullptr;
  auto* me = static_cast<Mech*>(player);
  auto* wm = static_cast<WeaponManager*>(me->ammo);
  me->heat = 0.0f;
  if (since == 1) {
    shot = nullptr, w = nullptr, frames = 0;
    for (uint32_t i = 0; wm && i < wm->count && !w; ++i)
      if (Weapon* c = wm->weapons[i]; c && !c->is_beam() && c->def) w = c;
    if (!w) return -1;
    std::snprintf(name, n, "%s", w->def->desc ? w->def->desc : "?");
  }
  if (!w) return -1;
  if (!shot) {
    if (w->can_fire()) w->trigger();
    zwep::Projectile* newest = nullptr;
    for (auto* p = static_cast<zwep::Projectile*>(w->def->active); p; p = p->next) newest = p;
    if (!newest) return since > 600 ? -1 : 0;
    shot = newest;
    from[0] = shot->position.x, from[1] = shot->position.y, from[2] = shot->position.z;
    std::printf("  shot of %s: stamp %u, frame %u, velocity %.1f\n", name, shot->frame_stamp, zcls::frames().frame,
                std::sqrt(shot->velocity.x * shot->velocity.x + shot->velocity.y * shot->velocity.y + shot->velocity.z * shot->velocity.z));
    return 0;
  }
  bool alive = false;
  for (auto* p = static_cast<zwep::Projectile*>(w->def->active); p; p = p->next) alive = alive || p == shot;
  if (alive) {
    const float dx = shot->position.x - from[0], dy = shot->position.y - from[1], dz = shot->position.z - from[2];
    *travelled = std::sqrt(dx * dx + dy * dy + dz * dz);
    speed = std::sqrt(shot->velocity.x * shot->velocity.x + shot->velocity.y * shot->velocity.y + shot->velocity.z * shot->velocity.z);
    *expected = speed * shot->age;
    ++frames;
    if (frames <= 3 || frames % 10 == 0)
      std::printf("  frame %u: age %.3f travelled %.1f (speed x age %.1f), stamp %u\n", zcls::frames().frame, shot->age, *travelled, *expected, shot->frame_stamp);
  }
  *seen = frames;
  return !alive || frames >= 30 ? 1 : 0;
}

// The ground under a point, from above.
float pm_kill_ground(float x, float y, float z) {
  float probe[3] = {x, y, z};
  static zcls::GroundHits g;
  zcls::ground_query(static_cast<zcls::Node*>(world_hooks().world), probe, 1, y, &g);
  float best = -1e9f;
  for (int i = 0; i < g.count; ++i)
    if (g.hit[i].height > best) best = g.hit[i].height;
  return best;
}

// PM_GROUND: every polygon under a line of points across the canyon.
void pm_kill_ground_line(float x0, float z0, float x1, float z1) {
  for (int i = 0; i <= 16; ++i) {
    const float x = x0 + (x1 - x0) * static_cast<float>(i) / 16.0f, z = z0 + (z1 - z0) * static_cast<float>(i) / 16.0f;
    float probe[3] = {x, 300.0f, z};
    static zcls::GroundHits g;
    const int32_t mode = zmod::zones().mode;
    zmod::zones().mode = 0;  // every zone's ground
    zcls::ground_query(static_cast<zcls::Node*>(world_hooks().world), probe, 1, 300.0f, &g);
    zmod::zones().mode = mode;
    std::printf("  ground %.0f %.0f (%d):", x, z, g.count);
    for (int k = 0; k < g.count && k < 6; ++k) {
      const zmod::Material* mt = g.hit[k].material;
      std::printf(" [%.1f soil %d tex %d %s]", g.hit[k].height, mt ? static_cast<int>(mt->soil) : -1, mt ? static_cast<int>(mt->texture_index) : -1, g.hit[k].node ? g.hit[k].node->name() : "-");
    }
    std::printf("\n");
  }
}

void pm_kill_hurt(void* mech) {
  auto* m = static_cast<Mech*>(mech);
  m->legs[0]->set_internal(0.4f, 0);
}
float pm_kill_leg_armor(void* mech) { return static_cast<Mech*>(mech)->legs[0]->internal; }

void pm_kill_loadout(void* player) {
  auto* me = static_cast<Mech*>(player);
  auto* wm = static_cast<WeaponManager*>(me->ammo);
  std::printf("  loadout %s fcs %d:", static_cast<zcls::Node*>(me->node) ? "mech" : "?", me->equipment[3]);
  for (uint32_t i = 0; i < wm->count; ++i)
    if (wm->weapons[i] && wm->weapons[i]->def)
      std::printf(" [%u %s part %d in %d]", i, wm->weapons[i]->def->desc, wm->weapons[i]->firepoint ? wm->weapons[i]->firepoint->part : -1, wm->weapons[i]->firepoint ? (weapon_calls().equipment_in_part(me, 3, wm->weapons[i]->firepoint->part) ? 1 : 0) : -1);
  std::printf("\n  equipment:");
  for (ConfigItem* it : me->config->items)
    if (it && it->kind() == 3) std::printf(" [id %d part %d count %d]", it->id(), it->location(-1), it->count);
  std::printf("\n");
}

// PM_FALL: a frame of the fall: height, fall speed, state, the water the skis see, the depth rule's number, structure.
void pm_kill_walk(void* mech, bool jets) {
  auto* m = static_cast<Mech*>(mech);
  m->controls->set_throttle(1.0f);
  if (jets) m->controls->set_jump(1.0f);
}

int pm_kill_falling_buffer(void* mech) {
  const auto* v = static_cast<const zsnd::VoiceBase*>(static_cast<Mech*>(mech)->sounds[18]);
  return v && v->kind == 0 ? static_cast<const zsnd::Voice*>(v)->buffer : -1;
}

void pm_kill_fall_line(void* mech, int t) {
  auto* m = static_cast<Mech*>(mech);
  float at[3];
  mech_world_position(mech, at);
  std::printf("  fall %4d: y %.2f vy %.2f state %d air %d surface %.1f %.1f depth %.1f torso %.3f legs %.3f %.3f\n", t, at[1], m->vec_3c.y, m->anim_state, m->airborne() ? 1 : 0,
              m->skis->surface[1], m->skis->surface[4], (m->skis->surface[4] + m->skis->surface[1]) * 0.5f - at[1], m->torso->internal, m->legs[0]->internal, m->legs[1]->internal);
}

// PM_M2_WATCH: what a mech on the stand-in is doing, for the mission 2 run (docs/pm-tests.md).
void pm_watch(void* mech, const char* name, const float player[3]) {
  auto* m = static_cast<Mech*>(mech);
  float p[3];
  mech_world_position(mech, p);
  const float dx = p[0] - player[0], dz = p[2] - player[2];
  auto* ac = dynamic_cast<AiControls*>(m->controls);
  ai::Brain* b = ac ? ac->brain.get() : nullptr;
  std::printf("    %-13s %7.0f %6.0f %8.0f d %6.0f anim %2d xbc %02x jets %d", name, p[0], p[1], p[2], std::sqrt(dx * dx + dz * dz), m->anim_state, m->xbc, m->equipment[11]);
  if (b) {
    std::printf(" strat %d orders %zu", b->params.get<int32_t>(0x40), b->orders.size());
    if (!b->orders.empty()) {
      ai::Order& o = b->current();
      std::printf(" state %s", o.state ? typeid(*o.state).name() : "-");
    }
  }
  std::printf(" target %d\n", m->target ? 1 : 0);
}
int pm_watch_follow(void* mech) {
  auto* m = static_cast<Mech*>(mech);
  if (const char* e = std::getenv("PM_M2_JETS")) m->equipment[11] = std::atoi(e);
  auto* ac = dynamic_cast<AiControls*>(m->controls);
  if (!ac || !ac->brain || !m->player_contact) return -1;
  return ac->brain->lance_order(0, m->player_contact);
}

// Every weapon through its trigger, the route a pull of the trigger takes (pm_kill_fire's aim_and_fire makes a
// beam but never starts it, so its lasers never fire); the reload and the beams' charge skipped.
int pm_trigger_all(void* player, const float at[3]) {
  auto* me = static_cast<Mech*>(player);
  auto* wm = static_cast<WeaponManager*>(me->ammo);
  me->heat = 0.0f;
  wm->aim = Vec3f{at[0], at[1], at[2]};
  int fired = 0;
  const bool beams = std::getenv("PM_BEAMS_ONLY") != nullptr;  // lasers alone, as Owen's loadout (pm-tests item 14)
  for (uint32_t i = 0; i < wm->count; ++i) {
    Weapon* w = wm->weapons[i];
    if (!w || (beams && !dynamic_cast<BeamWeapon*>(w))) continue;
    w->reload = 0.0f;
    if (auto* b = dynamic_cast<BeamWeapon*>(w); b && !b->firing) b->bin->amount = b->bin->capacity();
    if (w->can_fire()) fired += w->trigger() ? 1 : 0;
  }
  return fired;
}

// The player's Invincible flag and its parts' armour and internal summed (pm-tests item 12).
float pm_mech_toughness(void* mech, bool* invincible) {
  auto* m = static_cast<Mech*>(mech);
  *invincible = m->invincible;
  float sum = 0;
  for (MechPart* p : m->parts) sum += p->armor + p->internal;
  return sum;
}
// The player's held inputs for the resume test: set (turn, pitch, twist, jets, look and the throttle) or read back
// (turn, twist, pitch, jj_forward, look_x, throttle, throttle_target).
void pm_controls_hold(void* mech, float v) {
  auto* c = static_cast<PlayerControls*>(static_cast<Mech*>(mech)->controls);
  c->turn = c->turn_target = c->pitch = c->twist = c->jj_forward = c->jj_strafe = v;
  c->look_x = c->look_y = v;
  c->throttle = c->throttle_target = v;
}
void pm_controls_read(void* mech, float out[7]) {
  auto* c = static_cast<PlayerControls*>(static_cast<Mech*>(mech)->controls);
  const float v[7] = {c->turn, c->twist, c->pitch, c->jj_forward, c->look_x, c->throttle, c->throttle_target};
  for (int i = 0; i < 7; ++i) out[i] = v[i];
}

// Pirate's Moon 0x00415190: the player's MASC on, the mech made again as its own type; whether MASC is on after.
// Through the game's own caller (0x0047a6d0), which takes the mech off the view's target while it is made
// again and resets the HUD after: a bare rebuild of the player leaves the HUD on the freed weapon list.
bool pm_rebuild_keeps_masc(void* player) {
  auto* m = static_cast<Mech*>(player);
  m->masc(true);
  const bool before = m->masc_time != 0.0f;
  program_pilot_set_mech(m->pilot ? m->pilot->roster : -1);
  return before && m->masc_time != 0.0f;
}

int pm_controls_view(void* mech) {
  const Mech* m = static_cast<Mech*>(mech);
  return m && m->controls ? m->controls->view : -1;
}
// The turret whose node this is, destroyed as its health running out does (0x004a7620); false: none.
bool pm_kill_turret(const char* name) {
  zcls::Node* n = zcls::list_find_by_name(6, name);
  tur::Turret* t = n ? program_turret(node_entity(n)) : nullptr;
  if (!t) return false;
  t->deactivate(true);
  return true;
}
// pm-tests 37's survey: every turret's root (the topmost node whose entity is that turret).
void pm_turret_roots(std::vector<zcls::Node*>* out) {
  for (zcls::ListItem* it = zcls::list_head(6); it; it = it->next) {
    zcls::Node* n = it->node;
    tur::Turret* t = n ? program_turret(node_entity(n)) : nullptr;
    if (!t) continue;
    zcls::Node* p = n->parent_count() > 0 && n->parents ? n->parents[0] : nullptr;
    if (p && program_turret(node_entity(p)) == t) continue;
    out->push_back(n);
  }
}
void pm_controls_set_view(void* mech, int view) {
  Mech* m = static_cast<Mech*>(mech);
  if (m && m->controls) m->controls->view = view;
}
bool pm_cheat_fast() { return cheats().fast; }
// The reticle (look_position x, y) and the controls' twist and pitch.
void pm_controls_aim(void* mech, float out[4]) {
  out[0] = out[1] = out[2] = out[3] = 0.0f;
  Mech* m = static_cast<Mech*>(mech);
  if (!m || !m->controls) return;
  if (const float* l = m->controls->look_position()) out[0] = l[0], out[1] = l[1];
  out[2] = m->controls->twist, out[3] = m->controls->pitch;
}
bool pm_cheat(void* player, const char* text) { return cheat_enter(*static_cast<Mech*>(player), text); }

// pm-tests item 15: TAG added as a mech's build adds it (configure_weapons: by the first firepoint's name under the
// mech's node), aimed at `at`: its muzzle's distance from the mech's place, -1 when it can't be added or aimed, and whether
// it found its part and firepoint. This harness's mechs have no firepoint nodes (the program's do), so one is made
// under the torso, marked as the build marks a part's subtree (+0x28, the part at +0x40).
float pm_tag_muzzle(void* player, const float at[3], bool* mounted) {
  auto* me = static_cast<Mech*>(player);
  auto* wm = static_cast<WeaponManager*>(me->ammo);
  *mounted = false;
  const std::vector<Firepoint*>* fps = weapon_calls().firepoints ? weapon_calls().firepoints(me) : nullptr;
  if (!fps || fps->empty() || !me->torso) return -1.0f;
  auto* torso = static_cast<zcls::Node*>(me->torso->node);
  if (!weapon_calls().node_by_name || !weapon_calls().node_by_name(me->node, (*fps)[0]->name)) {
    zcls::Node* fp = zcls::object3d_create();
    zcls::node_set_name(fp, (*fps)[0]->name);
    fp->put<uint32_t>(0x28, fp->get<uint32_t>(0x28) | 1u);
    fp->owner = me->torso;
    zcls::node_add_child(torso, fp);
  }
  Weapon* t = wm->add(kTag, me->node, (*fps)[0]->name, 4);
  if (!t) return -1.0f;
  *mounted = t->part && t->firepoint;
  Vec3f to{at[0], at[1], at[2]}, mz{}, dir{};
  if (!t->aim(&to, &mz, &dir, nullptr)) return -1.0f;
  float p[3] = {};
  mech_world_position(me, p);  // (the torso's node_world_point can fail and leave it unset)
  const float d[3] = {mz.x - p[0], mz.y - p[1], mz.z - p[2]};
  return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

// pm-tests item 20: the player's headlight (CMD_HEADLIGHT_TOGGLE's turn) on or off; whether there is one, and its
// light's world place.
bool pm_headlight(void* player, bool on, float at[3]) {
  auto* me = static_cast<Mech*>(player);
  Headlight* h = me && me->torso ? me->torso->headlight : nullptr;
  if (!h) return false;
  h->turn(on, false);
  at[0] = at[1] = at[2] = 0.0f;
  if (h->light) zcls::node_world_point(h->light, at);
  return true;
}

// The torso's level-of-detail switch (Pirate's Moon 0x00468d50): the player's torso switched to `lod`; 1 when the
// headlight's parent is the torso's node now shown and the headlight's node is that node's child, 0 when not, -1 no
// headlight (or the switch had nothing to change to).
int pm_headlight_lod(void* player, int lod) {
  auto* me = static_cast<Mech*>(player);
  Torso* t = me ? me->torso : nullptr;
  Headlight* h = t ? t->headlight : nullptr;
  if (!h) return -1;
  const Handle was = t->node;
  t->switch_lod(lod);
  if (t->node == was) return -1;
  auto* n = static_cast<zcls::Node*>(t->node);
  bool child = false;
  for (int32_t i = 0; i < n->child_count(); ++i) child = child || n->children[i] == h->node;
  return h->parent == n && child ? 1 : 0;
}

// pm-tests item 23: a mech's knock (+0x53c), its threshold (the gait's, 0x00418850) and its state.
float pm_knock(void* mech, float* threshold, int* state) {
  auto* m = static_cast<Mech*>(mech);
  float t = m->tonnage();
  if (t < 10.0f) t = 10.0f;
  *threshold = (t - 10.0f) * 0.0111111114f * m->knockdown[4] + m->knockdown[3];
  *state = m->anim_state;
  return m->knock;
}

// pm-tests item 25: a player command (the keys' Mech::command, e.g. 0x48 E, the next enemy).
void pm_command(void* player, int32_t cmd) { static_cast<Mech*>(player)->command(cmd); }

// The water survey (PM_WATER; docs/pm-tests.md): the mission's world sampled with the ground query every `step`
// metres over the box round its points of interest (the player's start, the nav points, the AI mechs), each
// sample's water depth the water surface less the highest ground under it. One line: the deepest water and where,
// the samples past a tenth of `low` and of `high` (the shortest and the tallest mech's heights), and how near the
// nearest such water is to each point of interest. MW3's worlds would take the same function: it asks nothing of
// Pirate's Moon but the test's mission start.
#include "mech3/app_vehicle.h"
#include "mech3/mech_waynet.h"
#include <vector>
float pm_mech_height(void* mech) { return static_cast<Mech*>(mech)->height; }

void pm_water_survey(void* player, const char* label, float low, float high, float step) {
  struct Poi { std::string name; float x, z; float near_low = 1e9f, near_high = 1e9f; };
  std::vector<Poi> pois;
  float p[3];
  mech_world_position(player, p);
  pois.push_back({"start", p[0], p[2]});
  if (auto* w = static_cast<Waynet*>(program_mission_waynet()))  // the nav points: the mission waynet's labelled ones
    for (int32_t i = 0; i < w->count(); ++i)
      if (const auto* np = dynamic_cast<const WaynetPoint*>(w->elem(i)); np && np->label_or_null())
        pois.push_back({std::string("nav ") + np->label_or_null(), np->pos[0], np->pos[2]});
  int32_t made = 0;
  const AiMechMade* ai = mission_ai_mechs(&made);
  for (int32_t i = 0; i < made; ++i) {
    mech_world_position(ai[i].mech, p);
    pois.push_back({std::string("mech ") + (ai[i].name ? ai[i].name : "?"), p[0], p[2]});
  }
  float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
  for (const Poi& q : pois) x0 = std::min(x0, q.x), x1 = std::max(x1, q.x), z0 = std::min(z0, q.z), z1 = std::max(z1, q.z);
  const float margin = 1000.0f;
  x0 -= margin, x1 += margin, z0 -= margin, z1 += margin;
  static zcls::GroundHits g;
  const int32_t mode = zmod::zones().mode;
  zmod::zones().mode = 0;  // every zone's ground
  int samples = 0, wet = 0, past_low = 0, past_high = 0, no_floor = 0;
  float deepest = 0, at[3] = {};
  for (float x = x0; x <= x1; x += step)
    for (float z = z0; z <= z1; z += step) {
      float probe[3] = {x, 2000.0f, z};
      zcls::ground_query(static_cast<zcls::Node*>(world_hooks().world), probe, 1, 4000.0f, &g);
      ++samples;
      float surface = -1e9f;
      for (int k = 0; k < g.count; ++k)
        if (g.hit[k].material && g.hit[k].material->soil == 1 && g.hit[k].height > surface) surface = g.hit[k].height;  // kWater
      if (surface == -1e9f) continue;
      float floor = -1e9f;
      for (int k = 0; k < g.count; ++k)
        if (!(g.hit[k].material && g.hit[k].material->soil == 1) && g.hit[k].height <= surface && g.hit[k].height > floor) floor = g.hit[k].height;
      ++wet;
      if (floor == -1e9f) {  // water over nothing: off the land's edge
        ++no_floor;
        continue;
      }
      const float d = surface - floor;
      if (d > deepest) deepest = d, at[0] = x, at[1] = surface, at[2] = z;
      const bool lo = d > 0.1f * low, hi = d > 0.1f * high;
      past_low += lo, past_high += hi;
      if (!lo) continue;
      for (Poi& q : pois) {
        const float dist = std::sqrt((q.x - x) * (q.x - x) + (q.z - z) * (q.z - z));
        if (dist < q.near_low) q.near_low = dist;
        if (hi && dist < q.near_high) q.near_high = dist;
      }
    }
  zmod::zones().mode = mode;
  std::printf("WATER %s: %d samples every %.0f m over %.0f x %.0f m; %d over water (%d with no ground under), deepest %.1f m at %.0f %.0f %.0f; "
              "past a tenth of the shortest mech (%.1f m of %.1f): %d samples, of the tallest (%.1f m of %.1f): %d\n",
              label, samples, step, x1 - x0, z1 - z0, wet, no_floor, deepest, at[0], at[1], at[2], 0.1f * low, low, past_low, 0.1f * high, high, past_high);
  for (const Poi& q : pois) {
    if (q.near_low > 1e8f) continue;
    std::printf("WATER %s:   %-22s wadeable water %5.0f m away (the tallest's: %s)\n", label, q.name.c_str(), q.near_low,
                q.near_high > 1e8f ? "none" : (std::to_string(static_cast<int>(q.near_high)) + " m").c_str());
  }
}
