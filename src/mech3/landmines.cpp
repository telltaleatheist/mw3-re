#include "mech3/landmines.h"

#include <cstring>

#include "gamez/zclass/cls_di.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zweapon/zweapon.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
LandmineHooks g_hooks;
Landmines g_mines;

zrdr::Node* load(const char* file) { return g_hooks.load ? g_hooks.load(file) : zrdr::load(file); }
bool floats(const zrdr::Node* item) {
  return item->list[1].type == zrdr::Float && item->list[2].type == zrdr::Float && item->list[3].type == zrdr::Float;
}
}  // namespace

LandmineHooks& landmine_hooks() { return g_hooks; }
Landmines& landmines() { return g_mines; }

// Mech3.exe 1.2 @ 0x004135c0 (ECX 0x00604e48): landmines.zrd's "weapon" (a
// zWeapon definition by name), "radius" (else 20 m) and the mission's block,
// each (x y z) of it an armed mine added to the list. No file is a success; a
// missing or unknown weapon fails and leaves the tree unfreed, as the exe.
bool landmines_load(const char* file, const char* block) {
  Landmines& L = g_mines;
  zrdr::Node* root = load(file);
  if (!root) return true;
  const zrdr::Node* w = zrdr::find(root, "weapon");
  if (!w || w->type != zrdr::List || zrdr::list_count(w) != 2 || w->list[1].type != zrdr::String) return false;
  L.weapon = g_hooks.find_def ? g_hooks.find_def(w->list[1].s) : zwep::find_def(w->list[1].s);
  if (!L.weapon) return false;
  const zrdr::Node* r = zrdr::find(root, "radius");
  if (!r || r->type != zrdr::List || zrdr::list_count(r) != 2 || r->list[1].type != zrdr::Float) {
    L.radius = 20.0f;
    L.radius_sq = 400.0f;
  } else {
    L.radius = r->list[1].f;
    L.radius_sq = L.radius * L.radius;
  }
  const zrdr::Node* b = zrdr::find(root, block);
  if (b && b->type == zrdr::List) {
    for (int32_t i = 1; i < zrdr::list_count(b); ++i) {
      const zrdr::Node* item = &b->list[i];
      if (item->type != zrdr::List || zrdr::list_count(item) != 4 || !floats(item)) continue;
      Landmine m;
      m.at[0] = item->list[1].f, m.at[1] = item->list[2].f, m.at[2] = item->list[3].f;
      L.list.push_back(m);  // 0x004137c0 at the end
    }
  }
  if (g_hooks.free) g_hooks.free(root);
  else zrdr::free_node(root);
  return true;
}

// Mech3.exe 1.2 @ 0x00413770: the list emptied (the mission's cleanup).
void landmines_free() { g_mines.list.clear(); }

// Mech3.exe 1.2 @ 0x00413560 (ECX 0x00604e48), from every mech's update
// (0x0041a791, its place 0x005a85e0): each armed mine nearer than the radius
// (in 3D; a NaN distance counts as near, the x87's unordered compare) goes off.
bool landmines_check(const float at[3]) {
  if (!at) return false;  // the port's update may have no place yet
  bool any = false;
  for (Landmine& m : g_mines.list) {
    bool fired = false;
    if (m.armed) {
      const float dx = m.at[0] - at[0], dy = m.at[1] - at[1], dz = m.at[2] - at[2];  // 0x0051dfa0
      const float d2 = dz * dz + dy * dy + dx * dx;
      if (!(g_mines.radius_sq <= d2)) {
        landmine_detonate(m);
        fired = true;
      }
    }
    any |= fired;
  }
  return any;
}

// Mech3.exe 1.2 @ 0x00413470 (ECX the mine): the weapon goes off on the ground
// under the mine (asked from 50 m above it) through the stand-in shot, then
// its splash; the mine is disarmed. With no ground under it the impact is
// skipped and the splash comes from where the last mine went off.
Landmine* landmine_detonate(Landmine& m) {
  Landmines& L = g_mines;
  zwep::Projectile& p = L.shot;
  static zcls::GroundHits g;
  const int32_t none = g_hooks.ground ? g_hooks.ground(m.at[0], m.at[1] - -50.0f, m.at[2], &g) : 1;
  if (none == 0) {
    const zcls::GroundHit& gh = g.hit[0];
    zwep::HitRecord rec;
    rec.normal = {gh.normal[0], gh.normal[1], gh.normal[2]};
    rec.point = {m.at[0], gh.height, m.at[2]};
    rec.x18 = gh.zone[0];
    std::memcpy(&rec.distance_sq, &gh.zone[1], 4);
    rec.material = gh.material;
    rec.node = gh.node;
    p.spawn_at = rec.point;  // +0x24 and +0x48 both
    p.position = rec.point;
    p.damage_scale = 1.0f;
    p.node = nullptr;  // the exe's 0x00605438 (landmines.h)
    if (g_hooks.impact) g_hooks.impact(L.weapon, &rec, &p, nullptr);
    else zwep::impact(L.weapon, &rec, &p, nullptr);
  }
  if (L.weapon->impact_proximity > 0.0f) {
    static zwep::HitList hl;
    const int32_t n = g_hooks.splash_query ? g_hooks.splash_query(L.weapon, &p, 1, &hl)
                                           : zwep::splash_query(L.weapon, &p, 1, &hl);
    if (n) {
      if (g_hooks.splash) g_hooks.splash(L.weapon, &p, &hl, 0);
      else zwep::splash(L.weapon, &p, &hl, 0);
    }
  }
  m.armed = 0;
  return &m;
}

}  // namespace mech3
