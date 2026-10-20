// The mission load's hooks wired to the ports (docs/mission-load.md), and the
// pieces of the program a mission needs to come up through the real Load and
// Core states: the pilot's game for a debug launch (our addition), the view
// the Core state draws through. Not original code: it stands where the exe's
// other modules will be. What has no port is logged once by address.
#include <cstdio>
#include <cstring>
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
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
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
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"
#include "gamez/zweapon/zwep_init.h"
#include "gamez/zweapon/zweapon.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_states.h"
#include "mech3/mech3_interp.h"
#include "mech3/mission.h"
#include "mech3/mech_config.h"
#include "mech3/mech_reader.h"
#include "mech3/mech_skin.h"
#include "mech3/mission_world.h"
#include "mech3/options.h"
#include "mech3/player.h"
#include "mech3/zainetedit.h"
#include "platform/render3d.h"

namespace mech3 {

// From mech3/mech_names.h and mech3/weapon.h, whose own Engine and Handle
// clash with app_hooks.h's.
int32_t mech_index(const char* name);                        // 0x004845f0
void make_weapon_type(zrdr::Node* node, zwep::WeaponDef* def);  // 0x004b0020

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
  static MissionWorld m;
  m = MissionWorld{};
  world_init(m);
  if (!world_read_params(m, file, block)) std::fprintf(stderr, "mw3: no block %s in %s\n", block, file);
}

// player.cpp's hooks (docs/mission-start.md): the reader, the records, the
// mech definitions, the nets and zClass are ported; the vehicles, mechs and
// AI controllers are not, so their constructors are logged and give none.
struct WiredPlayerHooks : PlayerHooks {
  GameEngine* e = nullptr;
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return e->unported(key, args);
  }
  template <class T>
  T* as(Handle h) const { return static_cast<T*>(e->object_of(h)); }

  void nets_build_all() override { ai::nets_build_all(); }
  Handle object3d_create() override { return e->handle_of(zcls::object3d_create()); }
  void set_action_priority(Handle node, int32_t p) override { zcls::node_set_priority(as<zcls::Node>(node), p); }
  // The player's tick runs with the action lists (0x004f7d80), which the
  // Core state does not run yet (its lists_update is a hook).
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
  void set_field32(Handle h, uint32_t off, uint32_t v) override { e->poke32(h + off, v); }
  uint32_t field32(Handle h, uint32_t off) override { return e->peek32(h + off); }
  Handle controls_interface(Handle ctl) override { return ctl; }
  void report(int32_t level, int32_t line, const char* text) override {
    if (!e->quiet) std::fprintf(stderr, "mw3: report %#x player.cpp:%d %s\n", level, line, text);
  }
  float frame_dt() override { return e->frame_dt(); }
  Handle pilot_vehicle() override { return e->field32(e->pilot(), 0x14); }
  Handle view() override { return e->player_view(); }
  Handle view_target(Handle v) override { return e->field32(v, 0x64); }
  Handle stealth() override { return e->peek32(0x609a2c); }
  void clear_6099c0() override { e->poke32(0x6099c0, 0); }
};
WiredPlayerHooks g_player_hooks;

struct WiredMissionHooks : MissionHooks {
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
    const std::string dir = mission_dir(e->launch().index);
    const std::vector<std::string> roots = zutl::mounts().read;
    for (const std::string& r : roots) zutl::mount(r + "/zbd/" + dir);
    for (const std::string& r : roots) zutl::mount(r + "/zbd");
    for (int l = 0; l < 2; ++l) zimg::library_list(l).clear();
    zimg::texture_load() = zimg::TextureLoad{};
    zimg::texture_hooks() = zimg::TextureHooks{};
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
    h.begin_scene = [](void*) { zvid::begin_scene(); };
    h.end_scene = [](void*) { zvid::end_scene(); };
    h.flush_transparent = [](void*) { zvid::flush_transparent(); };
    h.flush_overlay = [](void*) { zvid::flush_overlay(); };
    h.puffers = [](void*) { zmod::puff_draw(); };
    h.world_sounds = [](void*, zcls::Node* world) { zcls::world_sounds(world); };  // 0x004f3170
    h.listener_speed_limit = [](void*) { return zsnd::speed_of_sound(); };       // 0x00572360
    h.listener = [](void*, const float* m, const float v[3]) { zsnd::set_listener(m, v); };  // 0x00571e50
  }
  Handle find_node(int32_t list, const char* name) override { return e->handle_of(zcls::list_find_by_name(list, name)); }
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
    return mem;
  }
  Handle make_608c84(Handle mem, Handle world) override {
    unported("0x0047f2b0", {mem, world});
    return mem;
  }
  void anim_allow_loading(int32_t on) override { zeff::anim_allow_loading(on); }
  // The animations' calls out: puffers (gmod_puff) and sounds (zSound) are
  // ported; zEffect's effect table 0x0050d9e0 is not, so a def naming an
  // effect is left out of the post-load pass (anim-postload.md).
  void anim_set_world(Handle world) override {
    zeff::AnimHooks& a = zeff::anim_hooks();
    a.copy_tree = [](zcls::Node*) -> zcls::Node* { return nullptr; };  // dead: copy_trees is never set
    a.create_puffer = []() -> void* { return zmod::puffer_create(); };
    a.find_sound = [](const char* name) -> void* { return zsnd::find(name); };
    a.effect_index = [](const char* name) -> int32_t {
      if (GameEngine* g = engine_of_hooks()) g->unported("0x0050d9e0", {name});
      return -1;
    };
    zcls::set_find_sound(a.find_sound);
    zmod::puff_hooks().create_node = [](void*) { return zcls::object3d_create(); };
    zeff::anim_set_world(mission_node(*e, world));
  }
  void anim_load(const char*) override { zeff::anim_load(); }
  void world_params(const char* file, const char* block) override {
    read_world_params(file, block, mission_node(*e, e->mission_camera()), e->launch().type, e->launch().number);
  }
  void reader_open(const char* path, int32_t current) override { zrdr::open_archive(path, current != 0); }
  // 0x00583880: weapons.zrd into zWeapon's definitions (each def's game half
  // by 0x004b0020, mech3/weapon), the projectile pool and the weapon_tick
  // object (priority 3, its action zwep::tick). Not ported and left out: the
  // impact lights 0x00586970, the defs' destroy animations (0x0050b450),
  // effects, craters' defaults, the pool's objects 0x005847c0.
  void weapons_load(Handle world, const char* file, int32_t a, uint32_t callback) override {
    zwep::InitHooks& w = zwep::init_hooks();
    w = zwep::InitHooks{};
    w.sound = [](const char* n) -> zwep::Handle { return zsnd::find(n); };
    w.message = zutl::localize;
    w.effect = [](const char*) -> int32_t { return -1; };
    w.find_node = [](int32_t list, const char* n) -> zwep::Handle { return zcls::list_find_by_name(list, n); };
    w.set_active = [](zwep::Handle n, int32_t on) { zcls::node_set_active(static_cast<zcls::Node*>(n), on); };
    w.object3d_create = []() -> zwep::Handle { return zcls::object3d_create(); };
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
    mech3::player_setup(full != 0);
  }
  bool player_setup_ran = false;
  Handle sound_find(const char* name) override { return e->sound_find(name); }
  void zerr(int32_t level, const char*, int32_t line, const char* fmt, const char* arg) override {
    char text[512];
    std::snprintf(text, sizeof text, fmt, arg);
    std::fprintf(stderr, "mw3: report %#x mission.cpp:%d %s\n", level, line, text);
  }
};
WiredMissionHooks g_mission_hooks;

}  // namespace

void wire_mission_hooks(GameEngine* e) {
  g_mission_hooks.e = e;
  g_player_hooks.e = e;
  g_mission_engine = e;
  set_mission_hooks(e ? &g_mission_hooks : nullptr);
  set_player_hooks(e ? &g_player_hooks : nullptr);
}

// 0x00490880 on the missions object: the load, then (a debug launch, our
// addition standing in for the briefing's Launch button) the Core state.
void GameEngine::missions_start(uint32_t list) {
  if (list != 0x6098d0) return Engine::missions_start(list);
  Missions& m = missions();
  m.start();
  if (m.failed) std::fprintf(stderr, "mw3: the mission load failed\n");
  zcls::Node* cam = mission_node(*this, camera_);
  if (launch_.active && launch_.have_pos && cam) {  // as the debug viewer places it (render.md section 0)
    zcls::camera_set_rotation(cam, -launch_.pitch, launch_.yaw + 3.14159265f, 0.0f);
    zcls::camera_set_position(cam, launch_.eye[0], launch_.eye[1], launch_.eye[2]);
  }
  if (launch_.active && !m.failed) app().replace(app().state(5), 0);
}

// The debug launch's pilot and game (our addition): [0x0060861c] a pilot
// whose game (+0x6c) answers the type, number and index, with a mech handle
// at +0x14 so the Core state stays (the player's mech is not built yet).
void GameEngine::set_launch(const Launch& l) {
  launch_ = l;
  launch_.active = true;
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

uint32_t GameEngine::vcall(Handle obj, uint32_t slot, std::initializer_list<Arg> args) {
  uint32_t r = 0;
  if (menu_vcall(obj, slot, args, &r)) return r;
  if (obj && obj == game_) {
    switch (slot) {
      case 0x2c: return static_cast<uint32_t>(launch_.type);
      case 0x4c: return static_cast<uint32_t>(launch_.number);
      case 0x54: return static_cast<uint32_t>(launch_.index);
      case 0x28: return 0;
      default: break;
    }
  }
  // The view's +0x04 runs its camera: the world drawn by the scene walk
  // (0x004f7030), the frame counted as the class lists' pass counts it.
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
