// The mech build's and the launch's hooks wired to the ports (docs/mech-build.md
// section 9). Not original code: it stands where the exe's other modules
// will be. zClass's nodes, the mech data (0x00430d10) and mechlib are the
// ports; what has no port is logged once by address through the engine.
#include "mech3/app_mech.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "gamez/zclass/cls_mechlib.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/list.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zinterp/zinterp.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/app_view.h"
#include "mech3/mech3_interp.h"
#include "mech3/mech_anim.h"
#include "mech3/mech_build.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_command.h"
#include "mech3/app_hud.h"
#include "mech3/mech_move.h"
#include "mech3/mech_skis.h"
#include "mech3/mech_wake.h"
#include "mech3/sound_extra.h"
#include "mech3/player_view.h"
#include "mech3/weapon.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zweapon/zweapon.h"
#include "mech3/mech_config.h"
#include "mech3/mech_data.h"
#include "mech3/mech_lib.h"
#include "mech3/mech_names.h"
#include "mech3/mech_reader.h"
#include "mech3/mission_launch.h"
#include "mech3/mission_world.h"
#include "gamez/zentity/zent_manager.h"
#include "mech3/mech_control.h"
#include "mech3/sensors.h"
#include "mech3/zai_target.h"
#include "mech3/weapon.h"

namespace mech3 {

int32_t invincible();  // 0x0040cf20, mech3/options.h

namespace {
int32_t g_spawn_updates = 0;  // the spawn's update steps run (0x004782f0)
float g_spawn_pos[3] = {};    // where the placement left the mech, before that step

MechWiring g_w;
zcls::Node* zn(Handle h) { return static_cast<zcls::Node*>(h); }

// The mech data's calls (mech_data.cpp): the ports, and the engine for what
// has none (the cameras, the effects by name, the game type, the error box).
class WiredData : public MechDataHooks {
 public:
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return g_w.unported ? g_w.unported(g_w.engine, key, args) : 0;
  }
  Handle cameras(Pilot& p) override { return program_cameras(p.roster); }      // 0x00430e70
  void cameras_free(Handle list) override { program_cameras_free(list); }  // 0x0040abf0
  // The motions the gaits read (0x005a9ad0's archive) open with the first mech.
  void animations(Pilot& p) override {
    if (open_motions("zbd/motion.zbd")) load_animations(p);
  }
};

class WiredBuild : public MechBuildHooks {
 public:
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return g_w.unported ? g_w.unported(g_w.engine, key, args) : 0;
  }
  // 0x005833b0(mech, node, 1, 0x004161a0): hits on the mech's node go to
  // Mech::TakeDamage (app_weapons.cpp).
  void touch_handler(Mech& m, Handle) override { mech_set_damage_handler(&m); }
  Pilot* mech_data(int32_t index, int32_t create) override { return mech3::mech_data(index, create); }
  Handle node_find(Handle root, const char* name) override { return zcls::node_find_in_tree(zn(root), name); }
  // Each mech its own tree: the models and their materials copied.
  Handle node_clone(Handle node) override { return zcls::tree_copy(zn(node), 1, 1); }
  void node_remove_child(Handle parent, Handle child) override { zcls::node_remove_child(zn(parent), zn(child)); }
  void node_free(Handle node) override { zcls::tree_destroy(zn(node)); }
  void node_tree_update(Handle node) override { zcls::update_subtree(zn(node)); }
  void nodes_pending() override { zcls::update_changed(); }
  void node_set_model(Handle node, Handle model) override {
    zcls::node_set_model(zn(node), static_cast<zmod::Model*>(model));
  }
  Handle model_new() override { return zmod::model_alloc(); }
  void node_set_action(Handle node, uint32_t) override { zcls::node_set_action(zn(node), nullptr); }
  void node_set_flag8(Handle node, int32_t on) override { zcls::node_set_flag8(zn(node), on); }
  // 0x005a7f50: onto zEntity's list, the port's (mech3/sensors).
  void entity_register(Mech& m) override { ai::entity_list().push_back(g_w.handle_of(g_w.engine, &m)); }
  void node_set_active(Handle node, int32_t on) override { zcls::node_set_active(zn(node), on); }
  void node_set_name(Handle node, const char* name) override { zcls::node_set_name(zn(node), name); }
  void node_add_child(Handle parent, Handle child) override {
    if (parent && child) zcls::node_add_child(zn(parent), zn(child));
  }
  void node_set_translation(Handle node, float x, float y, float z) override {
    zcls::object3d_set_translation(zn(node), x, y, z);
  }
  Handle object3d_create() override { return zcls::object3d_create(); }
  Handle view() override { return h(g_w.view ? g_w.view(g_w.engine) : 0); }
  Handle view_camera(Handle v) override { return v ? h(g_w.peek32(g_w.engine, low(v) + 0x58)) : nullptr; }
  uint32_t camera_mode(Handle c) override { return g_w.peek32(g_w.engine, low(c) + 4); }
  Handle view_target(Handle v) override { return v ? h(g_w.peek32(g_w.engine, low(v) + 0x64)) : nullptr; }
  // vt+0xfc: the port's handler on the pilot's mech (mech_command.cpp).
  void view_target_command(Handle target, int32_t cmd) override {
    void* m = g_w.object_of(g_w.engine, low(target));
    if (is_launched_mech(m)) static_cast<Mech*>(m)->command(cmd);
    else MechBuildHooks::view_target_command(target, cmd);
  }
};

class WiredLaunch : public LaunchHooks {
 public:
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return g_w.unported ? g_w.unported(g_w.engine, key, args) : 0;
  }
  Handle missions() override { return h(0x6098d0); }
  Handle pilot() override { return h(g_w.pilot(g_w.engine)); }
  Handle view() override { return h(g_w.view(g_w.engine)); }
  uint32_t field32(Handle o, uint32_t off) override { return o ? g_w.peek32(g_w.engine, low(o) + off) : 0; }
  void set_field32(Handle o, uint32_t off, uint32_t v) override {
    if (o) g_w.poke32(g_w.engine, low(o) + off, v);
  }
  void set_field8(Handle o, uint32_t off, uint8_t v) override {
    if (o) g_w.poke8(g_w.engine, low(o) + off, v);
  }
  Config* config_of(uint32_t handle) override { return static_cast<Config*>(g_w.object_of(g_w.engine, handle)); }
  Handle game_slot(Handle game) override { return h(g_w.vcall(g_w.engine, low(game), 0x34, {0})); }
  int32_t game_type(Handle game) override { return static_cast<int32_t>(g_w.vcall(g_w.engine, low(game), 0x2c, {})); }
  int32_t use_joystick() override { return 0; }   // keyboard and mouse
  int32_t camera_mode() override { return program_camera_option(); }  // 0x0040ce70
  void set_camera_mode(int32_t m) override { program_camera_set(m); }  // 0x0040ce10
  void mech_attach_view(Mech& m) override { program_view_attach(m); }   // vt+0x108, 0x0041ce30
  void view_set_camera(Handle, uint32_t mode) override { program_view_set_camera(static_cast<int32_t>(mode)); }
  // The pilot's vehicle is the mech (as the exe's 0x004782c0 sets it).
  void pilot_set_mech(Handle p, Mech& m) override {
    unported("0x004782c0", {low(p), 0});
    set_field32(p, 0x14, g_w.handle_of(g_w.engine, &m));
    // The mech globals: [0x0060861c]+0x14, and the view's target (+0x64,
    // set by the view attach 0x0041ce30, not ported) is the same mech.
    world().mission_player = &m;
    world().player = &m;
    weapon_world().mission_player = &m;
    weapon_world().player = &m;
  }
  void interp_set_mech(Handle, Mech& m) override {
    if (zint::Interp* in = mech3::interp()) mech3::interp_set_mech(*in, &m, zn(m.node));
  }
  void interp_set_cycle_node(Handle, Handle node) override {  // the script object's +0xb8
    if (zint::Interp* in = mech3::interp()) in->current = zn(node);
  }
  uint8_t interp_source(Handle, const char* path) override {
    zint::Interp* in = mech3::interp();
    return in && zint::source(*in, path) ? 1 : 0;
  }
  // 0x004782f0 on the mech the launch just built (the pilot's +0x14).
  void pilot_place(Handle p, const float pos[3], uint32_t yaw) override {
    if (Mech* m = mech3::launched_mech()) place_pilot_mech(*this, p, *m, pos, yaw);
    else LaunchHooks::pilot_place(p, pos, yaw);  // logged: no mech to place
  }
  // Mech3.exe 1.2 @ 0x004782f0's vtable +0x34: the spawn's one step, the
  // port's frame (0x00419fc0).
  void mech_update(Mech& m, float dt) override {
    ++g_spawn_updates;
    if (m.transform) std::memcpy(g_spawn_pos, m.transform + 9, sizeof g_spawn_pos);
    m.update(dt);
  }
  // 0x00478f10(0) at a campaign's launch: the pilot the screen chose, settled
  // and saved (pilot.md section 7); a debug start's stand-in is logged.
  void pilot_478f10(Handle p) override {
    if (!program_record_mission(false)) LaunchHooks::pilot_478f10(p);
  }
  uint8_t cheat_invincible() override { return static_cast<uint8_t>(g_w.peek32(g_w.engine, 0x608e40)); }
  int32_t option_invincible() override { return mech3::invincible(); }  // options.h (not includable with mech.h)
  // The missions' list at +0xc0 is not in the program's memory.
  void free(Handle) override {}
};

WiredBuild g_build;
WiredData g_data;
WiredLaunch g_launch;

}  // namespace

namespace {
// Mech3.exe 1.2 @ 0x005a85e0: the entity's node's world position, its world
// matrix refreshed first when stale (0x0051e9d0, 0x0051fde0, 0x004ed280, 0x0051ea00).
const Vec3* entity_position(Mech& m) {
  auto* n = zn(m.node);
  if (!n || !n->data || n->cls() != 5) return nullptr;
  auto* d = static_cast<zcls::Object3d*>(n->data);
  if (d->get<uint32_t>(0) & 0x20) {
    zmth::Mat34 buf;
    zmth::push(&buf);
    zmth::load_identity();
    zcls::node_to_world(n, 1);
    zmth::pop();
  }
  return reinterpret_cast<const Vec3*>(d->floats(0x84));
}

// The mech's frame (Mech::update and below) on the engine's ports.
void wire_mech_frame() {
  PoseCalls& p = pose_calls();
  p.node_set = [](Handle n, const float* m) { zcls::object3d_set_matrix(zn(n), m); };       // 0x004fc300
  p.node_matrix = [](Handle n) { return zcls::object3d_matrix(zn(n)); };                    // 0x004fc3f0
  SkisCalls& s = skis_calls();
  s.node_matrix = p.node_matrix;
  s.set_node_matrix = p.node_set;
  s.position = entity_position;
  s.soil = [](Handle mat) {  // [0x00608c00 + 4 * material +0x20]
    return static_cast<int>(game_soil(static_cast<const zmod::Material*>(mat)->soil));
  };
  CollideCalls& c = collide_calls();
  c.refresh_box = [](Handle n) { zcls::node_children_box(zn(n)); };                         // 0x004ecf70
  c.node_box = [](Handle n) { return reinterpret_cast<const float*>(zn(n)->raw + 0xa4); };
  c.set_position = [](Mech& m, const Vec3* at) {  // vtable +0x18, zEntity3d's 0x005a8550
    zcls::object3d_set_translation(zn(m.node), at->x, at->y, at->z);
    m.entity_moved();
  };
  update_calls().moved = [](Mech& m) { m.entity_moved(); };                                 // 0x005a85c0
  // What a probe hit (0x0040ef10 / 0x0040ef00): a mech's nodes are its own,
  // so it blocks and is shoved as a mech rather than as a wall.
  c.node_mech = [](Handle n) { return static_cast<Mech*>(node_owner(n)); };
  c.node_entity = [](Handle n) -> Handle {  // 0x0040e960
    const zcls::Node* z = zn(n);
    if (!z) return nullptr;
    const uint32_t id = z->get<uint32_t>(0x28);
    if (id & 0x40000) return z->owner;
    if (Handle o = node_owner(n)) return o;
    return id & 0x80000 ? z->owner : nullptr;  // 0x0040e9b0
  };
  // The bump looks through the sensors' contacts. A contact's vehicle (vt+0x2c)
  // and mech (vt+0x50) are the mech itself (0x004d2a70); targets.zrd's
  // objects answer vt+0x40 with 0 (0x0040bae0), and the vehicles that do
  // (the Mobile HQ) are not ported.
  c.contact_count = [](Mech& m) { return static_cast<int32_t>(m.contacts.size()); };
  c.contact = [](Mech& m, int32_t i) -> Handle { return m.contacts[i]; };
  c.contact_mech = [](Handle ct) -> Mech* {  // 0x004e6ff0
    const ai::Handle s = static_cast<ai::Contact*>(ct)->seen;
    for (const ai::Handle e : ai::entity_list())
      if (s && e == s) return static_cast<Mech*>(g_w.object_of(g_w.engine, s));
    return nullptr;
  };
  c.contact_target = [](Handle) { return false; };
  c.contact_dist2 = [](Handle ct) { return static_cast<ai::Contact*>(ct)->dist2; };
  c.contact_dist = [](Handle ct) { return static_cast<ai::Contact*>(ct)->distance(); };
  c.contact_dir = [](Handle ct) { return reinterpret_cast<const Vec3*>(static_cast<ai::Contact*>(ct)->dir); };
  // The AI mechs' controls run their brains' frames (AiControls::update).
  // The player's keys (Mech::command): what is not ported is logged once by
  // the engine; the HUD's keys wait for the HUD's own commands.
  CommandCalls& k = command_calls();
  k.unported = [](const char* key, int32_t cmd) { return g_w.unported(g_w.engine, key, {cmd}); };
  k.handle = [](Mech& m) { return g_w.handle_of(g_w.engine, &m); };
  k.view_camera = program_view_set_camera;                                                  // 0x0040ae70
  k.set_view_option = program_set_mouse_mode;                                              // [[0x005f4d44]]
}

// The mech's world position (0x005a85e0).
Vec3 mech_position(Mech& m) {
  if (skis_calls().position) return *skis_calls().position(m);
  return m.transform ? Vec3{m.transform[9], m.transform[10], m.transform[11]} : Vec3{0, 0, 0};
}

// A node's handler word (+0xbc) above 1 with a touch (slot 1, +0x0c).
bool handler_touchable(Handle n) {
  const zwep::RunHooks& rh = zwep::run_hooks();
  const uintptr_t h = n && rh.handler ? rh.handler(n) : 0;
  return h > 1 && reinterpret_cast<const zwep::NodeHandlers*>(h)->slot[1].fn != nullptr;
}

// What the parts log as they play (ours, for the headless tests): "step
// <sound>", "print <effect>", "wake <level>", "pilot <sound>".
std::vector<std::string> g_parts_fx;
void note_part(const char* what, const std::string& name) { g_parts_fx.push_back(std::string(what) + " " + name); }
std::string sound_name(const void* s) {
  const auto* o = static_cast<const zsnd::Object*>(s);
  if (!o) return "-";
  return o->kind == 0 ? static_cast<const zsnd::Sound*>(o)->name : static_cast<const zsnd::Group*>(o)->name;
}

// pilot_sound with the parts log's "pilot <sound>" line.
Sound pilot_play(Mech& m, int id, float volume, const Vec3* at) {
  if (!m.pilot) return nullptr;
  note_part("pilot", sound_name(m.pilot->sounds[id]));
  return pilot_sound(*m.pilot, id, volume, at ? &at->x : nullptr, nullptr);
}

// The parts' outside calls in the game (PartCalls, mech_parts.h; the parts'
// share of DamageCalls; the collider's touch): the engine's ports.
void wire_mech_parts() {
  PartCalls& pc = part_calls();
  pc = PartCalls{};
  pc.register_part = [](Mech& m, MechPart& p) { m.parts.push_back(&p); };  // 0x0041b8d0
  pc.foot_effect = []() -> Handle {                                         // 0x0045f980
    return foot_wake_new(world_hooks().world, "mech_leg_wake01", "mech_leg_wake02", "mech_leg_wake03");
  };
  pc.effect_playing = [](Handle e) { return foot_wake_playing(static_cast<FootWake*>(e)); };   // 0x0045fb80
  pc.effect_start = [](Handle e, float t) {                                                    // 0x0045faf0
    auto* w = static_cast<FootWake*>(e);
    const int32_t before = w->level;
    foot_wake_start(w, t);
    if (w->level != before) note_part("wake", std::to_string(w->level));
  };
  pc.effect_place = [](Handle e, float x, float y, float z) {                                  // 0x004fc0b0
    zcls::object3d_set_translation(static_cast<FootWake*>(e)->node, x, y, z);
  };
  pc.step_on = [](const Vec3& at, GroundHit& hit, float force) {  // 0x005836b0 with the contact as the record
    if (!handler_touchable(hit.node)) return;
    zwep::HitRecord r;
    r.point = {hit.x, hit.height, hit.z};
    r.material = hit.material, r.node = hit.node;
    const zwep::Vec3f3 from{at.x, at.y, at.z};
    zwep::touch(&from, &r, force);
  };
  pc.play_at = [](Handle s, const Vec3* at, float volume) {                                    // 0x0056e7e0(.., 0)
    note_part("step", sound_name(s));
    if (s) zsnd::play_at(static_cast<zsnd::Object*>(s), &at->x, volume, nullptr);
  };
  pc.look_limits = [](Mech& m, float* max_yaw, float* max_pitch, float* min_pitch) {
    // vt+0x10c(1): the cockpit camera; its vt+0x1c (0x004d2a70) is itself.
    auto* list = m.pilot ? static_cast<CameraList*>(m.pilot->cameras) : nullptr;
    auto* cam = list ? static_cast<MechCamera*>(list->find(1)) : nullptr;
    if (!cam) return;
    *max_yaw = cam->look_left, *max_pitch = cam->look_up, *min_pitch = cam->look_down;  // +0x68, +0x74, +0x70
  };
  pc.aim_point = [](Mech& m, Vec3* at) {  // +0xd4 (the weapons): +0x18 an aim, +0x0c the point
    const auto* wm = static_cast<const WeaponManager*>(m.ammo);
    if (!wm || !wm->aim_valid) return false;
    *at = Vec3{wm->aim.x, wm->aim.y, wm->aim.z};
    return true;
  };
  pc.ground_at = [](float x, float y, float z, void* hits) {  // 0x004edab0 on [0x00608c84] +4
    if (zcls::Node* w = world_hooks().world) zcls::ground_point(w, x, y, z, static_cast<zcls::GroundHits*>(hits));
  };
  DamageCalls& d = damage_calls();
  d.spawn_effect = [](Handle effect, Handle node, const float args[9]) -> Handle {  // 0x00508aa0
    if (!effect) return nullptr;
    auto* a = static_cast<zeff::AnimDef*>(effect);
    note_part("print", a->name());
    return zeff::anim_start_at(a, zn(node), args, args + 3, args + 6);
  };
  d.spawn_effect_at = [](Handle effect, Handle node, float x, float y, float z) -> Handle {  // 0x00508c10
    return effect ? zeff::anim_start(static_cast<zeff::AnimDef*>(effect), zn(node), x, y, z) : nullptr;
  };
  d.node_set_active = [](Handle n, int on) { zcls::node_set_active(zn(n), on); };          // 0x004eb170
  d.node_set_name = [](Handle n, const char* name) { zcls::node_set_name(zn(n), name); };  // 0x004eb300
  d.stop_foot_effect = [](Handle e) { foot_wake_stop(static_cast<FootWake*>(e)); };       // 0x0045fb50
  // The pilot's sounds (0x00432990 on +0x684, through the sounds' EXTRA
  // records, sound_extra.cpp): unplaced, or at the mech where the exe passes
  // its position.
  pc.sound = [](Mech& m, int id, float volume) -> void* { return pilot_play(m, id, volume, nullptr); };
  MoveCalls& mv = move_calls();
  mv.pilot_sound = [](Mech& m, int id, float volume, float) -> Sound {  // the fifth argument is not read
    const Vec3 at = mech_position(m);
    return pilot_play(m, id, volume, &at);
  };
  mv.fade_sound = [](Sound s) -> Sound {  // 0x00571d20(s, 1, 0)
    return s ? zsnd::fade(static_cast<zsnd::VoiceBase*>(s), 1, 0.0f) : nullptr;
  };
  mv.stop_effect = [](Handle e) { if (e) zeff::anim_stop(static_cast<zeff::AnimDef*>(e)); };  // 0x00506ca0
  mv.weapons_enable = [](Mech& m, int on) {                                                    // 0x004b0bf0 on +0xd4
    if (auto* wm = static_cast<WeaponManager*>(m.ammo)) wm->set_online(on != 0);
  };
  HeatCalls& hc = heat_calls();
  hc.throttle = [](Mech& m) { return m.controls ? m.controls->throttle : 0.0f; };  // +0x37c GetThrottle
  hc.play_sound = [](Mech& m, int id) -> Sound { return pilot_play(m, id, 1.0f, nullptr); };
  hc.play_sound_at = [](Mech& m, int id) -> Sound {
    const Vec3 at = mech_position(m);
    return pilot_play(m, id, 1.0f, &at);
  };
  hc.stop_sound = [](Sound s) { if (s) zsnd::stop(static_cast<zsnd::VoiceBase*>(s)); };  // 0x0056e9f0
  CollideCalls& c = collide_calls();
  c.node_touchable = [](Handle n) { return handler_touchable(n); };  // node +0xbc above 1 with slot 1 (0x004230a0)
  c.touch = [](Handle n, const Vec3* from, const CollisionHit& h, float damage) {  // 0x005836b0
    zwep::HitRecord r;
    std::memcpy(&r.normal, h.hit.normal, 12);
    std::memcpy(&r.point, h.hit.point, 12);
    r.material = h.hit.material, r.node = n;
    const zwep::Vec3f3 f{from->x, from->y, from->z};
    return zwep::touch(&f, &r, damage);
  };
}
}  // namespace

void wire_mech_build(const MechWiring* w) {
  if (!w) {
    g_w = MechWiring{};
    pose_calls() = PoseCalls{};
    skis_calls() = SkisCalls{};
    collide_calls() = CollideCalls{};
    controls_calls().ai_brain = nullptr;
    update_calls().moved = nullptr;
    command_calls() = CommandCalls{};
    part_calls() = PartCalls{};
    move_calls() = MoveCalls{};
    heat_calls().throttle = nullptr, heat_calls().play_sound = nullptr, heat_calls().play_sound_at = nullptr;
    heat_calls().stop_sound = nullptr;
    collide_calls().touch = nullptr, collide_calls().node_touchable = nullptr;
    unwire_hud();
    unwire_weapons();
    set_mech_build_hooks(nullptr);
    set_mech_data_hooks(nullptr);
    set_launch_hooks(nullptr);
    return;
  }
  g_w = *w;
  set_mech_build_hooks(&g_build);
  set_mech_data_hooks(&g_data);
  set_launch_hooks(&g_launch);
  // The weapons a mech mounts look up its owner, firepoints and jump jets
  // (weapon.h); the program's managers are all mechs' so far.

  wire_mech_frame();
  wire_mech_parts();
  wire_hud();
  wire_weapons([](const char* key) { return g_w.unported ? g_w.unported(g_w.engine, key, {}) : 0u; });
  WeaponCalls& c = weapon_calls();
  c.vehicle_mech = [](Handle v) { return v; };
  c.vehicle_node = [](Handle v) { return static_cast<Mech*>(v)->node; };
  c.add_jump_jet = [](Handle m) { static_cast<Mech*>(m)->add_jump_jet(true); };
  c.firepoints = [](Handle m) -> std::vector<Firepoint*>* {
    const MechDfn* d = mech_dfn(static_cast<Mech*>(m)->pilot->roster);
    return d ? d->firepoints : nullptr;
  };
}

void mech_types_open() {
  if (!open_mechlib()) std::fprintf(stderr, "mw3: no zbd\\mechlib.zbd: the mechs have no models\n");
}
void mech_types_refresh() { mech_data_refresh(); }
void mech_types_close() { close_mechlib(); }
void mech_types_forget() { mech_data_clear(); }

std::vector<std::string>& mech_parts_fx_log() { return g_parts_fx; }

bool launched_mech(LaunchedMech* out) { return describe_mech(mech3::launched_mech(), out); }

bool describe_mech(const void* mech, LaunchedMech* out) {
  const Mech* m = static_cast<const Mech*>(mech);
  if (!m) return false;
  LaunchedMech s;
  s.roster = m->pilot->roster;
  const MechPart* parts[] = {m->hip, m->torso, m->head, m->arms[0], m->arms[1], m->legs[0], m->legs[1]};
  for (const MechPart* p : parts) {
    s.parts += p && p->node;
    if (p) s.armor += p->armor + p->internal;
  }
  s.heat = m->heat;
  for (const Arm* a : m->arms) s.parts += a && a->hand && a->hand->node;
  for (const Leg* l : m->legs) {
    if (!l) continue;
    s.parts += l->calf && l->calf->node;
    s.parts += l->foot && l->foot->node;
    if (l->foot) s.toes += static_cast<int32_t>(l->foot->toe_count);
  }
  if (const auto* wm = static_cast<const WeaponManager*>(m->ammo)) {
    s.weapons = static_cast<int32_t>(wm->count);
    s.group = wm->group;
  }
  s.jets = m->equipment[11];
  s.jets_firing = m->jump_jets_firing;
  s.crouched = m->crouched;
  s.alive = m->alive_time;
  s.anim_state = m->anim_state;
  for (int i = 0; i < 4; ++i) s.blocked[i] = m->blocked[i];
  s.attacker = m->attacker;
  s.sinks = m->heat_sinks.count;
  s.height = m->height;
  s.max_speed = m->max_speed;
  s.torso_armor_scale = m->torso ? m->torso->armor_scale : 0;
  s.team = m->team;
  s.skis = m->skis != nullptr;
  s.ams = m->ams != nullptr;
  s.motions = m->anims[4] && m->anims[8];
  s.powering_down = m->power_down_pending || m->powered_down;
  s.powered_down = m->powered_down;
  if (m->transform) s.pos[0] = m->transform[9], s.pos[1] = m->transform[10], s.pos[2] = m->transform[11];
  s.yaw_sin = m->orientation[2];
  s.feet_down = m->skis && m->skis->down[0] && m->skis->down[1];
  const zcls::Node* n = static_cast<const zcls::Node*>(m->node);
  for (int32_t i = 0; n && i < n->parent_count(); ++i) s.in_world |= n->parents[i] == world_hooks().world && n->parents[i];
  *out = s;
  return true;
}

void* build_ai_mech(int32_t mech, void* controls, int32_t config, const char* name) {
  return mech_create(mech, static_cast<MechControls*>(controls), config, name);
}
bool set_mech_skin(void* mech, void* skin) { return mech && mech_set_skin(*static_cast<Mech*>(mech), skin); }
int32_t place_mech(void* mech, const float pos[3], uint32_t yaw) {
  float angle;
  std::memcpy(&angle, &yaw, 4);
  return mech ? static_cast<Mech*>(mech)->place(pos, angle) : 0;
}
int32_t spawn_updates() { return g_spawn_updates; }
const float* spawn_position() { return g_spawn_pos; }
bool is_launched_mech(const void* mech) { return mech && mech == mech3::launched_mech(); }
bool mech_aim_at(void* mech, const float at[3]) {
  if (!mech) return false;
  const Vec3 p{at[0], at[1], at[2]};
  return static_cast<Mech*>(mech)->aim_at(&p, nullptr, true);
}
void update_mech(void* mech, float dt) {
  if (mech) static_cast<Mech*>(mech)->update(dt);
}
// Mech3.exe 1.2 @ 0x004a1140 ([0x005e7284] is 1): the entity manager's walk
// 0x005a8850 over zEntity's list, the program's mechs; the sensors leave the
// player's +0x09 clear, so its frame is the player tick's alone.
void update_entities(float dt) {
  zent::budget().update_all(ai::entity_list(), [dt](ai::Handle h) {
    auto* m = static_cast<Mech*>(g_w.object_of(g_w.engine, h));
    if (m && m->x09) m->update(dt);  // vt+0x34
  });
}
// Mech3.exe 1.2 @ 0x0041ca30: the player's mech takes the value, any other 1.
void mech_set_hud_fall(void* mech, int32_t v) {
  if (!mech) return;
  Mech* m = static_cast<Mech*>(mech);
  m->hud_fall = m == world().mission_player ? v : 1;  // [0x0060861c]+0x14
}
void move_mech(void* mech, const float pos[3], float yaw) {
  if (mech) static_cast<Mech*>(mech)->set_place(pos, yaw);
}
void mech_power(void* mech, int32_t down) {
  if (mech) static_cast<Mech*>(mech)->power(down);
}
uint32_t mech_allies(const void* mech) { return mech ? static_cast<const Mech*>(mech)->allies : 0; }
bool mech_set_mask(void* mech, uint32_t off, uint32_t v) {
  Mech* m = static_cast<Mech*>(mech);
  if (!m) return false;
  if (off == 0xc0) return m->team = v, true;
  if (off == 0xc4) return m->allies = v, true;
  if (off == 0xc8) return m->enemies = v, true;
  return false;
}
void* mech_controls_of(void* mech) { return mech ? static_cast<Mech*>(mech)->controls : nullptr; }
float mech_game_time() { return world().time; }
float mech_health(void* mech) { return static_cast<Mech*>(mech)->health(); }
bool mech_powered_down(void* mech) { return static_cast<Mech*>(mech)->is_powered_down(); }
ai::AiTarget* mech_ai_target(void* mech) { return static_cast<Mech*>(mech); }
bool mech_ammo_value(void* mech, float* out) {
  const auto* m = static_cast<const WeaponManager*>(static_cast<Mech*>(mech)->ammo);
  if (!m) return false;
  *out = m->ammo_value_inv != 0 ? m->ammo_value * m->ammo_value_inv : 1.0f;
  return true;
}
bool mech_world_position(void* mech, float out[3]) {
  const Vec3* p = entity_position(*static_cast<Mech*>(mech));
  out[0] = p ? p->x : 0, out[1] = p ? p->y : 0, out[2] = p ? p->z : 0;
  return p != nullptr;
}
// The missions object's +0x4c (0x0048f470) is [0x0060991c]: the copies the ports keep.
void mech_set_game_time(float t) { world().time = weapon_world().time = t; }
bool mech_get_mask(const void* mech, uint32_t off, uint32_t* v) {
  const Mech* m = static_cast<const Mech*>(mech);
  if (!m) return false;
  if (off == 0xc0) return *v = m->team, true;
  if (off == 0xc4) return *v = m->allies, true;
  if (off == 0xc8) return *v = m->enemies, true;
  return false;
}
const void* mech_definition(void* mech) { return mech ? mech_dfn(static_cast<Mech*>(mech)->pilot->roster) : nullptr; }

uint32_t debug_slot_config(int32_t mech, void* e, uint32_t (*handle_of)(void* e, const void* p)) {
  const MechDfn* d = mech_dfn(mech);
  if (!d || !d->configs || d->configs->configs.empty()) return 0;
  return handle_of(e, d->configs->configs[0]);
}

}  // namespace mech3
