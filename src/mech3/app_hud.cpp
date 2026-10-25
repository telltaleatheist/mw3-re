#include "mech3/app_hud.h"
#include "mech3/app_sensors.h"
#include "mech3/app_turret.h"
#include "mech3/app_vehicle.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <map>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/window.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zmodel/gmod_init.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/ztwod/twod.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/zvideo/zvid_dd.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zutil/zutl_msg.h"
#include "gamez/zutil/zutl_msgdll.h"
#include "mech3/landmines.h"
#include "mech3/mech_config.h"
#include "mech3/app_mech.h"
#include "mech3/armor_display.h"
#include "mech3/cockpit_sound.h"
#include "mech3/app_view.h"
#include "mech3/hud.h"
#include "mech3/mech_pocket.h"
#include "mech3/pocket_panel.h"
#include "mech3/mech.h"
#include "mech3/mech_command.h"
#include "mech3/mech_control.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_hud.h"
#include "mech3/mech_names.h"
#include "mech3/mech_data.h"
#include "mech3/message_panel.h"
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

int32_t display_w();  // options.h (0x0040d760), not includable with mech.h
const char* program_pilot_name();  // app_menus.cpp: the current pilot's (+0x18)


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
  // The HUD's 3D views' nodes (hud-plan item 6c1): the cameras and windows it
  // makes, by handle (the hooks' 32-bit node handles; a pointer does not fit).
  std::vector<zcls::Node*> nodes;
  uint32_t keep(zcls::Node* n) {
    if (!n) return 0;
    for (size_t i = 0; i < nodes.size(); ++i)
      if (nodes[i] == n) return 0x7e000001u + static_cast<uint32_t>(i);  // asked again (the zoom, each frame)
    nodes.push_back(n);
    return 0x7e000000u + static_cast<uint32_t>(nodes.size());
  }
  zcls::Node* node(uint32_t h) const {
    const uint32_t i = h - 0x7e000001u;
    return i < nodes.size() ? nodes[i] : nullptr;
  }
  static constexpr uint32_t kWorld = 0x7dffffffu;  // the mission's world, the player camera's
  uint32_t world() override { return kWorld; }
  uint32_t world_node(uint32_t w) override {
    zcls::Node* c = camera_node();
    return w == kWorld && c ? keep(zcls::camera_world(c)) : 0;
  }
  uint32_t window_node_new() override { ++windows_made; return keep(zcls::window_create()); }  // 0x004f9e90
  uint32_t camera_node_new() override { ++cameras_made; return keep(zcls::camera_create()); }  // 0x004f32e0
  int cameras_made = 0, windows_made = 0, view_draws = 0, view_depth_fills = 0;  // for the reports
  void node_set_name(uint32_t n, const char* name) override { zcls::node_set_name(node(n), name); }
  void camera_set_flag8(uint32_t c, bool on) override { zcls::camera_set_flag8(node(c), on); }
  void node_set_active(uint32_t n, bool on) override { zcls::node_set_active(node(n), on ? 1 : 0); }
  void window_set_size(uint32_t n, int32_t w, int32_t h) override { zcls::window_set_size(node(n), w, h); }
  void window_set_pos(uint32_t n, int32_t x, int32_t y) override { zcls::window_set_origin(node(n), x, y); }
  void camera_set_window(uint32_t c, uint32_t w) override { zcls::camera_set_window(node(c), node(w)); }
  void camera_set_world(uint32_t c, uint32_t w) override { zcls::camera_set_world(node(c), node(w)); }
  void camera_set_zone(uint32_t c, uint32_t zone) override {
    uint8_t z[4];
    std::memcpy(z, &zone, 4);
    zcls::camera_set_zone(node(c), z);
  }
  void camera_set_zoom(uint32_t c, float z) override { zcls::camera_set_lod_scale(node(c), z); }  // 0x004f3ff0
  void camera_set_fov(uint32_t c, float x, float y) override { zcls::camera_set_fov_angles(node(c), x, y); }
  void camera_set_clip(uint32_t c, float n, float f) override { zcls::camera_set_clip(node(c), n, f); }
  void camera_set_position(uint32_t c, const float p[3]) override {
    zcls::camera_set_position(node(c), p[0], p[1], p[2]);
  }
  void camera_set_angles(uint32_t c, const float a[3]) override {
    zcls::camera_set_rotation(node(c), a[0], a[1], a[2]);
  }
  // The zoom's 3D pass (0x00442f50, 0x004436c0, hud.md section 23).
  uint32_t object_node(void* o) override { return keep(static_cast<zcls::Node*>(static_cast<Mech*>(o)->node)); }
  void player_camera_place(float out[3]) override {
    const zcls::Camera* c = camera_data();
    for (int i = 0; i < 3; ++i) out[i] = c ? c->get<float>(0x14 + 4 * i) : 0.0f;
  }
  uint32_t player_camera_zone() override {
    const zcls::Camera* c = camera_data();
    return c ? c->get<uint32_t>(0x1e4) : 0;
  }
  void camera_window_rect(uint32_t cam, int32_t* x, int32_t* y, int32_t* w, int32_t* h) override {
    *x = *y = 0, *w = *h = 1;
    zcls::Node* c = node(cam);
    zcls::Node* win = c && c->data ? static_cast<zcls::Camera*>(c->data)->nodes[1] : nullptr;  // 0x004f3630
    if (!win) return;
    zcls::window_origin(win, x, y);
    zcls::window_size(win, w, h);
  }
  void world_fog_range(uint32_t w, float* n, float* f) override {
    *n = *f = 0.0f;
    if (zcls::Node* wn = node(w)) zcls::world_get_fog_range(wn, n, f);
  }
  void world_set_fog_range(uint32_t w, float n, float f) override {
    if (zcls::Node* wn = node(w)) zcls::world_set_fog_range(wn, n, f);
  }
  void world_push_fog(uint32_t w) override {
    if (zcls::Node* wn = node(w)) zcls::world_push_fog(wn);
  }
  void mask_fan(const float* xyz, int32_t count) override { ++fans, zvid::clear_fan(xyz, count); }
  int fans = 0;  // for the reports
  // 0x00442870 -> the zView's vt+0x04 (0x004b52c0): its camera's scene walk
  // (0x004f7030); 0x004f6dc0 without a device is the software path (not ported).
  void zview_render(void* view, float) override {
    const HudZView& v = static_cast<HudZoomView*>(view)->view;
    if (!v.hardware) return;
    if (zcls::Node* c = node(v.camera)) ++view_draws, zcls::render_world(c, 0);
  }
  void dirty_rect(const ztwod::Rect* r) override {  // 0x00576ce0: the depth there cleared
    if (!r) return;
    const int32_t q[4] = {r->left, r->top, r->right, r->bottom};
    ++view_depth_fills;
    zvid::depth_fill(q);
  }
  // The HUD's channels are the cockpit sounds (0x0041e770, 0x0041e7a0, 0x0041e800).
  void hud_sound_play(int32_t channel) override { cockpit_sound_play(channel, 1.0f); }
  void hud_sound_stop(int32_t channel, int32_t fade) override { cockpit_sound_stop(channel, fade); }
  bool hud_sound_playing(int32_t channel) override { return cockpit_sound_voice(channel) != nullptr; }
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
  // 14) on the player's mech and its target: a contact's entity, vehicle and
  // seen object of any class (app_sensors.cpp, entity_classes.h); a mech's
  // place, centre and velocity its own, the others' by their class.
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
  void* contact_entity(void* c) override { return hud_contact_entity(c); }    // +8
  void* contact_vehicle(void* c) override { return hud_contact_vehicle(c); }  // 0x004e6fe0
  void* object_seen(void* c) override { return hud_contact_vehicle(c); }      // +4
  void* target_object(void* slot) override {
    ai::Contact* c = slot ? *static_cast<ai::Contact**>(slot) : nullptr;
    return c ? hud_contact_vehicle(c) : nullptr;
  }
  bool object_a8(void* o) override { return hud_object_hidden(o); }           // vt+0xa8
  bool object_mech(void* o) override { return hud_object_mech(o) != nullptr; }  // vt+0x50
  void* object_mech_ptr(void* o) override { return hud_object_mech(o); }
  const char* object_name(void* o) override { return hud_object_name(o); }   // vt+0xa4
  float contact_size(void* c) override { return hud_contact_size(c); }        // 0x004e7070
  void entity_position(void* e, float out[3]) override {  // the centroid node's (+0x14)
    if (!e) out[0] = out[1] = out[2] = 0.0f;
    else if (entity_is_mech(e)) mech_pos(mech_of(e), out);
    else hud_object_centroid(e, out);
  }
  void vehicle_center(void* v, float out[3]) override {  // vt+0xbc
    if (v && entity_is_mech(v)) mech_pos(mech_of(v), out);
    else if (v) hud_object_centre(v, out);
    else out[0] = out[1] = out[2] = 0.0f;
  }
  uint32_t object_team(void* o) override { return o ? hud_object_team(o) : 0; }  // vt+0x28 +0xc0
  // 0x0043da50's click (0x00432990 on the source HUD's mech): radar_up (0x10)
  // into +0x5f8 growing, radar_down (0x11) into +0x5fc shrinking.
  void radar_zoom_sound(void* source, bool zoomed) override {
    Mech* m = static_cast<Hud*>(source)->mech;
    if (m && heat_calls().play_sound) m->sounds[zoomed ? 16 : 17] = heat_calls().play_sound(*m, zoomed ? 0x10 : 0x11);
  }
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
  // The chosen part (+0xf8) and its point (+0xec, 0x0041d6f0; playtest #15).
  bool mech_aim_point(Mech* m, float* out) override {
    if (!m || m->xf8 == -1) return false;
    out[0] = m->part_aim.x, out[1] = m->part_aim.y, out[2] = m->part_aim.z;
    return true;
  }
  void mech_position(Mech* m, float out[3]) override { mech_pos(m, out); }
  void vehicle_position(void* v, float out[3]) override {  // 0x005a8630
    if (v && !entity_is_mech(v)) return hud_object_aim_place(v, out);
    entity_position(v, out);
  }
  void mech_velocity(Mech* m, float out[3]) override { out[0] = m->velocity.x, out[1] = m->velocity.y, out[2] = m->velocity.z; }
  void vehicle_velocity(void* v, float out[3]) override {  // vt+0xc
    if (v && !entity_is_mech(v)) return hud_object_velocity(v, out);
    mech_velocity(mech_of(v), out);
  }
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
      case 0x574: return m->masc_time;
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
  // mode), nor the nav route's polylines (no points). The
  // contacts' objects are of every class, each with its own blips (vt+0x3c).
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
  void object_view_flag(void* o, int32_t on) override { if (o) hud_object_view_tint(o, on != 0); }  // vt+0x68
  void object_position(void* o, float out[3]) override {  // 0x005a85e0
    if (o && !entity_is_mech(o)) return hud_object_place(o, out);
    entity_position(o, out);
  }
  void* object_blip(void* o, int32_t kind) override { return o ? hud_object_blip(o, kind) : nullptr; }  // vt+0x3c
  // For program_hud_report: the draws by the blip's class (its image).
  static int blip_class(void* b) {
    for (int32_t k = 0; k < 3; ++k) {
      if (b == mech_radar_blip(k)) return 0;
      if (b == program_vehicle_marker(k)) return 1;
      if (b == program_turret_marker(k)) return 2;
      if (b == hud_target_marker(k)) return 3;
    }
    return program_is_waypoint_icon(b) ? 4 : 0;
  }
  void blip_place(void* b, int32_t x, int32_t y) override {
    static_cast<ztwod::Image*>(b)->set_pos(x, y);
    if (blip_class(b) == 0) blip_at[0] = x, blip_at[1] = y;
  }
  void blip_draw(void* b) override {
    static_cast<ztwod::Image*>(b)->draw();
    ++drawn[blip_class(b)];
  }
  int32_t drawn[5] = {}, blip_at[2] = {};  // mechs, vehicles, turrets, targets, the waypoint
  // The radar's waypoint (the mission waynet's selected point; vehicles.md,
  // The nav waynet), its icon, place and name.
  void* network_marker() override { return program_waypoint_marker(); }
  void* marker_image(void* e) override { return program_waypoint_icon(e); }
  void marker_position(void* e, float out[3]) override { program_waypoint_position(e, out); }
  const char* marker_name(void* e) override { return program_waypoint_name(e); }
  // The edge arrow toward it (0x00479ef0 in the data update, 0x00479ed0 drawn).
  void network_data(int32_t active, float) override { program_waypoint_arrow(active); }
  void network_hud() override { program_waypoint_arrow_draw(); }
  bool radar_shot(int32_t i, float out[3], bool* hostile) override {
    auto& l = radar_blips();
    auto it = l.begin();
    for (int32_t k = 0; k < i && it != l.end(); ++k) ++it;
    if (it == l.end()) return false;
    const zwep::Vec3f3& p = (*it)->shot->position;
    out[0] = p.x, out[1] = p.y, out[2] = p.z;
    *hostile = (*it)->hostile;
    return true;
  }
  int32_t mine_count() override { return static_cast<int32_t>(landmines().list.size()); }
  bool mine(int32_t i, float out[3]) override {
    const Landmine& m = landmines().list[static_cast<size_t>(i)];
    out[0] = m.at[0], out[1] = m.at[1], out[2] = m.at[2];
    return m.armed != 0;
  }
  bool mech_ams(Mech* m, bool* on, int32_t* count) override {
    if (!m->ams) return false;
    *on = m->ams->on;
    *count = m->ams->rounds;
    return true;
  }
  const char* equipment_name(int32_t kind) override {  // 0x00484980
    return kind >= 0 && kind < 15 ? message(0x1c6 + kind) : "none";
  }
  // The static (hud.md section 21): the mission's player, GameIntensity's
  // flag, the 3D lock's back buffer (the 2D layer the HUD draws on; the
  // software path's surface without a device) and the ported fill.
  void* mission_player(void*) override { return mech3::world().mission_player; }
  bool intensity_flag() override { return program_intensity_flag(); }
  void static_target() override {
    const zvid::Surface& s = zrndr::device_fog().active ? zvid::surfaces().back : zvid::surfaces().copy;
    zrndr::set_finish_target(s.pixels, s.width, s.height, s.pitch);
  }
  void static_fill(const ztwod::Rect* r, double level, uint16_t lo, uint16_t mid, uint16_t hi) override {
    ++fills;
    zrndr::static_fill(r ? &r->left : nullptr, level, lo, mid, hi);
  }
  int fills = 0;  // for the reports
  // The damage displays and TARGET's text (hud.md section 16) on the ported
  // parts, configurations and Mech3Msg.dll's messages.
  float part_internal(void* mech, uint32_t part, int32_t section) override {
    Mech* m = mech_of(mech);
    const MechPart* p = part == 0x38c ? static_cast<MechPart*>(m->legs[0])
                        : part == 0x390 ? static_cast<MechPart*>(m->legs[1])
                        : part == 0x394 ? static_cast<MechPart*>(m->arms[0])
                        : part == 0x398 ? static_cast<MechPart*>(m->arms[1])
                        : part == 0x39c ? static_cast<MechPart*>(m->torso)
                                        : static_cast<MechPart*>(m->head);
    return p ? p->get_internal(section) : 1.0f;
  }
  const char* message(int32_t id) override {  // 0x00576040
    const zutl::Messages& ms = zutl::messages();
    const char* t = ms.get_text ? ms.get_text(id) : nullptr;
    return t ? t : "";
  }
  bool mech_remote(void* m) override { return mech_of(m)->remote; }
  // vt+0xa4 (0x0041df30): the player's pilot's name (program_pilot_name,
  // app_menus.cpp), else the brain's (0x004aee70); "" ([0x005f4c90]) when none.
  const char* mech_object_name(void* m) override {
    const char* n = m == mech_mission_player() ? program_pilot_name() : mech_brain_name(m);
    return n ? n : "";
  }
  const char* mech_config_name(void* m) override {
    const Config* c = mech_of(m)->config;
    return c ? c->name : nullptr;
  }
  const char* mech_display_name(int32_t kind) override {  // 0x00484670
    return kind >= 0 && kind < 19 ? message(0xd82 + kind) : "notmech";
  }
  int32_t mech_aim_part(Mech* m) override { return m->xf8; }
  bool part_by_id(Mech* m, int32_t part) override {  // 0x0041d510
    return weapon_calls().part_by_id && weapon_calls().part_by_id(m, part) != nullptr;
  }
  const char* part_display_name(int32_t part) override {  // 0x00484d70
    return mech3::part_display_name(
        part, [](void* c, int32_t id) { return static_cast<decltype(this)>(c)->message(id); }, this);
  }
  void* saved_or_ally(Mech* m) override { return m->saved_or_ally(); }
  const char* message_arg(int32_t msg, const char* arg) override {  // 0x00575fd0
    formatted = zutl::format_message(msg, arg);
    return formatted.c_str();
  }
  std::string formatted;
  // The MFD's and TARGET's armor display (mode 3): the ported LanceDisplay
  // (debriefing.cpp, 0x0047e840..0x0047f140), by a handle of ours.
  std::map<uint32_t, std::unique_ptr<LanceDisplay>> navs;
  uint32_t next_nav = 0x4e000000;
  LanceDisplay* nav_of(uint32_t h) {
    auto f = navs.find(h);
    return f == navs.end() ? nullptr : f->second.get();
  }
  uint32_t nav_list_new(int32_t x, int32_t y, int32_t h, int32_t w, int32_t sp) override {
    const uint32_t id = next_nav += 0x10;
    navs[id] = std::make_unique<LanceDisplay>([this](int32_t m) { return message(m); }, x, y, h, w, sp);
    return id;
  }
  void nav_list_place(uint32_t nav, int32_t x, int32_t y, int32_t h, int32_t w, int32_t sp) override {
    if (LanceDisplay* d = nav_of(nav)) d->place(x, y, h, w, sp);  // 0x0047eb60
  }
  static bool armor_of(void* m, LanceArmor* a) {
    return m && mech_lance_armor(m, &a->type, a->armor, a->left);
  }
  void nav_list_attach(uint32_t nav, Mech* m) override {  // 0x0047eba0
    LanceArmor a;
    if (LanceDisplay* d = nav_of(nav); d && armor_of(m, &a)) d->layout(a);
  }
  void nav_list_target(uint32_t nav, void* m) override {  // 0x0047ef10
    LanceArmor a;
    if (LanceDisplay* d = nav_of(nav); d && armor_of(m, &a)) d->fill(a);
  }
  void nav_list_update(uint32_t nav, float dt) override {
    if (LanceDisplay* d = nav_of(nav)) d->update(dt);
  }
  const ztwod::Rect* nav_rect(uint32_t nav) override {  // +0x1c84
    static ztwod::Rect r;
    const LanceDisplay* d = nav_of(nav);
    r = d ? ztwod::Rect{d->left, d->top, d->right, d->bottom} : ztwod::Rect{};
    return &r;
  }
  // The aimed node's game object (0x0040e920, app_weapons.cpp's node_entity)
  // and its +8 byte, ZEntity's active flag: a mech's; any other object's is
  // not ported (logged, read as 0). The node's +0xbc is its damage
  // callbacks' record (zwep_damage, a typed field here): "above 1" is "has one".
  void* node_object(void* n) override { return node_entity(n); }
  bool object_byte8(void* o) override {
    if (entity_is_mech(o)) return mech_of(o)->active;
    unported("object+8", {id(o)});
    return false;
  }
  uint32_t node_bc(void* n) override { return n && static_cast<zcls::Node*>(n)->damage ? 2u : 0u; }
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

void program_hud_zoom_show() {
  Mech* t = static_cast<Mech*>(program_view_target());
  if (Hud* h = t && t->pilot ? pilot_hud(*t->pilot) : nullptr) h->reticle.zoom.toggle_lock();  // keypad 0
}
void program_hud_mfd_mode(int m) {
  Mech* t = static_cast<Mech*>(program_view_target());
  if (Hud* h = t && t->pilot ? pilot_hud(*t->pilot) : nullptr) h->mfd.set_mode(m);
}
void program_set_torso_internal(void* mech, float v) {
  if (Torso* t = mech ? static_cast<Mech*>(mech)->torso : nullptr) t->internal = t->side_internal[0] = t->side_internal[1] = v;
}
void* program_pilot_hud(void* data) { return data ? pilot_hud(*static_cast<MechData*>(data)) : nullptr; }
void program_hud_zoom_pass(void* hud, float dt) {
  if (hud) static_cast<Hud*>(hud)->reticle.draw3d(dt);
}
void program_hud_static(void* hud, float dt) {
  if (hud) static_cast<Hud*>(hud)->static_tick(dt);
}

namespace {
// 0x00430aa0 on the pilot (+0x684): its HUD, made when first asked.
Hud* hud_of(Mech& m) { return m.pilot ? pilot_hud(*m.pilot) : nullptr; }
Hud* hud_of(Handle owner) { return owner ? hud_of(*static_cast<Mech*>(owner)) : nullptr; }

// Mech3.exe 1.2 @ 0x0041be80's HUD cases (Mech::command's hud hook, on the
// view's HUD; docs/keybindings.md).
void hud_command(void* s, int32_t cmd) {
  auto* h = static_cast<Hud*>(s);
  // FIX (bug): keypad 0 with no HUD (the view's mech without one) ran
  // 0x00442e00 on address 0x1508, a crash; nothing here.
  if (!h) return;
  switch (cmd) {
    case 0x15: h->reticle.zoom.toggle_lock(); break;  // keypad 0
    case 0x43:  // F12: the objectives' vt+0x60(-1) when hidden, else vt+0x64
      if (h->objectives.flags & ztwod::kHidden) h->objectives.show_for(-1.0f);
      else h->objectives.hide();
      break;
    case 0x49: program_hud_target_set(h->mech); break;  // the target set (0x0041d890's HUD part)
    case 0x68: h->radar.toggle_zoom(); break;         // R
    case 0x6b: h->set_active(h->active == 0); break;  // H: vt+4
    case 0x74: h->mfd.cycle_mode(); break;            // D (0x0043d240)
    case 0x75: h->target.cycle_mode(); break;         // Ctrl+D (0x0043d250)
    case 0xa5: h->radar.toggle_map(); break;          // Shift+R
    default: break;
  }
}

// Mech3.exe 1.2 @ 0x0041ded0 (the mech's vt+0xb8, after Alt+R): the view's
// HUD's radar amber while the radar is passive (+0xbc bit 2).
void radar_light(Mech& m) {
  if (&m != program_view_target()) return;
  if (Hud* h = hud_of(m)) h->radar.set_amber((m.xbc >> 2 & 1) != 0);
}
}  // namespace

void wire_hud() {
  set_hud_hooks(&g_hooks);
  // The mech's, the weapons' and the damage's calls on the HUD (playtest #6:
  // with no screen, Enter (0x27) and every HUD key found none): the screen,
  // the weapons list's layout 0x00446240, refresh 0x00446520, the fire mark
  // 0x004464c0 (+0x11438 = 1) and the weapon it highlights (+0x1795c).
  heat_calls().screen = [](Mech& m) -> void* { return hud_of(m); };
  command_calls().hud_weapon = [](void* s) -> void* { return static_cast<Hud*>(s)->weapons.next_weapon; };
  // The HUD's keys (playtest #13, #20): the commands', and the zoom key held
  // (the controls' update: the zoom window +0x1508, its flags +0x1514 and
  // its lock +0x167c), its show and hide every frame.
  command_calls().hud = hud_command;
  command_calls().hud_aimed_node = [](void* s) -> void* { return static_cast<Hud*>(s)->reticle.hit.node; };
  controls_calls().screen_busy = [](void* s) { return static_cast<Hud*>(s)->reticle.zoom.x174; };
  controls_calls().zoom = [](void* s, bool in) {
    HudZoomWindow& z = static_cast<Hud*>(s)->reticle.zoom;
    if (in) z.show_for(-1.0f);
    else z.hide();
  };
  part_calls().screen_flags = [](void* s) { return static_cast<Hud*>(s)->reticle.zoom.flags; };
  update_calls().hud_weapons = [](void* s) { static_cast<Hud*>(s)->weapons.layout(); };
  damage_calls().hud_weapons = [](void* s) { static_cast<Hud*>(s)->weapons.refresh_all(); };
  // A hit on the player's head (playtest #19): the cockpit's explosion run on
  // the cockpit, a crack or a ding shown (0x00460bff, docs/cockpit-damage.md).
  damage_calls().head_hit_hud = [](Handle explosion, Handle cockpit) {
    zeff::anim_start_connector(static_cast<zeff::AnimDef*>(explosion), static_cast<zcls::Node*>(cockpit), nullptr,
                               nullptr, nullptr);
  };
  WeaponCalls& wc = weapon_calls();
  wc.screen = [](Handle o) { return hud_of(o) != nullptr; };
  wc.hud_screen = [](Handle o) -> Handle { return hud_of(o); };
  wc.hud_groups = [](Handle o) { if (Hud* h = hud_of(o)) h->weapons.layout(); };
  wc.hud_weapons = [](Handle o) { if (Hud* h = hud_of(o)) h->weapons.refresh_all(); };
  wc.hud_fire = [](Handle o) { if (Hud* h = hud_of(o)) h->weapons.shown = 1; };
  wc.hud_highlight = [](Handle o) -> Weapon* {
    Hud* h = hud_of(o);
    return h ? static_cast<Weapon*>(h->weapons.next_weapon) : nullptr;
  };
  update_calls().player_frame = [](Mech& m, float) { player_frame(m); };  // 0x0041ce60
  mech_hud_calls().is_view_target = [](const Mech& m) { return &m == program_view_target(); };
  // The pocket commander's (mech_pocket.cpp, docs/ai.md 17.6a): the camera,
  // the viewed mech's HUD (its reticle's flag and image, the weapons' and
  // equipment's hold), the HUD for the mode's zoom key; the view's pocket
  // 0x004b5290 is not ported.
  PocketCalls& pk = pocket_calls();
  pk.camera_mode = [] { return program_view_camera_id(); };
  pk.set_camera_mode = [](int32_t id) { program_view_set_camera(id); };
  pk.display_width = [] { return display_w(); };
  pk.view_screen = [](Mech& m) -> void* { return &m == program_view_target() ? hud_of(m) : nullptr; };  // 0x0046b0a0's zoom box
  // The panel's draws (0x00467760, 0x00468900): places, sizes, the screen and
  // the projection as the HUD has them; the map's pixels and the node mark
  // are not wired (the panel's cursor picks are not either).
  DrawCalls& dc = draw_calls();
  dc.entity_position = [](zgame::Handle e, float out[3]) { ai::brain_hooks().position(e, out); };  // 0x005a85e0
  dc.entity_size = [](zgame::Handle e) {
    const ai::AiTarget* t = ai::brain_hooks().targetable(e);
    return t ? t->height : 0.0f;  // +0x68
  };
  dc.project = [](const float in[3], float out[3]) { return hud_hooks().project(in, out); };
  dc.screen_rect = [](ztwod::Rect* r) { *r = hud_hooks().window_rect(); };
  dc.view_prepare = [] { hud_hooks().view_prepare(); };
  pk.hud_mode = [](Mech& m, bool on) {
    if (&m != program_view_target()) return;
    if (Hud* h = hud_of(m)) {
      h->reticle.locked = on;  // +0x144c +0x90c
      if (on) h->reticle.set_image(h->reticle.img_x920);  // 0x0059bb50
    }
  };
  pk.hud_hold = [](Mech& m, bool hold) {  // 0x004484b0
    if (&m != program_view_target()) return;
    if (Hud* h = hud_of(m)) {
      if (hold) h->weapons.show_for(-1.0f), h->equipment.show_for(-1.0f);
      else h->weapons.hide(), h->equipment.hide();
    }
  };
  mech_hud_calls().cockpit = make_cockpit;  // 0x00430ad0's second half (playtest #4)
  command_calls().stations = [] { return program_mission_waynet(); };  // [0x00609980]
  command_calls().other = [](Mech& m, int32_t cmd) {
    if (cmd >= 0x36 && cmd <= 0x38 && program_mission_waynet_key(cmd, &m)) return;  // vt+0x60 / +0x64 / +0x68
    if (cmd == 0x73) return program_mcb_command(&m);  // MCB_COMMAND 0x0047a910 on [0x0060b544]
    if (cmd == 0xa6) {
      if (message_panel()) message_panel_toggle();  // 0x0040f070, the message panel's key
      return;
    }
    if (cmd == 0x69) {
      radar_light(m);
      return;
    }
    // The rest stay unported, recorded as the handler would (docs/keybindings.md).
    const char* key = cmd >= 0x36 && cmd <= 0x38 ? "[0x00609980] vtable +0x60..+0x68"
                      : cmd == 0x3e              ? "0x0041d070"
                      : cmd == 0x69              ? "vtable +0xb8"
                      : cmd == 0x73              ? "0x0047a910"
                      : cmd == 0x76 || cmd == 0x77 ? "0x0041e4e0"
                                                 : "mech command (other)";
    if (command_calls().unported) command_calls().unported(key, cmd);
  };
}

void unwire_hud() {
  set_hud_hooks(nullptr);
  heat_calls().screen = nullptr;
  command_calls().hud_weapon = nullptr;
  command_calls().hud = nullptr, command_calls().hud_aimed_node = nullptr;
  controls_calls().screen_busy = nullptr, controls_calls().zoom = nullptr;
  part_calls().screen_flags = nullptr;
  update_calls().hud_weapons = nullptr;
  damage_calls().hud_weapons = nullptr;
  damage_calls().head_hit_hud = nullptr;
  WeaponCalls& wc = weapon_calls();
  wc.screen = nullptr, wc.hud_screen = nullptr, wc.hud_groups = nullptr, wc.hud_weapons = nullptr;
  wc.hud_fire = nullptr, wc.hud_highlight = nullptr;
  mech_radar_blips_destroy();
  update_calls().player_frame = nullptr;
  mech_hud_calls() = MechHudCalls{};
}

void* program_hud_node_object(void* node) { return hud_hooks().node_object(node); }
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

namespace {
int g_count_id = 0;
int* g_count = nullptr;
Sound (*g_count_play)(Mech&, int) = nullptr;
void* (*g_count_part)(Mech&, int, float) = nullptr;
}  // namespace

void program_count_sounds(int id, int* count) {
  if (!count) {
    if (g_count) heat_calls().play_sound = g_count_play, part_calls().sound = g_count_part;
    g_count = nullptr;
    return;
  }
  g_count_id = id, g_count = count;
  g_count_play = heat_calls().play_sound, g_count_part = part_calls().sound;
  heat_calls().play_sound = [](Mech& m, int i) -> Sound {
    *g_count += i == g_count_id;
    return g_count_play ? g_count_play(m, i) : nullptr;
  };
  part_calls().sound = [](Mech& m, int i, float v) -> void* {
    *g_count += i == g_count_id;
    return g_count_part ? g_count_part(m, i, v) : nullptr;
  };
}

bool program_hud_report(HudReport* r) {
  Mech* m = static_cast<Mech*>(program_view_target());
  Hud* h = m && m->pilot ? pilot_hud(*m->pilot) : nullptr;
  auto* wm = m ? static_cast<WeaponManager*>(m->ammo) : nullptr;
  if (!h || !wm) return false;
  *r = HudReport{};
  for (int i = 0; i < 4; ++i) r->statics[i] = h->statics[i].level;
  r->static_fills = g_hooks.fills;
  g_hooks.fills = 0;
  r->view_cameras = g_hooks.cameras_made, r->view_windows = g_hooks.windows_made;
  r->views_drawn = g_hooks.view_draws, r->depth_fills = g_hooks.view_depth_fills;
  g_hooks.view_draws = g_hooks.view_depth_fills = 0;
  r->mfd_shown = h->mfd.shown_mode();
  HudZoomWindow& z = h->reticle.zoom;
  r->zoom_shown_now = !(z.flags & ztwod::kHidden), r->zoom_round = z.round, r->mask_fans = g_hooks.fans;
  g_hooks.fans = 0;
  if (zcls::Node* c = g_hooks.node(z.view.camera); c && c->data && camera_data()) {
    float d2 = 0.0f;
    for (int i = 0; i < 3; ++i) {
      const float k = static_cast<zcls::Camera*>(c->data)->get<float>(0x14 + 4 * i) - camera_data()->get<float>(0x14 + 4 * i);
      d2 += k * k;
    }
    r->zoom_cam_off = std::sqrt(d2);
  }
  std::snprintf(r->zoom_text, sizeof r->zoom_text, "%s", z.text.text);
  for (WeaponGroup* g : wm->groups)
    if (g) r->weapons += static_cast<int>(g->count);
  for (HudWeaponLine& line : h->weapons.lines) {
    if (line.flags & ztwod::kHidden) continue;
    ++r->lines;
    auto* w = static_cast<Weapon*>(line.weapon);
    if (w && w->def && w->def->desc && std::strcmp(line.name.text, w->def->desc) == 0) ++r->named;
  }
  if (const LanceDisplay* d = g_hooks.nav_of(h->mfd.nav)) {
    r->mfd_armor_most = d->most;
    r->mfd_armor_w = d->right - d->left;
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
  r->blips = g_hooks.drawn[0], r->blip_x = g_hooks.blip_at[0], r->blip_y = g_hooks.blip_at[1];
  r->vehicle_blips = g_hooks.drawn[1], r->turret_blips = g_hooks.drawn[2], r->target_blips = g_hooks.drawn[3];
  r->markers = g_hooks.drawn[4];
  for (int32_t& n : g_hooks.drawn) n = 0;
  r->sensor_range = m->sensor_range;
  for (const HudDamagePart& p : h->target.damage.parts) r->target_damaged += p.image != p.images->image[0];
  for (const HudDamagePart& p : h->mfd.damage.parts) r->mfd_damaged += p.image != p.images->image[0];
  r->target_mode = h->target.shown_mode();
  r->zoom_shown = !(h->reticle.zoom.flags & ztwod::kHidden);
  r->zoom_locked = h->reticle.zoom.x174;
  r->radar_large = h->radar.zoomed, r->radar_map = h->radar.active, r->radar_amber = h->radar.x58;
  r->mfd_mode = h->mfd.mode, r->target_mode_set = h->target.mode;
  r->objectives_shown = !(h->objectives.flags & ztwod::kHidden);
  r->hud_on = h->active;
  for (HudWeaponLine& line : h->weapons.lines) {
    if (line.flags & ztwod::kHidden) continue;
    r->bar_min = std::min(r->bar_min, line.value);
    r->bars_partial += line.value < 1.0f;
  }
  for (WeaponGroup* g : wm->groups)
    for (uint32_t i = 0; g && i < g->count; ++i)
      if (auto* w = static_cast<Weapon*>(g->weapons[i])) r->recycle_min = std::min(r->recycle_min, w->ready());
  for (const HudEquipLine& l : h->equipment.lines) r->equip_lines += !(l.flags & ztwod::kHidden);
  if (h->equipment.flags & ztwod::kHidden) r->equip_lines = 0;
  for (int32_t k : {0, 12, 3, 8, 14, 6, 7, 4}) r->equip_wanted += std::max(0, m->equipment[k]);
  r->equip_wanted = std::min(r->equip_wanted, 16);
  r->speed_kmh = h->gauges.speed;
  r->throttle_up = static_cast<int>(h->gauges.speed_bar.pts[1][1] - h->gauges.speed_bar.pts[0][1]);
  r->fuel_shown = !(h->gauges.fuel_bar.flags & ztwod::kHidden);
  r->heat_unlit = h->gauges.heat_src.top;
  r->target_internal = 2.0f;
  if (Mech* t = static_cast<Mech*>(h->target.damage.mech))
    for (uint32_t part : {0x38cu, 0x390u, 0x394u, 0x398u, 0x3a4u})
      r->target_internal = std::min(r->target_internal, g_hooks.part_internal(t, part, 0));
  if (Mech* t = static_cast<Mech*>(h->target.damage.mech))
    for (int sec = 0; sec < 3; ++sec) r->target_internal = std::min(r->target_internal, g_hooks.part_internal(t, 0x39c, sec));
  for (ai::Contact* c : m->contacts) {
    ++r->contacts;
    r->on_radar += c->flags & 1;
    r->vehicles += g_hooks.contact_vehicle(c) != nullptr;
  }
  return true;
}

// Mech3.exe 1.2 @ 0x0041d890's HUD part (the mech's vt+0xc0 after AiTarget's
// 0x004d5e40): the view's mech's TARGET turned on for its new target, and
// the target sound (0x18) played on the new target's mech, kept at its +0x618.
void program_hud_target_set(void* mech) {
  Mech* m = static_cast<Mech*>(mech);
  if (!m || m != program_view_target() || !m->pilot) return;
  Hud* h = pilot_hud(*m->pilot);
  if (!h) return;
  h->target.set_shown(true);
  Mech* tm = m->target && collide_calls().contact_mech ? collide_calls().contact_mech(m->target) : nullptr;
  if (tm && heat_calls().play_sound) tm->sounds[24] = heat_calls().play_sound(*tm, 0x18);
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

bool program_hud_target(void* mech, int mode) {
  Mech* m = static_cast<Mech*>(program_view_target());
  Hud* h = m && m->pilot ? pilot_hud(*m->pilot) : nullptr;
  if (!h) return false;
  for (ai::Contact* c : m->contacts)
    if (collide_calls().contact_mech && collide_calls().contact_mech(c) == mech) {
      m->target = c;
      h->target.set_target(&m->target);
      program_hud_target_set(m);
      h->target.set_mode(mode);
      return true;
    }
  return false;
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
