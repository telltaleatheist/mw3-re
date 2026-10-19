#include "gamez/zweapon/zweapon.h"

#include <cmath>

#include "crt/msvc_math.h"
#include "crt/msvc_rand.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zmodel/gmod_draw.h"

#pragma STDC FP_CONTRACT OFF

namespace zwep {

namespace {
FireState g_fire;
HitState g_hit;
HitHooks g_hh;

int32_t soil_of(Handle material) { return material && g_hh.soil ? g_hh.soil(material) : 0; }

void play_at(Handle anim, const Vec3f3& p) {
  const float a[9] = {p.x, p.y, p.z, 0, 0, 0, 0, 0, 0};
  if (g_hh.effect) g_hh.effect(anim, a);
}
}  // namespace

FireState& fire_state() { return g_fire; }
HitState& hit_state() { return g_hit; }
HitHooks& hit_hooks() { return g_hh; }

// Mech3.exe 1.2 @ 0x00583020: a beam's damage this frame. Short beams deal it
// once a shot; it falls off as the cosine over the beam's reach, down to
// DAMAGE_AT_MAX_RANGE (lasers 0.75). Under water the reach halves, the damage not.
float beam_damage(WeaponDef* def, Beam* b) {
  const bool timed = (def->flags & kTimedDamage) != 0;
  if (!timed && (b->flags & 2)) return 0.0f;
  float v;
  if (def->damage_at_max_range < 1.0f) {
    const float x = crt::x87_mul_f24(b->length, 3.141592653589793) / (b->range_scale * def->range);
    // fcos leaves an extended result: round once, after adding the 1.
    float f = static_cast<float>(std::cos(static_cast<double>(x)) + 1.0) * 0.5f;
    if (def->damage_at_max_range > f) f = def->damage_at_max_range;
    v = f * def->damage;
  } else {
    v = def->damage;
  }
  if (timed) return v * g_hit.dt * b->damage_scale;
  b->flags |= 2;
  return v * b->damage_scale;
}

// Mech3.exe 1.2 @ 0x00585cc0: one frame of flight into `move`: speed up to
// the maximum, gravity, RANDOM_DEVIATION's drift, the distance travelled,
// and what happens at RANGE or at the lock's DETONATION_DISTANCE.
void step(WeaponDef* def, Projectile* p, Vec3f3* move) {
  const float dt = g_hit.dt;
  p->state = 1;
  bool rebuild = false;
  if (!(def->flags & kMine) && p->speed < p->max_speed) {
    p->speed = def->acceleration * dt + p->speed;
    if (p->max_speed < p->speed) p->speed = p->max_speed;
    if (!(def->flags & kLockOn) || !p->target_at || (def->flags & kTetherGuided)) rebuild = true;
  }
  auto rebuild_velocity = [p] {  // 0x0051e0e0: base + direction x speed
    p->velocity = {p->speed * p->direction.x + p->base_velocity.x, p->direction.y * p->speed + p->base_velocity.y,
                   p->direction.z * p->speed + p->base_velocity.z};
  };
  if (def->gravity != 0.0f) {
    p->velocity.y = p->velocity.y - dt * def->gravity;
    // FIX (bug): here the original passes ECX = 1 as the base velocity (a
    // crash); no shipped weapon has gravity. The base is used (docs/fixes.md).
    if (rebuild) rebuild_velocity();
  } else if (rebuild) {
    rebuild_velocity();
  }
  *move = {p->velocity.x * dt, p->velocity.y * dt, p->velocity.z * dt};
  if (def->flags & kRandomDeviation) {
    bool drift = true;
    if ((def->flags & kLockOn) && p->target_at) {
      const zmth::Vec3 a{p->position.x, p->position.y, p->position.z};
      const zmth::Vec3 b{p->target_at->x, p->target_at->y, p->target_at->z};
      const float d2 = zmth::distance_sq(a, b);
      if (d2 < def->detonation_distance_sq * 4.0f || d2 < 400.0f) drift = false;  // steady near the target
    }
    if (drift) {
      const float r = dt * 5.0f;
      auto rnd = [r] {
        float v = static_cast<float>(crt::msvc_rand()) * 0x1.0002p-15f;  // 1 / 32767
        return (v + v - 1.0f) * r;
      };
      Vec3f3 step_dev;
      step_dev.x = rnd();
      step_dev.y = rnd();
      step_dev.z = rnd();
      const Vec3f3& dv = p->deviation;
      if ((dv.x * step_dev.x + dv.y * step_dev.y) + dv.z * step_dev.z < 0.0f) step_dev = {-step_dev.x, -step_dev.y, -step_dev.z};
      p->deviation = {dv.x + step_dev.x, dv.y + step_dev.y, dv.z + step_dev.z};
      // The drift so far is held within +-1 (y +-0.75) by pushing back.
      const Vec3f3& lim = p->drift;
      if (lim.x > 1.0f) p->deviation.x = p->deviation.x - dt * 10.0f;
      else if (lim.x < -1.0f) p->deviation.x = p->deviation.x - dt * -10.0f;
      if (lim.y > 0.75f) p->deviation.y = p->deviation.y - dt * 10.0f;
      else if (lim.y < -0.75f) p->deviation.y = p->deviation.y - dt * -10.0f;
      if (lim.z > 1.0f) p->deviation.z = p->deviation.z - dt * 10.0f;
      else if (lim.z < -1.0f) p->deviation.z = p->deviation.z - dt * -10.0f;
      const Vec3f3 d{p->deviation.x * dt, p->deviation.y * dt, p->deviation.z * dt};
      p->drift = {p->drift.x + d.x, p->drift.y + d.y, p->drift.z + d.z};
      *move = {move->x + d.x, move->y + d.y, move->z + d.z};
    }
  }
  if ((def->flags & kTetherGuided) && def->terrain_height + move->y > g_hit.tether_ceiling)
    move->y = g_hit.tether_ceiling - def->terrain_height;
  const float len = std::sqrt((move->x * move->x + move->y * move->y) + move->z * move->z);
  p->travelled = len + p->travelled;
  if (def->flags & kMine) p->travelled = p->travelled * 0.5f;
  bool burst = false, gone = false;
  if (!(p->travelled < def->range)) {
    // At RANGE: a homing shot without EXPIRES bursts, so does DETONATE_AT_RANGE;
    // the rest vanish (every shipped missile has EXPIRES).
    if (((def->flags & kLockOn) && !(def->flags & kExpires)) || (def->flags & kDetonateAtRange)) burst = true;
    gone = true;
  } else if ((def->flags & kLockOn) && p->target_at && def->detonation_distance_sq != 0.0f) {
    const zmth::Vec3 a{p->position.x, p->position.y, p->position.z};
    const zmth::Vec3 b{p->target_at->x, p->target_at->y, p->target_at->z};
    if (!(zmth::distance_sq(a, b) > def->detonation_distance_sq)) burst = gone = true;
  }
  if (burst) air_burst(def, p);
  if (gone) p->state = 0;
}

// Mech3.exe 1.2 @ 0x00583710
void note_kill(Handle killed, HitRecord* at) {
  if (at && at->node) g_hit.killed_at = at;
  g_hit.killed = killed;
}

// Mech3.exe 1.2 @ 0x00583480: the damage to what a record hit, through the
// node's handler (for a mech: Mech::TakeDamage), and the impact animation.
uint32_t apply_damage(WeaponDef* def, Handle owner, const Vec3f3* from, HitRecord* hit, float damage, Handle anim) {
  g_hit.killed = nullptr;
  g_hit.killed_at = nullptr;
  if (!(def->flags & kTargetHeat) && g_hh.surface_mark) g_hh.surface_mark(hit);
  const uintptr_t h = g_hh.node_handler ? g_hh.node_handler(hit->node) : 0;
  if (h == 0) return 0;
  uint32_t r = 0;
  if (h == 1) {
    // A projectile with FLYOUT_HEALTH: any hit wears it down.
    Projectile* p = g_hh.node_projectile ? g_hh.node_projectile(hit->node) : nullptr;
    float v = p->health - damage;
    if (!(v > 0.0f)) v = 0.0f;
    p->health = v;
  } else if (g_hh.handler_live && g_hh.handler_live(h)) {
    const int32_t soil = soil_of(hit->material);
    if (g_hit.record_shot == 1) {
      g_hit.shot_from = *from;
      g_hit.shot_to = hit->point;
    }
    g_hit.health_after = 1.0f;
    g_hit.attacker = owner;
    if (g_hit.pre_hit) g_hit.pre_hit(reinterpret_cast<Handle>(h), hit->node, damage, def);
    r = g_hh.call_handler ? g_hh.call_handler(h, def, hit, damage) : 0;
    if (g_hit.killed) {
      if (def->kill_animation) play_at(def->kill_animation, hit->point);
    } else if (anim) {
      play_at(anim, hit->point);
    } else if (def->damage_anim_count) {
      for (int32_t i = 0; i < def->damage_anim_count; ++i) {
        if (!(g_hit.health_after > def->damage_anims[i].health)) {
          play_at(def->damage_anims[i].anim, hit->point);
          break;
        }
      }
    } else if ((def->flags & kBeam) && g_hit.beam_ground_fx && def->impacts[soil].animation) {
      play_at(def->impacts[soil].animation, hit->point);
    }
  }
  if (g_hit.counted == owner) ++g_hit.hits;
  return r;
}

// Mech3.exe 1.2 @ 0x005826b0: one shot's impact: the weapon's callback, the
// damage, craters, the sound, animations and effect. The callback's bits skip
// the sound (1), craters and effect (2), the animation (4).
uint32_t impact(WeaponDef* def, HitRecord* rec, Projectile* p, Handle anim) {
  const int32_t soil = soil_of(rec->material);
  int32_t skip = 0;
  uint32_t flags = 0;
  if (def->callback) flags = def->callback(def, rec, p);
  const uint32_t r = apply_damage(def, p->owner, &p->position, rec, def->damage * p->damage_scale, anim);
  if ((def->flags & kCrater) && !(flags & 2)) {
    if (!(def->flags & kAnimationAlways)) skip = 1;
    skip &= g_hh.crater ? g_hh.crater(def, rec, soil, p) : 0;
  } else if ((def->flags & kQuicksand) && !(flags & 2)) {
    if (HitRecord* k = g_hit.killed_at) {
      if (g_hh.quicksand) g_hh.quicksand(def, k, soil_of(k->material), p);
    } else if (g_hh.quicksand) {
      g_hh.quicksand(def, rec, soil, p);
    }
  }
  // A kill makes its own crater where it fell.
  if (g_hit.killed && (def->flags & kCrater) && g_hit.killed_at && !(flags & 2) && g_hh.crater)
    g_hh.crater(def, g_hit.killed_at, soil_of(g_hit.killed_at->material), p);
  if ((def->flags & kQuicksand) && g_hit.killed_at && !(flags & 2)) {
    if (g_hh.quicksand) g_hh.quicksand(def, g_hit.killed_at, soil_of(g_hit.killed_at->material), p);
    return r;
  }
  if (!(flags & 1) && g_hh.sound) g_hh.sound(def, rec, soil, 1.0f);
  if (!skip) {
    if (!(flags & 4) && r == 0) {
      if (Handle a = def->impacts[soil].animation) play_at(anim ? anim : a, rec->point);
    }
    if (Handle s = def->impacts[soil].surface_animation) {
      float n[3] = {rec->normal.x, rec->normal.y, rec->normal.z};
      zmod::normalize(n);
      rec->normal = {n[0], n[1], n[2]};
      float ang[3] = {0, 0, 0};
      if (g_hh.surface_angles) g_hh.surface_angles(&rec->normal, ang);
      const float a[9] = {rec->point.x, rec->point.y, rec->point.z, ang[0], ang[1], ang[2], 0, 0, 0};
      if (g_hh.effect) g_hh.effect(s, a);
    }
  }
  if (const int32_t e = def->impacts[soil].effect; e && !(flags & 2) && g_hh.impact_effect)
    g_hh.impact_effect(e, &rec->point);
  return r;
}

// Mech3.exe 1.2 @ 0x00582920: what lies within IMPACT_PROXIMITY (scaled with
// the shot's damage) of the shot. The shot's own node is kept out of the query.
int32_t splash_query(WeaponDef* def, Projectile* p, int32_t flag, HitList* out) {
  Handle node = p->node;
  bool toggled = false;
  if (node && g_hh.node_intersect && g_hh.node_intersect(node)) {
    toggled = true;
    if (g_hh.set_intersect) g_hh.set_intersect(node, 0);
  }
  int32_t n = g_hh.query ? g_hh.query(node, &p->position, def->impact_proximity * p->damage_scale, out) : 1;
  if (toggled && g_hh.set_intersect) g_hh.set_intersect(node, 1);
  // Only the shooter in range: no splash (both callers pass flag 1, so dead).
  if (flag == 0 && out->count == 1 && out->rec[0].node == p->owner) n = 1;
  return n == 0;
}

// Mech3.exe 1.2 @ 0x005829b0: splash damage, falling off with the square of
// the distance (MINE weapons at full damage), to all in range but `exclude`
// (the handler of what the shot hit). The original queues a hit instead
// when 31 are already queued and the weapon does not set fires; nothing
// ever queues, so the queue (0x009a0ef4) and its drain in the tick are dead.
int32_t splash(WeaponDef* def, Projectile* p, HitList* list, uintptr_t exclude) {
  int32_t any = 0;
  const int n = list->count;
  for (int i = 0; i < n; ++i) {
    HitRecord* rec = &list->rec[i];
    const uintptr_t h = g_hh.node_handler ? g_hh.node_handler(rec->node) : 0;
    if (h == exclude) continue;
    float dmg;
    if (def->flags & kMine) dmg = def->damage;
    else dmg = (1.0f - rec->distance_sq / def->impact_proximity_sq) * def->damage;
    dmg = p->damage_scale * dmg;
    apply_damage(def, p->owner, &p->position, rec, dmg, nullptr);
    any = 1;
  }
  return any;
}

// Mech3.exe 1.2 @ 0x00586350: a projectile hits. Inside RANGE_MINIMUM a shot
// with no second value is a dud; otherwise its damage (and splash) is scaled
// by it, LRMs (90 0.25) to a quarter, with the min-range animation.
void hit(WeaponDef* def, Projectile* p, HitRecord* rec) {
  if (g_hit.record_shot == 1) {
    g_hit.shot_from = p->position;
    g_hit.shot_to = rec->point;
  }
  const bool min_range = (def->flags & kRangeMinimum) != 0;
  if (min_range && def->range_min[1] == 0.0f && p->travelled < def->range_min[0]) {
    p->health = 0.0f;
    return;
  }
  Handle anim = nullptr;
  if (min_range && p->travelled < def->range_min[0]) {
    const int32_t soil = soil_of(rec->material);
    if (Handle a = def->impacts[soil].animation_min_range) anim = a;
    else if (def->destroy_animation) anim = def->destroy_animation;
    p->damage_scale = def->range_min[1] * p->damage_scale;
  }
  p->state = 0;
  g_hit.record_shot = 0;
  p->position = rec->point;
  impact(def, rec, p, anim);
  if (def->impact_proximity > 0.0f) {
    HitList list;
    if (splash_query(def, p, 1, &list)) {
      const uintptr_t exclude = g_hh.node_handler ? g_hh.node_handler(rec->node) : 0;
      splash(def, p, &list, exclude);
    }
  }
  g_hit.record_shot = 1;
}

// Mech3.exe 1.2 @ 0x00582320: a shot bursts in the air (at range, or shot
// down): an impact on its own node at its position, then splash with no one
// spared. FIX (bug): the original's record has no surface normal (stack
// garbage) for the surface animation; here it is zero (docs/fixes.md).
void air_burst(WeaponDef* def, Projectile* p) {
  HitRecord rec;
  rec.point = p->position;
  rec.node = p->node;
  impact(def, &rec, p, nullptr);
  if (g_hit.splash_enabled && def->impact_proximity > 0.0f) {
    HitList list;
    if (splash_query(def, p, 1, &list)) splash(def, p, &list, 0);
  }
}

// Mech3.exe 1.2 @ 0x00581f60
void set_lock(int32_t* flag, Handle* target) {
  g_fire.lock_flag = flag;
  g_fire.lock_target = target;
}

// Mech3.exe 1.2 @ 0x00581f70: which FIRE sound the next shot plays, -1 none.
bool set_fire_sound(WeaponDef* def, int32_t choice) {
  if (!def || choice >= def->fire.sound_count) return false;
  def->fire.sound_choice = choice;
  return true;
}

// Mech3.exe 1.2 @ 0x00581f90
bool set_fire_animation(WeaponDef* def, int32_t choice) {
  if (!def || choice >= 2) return false;
  def->fire.animation_choice = choice;
  return true;
}

// Mech3.exe 1.2 @ 0x00581fb0
void attach_fire(WeaponDef* def, Handle node) {
  def->fire.attach_node = node;
  def->fire.attach_offset = nullptr;
}

// Mech3.exe 1.2 @ 0x00581fd0
void attach_fire(WeaponDef* def, Handle node, const void* offset) {
  def->fire.attach_node = node;
  def->fire.attach_offset = offset;
}

}  // namespace zwep
