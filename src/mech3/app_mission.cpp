// The mission load's hooks wired to the ports (docs/mission-load.md), and the
// pieces of the program a mission needs to come up through the real Load and
// Core states: the pilot's game for a debug launch (our addition), the view
// the Core state draws through. Not original code: it stands where the exe's
// other modules will be. What has no port is logged once by address.
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/light.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zclass/sound.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zdeclient/zdec_crater.h"
#include "gamez/zdeclient/zdec_init.h"
#include "gamez/zgeometry/zgeo_model.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zeffect/zeff_init.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zinterp/zinterp.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zmodel/gmod_init.h"
#include "gamez/zmodel/gmod_light.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zmodel/gmod_puff.h"
#include "gamez/znetwork/znet_object.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zutl_msg.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zvideo/zvid_dd.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"
#include "gamez/zweapon/zwep_damage.h"
#include "gamez/zweapon/zwep_init.h"
#include "gamez/zutil/ztimer.h"
#include "gamez/zweapon/zweapon.h"
#include "mech3/message_panel.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_hud.h"
#include "mech3/app_mech.h"
#include "mech3/app_sensors.h"
#include "mech3/app_states.h"
#include "mech3/app_view.h"
#include "mech3/player_view.h"
#include "mech3/mech3_interp.h"
#include "mech3/mission.h"
#include "mech3/mech_config.h"
#include "mech3/mech_control.h"
#include "mech3/mech_reader.h"
#include "mech3/mech_skin.h"
#include "mech3/mission_world.h"
#include "mech3/options.h"
#include "mech3/player.h"
#include "mech3/sensors.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_target.h"
#include "mech3/zrdraiv.h"
#include "mech3/zainetedit.h"
#include "mech3/zrdrvalve.h"
#include "platform/render3d.h"

namespace mech3 {

// From mech3/mech_names.h and mech3/weapon.h, whose own Engine and Handle
// clash with app_hooks.h's.
int32_t mech_index(const char* name);                        // 0x004845f0
void make_weapon_type(zrdr::Node* node, zwep::WeaponDef* def);  // 0x004b0020
void free_weapon_type(zwep::WeaponDef* def);                     // 0x004b0090
bool launch_player();                                        // 0x00491150 (mech3/mission_launch.h)
const char* mech_name(int32_t mech);                         // 0x00484650

namespace {

GameEngine* g_mission_engine = nullptr;
GameEngine* engine_of_hooks() { return g_mission_engine; }

zvid::D3DTexture g_fallback{};  // the default record (0x0080931c) for textures that fail

zcls::Node* mission_node(GameEngine& e, Handle h) { return static_cast<zcls::Node*>(e.object_of(h)); }

// world.zrd's block through the ported reader 0x00491de0, with the hooks it
// needs: the wind and the clouds to zModel, the sun to the world's light;
// weather (rain, snow) is not ported, so its hooks do nothing.
void read_world_params(const char* file, const char* block, zcls::Node* camera, int32_t type, int32_t number) {
  static int32_t s_type, s_number;
  s_type = type, s_number = number;
  WorldHooks& h = world_hooks();
  h = WorldHooks{};
  h.game_type = [] { return s_type; };
  h.mission_number = [] { return s_number; };
  h.hw_card_flag = [] { return hw_card_flag(); };
  h.world = camera ? zcls::camera_world(camera) : nullptr;
  h.camera = camera;
  zmod::puff_hooks().create_node = [](void*) { return zcls::object3d_create(); };
  h.puffer_tick_reset = zmod::puff_tick_reset;
  h.wind_velocity = zmod::wind_set_velocity;
  h.wind_max_speed = zmod::wind_set_max_speed;
  h.wind_random_accel = zmod::wind_set_random_accel;
  h.wind_random_angvel = zmod::wind_set_random_angvel;
  h.light_get_orientation = [](zcls::Node* l, float* x, float* y, float* z) { zcls::light_get_orientation(l, x, y, z); };
  h.light_set_orientation = [](zcls::Node* l, float x, float y, float z) { zcls::light_set_orientation(l, x, y, z); };
  h.light_set_ambient = [](zcls::Node* l, float v) { zcls::light_set_ambient(l, v); };
  h.light_set_diffuse = [](zcls::Node* l, float v) { zcls::light_set_diffuse(l, v); };
  h.create_snow = [](int32_t) -> Weather* { return nullptr; };
  h.create_rain = [](int32_t) -> Weather* { return nullptr; };
  h.weather_add = [](Weather*) {};
  h.weather_remove = [](Weather*) {};
  h.weather_destroy = [](Weather*) {};
  h.weather_start = [](Weather*, float) {};
  h.weather_stop = [](Weather*) {};
  h.fluctuate_on = zmod::clouds_set_on;
  h.fluctuate_period = zmod::clouds_set_period;
  h.fluctuate_wavelength = zmod::clouds_set_wavelength;
  h.fluctuate_base_amp = zmod::clouds_set_level;
  h.fluctuate_heading = zmod::clouds_set_heading;
  h.software_color = [](const float*) {};
  // The frame's cloud drift (0x004926d0): the camera's rotation, the frame's dt.
  h.camera_angles = []() -> const float* {
    static float none[3] = {};
    GameEngine* g = engine_of_hooks();
    zcls::Node* cam = g ? mission_node(*g, g->mission_camera()) : nullptr;
    return cam && cam->data ? static_cast<zcls::Camera*>(cam->data)->floats(0x20) : none;
  };
  h.frame_dt = [] { return engine_of_hooks() ? engine_of_hooks()->frame_dt() : 0.0f; };
  h.clouds_advance = zmod::clouds_advance;
  MissionWorld& m = missions().world;  // the missions object's first 0x4c bytes
  m = MissionWorld{};
  world_init(m);
  if (!world_read_params(m, file, block)) std::fprintf(stderr, "mw3: no block %s in %s\n", block, file);
}

// The AI mechs made this mission (app_mech.h), with their records' names.
std::vector<AiMechMade> g_ai_mechs;
std::vector<std::string> g_ai_names;

// player.cpp's hooks (docs/mission-start.md): the reader, the records, the
// mech definitions, the nets, zClass, the AI controllers (mech3/zai_brain)
// and the mechs are ported; the other vehicles are not, so their
// constructors are logged and give none (their controllers are deleted).
struct WiredPlayerHooks : PlayerHooks {
  GameEngine* e = nullptr;
  std::map<Handle, int> controls;  // the AI controllers made: 1 a mech's, 2 a vehicle's
  std::set<Handle> params;         // their brains' settings, as vt+0x90 gives them
  std::set<Handle> mechs;          // the AI mechs made
  // Handles are by address and kept for good (GameEngine::handle_of), so an
  // object made where a dead one lived gets its handle: these sets hold this
  // mission's objects only, or a new mech is taken for an old brain's
  // settings (set_field32 then writes AiParams offsets into it).
  void forget() { controls.clear(), params.clear(), mechs.clear(); }
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return e->unported(key, args);
  }
  template <class T>
  T* as(Handle h) const { return static_cast<T*>(e->object_of(h)); }

  void nets_build_all() override {
    g_ai_mechs.clear(), g_ai_names.clear();
    ai::nets_build_all();
  }
  Handle object3d_create() override { return e->handle_of(zcls::object3d_create()); }
  void set_action_priority(Handle node, int32_t p) override { zcls::node_set_priority(as<zcls::Node>(node), p); }
  // The player's tick runs with the action lists (0x004f7d80, the Core
  // state's frame).
  void set_action(Handle node, uint32_t fn) override {
    if (fn == kPlayerTick) zcls::node_set_action(as<zcls::Node>(node), [](zcls::Node*) { player_tick(); });
  }
  Handle reader_load(const char* path) override { return e->handle_of(zrdr::load(path)); }
  Handle reader_find(Handle node, const char* key) override {
    zrdr::Node* n = as<zrdr::Node>(node);
    return n ? e->handle_of(zrdr::find(n, key)) : 0;
  }
  void reader_free(Handle node) override {
    if (zrdr::Node* n = as<zrdr::Node>(node)) zrdr::free_node(n);
  }
  float reader_item_float(Handle node, int32_t i) override { return as<zrdr::Node>(node)->list[i].f; }
  Handle reader_item(Handle node, int32_t i) override { return e->handle_of(&as<zrdr::Node>(node)->list[i]); }
  int32_t game_type() override { return e->launch().type; }
  Handle campaign() override { return e->launch().type == 1 ? e->pilot_game(e->pilot()) : 0; }
  Handle training() override { return e->launch().type == 4 ? e->pilot_game(e->pilot()) : 0; }
  Handle mech_dfn(int32_t mech) override { return e->handle_of(mech3::mech_dfn(mech)); }
  int32_t dfn_config_count(Handle dfn) override {
    const MechDfn* d = as<MechDfn>(dfn);
    return d && d->configs ? static_cast<int32_t>(d->configs->configs.size()) : 0;
  }
  Handle dfn_config(Handle dfn, int32_t i) override { return e->handle_of(as<MechDfn>(dfn)->configs->configs[i]); }
  int32_t dfn_skin_count(Handle dfn) override {
    const MechDfn* d = as<MechDfn>(dfn);
    return d && d->skins ? static_cast<int32_t>(d->skins->skins.size()) : 0;
  }
  Handle dfn_skin(Handle dfn, int32_t i) override { return e->handle_of(as<MechDfn>(dfn)->skins->skins[i]); }
  int32_t mech_index(const char* name) override { return mech3::mech_index(name); }
  Handle znet_find(int32_t id) override { return e->handle_of(znet::find(id)); }
  Handle net_object(Handle o) override { return e->handle_of(dynamic_cast<ai::AiNet*>(as<znet::Object>(o))); }
  Handle net_compiled(Handle o) override { return e->handle_of(as<ai::AiNet>(o)->as_compiled()); }
  Handle alloc(uint32_t size) override { return e->alloc_block(size); }
  void set_field32(Handle h, uint32_t off, uint32_t v) override {
    if (params.count(h)) return as<ai::AiParams>(h)->put<uint32_t>(static_cast<int>(off), v);
    if (mechs.count(h) && mech_set_mask(e->object_of(h), off, v)) return;
    e->poke32(h + off, v);
  }
  uint32_t field32(Handle h, uint32_t off) override {
    if (params.count(h)) return as<ai::AiParams>(h)->get<uint32_t>(static_cast<int>(off));
    uint32_t v = 0;
    if (mechs.count(h) && mech_get_mask(e->object_of(h), off, &v)) return v;
    return e->peek32(h + off);
  }
  // The AI controllers: the port's, over the vehicle's compiled net and its
  // record (the blocks player.cpp allocates for them are not used).
  Handle ai_mech_controls(Handle, Handle net, const ai::AivRecord* rec) override {
    const Handle h = e->handle_of(new AiControls(nullptr, as<ai::AiNet>(net), *rec));
    controls[h] = 1;
    return h;
  }
  Handle alloc_4ac860(uint32_t size) override { return e->alloc_block(size); }
  Handle ai_vehicle_controls(Handle, Handle net, const ai::AivRecord* rec) override {
    const Handle h = e->handle_of(new ai::AiVehicleControls(nullptr, as<ai::AiNet>(net), *rec));
    controls[h] = 2;
    return h;
  }
  Handle controls_interface(Handle ctl) override { return ctl; }
  void controls_delete(Handle ictl) override {
    auto it = controls.find(ictl);
    if (it == controls.end()) return PlayerHooks::controls_delete(ictl);
    if (it->second == 1) {
      AiControls* c = as<AiControls>(ictl);
      if (c->brain) params.erase(e->handle_of(&c->brain->params));
      delete c;
    } else {
      ai::AiVehicleControls* c = as<ai::AiVehicleControls>(ictl);
      params.erase(e->handle_of(&c->brain->params));
      delete c;
    }
    controls.erase(it);
  }
  // vt+0x90 (0x004e8dc0): the brain, whose +0x88 player.cpp holds at 1 for
  // the power-down's update.
  Handle controls_vt90(Handle ictl) override {
    auto it = controls.find(ictl);
    if (it == controls.end()) return PlayerHooks::controls_vt90(ictl);
    ai::Brain* b = it->second == 1 ? as<AiControls>(ictl)->brain.get() : as<ai::AiVehicleControls>(ictl)->brain.get();
    if (!b) return 0;
    const Handle h = e->handle_of(&b->params);
    params.insert(h);
    return h;
  }
  // The mech itself is the port's (mech3/mech_build) on the AI's controller.
  Handle mech_create(int32_t mech, Handle ctl, int32_t config, const char* name) override {
    void* c = e->object_of(ctl);
    if (!c || controls.count(ctl) == 0 || controls[ctl] != 1) return PlayerHooks::mech_create(mech, ctl, config, name);
    void* m = build_ai_mech(mech, static_cast<MechControls*>(static_cast<AiControls*>(c)), config, name);
    if (!m) return 0;
    g_ai_names.emplace_back(name ? name : "");
    g_ai_mechs.push_back({m, c, nullptr});
    const Handle h = e->handle_of(m);
    mechs.insert(h);
    return h;
  }
  // 0x004161c0: the port's mech on the ground at the record's place.
  int32_t mech_place(Handle m, const ai::AivRecord* rec) override {
    void* mech = e->object_of(m);
    if (!mech || !mechs.count(m)) return PlayerHooks::mech_place(m, rec);
    const float pos[3] = {rec->get<float>(0x08), rec->get<float>(0x0c), rec->get<float>(0x10)};
    return place_mech(mech, pos, rec_yaw(rec));
  }
  void mech_power(Handle m, int32_t down) override { mech3::mech_power(e->object_of(m), down); }
  void mech_set_skin(Handle mech, Handle skin) override { set_mech_skin(e->object_of(mech), e->object_of(skin)); }
  Handle mech_dfn_of(Handle mech) override {
    return e->handle_of(const_cast<void*>(mech_definition(e->object_of(mech))));
  }
  void report(int32_t level, int32_t line, const char* text) override {
    if (!e->quiet) std::fprintf(stderr, "mw3: report %#x player.cpp:%d %s\n", level, line, text);
  }
  float frame_dt() override { return e->frame_dt(); }
  Handle pilot_vehicle() override { return e->field32(e->pilot(), 0x14); }
  // Mech3.exe 1.2 @ 0x00419fc0 (vtable +0x34): the pilot's mech is the
  // port's (the launch), its frame.
  void vehicle_41aba0(Handle v, float dt) override {
    void* m = e->object_of(v);
    if (!m || !is_launched_mech(m)) return PlayerHooks::vehicle_41aba0(v, dt);
    static GameEngine* s_e;
    s_e = e;
    mech_after_tick(m, [](const char* k) { return s_e->unported(k, {}); });
  }
  void vehicle_update(Handle v, float dt) override {
    void* m = e->object_of(v);
    if (m && is_launched_mech(m)) update_mech(m, dt);
    else PlayerHooks::vehicle_update(v, dt);
  }
  void entities_update(float dt) override { update_entities(dt); }  // 0x004a1140
  Handle view() override { return e->player_view(); }
  Handle view_target(Handle v) override { return e->field32(v, 0x64); }
  // view.vt+0x08 (0x0040ada0): the port's view, each player tick.
  void view_tick(Handle v, float dt) override {
    if (!v || !program_view()) return PlayerHooks::view_tick(v, dt);
    program_view()->update(dt);
    e->sync_view();
  }
  Handle stealth() override { return e->peek32(0x609a2c); }
  void clear_6099c0() override { e->poke32(0x6099c0, 0); }
};
WiredPlayerHooks g_player_hooks;

// The brain's calls out (mech3/zai_brain): the player's vehicle is the
// pilot's +0x14 (none until the launch makes it).
struct WiredBrainHooks : ai::BrainHooks {
  GameEngine* e = nullptr;
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return e->unported(key, args);
  }
  bool player_allies(uint32_t* allies) override {
    const Handle p = e->pilot();
    const Handle v = p ? e->field32(p, 0x14) : 0;
    if (!v) return false;
    *allies = mech_allies(e->object_of(v));
    return true;
  }
  // The brain's frame (docs/ai.md 17.1) on the AI mechs: the other vehicles
  // are not ported, so every pilot's vehicle and every contact's is a mech.
  void* mech(Handle v) const { return e->object_of(v); }
  Handle vehicle_handle(void* vehicle) override { return e->handle_of(vehicle); }
  float now() override { return mech_game_time(); }
  float health(Handle v) override { return mech_health(mech(v)); }
  void position(Handle v, float out[3]) override { mech_world_position(mech(v), out); }
  ai::AiTarget* targetable(Handle v) override { return v ? mech_ai_target(mech(v)) : nullptr; }
  int32_t powered_down(Handle v) override { return mech_powered_down(mech(v)) ? 1 : 0; }  // 0x0041cc10
  // 0x004e6fe0 then vt+0x40: a mech answers itself; targets.zrd's objects 0.
  Handle contact_vehicle(const ai::Contact* c) override {
    for (const ai::Handle s : ai::entity_list())
      if (c->seen && s == c->seen) return s;
    return 0;
  }
  float contact_range2(Handle v) override { return mech_ai_target(mech(v))->range2; }
  void count_awake() override { e->poke32(0x6099c0, e->peek32(0x6099c0) + 1); }
  float intensity() override { return options().intensity[0]; }
  bool ammo_value(Handle v, float* out) override { return mech_ammo_value(mech(v), out); }
  Handle player_vehicle() override {
    const Handle p = e->pilot();
    return p ? e->field32(p, 0x14) : 0;
  }
};
WiredBrainHooks g_brain_hooks;

// valves.zrd's actions (mech3/zrdrvalve) on what the program has: the AI
// mechs by their records' names (the other vehicles are not ported), their
// controls and brains, Mech::power, zSound. The rest is logged.
struct WiredValveHooks : ai::ValveHooks {
  GameEngine* e = nullptr;
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return e->unported(key, args);
  }
  void report(int32_t level, int32_t line, const char* text) override {
    if (!e->quiet) std::fprintf(stderr, "mw3: report %#x ZRdrValve.cpp:%d %s\n", level, line, text);
  }
  Handle sound_find(const char* name) override { return e->sound_find(name); }
  void sound_play(Handle s) override {
    if (auto* o = static_cast<zsnd::Object*>(e->object_of(s))) zsnd::play_at(o, nullptr, 1.0f, nullptr);
  }
  Handle target_find(const char* name) override {
    int32_t n = 0;
    const AiMechMade* m = mission_ai_mechs(&n);
    for (int32_t i = 0; name && i < n; ++i)
      if (std::strcmp(m[i].name, name) == 0) return e->handle_of(m[i].mech);
    return ValveHooks::target_find(name);
  }
  Handle entity_find(const char* name) override {
    const Handle h = target_find(name);
    return h ? h : ValveHooks::entity_find(name);
  }
  bool mech(Handle h) const {
    int32_t n = 0;
    const AiMechMade* m = mission_ai_mechs(&n);
    for (int32_t i = 0; i < n; ++i)
      if (e->object_of(h) == m[i].mech) return true;
    return false;
  }
  Handle as_vehicle(Handle o) override { return mech(o) ? o : ValveHooks::as_vehicle(o); }
  Handle vehicle_of(Handle o) override { return mech(o) ? o : ValveHooks::vehicle_of(o); }
  Handle mech_of(Handle o) override { return mech(o) ? o : ValveHooks::mech_of(o); }
  Handle controls_of(Handle v) override {
    return mech(v) ? e->handle_of(mech_controls_of(e->object_of(v))) : ValveHooks::controls_of(v);
  }
  ai::Brain* brain(Handle c) const {
    auto* ctl = dynamic_cast<AiControls*>(static_cast<MechControls*>(e->object_of(c)));
    return ctl ? ctl->brain.get() : nullptr;
  }
  Handle brain_of(Handle c) override { return e->handle_of(brain(c)); }
  void mech_power(Handle m, int32_t down) override { mech3::mech_power(e->object_of(m), down); }
  Handle brain_order_net(Handle b) override {
    auto* br = static_cast<ai::Brain*>(e->object_of(b));
    return br && !br->orders.empty() ? e->handle_of(br->current().net->net()) : 0;
  }
  void controls_set_net(Handle c, Handle net) override {
    if (ai::Brain* b = brain(c)) b->set_net(static_cast<ai::AiNet*>(e->object_of(net)));
  }
  void brain_write(Handle b, uint32_t off, uint32_t v) override {
    auto* br = static_cast<ai::Brain*>(e->object_of(b));
    if (!br) return;
    if (off < ai::AiParams::kEnd) return br->params.put_raw(static_cast<int>(off), v);
    if (off == 0xdc) std::memcpy(&br->xdc, &v, 4);
  }
  void target_masks(Handle o, uint32_t t, uint32_t a, uint32_t en) override {
    void* m = e->object_of(o);
    mech_set_mask(m, 0xc0, t), mech_set_mask(m, 0xc4, a), mech_set_mask(m, 0xc8, en);
    player_clear_60b56c();
  }
  float game_time() override { return mech_game_time(); }
  void mission_abort() override { missions().lose(); }  // 0x0048f730 -> 0x0048f9e0
};
WiredValveHooks g_valve_hooks;

// targets.zrd's objects (mech3/zai_target) on what the program has: the
// reader's files, zClass's walk of list 6, zEffect's animations, zWeapon's
// damage records; zEntity3d, the radar's node map and the HUD's markers are
// not ported (logged). zWeapon's hit code is not ported either, so nothing
// calls the callbacks yet.
int32_t target_weapon_hit(void* owner, const void* hit, int32_t, float damage) {
  return static_cast<int32_t>(static_cast<ai::CZAiTarget*>(owner)->on_hit(0, 0, damage)) + (hit ? 0 : 0);
}
float target_collide(void* owner, float damage) { return static_cast<ai::CZAiTarget*>(owner)->on_collide(damage); }

struct WiredTargetHooks : ai::TargetHooks {
  GameEngine* e = nullptr;
  std::unique_ptr<ai::TargetRec> rec[2];
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return e->unported(key, args);
  }
  ai::RecordFile& file(bool traps) { return traps ? ai::traps_file() : ai::targets_file(); }
  void records_open(bool traps) override {
    rec[traps] = std::make_unique<ai::TargetRec>();  // 0x004e0870 / 0x004e07d0
    file(traps).add_ref();
  }
  int32_t record_count(bool traps) override { return file(traps).record_count(); }
  ai::TargetRec* record(bool traps, int32_t i, const char** name) override {
    zrdr::Node* value = file(traps).record(i, name);
    return rec[traps]->parse(value, *name);
  }
  void records_close(bool traps) override {
    file(traps).release();
    rec[traps].reset();
  }
  Handle target_handle(ai::CZAiTarget* t) override { return e->handle_of(t); }
  // A contact's sight and teams (0x004e6fe0, vt+0x28): a mech's and a
  // target's vt+0x2c and vt+0x28 answer themselves.
  Handle seen_vehicle(Handle s) override { return s; }
  Handle ai_target(Handle v) override { return v; }
  uint32_t team_of(Handle t) override {
    const ai::AiTarget* a = ai::sensor_hooks().fields(t);
    return a ? a->team : 0;
  }
  // 0x005a8390 (zEntity, not ported): the record's target and centroid
  // subnodes found by name, else the node itself; none are made.
  void entity_set_node(Handle self, Handle n, Handle a, const char* centroid, const char* target,
                       Handle* target_node, Handle* centroid_node) override {
    unported("0x005a8390", {self, n, a, centroid, target});
    zcls::Node* nd = node(n);
    auto find = [&](const char* name) {
      zcls::Node* f = name && nd ? zcls::node_find_in_tree(nd, name) : nullptr;
      return e->handle_of(f ? f : nd);
    };
    *target_node = nd ? find(target) : 0;
    *centroid_node = nd ? find(centroid) : 0;
  }
  bool have_pilot() override { return e->pilot() != 0; }
  Handle player_vehicle() override { return e->pilot() ? e->field32(e->pilot(), 0x14) : 0; }
  int32_t game_type() override { return e->launch().type; }
  void sorted_clear() override { player_clear_60b56c(); }
  // zClass's list 6 by name (0x004f8c30) or by prefix (0x004f8b80).
  static bool prefixed(zcls::Node* n) {
    const char* key = static_cast<const char*>(zcls::list_walk().key);
    return std::strncmp(n->name(), key, std::strlen(key)) == 0;
  }
  Handle nodes_first(const char* name, bool prefix) override {
    return e->handle_of(prefix ? zcls::list_walk_next(name, 6, prefixed) : zcls::list_walk_by_name(name, 6));
  }
  Handle nodes_next(bool prefix) override {
    return e->handle_of(prefix ? zcls::list_walk_next(nullptr, 6, prefixed) : zcls::list_walk_by_name(nullptr, 6));
  }
  zcls::Node* node(Handle h) const { return static_cast<zcls::Node*>(e->object_of(h)); }
  int32_t node_box(Handle n, float box[6]) override {
    zcls::Node* nd = node(n);
    if (!nd || !nd->data) return 5;
    std::memcpy(box, nd->raw + 0x74, 6 * sizeof(float));  // 0x004ebd50
    return (nd->flags() & 0x100) == 0 ? 1 : 0;
  }
  // 0x0050b4e0: the first def in use named so whose node is this one.
  Handle anim_of_node(const char* name, int32_t len, Handle n) override {
    zeff::AnimState& a = zeff::anim();
    if (!name || len <= 0 || !n) return 0;
    for (int32_t i = 1; a.defs && i < a.def_count(); ++i) {
      zeff::AnimDef& d = a.defs[i];
      if (d.raw[0x98] != 5 && std::strncmp(d.name(), name, len) == 0 && d.node == node(n)) return e->handle_of(&d);
    }
    return 0;
  }
  zeff::AnimDef* def(Handle h) const { return static_cast<zeff::AnimDef*>(e->object_of(h)); }
  float anim_health(Handle a) override { return def(a) ? def(a)->get<float>(0xb0) : 0.0f; }
  float anim_max_health(Handle a) override { return def(a) ? def(a)->get<float>(0xac) : 0.0f; }
  Handle anim_node(Handle a) override { return def(a) && def(a)->raw[0x98] != 5 ? e->handle_of(def(a)->node) : 0; }
  Handle anim_node2(Handle a) override { return def(a) && def(a)->raw[0x98] != 5 ? e->handle_of(def(a)->root) : 0; }
  void anim_run(Handle a) override {
    if (zeff::AnimDef* d = def(a)) zeff::anim_start(d, nullptr, 0, 0, 0);
  }
  // zWeapon's records: the callbacks as the exe's addresses for the checks.
  Handle node_handlers(Handle n) override { return node(n) ? e->handle_of(zwep::record_of(node(n))) : 0; }
  uint32_t handler_word(Handle t, int32_t i) override {
    auto* r = static_cast<zwep::DamageRecord*>(e->object_of(t));
    if (!r) return 0;
    const zwep::DamageRecord::Slot& s = r->slot[i / 2];
    if (i % 2 == 0) return e->handle_of(s.owner);
    if (!s.callback) return 0;
    if (s.callback == reinterpret_cast<void*>(&zeff::anim_on_weapon_hit)) return 0x502e60;
    if (s.callback == reinterpret_cast<void*>(&zeff::anim_on_collide)) return 0x502df0;
    if (s.callback == reinterpret_cast<void*>(&target_weapon_hit)) return 0x4d26b0;
    if (s.callback == reinterpret_cast<void*>(&target_collide)) return 0x4d26d0;
    return e->handle_of(s.callback);
  }
  static void* callback_of(uint32_t v) {
    if (v == 0x4d26b0) return reinterpret_cast<void*>(&target_weapon_hit);
    if (v == 0x4d26d0) return reinterpret_cast<void*>(&target_collide);
    return nullptr;
  }
  void set_handler_word(Handle t, int32_t i, uint32_t v) override {
    auto* r = static_cast<zwep::DamageRecord*>(e->object_of(t));
    if (!r) return;
    if (i % 2 == 0) r->slot[i / 2].owner = e->object_of(v);
    else r->slot[i / 2].callback = callback_of(v);
  }
  uint32_t handlers_own(Handle n) override {
    zcls::Node* nd = node(n);
    return nd && zwep::record_of(nd) ? static_cast<uint32_t>(zwep::owns_record(nd, zwep::record_of(nd))) : 0;
  }
  int32_t set_weapon_handler(Handle self, Handle n, uint32_t fn) override {
    return zwep::register_weapon_hit(e->object_of(self), node(n), 1, callback_of(fn));
  }
  int32_t set_collide_handler(Handle self, Handle n, uint32_t fn) override {
    return zwep::register_collide(e->object_of(self), node(n), callback_of(fn));
  }
  void valve_set(void* valve, int32_t v) override {
    if (valve) static_cast<ai::Valve*>(valve)->set(v);
  }
  void report(int32_t line, const char* fmt, Handle n) override {
    if (!e->quiet) std::fprintf(stderr, "mw3: report 0x200 ZAiTarget.cpp:%d %s (%s)\n", line, fmt,
                                node(n) ? node(n)->name() : "?");
  }
};
WiredTargetHooks g_target_hooks;

struct WiredMissionHooks : MissionHooks {
  void fn_4ea750() override { ai::ai_switch_off(); }  // the AI switch off
  void fn_40f040() override { message_panel_reset(); }        // the message panel cleared and out
  void fn_40f0b0(uint8_t v) override { if (message_panel()) message_panel_set(v != 0); }  // slid in or out
  GameEngine* e = nullptr;
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return e->unported(key, args);
  }
  // The loading screen is the briefing's (not ported): its lines go to the log.
  void progress(float) override {}
  void progress_text(const char* s) override {
    if (s && *s) std::printf("mw3: load: %s\n", s);
  }
  int32_t game_mission_index() override { return e->launch().index; }
  int32_t game_type() override { return e->launch().type; }
  int32_t game_mission_number() override { return e->launch().number; }
  uint8_t game_saved() override { return 0; }
  void fn_484270(int32_t) override { mech3::mech_types_open(); }  // 0x00484270: mechlib and the voices
  void fn_4319a0() override { mech3::mech_types_refresh(); }    // 0x004319a0: the mech types' sounds again
  uint8_t app_rgb() override { return app().byte_at(0x1b9); }       // 0x00608e41
  uint8_t app_turrets() override { return app().byte_at(0x1b2); }   // 0x00608e3a
  void set_holes(uint8_t on) override { e->poke8(0x6061cc, on); }
  void set_use_archive() override { zint::globals().use_archive = 1; }
  void clear_9a17c0() override { e->poke32(0x9a17c0, 0); }
  void set_view(Handle v) override { e->poke32(0x5f4c8c, v); }
  Handle view() override { return e->peek32(0x5f4c8c); }
  void set_608c84(Handle v) override { e->poke32(0x608c84, v); }
  Handle get_608c84() override { return e->peek32(0x608c84); }
  void set_stealth(Handle v) override { e->poke32(0x609a2c, v); }
  Handle stealth() override { return e->peek32(0x609a2c); }
  uint32_t field32(Handle h, uint32_t off) override { return e->peek32(h + off); }
  void set_field32(Handle h, uint32_t off, uint32_t v) override { e->poke32(h + off, v); }
  // The mission unload (Missions::unload, 0x00490480) on the ports.
  void camera_flag_off(Handle camera) override {  // 0x004f3430(camera, 0): the camera's +0x10 bit 0
    zcls::Node* n = mission_node(*e, camera);
    if (n && n->data && n->cls() == 1) {
      auto* c = static_cast<zcls::Camera*>(n->data);
      c->put<uint32_t>(0x10, c->get<uint32_t>(0x10) & ~1u);
    }
  }
  void mech_types_clear() override { mech3::mech_types_forget(); }  // 0x004318e0
  void mech_defs_clear() override { mech3::mech_types_close(); }    // 0x004844b0: mechlib and the definitions
  void sound_set_unload_608dd4() override {}  // the app's +0x14c is never written: no set by that name
  void effects_free() override { zeff::effects_free(); }   // 0x0050d570
  void anims_free() override { zeff::anim_free_all(); }    // 0x0050b380
  void weapons_free() override { zwep::shutdown(mech3::free_weapon_type); }  // 0x00584a60(0x004b0090)
  void view_free(Handle view) override {  // its window (+0x08), then itself: the port's view
    program_view_free();
    if (view) e->set_mission_view(0, 0);
  }
  void delete_608c84(Handle) override {}  // [0x00608c84] (0x0047f2b0) is a block the program keeps nothing in
  void classes_free() override { zcls::classes_reset(); }  // 0x004f8fb0
  void models_free() override {  // 0x00526920 with the materials' 0x005372c0
    zmod::ModelPool& p = zmod::models();
    for (int32_t i = 0; i < p.count; ++i) zmod::model_release(&p.slots[i]);  // 0x00539150
    zmod::models() = zmod::ModelPool{};
    zmod::materials() = zmod::MaterialTable{};
  }
  void textures_free() override { zimg::textures_free(); }  // 0x00516f30: 0x00518b70
  void set_field8(Handle h, uint32_t off, uint8_t v) override { e->poke8(h + off, v); }
  Handle alloc(uint32_t size) override { return e->alloc_block(size); }
  void interp_define(const char* name) override {
    if (zint::Interp* in = interp()) zint::macro_set_true(*in, name);
  }
  uint8_t interp_source(const char* path) override {
    zint::Interp* in = interp();
    return in && zint::source(*in, path) ? 1 : 0;
  }
  int32_t hw_card_flag() override { return mech3::hw_card_flag(); }
  int32_t texture_memory() override { return mech3::texture_memory(); }
  uint32_t install_options(uint32_t mask) override { return e->install_options(mask); }
  void set_render_target(Handle w) override { mech3::set_render_target(w); }
  void set_display_target(Handle d) override { mech3::set_display_target(d); }
  void set_camera(Handle c) override { mech3::set_camera(c); }
  void render_rect(int32_t out[4]) override {
    const Rect& r = mech3::render();
    out[0] = r.x, out[1] = r.y, out[2] = r.x_end, out[3] = r.y_end;
  }
  Handle window_rect() override { return e->window_rect(); }
  Handle display_rect() override { return e->display_rect(); }
  uint32_t video_texture_memory(int32_t dev, uint32_t* total, uint32_t* free_) override {
    const uint32_t r = e->video_card_memory(dev, total);
    *free_ = *total;
    return r;
  }
  uint32_t video_fn_390(int32_t dev) override { return e->video_card_fn_390(dev); }
  const char* version_number() override { return e->version_number(); }
  const char* message(int32_t id) override { return e->message(id); }

  // zImage's texture libraries (0x005182a0, 0x005190d0), found through the
  // mounts: each asset root's zbd/<mission>/ and zbd/ (FIX (A): the
  // original's search path "zbd;zbd\<dir>" set by the Load state).
  void textures_begin(int32_t v) override {
    unported("0x00516f20", {v});
    zimg::TextureTable& t = zimg::textures();
    std::memset(t.entries, 0, sizeof t.entries);
    t.count = 0, t.group = 0;
    zimg::state() = zimg::State{};
    zimg::state().display_format = 6;
    // The mission's directory searched before the common one (the Load
    // state's "zbd;zbd\\<dir>"); the last mission's mounts taken out first.
    static std::vector<std::string> s_added;
    std::vector<std::string>& read = zutl::mounts().read;
    for (const std::string& m : s_added) {
      const auto it = std::find(read.begin(), read.end(), m);
      if (it != read.end()) read.erase(it);
    }
    s_added.clear();
    const std::string dir = mission_dir(e->launch().index);
    const std::vector<std::string> roots = read;
    for (const std::string& r : roots) s_added.push_back(r + "/zbd/" + dir);
    for (const std::string& r : roots) s_added.push_back(r + "/zbd");
    for (const std::string& m : s_added) zutl::mount(m);
    zimg::texture_load() = zimg::TextureLoad{};
    zimg::texture_hooks() = zimg::TextureHooks{};
    zimg::texture_hooks().release = [](void* s) { zvid::release_texture(static_cast<zvid::D3DTexture*>(s)); };  // 0x0057bb00
    zvid::set_pixel_format(5, 6, 5);
    zvid::channel_masks() = {0xf800, 0x07e0, 0x001f};
    zvid::set_render3d(e->platform_parts().render);
    zvid::default_texture() = &g_fallback;
    zimg::default_entry()->surface = &g_fallback;
  }
  void texture_library(const char* name, int32_t level, int32_t hw) override {
    zimg::new_library(name, level, hw != 0);
    if (std::strcmp(name, "texture") != 0) zimg::register_image_library(1);  // after the mech set, as 0x0048fcfe registers
  }
  void textures_load() override { zimg::load_group_0(); }

  // zClass's reset 0x004f8f70 and zModel's 0x00526800: fresh pools and the
  // draw table ([0x0074a2e0] = 0x0053f2e0 on Direct3D).
  void classes_reset() override {
    zcls::nodes() = zcls::NodePool{};
    zcls::nodes().free_head = -1;
    zcls::lists() = zcls::Lists{};
    zmth::reset_stack();
  }
  void models_reset() override {
    zmod::materials() = zmod::MaterialTable{};
    zmod::models() = zmod::ModelPool{};
    zmod::reset_pool();
    zmod::fog() = zmod::Fog{};
    zrndr::fog_color() = zrndr::FogColor{};
    zrndr::device_fog().active = 1;  // the Direct3D path
    zvid::install_model_draw_table();
    zcls::scene_walk() = zcls::SceneWalk{};
    zmod::walk_state() = zmod::WalkState{};
    zmod::walk_state().draw = zmod::draw_model;
    zmod::zones() = zmod::Zones{};
    zmod::shadow_casters() = zmod::ShadowCasters{};
    zmod::draw_state() = zmod::DrawState{};
    zmod::cull_hooks() = zmod::CullHooks{};
    zcls::update_hooks() = zcls::UpdateHooks{};
    zcls::WorldRenderHooks& h = zcls::world_render_hooks();
    h = zcls::WorldRenderHooks{};
    h.begin_scene = [](void*) { zvid::scene_begin(); };
    h.end_scene = [](void*) { zvid::scene_end(); };
    h.flush_transparent = [](void*) { zvid::flush_transparent(); };
    h.flush_overlay = [](void*) { zvid::flush_overlay(); };
    h.draw_quad = [](void*) { zvid::draw_tint_quad(); };
    h.puffers = [](void*) { zmod::puff_draw(); };
    h.world_sounds = [](void*, zcls::Node* world) { zcls::world_sounds(world); };  // 0x004f3170
    h.listener_speed_limit = [](void*) { return zsnd::speed_of_sound(); };       // 0x00572360
    h.listener = [](void*, const float* m, const float v[3]) { zsnd::set_listener(m, v); };  // 0x00571e50
  }
  Handle find_node(int32_t list, const char* name) override { return e->handle_of(zcls::list_find_by_name(list, name)); }
  void lists_4f7f70() override { zcls::frame_update(); }
  void lists_4f7bd0() override { zcls::purge_lists(); }
  // The world files keep a 320x200 window; set to the screen mode's.
  void window_set_size(Handle node, int32_t w, int32_t h) override {
    zcls::Node* n = mission_node(*e, node);
    if (n && n->data) {
      auto* d = static_cast<zcls::Window*>(n->data);
      d->put<int32_t>(0x8, w), d->put<int32_t>(0xc, h);
    }
    if (e->platform_parts().render) e->platform_parts().render->set_viewport(0, 0, w, h);
  }
  void window_set_origin(Handle node, int32_t x, int32_t y) override {
    zcls::Node* n = mission_node(*e, node);
    if (n && n->data) {
      auto* d = static_cast<zcls::Window*>(n->data);
      d->put<int32_t>(0x0, x), d->put<int32_t>(0x4, y);
    }
  }
  // The view and its window (0x0040aca0, 0x004b52e0, not ported): blocks in
  // the engine's memory, the window node at the window's +4 as the
  // constructor keeps it; the camera the Core state draws through.
  Handle make_view_window(Handle mem, Handle window) override {
    unported("0x004b52e0", {mem, window});
    e->poke32(mem + 4, window);
    return mem;
  }
  Handle make_view(Handle mem, Handle camera, Handle window, Handle x) override {
    unported("0x0040aca0", {mem, camera, window, x});
    e->set_mission_view(mem, camera);
    program_view_make(mission_node(*e, camera));
    return mem;
  }
  Handle make_608c84(Handle mem, Handle world) override {
    unported("0x0047f2b0", {mem, world});
    build_soil_table();  // 0x0047f400, in the constructor
    return mem;
  }
  void anim_allow_loading(int32_t on) override { zeff::anim_allow_loading(on); }
  // The animations' calls out: puffers (gmod_puff) and sounds (zSound) are
  // ported; zEffect's effect table 0x0050d9e0 is not, so a def naming an
  // effect is left out of the post-load pass (anim-postload.md).
  void anim_set_world(Handle world) override {
    zeff::AnimHooks& a = zeff::anim_hooks();
    // 0x004f9b50 with [0x005ebcb4] and [0x005ebcb8] (both 1): a running def's
    // second instance at no node (an impact while the last one plays) copies its tree.
    a.copy_tree = [](zcls::Node* n) { return zcls::tree_copy_materials(n, 1, 1); };
    a.create_puffer = []() -> void* { return zmod::puffer_create(); };
    a.find_sound = [](const char* name) -> void* { return zsnd::find(name); };
    a.effect_index = zeff::effect_find;  // 0x0050d9e0
    zcls::set_find_sound(a.find_sound);
    zmod::puff_hooks().create_node = [](void*) { return zcls::object3d_create(); };
    zeff::anim_set_world(mission_node(*e, world));
  }
  void anim_load(const char*) override { zeff::anim_load(); }
  void anim_cleanup() override { zeff::anim_stop_all(1); }  // 0x0050ace0(1)
  // 0x00501ab0: declient.zrd's crater and quicksand settings (craters.md).
  // 0x00502020: the craters' history, the quicksand frames and the texture
  // table go with the mission.
  void decals_free() override { zdec::decals_free(); }
  // The craters' ties: zGeometry finds the world through zDEClient, and the
  // dust is an animation at the crater (0x00508aa0, no node, unturned).
  void decals_load(Handle world, int32_t) override {
    zdec::init(mission_node(*e, world));
    zgeo::hooks().world_node = zdec::world_node;
    zdec::crater_hooks().play_anim = [](zeff::AnimDef* a, const float at[3]) {
      static const float zero[3] = {0, 0, 0};
      zeff::anim_start_at(a, nullptr, at, zero, zero);
    };
  }
  // 0x0050d2c0: zEffect's table from effects.zrd (an empty list in both games).
  void effects_load(Handle, Handle, const char* file) override {
    zeff::EffectHooks& h = zeff::effect_hooks();
    h.unported = [](const char* key, int32_t i) {
      if (GameEngine* g = engine_of_hooks()) g->unported(key, {i});
    };
    h.error = [](const char* f) {
      if (GameEngine* g = engine_of_hooks(); g && !g->quiet) std::fprintf(stderr, "mw3: Failed to read %s\n", f);
    };
    zeff::effects_init(file);
  }
  void world_params(const char* file, const char* block) override {
    read_world_params(file, block, mission_node(*e, e->mission_camera()), e->launch().type, e->launch().number);
  }
  void reader_open(const char* path, int32_t current) override { zrdr::open_archive(path, current != 0); }
  // 0x00583880: weapons.zrd into zWeapon's definitions (each def's game half
  // by 0x004b0020, mech3/weapon), the projectile pool, its nodes and the
  // impact lights, the weapon_tick object (priority 3, its action
  // zwep::tick). The animations are the mission's (0x0050b450), the effects
  // zEffect's table, the crater defaults zDEClient's (read just before).
  void weapons_load(Handle world, const char* file, int32_t a, uint32_t callback) override {
    zwep::InitHooks& w = zwep::init_hooks();
    w = zwep::InitHooks{};
    w.sound = [](const char* n) -> zwep::Handle { return zsnd::find(n); };
    w.message = zutl::localize;
    w.effect = zeff::effect_find;                                    // 0x0050d9e0
    w.anim = [](const char* n) -> zwep::Handle { return zeff::anim_find(n); };  // 0x0050b450
    w.lights_init = []() { zwep::lights_init(); };                    // 0x00586970
    w.crater_defaults = [](zwep::Crater* out) {  // 0x004ff920
      const zwep::CraterOrder& c = zdec::state().crater;
      *out = zwep::Crater{};
      out->x00 = c.x00, out->points = c.points, out->x08[1] = c.x0c, out->depth = c.depth, out->radius = c.radius;
    };
    w.set_intersect_surface = [](zwep::Handle n, int32_t on) {        // 0x004eb6d0
      zcls::node_set_intersect(static_cast<zcls::Node*>(n), on);
    };
    w.tree_unlit = [](zwep::Handle n) {                               // 0x004f9e00 with 0x004f9de0
      return zcls::node_tree_any(static_cast<zcls::Node*>(n), zcls::node_is_billboard);
    };
    w.find_node = [](int32_t list, const char* n) -> zwep::Handle { return zcls::list_find_by_name(list, n); };
    w.set_active = [](zwep::Handle n, int32_t on) { zcls::node_set_active(static_cast<zcls::Node*>(n), on); };
    w.object3d_create = []() -> zwep::Handle { return zcls::object3d_create(); };
    w.pool_objects = [](int32_t create) { zwep::pool_objects(create); };  // 0x005847c0: the shots' nodes
    w.set_action_priority = [](zwep::Handle n, int32_t p) {
      zcls::node_set_priority(static_cast<zcls::Node*>(n), p);
    };
    w.set_tick_action = [](zwep::Handle n) {
      zcls::node_set_action(static_cast<zcls::Node*>(n), [](zcls::Node*) { zwep::tick(); });
    };
    w.error = [](int32_t level, int32_t line, const char* what) {
      if (GameEngine* g = engine_of_hooks(); g && !g->quiet)
        std::fprintf(stderr, "mw3: report %#x zwep_init.cpp:%d %s\n", level, line, what);
    };
    w.hardware = true;
    if (callback != 0x004b0020) unported("0x00583880", {world, file, a, callback});
    zwep::init(e->object_of(world), file, a, mech3::make_weapon_type);
  }
  // 0x0049e810: the mission's vehicles (mech3/player).
  void player_setup(uint8_t full) override {
    player_setup_ran = true;
    g_player_hooks.forget();  // the last mission's AI is gone
    mech3::player_setup(full != 0);
  }
  bool player_setup_ran = false;
  Handle sound_find(const char* name) override { return e->sound_find(name); }
  // The mission's valves.zrd (0x004e2120, mech3/zrdrvalve).
  void fn_4e2120() override { ai::valves_load(); }
  // targets.zrd and traps.zrd (0x004d2e20, 0x004d31d0, mech3/zai_target).
  void fn_4d2e20() override { ai::targets_create(); }
  void fn_4d31d0() override { ai::traps_create(); }

  // The frame (0x0048f470, docs/mission-frame.md): the game time to the
  // ports' copies of [0x0060991c], the valves' timers, zSound's voice lines.
  void valves_tick(float) override {
    mech_set_game_time(missions().time);
    ai::valves_tick(missions().time);
  }
  Handle player_vehicle() override { return e->pilot() ? e->field32(e->pilot(), 0x14) : 0; }
  void bind_command(int32_t cmd, uint32_t handler) override { unported("0x0051d2f0", {cmd, handler}); }
  uint8_t app_network() override { return options().multiplayer; }
  int32_t invincible() override { return mech3::invincible(); }
  int32_t infinite_ammo() override { return mech3::infinite_ammo(); }
  int32_t heat() override { return mech3::heat(); }
  uint8_t app_1c2() override { return app().byte_at(0x1c2); }  // 0x00608e4a, never set
  zsnd::VoiceBase* voice_of(Handle s) const { return static_cast<zsnd::VoiceBase*>(e->object_of(s)); }
  uint32_t sound_playing(Handle s) override { return static_cast<uint32_t>(zsnd::status(voice_of(s))); }
  void sound_stop(Handle s) override { zsnd::stop(voice_of(s)); }
  Handle sound_play(Handle s, float volume) override {
    auto* o = static_cast<zsnd::Object*>(e->object_of(s));
    return o ? e->handle_of(zsnd::play(o, volume)) : 0;
  }
  // The cleanup (0x00491780): the valves end, the voice set of the mission
  // goes; the clean-ups of modules not ported are logged.
  void cleanup(int32_t unload) override { missions().cleanup(unload != 0); }
  void valves_end() override { ai::valves_end(); }
  void targets_end() override { ai::targets_end(); }
  void fn_4d8270() override { ai::targets_delete_all(); }  // the targets and traps deleted (0x004d8360)
  void voice_release(const char* name) override { zsnd::unload_set(name); }
  void sound_reset() override { zsnd::reset(); }
  void zerr(int32_t level, const char*, int32_t line, const char* fmt, const char* arg) override {
    char text[512];
    std::snprintf(text, sizeof text, fmt, arg);
    std::fprintf(stderr, "mw3: report %#x mission.cpp:%d %s\n", level, line, text);
  }
};
WiredMissionHooks g_mission_hooks;

}  // namespace

const AiMechMade* mission_ai_mechs(int32_t* count) {
  for (size_t i = 0; i < g_ai_mechs.size(); ++i) g_ai_mechs[i].name = g_ai_names[i].c_str();
  if (count) *count = static_cast<int32_t>(g_ai_mechs.size());
  return g_ai_mechs.data();
}

void wire_mission_hooks(GameEngine* e) {
  g_mission_hooks.e = e;
  g_player_hooks.e = e;
  g_player_hooks.forget();  // a new engine numbers its handles afresh
  g_mission_engine = e;
  set_mission_hooks(e ? &g_mission_hooks : nullptr);
  set_player_hooks(e ? &g_player_hooks : nullptr);
  g_brain_hooks.e = e;
  ai::set_brain_hooks(e ? &g_brain_hooks : nullptr);
  g_valve_hooks.e = e;
  ai::set_valve_hooks(e ? &g_valve_hooks : nullptr);
  g_target_hooks.e = e;
  ai::set_target_hooks(e ? &g_target_hooks : nullptr);
  // The mech build and the launch (app_mech.cpp, docs/mech-build.md).
  if (!e) {
    wire_sensors(nullptr, nullptr);
    return wire_mech_build(nullptr);
  }
  static MechWiring w;
  w.engine = e;
  auto en = [](void* p) { return static_cast<GameEngine*>(p); };
  w.unported = [](void* p, const char* key, std::initializer_list<zgame::Arg> args) {
    return static_cast<GameEngine*>(p)->unported(key, args);
  };
  w.peek32 = [](void* p, uint32_t a) { return static_cast<GameEngine*>(p)->peek32(a); };
  w.poke32 = [](void* p, uint32_t a, uint32_t v) { static_cast<GameEngine*>(p)->poke32(a, v); };
  w.poke8 = [](void* p, uint32_t a, uint8_t v) { static_cast<GameEngine*>(p)->poke8(a, v); };
  w.object_of = [](void* p, uint32_t h) { return static_cast<GameEngine*>(p)->object_of(h); };
  w.handle_of = [](void* p, const void* o) { return static_cast<GameEngine*>(p)->handle_of(const_cast<void*>(o)); };
  w.vcall = [](void* p, uint32_t obj, uint32_t slot, std::initializer_list<zgame::Arg> args) {
    return static_cast<GameEngine*>(p)->vcall(obj, slot, args);
  };
  w.pilot = [](void* p) { return static_cast<GameEngine*>(p)->pilot(); };
  w.view = [](void* p) { return static_cast<GameEngine*>(p)->player_view(); };
  (void)en;
  wire_mech_build(&w);
  wire_sensors(&w, [](void* p, int32_t id) { return static_cast<GameEngine*>(p)->message(id); });
}

// 0x00490880 on the missions object: the load, then (a debug launch, our
// addition standing in for the briefing's Launch button) the Core state.
void GameEngine::missions_start(uint32_t list) {
  if (list != 0x6098d0) return Engine::missions_start(list);
  Missions& m = missions();
  ai::entity_list().clear();  // zEntity's list: the last mission's mechs are gone (no destructor ported)
  m.start();
  mech_set_game_time(m.time);  // 0 from the setup (0x0048f400)
  if (m.failed) std::fprintf(stderr, "mw3: the mission load failed\n");
  zcls::Node* cam = mission_node(*this, camera_);
  if (launch_.active && launch_.have_pos && cam) {  // as the debug viewer places it (render.md section 0)
    zcls::camera_set_rotation(cam, -launch_.pitch, launch_.yaw + 3.14159265f, 0.0f);
    zcls::camera_set_position(cam, launch_.eye[0], launch_.eye[1], launch_.eye[2]);
  }
  if (launch_.active && !m.failed) {
    // The briefing's Launch (0x0040500a): the player's mech (0x00491150).
    LaunchedMech s;
    if (!launch_player() || !launched_mech(&s)) {
      std::fprintf(stderr, "mw3: the launch made no mech\n");
    } else if (!quiet) {
      std::fprintf(stderr, "mw3: launched the %s: %d parts, %d weapons, %d heat sinks, %.1f m tall, %.1f m/s\n",
                   mech_name(s.roster), s.parts, s.weapons, s.sinks, static_cast<double>(s.height),
                   static_cast<double>(s.max_speed));
    }
    app().replace(app().state(5), 0);
  }
}

// 0x004a8cd0 and 0x004d5ca0: the ports (mech3/sensors).
void GameEngine::sensors_update() { ai::sensors_update(); }
void GameEngine::entities_sort() { ai::entities_sort(); }
void GameEngine::ai_on() { ai::ai_switch_on(); }
// Mech3.exe 1.2 @ 0x00575a00 (ztimer::update), once a frame from zGameApp::Run:
// the step [0x0072c52c] is one global in the exe, so its copies in the mech,
// weapon and zWeapon modules are set with it.
void GameEngine::timer_update() {
  ztimer::update();
  sync_frame_step(ztimer::state().dt);
}

// Mech3.exe 1.2 @ 0x00583870 -> 0x00584920: zWeapon down (its pool's nodes
// and the defs), without the weapon types' free callback, as the exe.
void GameEngine::weapons_shutdown() { zwep::shutdown(nullptr); }

// Mech3.exe 1.2 @ 0x0041ca30 on the launched mech
void GameEngine::mech_set_mode_a(Handle mech, int32_t a) {
  if (void* m = object_of(mech); m && is_launched_mech(m)) mech_set_hud_fall(m, a);
  else Engine::mech_set_mode_a(mech, a);
}
// Mech3.exe 1.2 @ 0x0041cc30 (Mech::power) on the launched mech
void GameEngine::mech_set_mode_b(Handle mech, int32_t a) {
  if (void* m = object_of(mech); m && is_launched_mech(m)) mech_power(m, a);
  else Engine::mech_set_mode_b(mech, a);
}

// 0x0048ee70: the port's fade (mech3::fade_push, next 0); the Core state's
// mission start fades in from black over 2 s, then gets resume(7).
void GameEngine::fade(int32_t a, int32_t b, int32_t c, uint32_t d, int32_t e, int32_t f, int32_t g) {
  fade_push(nullptr, static_cast<uint32_t>(a), b, c, zgame::from_bits(d), static_cast<uint8_t>(e),
            static_cast<uint8_t>(f), static_cast<uint8_t>(g));
}

// 0x0048f470 on the missions object, each Core frame: the port's (its
// valves' tick copies the game time to the ports' [0x0060991c]).
void GameEngine::missions_update(uint32_t list, float dt) {
  if (list != 0x6098d0) return Engine::missions_update(list, dt);
  missions().update(dt);
}

// 0x00490720 (the Load state's): the mission number (+0x64, the game's
// vt+0x4c), which the cleanup names the mission's voice set by, and the sound
// sets the mission plays from: "common" (the weapons' own sounds), "mech",
// the mission directory's and "<dir><block>" (its voices), unless the game is
// of kind 6 (vt+0x54) or the briefing voices are off (app +0x1c3). Not ported:
// the stations line-up (0x004797c0 into +0xb0 / +0xb4).
void GameEngine::missions_reset(uint32_t list) {
  Engine::missions_reset(list);
  if (list != 0x6098d0 || !pilot_) return;
  missions().number = launch_.number;
  if (launch_.index != 6 && !app().no_voices()) {
    const char* dir = mission_dir(launch_.index);
    zsnd::load_set("common");  // 0x0056f520
    zsnd::load_set("mech");
    zsnd::load_set(dir);
    char block[32] = {};
    missions().block_name(block);  // 0x00490600
    zsnd::load_set((std::string(dir) + block).c_str());  // "%s%s"
  }
}

// The debug launch's pilot and game (our addition): [0x0060861c] a pilot
// whose game (+0x6c) answers the type, number and index, with a mech handle
// at +0x14 so the Core state stays (the player's mech is not built yet).
void GameEngine::set_launch(const Launch& l) {
  launch_ = l;
  launch_.active = true;
  slot_ = 0;  // made again for this launch's mech: a mission unload frees the last one's types
  pilot_ = alloc_block(0x100);
  game_ = alloc_block(0x100);
  poke32(0x60861c, pilot_);
  poke32(pilot_ + 0x6c, game_);
  poke32(pilot_ + 0x14, alloc_block(0x700));
}
Handle GameEngine::pilot_game(Handle p) { return p == pilot_ && pilot_ ? game_ : Engine::pilot_game(p); }
uint32_t GameEngine::pilot_campaign_number(Handle p) {
  return p == pilot_ && pilot_ ? static_cast<uint32_t>(launch_.index) : Engine::pilot_campaign_number(p);
}

// Mech3.exe 1.2 @ 0x0040ad90: the draw path resets the view before its update
// (app_states.cpp), so the camera snaps to the mech each frame.
void GameEngine::view_reset(Handle view) {
  if (view && view == view_ && program_view()) {
    program_view()->reset();
    return;
  }
  Engine::view_reset(view);
}

uint32_t GameEngine::vcall(Handle obj, uint32_t slot, std::initializer_list<Arg> args) {
  uint32_t r = 0;
  if (menu_vcall(obj, slot, args, &r)) return r;
  if (obj && obj == game_) {
    switch (slot) {
      case 0x2c: return static_cast<uint32_t>(launch_.type);
      case 0x34:  // the pilot's slot: the mech, its first configuration, skin 0
        if (!slot_) {
          slot_ = alloc_block(0x20);
          poke32(slot_ + 4, static_cast<uint32_t>(launch_.mech));
          poke32(slot_ + 8, debug_slot_config(launch_.mech, this, [](void* p, const void* o) {
            return static_cast<GameEngine*>(p)->handle_of(const_cast<void*>(o));
          }));
          poke32(slot_ + 0x10, 0);
        }
        return slot_;
      case 0x4c: return static_cast<uint32_t>(launch_.number);
      case 0x54: return static_cast<uint32_t>(launch_.index);
      case 0x28: return 0;
      default: break;
    }
  }
  // The view target's vt+0xe4 (0x0041cd90): its HUD's Direct3D pass.
  if (obj && slot == 0xe4 && view_ && obj == field32(view_, 0x64)) {
    program_hud_draw3d(zgame::from_bits(args.begin()->v));
    return 0;
  }
  // The view's +0x04 runs its camera: the world drawn by the scene walk
  // (0x004f7030), the frame counted as the class lists' pass counts it.
  // +0x08 its update (0x0040ada0, the port's).
  if (obj && obj == view_ && slot == 0x08 && program_view()) {
    program_view()->update(zgame::from_bits(args.begin()->v));
    sync_view();
    return 0;
  }
  if (obj && obj == view_ && slot == 0x04) {
    if (zcls::Node* cam = mission_node(*this, camera_)) {
      ++zcls::frames().frame;
      zcls::render_world(cam, 0);
      scene_drawn_ = true;
    }
    return 0;
  }
  return Engine::vcall(obj, slot, args);
}

const float* program_mission_fog(bool water) {
  static float f[7];
  const MissionWorld& w = missions().world;
  const float* r = water ? w.water_range : w.fog_range;
  const float* a = water ? w.water_altitude : w.fog_altitude;
  const float* c = water ? w.water_color : w.fog_color;
  f[0] = r[0], f[1] = r[1], f[2] = a[0], f[3] = a[1], f[4] = c[0], f[5] = c[1], f[6] = c[2];
  return f;
}

// The player view's camera id and target into its block, as the app code
// reads them (+0x58 a block whose +4 is the id, +0x64 the mech's handle).
void GameEngine::sync_view() {
  if (!view_) return;
  Handle mode = 0;
  if (const int32_t id = program_view_mode_id(); id >= 0) {
    Handle& b = view_modes_[id];
    if (!b) b = alloc_block(0x10);
    poke32(b + 4, static_cast<uint32_t>(id));
    mode = b;
  }
  poke32(view_ + 0x58, mode);
  void* t = program_view_target();
  poke32(view_ + 0x64, t ? handle_of(t) : 0);
}

void GameEngine::set_mission_view(Handle view, Handle camera) {
  view_ = view;
  camera_ = camera;
}

Handle GameEngine::alloc_block(uint32_t size) {
  const Handle h = next_block_;
  next_block_ += (size + 0xff) & ~0xffu;
  return h;
}

}  // namespace mech3
