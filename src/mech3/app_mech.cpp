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
#include "gamez/zclass/object3d.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/list.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zinterp/zinterp.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/mech3_interp.h"
#include "mech3/mech_anim.h"
#include "mech3/mech_build.h"
#include "mech3/mech_config.h"
#include "mech3/mech_data.h"
#include "mech3/mech_lib.h"
#include "mech3/mech_names.h"
#include "mech3/mech_reader.h"
#include "mech3/mission_launch.h"
#include "mech3/mission_world.h"
#include "mech3/sensors.h"
#include "mech3/weapon.h"

namespace mech3 {

int32_t invincible();  // 0x0040cf20, mech3/options.h

namespace {

MechWiring g_w;
zcls::Node* zn(Handle h) { return static_cast<zcls::Node*>(h); }

// The mech data's calls (mech_data.cpp): the ports, and the engine for what
// has none (the cameras, the effects by name, the game type, the error box).
class WiredData : public MechDataHooks {
 public:
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return g_w.unported ? g_w.unported(g_w.engine, key, args) : 0;
  }
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
  int32_t camera_mode() override { return 0; }
  void set_camera_mode(int32_t) override {}
  // The pilot's vehicle is the mech (as the exe's 0x004782c0 sets it).
  void pilot_set_mech(Handle p, Mech& m) override {
    unported("0x004782c0", {low(p), 0});
    set_field32(p, 0x14, g_w.handle_of(g_w.engine, &m));
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

void wire_mech_build(const MechWiring* w) {
  if (!w) {
    g_w = MechWiring{};
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

bool launched_mech(LaunchedMech* out) { return describe_mech(mech3::launched_mech(), out); }

bool describe_mech(const void* mech, LaunchedMech* out) {
  const Mech* m = static_cast<const Mech*>(mech);
  if (!m) return false;
  LaunchedMech s;
  s.roster = m->pilot->roster;
  const MechPart* parts[] = {m->hip, m->torso, m->head, m->arms[0], m->arms[1], m->legs[0], m->legs[1]};
  for (const MechPart* p : parts) s.parts += p && p->node;
  for (const Arm* a : m->arms) s.parts += a && a->hand && a->hand->node;
  for (const Leg* l : m->legs) {
    if (!l) continue;
    s.parts += l->calf && l->calf->node;
    s.parts += l->foot && l->foot->node;
    if (l->foot) s.toes += static_cast<int32_t>(l->foot->toe_count);
  }
  if (const auto* wm = static_cast<const WeaponManager*>(m->ammo)) s.weapons = static_cast<int32_t>(wm->count);
  s.sinks = m->heat_sinks.count;
  s.height = m->height;
  s.max_speed = m->max_speed;
  s.torso_armor_scale = m->torso ? m->torso->armor_scale : 0;
  s.team = m->team;
  s.skis = m->skis != nullptr;
  s.ams = m->ams != nullptr;
  s.motions = m->anims[4] && m->anims[8];
  s.powering_down = m->power_down_pending || m->powered_down;
  if (m->transform) s.pos[0] = m->transform[9], s.pos[1] = m->transform[10], s.pos[2] = m->transform[11];
  s.yaw_sin = m->orientation[2];
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
