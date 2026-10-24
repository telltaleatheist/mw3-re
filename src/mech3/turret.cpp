#include "mech3/turret.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "mech3/veh_controls.h"
#include "mech3/weapon.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::tur {

namespace {
TurretHooks g_default_hooks;
TurretHooks* g_hooks = &g_default_hooks;

Handle g_markers[3] = {};      // [0x00609a38..0x00609a40]
zrdr::Node* g_root = nullptr;  // [0x00609a44] aiTurrets.zrd
int32_t g_count = 0;           // [0x00609a48]
std::vector<Turret*> g_list;   // ours: zEntity's list holds them in the exe

// A list's count (its first slot) and item i's value as the exe reads it,
// whatever its type. FIX (bug): a node that is not a list, or an item past
// its end, reads as none (the exe reads past the list; no shipped file has
// one; docs/fixes.md).
int32_t count(const zrdr::Node* n) { return n && n->type == zrdr::List ? n->list[0].i : 0; }
const zrdr::Node* item(const zrdr::Node* n, int32_t i) { return i < count(n) ? &n->list[i] : nullptr; }
const char* str(const zrdr::Node* n, int32_t i) {
  const zrdr::Node* v = item(n, i);
  return v && v->type == zrdr::String ? v->s : nullptr;
}
float raw_float(const zrdr::Node* n, int32_t i) {  // a float's bits, or an int's
  const zrdr::Node* v = item(n, i);
  return v ? zgame::from_bits(static_cast<uint32_t>(v->i)) : 0.0f;
}

double dbits(uint64_t u) {
  double d;
  std::memcpy(&d, &u, 8);
  return d;
}
const double kPi = dbits(0x400921fb54442eeaull);   // [0x005c82e0] 3.14159265359
const double kTwoPi = dbits(0x401921fb54442eeaull);  // [0x005c82e8]
const double kDegToRad = dbits(0x3f91df46a2529983ull);  // [0x005c8860] 0.01745329251994
const float kPitch = zgame::from_bits(0x3fc90fd8u);             // 1.570796, the push in 0x004a4ac0's callers
const float kHalfPi = zgame::from_bits(0x3fc90fdbu);
const float kTwoPiF = zgame::from_bits(0x40c90fdbu);

float fsin(float a) { return static_cast<float>(std::sin(static_cast<double>(a))); }
float fcos(float a) { return static_cast<float>(std::cos(static_cast<double>(a))); }
}  // namespace

void set_turret_hooks(TurretHooks* h) { g_hooks = h ? h : &g_default_hooks; }
TurretHooks& turret_hooks() { return *g_hooks; }
int32_t& turret_count() { return g_count; }

zrdr::Node* TurretHooks::read(const char* file) { return zrdr::load(file); }
void TurretHooks::free_tree(zrdr::Node* root) { zrdr::free_node(root); }
void TurretHooks::node_world_position(Handle node, float out[3]) {
  unported("0x004ed650", {node});
  out[0] = out[1] = out[2] = 0;
}
void TurretHooks::node_rotation(Handle node, float out[3]) {
  unported("0x004fbe90", {node});
  out[0] = out[1] = out[2] = 0;
}
void TurretHooks::node_translation(Handle node, float out[3]) {
  unported("0x004fc020", {node});
  out[0] = out[1] = out[2] = 0;
}
int32_t TurretHooks::node_box(Handle node, float box[6]) {
  (void)box;
  return static_cast<int32_t>(unported("0x004ebd50", {node}));
}

// --- The turn limits (0x004a3fc0..0x004a4b30) ----------------------------------------

// Mech3.exe 1.2 @ 0x004a4a70 over 0x004a4000 (0x004a3fc0 twice).
void TurnLimits::make_full(float lo, float hi) {
  lo_sin = fsin(lo), lo_cos = fcos(lo);
  hi_sin = fsin(hi), hi_cos = fcos(hi);
  kind = LimitKind::Full;
}

// Mech3.exe 1.2 @ 0x004a4b30: the old one's destructor (vt+0, not freed:
// it lives in the turret), then the kind by the yaw's range, compared in
// the x87 against a float 0.001 and the double pi and 2 pi.
void TurnLimits::make(float lo, float hi, float ylo, float yhi) {
  const float range = yhi - ylo;
  if (!(range > zgame::from_bits(0x3a83126fu))) {  // 0x004a42f0
    lo_sin = fsin(lo), lo_cos = fcos(lo), hi_sin = fsin(hi), hi_cos = fcos(hi);
    lo_edge[0] = fcos(ylo), lo_edge[1] = 0.0f, lo_edge[2] = -fsin(ylo);
    kind = LimitKind::Fixed;
    return;
  }
  if (static_cast<double>(range) >= kTwoPi) {
    make_full(lo, hi);
    return;
  }
  // 0x004a4580 / 0x004a4800 over 0x004a4270.
  lo_sin = fsin(lo), lo_cos = fcos(lo), hi_sin = fsin(hi), hi_cos = fcos(hi);
  lo_edge[0] = -fcos(ylo), lo_edge[1] = 0.0f, lo_edge[2] = fsin(ylo);
  hi_edge[0] = fcos(yhi), hi_edge[1] = 0.0f, hi_edge[2] = -fsin(yhi);
  kind = static_cast<double>(range) >= kPi ? LimitKind::Wide : LimitKind::Narrow;
}

// --- The turret ----------------------------------------------------------------------

// Mech3.exe 1.2 @ 0x004a5c60, over AiTarget 0x004d56e0(0, 0) (zEntity3d
// 0x005a7e10(0), team 0). The team is the enemies' (0x004d5af0(1): 2); the
// limits start a full turn on -pi/2..pi/2 (0x004a4af0), then -1.570796..
// 1.570796 (0x004a4ac0).
Turret::Turret(Handle s, Handle n, Handle generic_anim, zrdr::Node* def) : self(s) {
  TurretHooks& h = *g_hooks;
  h.entity_init(self);
  set_team(0);
  limits.make_full(-kHalfPi, kHalfPi);  // 0x004a4af0: 0x004a4a70(-pi/2, pi/2, -pi, pi)
  set_team(2);
  id = g_count++;
  limits.make(-kPitch, kPitch, -kTwoPiF, kTwoPiF);  // 0x004a4ac0
  if (n) {
    set_node(n);
    h.entity_list(self);
  }
  setup(generic_anim, def);
}

// Mech3.exe 1.2 @ 0x004a6230 (with 0, 0, 0): a turret losing its node
// takes the weapons' handlers off it first.
void Turret::set_node(Handle n) {
  TurretHooks& h = *g_hooks;
  if (!n && node) h.remove_handlers(node);
  node = n;
  h.entity_set_node(self, n, &target_node, &centroid_node);  // 0x004d6c90
}

// Mech3.exe 1.2 @ 0x004a6270: the definition read (docs/vehicles.md, The
// turrets' definitions).
void Turret::setup(Handle generic_anim, zrdr::Node* def) {
  TurretHooks& h = *g_hooks;
  Handle destroy = generic_anim;
  if (const Handle a = h.anim_find(h.node_name(node))) destroy = a;  // an animation named as the node
  if (const zrdr::Node* parts = zrdr::find(def, "PARTS")) {
    healthy = h.node_child(node, "healthy");
    if (healthy) {
      active = 1;
      if (count(parts) == 5) {
        firepoints = 2;
        turret = h.node_find(node, str(parts, 1));
        gun = h.node_find(node, str(parts, 2));
        fire_node[0] = h.node_find(node, str(parts, 3));
        fire_node[1] = h.node_find(node, str(parts, 4));
      }
      if (count(parts) == 4) {
        firepoints = 1;
        turret = h.node_find(node, str(parts, 1));
        gun = h.node_find(node, str(parts, 2));
        fire_node[0] = h.node_find(node, str(parts, 3));
      } else if (count(parts) == 3) {
        firepoints = 1;
        gun = h.node_find(node, str(parts, 1));
        fire_node[0] = h.node_find(node, str(parts, 2));
      }
    }
  }
  if (const zrdr::Node* d = zrdr::find(def, "DEACTIVATE")) {
    off_node = h.node_named(str(d, 1));
    for (int32_t i = 2; i < count(d); ++i) off_node = h.node_child(off_node, str(d, i));
  }
  if (const zrdr::Node* e = zrdr::find(def, "EFFECT")) {
    effect = h.node_find(node, str(e, 1));
    effect_time = raw_float(e, 2);
    if (effect) h.model_scroll(h.node_model(effect));
  }
  per_damage = 1.0f;
  health = 1.0f;
  zrdr::get_float(def, "HEALTH", &per_damage);
  per_damage = 1.0f / per_damage;
  if (const char* s = zrdr::get_string(def, "DESTROY_ANIM")) destroy = h.anim_find(s);
  auto anims = [&](const char* key, std::list<Handle>& out, bool claim) {
    const zrdr::Node* l = zrdr::find(def, key);
    if (!l) return;
    for (int32_t i = 0; i < count(l) - 1; ++i) {
      const Handle a = h.anim_find(str(l, i + 1));
      if (!a) continue;
      if (claim) h.anim_claim(self, a);
      out.push_back(a);
    }
  };
  anims("ONFIRE_ANIM", onfire, false);
  anims("WAKEUP_ANIM", wakeup, false);
  anims("SLEEP_ANIM", sleep, false);
  anims("AUX_ANIM", aux, true);
  bool b;
  if (zrdr::get_bool(def, "IS_IDLE", &b)) idle = b ? 1 : 0;
  if (const zrdr::Node* snd = zrdr::find(def, "SOUNDS"))
    if (const zrdr::Node* fire = zrdr::find(const_cast<zrdr::Node*>(snd), "FIRE")) fire_sound = h.sound_find(str(fire, 1));
  int32_t box_hit = 1;
  if (zrdr::Node* lim = zrdr::find(def, "LIMITS")) {
    float yaw[2] = {-360.0f, 360.0f};
    if (zrdr::get_vec2(lim, "YAW", yaw)) {
      yaw_lo = static_cast<float>(static_cast<double>(yaw[0]) * kDegToRad);
      yaw_hi = static_cast<float>(static_cast<double>(yaw[1]) * kDegToRad);
      limits.make(-kPitch, kPitch, yaw_lo, yaw_hi);
    }
  }
  zrdr::get_int(def, "BBOX", &box_hit);
  if (zrdr::Node* w = zrdr::find(def, "WEAPON")) {
    const zrdr::Node* name = zrdr::find(w, "NAME");
    if (name && count(name) == 2) weapon = h.weapon_def(str(name, 1));
    zrdr::get_int(w, "AMMO", &ammo);
    // FIX (bug): a WEAPON without its weapon (tur_ams's NAME is empty) and
    // without DETECTION_RANGE reads the null definition's range in the
    // exe; the range stays 200 (no shipped turret has both; docs/fixes.md).
    if (!zrdr::get_float(w, "DETECTION_RANGE", &detect_range) && weapon) detect_range = h.weapon_range(weapon);
    if (const zrdr::Node* fl = zrdr::find(w, "FIRE_LIMITS")) {
      burst[0] = raw_float(fl, 1);
      burst[1] = raw_float(fl, 2);
    }
    zrdr::get_float(w, "FIRE_RATE", &rate);
  }
  float ams_range = 0.0f;
  if (zrdr::get_float(def, "AMS_RANGE", &ams_range)) {
    ams_on = 1;
    ams = new Ams(ams_range, 0.0f, 100);  // 0x004b3ab0
  }
  h.node_world_position(node, pos);
  h.node_rotation(node, facing);
  vec_normalise(facing);
  aim_from[0] = pos[0], aim_from[1] = pos[1], aim_from[2] = pos[2];
  if (turret) {
    turret_matrix = h.node_matrix(turret);
    aim_from[1] = h.matrix_y(turret_matrix) + aim_from[1];
  }
  if (gun) {
    gun_matrix = h.node_matrix(gun);
    aim_from[1] = h.matrix_y(gun_matrix) + aim_from[1];
  }
  // The firepoints' mean height (0 / 0 without one: the inactive turrets').
  float sum = 0.0f;
  int32_t n = 0;
  for (int k = 0; k < 2; ++k) {
    if (!fire_node[k]) continue;
    h.node_translation(fire_node[k], muzzle[k]);
    sum = sum + muzzle[k][1];
    ++n;
  }
  aim_from[1] = sum / static_cast<float>(n) + aim_from[1];
  if (effect) h.node_set_active(effect, 0);
  h.node_set_box_hit(healthy, box_hit);
  if (weapon && (h.weapon_flags(weapon) & 2) != 0) {
    beam = h.beam_make(weapon, node, gun, self + 0x16c, self + 0x160);
    rate = burst[0];
  }
  if (destroy) {
    destroy_anim = destroy;
    h.anim_reset(destroy, node);
    if (const Handle d = h.node_find(node, "destroyed")) h.node_set_active(d, 0);
    if (health > 0.0f && !h.set_hit_callback(self, node))
      h.report(0x221, "Failed to install weapons callback for (%s)", h.node_name(node));
  } else {
    h.report(0x226, "No destroy animation for (%s)", h.node_name(node));
  }
  h.nodes_pending();
  // The box's height and width (8 and 4 without one) place the markers and
  // give the AiTarget its height, unless TURRET_HEIGHT does.
  float tall = 8.0f, wide = 4.0f;
  float box[6];
  if (h.node_box(node, box) == 0) {
    tall = box[4] - box[1];
    wide = box[3] - box[0];
  }
  h.node_set_translation(centroid_node, 0.0f, tall * 0.5f, 0.0f);
  h.entity_markers(self);
  h.node_set_translation(target_node, 0.0f, tall * zgame::from_bits(0x3f266666u), 0.0f);
  h.entity_markers(self);
  if (zrdr::get_float(def, "TURRET_HEIGHT", &tall)) height = tall;
  else height = (tall > wide ? tall : wide) * zgame::from_bits(0x3f933333u);
  set_range(detect_range);
  h.entity_refresh(self);
}

// Mech3.exe 1.2 @ 0x004a5ff0 (0x004a5fd0 with the delete), over AiTarget's
// 0x004d58a0. The limits live in the turret.
Turret::~Turret() {
  TurretHooks& h = *g_hooks;
  delete ams;  // 0x005ac5c0, empty
  ams = nullptr;
  if (beam) beam = h.beam_free(beam);
  if (node) {
    set_node(0);
    h.entity_unlist(self);
  }
  clear_contacts();  // 0x004d58a0: 0x004a7be0, the node already gone
  net = nullptr;
  release_shadow();
  h.entity_end(self);
}

// Mech3.exe 1.2 @ 0x004a7af0
void Turret::aux_reset() {
  for (Handle a : aux) g_hooks->anim_reset(a, node);
}

// Mech3.exe 1.2 @ 0x004a74f0: 0x32 and 0x33 take IS_IDLE back and set it
// (the mission scripts' turret messages), 0x34 shuts it down destroyed.
void Turret::message(uint32_t from, int32_t msg) {
  if (msg == 0x32) {
    idle = 0;
  } else if (msg == 0x33) {
    idle = 1;
  } else if (msg == 0x34) {
    deactivate(true);
  } else {
    g_hooks->target_message(self, from, msg);
  }
}

// Mech3.exe 1.2 @ 0x004a7530 (through the callback 0x004a7940): the hit's
// damage over the definition's HEALTH; the attacker kept for the kill.
uint32_t Turret::hit(Handle weapon_def, Handle x, float damage) {
  (void)weapon_def, (void)x;
  TurretHooks& h = *g_hooks;
  health = health - per_damage * damage;
  attacker = h.node_object(h.hit_attacker());
  if (!(health > 0.0f)) {
    deactivate(true);
    h.hit_killed();
    return 0;
  }
  h.hit_health(health);
  return 0;
}

// Mech3.exe 1.2 @ 0x004a7600 (vt+0x70): killed outright, the player credited.
void Turret::kill() {
  health = 0.0f;
  attacker = g_hooks->player_vehicle();
  deactivate(true);
}

// Mech3.exe 1.2 @ 0x004a7620. The effect is put out only when it is not
// showing (as written: the test is the other way round from the setup's).
void Turret::deactivate(bool destroyed) {
  TurretHooks& h = *g_hooks;
  if (attacker) h.credit_kill(attacker, self);
  if (destroyed && destroy_anim && node) h.anim_run(destroy_anim, node);
  active = 0;
  if (effect && (h.node_flags(effect) & 4) == 0) h.node_set_active(effect, 0);
  h.remove_handlers(node);
  if (beam_on) {
    beam_on = 0;
    h.beam_stop(beam);
  }
  attacker = 0;
}

// Mech3.exe 1.2 @ 0x004a75b0 (vt+0x68): the target view's red tint, by the damage.
void Turret::tint(bool on) {
  TurretHooks& h = *g_hooks;
  if (!active) return;
  if (on) h.node_tint(node, (1.0f - health) * zgame::from_bits(0x3f19999au));
  h.node_tinted(node, on ? 1 : 0);
}

// Mech3.exe 1.2 @ 0x004a7a70
bool Turret::awake() const {
  TurretHooks& h = *g_hooks;
  if (!active || idle == 1) return false;
  return h.zones_meet(h.player_vehicle() + 0x64, self + 0x64) == 1;
}

// Mech3.exe 1.2 @ 0x004a7ab0
bool Turret::in_range() const { return active && player_contact && detect_range * detect_range > player_contact->dist2; }

// --- The file ---------------------------------------------------------------------------

// Mech3.exe 1.2 @ 0x004a76b0. A turret is made on every node of list 6
// whose name starts with a TURRET entry's (several entries can take the
// same node) and whose `healthy` child is active; nothing keeps it but
// zEntity's list.
bool turrets_load(const char* file, Handle at) {
  TurretHooks& h = *g_hooks;
  static const char* const kNames[3] = {"hud_turret_ally_sm", "hud_turret_enemy_sm", "hud_turret_other_sm"};
  for (int i = 0; i < 3; ++i) g_markers[i] = h.marker_create(kNames[i]);
  zrdr::Node* root = h.read(file);
  if (!root) {
    h.report(0x487, "Failed to read %s", file);
    return false;
  }
  g_root = root;
  Handle generic = 0;
  if (const zrdr::Node* g = zrdr::find(root, "GENERIC_DESTROY_ANIM")) generic = h.anim_find(str(g, 1));
  zrdr::Node* list = zrdr::find(root, "TURRET");
  if (!list) return true;
  for (int32_t i = 1; i < count(list); i += 2) {
    const char* name = str(list, i);
    zrdr::Node* def = name ? zrdr::find(list, name) : nullptr;
    if (!def) continue;
    h.nodes_first(name);
    for (Handle node = h.nodes_next(); node; node = h.nodes_next()) {
      const Handle healthy = h.node_find(node, "healthy");
      if (!healthy || (h.node_flags(healthy) & 4) == 0) continue;
      Turret* t = new Turret(h.turret_handle(), node, generic, def);
      g_list.push_back(t);
      h.entity_place(t->self, h.world_node(), at);  // vt+0x38
      t->aux_reset();
    }
  }
  return true;
}

// Mech3.exe 1.2 @ 0x004a7970: the turrets themselves go with zEntity's list.
void turrets_end() {
  TurretHooks& h = *g_hooks;
  for (Handle& m : g_markers) {
    if (m) h.marker_delete(m);
    m = 0;
  }
  if (g_root) {
    h.free_tree(g_root);
    g_root = nullptr;
  }
}

std::vector<Turret*>& turret_list() { return g_list; }

void turrets_delete_all() {
  for (Turret* t : g_list) g_hooks->turret_free(t);
  g_list.clear();
}
Handle turret_marker(int32_t side) { return side >= 0 && side < 3 ? g_markers[side] : 0; }

}  // namespace mech3::tur
