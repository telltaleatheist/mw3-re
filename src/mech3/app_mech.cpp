// The mech build's and the launch's hooks wired to the ports (docs/mech-build.md
// section 9). Not original code: it stands where the exe's other modules
// will be. zClass's nodes, the mech data (0x00430d10) and mechlib are the
// ports; what has no port is logged once by address through the engine.
#include "mech3/cockpit_sound.h"
#include "gamez/zimage/zimg_texture.h"
#include "mech3/app_mech.h"

#include <cstdio>
#include <cstring>
#include <unordered_set>
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
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zmodel/gmod_init.h"
#include "gamez/zmodel/gmod_light.h"
#include "gamez/zvideo/zvid_format.h"
#include "mech3/app_shadows.h"
#include "gamez/zinterp/zinterp.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/app_view.h"
#include "mech3/mech3_interp.h"
#include "mech3/mech_anim.h"
#include "mech3/mech_build.h"
#include "mech3/mech_dead.h"
#include "mech3/mech_lod.h"
#include "mech3/mech_skin.h"
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
#include "gamez/zerror/zerr.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zweapon/zweapon.h"
#include "mech3/mech_config.h"
#include "mech3/mech_data.h"
#include "mech3/mech_lib.h"
#include "mech3/mech_shade.h"
#include "gamez/zclass/camera.h"
#include "gamez/zmodel/gmod_draw.h"
#include "mech3/mech_names.h"
#include "mech3/mech_reader.h"
#include "mech3/mech_salvage.h"
#include "crt/msvc_rand.h"
#include "mech3/mission_launch.h"
#include "mech3/mission_world.h"
#include "gamez/zentity/zent_manager.h"
#include "mech3/mech_control.h"
#include "mech3/sensors.h"
#include "mech3/zai_target.h"
#include "mech3/zai_brain.h"
#include "mech3/weapon.h"

namespace mech3 {

int32_t invincible();  // 0x0040cf20, mech3/options.h

namespace {
// The program's mechs (ours): what a zEntity entry's vt+0x2c then vt+0x50
// answers in the exe (a mech itself; the basic vehicle, a turret or a target
// 0). zEntity's list holds handles of any entity; only these are Mechs.
std::unordered_set<const void*>& mech_objects() {
  static std::unordered_set<const void*> s;
  return s;
}
Mech* entity_mech(ai::Handle h);
Vec3 mech_position(Mech& m);
}  // namespace

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
  Handle cameras(MechData& p) override { return program_cameras(p.roster); }      // 0x00430e70
  void cameras_free(Handle list) override { program_cameras_free(list); }  // 0x0040abf0
  Handle anim_find(const char* name) override { return zeff::anim_find(name); }  // 0x0050b450, the mission's
  // The motions the gaits read (0x005a9ad0's archive) open with the first mech.
  void animations(MechData& p) override {
    if (open_motions("zbd/motion.zbd")) load_animations(p);
  }
};

// The mechs' sun shading (mech_shade.cpp, docs/shade.md) on the program's
// ports: the mission's sun, the mechs' nodes and parts, zClass's queries and
// flags, the scene walk's before and after calls, the model draw's bake.
class WiredShade : public ShadeHooks {
 public:
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return g_w.unported ? g_w.unported(g_w.engine, key, args) : 0;
  }
  static uint32_t id(const void* p) { return p ? g_w.handle_of(g_w.engine, p) : 0; }
  static void* obj(uint32_t h) { return h ? g_w.object_of(g_w.engine, h) : nullptr; }
  static zcls::Node* node(uint32_t h) { return static_cast<zcls::Node*>(obj(h)); }
  static zcls::Light* light(uint32_t h) { return static_cast<zcls::Light*>(obj(h)); }
  zgame::Handle world_sun() override {
    auto* sun = static_cast<zcls::Node*>(mission_sun());
    return sun && sun->data ? id(sun->data) : 0;
  }
  float sun_diffuse(zgame::Handle s) override { return light(s) ? light(s)->get<float>(0x9c) : 1.0f; }
  void set_sun_diffuse(zgame::Handle s, float v) override {
    if (light(s)) light(s)->put<float>(0x9c, v);
  }
  void sun_direction(zgame::Handle s, float out[3]) override {
    for (int i = 0; i < 3; ++i) out[i] = light(s) ? light(s)->get<float>(0x84 + 4 * i) : 0.0f;
  }
  zgame::Handle entity_node(zgame::Handle e) override {
    auto* m = static_cast<Mech*>(obj(e));
    return m ? id(m->node) : 0;
  }
  zgame::Handle entity_centroid(zgame::Handle e) override {
    auto* m = static_cast<Mech*>(obj(e));
    return m ? id(m->centroid) : 0;
  }
  void node_world_cached(zgame::Handle h, float out[3]) override {
    out[0] = out[1] = out[2] = 0;
    zcls::Node* n = node(h);
    if (!n || !n->data) return;
    auto* d = static_cast<uint8_t*>(n->data);
    if (d[0] & 0x20) zcls::node_to_world(n, 1);  // its world matrix redone (0x004ed280(1))
    std::memcpy(out, d + 0x84, 12);
  }
  void node_world_position(zgame::Handle h, float out[3]) override {
    if (zcls::Node* n = node(h)) zcls::node_world_position(n, out);
  }
  void node_intersect(zgame::Handle h, int32_t on) override {
    if (zcls::Node* n = node(h)) zcls::node_set_intersect(n, on);
  }
  void query_mode(int32_t on) override { zcls::segment_options().first_hit = on; }
  void query_exclude(uint32_t mask) override { zcls::segment_options().exclude = mask; }
  int32_t segment(const float a[3], const float b[3], uint8_t* first) override {
    static zcls::LineHits r;
    zcls::Node* w = world_hooks().world;
    if (!w) return *first = 0, 1;
    const int32_t res = zcls::segment_list(w, a, b, &r);
    *first = r.count;
    return res;
  }
  void node_flags(zgame::Handle h, uint8_t on) override {  // 0x004eb8a0, 0x004eb8e0: bits 5 and 6
    zcls::Node* n = node(h);
    if (!n) return;
    const uint32_t v = n->get<uint32_t>(0x2c) & ~0x60u;
    n->put<uint32_t>(0x2c, v | (on & 1 ? 0x60u : 0u));
  }
  int32_t frame() override { return static_cast<int32_t>(zcls::frames().frame); }
  void walk_callbacks(bool on) override {
    zcls::scene_walk().pre = on ? [](zcls::Node* n) { shade_before(id(n)); } : nullptr;
    zcls::scene_walk().post = on ? [](zcls::Node* n) { shade_after(id(n)); } : nullptr;
  }
  void static_lights(int32_t on) override { zmod::draw_state().static_lights = on; }
  void stamps_frozen(int32_t on) override { zcls::frames().frozen = on; }
  // 0x0040ef30 / 0x0040ef10 on the node: a part's +0x68, else its owner's +0x68c.
  ShadeLevel* part_level(zgame::Handle h) override {
    zcls::Node* n = node(h);
    if (!n || !(n->get<uint32_t>(0x28) & 0xff)) return nullptr;
    auto* p = static_cast<MechPart*>(n->owner);
    return p ? static_cast<ShadeLevel*>(p->x68) : nullptr;
  }
  ShadeLevel* mech_level(zgame::Handle h) override {
    auto* m = static_cast<Mech*>(node_owner(node(h)));
    return m && m->x68c ? &static_cast<SunShade*>(m->x68c)->level : nullptr;
  }
  int32_t part_count(SunShade& sh) override {
    auto* m = static_cast<Mech*>(obj(sh.mech));
    return m ? static_cast<int32_t>(m->parts.size()) : 0;
  }
  ShadeLevel* part(SunShade& sh, int32_t i, zgame::Handle* n) override {
    auto* m = static_cast<Mech*>(obj(sh.mech));
    *n = 0;
    if (!m) return nullptr;
    auto it = m->parts.begin();
    std::advance(it, i);
    if (!*it) return nullptr;
    *n = id((*it)->node);
    return static_cast<ShadeLevel*>((*it)->x68);
  }
  int32_t entity_count() override { return static_cast<int32_t>(ai::entity_list().size()); }
  SunShade* entity_shade(int32_t i) override {
    Mech* m = entity_mech(ai::entity_list()[static_cast<size_t>(i)]);
    return m ? static_cast<SunShade*>(m->x68c) : nullptr;
  }
  float game_time() override { return mech_game_time(); }
};
WiredShade g_shade_hooks;

class WiredBuild : public MechBuildHooks {
 public:
  // new(0x24) the mech's shade, new(0xc) a part's level (mech_shade.h).
  Handle alloc(uint32_t size) override {
    if (size == 0x24) return new SunShade;
    if (size == 0xc) return new ShadeLevel;
    return MechBuildHooks::alloc(size);
  }
  Handle x68c_init(Handle mem, Mech& m) override {  // 0x004e61e0(mech, &mech+0x380)
    auto* s = static_cast<SunShade*>(mem);
    const uint32_t me = g_w.handle_of(g_w.engine, &m);
    shade_init(*s, me, me);
    return s;
  }
  void x68c_unlink(Handle x68c) override { shade_reset(*static_cast<SunShade*>(x68c)); }  // 0x004e67c0
  Handle node_ref_init(Handle mem) override {  // 0x004e6250
    auto* l = static_cast<ShadeLevel*>(mem);
    *l = ShadeLevel{};
    shade_level_reset(*l, 0);
    return l;
  }
  void node_ref_set(Handle ref, Handle n) override {  // 0x004e66a0
    shade_level_reset(*static_cast<ShadeLevel*>(ref), WiredShade::id(n));
  }
  void entity_update(Mech& m) override { m.refresh_markers(); }  // 0x005a8500
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return g_w.unported ? g_w.unported(g_w.engine, key, args) : 0;
  }
  // 0x005833b0(mech, node, 1, 0x004161a0): hits on the mech's node go to
  // Mech::TakeDamage (app_weapons.cpp).
  void touch_handler(Mech& m, Handle) override { mech_set_damage_handler(&m); }
  MechData* mech_data(int32_t index, int32_t create) override { return mech3::mech_data(index, create); }
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
  void entity_register(Mech& m) override {  // 0x005a7f50
    ai::entity_list().push_back(g_w.handle_of(g_w.engine, &m));
    mech_objects().insert(&m);
  }
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
  // The cockpit screens' cycled textures, added by the scripts, loaded
  // (playtest #19: unloaded, they drew with no surface).
  void textures_load() override { zimg::load_group_0(); }
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
SalvageMech salvage_view(Mech& m);
const SalvageCalls& salvage_calls();
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
    return s ? entity_mech(s) : nullptr;
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
  // Z and Q's aimed object (0x0040eee0) by its handle: a mech's team and no
  // refusal (0x004d7290), a targets.zrd object's by its zoom_select /
  // reticule_select (0x004d3360, 0x004d3370); anything else (the vehicles,
  // not ported) refuses nothing and has no team.
  k.node_entity = [](void* n) { return n ? g_w.handle_of(g_w.engine, node_entity(n)) : 0u; };
  k.entity_team = [](uint32_t e) -> uint32_t {
    if (Mech* m = entity_mech(e)) return m->team;
    for (const ai::CZAiTarget* t : ai::target_list())
      if (t->self == e) return t->team;
    return 0;
  };
  k.entity_refuses = [](uint32_t e, bool zoom) {
    for (const ai::CZAiTarget* t : ai::target_list())
      if (t->self == e) return !(t->desc.flags & (zoom ? 8 : 4));
    return false;
  };
  // The look keys (0x62..0x67) and FREELOOK: the view's mode vt+0x10, the
  // free look (playtest #14: wired to the camera switch before).
  k.view_camera = program_view_look;
  // 0x005a8630 on a contact's vehicle (the part aim's fallback): a mech's
  // position; the other entities' are not ported (none).
  k.seen_position = [](uint32_t s) -> const float* {
    static Vec3 at;
    Mech* m = entity_mech(s);
    if (!m) return nullptr;
    at = mech_position(*m);
    return &at.x;
  };
  k.set_view_option = program_set_mouse_mode;  // [[0x005f4d44]]
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
void note_part(const char* what, const std::string& name) {
  if (g_parts_fx.size() < 4096) g_parts_fx.push_back(std::string(what) + " " + name);  // as weapon_fx_log: bounded in play
}
std::string sound_name(const void* s) {
  const auto* o = static_cast<const zsnd::Object*>(s);
  if (!o) return "-";
  return o->kind == 0 ? static_cast<const zsnd::Sound*>(o)->name : static_cast<const zsnd::Group*>(o)->name;
}

// pilot_sound with the parts log's "pilot <sound>" line.
Sound pilot_play(Mech& m, int id, float volume, const Vec3* at) {
  if (!m.pilot) return nullptr;
  note_part("pilot", sound_name(m.pilot->sounds[id]));
  return pilot_sound(m.pilot->sounds, id, volume, at ? &at->x : nullptr, nullptr);
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
  pc.effect_free = [](Handle e) { foot_wake_free(static_cast<FootWake*>(e), world_hooks().world); };  // 0x0045fab0
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
  // The deaths and damage reactions (c1m1 item 2, docs/damage.md).
  d.death_prep = [](Mech& m) { m.death_prep(); };                 // 0x0041d070
  d.death_anim = [](Mech& m, int anim) { m.death_fall(anim); };   // 0x0041b760
  d.stop_action = [](Handle a) { if (a) static_cast<MechAction*>(a)->stop(); };  // 0x00421060
  d.blend_anim = [](Handle base, Handle limp) -> Handle {         // 0x0041ff30
    return make_blend(static_cast<MechAnim*>(base), static_cast<MechAnim*>(limp));
  };
  d.on_effect_end = [](Handle e, MechPart& p) {                   // 0x00508f00 with 0x00461c90
    zeff::anim_set_callback(
        static_cast<zeff::AnimDef*>(e),
        [](zeff::AnimDef* def, void* part, int32_t v) { part_effect_ended(def, *static_cast<MechPart*>(part), v); },
        &p);
  };
  d.weapons_stop = [](Mech& m) {                                   // 0x004b1110 on +0xd4
    if (auto* wm = static_cast<WeaponManager*>(m.ammo)) wm->destroy_all();
  };
  d.weapons_lost = [](Mech& m, int part) {                         // 0x004b1040 on +0xd4
    if (auto* wm = static_cast<WeaponManager*>(m.ammo)) wm->part_lost(part);
  };
  d.weapons_critical = [](Mech& m, int part, int ammo) {           // 0x004b0de0 on +0xd4
    auto* wm = static_cast<WeaponManager*>(m.ammo);
    return wm && wm->critical(part, ammo != 0);
  };
  // A part flying off (0x00461d80, mech_parts.cpp) on zClass.
  FlyCalls& f = fly_calls();
  f.copy = [](Handle stub) -> Handle { return zcls::tree_copy(zn(stub), 0, 0); };  // 0x004f9af0
  f.create = []() -> Handle { return zcls::object3d_create(); };                   // 0x004fb740
  f.report = [](const char* mech, int32_t id) {
    char msg[96];
    std::snprintf(msg, sizeof msg, "Failed to copy stub for %s partId %d", mech ? mech : "", id);
    zerr::report(0x400, "C:\\zipper\\Mech3\\mech_parts.cpp", 0x4a4, msg);
  };
  f.rename = [](Handle n, Handle from) { zcls::node_set_name(zn(n), zn(from)->name()); };  // 0x004eb300
  f.parent = [](Handle n) -> Handle { return zn(n)->parent_count() > 0 ? zn(n)->parents[0] : nullptr; };
  f.set_flag8 = [](Handle n, int32_t on) { zcls::node_set_flag8(zn(n), on); };          // 0x004eb620
  f.remove_child = [](Handle p, Handle c) { zcls::node_remove_child(zn(p), zn(c)); };  // 0x004ebb40
  f.attach = [](Handle n) {                                                              // [0x00608c84] vt+0
    if (zcls::Node* w = world_hooks().world) zcls::node_add_child(w, zn(n));
  };
  f.set_flags2c = [](Handle n, int32_t bit, int32_t on) {  // 0x004eb8a0, 0x004eb8e0
    const uint32_t v = zn(n)->get<uint32_t>(0x2c);
    zn(n)->put<uint32_t>(0x2c, (v & ~(1u << bit)) | (static_cast<uint32_t>(on & 1) << bit));
  };
  f.set_pose = [](Handle n, const float m[12]) {  // 0x00461d30
    zmth::Mat34 mat;
    std::memcpy(&mat, m, sizeof mat);
    float a[3];
    zmth::matrix_to_euler(mat, a);  // 0x00523bc0
    zcls::object3d_set_translation(zn(n), m[9], m[10], m[11]);
    zcls::object3d_set_rotation(zn(n), a[0], a[1], a[2]);
  };
  f.add_child = [](Handle p, Handle c) { zcls::node_add_child(zn(p), zn(c)); };  // 0x004eb9b0
  f.clear_ids = [](Handle n) {  // 0x00462000
    struct Clear {
      static void run(zcls::Node* x) {
        x->put<uint32_t>(0x28, 0);
        x->owner = nullptr;
        x->damage = nullptr;
        for (int32_t i = 0; i < x->child_count(); ++i) run(x->children[i]);
      }
    };
    Clear::run(zn(n));
  };
  f.set_node = [](MechPart& p, Handle n) { part_set_node(p, n); };          // vt+0x0c, 0x00460190
  f.mark = [](MechPart& p, int32_t id) { part_mark_nodes(p, id, nullptr); };  // 0x00460050(id, 0)
  f.opacity_used = [](Handle n) {  // 0x004fbb10
    uint32_t on = 0;
    return zcls::object3d_opacity_used(zn(n), &on) == 0 && on != 0;
  };
  f.use_opacity = [](Handle n, int32_t on) { zcls::object3d_use_opacity(zn(n), on); };  // 0x004fbb90
  f.opacity = [](Handle n) {                                                              // 0x004fbaa0
    float v = 0.0f;
    zcls::object3d_get_opacity(zn(n), &v);
    return v;
  };
  f.set_opacity = [](Handle n, float v) { zcls::object3d_set_opacity(zn(n), v); };  // 0x004fba20

  f.instant_action = [] { return mission_game_type() == 2; };  // 0x00479460
  // Tearing parts and mechs down (mech_parts.cpp release, Mech::release).
  FreeCalls& fr = free_calls();
  fr.stop_effect = [](Handle e) { if (e) zeff::anim_stop(static_cast<zeff::AnimDef*>(e)); };  // 0x00506ca0
  fr.rename = [](Handle n, const char* name) { zcls::node_set_name(zn(n), name); };            // 0x004eb300
  fr.set_flags2c = fly_calls().set_flags2c;
  fr.clear_ids = [](Handle n) {  // 0x004600b0: +0x28 and +0x40, not the handlers
    struct Clear {
      static void run(zcls::Node* x) {
        x->put<uint32_t>(0x28, 0);
        x->owner = nullptr;
        for (int32_t i = 0; i < x->child_count(); ++i) run(x->children[i]);
      }
    };
    Clear::run(zn(n));
  };
  fr.debris_free = [](Handle n) {  // 0x00413db0
    if (zn(n)->parent_count() != 0) zcls::node_remove_child(zn(n)->parents[0], zn(n));
    zcls::tree_destroy(zn(n));
  };
  fr.decals_free = [](Handle d) { delete static_cast<Skin*>(d); };  // 0x00473f40
  fr.viewed = [](Mech& m) { return program_view_target() == static_cast<void*>(&m); };
  fr.view_detach = [] { if (PlayerView* v = program_view()) v->attach(nullptr, nullptr); };  // 0x0040adf0(0, 0)
  fr.torso_cockpit_off = [](Mech& m) { if (m.torso) m.torso->set_cockpit(false); };  // 0x0041ce10(0)
  fr.is_child = [](Handle p, Handle c) {  // 0x004600e0(c, 0, 0): a direct child
    for (int32_t i = 0; p && i < zn(p)->child_count(); ++i)
      if (zn(p)->children[i] == zn(c)) return true;
    return false;
  };
  fr.first_parent = [](Handle n) -> Handle { return zn(n)->parent_count() > 0 ? zn(n)->parents[0] : nullptr; };
  fr.remove_child = [](Handle p, Handle c) { zcls::node_remove_child(zn(p), zn(c)); };
  fr.add_child = [](Handle p, Handle c) { zcls::node_add_child(zn(p), zn(c)); };
  fr.mark = [](MechPart& p, int32_t id, Handle n) { part_mark_nodes(p, id, n); };
  fr.cockpit_node = [](MechData& p) -> Handle { return p.cockpit_model ? p.cockpit_model->node : nullptr; };  // +0x228 +4
  fr.handlers_off = [](Mech& m) {  // the other model under the node while its handlers go
    zcls::Node* other = zn(m.x690 == 0 ? m.lod_node : m.model_node);
    zcls::node_add_child(zn(m.node), other);
    zwep::remove_handlers(m.node);  // 0x00583400
    zcls::node_remove_child(zn(m.node), other);
  };
  fr.controls_free = mission_controls_free;
  fr.node_free = [](Mech& m) {
    zcls::Node* n = zn(m.node);
    if (n->parent_count() == 1) zcls::node_remove_child(n->parents[0], n);
    for (int32_t i = 0; i < n->child_count(); ++i) {  // as the exe: a removal moves the next one into i
      zcls::Node* c = n->children[i];
      if (c == zn(m.model_node) || c == zn(m.lod_node)) zcls::node_remove_child(n, c);
    }
    zcls::node_set_name(n, "deletedmech");  // 0x005a84c0
    zcls::tree_destroy(n);
    zcls::tree_destroy(zn(m.model_node));
    zcls::tree_destroy(zn(m.lod_node));
    m.model_node = m.lod_node = nullptr;
  };
  fr.entity_free = [](Mech& m) {  // 0x005a7f70: off zEntity's list
    mech_objects().erase(&m);
    auto& list = ai::entity_list();
    for (auto it = list.begin(); it != list.end();)
      it = g_w.object_of(g_w.engine, *it) == &m ? list.erase(it) : std::next(it);
  };

  // The level of detail's model switch (mech_lod.cpp) on zClass.
  LodCalls& lc = lod_calls();
  lc.player_target = [](Mech& player) -> Mech* {  // 0x004e6ff0 on +0x6c: the contact's vehicle, a mech
    const ai::Handle seen = player.target ? player.target->seen : 0;
    return seen ? entity_mech(seen) : nullptr;
  };
  lc.remove_child = [](Handle p, Handle c) { zcls::node_remove_child(zn(p), zn(c)); };
  lc.add_child = [](Handle p, Handle c) { zcls::node_add_child(zn(p), zn(c)); };
  lc.find = [](Handle n, const char* name) -> Handle { return zcls::node_find_in_tree(zn(n), name); };  // 0x004f9d60
  lc.node_set_active = [](Handle n, int32_t on) { zcls::node_set_active(zn(n), on); };
  lc.node_set_matrix = [](Handle n, const float* m) { zcls::object3d_set_matrix(zn(n), m); };
  lc.set_node = [](MechPart& p, Handle n) { part_set_node(p, n); };
  lc.stop_effect = [](Handle e) { if (e) zeff::anim_stop(static_cast<zeff::AnimDef*>(e)); };
  lc.jets_effect = [](Mech& m) { return canned_effect(*m.pilot, 1, nullptr, nullptr, nullptr); };

  // The dead mechs (mech_dead.cpp).
  update_calls().add_dead = [](Mech& m) { dead_add(m); };  // 0x0041e350 on 0x00605510
  DeadCalls& dc = dead_calls();
  dc.networked = [] { return mission_game_type() == 3; };       // 0x00479480
  dc.instant_action = [] { return mission_game_type() == 2; };  // 0x00479460
  dc.node_set_active = [](Handle n, int32_t on) { zcls::node_set_active(zn(n), on); };
  dc.use_opacity = [](Handle n, int32_t on) { zcls::object3d_use_opacity(zn(n), on); };
  dc.set_opacity = [](Handle n, float v) { zcls::object3d_set_opacity(zn(n), v); };
  dc.ia_tally = [](int32_t field, int32_t kills) {  // the instant action game is not the program's
    if (g_w.unported) g_w.unported(g_w.engine, "ia game +0x4c..+0x54", {field, kills});
  };
  dc.sensors_forget = [](Mech& m) {  // the AI's (0x004d5100, 0x004aa4a0)
    if (g_w.unported) g_w.unported(g_w.engine, "0x004aa6b0", {g_w.handle_of(g_w.engine, &m)});
  };
  dc.free = [](Mech& m) { mech_delete(&m); };

  // The kill credit: a mech killer's vtable +0x80 is on_kill (0x0041b420);
  // other killers (the AI's vehicles and turrets) are not ported.
  d.killer_notify = [](Handle killer, Mech& victim) {
    if (entity_is_mech(killer)) return static_cast<Mech*>(killer)->on_kill(&victim);
    if (g_w.unported) g_w.unported(g_w.engine, "killer vt+0x80", {});
  };
  d.salvage_mech = [](Handle m) {  // 0x00472f00 on the mission's store
    SalvageMech v = salvage_view(*static_cast<Mech*>(m));
    add_salvage(mission_salvage(), v, salvage_calls());
  };
  d.mission_player_died = mission_lose;                            // 0x0048f9e0 on 0x006098d0
  d.view_mode = [] { return static_cast<int>(program_view_mode_id()); };  // [0x005f4c8c] +0x58 +4
  d.view_camera = [](int mode) { program_view_set_camera(mode); };  // 0x0040ae70
  d.position = [](Mech& m) -> const Vec3* {                         // 0x005a85e0
    static Vec3 at;
    at = mech_position(m);
    return &at;
  };
  d.node_update = [](MechPart& p) {                                 // 0x004ed530 on the part's node
    float at[3];
    if (p.node) zcls::node_world_position(zn(p.node), at);
  };
  // Its world 3x4, object3d +0x60 (0x00461cf0 copies node +0x38 +0x60); a
  // part's node is always an object3d, as the exe assumes.
  d.node_matrix = [](MechPart& p) -> const float* {
    return static_cast<zcls::Object3d*>(zn(p.node)->data)->floats(0x60);
  };
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
  update_calls().purge = [](Mech& m, float dt) { m.fade_debris(dt); };  // 0x00413cb0
  update_calls().set_volume = [](Sound s, float v) {  // 0x0056f370
    if (s) zsnd::set_voice_volume(static_cast<zsnd::VoiceBase*>(s), v);
  };
  update_calls().set_position = [](Sound s, const Vec3* at) {  // 0x00571f30(at, 0, 0): a group's voice is skipped there
    auto* v = static_cast<zsnd::VoiceBase*>(s);
    if (v && at && v->kind == 0) zsnd::update_3d(static_cast<zsnd::Voice*>(v), &at->x, nullptr, 0);
  };
  mv.stop_effect = [](Handle e) { if (e) zeff::anim_stop(static_cast<zeff::AnimDef*>(e)); };  // 0x00506ca0
  mv.pilot_effect = [](Mech& m, int slot, Handle node) {  // 0x00432830(slot, node, zero, zero): the jets' flames
    return canned_effect(*m.pilot, slot, node, nullptr, nullptr);
  };
  mv.weapons_enable = [](Mech& m, int on) {                                                    // 0x004b0bf0 on +0xd4
    if (auto* wm = static_cast<WeaponManager*>(m.ammo)) wm->set_online(on != 0);
  };
  HeatCalls& hc = heat_calls();
  hc.game_type = mission_game_type;  // [0x0060861c] +0x6c, vt+0x2c
  hc.throttle = [](Mech& m) { return m.controls ? m.controls->throttle : 0.0f; };  // +0x37c GetThrottle
  hc.play_sound = [](Mech& m, int id) -> Sound { return pilot_play(m, id, 1.0f, nullptr); };
  hc.play_sound_at = [](Mech& m, int id) -> Sound {
    const Vec3 at = mech_position(m);
    return pilot_play(m, id, 1.0f, &at);
  };
  hc.stop_sound = [](Sound s) { if (s) zsnd::stop(static_cast<zsnd::VoiceBase*>(s)); };  // 0x0056e9f0
  // The cockpit sounds (playtest #17): the fire modes, the gyro, MASC, the
  // jettison, the coolant, the hits (0x0041e770 at 1.0; 0x0041e7a0(i, 0)).
  hc.play_mech_sound = [](int i) { cockpit_sound_play(i, 1.0f); };
  update_calls().stop_cockpit_sound = [](int i) { cockpit_sound_stop(i, 0); };
  CollideCalls& c = collide_calls();
  c.node_touchable = [](Handle n) { return handler_touchable(n); };  // node +0xbc above 1 with slot 1 (0x004230a0)
  // The ceiling's crush (0x00423600): the box of what the mech stands under
  // (0x004ec9c0). Unwired before, the crush never happened in play.
  c.hit_box = [](Handle n) { return zcls::node_box(static_cast<zcls::Node*>(n)); };
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
    update_calls().moved = nullptr, update_calls().purge = nullptr, update_calls().add_dead = nullptr, update_calls().set_volume = nullptr, update_calls().set_position = nullptr;
    command_calls() = CommandCalls{};
    part_calls() = PartCalls{};
    move_calls() = MoveCalls{};
    heat_calls().throttle = nullptr, heat_calls().game_type = nullptr, heat_calls().play_sound = nullptr, heat_calls().play_sound_at = nullptr;
    heat_calls().stop_sound = nullptr, heat_calls().play_mech_sound = nullptr;
    update_calls().stop_cockpit_sound = nullptr;
    collide_calls().touch = nullptr, collide_calls().node_touchable = nullptr, collide_calls().hit_box = nullptr;
    unwire_hud();
    unwire_weapons();
    set_mech_build_hooks(nullptr);
    set_shade_hooks(nullptr);
    update_calls().x68c_update = nullptr;
    free_calls().ref_free = nullptr, free_calls().ref_delete = nullptr;
    damage_calls().node_unlink = nullptr;
    set_mech_data_hooks(nullptr);
    set_launch_hooks(nullptr);
    return;
  }
  g_w = *w;
  set_shade_hooks(&g_shade_hooks);
  update_calls().x68c_update = [](Handle s, float dt) { shade_update(*static_cast<SunShade*>(s), dt); };
  free_calls().ref_free = [](Handle ref, Handle n) {  // 0x004e66a0, then the delete
    shade_level_reset(*static_cast<ShadeLevel*>(ref), WiredShade::id(n));
    delete static_cast<ShadeLevel*>(ref);
  };
  free_calls().ref_delete = [](Handle s) { delete static_cast<SunShade*>(s); };  // vt+0 (0x004e6220)
  damage_calls().node_unlink = [](Handle ref, Handle n) {                        // 0x004e66a0
    shade_level_reset(*static_cast<ShadeLevel*>(ref), WiredShade::id(n));
  };
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
void mech_canned_nodes_free() { canned_nodes_free(); }
void mech_destroy_part(void* mech, int32_t id) { static_cast<Mech*>(mech)->destroy_part(id); }
// Mech3.exe 1.2 @ 0x004782c0 with 0: the pilot lets its mech go, keeping
// the weapons' fire mode (+0xd4 +0x1c) for its +0x80; the mech globals the
// launch set (pilot_set_mech) go with it.
bool mech_player_release(void* mech, int32_t* fire_mode) {
  auto* m = static_cast<Mech*>(mech);
  const auto* wm = m ? static_cast<const WeaponManager*>(m->ammo) : nullptr;
  if (wm) *fire_mode = wm->mode;
  world().mission_player = world().player = nullptr;
  weapon_world().mission_player = weapon_world().player = nullptr;
  return wm != nullptr;
}

void* mission_player_mech() { return world().mission_player; }

void mech_dead_tick(float dt) { dead_tick(dt); }
void mech_dead_clear() { dead_mechs().clear(); }

bool mech_free(void* obj) {
  if (!entity_is_mech(obj)) return false;
  mech_delete(static_cast<Mech*>(obj));
  mech_objects().erase(obj);  // a mech without a node keeps its zEntity entry (as the exe's)
  return true;
}
void mech_kill(void* mech, void* killer) {
  auto* m = static_cast<Mech*>(mech);
  if (killer) m->attacker = killer;
  m->torso->destroy(0, 1);
}

std::vector<std::string>& mech_parts_fx_log() { return g_parts_fx; }

bool launched_mech(LaunchedMech* out) { return describe_mech(mech3::launched_mech(), out); }

bool describe_mech(const void* mech, LaunchedMech* out) {
  if (!entity_is_mech(mech)) return false;  // not a mech (or gone): nothing to read
  const Mech* m = static_cast<const Mech*>(mech);
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
    if (const WeaponGroup* g = wm->group >= 0 && wm->group < 5 ? wm->groups[wm->group] : nullptr) s.weapon = g->current_weapon;
  }
  s.jets = m->equipment[11];
  s.jets_firing = m->jump_jets_firing;
  s.jet_flame = m->jet_effect != nullptr;
  s.crouched = m->crouched;
  s.alive = m->alive_time;
  s.anim_state = m->anim_state;
  for (int i = 0; i < 4; ++i) s.blocked[i] = m->blocked[i];
  s.attacker = m->attacker;
  s.action = m->actions.empty() ? 0 : m->actions.front();
  if (auto* n = zn(m->node)) s.deadmech = std::strcmp(n->name(), "deadmech") == 0;
  s.aim_torso = m->torso && m->target_node == m->torso->node;
  for (int i = 0; i < 6; ++i) s.kills[i] = m->x3f4[i];
  s.detail = m->detail, s.lod = m->x690;
  for (const Mech* d : dead_mechs()) s.dead_listed |= d == m;
  s.debris = static_cast<int32_t>(m->x6d0.size());
  s.debris_in_world = !m->x6d0.empty();
  for (Handle d : m->x6d0)
    if (!(zn(d)->parent_count() > 0 && zn(d)->parents[0] == world_hooks().world)) s.debris_in_world = false;
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
    Mech* m = entity_mech(h);
    if (!m) {  // another entity's vt+0x34: none is the port's yet
      if (g_w.unported) g_w.unported(g_w.engine, "entity vt+0x34", {h});
      return;
    }
    if (m->x09) m->update(dt);  // vt+0x34
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
std::vector<DebriefMech> mech_debrief_lance() {
  std::vector<DebriefMech> r;
  for (const ai::Handle h : ai::entity_list()) {
    Mech* m = entity_mech(h);
    if (!m) continue;
    DebriefMech d;
    d.mech = m;
    d.player = m == world().mission_player;
    d.team = m->team;
    d.type = m->pilot ? m->pilot->roster : -1;
    for (int i = 0; i < 6; ++i) d.stats[i] = static_cast<int32_t>(m->x3f4[i]);
    r.push_back(d);
  }
  return r;
}
namespace {
// A ported mech as 0x00472f00 reads it.
// The salvage's own calls on the ported mechs.
const SalvageCalls& salvage_calls() {
  static const SalvageCalls c = [] {
    SalvageCalls k;
    k.rand = [] { return static_cast<int32_t>(crt::msvc_rand()); };
    k.empty_bin = [](const SalvageMech& v, int32_t bin) {
      const_cast<AmmoBin*>(static_cast<const AmmoBin*>(v.owner_bins[bin]))->set_amount(0);
    };
    return k;
  }();
  return c;
}

SalvageMech salvage_view(Mech& m) {
  SalvageMech v;
  v.has_data = m.pilot != nullptr;
  v.type = m.pilot ? m.pilot->roster : -1;
  // 0x0041df10: mech_tech of the definition's +0x00, which is its roster
  // index (MechDfn::index, the cache 0x004846a0 is by roster); a built
  // mech's definition is already loaded, so the cache's lazy load is moot.
  v.tech = mech_tech(v.type);
  v.torso_gone = m.torso && m.torso->destroyed;
  v.head_gone = m.head && m.head->destroyed;
  for (int i = 0; i < 2; ++i) v.leg_gone[i] = m.legs[i] && m.legs[i]->destroyed;
  if (const auto* wm = static_cast<const WeaponManager*>(m.ammo)) {
    v.has_manager = true;
    std::vector<const AmmoBin*> bins;
    for (uint32_t i = 0; i < wm->count && i < 16; ++i) {
      SalvageWeapon w;
      const Weapon* x = wm->weapons[i];
      if (x) {
        w.present = true;
        w.type = x->def && x->def->type ? x->def->type->id : -1;
        w.has_item = x->item != nullptr;
        w.item_tech = x->item ? x->item->tech : 0;
        w.beam = x->is_beam();
        if (x->bin) {
          size_t k = 0;
          while (k < bins.size() && bins[k] != x->bin) ++k;
          if (k == bins.size()) {
            bins.push_back(x->bin);
            v.bins.push_back(x->bin->left());
          }
          w.bin = static_cast<int32_t>(k);
        }
      }
      v.weapons.push_back(w);
    }
    v.owner_bins.assign(bins.begin(), bins.end());
  }
  v.config = m.config;
  for (int id = 0; id < 16; ++id) {
    const MechPart* p = part_by_id(m, id);
    v.parts[id] = p ? (p->destroyed ? 2 : 1) : 0;
  }
  v.sink_tech = m.heat_sinks.tech;
  v.sinks = m.heat_sinks.count;
  v.doubles = m.heat_sinks.doubles;
  return v;
}
}  // namespace

void mech_lance_salvage(Store& own, bool team) {
  std::vector<SalvageEntity> list;
  Mech* player = world().mission_player;
  for (const ai::Handle h : ai::entity_list()) {
    Mech* m = entity_mech(h);
    SalvageEntity e;
    if (m) {
      e.mech = true;
      e.player = m == player;
      e.team = m->team;
      e.m = salvage_view(*m);
    }
    list.push_back(std::move(e));
  }
  lance_salvage(mission_salvage(), own, list, player ? player->allies : 0, team, salvage_calls());

}

// Departures: a mech without a pilot (+0x684), a loadout (+0x3ec) or one of
// the parts reads as type -1, no armor, nothing left, where the exe would
// read through null.
bool mech_lance_armor(void* mech, int32_t* type, int32_t armor[11], float left[11]) {
  auto* m = static_cast<Mech*>(mech);
  if (!m) return false;
  *type = m->pilot ? m->pilot->roster : -1;
  if (m->config)
    for (int i = 0; i < 11; ++i) armor[i] = m->config->armor[i];
  // Bar k: part, section (0x0047ef10's order).
  static const int8_t kParts[11][2] = {{1, 0}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5},
                                       {6, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}};
  for (int k = 0; k < 11; ++k) {
    const MechPart* p = part_by_id(*m, kParts[k][0]);
    left[k] = p ? p->get_armor(kParts[k][1]) : 0.0f;
  }
  return true;
}
const char* mech_brain_name(void* mech) {
  auto* m = static_cast<Mech*>(mech);
  auto* c = m ? dynamic_cast<AiControls*>(m->controls) : nullptr;  // vt+0x140, then vt+0x54
  return c && c->brain ? c->brain->params.pilot : nullptr;
}
void* mech_mission_player() { return world().mission_player; }
bool entity_is_mech(const void* o) { return o && mech_objects().count(o) != 0; }

void mech_entities_forget() { mech_objects().clear(); }

void* mech_shade_of(void* mech) { return entity_is_mech(mech) ? static_cast<Mech*>(mech)->x68c : nullptr; }

const void* mech_shadow(const void* mech) {
  return entity_is_mech(mech) ? static_cast<const Mech*>(mech)->shadow : nullptr;
}

// Mech3.exe 1.2 @ 0x004814e0, on the program's mechs: within 100 m of the
// player (the player's own always, 64x64; others 32x32) a mech keeps a caster
// (+0xcc), redrawn along the sun onto its centre ski's ground; further away it
// is freed. Entries that are not mechs are skipped until vehicles are ported
// (they cast in the exe; docs/vehicles.md).
void update_mech_shadows() {
  if (!zmod::shadows_on()) return;  // 0x00526a80
  auto* sun = static_cast<zcls::Node*>(mission_sun());
  if (!sun || !sun->data) return;
  auto* light = static_cast<zcls::Light*>(sun->data);
  Mech* player = world().mission_player;
  for (ai::Handle h : ai::entity_list()) {
    Mech* m = entity_mech(h);
    if (!m) continue;
    const Vec3* pos = entity_position(*m);
    if (!pos) continue;
    float fade = 1.0f;
    if (m != player) {
      const Vec3* p = player ? entity_position(*player) : nullptr;
      if (!p) {
        fade = 0.0f;  // no player to measure from (the exe always has one)
      } else {
        const float dx = p->x - pos->x, dy = p->y - pos->y, dz = p->z - pos->z;
        const float xx = dx * dx, yy = dy * dy, zz = dz * dz;
        const float xy = xx + yy;  // 0x0051dfa0's order
        fade = shadow_fade(xy + zz);
      }
    }
    if (!(fade > 0.0f)) {
      m->release_shadow();
      continue;
    }
    if (!m->shadow) {
      const int32_t size = m != player ? 32 : 64;
      m->shadow = zmod::shadow_new();
      zmod::shadow_set_size(m->shadow, size, size);
      zmod::shadow_set_node(m->shadow, zn(m->node));
      zmod::shadow_enable(m->shadow, 1);
    }
    const float clouds = zmod::clouds_at(pos->x, pos->z);
    const uint8_t g = shadow_grey(*light->floats(0xa0) * clouds, *light->floats(0x9c) * clouds,
                                  *light->floats(0x88), fade);
    zmod::shadow_set_color(m->shadow, zvid::pack_bytes(g, g, g));
    // vt+0x50 is the mech itself: its skis' +0x13c, the centre probe's ground.
    const float ground = m->skis ? m->skis->ground[6] : pos->y;
    const float at[3] = {pos->x, pos->y, pos->z};
    zmod::shadow_set_position(m->shadow, at);
    zmod::shadow_draw(m->shadow, light->floats(0x84), ground);
    zmod::shadow_upload(m->shadow);
  }
}

namespace {
// A zEntity entry's mech (vt+0x2c then vt+0x50), none for other entities.
Mech* entity_mech(ai::Handle h) {
  void* o = h ? g_w.object_of(g_w.engine, h) : nullptr;
  return entity_is_mech(o) ? static_cast<Mech*>(o) : nullptr;
}
}  // namespace
uint32_t mech_player_allies() { return world().mission_player ? world().mission_player->allies : 0; }
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
float mech_speed_at(void* mech, float share) { return static_cast<Mech*>(mech)->speed_at(share); }
float mech_stopping_distance(void* mech, float speed) { return static_cast<Mech*>(mech)->stopping_distance(speed); }
void mech_forward(void* mech, float out[3]) {
  const Mech* m = static_cast<Mech*>(mech);
  out[0] = m->forward.x, out[1] = m->forward.y, out[2] = m->forward.z;
}
int32_t mech_jump_jets(void* mech) { return static_cast<Mech*>(mech)->equipment[11]; }
float mech_vertical_speed(void* mech) { return static_cast<Mech*>(mech)->vec_3c.y; }
void* mech_weapon_manager(void* mech) { return mech ? static_cast<Mech*>(mech)->ammo : nullptr; }
void* wm_current_weapon(void* wm) { return wm ? static_cast<WeaponManager*>(wm)->current() : nullptr; }
// FIX (bug): the exe's firing clears a missing manager's aim through null
// (a vehicle without weapons standing down).
void wm_clear_aim(void* wm) {
  if (wm) static_cast<WeaponManager*>(wm)->set_aim(nullptr);
}
void wm_alpha_strike(void* wm, float seconds) {
  if (wm) static_cast<WeaponManager*>(wm)->alpha_strike(seconds);
}
bool weapon_type_ready(void* w) { return w && static_cast<Weapon*>(w)->type_ready(); }
float weapon_reload(void* w) { return w ? static_cast<Weapon*>(w)->reload : 0.0f; }
int32_t weapon_ammo(void* w) { return w ? static_cast<Weapon*>(w)->ammo() : 0; }
// FIX (bug): a weapon without a definition (or a type) would be written
// through null; it holds nothing.
void weapon_hold_for_flight(void* w) {
  auto* x = static_cast<Weapon*>(w);
  if (x && x->def && x->def->type) x->def->type->hold_for_flight();
}
bool mech_active(void* mech) { return mech && static_cast<Mech*>(mech)->active; }
void mech_torso_centre(void* mech, int32_t how) {
  if (auto* m = static_cast<Mech*>(mech); m && m->torso) m->torso->centre(how);
}
bool mech_set_heat(void* mech, float heat) {
  if (!entity_is_mech(mech)) return false;
  static_cast<Mech*>(mech)->heat = heat;
  return true;
}
bool mech_hit_part(void* mech, int32_t id, float damage) {
  if (!entity_is_mech(mech)) return false;
  for (MechPart* p : static_cast<Mech*>(mech)->parts)
    if (p && p->id == id) {
      p->take_hit(nullptr, nullptr, damage, nullptr);
      return true;
    }
  return false;
}
bool mech_ceiling_hit(void* mech, void* node, float out[7]) {
  Mech* m = entity_is_mech(mech) ? static_cast<Mech*>(mech) : nullptr;
  if (!m || !m->head || !m->torso || !m->hip) return false;
  CollisionHit h{};
  h.hit.node = static_cast<zcls::Node*>(node);
  m->ceiling_response(h);
  out[0] = m->head->get_internal(0);
  for (int s = 0; s < 3; ++s) out[1 + s] = m->torso->get_internal(s);
  out[4] = m->hip->offset.y;  // the duck (0x004622a0)
  out[5] = collide_calls().hit_box && collide_calls().hit_box(node) ? 1.0f : 0.0f;
  out[6] = m->crouch_depth;
  return true;
}
bool mech_textures(void* mech, MechTextures* out) {
  Mech* m = entity_is_mech(mech) ? static_cast<Mech*>(mech) : nullptr;
  if (!m) return false;
  auto walk = [&](auto& self, zcls::Node* n, std::vector<std::string>& names) -> void {
    if (!n) return;
    if (zmod::Model* md = n->model)
      for (uint32_t k = 0; k < md->count(0x10); ++k)
        if (const zmod::Material* mat = md->polygons[k].material; mat && (mat->flags & 1) && mat->texture) {
          names.push_back(mat->texture->name);
          out->loaded += mat->texture->image != nullptr;
        }
    for (int32_t k = 0; k < n->child_count(); ++k) self(self, n->children[k], names);
  };
  walk(walk, static_cast<zcls::Node*>(m->model_node), out->names);
  walk(walk, static_cast<zcls::Node*>(m->lod_node), out->lod);
  if (const auto* s = static_cast<const Skin*>(m->decals)) out->skin = s->name ? s->name : "";
  return true;
}
bool mech_marker_heights(void* mech, float out[4]) {
  Mech* m = entity_is_mech(mech) ? static_cast<Mech*>(mech) : nullptr;
  if (!m || !m->node || !m->target_node || !m->centroid) return false;
  float p[3];
  zcls::node_world_position(static_cast<zcls::Node*>(m->node), p), out[0] = p[1];
  zcls::node_world_position(static_cast<zcls::Node*>(m->target_node), p), out[1] = p[1];
  zcls::node_world_position(static_cast<zcls::Node*>(m->centroid), p), out[2] = p[1];
  out[3] = m->height;
  return true;
}
int32_t mech_controls_view(void* mech) {
  Mech* m = entity_is_mech(mech) ? static_cast<Mech*>(mech) : nullptr;
  return m && m->controls ? m->controls->view : -1;
}
bool mech_hold_throttle(void* mech, float v) {
  Mech* m = entity_is_mech(mech) ? static_cast<Mech*>(mech) : nullptr;
  if (!m || !m->controls) return false;
  m->controls->throttle_target = v;
  m->controls->throttle_held = true;
  return true;
}
float mech_throttle_target(void* mech) {
  Mech* m = entity_is_mech(mech) ? static_cast<Mech*>(mech) : nullptr;
  return m && m->controls ? m->controls->throttle_target : 0.0f;
}
void* mech_node_of(void* mech) { return entity_is_mech(mech) ? static_cast<Mech*>(mech)->node : nullptr; }
void* mech_cockpit_node(void* mech) {
  Mech* m = entity_is_mech(mech) ? static_cast<Mech*>(mech) : nullptr;
  return m && m->pilot && m->pilot->cockpit_model ? m->pilot->cockpit_model->node : nullptr;
}
void* mech_arm_node(void* mech, int side) {
  Mech* m = entity_is_mech(mech) ? static_cast<Mech*>(mech) : nullptr;
  return m && m->arms[side & 1] ? m->arms[side & 1]->node : nullptr;
}
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
