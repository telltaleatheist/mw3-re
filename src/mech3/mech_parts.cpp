#include "mech3/mech_parts.h"

#include <vector>

#include "crt/msvc_rand.h"
#include "mech3/mech.h"
#include "mech3/mech_config.h"
#include "mech3/weapon.h"

namespace mech3 {

namespace {
constexpr float kDamageScale = 0.0062f;   // 0x005e1f68
constexpr float kCritArmor = 0.15f;       // 0x005c2774
constexpr float kRandUnit = 3.051851e-05f;  // 0x005c2778
constexpr int kMultiplayer = 3;           // docs/gametypes.md
constexpr int kElemental = 7;             // roster index
constexpr int kCase = 5, kSink = 9, kDoubleSink = 10;  // equipment kinds
constexpr int kMechSoundAmmo = 15, kMechSoundCritical = 3;
constexpr int kNetSection = 0x15;         // docs/network-messages.md

DamageCalls g_calls;

float rand_unit() { return static_cast<float>(crt::msvc_rand()) * kRandUnit; }
int game_type() { return heat_calls().game_type ? heat_calls().game_type() : 0; }
void* screen(Mech& m) { return heat_calls().screen ? heat_calls().screen(m) : nullptr; }
void* player_screen(Mech& m) { return &m == world().player ? screen(m) : nullptr; }
void mech_sound(int i) {
  if (heat_calls().play_mech_sound) heat_calls().play_mech_sound(i);
}
void net_event(Mech& m, int kind, intptr_t arg) {
  if (g_calls.net_event) g_calls.net_event(m, kind, arg);
}
Handle spawn(Handle effect, Handle node, const float (&args)[9]) {
  return g_calls.spawn_effect ? g_calls.spawn_effect(effect, node, args) : nullptr;
}
void on_end(Handle effect, MechPart& p) {
  if (g_calls.on_effect_end) g_calls.on_effect_end(effect, p);
}

// x87 compares: C0 or C3 is also set for an unordered operand.
bool zeroish(float f) { return !(f < 0.0f) && !(f > 0.0f); }

const float* matrix(MechPart& p) {
  static const float identity[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
  return g_calls.node_matrix ? g_calls.node_matrix(p) : identity;
}

// The node's rotation applied backwards (a row vector times its rows).
Vec3 rotate_back(const Vec3& v, const float* m) {
  return {(v.x * m[0] + v.y * m[1]) + v.z * m[2], (v.x * m[3] + v.y * m[4]) + v.z * m[5],
          (v.x * m[6] + v.y * m[7]) + v.z * m[8]};
}

// Mech3.exe 1.2 @ 0x00460780. Does edge p-q cross the line from b to a? The
// first test is one-sided: a must lie on the edge's inner side and b on the
// outer, so the edge only counts for a shot entering through it.
bool crosses(const Vec3& b, const Vec3& a, const Vec3& p, const Vec3& q) {
  const float nz = q.z - p.z, nx = p.x - q.x;
  const Vec3 u{a.x - q.x, a.y - q.y, a.z - q.z}, v{b.x - q.x, b.y - q.y, b.z - q.z};
  const float s1 = u.x * nz + u.z * nx, s2 = v.x * nz + v.z * nx;
  if (!(s1 > 0.0f) && !(s2 > 0.0f)) return false;
  if (!(s1 >= 0.0f)) return false;
  if (s2 >= 0.0f) return false;
  const float mz = a.z - b.z, mx = b.x - a.x;
  const Vec3 u2{q.x - a.x, q.y - a.y, q.z - a.z}, v2{p.x - a.x, p.y - a.y, p.z - a.z};
  const float t1 = u2.x * mz + u2.z * mx, t2 = v2.x * mz + v2.z * mx;
  if (!(t1 > 0.0f) && !(t2 > 0.0f)) return false;
  return !(t1 >= 0.0f) || !(t2 >= 0.0f);
}

std::vector<EquipmentItem*> equipment_in(Mech& m, int part) {
  std::vector<EquipmentItem*> out;
  for (ConfigItem* it : m.config->items)
    if (it && it->kind() == 3 && it->part == part) out.push_back(static_cast<EquipmentItem*>(it));
  return out;
}
}  // namespace

DamageCalls& damage_calls() { return g_calls; }

// Mech3.exe 1.2 @ 0x00460970. Docs: damage.md, the hit path.
int MechPart::take_hit(WeaponDef* def, Collision*, float damage, Vec3* impulse) {
  World& w = world();
  Mech& m = *mech;
  float d = damage * kDamageScale;
  if (game_type() != kMultiplayer) {
    // Difficulty (0x0040cc20) scales what the player and its side take.
    if (&m == w.mission_player || (w.mission_player->allies & m.team) != 0) d = w.difficulty * d;
  }
  const Vec3* shot = w.shot;  // 0x00581e30
  if (impulse) {
    WeaponType* t = def ? def->type : nullptr;
    apply_impact(impulse, t->impact_force);
  }
  int section = 0;
  if (id == kTorso) section = m.torso->hit_section(shot);

  bool crit = false;
  int kind = 0;
  if (!m.invincible && armor <= kCritArmor && def && !(def->flags & 2)) {
    crit = true;
    const double r = rand_unit();
    if (!(r < 0.02)) crit = false;          // 0x005c2780
    else if (r < 0.008) kind = kCritSink;   // 0x005c2788 ...
    else if (r < 0.013) kind = kCritAmmo;
    else if (r < 0.016) kind = kCritEquipment;
    else if (r < 0.019) kind = kCritWeapon;
    else kind = kCritSpecial;
  }
  if (crit) {
    // A critical replaces the hit's damage, whatever it does.
    critical(kind, section);
    return 0;
  }
  apply_damage(d, section);
  if (&m == w.mission_player && id == kHead) {
    Pilot& p = *m.pilot;
    if (p.x228 && p.x228[1] && p.x200 && g_calls.head_hit_hud) g_calls.head_hit_hud();
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00461300. Internal damage uses the armor before the hit
// and is floored at the armor left (damage.md).
void MechPart::apply_damage(float d, int s) {
  if (destroyed) return;
  const float h = get_internal(s);
  const float a = get_armor(s);
  const float inner = get_internal_scale(s) * (1.2f - a) * d;  // 0x005e4efc
  const float outer = get_armor_scale(s) * 0.62f * d;          // 0x005e4ef8
  float a2 = a - outer;
  float h2 = h - inner;
  if (!mech->invincible) {
    if (a2 > 1.0f) a2 = 1.0f;
    else if (!(a2 >= 0.0f)) a2 = 0.0f;
    if (id == kTorso) {
      const int other = s < 3 ? s + 3 : s - 3;
      const float floor = !(get_armor(other) >= a2) ? get_armor(other) : a2;
      if (h2 > 1.0f) h2 = 1.0f;
      else if (!(h2 >= floor)) h2 = floor;
    } else {
      if (h2 > 1.0f) h2 = 1.0f;
      else if (!(h2 >= a2)) h2 = a2;
    }
  } else {
    if (h2 > 1.0f) h2 = 1.0f;
    else if (!(h2 >= 0.01f)) h2 = 0.01f;  // 0x005e4ee4
    if (a2 > 1.0f) a2 = 1.0f;
    else if (!(a2 >= 0.0f)) a2 = 0.0f;
  }
  set_armor(a2, s);
  set_internal(h2, s);
}

// Mech3.exe 1.2 @ 0x00461510. Only section 0 counts for a plain part.
void MechPart::set_internal(float v, int s) {
  if (s != 0) return;
  if (destroyed) v = 0.0f;
  if (v < internal || v > internal) {
    internal = v;
    net_event(*mech, kNetSection, id);
    if (!(internal > 0.0)) {
      internal = 0.0f;
      // Which of the owner's two destruction effects: 60 / 40.
      const float r = (g_calls.force_variant0 && g_calls.force_variant0()) ? 1.0f : rand_unit();
      destroy(r > 0.4f ? 0 : 1, 1);
    }
  }
  mech->damaged = true;
}

// Mech3.exe 1.2 @ 0x00461640
void MechPart::set_armor(float v, int s) {
  const float val = destroyed ? 0.0f : v;
  if (s == 0) armor = val;
}

// Mech3.exe 1.2 @ 0x00460190, with no node: the part leaves the scene.
void MechPart::detach() {
  if (x68 && g_calls.node_unlink) g_calls.node_unlink(x68, node);
  node = nullptr;
  id = -1;
}

Handle MechPart::fly_off(Vec3* from, Vec3* x, Vec3* velocity, int flag) {
  return g_calls.fly_off ? g_calls.fly_off(*this, from, x, velocity, flag) : nullptr;
}

// Mech3.exe 1.2 @ 0x00460cc0 / 0x00460c10. A destroyed item still counts.
bool MechPart::has_equipment(int kind) {
  if (!node) return false;
  for (EquipmentItem* it : equipment_in(*mech, id))
    if (it->equipment == kind) return true;
  return false;
}

// Mech3.exe 1.2 @ 0x004617f0. Sinks in the part are lost; other equipment
// takes one off the mech's count of that kind per item.
void MechPart::lose_contents() {
  destroyed = true;
  mech->on_part_destroyed(this);
  internal = 0.0f;
  armor = 0.0f;
  Mech& m = *mech;
  for (EquipmentItem* it : equipment_in(m, id)) {
    if (it->group != 0 || it->slots() <= 0) continue;  // group: destroyed by a critical
    const int k = it->equipment;
    if (k == kSink || k == kDoubleSink) m.heat_sinks.lose(it->count);
    else if (m.equipment[k] > 0) --m.equipment[k];
  }
}

// Mech3.exe 1.2 @ 0x00461a30. Arms and thighs fly off (not on the
// Elemental); the owner's effects play; the contents are lost.
void MechPart::destroy(int variant, int fly) {
  if (destroyed || !mech) return;
  Mech& m = *mech;
  if (g_calls.weapons_lost) g_calls.weapons_lost(m, id);
  Vec3 from{}, x{}, vel{};
  Handle at;
  bool trail;
  if (m.pilot->roster != kElemental && (id == kLeftArm || id == kRightArm || id == kLeftThigh || id == kRightThigh)) {
    at = fly_off(&from, &x, &vel, fly);
    trail = static_cast<uint8_t>(fly) != 0;
  } else {
    from = {0, 0, 0};
    vel = m.velocity;
    at = node;
    trail = false;
  }
  const bool listed = id >= 1 && id <= 5;
  const Handle effect = listed ? m.pilot->part_effects[id][variant] : nullptr;
  if (at && effect) debris_effect = spawn(effect, at, {from.x, from.y, from.z, 0, 0, 0, vel.x, vel.y, vel.z});
  if (trail && node) {
    const Handle e = listed ? m.pilot->part_effects[id][2] : nullptr;
    if (e) trail_effect = spawn(e, node, {from.x, from.y, from.z, 0, 0, 0, 0, 0, 0});
  }
  if (trail_effect) on_end(trail_effect, *this);
  if (debris_effect) on_end(debris_effect, *this);
  lose_contents();
}

// Mech3.exe 1.2 @ 0x00460d50. Docs: damage.md, critical hits.
bool MechPart::critical(int kind, int) {
  Mech& m = *mech;
  void* scr = player_screen(m);
  const bool player = scr != nullptr;
  if (!m.active) return false;
  bool ok = false;
  switch (kind) {
    case kCritSink: {
      // FIX (bug): 0x0045fbc0 always reports failure, so the callout never
      // played; a sink actually lost counts as success (docs/fixes.md).
      const int32_t before = m.heat_sinks.count;
      m.heat_sinks.lose(1);
      ok = m.heat_sinks.count < before;
      if (!player) return ok;
      if (ok && g_calls.equip_voice) g_calls.equip_voice(kSink);
      break;
    }
    case kCritWeapon:
      ok = g_calls.weapons_critical && g_calls.weapons_critical(m, id, 0);
      if (!player) return ok;
      if (ok && g_calls.hud_weapons) g_calls.hud_weapons(scr);
      break;
    case kCritAmmo:
      if (has_equipment(kCase)) break;
      ok = g_calls.weapons_critical && g_calls.weapons_critical(m, id, 1);
      if (!player) return ok;
      if (ok) {
        if (g_calls.hud_weapons) g_calls.hud_weapons(scr);
        mech_sound(kMechSoundAmmo);
      }
      break;
    case kCritEquipment: {
      // Intact equipment other than heat sinks, weighted by its slots. The
      // exe keeps at most 12 candidates on the stack; no part holds more.
      std::vector<EquipmentItem*> items;
      std::vector<float> weights;
      int32_t total = 0;
      for (EquipmentItem* it : equipment_in(m, id)) {
        if (it->group != 0 || it->equipment == kSink || it->equipment == kDoubleSink) continue;
        const int32_t n = it->slots();
        if (n <= 0) continue;
        items.push_back(it);
        weights.push_back(static_cast<float>(n));
        total += n;
      }
      if (items.empty()) break;
      EquipmentItem* hit = items[0];
      if (items.size() > 1) {
        const float inv = 1.0f / static_cast<float>(total);
        std::vector<float> cdf(items.size());
        float sum = 0.0f;
        for (size_t i = 0; i < items.size(); ++i) cdf[i] = sum = sum + weights[i] * inv;
        const float r = rand_unit();
        // FIX (bug): when rounding leaves r above the last bound the exe uses
        // r's bits as the item pointer (0x00461211), a crash; take the last.
        hit = items.back();
        for (size_t i = 0; i < items.size(); ++i)
          if (!(r > cdf[i])) {
            hit = items[i];
            break;
          }
      }
      hit->group = 1;
      if (m.equipment[hit->equipment] > 0) --m.equipment[hit->equipment];
      if (player && g_calls.equip_voice) g_calls.equip_voice(hit->equipment);
      if (void* s = player_screen(m); s && g_calls.hud_weapons) g_calls.hud_weapons(s);
      ok = true;
      break;
    }
    case kCritSpecial:
      if (id == kTorso) {
        // An engine hit: 15% off the top speed, each time.
        if (player && g_calls.voice) g_calls.voice(6);
        m.set_max_speed(m.max_speed * 0.85f);
        ok = true;
        break;
      }
      if (id == kLeftThigh || id == kRightThigh) {
        // Never reached: the Leg class handles its own special critical.
        m.scale_knockdown(0.85f);
        const float chance = (100.0f - m.tonnage()) * 0.01f;
        if (chance > rand_unit()) m.fall();
        ok = true;
        break;
      }
      if (id == kHead && !m.invincible) destroy(0, 1);
      ok = true;
      break;
    default:
      break;
  }
  if (player && ok) mech_sound(kMechSoundCritical);
  return ok;
}

// --- Torso -----------------------------------------------------------------

// Mech3.exe 1.2 @ 0x00464720 (arm 0x00466ba0 kind 3, head 0x00467200 kind 4):
// the weapon's ZAPS_HUD static on the player's screen, then the hit.
static void zap(MechPart& p, WeaponDef* def, int kind) {
  WeaponType* t = def ? def->type : nullptr;
  void* scr = player_screen(*p.mech);
  if (scr && t && t->zaps_hud > 0.0f && g_calls.zaps_hud) g_calls.zaps_hud(scr, t->zaps_hud, kind, t->zaps_hud_2);
}

int Torso::take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) {
  zap(*this, def, 4);
  return MechPart::take_hit(def, coll, damage, impulse);
}

float* Torso::internal_slot(int s) {
  if (s == 0 || s == 3) return &internal;
  if (s == 1 || s == 4) return &side_internal[0];
  if (s == 2 || s == 5) return &side_internal[1];
  return &internal;
}

// Mech3.exe 1.2 @ 0x004649c0
float Torso::get_armor(int s) const { return s == 0 ? armor : sec_armor[s]; }
// Mech3.exe 1.2 @ 0x004649f0
float Torso::get_armor_scale(int s) const { return s == 0 ? armor_scale : sec_armor_scale[s]; }
// Mech3.exe 1.2 @ 0x00464a20
float Torso::get_internal(int s) const { return *const_cast<Torso*>(this)->internal_slot(s); }

// Mech3.exe 1.2 @ 0x00464a70
float Torso::get_internal_scale(int s) const {
  if (s == 0 || s == 3) return internal_scale;
  if (s == 1 || s == 4) return side_internal_scale[0];
  if (s == 2 || s == 5) return side_internal_scale[1];
  return 0.0f;
}

// Mech3.exe 1.2 @ 0x00464950. A section whose structure is gone keeps 0 armor.
void Torso::set_armor(float v, int s) {
  const float val = zeroish(*internal_slot(s > 2 ? s - 3 : s)) ? 0.0f : v;
  if (s == 0) armor = val;
  else sec_armor[s] = val;
}

// Mech3.exe 1.2 @ 0x00464830. Centre at 0 destroys the torso (death); a side
// torso at 0 takes its arm.
void Torso::set_internal(float v, int s) {
  float* slot = internal_slot(s);
  Mech& m = *mech;
  if (*slot < v || *slot > v) {
    *slot = v;
    const int part = (s == 1 || s == 4) ? kLeftTorso : (s == 2 || s == 5) ? kRightTorso : kTorso;
    if (!(v > 0.0)) {
      *slot = 0.0f;
      if (part == kTorso) destroy(0, 1);
      else if (part == kLeftTorso) m.arms[0]->set_internal(0.0f, 0);
      else m.arms[1]->set_internal(0.0f, 0);
    }
    net_event(m, kNetSection, part);
  }
  m.damaged = true;
}

// Mech3.exe 1.2 @ 0x00460350. Which section a shot hits: the footprint's
// edges in a fixed order, each counting only for a shot entering through it
// (docs/damage.md, front or rear).
int Torso::hit_section(const Vec3* shot) const {
  if (!shot) return 0;
  const float c = 0.2f * half_width, w = half_width, d = half_depth;  // 0x005e4ef0
  const float nc = -c, nw = -w, nd = -d;
  const Vec3 local[16] = {{c, 0, nd}, {nc, 0, nd},  {nc, 0, d},  {c, 0, d},   // centre front, rear
                          {nc, 0, nd}, {nw, 0, nd}, {nw, 0, d},  {nc, 0, d},  // left front, rear
                          {w, 0, nd},  {c, 0, nd},  {c, 0, d},   {w, 0, d},   // right front, rear
                          {nw, 0, nd}, {nw, 0, d},  {w, 0, d},   {w, 0, nd}};  // left, right side
  const float* m = matrix(*const_cast<Torso*>(this));
  Vec3 pts[16];
  for (int i = 0; i < 16; ++i) {
    const Vec3& p = local[i];
    pts[i] = {((p.x * m[0] + p.y * m[3]) + p.z * m[6]) + m[9], ((p.x * m[1] + p.y * m[4]) + p.z * m[7]) + m[10],
              ((p.x * m[2] + p.y * m[5]) + p.z * m[8]) + m[11]};
  }
  // The shot stretched ten times past each end (0x005e4ef4).
  const Vec3& s0 = shot[0];
  const Vec3& s1 = shot[1];
  const Vec3 a{(s1.x - s0.x) * 10.0f + s0.x, (s1.y - s0.y) * 10.0f + s0.y, (s1.z - s0.z) * 10.0f + s0.z};
  const Vec3 b{(s0.x - s1.x) * 10.0f + s1.x, (s0.y - s1.y) * 10.0f + s1.y, (s0.z - s1.z) * 10.0f + s1.z};
  static const int kSection[8] = {0, 3, 1, 4, 2, 5, 1, 2};
  int edge = 0;
  while (edge < 8 && !crosses(b, a, pts[2 * edge], pts[2 * edge + 1])) ++edge;
  if (edge == 8) return 0;
  const int s = kSection[edge];
  // A destroyed side torso passes the hit to the centre, front or rear.
  const float left = (s == 1 || s == 4) ? side_internal[0] : (s == 2 || s == 5) ? side_internal[1] : internal;
  if (!zeroish(left)) return s;
  return s <= 2 ? 0 : 3;
}

// Mech3.exe 1.2 @ 0x00464af0. A jolt: a spin about the horizontal axis
// across the hit, and a shove scaled per axis, both in the torso's frame.
void Torso::apply_impact(Vec3* impulse, float force) {
  if (zeroish(impulse->z) && zeroish(impulse->x)) {
    jolt_spin = {0, 0, 0};
    return;
  }
  Vec3 axis{impulse->z, 0.0f, -impulse->x};
  if (g_calls.node_update) g_calls.node_update(*this);  // 0x00461cf0
  float mtx[12];
  const float* src = matrix(*this);
  for (int i = 0; i < 12; ++i) mtx[i] = src[i];
  axis = rotate_back(axis, mtx);
  const float k = 0.2617994f * force * -0.5f;  // 15 degrees, 0x005c2738
  jolt_spin = {k * axis.x, k * axis.y, k * axis.z};
  jolt = {jolt_scale[0] * impulse->z * -0.5f, jolt_scale[1] * impulse->x * 0.5f, jolt_scale[2] * impulse->x * 0.5f};
  jolt = rotate_back(jolt, mtx);
}

// Mech3.exe 1.2 @ 0x00465f40. Death of the mech: the effect, the fall
// animation by speed, the contents, then Die.
void Torso::destroy(int explode, int) {
  if (destroyed) return;
  Mech& m = *mech;
  World& w = world();
  if (explode == 1) m.killed_by_nuke = true;
  else if (!m.killed_by_nuke) m.killed_by_nuke = false;  // a no-op in the exe
  int effect;
  if (explode == 1 || m.pilot->roster == kElemental) {
    effect = 1;
    if (m.foot(0)->in_water && m.foot(1)->in_water && m.pilot->water_death) {
      const Vec3 p = g_calls.position ? *g_calls.position(m) : Vec3{};
      death_effect = spawn(m.pilot->water_death, nullptr, {p.x, m.foot(0)->water_surface, p.z, 0, 0, 0, 0, 0, 0});
    }
  } else {
    int anim;
    effect = 0;
    const float speed = m.vec_3c.z;
    if (m.airborne() || !(speed >= -13.888889f)) {  // 50 km/h, 0x005c2818
      anim = 1;
      effect = 2;
    } else if (speed > 13.888889f) {
      anim = 2;
      effect = 3;
    } else {
      // Slow deaths alternate between two falls, one flag for all mechs.
      anim = w.death_anim_toggle ? 8 : 7;
      w.death_anim_toggle = !w.death_anim_toggle;
    }
    if (g_calls.death_prep) g_calls.death_prep(m);
    if (g_calls.death_anim) g_calls.death_anim(m, anim);
  }
  if (g_calls.node_set_name) g_calls.node_set_name(m.node, "deadmech");  // 0x005e503c
  const Handle e = m.pilot->death_effects[effect];
  if (e && g_calls.spawn_effect_at)
    death_effect2 = g_calls.spawn_effect_at(e, m.node, m.velocity.x, m.velocity.y, m.velocity.z);
  if (death_effect) on_end(death_effect, *this);
  if (death_effect2) on_end(death_effect2, *this);
  lose_contents();
  m.die();
}

// --- Legs ------------------------------------------------------------------

// Mech3.exe 1.2 @ 0x00463b40. After the hit, a chance to start limping:
// (1 - h)(p - h) for structure h, p = 0.35 for the player and 1 for others,
// and always below 30%.
int Leg::take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) {
  const int r = MechPart::take_hit(def, coll, damage * 1.0f, impulse);  // 0x005e1f74
  Mech* m = mech;
  if (!m) return r;
  // Mech vtable +0x5c (0x0040bae0) is 0 for every mech.
  if (m->limp == 0 || m->limp == 1) return r;
  const float p = m == world().mission_player ? 0.35f : 1.0f;
  const float k = p - -1.0f;  // 0x005c27b4
  const float h = internal;
  const float chance = (h * h + -k * h) + p;
  if (!(rand_unit() >= chance) || !(get_internal(0) >= 0.3f)) {  // 0x005e1f78
    const bool fall = (crt::msvc_rand() & 1) != 0;
    m->start_limp(id != kLeftThigh, fall);
  }
  return r;
}

// Mech3.exe 1.2 @ 0x00463ef0. The special critical starts a limp, once.
bool Leg::critical(int kind, int section) {
  if (kind != kCritSpecial) return MechPart::critical(kind, section);
  Mech& m = *mech;
  if (m.limp == 0 || m.limp == 1) return false;
  const bool fall = (crt::msvc_rand() & 1) != 0;
  m.start_limp(id == kLeftThigh ? 0 : 1, fall);
  return true;
}

// Mech3.exe 1.2 @ 0x00463f70. Losing a thigh kills.
void Leg::destroy(int variant, int fly) {
  if (destroyed) return;
  Foot* f = foot;
  if (f && f->effect && g_calls.stop_foot_effect) g_calls.stop_foot_effect(f->effect);
  if (g_calls.node_set_active) g_calls.node_set_active(f->node, 0);  // no check for a foot, as in the exe
  MechPart::destroy(variant, fly);
  if (f) {
    f->lose_contents();
    f->detach();
  }
  if (calf) {
    calf->lose_contents();
    calf->detach();
  }
  mech->fall();
  mech->die();
}

// Mech3.exe 1.2 @ 0x00462d80
Leg* Foot::leg() const { return this == mech->foot(0) ? mech->legs[0] : mech->legs[1]; }

// Mech3.exe 1.2 @ 0x00462980
int Foot::take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) {
  return leg()->take_hit(def, coll, damage, impulse);
}

// Mech3.exe 1.2 @ 0x004629f0
void Foot::destroy(int variant, int fly) { leg()->destroy(variant, fly); }

// Mech3.exe 1.2 @ 0x00462890
void Foot::set_internal(float v, int s) {
  if (Leg* l = leg()) l->set_internal(v, s);
}

// Mech3.exe 1.2 @ 0x00463100
void Foot::stop_effect() {
  if (effect && g_calls.stop_foot_effect) g_calls.stop_foot_effect(effect);
}

// Mech3.exe 1.2 @ 0x00466f80
Leg* Calf::leg() const { return mech->calf(0) == this ? mech->legs[0] : mech->legs[1]; }

// Mech3.exe 1.2 @ 0x00466f50
int Calf::take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) {
  return leg()->take_hit(def, coll, damage, impulse);
}

// Mech3.exe 1.2 @ 0x00466f30
void Calf::destroy(int variant, int fly) { leg()->destroy(variant, fly); }

// Mech3.exe 1.2 @ 0x00466fc0
bool Calf::critical(int kind, int section) { return leg()->critical(kind, section); }

// Mech3.exe 1.2 @ 0x00466f00
void Calf::set_internal(float v, int s) {
  if (Leg* l = leg()) l->set_internal(v, s);
}

// --- Arms, head, hip -------------------------------------------------------

int Arm::take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) {
  zap(*this, def, 3);
  return MechPart::take_hit(def, coll, damage, impulse);
}

// Mech3.exe 1.2 @ 0x00466c30 (also head, hip and hand): the torso takes the jolt.
void Arm::apply_impact(Vec3* impulse, float force) { mech->torso->apply_impact(impulse, force); }

// Mech3.exe 1.2 @ 0x00466c50. The hand goes with the arm.
void Arm::destroy(int variant, int fly) {
  if (destroyed) return;
  internal = 0.0f;
  MechPart::destroy(variant, fly);
  if (!hand) return;
  mech->on_part_destroyed(hand);
  MechPart* h = hand;
  mech->parts.remove(h);
  if (g_calls.free_part) g_calls.free_part(h);
  else delete h;
  hand = nullptr;
}

int Head::take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) {
  zap(*this, def, 4);
  return MechPart::take_hit(def, coll, damage, impulse);
}

void Head::apply_impact(Vec3* impulse, float force) { mech->torso->apply_impact(impulse, force); }

// Mech3.exe 1.2 @ 0x004671e0. Losing the head kills; its own contents stay.
void Head::destroy(int, int) {
  destroyed = true;
  mech->torso->destroy(0, 1);
}

// Mech3.exe 1.2 @ 0x00462560: straight to the torso's plain hit, no HUD static.
int Hip::take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) {
  return mech->torso->base_take_hit(def, coll, damage, impulse);
}

void Hip::apply_impact(Vec3* impulse, float force) { mech->torso->apply_impact(impulse, force); }

// Mech3.exe 1.2 @ 0x00462490
void Hip::destroy(int, int) { mech->torso->destroy(0, 0); }

// Mech3.exe 1.2 @ 0x00466e40
Arm* Hand::arm() const {
  if (id == kLeftHand) return mech->arms[0];
  if (id == kRightHand) return mech->arms[1];
  return nullptr;
}

// Mech3.exe 1.2 @ 0x00467440
int Hand::take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) {
  return arm()->take_hit(def, coll, damage, impulse);
}

void Hand::apply_impact(Vec3* impulse, float force) { mech->torso->apply_impact(impulse, force); }

// Mech3.exe 1.2 @ 0x00466e10
void Hand::destroy(int variant, int fly) {
  if (destroyed) return;
  if (Arm* a = arm()) a->destroy(variant, fly);
}

// Mech3.exe 1.2 @ 0x00466eb0
void Hand::set_internal(float v, int s) {
  if (Arm* a = arm()) a->set_internal(v, s);
}

// Mech3.exe 1.2 @ 0x00466200. A non-zero twist takes the torso off the AI's aim.
bool Torso::set_twist(float twist, float look) {
  if (twist != 0.0f) aim = 0;
  twist_in = twist;
  look_yaw = look;
  return true;
}

// Mech3.exe 1.2 @ 0x00466240
bool Torso::set_pitch(float pitch, float look) {
  if (pitch != 0.0f) aim = 0;
  pitch_in = pitch;
  look_pitch = look;
  return true;
}

}  // namespace mech3
