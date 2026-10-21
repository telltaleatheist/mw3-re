#include "mech3/app_hud.h"

#include <cstdio>
#include <cstring>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zmodel/gmod_init.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/ztwod/twod.h"
#include "gamez/zvideo/zvid_dd.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "mech3/app_mech.h"
#include "mech3/app_view.h"
#include "mech3/hud.h"
#include "mech3/mech.h"
#include "mech3/mech_control.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_hud.h"
#include "mech3/mech_skis.h"
#include "gamez/zmodel/gmod_screen.h"
#include "mech3/mech_move.h"
#include "mech3/mech_parts.h"
#include "mech3/mission_world.h"
#include "mech3/player_view.h"
#include "mech3/weapon.h"
#include "mech3/weapon_planner.h"
#include "mech3/zai_target.h"

namespace mech3 {

namespace {
zcls::Node* camera_node() {
  PlayerView* v = program_view();
  return v ? static_cast<zcls::Node*>(v->camera) : nullptr;
}
zcls::Camera* camera_data() {
  zcls::Node* n = camera_node();
  return n ? static_cast<zcls::Camera*>(n->data) : nullptr;
}

// The HUD's outside on the ports. What is left unported answers 0 (the
// layout's mech fields, the nav list, sounds; hud.md section 9).
struct ProgramHudHooks : HudHooks {
  uint32_t id(const void* p) override { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  void player_camera_clip(float* n, float* f) override {  // 0x004f3b00
    const zcls::Camera* c = camera_data();
    *n = c ? c->get<float>(0xb0) : 1.0f;
    *f = c ? c->get<float>(0xb4) : 1.0f;
  }
  float player_camera_fov() override {
    const zcls::Camera* c = camera_data();
    return c ? c->get<float>(0xe8) : 1.0471976f;
  }
  void view_prepare() override {  // 0x004b51f0
    zcls::Node* n = camera_node();
    if (!n) return;
    zmth::Mat34 m;
    zmth::push(&m);
    zcls::camera_update(n);
    zmod::set_camera(camera_data());
    zmth::pop();
  }
  void unproject_world(const float s[3], float out[3]) override {  // 0x00523930
    const zmth::Vec3 in = {s[0], s[1], s[2]};
    zmth::Vec3 o;
    zmth::unproject_world(&in, &o, 1);
    out[0] = o.x, out[1] = o.y, out[2] = o.z;
  }
  void node_set_intersect(uint32_t, bool) override {}  // handles are 64-bit here: see mech_node
  uint32_t mech_node(Mech*) override { return 0; }
  int32_t segment_list(uint32_t, const float a[3], const float b[3], zwep::HitList* out) override {
    static zcls::LineHits r;
    zcls::Node* w = world_hooks().world;
    if (!w) return out->count = 0, 1;
    // The player's mech left out of the query (0x004eb6d0 on its node).
    Mech* m = static_cast<Mech*>(program_view_target());
    zcls::Node* own = m ? static_cast<zcls::Node*>(m->node) : nullptr;
    if (own) zcls::node_set_intersect(own, 0);
    const int32_t res = zcls::segment_list(w, a, b, &r);
    if (own) zcls::node_set_intersect(own, 1);
    out->count = r.count;
    out->total = r.total;
    for (int i = 0; i < r.count && i < 31; ++i) std::memcpy(&out->rec[i], &r.hit[i], sizeof(zwep::HitRecord));
    return res;
  }
  // The player's mech, its weapon manager and target (hud.md sections 12,
  // 13), read-only: the HUD's handles are the program's own pointers.
  static WeaponManager* wm(void* p) { return static_cast<WeaponManager*>(p); }
  // 0x00440a30 (the reticle over TARGET's box) waits for TARGET's tracking
  // (0x00440b00, hud.md section 7 item 6): none, unless a test forces it.
  bool force_over = false;
  bool reticle_over_target(void* t, int32_t x, int32_t y) override {
    return force_over || HudHooks::reticle_over_target(t, x, y);
  }
  static Weapon* wp(void* p) { return static_cast<Weapon*>(p); }
  void* mech_weapons(Mech* m) override { return m ? m->ammo : nullptr; }
  int32_t weapons_count(void* w) override { return w ? static_cast<int32_t>(wm(w)->count) : 0; }
  void* mech_target_slot(Mech* m) override { return m ? &m->target : nullptr; }  // +0x6c
  bool target_valid(void* t) override { return t && *static_cast<ai::Contact**>(t); }
  float group_range(void* w) override { return group_max_range(*wm(w)); }  // 0x004b1bd0
  bool group_locks(void* w) override { return group_locks_on(*wm(w)); }    // 0x004b1c70
  bool group_4b1cd0(void* w) override { return group_has_ammo(*wm(w)); }   // 0x004b1cd0
  void set_lock_shown(void* w, bool on) override { wm(w)->has_target = on; }  // +0x88
  void* slot_contact(void* slot) override { return *static_cast<ai::Contact**>(slot); }
  void* contact_mech(void* c) override {  // 0x004e6ff0
    return collide_calls().contact_mech ? collide_calls().contact_mech(c) : nullptr;
  }
  float contact_distance(void* c) override { return static_cast<ai::Contact*>(c)->distance(); }  // 0x004e7030
  int32_t mech_kind_of(void* m) override {
    const Mech* me = static_cast<Mech*>(m);
    return me->pilot ? me->pilot->roster : 0;  // +0x684 +0xc: 7 the Elemental
  }
  int32_t mech_3cc(void* m) override { return static_cast<Mech*>(m)->equipment[8]; }  // ECM
  // TARGET's tracking, the lead point and the data update (hud.md section
  // 14) on the player's mech and its target's mech: a contact's entity,
  // vehicle and seen object are the contact's mech here.
  static Mech* mech_of(void* p) { return static_cast<Mech*>(p); }
  static void mech_pos(Mech* m, float out[3]) {  // 0x005a85e0
    Vec3 v{0, 0, 0};
    if (skis_calls().position) v = *skis_calls().position(*m);
    else if (m->transform) v = {m->transform[9], m->transform[10], m->transform[11]};
    out[0] = v.x, out[1] = v.y, out[2] = v.z;
  }
  void contact_dir(void* c, float out[3]) override {
    const ai::Contact* k = static_cast<ai::Contact*>(c);
    out[0] = k->dir[0], out[1] = k->dir[1], out[2] = k->dir[2];
  }
  void* contact_entity(void* c) override { return contact_mech(c); }
  void* contact_vehicle(void* c) override { return contact_mech(c); }
  void* object_seen(void* c) override { return contact_mech(c); }
  void* target_object(void* slot) override {
    ai::Contact* c = slot ? *static_cast<ai::Contact**>(slot) : nullptr;
    return c ? contact_mech(c) : nullptr;
  }
  bool object_a8(void*) override { return false; }
  bool object_mech(void* o) override { return o != nullptr; }
  void* object_mech_ptr(void* o) override { return o; }
  const char* object_name(void*) override { return nullptr; }
  float contact_size(void* c) override {  // 0x004e7070: its vehicle's +0x68, else 5
    Mech* m = mech_of(contact_mech(c));
    return m ? m->height : 5.0f;
  }
  void entity_position(void* e, float out[3]) override {
    if (e) mech_pos(mech_of(e), out);
    else out[0] = out[1] = out[2] = 0.0f;
  }
  void vehicle_center(void* v, float out[3]) override {  // vt+0xbc: the node's place here (hud.md, Left)
    entity_position(v, out);
  }
  uint32_t object_team(void* o) override { return o ? mech_of(o)->team : 0; }
  uint32_t mech_allies(Mech* m) override { return m->allies; }
  uint32_t mech_enemies(Mech* m) override { return m->enemies; }
  uint32_t project(const float in[3], float out[3]) override {  // 0x0053cf10
    zmth::Vec3 o;
    const uint32_t code = zmod::project_point(reinterpret_cast<const zmth::Vec3*>(in), &o);
    out[0] = o.x, out[1] = o.y, out[2] = o.z;
    return code;
  }
  int32_t mech_part_count(Mech* m, int32_t i) override { return m ? m->equipment[i] : 0; }
  float mech_narc(void* m) override { return mech_of(m)->narc_time; }
  void* group_current_weapon(void* g) override { return static_cast<WeaponGroup*>(g)->current_weapon; }
  float weapon_speed(void* w) override { return wp(w)->def ? wp(w)->def->velocity : 0.0f; }
  uint32_t weapon_def_flags(void* w) override { return wp(w)->def ? wp(w)->def->flags : 0; }
  bool mech_aim_point(Mech*, float*) override { return false; }  // +0xec / +0xf8 not modelled (hud.md, Left)
  void mech_position(Mech* m, float out[3]) override { mech_pos(m, out); }
  void vehicle_position(void* v, float out[3]) override { entity_position(v, out); }
  void mech_velocity(Mech* m, float out[3]) override { out[0] = m->velocity.x, out[1] = m->velocity.y, out[2] = m->velocity.z; }
  void vehicle_velocity(void* v, float out[3]) override { mech_velocity(mech_of(v), out); }
  float controls_throttle(Mech* m) override { return m->controls ? m->controls->throttle : 0.0f; }
  float controls_throttle_target(Mech* m) override { return m->controls ? m->controls->throttle_target : 0.0f; }
  void mech_forward(Mech* m, float out[3]) override { out[0] = m->forward.x, out[1] = m->forward.y, out[2] = m->forward.z; }
  float mech_f(Mech* m, uint32_t off) override {
    switch (off) {
      case 0x518: return m->max_speed;
      case 0x528: return m->speed_factor;
      case 0x554: return m->heat;
      case 0x558: return m->heat_added;
      case 0x55c: return m->heat_dissipation;
      case 0x56c: return m->coolant;
      default: return 0.0f;
    }
  }
  float mech_jump_fuel(Mech* m) override { return m->jet_fuel; }  // vt+0x148
  void mech_radar_dir(Mech* m, float out[3]) override {  // 0x0041cdf0: the torso's flat front (+0x39c, +0x16c)
    if (!m->torso) return mech_forward(m, out);
    out[0] = m->torso->flat_front.x, out[1] = m->torso->flat_front.y, out[2] = m->torso->flat_front.z;
  }
  float mission_time(bool) override { return mech_game_time(); }
  // The radar (hud.md section 15): the mission map is not ported (no map
  // mode), nor the nav route's polylines (no points), the network's marker
  // or the landmines. Contacts resolve to mechs only (contact_mech); the
  // other vehicle classes' blips (their vt+0x3c) wait for those vehicles.
  float normalize(float v[3]) override {
    zmth::Vec3 d{v[0], v[1], v[2]};
    const float len = zmth::normalize(d);
    v[0] = d.x, v[1] = d.y, v[2] = d.z;
    return len;
  }
  void* radar_map() override { return nullptr; }
  float mech_sensor_range(Mech* m) override { return m->sensor_range; }
  uint8_t mech_bc(Mech* m) override { return m->xbc; }
  void mech_orientation(Mech* m, float q[4]) override { std::memcpy(q, m->orientation, sizeof m->orientation); }
  int32_t mech_contact_count(Mech* m) override { return static_cast<int32_t>(m->contacts.size()); }
  void* mech_contact(Mech* m, int32_t i) override { return m->contacts[static_cast<size_t>(i)]; }
  bool contact_on_radar(void* c) override { return (static_cast<ai::Contact*>(c)->flags & 1) != 0; }
  bool contact_same(void* a, void* b) override {
    return static_cast<ai::Contact*>(a)->entity == static_cast<ai::Contact*>(b)->entity;
  }
  void object_position(void* o, float out[3]) override { entity_position(o, out); }
  void* object_blip(void* o, int32_t kind) override {
    if (!o) return nullptr;
    if (!mech_radar_blip(0)) mech_radar_blips_create();  // our addition: made when first needed (0x00413e60's part)
    return mech_radar_blip(kind);
  }
  void blip_place(void* b, int32_t x, int32_t y) override {
    static_cast<ztwod::Image*>(b)->set_pos(x, y);
    blip_at[0] = x, blip_at[1] = y;
  }
  void blip_draw(void* b) override {
    static_cast<ztwod::Image*>(b)->draw();
    ++blips;
  }
  int32_t blips = 0, blip_at[2] = {};  // for program_hud_report
  void* network_marker() override { return nullptr; }
  bool radar_shot(int32_t i, float out[3], bool* friendly) override {
    auto& l = radar_blips();
    auto it = l.begin();
    for (int32_t k = 0; k < i && it != l.end(); ++k) ++it;
    if (it == l.end()) return false;
    const zwep::Vec3f3& p = (*it)->shot->position;
    out[0] = p.x, out[1] = p.y, out[2] = p.z;
    *friendly = (*it)->friendly;
    return true;
  }
  int32_t mine_count() override { return 0; }
  // 0x0040e920 (a node's game object) is not ported: the aim's node gives
  // none, and its +0xbc none either (hud.md section 12, Left).
  void* node_object(void*) override { return nullptr; }
  uint32_t node_bc(void*) override { return 0; }
  int32_t wm_mode(void* w) override { return wm(w)->mode; }
  int32_t wm_group_index(void* w) override { return wm(w)->group; }
  void* wm_group(void* w, int32_t i) override { return wm(w)->groups[i]; }
  int32_t group_count(void* g) override { return static_cast<int32_t>(static_cast<WeaponGroup*>(g)->count); }
  void* group_weapon(void* g, int32_t k) override { return static_cast<WeaponGroup*>(g)->weapons[k]; }
  void* wm_current(void* w) override { return wm(w)->current(); }  // 0x004b04c0
  int32_t weapon_group(void* w) override { return wp(w)->group; }
  const char* weapon_name(void* w) override {  // the definition's DESC (+8)
    return wp(w)->def && wp(w)->def->desc ? wp(w)->def->desc : "";
  }
  void weapon_vt8(void* w) override { wp(w)->is_beam(); }
  float weapon_recycle(void* w) override { return wp(w)->ready(); }        // vt+0x34
  float weapon_damage(void* w) override { return wp(w)->damage; }          // 0x004b3010
  bool weapon_enabled(void* w) override { return wp(w)->enabled; }         // +0x74
  int32_t weapon_ammo(void* w) override { return wp(w)->ammo(); }         // vt+0x14
  // The Direct3D pass (0x0043c6b0) on the zVideo ports; the dirty rects, the
  // message panel and the 3D views are not wired yet (hud.md section 11).
  void scene_begin() override { zvid::scene_begin(); }
  void scene_end() override { zvid::scene_end(); }
  void flat_quad(const float v[4][3], int16_t colour, uint8_t alpha) override {
    zvid::TLVertex* tl = zvid::draw_buffers().tl;
    for (int i = 0; i < 4; ++i) tl[i].sx = v[i][0], tl[i].sy = v[i][1], tl[i].sz = v[i][2];
    zvid::draw_flat(static_cast<uint32_t>(static_cast<int32_t>(colour)), alpha, 4, 0, 0);
  }
  void flush_transparent() override { zvid::flush_transparent(); }
  uint32_t material_soil(zwep::Handle m) override {
    return m ? game_soil(static_cast<zmod::Material*>(m)->soil) : 0;
  }
};
ProgramHudHooks g_hooks;
}  // namespace

void wire_hud() {
  set_hud_hooks(&g_hooks);
  update_calls().player_frame = [](Mech& m, float) { player_frame(m); };  // 0x0041ce60
  mech_hud_calls().is_view_target = [](const Mech& m) { return &m == program_view_target(); };
}

void unwire_hud() {
  set_hud_hooks(nullptr);
  mech_radar_blips_destroy();
  update_calls().player_frame = nullptr;
  mech_hud_calls() = MechHudCalls{};
}

void program_hud_update(float dt) {
  if (Mech* m = static_cast<Mech*>(program_view_target())) hud_handoff(*m, dt);
}

void program_hud_force_over(bool on) { g_hooks.force_over = on; }

// Mech3.exe 1.2 @ 0x0040f8d0's HUD part: the view target's HUD's data update
// (the message panel's slide before it is not ported).
void program_hud_data(float dt) {
  Mech* m = static_cast<Mech*>(program_view_target());
  if (Hud* h = m && m->pilot ? pilot_hud(*m->pilot) : nullptr) h->data_update(dt);
}

bool program_hud_report(HudReport* r) {
  Mech* m = static_cast<Mech*>(program_view_target());
  Hud* h = m && m->pilot ? pilot_hud(*m->pilot) : nullptr;
  auto* wm = m ? static_cast<WeaponManager*>(m->ammo) : nullptr;
  if (!h || !wm) return false;
  *r = HudReport{};
  for (WeaponGroup* g : wm->groups)
    if (g) r->weapons += static_cast<int>(g->count);
  for (HudWeaponLine& line : h->weapons.lines) {
    if (line.flags & ztwod::kHidden) continue;
    ++r->lines;
    auto* w = static_cast<Weapon*>(line.weapon);
    if (w && w->def && w->def->desc && std::strcmp(line.name.text, w->def->desc) == 0) ++r->named;
  }
  r->bracketed = !(h->weapons.poly.flags & ztwod::kHidden);
  r->lock = h->reticle.lock;
  r->lock_timer = h->reticle.lock_timer;
  r->target_hidden = (h->target.flags & ztwod::kHidden) != 0;
  r->shape_hidden = (h->target.shape1.flags & ztwod::kHidden) != 0;
  r->rx = h->reticle.get_x(), r->ry = h->reticle.get_y();
  if (h->target.shape1.xform) h->target.shape1.transform();
  r->apex_y = h->target.shape1.pts[0][1], r->base_y = h->target.shape1.pts[1][1];
  r->shape_x = h->target.shape1.translation[0];
  r->radar_shown = !(h->radar.flags & ztwod::kHidden);
  r->radar_r = h->radar.radius, r->radar_x = h->radar.get_x(), r->radar_y = h->radar.get_y();
  r->blips = g_hooks.blips, r->blip_x = g_hooks.blip_at[0], r->blip_y = g_hooks.blip_at[1];
  g_hooks.blips = 0;
  r->sensor_range = m->sensor_range;
  for (ai::Contact* c : m->contacts) {
    ++r->contacts;
    r->on_radar += c->flags & 1;
    r->vehicles += g_hooks.contact_vehicle(c) != nullptr;
  }
  return true;
}

// Mech3.exe 1.2 @ 0x0041d890's HUD part (the mech's vt+0xc0 after AiTarget's
// 0x004d5e40): the view's mech's TARGET turned on for its new target (its
// target sound 0x00432990 with 0x18 not ported here).
void program_hud_target_set(void* mech) {
  Mech* m = static_cast<Mech*>(mech);
  if (!m || m != program_view_target() || !m->pilot) return;
  if (Hud* h = pilot_hud(*m->pilot)) h->target.set_shown(true);
}

int program_hud_lock_on(void* mech, bool* target_set) {
  Mech* m = static_cast<Mech*>(program_view_target());
  auto* wm = m ? static_cast<WeaponManager*>(m->ammo) : nullptr;
  *target_set = false;
  if (!wm) return -1;
  int found = -1;
  for (int g = 0; g < 5 && found < 0; ++g)
    if (wm->groups[g] && wm->select_group(g) && group_locks_on(*wm)) found = g;
  for (ai::Contact* c : m->contacts)
    if (collide_calls().contact_mech && collide_calls().contact_mech(c) == mech) {
      m->target = c;
      *target_set = true;
    }
  if (*target_set) {
    // TARGET follows the target set (0x0041d890), as the targeting keys' path
    // would: shown, then turned on.
    if (Hud* h = m->pilot ? pilot_hud(*m->pilot) : nullptr) h->target.set_target(&m->target);
    program_hud_target_set(m);
  }
  return found;
}

// Mech3.exe 1.2 @ 0x0041cd90: the view target's HUD, its Direct3D pass.
void program_hud_draw3d(float dt) {
  Mech* m = static_cast<Mech*>(program_view_target());
  if (Hud* h = m && m->pilot ? pilot_hud(*m->pilot) : nullptr) h->draw3d(dt);
}

bool program_hud_aim(int32_t reticle[2], float point[3], bool* hit) {
  Mech* m = static_cast<Mech*>(program_view_target());
  Hud* h = m && m->pilot ? pilot_hud(*m->pilot) : nullptr;
  if (!h) return false;
  reticle[0] = h->reticle.get_x();
  reticle[1] = h->reticle.get_y();
  for (int i = 0; i < 3; ++i) point[i] = h->reticle.aim_point[i];
  *hit = h->reticle.hit.node != nullptr;
  return true;
}

bool program_hud_look(float x, float y) {
  Mech* m = static_cast<Mech*>(program_view_target());
  auto* c = m ? dynamic_cast<PlayerControls*>(m->controls) : nullptr;
  if (!c) return false;
  c->look_x = x;
  c->look_y = y;
  return true;
}

}  // namespace mech3
