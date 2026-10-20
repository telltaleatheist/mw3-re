#include "mech3/app_hud.h"

#include <cstdio>
#include <cstring>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_init.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zvideo/zvid_dd.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "mech3/app_view.h"
#include "mech3/hud.h"
#include "mech3/mech.h"
#include "mech3/mech_control.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_hud.h"
#include "mech3/mech_move.h"
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
  bool reticle_over_target(void*, int32_t, int32_t) override { return force_over; }
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
  update_calls().player_frame = nullptr;
  mech_hud_calls() = MechHudCalls{};
}

void program_hud_update(float dt) {
  if (Mech* m = static_cast<Mech*>(program_view_target())) hud_handoff(*m, dt);
}

void program_hud_force_over(bool on) { g_hooks.force_over = on; }

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
  return true;
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
