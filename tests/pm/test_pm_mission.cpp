// Pirate's Moon missions run headless with the AI on, for what the AI does in play.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "check.h"
#include "gamez/ztwod/twod_weather.h"
#include "mech3/app_hud.h"
#include "mech3/app_view.h"
#include "mech3/player_view.h"
#include "mech3/app_vehicle.h"
#include "gamez/zutil/zfix.h"
#include "mech3/player.h"
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
#include "mech3/mech_objective.h"
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
  bool nav = false, still_core = false;
  int camp_done = 0, camp_failed = 0;
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
    if (frames == 13000)  // the camp lost: "Protect Rim Collective Camp"'s fails (0x004600d0)
      if (mech3::ai::Valve* v = mech3::ai::valve_find("collective_camp1")) v->set(1);
    if (frames == 13100) {
      mech3::Objectives& o = mech3::mission_objectives();
      if (o.secondary && !o.secondary->items.empty()) {
        const mech3::Objective& camp = *o.secondary->items.front();
        camp_done = camp.done, camp_failed = camp.failed;
      }
      still_core = app.current() == &mech3::states().core;
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
  std::printf("  c1m1: the camp's objective done %d failed %d, still in the mission %d\n", camp_done, camp_failed, still_core);
  EXPECT(camp_done && camp_failed && still_core);
  std::printf("  c1m1: turf_ turrets %d, on the player's side %d; others %d, enemy %d\n", turf, turf_ally, tur_other, tur_enemy);
  EXPECT(turf > 0 && turf_ally == turf && tur_enemy == tur_other);
  EXPECT(cats[0] && cats[1]);
  EXPECT(go > 0);
  EXPECT(up0 > 0 && up1 > 0);
}

// The one destroy valve PM ships that names a unit: mission 4's `werewolf_destroyed`, any of
// `werewolf_destroyed_ready` and `werewolf_down`, kills `werewolf` through the kill 0x41de00 (0x4f5210
// calls vt+0x78). PM has no gas: MW3's c2 m2 records aren't in its data.
// PM_KILL=index,type,number: the player's real weapons, all of them each frame, at each AI mech of a mission in turn,
// at its torso from close by, until it's dead. The player can't die or run dry. One mission a process.
extern int pm_kill_on_part, pm_kill_off_part;  // pm_kill.cpp
void pm_kill_begin(void* player);
int pm_kill_fire(void* player, const float at[3]);
void pm_kill_left(void* mech, char* out, size_t n);
void pm_kill_ray(const float from[3], const float to[3]);
uint32_t pm_kill_root_flags(void* mech);
void pm_kill_tree(void* mech);
int pm_leg_fire(void* player, void* mech, int weapon, int leg, int shown, bool ready);
void pm_leg_reset();
const char* pm_weapon_name(void* player, int i);
bool pm_kill_only_leg_lost(void* mech, int leg);
int pm_missile_step(void* player, void* mech, int weapon, bool lock, bool artemis);
int pm_missile_hits();
float pm_kill_ground(float x, float y, float z);
void pm_kill_loadout(void* player);
void pm_kill_fall_line(void* mech, int t);
bool pm_kill_falling_sound(void* mech);
void pm_kill_hurt(void* mech);
float pm_kill_leg_armor(void* mech);
void pm_kill_ground_line(float x0, float z0, float x1, float z1);
void pm_kill_weapons(void* mech);
void pm_kill_scales(void* mech);
// Pirate's Moon 1.0 @ 0x004a4490, 0x004a4440: the box window opens before the player's update and the walk and closes after both.
struct TickOrder : mech3::PlayerHooks {
  std::string calls;
  float reader_item_float(mech3::Handle, int32_t) override { return 0; }
  mech3::Handle reader_item(mech3::Handle, int32_t) override { return 0; }
  int32_t dfn_config_count(mech3::Handle) override { return 0; }
  mech3::Handle dfn_config(mech3::Handle, int32_t) override { return 0; }
  int32_t dfn_skin_count(mech3::Handle) override { return 0; }
  mech3::Handle dfn_skin(mech3::Handle, int32_t) override { return 0; }
  void set_field32(mech3::Handle, uint32_t, uint32_t) override {}
  uint32_t field32(mech3::Handle, uint32_t) override { return 0; }
  mech3::Handle controls_interface(mech3::Handle) override { return 0; }
  float frame_dt() override { return 0.033f; }
  mech3::Handle pilot_vehicle() override { return 0; }
  mech3::Handle view() override { return 0; }
  mech3::Handle view_target(mech3::Handle) override { return 0; }
  mech3::Handle stealth() override { return 8; }
  void clear_6099c0() override { calls += 'c'; }
  void entities_before(float) override { calls += '('; }
  void vehicle_update(mech3::Handle, float) override { calls += 'p'; }
  void entities_update(float) override { calls += 'w'; }
  void entities_after(float) override { calls += ')'; }
  void tick_41e3f0(float) override { calls += 'd'; }
  int32_t game_type() override { return 0; }
  void view_tick(mech3::Handle, float) override { calls += 'v'; }
};

TEST(pm_player_tick_box_window) {
  TickOrder o;
  mech3::PlayerHooks& was = mech3::player_hooks();
  mech3::set_player_hooks(&o);
  mech3::player_tick();
  mech3::set_player_hooks(&was);
  EXPECT(o.calls == "c(pw)dv");
}

static bool g_refit_point = false, g_refit_done = false;
static float g_fall_lowest = 1e9f, g_fall_died_at = 1e9f;
static bool g_fall_failed = false, g_fall_ai_dead = false;
static int g_fall_mode = -1, g_fall_frames_dead = 0, g_fall_sound_from = 0, g_fall_sound_frames = 0;
static float g_fall_camera_up = 0;
static int g_cloud_frames = 0, g_cloud_wrong = 0, g_cloud_full = 0, g_cloud_ramp = 0, g_cloud_mode = -1;

static bool me_alive(void* player) {
  mech3::LaunchedMech me;
  mech3::describe_mech(player, &me);
  return me.anim_state != 10;
}

static void kill_run(const char* which, bool quick) {
  if (!which || !std::filesystem::exists(MW3PM_GAME_DIR)) return;
  if (std::getenv("PM_BUGS_OFF")) zfix::options().bugs = false;
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_kill_write";
  std::filesystem::create_directories(wd);
  zutl::set_write_dir(wd.c_str());
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
  std::sscanf(which, "%d,%d,%d", &l.index, &l.type, &l.number);
  if (const char* e = std::getenv("PM_MECH")) l.mech = std::atoi(e);
  engine.set_launch(l);
  app.post_intro_ = &mech3::states().load;
  std::vector<void*> targets;
  std::vector<const char*> names;
  size_t at = 0;
  int frames = 0, since = 0, dead = 0, stuck = 0, shots = 0, hits0 = 0, off0 = 0, box_during = 0, box_left = 0, box_field = 0, box_stealth = 0;
  bool done = false;
  const auto state = [](void* m) {
    mech3::LaunchedMech o;
    mech3::describe_mech(m, &o);
    return o.anim_state;
  };
  engine.on_present = [&](int) {
    if (app.current() != &mech3::states().core) return;
    if (++frames < 10) return;
    void* player = mech3::mech_mission_player();
    if (!player) return;
    if (targets.empty()) {
      int32_t made = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
      for (int32_t i = 0; i < made; ++i) {
        targets.push_back(ai[i].mech), names.push_back(ai[i].name);
        std::printf("  frame %d: %s root flags %x\n", frames, ai[i].name ? ai[i].name : "?", pm_kill_root_flags(ai[i].mech));
      }
      std::printf("  %d AI mechs; the player's root flags %x\n", made, pm_kill_root_flags(player));
      pm_kill_begin(player);
      if (std::getenv("PM_KILL_TREE") && made) pm_kill_tree(ai[0].mech);
      pm_kill_weapons(player);
      for (int32_t i = 0; i < made; ++i) std::printf("  %s:", ai[i].name ? ai[i].name : "?"), pm_kill_scales(ai[i].mech);
    }
    while (at < targets.size() && state(targets[at]) == 10) {
      char left[200];
      pm_kill_left(targets[at], left, sizeof left);
      std::printf("  %s dead: %d frames, %d shots, %d hits on a part, %d off: %s\n", names[at] ? names[at] : "?", since, shots, pm_kill_on_part - hits0,
                  pm_kill_off_part - off0, left);
      if (const char* leg = std::getenv("PM_LEG"); leg && std::atoi(leg) == 5 && at == 0) {  // ER Large at the Thor: docs/pm-map.md
        EXPECT(shots >= 11 && shots <= 14);
        EXPECT(pm_kill_only_leg_lost(targets[at], 0));
      }
      ++dead, ++at, since = 0, shots = 0, hits0 = pm_kill_on_part, off0 = pm_kill_off_part;
      pm_leg_reset();
    }
    if (at >= targets.size() || (quick && at > 0)) {
      done = true;
      return;
    }
    void* m = targets[at];
    float there[3];
    mech3::LaunchedMech info;
    mech3::describe_mech(m, &info);
    mech3::mech_world_position(m, there);
    int32_t during = 0, left_on = 0, fields = 0, stealth = 0;
    mech3::entities_box_counts(&during, &left_on, &fields, &stealth);
    box_field = std::max(box_field, fields), box_stealth = std::max(box_stealth, stealth);
    box_during = std::max(box_during, during), box_left = std::max(box_left, left_on);
    if (const char* mfb = std::getenv("PM_MFB")) {  // x,z: the player there, the MFB command twice, each base's way to its point
      static int t = 0, order_at = 0;
      static float dock[3] = {};
      float x = 0, z = 0;
      std::sscanf(mfb, "%f,%f", &x, &z);
      ++t;
      if (t == 2) {
        const float at[3] = {x, pm_kill_ground(x, 300.0f, z), z};
        mech3::move_mech(player, at, 0.0f);
      }
      if ((t == 30 || t == 31) && !std::getenv("PM_MFB_NO_CALL")) mech3::program_mcb_command(player);
      if (t == 32) {
        const char* name = nullptr;
        const bool sent = mech3::program_dock_point(&name, dock);
        std::printf("  mfb: sent %d to %s at %.1f %.1f %.1f, %d bases\n", sent ? 1 : 0, name ? name : "-", dock[0], dock[1], dock[2], mech3::program_field_bases_count());
        order_at = t;
        g_refit_point = name && std::strstr(name, "starbase") != nullptr;
      }
      static float base0[3][2], health0 = 0;
      static int down_at = 0, drive_off = 0;
      if (std::getenv("PM_MFB_NO_CALL")) {  // the control: where the mission takes the bases by itself
        if (t == 33 || t % 300 == 0) {
          std::printf("  alone %4d s:", t / 60);
          for (int i = 0; i < 3; ++i) {
            mech3::VehicleSummary v;
            if (mech3::describe_vehicle(mech3::program_field_base(i), &v)) std::printf(" [%.0f %.0f]", v.pos[0], v.pos[2]);
          }
          std::printf("\n");
        }
        if (t > 1300) done = true;
        return;
      }
      if (t == 33 && !g_refit_point) {  // an ordinary point: the log alone
        order_at = t;
      } else if (t == 33) {
        for (int i = 0; i < 3; ++i) {
          mech3::VehicleSummary v;
          if (mech3::describe_vehicle(mech3::program_field_base(i), &v)) base0[i][0] = v.pos[0], base0[i][1] = v.pos[2];
        }
        pm_kill_hurt(player);
        health0 = pm_kill_leg_armor(player);
        mech3::move_mech(player, dock, 0.0f);
      }
      if (g_refit_point && t == 60) mech3::mech_power(player, 1), down_at = t;
      if (g_refit_point && t > 60 && !drive_off && !mech3::program_dock_contact()) drive_off = t;
      if (drive_off) {
        float moved = 0;
        for (int i = 0; i < 3; ++i) {
          mech3::VehicleSummary v;
          if (mech3::describe_vehicle(mech3::program_field_base(i), &v)) moved = std::max(moved, std::hypot(v.pos[0] - base0[i][0], v.pos[1 + 1] - base0[i][1]));
        }
        std::printf("  refit over at frame %d, %d after power down: leg structure %.3f to %.3f, bases moved %.1f m\n", t, t - down_at, health0, pm_kill_leg_armor(player), moved);
        EXPECT(pm_kill_leg_armor(player) > health0);
        EXPECT(moved < 1.0f);
        g_refit_done = true;
        done = true;
        return;
      }
      if (order_at && (t - order_at) % 300 == 0) {
        std::printf("  mfb %5d s:", (t - order_at) / 60);
        for (int i = 0; i < 3; ++i) {
          void* b = mech3::program_field_base(i);
          mech3::VehicleSummary v;
          if (b && mech3::describe_vehicle(b, &v))
            std::printf(" [%.0f %.0f d %.0f mode %d]", v.pos[0], v.pos[2], std::hypot(v.pos[0] - dock[0], v.pos[2] - dock[2]), v.mode);
        }
        std::printf("\n");
      }
      if (t > 12000) done = true;
      return;
    }
    if (std::getenv("PM_LOADOUT")) {
      pm_kill_loadout(player);
      done = true;
      return;
    }
    if (const char* e = std::getenv("PM_FALL")) {  // x,z: from the rim, down into the canyon
      static int t = 0;
      float x = 0, z = 0;
      std::sscanf(e, "%f,%f", &x, &z);
      ++t;
      if (t == 2) {
        const float at[3] = {x, 30.0f, z};
        mech3::move_mech(player, at, 0.0f);
      }
      if (const char* c = std::getenv("PM_FALL_CAMERA")) {
        if (t == 2) mech3::program_view_set_camera(std::atoi(c));
        if (me_alive(player) && mech3::program_view_mode_id() >= 0) g_cloud_mode = mech3::program_view_mode_id();
      }
      {  // the tint this frame's camera height asks for, what the weather layer holds
        double a = 0;
        const mech3::CloudCover cover = mech3::program_cloud_cover();
        const ztwod::ScreenTint& tint = ztwod::weather_layer().tint;
        if (mech3::cloud_fill(cover, mech3::program_view_camera_height(), &a)) {
          ++g_cloud_frames;
          g_cloud_full += a >= 1.0 ? 1 : 0;
          g_cloud_ramp += a < 1.0 ? 1 : 0;
          if (tint.alpha != a || tint.colour != cover.colour) ++g_cloud_wrong;
        }
      }
      if (t == 3 && targets.size() > 1) {  // an AI mech in with it, a little along
        const float at[3] = {x + 40.0f, 10.0f, z};
        mech3::move_mech(targets[1], at, 0.0f);
      }
      if (t % 20 == 0 && !quick) pm_kill_fall_line(player, t);
      mech3::LaunchedMech me;
      mech3::describe_mech(player, &me);
      float there_y[3];
      mech3::mech_world_position(player, there_y);
      g_fall_lowest = std::min(g_fall_lowest, there_y[1]);
      if (me.anim_state == 10 && g_fall_died_at > 0.0f) g_fall_mode = mech3::program_view_mode_id();
      if (me.anim_state == 10) {
        ++g_fall_frames_dead;
        const float up = mech3::program_view_camera_height() - there_y[1];
        if (g_fall_frames_dead == 10) g_fall_camera_up = up;
        if (pm_kill_falling_sound(player)) {
          if (g_fall_sound_from == 0) g_fall_sound_from = g_fall_frames_dead;
          ++g_fall_sound_frames;
        }
      }
      if (me.anim_state == 10 && g_fall_died_at > 0.0f) g_fall_died_at = there_y[1], g_fall_failed = engine.mission_failed();
      if (targets.size() > 1 && state(targets[1]) == 10) g_fall_ai_dead = true;
      g_fall_failed = g_fall_failed || engine.mission_failed();
      if (t > 300) done = true;
      return;
    }
    if (std::getenv("PM_GROUND")) {
      pm_kill_ground_line(2517, -4413, 3181, -4860);
      pm_kill_ground_line(2700, -4800, 3200, -4300);
      done = true;
      return;
    }
    int missile[5] = {-1, 0, 0, 300, 0};
    if (const char* e = std::getenv("PM_MISSILE")) std::sscanf(e, "%d,%d,%d,%d,%d", &missile[0], &missile[1], &missile[2], &missile[3], &missile[4]);
    if (missile[0] >= 0 && static_cast<int>(at) < missile[4]) { ++at; return; }
    if (since++ % 60 == 0 && missile[0] < 0) {
      const float mine[3] = {there[0] - 40.0f, there[1], there[2]};
      mech3::move_mech(player, mine, -1.5707964f);  // facing +x
    }
    if (missile[0] >= 0) {  // once, at the range, then the shooter stands
      if (since == 1) {
        std::printf("  == %s at %s, lock %d artemis %d range %d, state %d\n", pm_weapon_name(player, missile[0]), names[at] ? names[at] : "?", missile[1], missile[2], missile[3], info.anim_state);
      }
      static float mine[3];
      if (since == 1) {  // level ground at the range, as a player would pick
        float best = 1e9f;
        const float here = pm_kill_ground(there[0], there[1] + 200.0f, there[2]);
        for (int k = 0; k < 32; ++k) {
          const float a = 6.2831853f * static_cast<float>(k) / 32.0f, x = there[0] + std::cos(a) * static_cast<float>(missile[3]), z = there[2] + std::sin(a) * static_cast<float>(missile[3]);
          const float g = pm_kill_ground(x, there[1] + 200.0f, z);
          if (std::fabs(g - here) < best) best = std::fabs(g - here), mine[0] = x, mine[1] = g, mine[2] = z;
        }
        std::printf("  shooter %.0f %.1f %.0f, target ground %.1f\n", mine[0], mine[1], mine[2], here);
      }
      if (since > 1) {  // the shooter holds its ground and keeps the reticle on the target
        const float dx = there[0] - mine[0], dz = there[2] - mine[2];
        mech3::move_mech(player, mine, std::atan2(-dx, -dz));
      }
      if (since > 20) shots += pm_missile_step(player, m, missile[0], missile[1] != 0, missile[2] != 0);
      if (since > 1500) {
        std::printf("  %s: %d salvos, %d hits; state %d\n", names[at] ? names[at] : "?", shots, pm_missile_hits(), info.anim_state);
        done = true;
      }
      return;
    }
    if (const char* leg = std::getenv("PM_LEG")) {  // weapon index: one weapon, ready shots only, at a leg
      const int weapon = std::atoi(leg);
      if (since == 1) std::printf("  == %s at %s, height %.1f\n", pm_weapon_name(player, weapon), names[at] ? names[at] : "?", info.height);
      shots += pm_leg_fire(player, m, weapon, 0, quick ? 0 : 24, quick);
      if (state(m) != 10 && since > 3600) {
        char left[200];
        pm_kill_left(m, left, sizeof left);
        std::printf("  %s alive after %d shots, %d on a part, %d off: %s\n", names[at] ? names[at] : "?", shots, pm_kill_on_part - hits0, pm_kill_off_part - off0, left);
        ++stuck, ++at, since = 0, shots = 0, hits0 = pm_kill_on_part, off0 = pm_kill_off_part;
        pm_leg_reset();
      }
      return;
    }
    const float aim[3] = {there[0], there[1] + info.height * 0.6f, there[2]};
    shots += pm_kill_fire(player, aim);
    if (since == 30 || since == 31) {
      float me[3];
      mech3::mech_world_position(player, me);
      std::printf("  %s at %.1f %.1f %.1f height %.1f state %d down %d, player at %.1f %.1f %.1f, weapons %d\n", names[at] ? names[at] : "?", there[0],
                  there[1], there[2], info.height, info.anim_state, info.powered_down, me[0], me[1], me[2], info.weapons);
      const float from[3] = {aim[0] - 40.0f, aim[1], aim[2]};
      pm_kill_ray(from, aim);
    }
    char left[200];
    if (since % 300 == 0) {
      pm_kill_left(m, left, sizeof left);
      std::printf("  %s after %d frames, %d shots, hits on a part %d, off %d: %s\n", names[at] ? names[at] : "?", since, shots,
                  pm_kill_on_part - hits0, pm_kill_off_part, left);
    }
    if (since > 3600) {
      ++stuck, ++at, since = 0, shots = 0, hits0 = pm_kill_on_part, off0 = pm_kill_off_part;
    }
  };
  Pump pump;
  pump.limit = 60000;
  pump.stop = [&] { return done; };
  app.run(pump);
  std::printf("  %s: %zu AI mechs, %d killed, %d not; hits on a part %d, off a part %d; frames %d\n", which, targets.size(), dead, stuck,
              pm_kill_on_part, pm_kill_off_part, frames);
  zgame::set_game(zgame::mw3_table());
  std::printf("  roots boxed during the frame, most %d; left boxed after it, most %d; field bases boxed %d, the stealth object %d\n", box_during, box_left, box_field, box_stealth);
  EXPECT(!targets.empty());
  EXPECT(box_during > static_cast<int>(targets.size()));  // on through the frame: 0x005cb4e0 (the reticle alone boxes its near contacts)
  EXPECT(box_left == 0);   // and off for the shots: 0x005cb510
  const char* leg_mode = std::getenv("PM_LEG");
  if (!leg_mode || std::atoi(leg_mode) == 5) EXPECT(stuck == 0);  // the slower weapons run out of time or ammo
  if (quick && !std::getenv("PM_FALL")) EXPECT(dead == 1 || g_refit_done);
  EXPECT(pm::zmod::clip3d_state().overruns == 0);
}

// Always on: the same fall with the camera by the cockpit and outside: entering the cloud
// tints the screen to its colour by the camera's height (docs/vehicles.md, The cloud's screen tint).
TEST(pm_cloud_screen_tint) {
#if !defined(_WIN32)
  for (const char* camera : {"1", "2"}) {
    std::fflush(nullptr);
    const pid_t child = fork();
    if (child == 0) {
      setenv("PM_FALL", "3056,-4776", 1);
      setenv("PM_FALL_CAMERA", camera, 1);
      kill_run("0,1,2", true);
      std::printf("  camera %s (mode %d): %d frames in the cloud, %d on the ramp, %d full, %d wrong\n", camera, g_cloud_mode, g_cloud_frames, g_cloud_ramp, g_cloud_full, g_cloud_wrong);
      EXPECT(g_cloud_ramp > 0);
      if (std::strcmp(camera, "2") == 0) EXPECT(g_cloud_full > 0);  // the cockpit dies at the feet's -30, its eye still on the ramp
      EXPECT(g_cloud_wrong == 0);
      std::fflush(nullptr);
      _exit(check::failures ? 1 : 0);
    }
    int status = 1;
    waitpid(child, &status, 0);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }
#endif
}

TEST(pm_enemies_die_under_real_fire) { kill_run(std::getenv("PM_KILL"), false); }

// Always on: c1m1's Thor, one ER Large, every shot on its leg: it goes down with the leg, the player's hits on legs only.
// Always on: mission 2's canyon, cloud_cover's death band: the player dropped in dies at y = -30, in the air, the
// mission failed; an AI mech dies there too (docs/vehicles.md, The cloud's death band).
TEST(pm_canyon_death_band) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child == 0) {
    setenv("PM_FALL", "3056,-4776", 1);
    kill_run("0,1,2", true);
    std::printf("  canyon: died at y %.2f, lowest %.2f, mission failed %d, the AI mech dead %d\n", g_fall_died_at, g_fall_lowest, g_fall_failed ? 1 : 0, g_fall_ai_dead ? 1 : 0);
    EXPECT(g_fall_died_at < -30.0f && g_fall_died_at > -36.0f);
    EXPECT(g_fall_failed);
    EXPECT(g_fall_ai_dead);
    std::printf("  the god's eye: mode %d, %.1f m over the body; mech_falling from dead frame %d for %d frames\n", g_fall_mode, g_fall_camera_up, g_fall_sound_from, g_fall_sound_frames);
    EXPECT(g_fall_mode == 3);                        // 0x00408a70(3)
    EXPECT(g_fall_camera_up > 100.0f);
    EXPECT(g_fall_sound_from >= 1 && g_fall_sound_from <= 2);  // the fall action's start, the frame after
    EXPECT(g_fall_sound_frames > 30);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  waitpid(child, &status, 0);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// Always on: mission 1's static MFB: called from the start, no base moves, the mech at the point is refitted and the dock ends.
TEST(pm_starbase_refit) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child == 0) {
    setenv("PM_MFB", "4476,-4486", 1);
    kill_run("0,1,1", true);
    std::fflush(nullptr);
    _exit(check::failures || !g_refit_done ? 1 : 0);
  }
  int status = 1;
  waitpid(child, &status, 0);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

TEST(pm_leg_down) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = fork();  // a second mission in one process crashes (pm_c2m4's note)
  if (child == 0) {
    setenv("PM_LEG", "5", 1);
    kill_run("0,1,1", true);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  waitpid(child, &status, 0);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

TEST(pm_c2m4_destroy_valve_kills_the_werewolf) {
  // Not after the Shadow Cats in one process: what a mission leaves here crashes a second one (zcls's lists).
  if (std::getenv("MW3_CAMPAIGN") || std::getenv("PM_KILL") || !std::filesystem::exists(MW3PM_GAME_DIR)) return;
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
