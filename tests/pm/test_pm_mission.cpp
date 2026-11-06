// Pirate's Moon missions run headless with the AI on, for what the AI does in play.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zdeclient/zdec_crater.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"
#include "pm/gamez/zmodel/gmod_clip3d.h"
#include "pm/gamez/zmodel/gmod_const.h"
#include "pm/pm_game.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_states.h"
#include "mech3/app_mech.h"
#include "mech3/mech_control.h"
#include "mech3/mission.h"
#include "mech3/app_engine.h"
#include "mech3/app_states.h"
#include "mech3/options.h"
#include "mech3/turret.h"
#include "mech3/zai_valve.h"
#include "mech3/zrdrvalve.h"
#include "gamez/zsound/zsnd.h"
#include "platform/silent_sound.h"

namespace {
struct Window : platform::Window {
  uint32_t create(const std::string&, int, int) override { return 0x1234; }
  void set_title(const std::string&) override {}
  void show() override {}
  void client_size(int* w, int* h) override { *w = 640, *h = 480; }
};
struct Alert : platform::Alert {
  void abort_message(const std::string& m) override { std::fprintf(stderr, "abort: %s\n", m.c_str()); }
};
struct Clock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now += 16; }
};
struct MovieClock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now += 1000; }
};
struct Screen : platform::Screen2D {
  void present_16(const uint16_t*, int, int, int, bool) override {}
};
struct NoKeys : platform::Input {
  bool open(Device d) override { return d == kKeyboard; }
  void close(Device) override {}
  int32_t acquire(Device) override { return 0; }
  int32_t unacquire(Device) override { return 0; }
  int32_t keyboard_data(platform::KeyEvent*, uint32_t* count) override {
    *count = 0;
    return 0;
  }
  int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
  int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
};
struct Pump : platform::MessagePump {
  int passes = 0, limit = 200;
  std::function<bool()> stop;
  bool peek() override { return ++passes > limit || (stop && stop()); }
  bool pump() override {
    mech3::missions().cleanup(false);
    return false;
  }
  void wait(uint32_t) override {}
  void post_quit() override {}
};
int32_t valve_value(const char* name) {
  const mech3::ai::Valve* v = mech3::ai::valve_find(name);
  return v ? v->value->get() : -1;
}
}  // namespace

// c1m1's two Shadow Cats start powered down; the data wakes them 180 s in, by `shadowcat_time`
// through `shadowcat_go`, or when the player's close. The first thing PM's AI was seen not to do.
TEST(pm_c1m1_shadow_cats_wake) {
  // A minute in an optimized build, several in the default: run with the campaign chains.
  if (!std::getenv("MW3_CAMPAIGN") || !std::filesystem::exists(MW3PM_GAME_DIR)) return;
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_c1m1_write";
  std::filesystem::create_directories(wd);
  zutl::set_write_dir(wd.c_str());
  // As a new process starts: the tests before this one leave their state.
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  NoKeys keys;
  Window window;
  Alert alert;
  Clock clock;
  MovieClock movies;
  Screen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
  p.sound = &sound, p.files = &files, p.input = &keys;
  mech3::GameEngine engine(p, "mech3.exe -nofmv");
  engine.quiet = true;
  mech3::Mech3App app(&engine);
  mech3::set_app(&app);
  EXPECT(app.init_instance());
  mech3::GameEngine::Launch l;
  // PM_MISSION=index,type,number,frames: another mission for the same run, for looking at it.
  int limit = 16000;
  const char* other = std::getenv("PM_MISSION");
  if (other) std::sscanf(other, "%d,%d,%d,%d", &l.index, &l.type, &l.number, &limit);
  engine.set_launch(l);
  app.post_intro_ = &mech3::states().load;
  int frames = 0, up0 = -1, up1 = -1, go = -1;
  void* cats[2] = {};
  int turf = 0, turf_ally = 0, tur_other = 0, tur_enemy = 0;
  bool nav = false;
  engine.on_present = [&](int) {
    if (app.current() != &mech3::states().core) return;
    if (frames == 2) {
      nav = mech3::missions().stations != 0;  // waynet_m1, the op points (0x00493a90)
    }
    if (frames == 2) {  // aiTurrets.zrd's ALLY: the turf_ turrets are the player's side (0x004a9ce8)
      for (mech3::tur::Turret* t : mech3::tur::turret_list()) {
        const auto* node = static_cast<zcls::Node*>(engine.object_of(t->node));
        const char* name = node ? node->name() : "";
        if (!std::strncmp(name, "turf", 4)) ++turf, turf_ally += t->team == 2u;
        else if (!std::strncmp(name, "turm_ac", 7) || !std::strncmp(name, "turmf_ac", 8)) ++turf, turf_ally += t->team == 2u;  // ALLY too
        else if (!std::strncmp(name, "tur", 3)) ++tur_other, tur_enemy += t->team == 4u;
      }
    }
    if (++frames == 3) {
      int32_t made = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
      for (int32_t i = 0; i < made; ++i) {
        if (ai[i].name && !std::strcmp(ai[i].name, "shadowcat_01")) cats[0] = ai[i].mech;
        if (ai[i].name && !std::strcmp(ai[i].name, "shadowcat_02")) cats[1] = ai[i].mech;
      }
      std::printf("  c1m1: %d AI mechs, shadow cats %p %p\n", made, cats[0], cats[1]);
    }
    if (frames < 3 || (!cats[0] && !cats[1])) return;
    mech3::LaunchedMech a, b;
    if (cats[0]) mech3::describe_mech(cats[0], &a);
    if (cats[1]) mech3::describe_mech(cats[1], &b);
    if (go < 0 && valve_value("shadowcat_go") > 0) go = frames;
    if (up0 < 0 && cats[0] && !a.powered_down) up0 = frames;
    if (up1 < 0 && cats[1] && !b.powered_down) up1 = frames;
    if (frames % 1000 == 0)
      std::printf("  frame %d (%.0f s): go %d time %d close %d; down %d %d, asked %d %d\n", frames, mech3::ai::valve_hooks().game_time(),
                  valve_value("shadowcat_go"), valve_value("shadowcat_time"), valve_value("shadowcat_close"), a.powered_down,
                  b.powered_down, a.powering_down, b.powering_down);
  };
  Pump pump;
  pump.limit = limit;
  app.run(pump);
  std::printf("  frames %d: go at %d, up at %d and %d\n", frames, go, up0, up1);
  zgame::set_game(zgame::mw3_table());
  if (other) return;
  EXPECT(nav);
  std::printf("  c1m1: turf_ turrets %d, on the player's side %d; others %d, enemy %d\n", turf, turf_ally, tur_other, tur_enemy);
  EXPECT(turf > 0 && turf_ally == turf && tur_enemy == tur_other);
  EXPECT(cats[0] && cats[1]);
  EXPECT(go > 0);
  EXPECT(up0 > 0 && up1 > 0);
}

// The one destroy valve PM ships that names a unit: mission 4's `werewolf_destroyed`, any of
// `werewolf_destroyed_ready` and `werewolf_down`, kills `werewolf` through the kill 0x41de00 (0x4f5210
// calls vt+0x78). PM has no gas: MW3's c2 m2 records aren't in its data.
TEST(pm_c2m4_destroy_valve_kills_the_werewolf) {
  // Not after the Shadow Cats in one process: what a mission leaves here crashes a second one (zcls's lists).
  if (std::getenv("MW3_CAMPAIGN") || !std::filesystem::exists(MW3PM_GAME_DIR)) return;
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_c2m4_write";
  std::filesystem::create_directories(wd);
  zutl::set_write_dir(wd.c_str());
  // As a new process starts: the tests before this one leave their state.
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  NoKeys keys;
  Window window;
  Alert alert;
  Clock clock;
  MovieClock movies;
  Screen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
  p.sound = &sound, p.files = &files, p.input = &keys;
  mech3::GameEngine engine(p, "mech3.exe -nofmv");
  engine.quiet = true;
  mech3::Mech3App app(&engine);
  mech3::set_app(&app);
  EXPECT(app.init_instance());
  mech3::GameEngine::Launch l;
  l.index = 1, l.number = 4;
  engine.set_launch(l);
  app.post_intro_ = &mech3::states().load;
  int frames = 0, before = -1, after = -1, unit_valve = -1;
  int soil_on = 0, soil_all = 0, tex_names = 0;
  int crater = -9, crater_polys = 0, crater_refs = 0;
  float player_height = 0, centroid_over = 0;
  size_t crater_history = 0;
  void* werewolf = nullptr;
  const auto state = [](void* m) {
    mech3::LaunchedMech o;
    mech3::describe_mech(m, &o);
    return o.anim_state;
  };
  engine.on_present = [&](int) {
    if (app.current() != &mech3::states().core) return;
    if (++frames == 5) {
      int32_t made = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
      for (int32_t i = 0; i < made; ++i)
        if (ai[i].name && !std::strcmp(ai[i].name, "werewolf")) werewolf = ai[i].mech;
      if (werewolf) before = state(werewolf);
    }
    if (frames == 20) {
      unit_valve = valve_value("werewolf_destroyed");
      if (mech3::ai::Valve* v = mech3::ai::valve_find("werewolf_destroyed_ready")) v->set(1);  // on_mission_start's, 50 s in
    }
    if (frames == 40 && werewolf) after = state(werewolf);
    if (frames == 10) {  // a crater cuts PM's ground: the ground models replaced, its own drawn by material
      mech3::LaunchedMech o;
      mech3::describe_mech(mech3::mech_mission_player(), &o);
      zwep::CraterOrder order;
      order.x00 = 0x1008, order.points = 10, order.depth = 0.75f, order.radius = 5.0f;
      order.at = {o.pos[0], o.pos[1], o.pos[2]};
      player_height = o.height;
      float centroid[3] = {};
      if (void* c = mech3::mech_centroid_of(mech3::mech_mission_player()))
        zcls::node_world_position(static_cast<zcls::Node*>(c), centroid);
      centroid_over = centroid[1] - o.pos[1];
      crater = zdec::crater_instance(&order, 0);
      crater_history = zdec::craters().history.size();
      if (zcls::Node* n = zcls::list_find_by_name(6, "ZDEC_FEATURE"); n && n->pm_model)
        crater_polys = n->pm_model->count(0x10), crater_refs = n->pm_model->count(0x5c);
    }
    if (frames == 5) {  // the skins reach PM's models (0x00476610): "mech" soil on the mech's materials
      mech3::MechTextures tex;
      if (mech3::mech_textures(mech3::mech_mission_player(), &tex))
        soil_on = tex.mech_soil, soil_all = tex.polygons, tex_names = static_cast<int>(tex.names.size());
    }
  };
  Pump pump;
  pump.limit = 3000;
  pump.stop = [&] { return frames >= 41; };
  app.run(pump);
  std::printf("  c2m4: werewolf %p, state %d -> %d, the valve was %d, frames %d\n", werewolf, before, after, unit_valve, frames);
  zgame::set_game(zgame::mw3_table());
  EXPECT(werewolf);
  EXPECT(before != 10);  // Mech::Die's +0x578
  EXPECT(after == 10);
  std::printf("  c2m4: the player's polygons on mech soil %d of %d, textures %d\n", soil_on, soil_all, tex_names);
  EXPECT(soil_all > 100 && soil_on > soil_all / 2 && tex_names > 100);
  // The box behind PM's +0x70 pointer: at 0 the chase camera, aim and steps all sat at the feet (0x004fd9d0).
  std::printf("  c2m4: the player's height %.2f, centroid %.2f over its place\n", player_height, centroid_over);
  EXPECT(player_height > 5.0f && player_height < 30.0f);
  EXPECT(centroid_over > player_height * 0.45f && centroid_over < player_height * 0.55f);
  std::printf("  c2m4: crater %d, its polygons %d, material refs %d, history %zu\n", crater, crater_polys, crater_refs,
              crater_history);
  EXPECT(crater == 0 && crater_polys >= 20 && crater_refs > 0 && crater_history == 1);  // 10 quads, 10 triangles; a bent quad is 2
  EXPECT(pm::zmod::clip3d_state().overruns == 0);  // the clippers stayed in their arena (gmod_draw.cpp)
  // Leaving the mech lab purges what no model uses (0x005394c0): the world's materials stay.
  const int32_t materials = ::zmod::materials().count;
  ::zmod::models_purge(true, true);
  std::printf("  c2m4: materials %d before the lab's purge, %d after\n", materials, ::zmod::materials().count);
  EXPECT(materials > 1000 && ::zmod::materials().count > 1000);
}
