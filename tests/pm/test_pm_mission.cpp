// Pirate's Moon missions run headless with the AI on, for what the AI does in play.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <functional>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "gamez/zmath/zmth_matrix.h"
#include "check.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zclass/light.h"
#include "mech3/zainet_inout.h"
#include "crt/msvc_rand.h"
#include "gamez/ztwod/twod_weather.h"
#include "mech3/app_hud.h"
#include "mech3/app_sensors.h"
#include "mech3/app_turret.h"
#include "mech3/turret.h"
#include "mech3/sensors.h"
#include "mech3/app_view.h"
#include "mech3/player_view.h"
#include "mech3/app_vehicle.h"
#include "gamez/zutil/zfix.h"
#include "mech3/player.h"
#include "mech3/pilot.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/object3d.h"
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
#include "mech3/zai_target.h"
#include "mech3/zai_wire_orders_pm.h"
#include "mech3/mech_objective.h"
#include "mech3/mission.h"
#include "mech3/app_engine.h"
#include "mech3/app_states.h"
#include "mech3/options.h"
#include "mech3/turret.h"
#include "mech3/zai_valve.h"
#include "mech3/zrdrvalve.h"
#include "mech3/zai_wire_pm.h"
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

float pm_kill_ground(float x, float y, float z);  // pm_kill.cpp

// c1m1's two Shadow Cats start powered down; the data wakes them 180 s in, by `shadowcat_time`
// through `shadowcat_go`, or when the player's close. The first thing PM's AI was seen not to do.
static void shadow_cats_run();
TEST(pm_c1m1_shadow_cats_wake) {
  // A minute in an optimized build, several in the default: run with the campaign chains.
  if (!std::getenv("MW3_CAMPAIGN") || !std::filesystem::exists(MW3PM_GAME_DIR)) return;
#if !defined(_WIN32)
  // In a child, as the other whole-mission runs: the mission's world stays on zClass's lists after the run, and
  // the next mission (in each later test's child) then counts two worlds and updates the old one's freed cells.
  // tools/pm-hits counts in this process and writes at its exit: that run (this test alone) stays in it.
  if (std::getenv("PM_AI_HITS")) return shadow_cats_run();
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child == 0) {
    shadow_cats_run();
    // _exit skips the atexit report: the seeded pair compares these lines, so the child prints them itself.
    for (const auto& [key, n] : mech3::ai::pm_hooks_missing()) std::printf("mw3: pm ai: %8d %s\n", n, key.c_str());
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  waitpid(child, &status, 0);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else
  shadow_cats_run();
#endif
}

static void shadow_cats_run() {
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
  // The clock seeds the game's rand (time(), as the exe): a fixed seed makes two runs repeat.
  const std::optional<uint32_t> seed_was = crt::rand_fixed_seed();
  // PM_SEED=n another fixed seed: one seed's run takes one path; a score over several is the fairer one (pm-tests).
  crt::rand_fixed_seed() = std::getenv("PM_SEED") ? static_cast<uint32_t>(std::atoi(std::getenv("PM_SEED"))) : 1u;
  struct SeedBack {
    std::optional<uint32_t> was;
    ~SeedBack() { crt::rand_fixed_seed() = was; }
  } seed_back{seed_was};
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
  // PM_HIT_MATE=<frame>: the player hits a lancemate once (its controls' hit, as Mech::take_damage reports one), then
  // the run says whether it takes the player as its target (docs/pm-tests.md, One hit turns a lancemate).
  // PM_HIT_MATE=f1[,f2] hits at those frames; PM_HIT_DAMAGE=d1[,d2] their raw damage (1 by default).
  const char* hit_mate = std::getenv("PM_HIT_MATE");
  int hit_frames[2] = {-1, -1};
  float hit_damage[2] = {1.0f, 1.0f};
  if (hit_mate) std::sscanf(hit_mate, "%d,%d", &hit_frames[0], &hit_frames[1]);
  if (const char* d = std::getenv("PM_HIT_DAMAGE")) std::sscanf(d, "%f,%f", &hit_damage[0], &hit_damage[1]);
  const int hit_frame = hit_frames[0];
  void* mate = nullptr;
  void* mate_controls = nullptr;
  int on_player = 0, first_on = -1, last_on = -1;
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
      if (hit_frame >= 0) {
        const mech3::ai::AiTarget* pt = mech3::mech_ai_target(mech3::mission_player_mech());
        for (int32_t i = 0; i < made; ++i) {
          const mech3::ai::AiTarget* t = mech3::mech_ai_target(ai[i].mech);
          std::printf("  c1m1: %s team %x allies %x enemies %x\n", ai[i].name ? ai[i].name : "?", t->team, t->allies, t->enemies);
          if (!mate && pt && (t->team == pt->team || (t->allies & pt->team))) mate = ai[i].mech, mate_controls = ai[i].controls;
        }
        if (pt) std::printf("  c1m1: player team %x allies %x enemies %x; lancemate %p\n", pt->team, pt->allies, pt->enemies, mate);
      }
    }
    for (int k = 0; k < 2; ++k)
      if (mate && frames == hit_frames[k]) {
        auto* mc = static_cast<mech3::AiControls*>(mate_controls);
        if (std::getenv("PM_HIT_REACTS") && mc->brain) mc->brain->params.put<int32_t>(0x9c, 1);  // as scan_hit ( 1 1 )
        mc->on_hit(mech3::mission_player_mech(), hit_damage[k]);
        const mech3::ai::AiTarget* t = mech3::mech_ai_target(mate);
        std::printf("  c1m1: hit %d (%g) at %g s: her team %x allies %x enemies %x\n", k + 1, hit_damage[k],
                    mech3::ai::brain_hooks().now(), t->team, t->allies, t->enemies);
      }
    // PM_WALK_TO_ENEMY=<frame>: from then the player steps toward the nearest enemy AI mech (0.25 m a frame, about
    // 15 m/s) until 80 m off, for seeing whether it engages (the named "Puma engages" check, c1m2).
    if (const char* walk = std::getenv("PM_WALK_TO_ENEMY"); walk && frames >= std::atoi(walk)) {
      void* pl = mech3::mission_player_mech();
      const mech3::ai::AiTarget* pt = pl ? mech3::mech_ai_target(pl) : nullptr;
      int32_t made = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
      float at[3], best[3] = {}, d2 = 1e30f;
      const char* who = "";
      if (pl && pt && mech3::mech_world_position(pl, at))
        for (int32_t k = 0; k < made; ++k) {
          const mech3::ai::AiTarget* t = mech3::mech_ai_target(ai[k].mech);
          float q[3];
          if (!t || !(t->team & pt->enemies) || !mech3::mech_world_position(ai[k].mech, q)) continue;
          const float dd = (q[0] - at[0]) * (q[0] - at[0]) + (q[2] - at[2]) * (q[2] - at[2]);
          if (dd < d2) d2 = dd, best[0] = q[0], best[1] = q[1], best[2] = q[2], who = ai[k].name ? ai[k].name : "?";
        }
      if (d2 < 1e30f) {
        const float d = std::sqrt(d2);
        if (d > 80.0f) {
          const float x = at[0] + (best[0] - at[0]) / d * 0.25f, z = at[2] + (best[2] - at[2]) / d * 0.25f;
          const float to[3] = {x, pm_kill_ground(x, 300.0f, z), z};
          mech3::move_mech(pl, to, static_cast<float>(std::atan2(best[0] - at[0], -(best[2] - at[2]))));
        }
        if (frames % 500 == 0) std::printf("  walk: %s %.0f m off at %d\n", who, d, frames);
      }
    }
    if (mate && hit_frames[1] >= 0 && frames == hit_frames[1] + 1) {
      const mech3::ai::AiTarget* t = mech3::mech_ai_target(mate);
      std::printf("  c1m1: after hit 2: her team %x allies %x enemies %x\n", t->team, t->allies, t->enemies);
    }
    if (mate && frames == hit_frame + 1) {
      auto* mc = static_cast<mech3::AiControls*>(mate_controls);
      mech3::ai::Brain* br = mc->brain.get();
      std::printf("  c1m1: after the hit: reacts %d hit_lately %d busy %d may_fire %d react_gap %g now %g\n",
                  br ? br->params.get<int32_t>(0x9c) : -1, int(mc->hit_lately()), br ? int(br->xf0) : -1,
                  br ? int(mech3::ai::pm_may_fire(*br)) : -1, br ? br->params.get<float>(0xb0) : -1.0f,
                  mech3::ai::brain_hooks().now());
    }
    if (mate && hit_frame >= 0 && frames >= hit_frame) {
      const mech3::ai::AiTarget* t = mech3::mech_ai_target(mate);
      const zgame::Handle p = engine.handle_of(mech3::mission_player_mech());
      if (t->target && t->target->seen == p) {
        ++on_player, last_on = frames;
        if (first_on < 0) first_on = frames;
      }
    }
    if (frames == 13000 && !std::getenv("PM_IDLE"))  // PM_IDLE: the trace's c1m1-idle, nothing forced; the camp lost: "Protect Rim Collective Camp"'s fails (0x004600d0)
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
  if (hit_frame >= 0)
    std::printf("  c1m1: hit the lancemate at frame %d; it targets the player on %d frames, first %d, last %d\n", hit_frame,
                on_player, first_on, last_on);
  zgame::set_game(zgame::mw3_table());
  if (other || std::getenv("PM_IDLE")) return;  // a run to look at, not this test's checks
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
void pm_watch(void* mech, const char* name, const float player[3]);
int pm_watch_follow(void* mech);
void pm_kill_begin(void* player);
int pm_kill_fire(void* player, const float at[3]);
int pm_trigger_all(void* player, const float at[3]);
float pm_tag_muzzle(void* player, const float at[3], bool* mounted);
float pm_mech_toughness(void* mech, bool* invincible);
bool pm_cheat(void* player, const char* text);
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
int pm_kill_falling_buffer(void* mech);
void pm_kill_walk(void* mech, bool jets);
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
static int g_fall_sound_heard = 0, g_fall_quietest = 0;
static int g_cloud_frames = 0, g_cloud_wrong = 0, g_cloud_full = 0, g_cloud_ramp = 0, g_cloud_mode = -1;

// The canyon tests: what the output was told for each buffer, as the mixer takes it.
struct HeardSound : platform::SilentSoundOutput {
  std::map<platform::SoundBuffer, int32_t> volume;
  int32_t set_volume(platform::SoundBuffer b, int32_t v) override {
    volume[b] = v;
    return platform::SilentSoundOutput::set_volume(b, v);
  }
  bool playing(platform::SoundBuffer b) {
    uint32_t st = 0;
    return get_status(b, &st) == 0 && (st & kStatusPlaying);
  }
};
static HeardSound* g_heard = nullptr;

static bool me_alive(void* player) {
  mech3::LaunchedMech me;
  mech3::describe_mech(player, &me);
  return me.anim_state != 10;
}

struct LightSeen {
  bool found = false, active = false, in_world = false;
  uint32_t flags = 0;
  float near = 0, far = 0, ambient = 0, diffuse = 0, color[3] = {}, mixed[12] = {};
};
static LightSeen g_light;
static struct { int nodes, moved; } g_motions;  // PM_MOTIONS: event 12's nodes and how many moved

static int g_crater_spikes = -1, g_craters_made = 0;

static bool t_everyone_once() {
  static int n = 0;
  return ++n == 3;
}

static void kill_run(const char* which, bool quick) {
  EXPECT(zmth::matrix_stack().slot[0] == nullptr);  // a test's own local left there sends every box astray
  if (!which || !std::filesystem::exists(MW3PM_GAME_DIR)) return;
  if (std::getenv("PM_BUGS_OFF")) zfix::options().bugs = false;
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  // PM_PILOT_DIR: a write dir to play from (a copy of a player's, with PM_PILOT the pilot to launch as).
  const std::string wd = std::getenv("PM_PILOT_DIR") ? std::getenv("PM_PILOT_DIR") : std::string(MW3_TEST_TMP_ROOT) + "/pm_kill_write";
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
  HeardSound sound;
  g_heard = &sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
  p.sound = &sound, p.files = &files, p.input = &keys;
  mech3::GameEngine engine(p, "mech3.exe -nofmv");
  engine.quiet = std::getenv("PM_PILOT") == nullptr;
  mech3::Mech3App app(&engine);
  mech3::set_app(&app);
  EXPECT(app.init_instance());
  mech3::GameEngine::Launch l;
  std::sscanf(which, "%d,%d,%d", &l.index, &l.type, &l.number);
  if (const char* e = std::getenv("PM_MECH")) l.mech = std::atoi(e);
  if (const char* name = std::getenv("PM_PILOT")) {  // the pilot's current game's slot: its mech, loadout and skin
    mech3::current_pilot() = mech3::load_pilot(name, false);
    std::printf("  pilot %s: %s\n", name, mech3::current_pilot() ? "loaded" : "not found");
    if (mech3::Pilot* pp = mech3::current_pilot(); pp && std::getenv("PM_PILOT_GAME"))  // the saved game of that mission, as the pilot screen picks it
      for (mech3::Game* g : pp->campaigns)
        if (auto* cg = dynamic_cast<mech3::CampaignGame*>(g); cg && cg->mission_number() == std::atoi(std::getenv("PM_PILOT_GAME"))) pp->current = g;
    l.keep_pilot = mech3::current_pilot() != nullptr;
  }
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
    if (const char* tri = std::getenv("PM_TRIANGLE")) {  // pm-tests item 16: the player that far from this mech, it targeted
      static int t = 0;
      ++t;
      if (t == 2) {
        const float d = static_cast<float>(std::atof(tri));
        const float to[3] = {there[0] + d, pm_kill_ground(there[0] + d, 300.0f, there[2]), there[2]};
        mech3::move_mech(player, to, static_cast<float>(std::atan2(d, 0.0)));
      }
      if (t == 30) mech3::program_hud_target(m, 0);
      if (t >= 30 && t < 400) {
        int sets = 0;
        const float pulse = mech3::program_hud_target_pulse(&sets);
        float me[3];
        mech3::mech_world_position(player, me);
        const float dx = me[0] - there[0], dz = me[2] - there[2];
        if (t % 10 == 0 || t < 40) std::printf("  triangle t %d dist %.0f pulse %.3f sets %d\n", t, std::sqrt(dx * dx + dz * dz), pulse, sets);
      }
      if (t == 400) done = true;
      return;
    }
    if (const char* watch = std::getenv("PM_M2_WATCH")) {  // x,z: the player there; PM_M2_ACROSS=x,z,t later; each AI mech logged
      static int t = 0;
      float x = 0, z = 0;
      std::sscanf(watch, "%f,%f", &x, &z);
      ++t;
      if (t == 2) {
        const float to[3] = {x, pm_kill_ground(x, 300.0f, z), z};
        mech3::move_mech(player, to, 0.0f);
      }
      if (const char* across = std::getenv("PM_M2_ACROSS")) {
        float ax = 0, az = 0;
        int when = 0;
        std::sscanf(across, "%f,%f,%d", &ax, &az, &when);
        if (t == when) {
          const float to[3] = {ax, pm_kill_ground(ax, 300.0f, az), az};
          mech3::move_mech(player, to, 0.0f);
        }
      }
      int32_t count = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&count);
      if (t == 30) std::printf("  vias on the area: %d\n", mech3::ai::inout_via_count());
      if (std::getenv("PM_M2_FOLLOW") && t == 30)
        for (int32_t i = 0; i < count; ++i)
          if (ai[i].name && std::strcmp(ai[i].name, std::getenv("PM_M2_FOLLOW")) == 0)
            std::printf("  follow %s: %d\n", ai[i].name, pm_watch_follow(ai[i].mech));
      if (t % 300 == 1) {
        float me[3];
        mech3::mech_world_position(player, me);
        std::printf("  t %d frames, player %.0f %.0f %.0f\n", t, me[0], me[1], me[2]);
        for (int32_t i = 0; i < count; ++i) pm_watch(ai[i].mech, ai[i].name ? ai[i].name : "?", me);
      }
      if (const char* e = std::getenv("PM_M2_END"); t > (e ? std::atoi(e) : 3000)) done = true;
      return;
    }
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
    if (std::getenv("PM_EVERYONE") && t_everyone_once()) {  // every object the sensors know: kind, name, team, allies, alive
      for (const mech3::ChainSide& x : mech3::chain_everyone())
      {
        float at[3] = {};
        mech3::hud_object_place(mech3::chain_object(x.t.h), at);
        std::printf("  everyone %s %s team %x allies %x %s at %.0f %.0f %.0f\n", x.t.kind, x.t.name.c_str(), x.team, x.allies,
                    x.alive ? "alive" : "dead", at[0], at[1], at[2]);
      }
    }
    // PM_HOSTILES=metres: each hostile but the mechs in turn (vehicles, turrets, targets.zrd's), the player that far
    // from it, every weapon through its trigger at its node until it dies or 300 frames pass (pm-tests item 9).
    if (std::getenv("PM_TAG_MUZZLE")) {  // pm-tests item 15: TAG added as the build does, aimed ahead
      static int t = 0;
      if (++t == 30) {
        bool mounted = false;
        float at[3];
        zcls::node_world_point(static_cast<zcls::Node*>(mech3::hud_object_node(player)), at);
        at[0] += 100.0f;
        const float d = pm_tag_muzzle(player, at, &mounted);
        std::printf("  TAG: mounted %d, its muzzle %.1f m from the mech\n", mounted, d);
        EXPECT(mounted);
        EXPECT(d >= 0.0f && d < 15.0f);
        done = true;
      }
      return;
    }
    // pm-tests item 17: an ALLY turret (turf_lrm01) with an AI mech 120 m from it and the player 60 m; at frame 120
    // the mech goes 900 m away. The turret's target may be the mech, never anything of the player's side.
    if (std::getenv("PM_ALLY_TURRET")) {
      static int t = 0, bad = 0, engaged = 0;
      static mech3::tur::Turret* tu = nullptr;
      static float tp[3] = {};
      ++t;
      if (t == 2) {
        for (const mech3::ChainSide& x : mech3::chain_everyone())
          if (x.t.name == "turf_lrm01") tu = mech3::program_turret_by_handle(x.t.h), mech3::hud_object_place(mech3::chain_object(x.t.h), tp);
        EXPECT(tu != nullptr);
        if (!tu) return void(done = true);
        const float pa[3] = {tp[0] + 60.0f, pm_kill_ground(tp[0] + 60.0f, 300.0f, tp[2]), tp[2]};
        mech3::move_mech(player, pa, 0.0f);
        const float ma[3] = {tp[0] - 120.0f, pm_kill_ground(tp[0] - 120.0f, 300.0f, tp[2]), tp[2]};
        mech3::move_mech(m, ma, 0.0f);
      }
      if (t == 120) {
        const float far[3] = {tp[0] - 900.0f, pm_kill_ground(tp[0] - 900.0f, 300.0f, tp[2]), tp[2]};
        mech3::move_mech(m, far, 0.0f);
      }
      if (t > 2 && tu && tu->target && tu->target->seen) {
        const mech3::ai::AiTarget* a = mech3::ai::sensor_hooks().fields(tu->target->seen);
        if (a && (a->team & tu->enemies)) ++engaged;
        else ++bad;
      }
      if (t == 300) {
        std::printf("  ally turret: %d frames on an enemy, %d on its own side\n", engaged, bad);
        EXPECT(engaged > 0);
        EXPECT(bad == 0);
        done = true;
      }
      return;
    }
    if (const char* range = std::getenv("PM_HOSTILES")) {
      static int t = 0, from = 0, shots_at = 0, killed = 0, alive = 0;
      static std::vector<mech3::ChainTarget> all;
      static size_t i = 0;
      static bool placed = false;
      if (++t == 2) {
        for (const mech3::ChainTarget& c : mech3::chain_hostiles())
          if (std::strcmp(c.kind, "mech") != 0) all.push_back(c);
        std::printf("  %zu hostiles but mechs\n", all.size());
        return;
      }
      if (t < 2) return;
      if (i >= all.size() || t > 40000) {
        std::printf("  hostiles: %zu, killed %d, alive after 300 frames %d\n", all.size(), killed, alive);
        EXPECT(alive == 0);
        done = true;
        return;
      }
      const mech3::ChainTarget& c = all[i];
      auto* n = static_cast<zcls::Node*>(mech3::hud_object_node(mech3::chain_object(c.h)));
      if (!placed) {
        if (!mech3::chain_alive(c.h)) {
          std::printf("  %s %s gone before its turn\n", c.kind, c.name.c_str());
          ++i;
          return;
        }
        mech3::chain_player_near(n, static_cast<float>(std::atof(range)));
        placed = true, from = t, shots_at = 0;
        return;
      }
      if (!mech3::chain_alive(c.h)) {
        std::printf("  %s %s dead after %d frames, %d shots\n", c.kind, c.name.c_str(), t - from, shots_at);
        ++killed, ++i, placed = false;
        return;
      }
      float at[3] = {};
      if (n) zcls::node_world_position(n, at);
      at[1] += std::getenv("PM_AIM_UP") ? static_cast<float>(std::atof(std::getenv("PM_AIM_UP"))) : 2.0f;
      if (t - from > 5) shots_at += pm_trigger_all(player, at);
      if (t - from > 300) {
        std::printf("  %s %s at %.0f %.0f %.0f alive after %d shots\n", c.kind, c.name.c_str(), at[0], at[1], at[2], shots_at);
        ++alive, ++i, placed = false;
      }
      return;
    }
    if (std::getenv("PM_BASE_LIGHT")) {  // c1m2's starbase_light (turnon_baselight's LIGHT_STATE, PM's event 4) at 200 frames
      static int t = 0;
      if (++t < 200) return;
      auto& a = zeff::anim();
      for (int i = 0; i < a.def_count(); ++i) {
        zeff::AnimDef* d = &a.defs[i];
        if (std::strcmp(d->name(), "turnon_baselight") != 0 || d->count(0x10b) < 2) continue;
        auto* n = static_cast<zcls::Node*>(d->lights[1].made);
        auto* l = static_cast<zcls::Light*>(n->data);
        g_light.found = true, g_light.active = (n->flags() & 4) != 0, g_light.in_world = d->lights[1].get<int32_t>(0x28) != 0;
        g_light.flags = l->get<uint32_t>(0xb0), g_light.near = l->get<float>(0xb4), g_light.far = l->get<float>(0xb8);
        g_light.ambient = l->get<float>(0xa0), g_light.diffuse = l->get<float>(0x9c);
        for (int k = 0; k < 3; ++k) g_light.color[k] = l->get<float>(0xa4 + 4 * k);
        for (int k = 0; k < 12; ++k) g_light.mixed[k] = l->pm_colors[k];
      }
      done = true;
      return;
    }
    if (std::getenv("PM_MOTIONS")) {  // the nodes event 12 (an SI script's motion) moves, by 300 frames (pm-tests, the event sweep)
      static std::map<zcls::Node*, std::array<float, 6>> start;
      static int t = 0;
      const auto pose = [](zcls::Node* n) {
        std::array<float, 6> p{};
        zcls::object3d_get_translation(n, &p[0], &p[1], &p[2]);
        zcls::object3d_get_rotation(n, &p[3], &p[4], &p[5]);
        return p;
      };
      if (t++ == 0) {
        auto& a = zeff::anim();
        for (int i = 0; i < a.def_count(); ++i) {
          zeff::AnimDef* d = &a.defs[i];
          for (int k = 0; k < d->count(0x108); ++k) {
            const uint8_t* e = d->seqs[k].events;
            const uint32_t size = e ? d->seqs[k].get<uint32_t>(0x3c) : 0;
            for (uint32_t at = 0; at + 12 <= size;) {
              uint32_t n;
              int32_t ni;
              std::memcpy(&n, e + at + 4, 4), std::memcpy(&ni, e + at + 0xc, 4);
              if (e[at] == 12 && ni >= 0 && d->nodes[ni].node && d->nodes[ni].node->cls() == 5)
                start.emplace(d->nodes[ni].node, pose(d->nodes[ni].node));
              if (n < 12) break;
              at += n;
            }
          }
        }
      }
      if (t < 300) return;
      int moved = 0;
      for (const auto& [n, p] : start)
        if (pose(n) != p) std::printf("%s%s", moved++ ? " " : "  moved:", n->name());
      if (moved) std::printf("\n");
      std::printf("  event 12: %zu nodes, %d moved\n", start.size(), moved);
      g_motions = {static_cast<int>(start.size()), moved};
      done = true;
      return;
    }
    if (std::getenv("PM_EVENTS")) {  // the world's anim events by type and record size (pm-tests, the event sweep)
      std::map<std::pair<int, uint32_t>, int> seen;
      auto& a = zeff::anim();
      const auto walk = [&](const uint8_t* e, uint32_t size) {
        for (uint32_t at = 0; at + 12 <= size;) {
          uint32_t n;
          std::memcpy(&n, e + at + 4, 4);
          ++seen[{e[at], n}];
          if (n < 12) break;
          at += n;
        }
      };
      for (int i = 0; i < a.def_count(); ++i) {
        zeff::AnimDef* d = &a.defs[i];
        for (int k = 0; k < d->count(0x108); ++k)
          if (d->seqs[k].events) walk(d->seqs[k].events, d->seqs[k].get<uint32_t>(0x3c));
        if (d->reset_events) walk(d->reset_events, d->get<uint32_t>(0x104));
      }
      for (const auto& [k, n] : seen) std::printf("  event %d size %#x: %d\n", k.first, k.second, n);
      done = true;
      return;
    }
    if (std::getenv("PM_CHEAT")) {  // pm-tests item 12: Invincible on, the player 150 m from a hostile mech, his armour watched
      static int t = 0;
      static float start = 0;
      ++t;
      if (t == 10) std::printf("  cheat taken %d\n", pm_cheat(player, "power overwhelming") ? 1 : 0);
      if (t == 12)
        for (const mech3::ChainTarget& c : mech3::chain_hostiles())
          if (std::strcmp(c.kind, "mech") == 0) {
            mech3::chain_player_near(mech3::hud_object_node(mech3::chain_object(c.h)), 150.0f);
            std::printf("  near %s\n", c.name.c_str());
            break;
          }
      if (t >= 12 && (t - 12) % 300 == 0) {
        bool inv = false;
        const float now = pm_mech_toughness(player, &inv);
        if (t == 12) start = now;
        std::printf("  t %d: invincible %d, armour and internal %.2f of %.2f\n", t, inv ? 1 : 0, now, start);
      }
      if (t > 3600) done = true;
      return;
    }
    // PM_CRATERS=step[,x0,z0,x1,z1]: the player taken over a grid of the ground, an LRM's crater (radius 3, depth 0.75)
    // dug 10 m ahead the frame after; every ground model with a vertex within 60 m of it checked before and after: its
    // heights there must stay within 10 m of what they were, and finite (pm-tests item 11).
    if (const char* cr = std::getenv("PM_CRATERS")) {
      static std::vector<std::array<float, 3>> points;
      static size_t at = 0;
      static int made = 0, failed = 0, spikes = 0, phase = 0;
      static std::vector<std::pair<zcls::Node*, std::vector<float>>> before;
      if (points.empty()) {
        float step = 60, x0 = 0, z0 = -4000, x1 = 3000, z1 = 0;
        std::sscanf(cr, "%f,%f,%f,%f,%f", &step, &x0, &z0, &x1, &z1);
        for (float x = x0; x <= x1; x += step)
          for (float z = z0; z <= z1; z += step) {
            const float y = pm_kill_ground(x, 400.0f, z);
            if (y > -500.0f) points.push_back({x, y, z});
          }
        std::printf("  %zu crater points\n", points.size());
      }
      if (at >= points.size()) {
        std::printf("  craters: %d made, %d failed, %d spikes\n", made, failed, spikes);
        g_crater_spikes = spikes, g_craters_made = made;
        done = true;
        return;
      }
      const auto& q = points[at];
      const auto near = [&](zcls::Node* n) {
        const pm::zmod::Model* m = n->pm_model;
        std::vector<float> ys;
        bool any = false;
        for (int i = 0; m && m->vertices && i < m->count(0x14); ++i) {
          const float* v = m->vertices + 3 * i;
          ys.push_back(v[1]);
          if (std::fabs(v[0] - q[0]) < 60.0f && std::fabs(v[2] - q[2]) < 60.0f) any = true;
        }
        return any ? ys : std::vector<float>{};
      };
      if (phase == 0) {  // the player there
        const float to[3] = {q[0], q[1] + 1.0f, q[2] + 10.0f};
        mech3::move_mech(player, to, 0.0f);
        phase = 1;
        return;
      }
      before.clear();
      for (zcls::ListItem* li = zcls::list_head(6); li; li = li->next)
        if (zcls::Node* n = static_cast<zcls::Node*>(li->node); n && n->pm_model) { auto ys = near(n); if (!ys.empty()) before.push_back({n, ys}); }
      zwep::CraterOrder order;
      order.x00 = 0x1008, order.points = 10, order.depth = 0.75f, order.radius = 3.0f;
      order.at = {q[0], q[1], q[2]};
      const int r = zdec::crater_instance(&order, 0);
      r < 0 ? ++failed : ++made;
      for (auto& [n, ys] : before) {
        const pm::zmod::Model* m = n->pm_model;
        if (!m || !m->vertices) continue;
        float worst = 0, wv[3] = {};
        bool finite = true;
        float lo = 1e30f, hi = -1e30f;
        for (float y : ys) lo = std::min(lo, y), hi = std::max(hi, y);
        for (int i = 0; i < m->count(0x14); ++i) {
          const float* v = m->vertices + 3 * i;
          if (!std::isfinite(v[0]) || !std::isfinite(v[1]) || !std::isfinite(v[2])) finite = false;
          const float off = std::max(v[1] - hi, lo - v[1]);
          if (off > worst) worst = off, wv[0] = v[0], wv[1] = v[1], wv[2] = v[2];
        }
        if (worst > 10.0f) std::printf("    worst vertex %.1f %.1f %.1f (heights before %.1f..%.1f), vertices %zu to %d, polygons %d\n", wv[0], wv[1], wv[2], lo, hi, ys.size() / 1, m->count(0x14), m->count(0x10));
        if (!finite || worst > 10.0f) {
          ++spikes;
          std::printf("  spike: crater at %.1f %.1f %.1f (result %d): %s off its heights by %.1f m%s\n", q[0], q[1], q[2], r, n->name(), worst,
                      finite ? "" : ", not finite");
        }
      }
      ++at, phase = 0;
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
      const char* walk = std::getenv("PM_FALL_WALK");  // the yaw: on foot over the edge at full throttle
      ++t;
      if (t == 2) {
        const float yaw = walk ? std::strtof(walk, nullptr) : 0.0f;
        const float at[3] = {x, walk ? pm_kill_ground(x, 300.0f, z) : 30.0f, z};
        mech3::move_mech(player, at, yaw);
      }
      if (walk && me_alive(player)) pm_kill_walk(player, std::getenv("PM_FALL_JETS") != nullptr);
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
        const int buffer = pm_kill_falling_buffer(player);
        if (buffer >= 0) {
          if (g_fall_sound_from == 0) g_fall_sound_from = g_fall_frames_dead;
          ++g_fall_sound_frames;
          // At the mixer: playing, and at what it was told.
          if (g_heard->playing(buffer)) ++g_fall_sound_heard, g_fall_quietest = std::min(g_fall_quietest, g_heard->volume[buffer]);
        }
      }
      if (me.anim_state == 10 && g_fall_died_at > 0.0f) g_fall_died_at = there_y[1], g_fall_failed = engine.mission_failed();
      if (targets.size() > 1 && state(targets[1]) == 10) g_fall_ai_dead = true;
      g_fall_failed = g_fall_failed || engine.mission_failed();
      if (t > (walk ? 900 : 300)) done = true;
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
  if (quick && !std::getenv("PM_FALL") && !std::getenv("PM_BASE_LIGHT") && !std::getenv("PM_CRATERS") && !std::getenv("PM_MOTIONS") && !std::getenv("PM_HOSTILES") && !std::getenv("PM_TAG_MUZZLE") && !std::getenv("PM_ALLY_TURRET")) EXPECT(dead == 1 || g_refit_done);
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

// Always on: c2's craters raise no spikes (pm-tests item 11). PM's ground has strips (0x2000); the crater's clip makes
// them triangles first (0x005726f0). Read whole as one polygon, a strip's zigzag clipped into vertices tens of metres
// off the ground (5 here before) and holes to the sky. An LRM's crater every 60 m over part of the map, each touched
// ground model's heights checked.
// pm-tests item 14: c2m3's cargo tanker (tcab03, a targets.zrd object) dies to lasers alone. A target isn't put in
// the HUD's box test (its vt+0x48 is none); when it was, the beam stopped on its box, a node with no handler.
TEST(pm_c2m3_lasers_kill_the_tanker) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child == 0) {
    setenv("PM_HOSTILES", "60", 1);
    setenv("PM_BEAMS_ONLY", "1", 1);
    kill_run("1,1,3", true);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  waitpid(child, &status, 0);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// pm-tests item 15: TAG, added by its node (the mech's equipment 13), finds its part and firepoint, so its beam
// starts at the mech; without the weapon calls' node lookups it fired from the world's origin, out of sight.
TEST(pm_tag_fires_from_the_mech) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child == 0) {
    setenv("PM_TAG_MUZZLE", "1", 1);
    kill_run("1,1,3", true);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  waitpid(child, &status, 0);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// pm-tests item 17: Pirate's Moon's ALLY turrets (turf_) never take the player's side as a target. MW3's pick ended
// on the player's own contact (0x004a7b40), which an ALLY turret kept once its enemy left, and fired on the player.
TEST(pm_c2m3_ally_turret_keeps_off_the_player) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child == 0) {
    setenv("PM_ALLY_TURRET", "1", 1);
    kill_run("1,1,3", true);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  waitpid(child, &status, 0);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

TEST(pm_c2m3_craters_raise_no_spikes) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child == 0) {
    setenv("PM_CRATERS", "60,600,-2500,1000,-300", 1);
    kill_run("1,1,3", true);
    EXPECT(g_crater_spikes == 0);
    EXPECT(g_craters_made > 20);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  waitpid(child, &status, 0);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// Always on: c1m2's base lights (pm-tests item 6). start_hwlights calls turnon_baselight, whose loop's first range test
// (2 s in) lights starbase_light by PM's LIGHT_STATE (0x0051b910): the file's range 0..150, ambient 0.45, diffuse 1,
// white, static and not a light map, and PM's mixed colours from them. MW3's handler on PM's record set only the switch.
TEST(pm_c1m2_base_light_takes_its_state) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child == 0) {
    setenv("PM_BASE_LIGHT", "1", 1);
    kill_run("0,1,2", true);
    const LightSeen& l = g_light;
    std::printf("  starbase_light: found %d active %d in the world %d flags %x range %.1f..%.1f ambient %.3f diffuse %.3f colour %.2f %.2f %.2f; mixed",
                l.found, l.active, l.in_world, l.flags, l.near, l.far, l.ambient, l.diffuse, l.color[0], l.color[1], l.color[2]);
    for (float m : l.mixed) std::printf(" %.3f", m);
    std::printf("\n");
    EXPECT(l.found && l.active && l.in_world);
    EXPECT(l.near == 0.0f && l.far == 150.0f);
    EXPECT(l.ambient == 0.45f && l.diffuse == 1.0f);
    EXPECT(l.color[0] == 1.0f && l.color[1] == 1.0f && l.color[2] == 1.0f);
    EXPECT(l.mixed[3] == 1.0f && l.mixed[6] == 0.45f && l.mixed[9] == 1.45f);  // diffuse mixed, ambient mixed, their sum
    EXPECT((l.flags & 0x80) == 0);  // LIGHTMAP FALSE
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  waitpid(child, &status, 0);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// Always on: c1m2's motions from SI scripts (PM's event 12, 0x0051e2c0; pm-tests, the event sweep): by 300 frames the
// base crew of the_dude_runa06 and a07 run (torso and legs, 10 nodes). Routed to MW3's handler, PM's records moved none.
TEST(pm_c1m2_motions_play) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child == 0) {
    setenv("PM_MOTIONS", "1", 1);
    kill_run("0,1,2", true);
    EXPECT(g_motions.nodes > 100 && g_motions.moved >= 10);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  waitpid(child, &status, 0);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// Always on: c1m1's Thor, one ER Large, every shot on its leg: it goes down with the leg, the player's hits on legs only.
// Always on: mission 2's canyon, cloud_cover's death band: the player over the edge dies at y = -30, in the air,
// mission failed; an AI mech dies there too (docs/vehicles.md, The cloud's death band). Dropped in, walked in in
// the Centurion, jetted in in the Thor: the god's eye over the body and mech_falling at the mixer.
TEST(pm_canyon_death_band) {
#if !defined(_WIN32)
  struct Way { const char* name; const char* at; const char* walk; const char* mech; bool jets; };
  const Way ways[] = {{"dropped", "3056,-4776", nullptr, nullptr, false},
                      {"walked, Centurion", "2985,-4727", "-0.98", "7", false},
                      {"jetted, Thor", "2985,-4727", "-0.98", "23", true}};
  for (const Way& w : ways) {
    std::fflush(nullptr);
    const pid_t child = fork();
    if (child == 0) {
      setenv("PM_FALL", w.at, 1);
      if (w.walk) setenv("PM_FALL_WALK", w.walk, 1);
      if (w.mech) setenv("PM_MECH", w.mech, 1);
      if (w.jets) setenv("PM_FALL_JETS", "1", 1);
      kill_run("0,1,2", true);
      std::printf("  %s: died at y %.2f, lowest %.2f, mission failed %d, the AI mech dead %d\n", w.name, g_fall_died_at, g_fall_lowest, g_fall_failed, g_fall_ai_dead);
      std::printf("  the god's eye: mode %d, %.1f m over the body; mech_falling from dead frame %d for %d frames, %d of them at the mixer, quietest %d (0.01 dB)\n", g_fall_mode, g_fall_camera_up, g_fall_sound_from, g_fall_sound_frames, g_fall_sound_heard, g_fall_quietest);
      EXPECT(g_fall_died_at < -30.0f && g_fall_died_at > -36.0f);
      EXPECT(g_fall_failed);
      EXPECT(g_fall_mode == 3);                        // 0x00408a70(3)
      EXPECT(g_fall_camera_up > 100.0f);
      EXPECT(g_fall_sound_from >= 1 && g_fall_sound_from <= 2);  // the fall action's start, the frame after
      EXPECT(g_fall_sound_heard > 30);
      EXPECT(g_fall_quietest > -1500);
      std::fflush(nullptr);
      _exit(check::failures ? 1 : 0);
    }
    int status = 1;
    waitpid(child, &status, 0);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }
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
