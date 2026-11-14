// Pirate's Moon missions run headless with the AI on, for what the AI does in play.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <typeinfo>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <functional>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#else
#include <process.h>  // _getpid, as tests/main.cpp
#define getpid _getpid
#endif

#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zinput/zinput.h"
#include "check.h"
#include "gamez/zeffect/zeff_anim_events.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_save.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zclass/light.h"
#include "gamez/zweapon/zwep_init.h"
#include "gamez/zweapon/zweapon.h"
#include "mech3/zainet_inout.h"
#include "mech3/zainet_inout_view.h"
#include "mech3/zai_repair.h"
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
#include "mech3/action_menu.h"
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
#include "gamez/zimage/zimg_texture.h"
#include "pm/gamez/zclass/cls_clutter.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/app.h"
#include "mech3/game_editor.h"
#include "mech3/game_editor_net.h"
#include "mech3/net_save.h"
#include "mech3/teleport.h"
#include "upgrades/editor/form.h"
#include "upgrades/editor/teleport.h"
#include "mech3/mission_world.h"
#include "gamez/zclass/cls_data.h"
#include "mech3/briefing.h"
#include "mech3/debriefing.h"
#include "mech3/mainmenu.h"
#include "mech3/pilot_menu.h"
#include "gamez/zinterp/zinterp.h"
#include "mech3/mission_map.h"
#include "mech3/zai_anim_owner.h"
#include "gamez/zentity/zent_manager.h"
#include "gamez/zmodel/gmod_puff.h"
#include "gamez/zmodel/gmod_draw.h"
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
#include "platform/render3d.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zutil/zutl_msgdll.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zmodel/gmod_cull.h"

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
  const pid_t child = check::fork_child();
  if (child == 0) {
    shadow_cats_run();
    // _exit skips the atexit report: the seeded pair compares these lines, so the child prints them itself.
    for (const auto& [key, n] : mech3::ai::pm_hooks_missing()) std::printf("mw3: pm ai: %8d %s\n", n, key.c_str());
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
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
bool pm_headlight(void* player, bool on, float at[3]);
float pm_knock(void* mech, float* threshold, int* state);
void pm_command(void* player, int32_t cmd);
void pm_kill_left(void* mech, char* out, size_t n);
void pm_kill_ray(const float from[3], const float to[3]);
uint32_t pm_kill_root_flags(void* mech);
void pm_kill_tree(void* mech);
int pm_leg_fire(void* player, void* mech, int weapon, int leg, int shown, bool ready);
void pm_leg_reset();
const char* pm_weapon_name(void* player, int i);
bool pm_kill_only_leg_lost(void* mech, int leg);
int pm_missile_step(void* player, void* mech, int weapon, bool lock, bool artemis);
void pm_hold_down(void* mech);
int pm_shot_flight(void* player, int since, float* travelled, float* expected, int* seen, char* name, size_t n);
static float g_shot_travelled = 0, g_shot_expected = 0;
static int g_shot_seen = 0, g_shot_result = 0;
int pm_controls_view(void* mech);
void pm_controls_set_view(void* mech, int view);
bool pm_kill_turret(const char* name);
void pm_turret_roots(std::vector<zcls::Node*>* out);
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
float pm_mech_height(void* mech);
void pm_water_survey(void* player, const char* label, float low, float high, float step);
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
// pm-tests 35's run (PM_REPAIR_AFTER): her order's answer, her legs then and at the end, down on the point, done.
static int g_mate_order = -1;
static float g_mate_legs0 = -1, g_mate_legs1 = -1;
static bool g_mate_docked = false, g_mate_done = false;
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
static struct { bool found[2] = {}, restored[2] = {}; const char* name[2] = {}; } g_animsave;  // PM_ANIMSAVE
static struct { bool made = false; int counted = -1, listed = -1, damaged = -1, tried = 0; } g_splash;  // PM_SPLASH
static struct { bool built = false, freed = false; int models = 0, polygons = 0, refs = 0, back = 0; } g_inout;  // PM_INOUT
static struct {  // PM_TELEPORT
  std::string listed, place, saved, text;
  float want[3] = {}, at[3] = {};
  bool moved = false, cancel_stayed = false;
  std::vector<mech3::TeleportSpot> nav_names;  // the spots not kept: the nav points
} g_tp;
// PM_ANHUR (pm-tests 37): anhur04 shot down near the bridge; its root boxed while alive, what a shot and a walk meet after.
static struct {
  bool killed = false, boxed_alive = false, boxed_dead = true;
  zcls::Node* shot = nullptr; zcls::Node* walk = nullptr;
  float shot_at = 0, walk_at = 0;
  int deck_gone = -1;  // the frame hslid01 went inactive
  int kill_frame = -1;  // the frame anhur04 was shot down
  bool away = false;    // its loop has taken it past 400 m from the bridge (it starts near it)
  int solid = 0, unseen = 0;
  bool bridge_boxed_alive = false, bridge_boxed_dead = true;  // xslid01, a target (targets.zrd), as the deck goes  // anhur04's live collidable pieces after the fall; those with nothing drawable
} g_anhur;
static struct { int nodes, moved; } g_motions;  // PM_MOTIONS: event 12's nodes and how many moved

static int g_crater_spikes = -1, g_craters_made = 0;

static bool t_everyone_once() {
  static int n = 0;
  return ++n == 3;
}

// PM_MOTION_SURVEY: OBJECT_MOTION's ground under each falling node, with MW3's exclusion (its anim's node) and
// PM's (the node moved, 0x0051cd60), as 0x0051d960 picks it: where the two differ.
static int g_msurvey_calls = 0, g_msurvey_differ = 0, g_msurvey_self = 0, g_msurvey_turret_differ = 0;
static void pm_motion_survey(zcls::Node* anim, zcls::Node* moved, zcls::Node* out, const float pos[3], float reach) {
  auto* world = static_cast<zcls::Node*>(mech3::world_hooks().world);
  if (!world || !anim || !moved) return;
  const auto nearest = [&](zcls::Node* out, std::string* who) {
    int32_t saved;
    zcls::node_get_flag8(out, &saved);
    zcls::node_set_flag8(out, 0);
    static zcls::GroundHits g;
    float best_y = -1e30f, best_d = 0;
    int best = -1;
    if (zcls::ground_point(world, pos[0], reach + pos[1], pos[2], &g) == 0)
      for (int32_t i = 0; i < g.count; ++i) {
        const float d = std::fabs(pos[1] - g.hit[i].height);
        if (best < 0 || (d < best_d && g.hit[i].height - pos[1] < 10.0f)) best = i, best_d = d, best_y = g.hit[i].height;
      }
    *who = best >= 0 && g.hit[best].node ? g.hit[best].node->name() : "-";
    zcls::node_set_flag8(out, saved);
    return best_y;
  };
  std::string a, b;
  const float mw3 = nearest(anim, &a), pm = nearest(moved, &b);
  ++g_msurvey_calls;
  g_msurvey_self += out == moved && out != anim;
  if (mw3 != pm && std::strncmp(anim->name(), "tur", 3) == 0 && a == "-" && b == "g3") ++g_msurvey_turret_differ;
  if (mw3 != pm && ++g_msurvey_differ <= 40)
    std::printf("  motion survey: anim %s moves %s at %.0f %.0f %.0f: MW3's ground %.1f (%s), PM's %.1f (%s)\n", anim->name(), moved->name(),
                pos[0], pos[1], pos[2], mw3, a.c_str(), pm, b.c_str());
}

// zClass's frame counter is raised by the flip, every flip (0x00576ea0's `inc` at 0x00576eb9), menus too; not by the
// scene walk (docs/game-loop.md).
TEST(pm_each_flip_counts_a_frame) {
  NoKeys keys;
  Window window;
  Alert alert;
  Clock clock;
  MovieClock movies;
  Screen screen;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
  p.files = &files, p.input = &keys;
  mech3::GameEngine engine(p, "mech3.exe -nofmv");
  engine.quiet = true;
  const uint32_t before = zcls::frames().frame;
  engine.video_flip(0, 0, 0, 0);
  engine.video_flip(0, 0, 1, 1);
  EXPECT(zcls::frames().frame == before + 2);
  zcls::frames().frame = before;
}

static void kill_run(const char* which, bool quick) {
  EXPECT(zmth::matrix_stack().slot[0] == nullptr);  // a test's own local left there sends every box astray
  if (!which || !std::filesystem::exists(MW3PM_GAME_DIR)) return;
  if (std::getenv("PM_BUGS_OFF")) zfix::set_all(false);
  zgame::set_game(pm::table());
  if (std::getenv("PM_MOTION_SURVEY")) zeff::motion_survey() = &pm_motion_survey;
  if (std::getenv("PM_MOTION_MW3")) {  // the test's MW3 way: the anim's node out of OBJECT_MOTION's ground query
    static zgame::Table t = pm::table();
    t.anim_motion_ground_self = false;
    zgame::set_game(t);
  }
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
  if (const char* e = std::getenv("PM_OBJECT_LOD")) mech3::options().object_lod[0] = mech3::options().object_lod[1] = std::atoi(e);
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
    if (std::getenv("PM_KNOCK") && at < targets.size()) {  // pm-tests item 23: the target's knock and falls under fire
      static float most = 0;
      static int last = -1;
      float thr = 0;
      int st = 0;
      const float k = pm_knock(targets[at], &thr, &st);
      if (k > most) most = k, std::printf("  knock %s %.2f of %.2f (state %d)\n", names[at] ? names[at] : "?", k, thr, st);
      if (st != last && st >= 11 && st <= 13) std::printf("  FELL %s state %d at frame %d\n", names[at] ? names[at] : "?", st, frames);
      last = st;
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
    // PM_ANHUR runs its own clock: a mech's death (MW3's AI kills one before the Anhur passes) doesn't end it.
    if (at >= targets.size() || (quick && at > 0 && !std::getenv("PM_ANHUR"))) {
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
    if (const char* rp = std::getenv("PM_REPAIR")) {  // x,z: pm-tests 35, a lancemate ordered to repair near there
      static int t = 0;
      static void* mate = nullptr;
      static mech3::ai::Brain* brain = nullptr;
      float x = 0, z = 0;
      std::sscanf(rp, "%f,%f", &x, &z);
      ++t;
      int32_t count = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&count);
      if (t == 2) {
        const zgame::Handle pv = engine.handle_of(player);
        for (int32_t i = 0; i < count; ++i) {
          const mech3::ai::AiTarget* tg = ai[i].mech ? mech3::ai::brain_hooks().targetable(engine.handle_of(ai[i].mech)) : nullptr;
          const bool ally = tg && tg->is_ally(pv);
          std::printf("  ai %d %s ally %d\n", i, ai[i].name ? ai[i].name : "?", ally ? 1 : 0);
          if (ally && !mate && ai[i].controls) mate = ai[i].mech, brain = static_cast<mech3::AiControls*>(ai[i].controls)->brain.get();
        }
        const float at[3] = {x, pm_kill_ground(x, 300.0f, z), z};
        mech3::move_mech(player, at, 0.0f);
        if (mate) {
          const float m[3] = {x + 20, pm_kill_ground(x + 20, 300.0f, z), z};
          mech3::move_mech(mate, m, 0.0f);
          pm_kill_hurt(mate);
        }
      }
      if (t == 3)
        for (int32_t i = 1; i <= mech3::program_mcb_points(); ++i) {
          float q[3];
          if (mech3::program_mcb_point(i, q)) std::printf("  mcb point %d at %.0f %.0f %.0f\n", i, q[0], q[1], q[2]);
        }
      if ((t == 30 || t == 31) && std::getenv("PM_REPAIR_CALL")) mech3::program_mcb_command(player);
      if (std::getenv("PM_REPAIR_AFTER") && brain) {  // the player repaired first, then her order (Owen's)
        static int contact = -1, closed = -1, idle = -1, ordered = -1, last_front = -9;
        static float point[3];
        const mech3::DockSummary d = mech3::program_dock_summary();
        if (d.front != last_front) std::printf("  t %d: front %d drive %d busy %d requester %s\n", t, d.front, d.drive ? 1 : 0,
                                               d.busy ? 1 : 0, d.requester == nullptr ? "none" : d.requester == mate ? "her" : d.requester == player ? "player" : "other");
        last_front = d.front;
        if (t == 5 || t == 6) mech3::program_mcb_command(player);
        if (t == 6) {
          const char* l = nullptr;
          mech3::program_dock_point(&l, point);
        }
        if (t == 7 || (d.front == 2 && contact < 0 && ordered < 0)) {
          const float at[3] = {point[0], pm_kill_ground(point[0], point[1] + 100.0f, point[2]), point[2]};
          mech3::move_mech(player, at, 0.0f);
          if (d.front == 2) contact = t, mech3::mech_power(player, 1);
        }
        static bool starbase = false;
        if (d.front == -1 && t == 12) starbase = true, mech3::mech_power(player, 1);  // PM's static MFB: on the point, down
        if (starbase && d.front != -1 && closed < 0) closed = t;
        if (d.front == 5 && closed < 0) closed = t;
        if ((d.front == 0 || d.front == 7) && closed >= 0 && idle < 0) idle = t, mech3::mech_power(player, 0);
        if (idle >= 0 && t == idle + 120) {  // walked off the dock, then the order
          const float off[3] = {point[0] + 60, pm_kill_ground(point[0] + 60, point[1] + 100.0f, point[2]), point[2]};
          mech3::move_mech(player, off, 0.0f);
          const float m[3] = {point[0] + 40, pm_kill_ground(point[0] + 40, point[1] + 100.0f, point[2] + 30), point[2] + 30};
          mech3::move_mech(mate, m, 0.0f);
          pm_kill_hurt(mate);
        }
        if (idle >= 0 && t == idle + 180) {
          ordered = t;
          std::printf("  after the repair: bases alive %d, mfb ask %d, repair possible %d\n", mech3::program_bases_alive(),
                      mech3::program_mfb_ask(mate) ? 1 : 0, mech3::ai::repair_possible(&brain->current()) ? 1 : 0);
          g_mate_legs0 = pm_kill_leg_armor(mate);
          g_mate_order = brain->lance_order(12, nullptr);
          std::printf("  order 12: %d\n", g_mate_order);
        }
        if (ordered >= 0) {
          mech3::LaunchedMech dm;
          mech3::describe_mech(mate, &dm);
          float m[3];
          mech3::mech_world_position(mate, m);
          if (dm.powered_down && std::hypot(m[0] - point[0], m[2] - point[2]) < 6.5f) g_mate_docked = true;
          const mech3::ai::Behaviour* beh = brain->current().state ? brain->current().state->behaviour() : nullptr;
          const bool repairing = beh && beh->kind == mech3::ai::StepKind::Repair;  // PM's state or MW3's
          if (g_mate_docked && !repairing && !g_mate_done) {
            g_mate_done = g_refit_done = true;  // kill_run's end asks for a refit done
            g_mate_legs1 = pm_kill_leg_armor(mate), ordered = t - 3300;
          }
        }
        if (ordered >= 0 && (t - ordered) % 300 == 0) {
          float m[3];
          mech3::mech_world_position(mate, m);
          mech3::LaunchedMech dm;
          mech3::describe_mech(mate, &dm);
          std::printf("  her t+%d: %.1f m from the point, powered down %d, legs %.3f, state %s\n", t - ordered,
                      std::hypot(m[0] - point[0], m[2] - point[2]), dm.powered_down ? 1 : 0, pm_kill_leg_armor(mate),
                      typeid(*brain->current().state).name());
        }
        if (t > 12000 || (ordered >= 0 && t > ordered + 3600)) done = true;
        return;
      }
      if (t == 40 && brain) {
        std::printf("  bases alive %d, mfb ask %d, repair possible %d\n", mech3::program_bases_alive(),
                    mech3::program_mfb_ask(mate) ? 1 : 0, mech3::ai::repair_possible(&brain->current()) ? 1 : 0);
        float dp[3] = {};
        std::printf("  has mcb %d, drive %d, points %d, dock point %d\n", mech3::program_has_mcb() ? 1 : 0,
                    mech3::program_dock_drive() ? 1 : 0, mech3::program_mcb_points(), mech3::program_mcb_point(dp) ? 1 : 0);
        std::printf("  order 12: %d\n", brain->lance_order(12, nullptr));
      }
      if (t > 60) done = true;
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
    if (std::getenv("PM_HEADLIGHT")) {  // the player's headlight on at frame 2, its light's place at 2 and 60
      static int t = 0;
      float at[3] = {};
      if (++t == 2 || t == 60) std::printf("  headlight: %d at %.0f %.0f %.0f\n", pm_headlight(player, true, at), at[0], at[1], at[2]);
      if (t < 60) return;
    }
    // The vehicle editor's Show InOut (docs/editor.md, Pirate's Moon's inout drawing): the boundary's walls built
    // as PM's models on the world at frame 30, then taken down.
    if (std::getenv("PM_INOUT")) {
      static int t = 0;
      if (++t < 30) return;
      mech3::ai::inout_view_build();
      if (zcls::Node* root = mech3::ai::inout_view()) {
        g_inout.built = true;
        for (int32_t i = 0; i < root->child_count(); ++i) {
          const pm::zmod::Model* m = root->children[i] ? root->children[i]->pm_model : nullptr;
          if (!m) continue;
          ++g_inout.models;
          g_inout.polygons += m->count(0x10);
          g_inout.refs += m->count(0x5c);
          for (int32_t k = 0; k < m->count(0x10); ++k) g_inout.back += (m->polygons[k].flags & 0x400) != 0;
        }
      }
      mech3::ai::inout_view_show(nullptr);
      g_inout.freed = mech3::ai::inout_view_empty();
      done = true;
      return;
    }
    // The editor's TELEPORT (docs/editor.md, Pirate's Moon's teleport) at frame 30: the last of the world's places
    // picked, OK moves the player there and saves into the mod; then another picked and Cancel, nothing moved or saved.
    // The water survey (pm_kill.cpp's pm_water_survey): PM_WATER the label, PM_WATER_HEIGHTS "low,high" the shortest and
    // tallest mech's heights; PM_HEIGHT prints the player's mech's height instead (PM_MECH picks the mech).
    if (std::getenv("PM_WATER") || std::getenv("PM_HEIGHT")) {
      static int t = 0;
      if (++t < 30) return;
      done = true;
      if (std::getenv("PM_HEIGHT")) {
        std::printf("HEIGHT mech %s: %.2f\n", std::getenv("PM_MECH") ? std::getenv("PM_MECH") : "-", pm_mech_height(player));
        return;
      }
      float low = 8.0f, high = 16.0f;
      if (const char* h = std::getenv("PM_WATER_HEIGHTS")) std::sscanf(h, "%f,%f", &low, &high);
      pm_water_survey(player, std::getenv("PM_WATER"), low, high, 20.0f);
      return;
    }
    if (std::getenv("PM_TELEPORT")) {
      static int t = 0;
      if (++t < 30) return;
      done = true;
      mech3::editor_key(mech3::kEditorCommand);
      mech3::ai::net_save_into_mod() = "tp";
      {  // the editor's own mod: saves go only into a folder with its marker (save_as.h)
        std::error_code ec;
        const std::filesystem::path mod = std::filesystem::path(zutl::mounts().write) / "mods" / "tp";
        std::filesystem::create_directories(mod, ec);
        std::ofstream(mod / mech3::ai::kEditorMarker, std::ios::binary) << "the editor's\n";
      }
      mech3::teleport_open(0);
      mech3::EditorForm& form = mech3::teleport_form();
      mech3::FormField* place = form.field("place");
      const std::vector<mech3::TeleportSpot>* spots = mech3::teleport_dialog().spots;
      if (!place || !place->combo || !spots || spots->empty()) return;
      for (const std::string& s : place->combo->texts()) g_tp.listed += s + "; ";
      for (const mech3::TeleportSpot& s : *spots)
        if (!s.kept) g_tp.nav_names.push_back(s);
      const mech3::TeleportSpot& to = spots->back();  // location.zrd's last
      g_tp.place = to.name;
      for (int k = 0; k < 3; ++k) g_tp.want[k] = to.pos[k];
      for (size_t i = 0; i < place->combo->items.size(); ++i)
        if (place->combo->items[i].text == to.name) *place->i = static_cast<int32_t>(i);
      form.changed(*place);
      form.press("OK");
      g_tp.saved = mech3::teleport_last_saved();
      g_tp.moved = mech3::mech_world_position(player, g_tp.at);
      if (std::FILE* f = std::fopen(g_tp.saved.c_str(), "rb")) {
        char b[4096];
        g_tp.text.assign(b, std::fread(b, 1, sizeof b, f));
        std::fclose(f);
      }
      mech3::teleport_open(0);
      mech3::EditorForm& again = mech3::teleport_form();
      if (mech3::FormField* p2 = again.field("place"); p2 && p2->combo)
        for (size_t i = 0; i < p2->combo->items.size(); ++i) {
          const std::string& s = p2->combo->items[i].text;
          if (s != "Current Location" && s != g_tp.place) *p2->i = static_cast<int32_t>(i);
        }
      if (mech3::FormField* p2 = again.field("place")) again.changed(*p2);
      std::error_code ec;
      const auto when = std::filesystem::last_write_time(g_tp.saved, ec) - std::chrono::hours(1);
      std::filesystem::last_write_time(g_tp.saved, when, ec);
      again.press("Cancel");
      float after[3] = {};
      g_tp.cancel_stayed = mech3::mech_world_position(player, after) && std::fabs(after[0] - g_tp.at[0]) < 0.01f &&
                           std::fabs(after[2] - g_tp.at[2]) < 0.01f && std::filesystem::last_write_time(g_tp.saved, ec) == when;
      mech3::ai::net_save_into_mod().clear();
      mech3::editor_key(mech3::kEditorCommand);
      return;
    }
    // A mod's splash on AC5 (200 m), hit where the query fills 32: the player's spot, else each one found within 2 km.
    if (std::getenv("PM_SPLASH")) {
      static int t = 0;
      if (++t < 30) return;
      float at[3], points[32][3];
      mech3::mech_world_position(player, at);
      const int32_t n = mech3::weapons_query_points(at, 2000.0f, points);
      for (int32_t i = -1; i < n && g_splash.counted < 32; ++i, ++g_splash.tried)
        g_splash.made = mech3::weapons_splash_copy("AC5", 200.0f, i < 0 ? at : points[i], &g_splash.counted,
                                                   &g_splash.listed, &g_splash.damaged);
      done = true;
      return;
    }
    // pm-tests 37: the player on the land bridge where Owen stood, anhur04 shot down as it passes (alive and back within 240 m
    // of him after its loop, about frame 1500 on its scripted flight under either AI), the segments a shot and a walk take toward its
    // fall 150 frames on. PM_ANHUR=frame kills it at that frame instead; PM_ANHUR_TRACK logs to 900 frames past the
    // kill, with the frame the deck (hslid01) gives way under the Anhur's fire.
    if (std::getenv("PM_ANHUR")) {
      static int t = 0;
      ++t;
      zcls::Node* root = zcls::list_find_by_name(6, "anhur04");
      if (t == 5) {
        const float at[3] = {1958.6f, 0, -828.7f};
        mech3::chain_player_at(at);
      }
      // The AI mechs held powered down: their fire on the deck (random under MW3's AI, from frame 371 in some runs)
      // dropped the player into the canyon and ended the mission before the Anhur came back.
      if (t >= 5)
        for (void* m : targets) pm_hold_down(m);
      const int kill_at = std::atoi(std::getenv("PM_ANHUR")) > 1 ? std::atoi(std::getenv("PM_ANHUR")) : -1;
      if (zcls::Node* d = zcls::list_find_by_name(6, "hslid01"); d && !(d->flags() & 4) && g_anhur.deck_gone < 0) g_anhur.deck_gone = t;
      if (std::getenv("PM_ANHUR_TRACK") && t % 30 == 0) {
        float a[3] = {}, h[3] = {}, me[3] = {}, lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        zcls::Node *dn = nullptr, *hn = nullptr;  // anhur04's own (each Anhur has a "destroyed")
        std::function<void(zcls::Node*)> find = [&](zcls::Node* q) {
          if (!dn && std::strcmp(q->name(), "destroyed") == 0) dn = q;
          if (!hn && std::strcmp(q->name(), "healthy") == 0) hn = q;
          for (int32_t i = 0; i < q->child_count(); ++i) find(q->children[i]);
        };
        if (root) find(root);
        if (dn) zcls::node_world_point(dn, a);
        if (hn) zcls::node_world_point(hn, h);
        float c[8][3];
        if (root && zcls::node_box_corners(root, c) == 0)
          for (int i = 0; i < 8; ++i)
            for (int k = 0; k < 3; ++k) lo[k] = std::min(lo[k], c[i][k]), hi[k] = std::max(hi[k], c[i][k]);
        if (dn && std::getenv("PM_ANHUR_GROUND")) {  // what the fall's ground query (0x004edab0) returns under the wreck
          auto* world = static_cast<zcls::Node*>(mech3::world_hooks().world);
          static zcls::GroundHits g;
          const int rc = zcls::ground_point(world, a[0], a[1] + 20.0f, a[2], &g);
          std::printf("  anhur ground under the wreck (rc %d): %d hits", rc, g.count);
          for (int32_t i = 0; i < g.count && i < 6; ++i) std::printf(" | %.1f %s", g.hit[i].height, g.hit[i].node ? g.hit[i].node->name() : "-");
          std::printf("\n");
        }
        mech3::mech_world_position(player, me);
        std::printf("  anhur t %d: root flags %x, healthy %x at %.0f %.0f %.0f, destroyed %x at %.0f %.0f %.0f, box x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f, player %.1f %.1f %.1f, deck gone %d\n",
                    t, root ? root->flags() : 0, hn ? hn->flags() : 0, h[0], h[1], h[2], dn ? dn->flags() : 0, a[0], a[1], a[2], lo[0], hi[0], lo[1], hi[1], lo[2], hi[2],
                    me[0], me[1], me[2], g_anhur.deck_gone);
      }
      const int k = g_anhur.kill_frame;
      if (zcls::Node* x = zcls::list_find_by_name(6, "xslid01"); x && k >= 0) {
        if (t == k + 90) g_anhur.bridge_boxed_alive = (x->flags() & 0x20) != 0;
        if (t == k + 150) g_anhur.bridge_boxed_dead = (x->flags() & 0x20) != 0;
      }
      if (k < 0 && root && t > 5) {  // the kill: at PM_ANHUR's frame, else as it passes the bridge
        float p[3] = {};
        std::function<zcls::Node*(zcls::Node*)> flying = [&](zcls::Node* q) -> zcls::Node* {  // its "destroyed" follows the flight
          if (std::strcmp(q->name(), "destroyed") == 0) return q;
          for (int32_t i = 0; i < q->child_count(); ++i)
            if (zcls::Node* r = flying(q->children[i])) return r;
          return nullptr;
        };
        zcls::Node* body = flying(root);
        zcls::node_world_point(body ? body : root, p);
        const float dx = p[0] - 1958.6f, dz = p[2] + 828.7f;
        if (dx * dx + dz * dz > 400.0f * 400.0f) g_anhur.away = true;
        if (kill_at > 1 ? t == kill_at : g_anhur.away && (root->flags() & 4) && dx * dx + dz * dz < 240.0f * 240.0f) {
          g_anhur.boxed_alive = (root->flags() & 0x20) != 0;
          g_anhur.killed = pm_kill_turret("anhur04");
          g_anhur.kill_frame = t;
          std::printf("  anhur04 shot down at frame %d, %.0f m from the bridge\n", t, std::sqrt(dx * dx + dz * dz));
        }
      }
      if (k >= 0 && t == k + 150 && root) {
        g_anhur.boxed_dead = (root->flags() & 0x20) != 0;
        auto* world = static_cast<zcls::Node*>(mech3::world_hooks().world);
        static zcls::LineHits r;
        const auto first = [&](const float a[3], const float b[3], float* d) -> zcls::Node* {
          if (zcls::segment_nearest(world, a, b, &r) != 0 || r.count >= r.total) return nullptr;
          const zcls::LineHit& h = r.hit[r.count];
          *d = std::sqrt((h.point[0] - a[0]) * (h.point[0] - a[0]) + (h.point[1] - a[1]) * (h.point[1] - a[1]) + (h.point[2] - a[2]) * (h.point[2] - a[2]));
          return h.node;
        };
        // Up past the falling wreck toward where the Anhur died, and along the deck at a mech's knee (Owen's "hit wall" at y 1.9).
        const float eye[3] = {1958.6f, 7.0f, -828.7f}, sky[3] = {1930.0f, 45.0f, -1000.0f};
        const float knee[3] = {1958.6f, 1.9f, -828.7f}, ahead[3] = {1958.6f, 1.9f, -900.0f};
        std::function<void(zcls::Node*, bool)> pieces = [&](zcls::Node* q, bool live) {
          live = live && (q->flags() & 4);
          if (pm::zmod::Model* md = q->pm_model; live && md && (q->flags() & 0x10) && md->count(0x10) > 0) {
            bool seen = false;
            for (int32_t i = 0; i < md->count(0x10) && !seen; ++i)
              for (int32_t k = 0; k < md->polygons[i].material_count; ++k)
                if (md->polygons[i].materials[k] && md->polygons[i].materials[k]->alpha != 0) seen = true;
            ++g_anhur.solid;
            if (!seen) ++g_anhur.unseen, std::printf("  anhur: %s solid, nothing drawn\n", q->name());
          }
          for (int32_t i = 0; i < q->child_count(); ++i) pieces(q->children[i], live);
        };
        pieces(root, true);
        g_anhur.shot = first(eye, sky, &g_anhur.shot_at);
        g_anhur.walk = first(knee, ahead, &g_anhur.walk_at);
      }
      if ((k >= 0 && t == k + (std::getenv("PM_ANHUR_TRACK") ? 900 : 150)) || t > 4000) done = true;  // 4000: it never came
      return;
    }
    // pm-tests 37's survey: every turret boxed by hand (PM's aim ray never boxes one; a data BOX would), then
    // destroyed (frame 30); 1500 frames on, each root's box against its live pieces' boxes: how far past them it reaches.
    if (std::getenv("PM_DEADROOTS")) {
      static int t = 0;
      static std::vector<zcls::Node*> roots;
      if (++t == 30) {
        pm_turret_roots(&roots);
        for (zcls::Node* r : roots) {
          std::printf("  deadroot %s at load: flags %x%s\n", r->name(), r->flags(), (r->flags() & 0x20) ? " (boxed already: the reticle, as the player starts near it)" : "");
          zcls::node_set_intersect_bbox(r, 1);
          pm_kill_turret(r->name());
        }
        std::printf("  deadroots: %zu turrets destroyed\n", roots.size());
      }
      if (t < 1530) return;
      const auto extent = [](zcls::Node* n, float lo[3], float hi[3]) {
        float c[8][3];
        if (zcls::node_box_corners(n, c) != 0) return false;
        for (int i = 0; i < 8; ++i)
          for (int k = 0; k < 3; ++k) lo[k] = std::min(lo[k], c[i][k]), hi[k] = std::max(hi[k], c[i][k]);
        return true;
      };
      int walls = 0;
      for (zcls::Node* r : roots) {
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f}, at[3] = {};
        if (!extent(r, lo, hi)) continue;
        zcls::node_world_point(r, at);
        const bool boxed = (r->flags() & 0x20) != 0;
        walls += boxed;
        std::printf("  deadroot %s at %.0f %.0f %.0f: %s, box %.0f x %.0f x %.0f m, y %.0f..%.0f\n", r->name(), at[0], at[1], at[2],
                    boxed ? "BOXED (stops shots and mechs)" : "mesh only", hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], lo[1], hi[1]);
        if (const char* tree = std::getenv("PM_DEADROOTS_TREE"); tree && std::strcmp(tree, r->name()) == 0) {
          std::function<void(zcls::Node*, int)> dump = [&](zcls::Node* q, int d) {
            float w[3] = {};
            zcls::node_world_point(q, w);
            std::printf("    %*s%s flags %x at %.0f %.0f %.0f\n", 2 * d, "", q->name(), q->flags(), w[0], w[1], w[2]);
            for (int32_t i = 0; i < q->child_count() && d < 5; ++i) dump(q->children[i], d + 1);
          };
          dump(r, 0);
        }
      }
      std::printf("  deadroots: %d of %zu dead turrets boxed\n", walls, roots.size());
      done = true;
      return;
    }
    // pm-tests item 30: an animation object's saved state loaded back, in PM's layout and in MW3's (our earlier builds').
    if (std::getenv("PM_ANIMSAVE")) {
      static int t = 0;
      if (++t < 30) return;
      for (int old = 0; old < 2; ++old)
        g_animsave.found[old] = zeff::anim_state_round_trip(old != 0, &g_animsave.name[old], &g_animsave.restored[old]);
      done = true;
      return;
    }
    // pm-tests item 21: the current weapon destroyed with the next in the list already gone, the HUD up.
    if (std::getenv("PM_GONE")) {
      bool picked = false;
      const int32_t on = mech3::mech_weapon_gone_case(player, &picked);
      const bool hud = mech3::program_mech_hud(player) != nullptr;
      std::printf("  the HUD up %d, the current weapon selected %d, on a destroyed one after %d\n", hud, picked, on);
      EXPECT(hud && picked && on == 0);
      done = true;
      return;
    }
    // pm-tests item 25: the player 200 m from this mech, it targeted and the reticle locked at frame 30; at 60
    // E (the next enemy) and the lock logged each frame after.
    if (std::getenv("PM_LOCK")) {
      static int t = 0;
      ++t;
      bool has = false;
      if (t == 2) {
        const float to[3] = {there[0] + 200.0f, pm_kill_ground(there[0] + 200.0f, 300.0f, there[2]), there[2]};
        mech3::move_mech(player, to, static_cast<float>(std::atan2(200.0, 0.0)));
      }
      if (t == 30) mech3::program_hud_target(m, 0), mech3::program_reticle_lock(3, 3.5f, &has);
      if (t == 60) pm_command(player, 0x48);
      if (t >= 58 && t <= 110 && (t < 64 || t % 4 == 0)) {
        const int32_t lock = mech3::program_reticle_lock(-1, 0, &has);
        std::printf("  lock t %d: state %d weapons may lock %d\n", t, lock, has);
      }
      if (t == 110) done = true;
      return;
    }
    if (std::getenv("PM_LIGHTS")) {  // every world light: active, its parents, its world place (pm-tests item 20)
      auto* wn = static_cast<zcls::Node*>(mech3::world_hooks().world);
      auto* w = wn ? static_cast<zcls::World*>(wn->data) : nullptr;
      for (int32_t i = 0; w && i < w->get<int32_t>(0xa0); ++i) {
        zcls::Node* l = w->lights[i];
        float at[3] = {};
        zcls::node_world_point(l, at);
        std::printf("  light %s flags %x at %.0f %.0f %.0f parents", l->name(), l->flags(), at[0], at[1], at[2]);
        for (int32_t k = 0; k < l->parent_count(); ++k) std::printf(" %s", l->parents[k] ? l->parents[k]->name() : "-");
        std::printf("\n");
      }
      done = true;
      return;
    }
    if (const char* sky = std::getenv("PM_WALL_SKY")) {  // pm-tests 37: "x,z,heading,frames": the player kept there, a fan up into the sky every 30 frames
      static int t = 0;
      float sx = 2019.0f, sz = -834.0f, sh = 270.0f, until = 4000.0f;
      std::sscanf(sky, "%f,%f,%f,%f", &sx, &sz, &sh, &until);
      ++t;
      if (!std::getenv("PM_WALL_STAY") && (t == 5 || (t % 300 == 0 && !std::getenv("PM_WALL_ONCE")))) {
        const float at[3] = {sx, 0, sz};
        mech3::chain_player_at(at);
      }
      if (t % 30 != 0) return;
      if (t > until) {
        done = true;
        return;
      }
      auto* world = static_cast<zcls::Node*>(mech3::world_hooks().world);
      float me[3], cam[3], torso[3];
      mech3::mech_world_position(player, me);
      if (const char* stay = std::getenv("PM_WALL_STAY")) me[0] = sx, me[1] = static_cast<float>(std::atof(stay)), me[2] = sz;  // the player left be, the rays from there
      if (std::getenv("PM_WALL_CAMERA")) {  // from the outside camera (view 3), as Owen's range reads
        pm_controls_set_view(player, 3);
        if (mech3::program_view_positions(cam, torso)) me[0] = cam[0], me[1] = cam[1] - 10.0f, me[2] = cam[2];
      }
      if (const char* back = std::getenv("PM_WALL_BACK")) {  // dist,up: from behind the mech, as a chase camera
        float bd = 25.0f, bu = 10.0f;
        std::sscanf(back, "%f,%f", &bd, &bu);
        const float a = sh * 3.14159265f / 180.0f;
        me[0] -= std::sin(a) * bd, me[2] -= std::cos(a) * bd, me[1] += bu;
      }
      static zcls::LineHits r;
      std::map<std::string, std::pair<int, float>> seen;  // chain: rays, the nearest
      int rays = 0;
      for (float side = -30.0f; side <= 30.0f; side += 10.0f)
        for (float el = 0.0f; el <= 80.0f; el += 10.0f) {
          ++rays;
          const float a = (sh + side) * 3.14159265f / 180.0f, e = el * 3.14159265f / 180.0f;
          const float from[3] = {me[0], me[1] + 10.0f, me[2]};
          const float to[3] = {from[0] + std::sin(a) * std::cos(e) * 200.0f, from[1] + std::sin(e) * 200.0f, from[2] + std::cos(a) * std::cos(e) * 200.0f};
          if (zcls::segment_nearest(world, from, to, &r) != 0 || r.count >= r.total) continue;
          const zcls::LineHit& h = r.hit[r.count];
          if (el < 10.0f && h.node && (h.node->flags() & 0x8000)) continue;  // the ground ahead
          std::string c;
          for (zcls::Node* n = h.node; n && c.size() < 120; n = n->parent_count() > 0 && n->parents ? n->parents[0] : nullptr)
            c += (c.empty() ? "" : " < ") + std::string(n->name()) + (c.empty() ? "" : "");
          char tail[64];
          std::snprintf(tail, sizeof tail, " (flags %x)", h.node ? h.node->flags() : 0);
          c += tail;
          const float d = std::sqrt((h.point[0] - from[0]) * (h.point[0] - from[0]) + (h.point[1] - from[1]) * (h.point[1] - from[1]) + (h.point[2] - from[2]) * (h.point[2] - from[2]));
          auto& s = seen[c];
          s.second = s.first++ ? std::min(s.second, d) : d;
        }
      std::printf("  sky t %d: from %.0f %.0f %.0f, view %d, %d rays\n", t, me[0], me[1] + 10.0f, me[2], pm_controls_view(player), rays);
      if (const char* kill = std::getenv("PM_WALL_KILL"); kill && t == std::atoi(kill)) {  // frame: destroy_anhur04 run, as a shot down
        std::printf("  sky: anhur04's turret %s\n", pm_kill_turret("anhur04") ? "destroyed" : "not found");
      }
      for (const char* nm : {"anhur01", "anhur02", "anhur03", "anhur04"})
        if (zcls::Node* z = zcls::list_find_by_name(6, nm)) {
          float p[3] = {}, c[8][3] = {}, lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
          zcls::node_world_point(z, p);
          const int got = zcls::node_box_corners(z, c);
          for (int i = 0; i < 8; ++i)
            for (int k = 0; k < 3; ++k) lo[k] = std::min(lo[k], c[i][k]), hi[k] = std::max(hi[k], c[i][k]);
          std::printf("    %s at %.1f %.1f %.1f flags %x, box (%d) %.0f..%.0f %.0f..%.0f %.0f..%.0f\n", nm, p[0], p[1], p[2], z->flags(), got, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]);
          if (std::getenv("PM_WALL_KILL") && std::strcmp(nm, "anhur04") == 0) {
            std::function<void(zcls::Node*, int)> walk = [&](zcls::Node* q, int d) {
              float w[3] = {};
              zcls::node_world_point(q, w);
              std::printf("      %*s%s cls %d flags %x at %.0f %.0f %.0f\n", 2 * d, "", q->name(), q->cls(), q->flags(), w[0], w[1], w[2]);
              for (int32_t i = 0; i < q->child_count() && d < 4; ++i) walk(q->children[i], d + 1);
            };
            walk(z, 0);
          }
        }
      for (auto& [c, s] : seen) std::printf("    %d rays, nearest %.1f m: %s\n", s.first, s.second, c.c_str());
      return;
    }
    if (std::getenv("PM_WALL") && std::getenv("PM_WALL_AT")) {  // the player left be until then
      static int waited = 0;
      if (const char* go = std::getenv("PM_WALL_GOTO"); go && (waited == 5 || waited % 300 == 0)) {  // x,z: the player kept there, as if walked in
        float at[3] = {0, 0, 0};
        std::sscanf(go, "%f,%f", &at[0], &at[2]);
        mech3::chain_player_at(at);
      }
      if (++waited < std::atoi(std::getenv("PM_WALL_AT"))) return;
      if (waited == std::atoi(std::getenv("PM_WALL_AT"))) std::printf("  wall: probing at frame %d\n", waited);
    }
    if (const char* wall = std::getenv("PM_WALL")) {  // pm-tests 37 (PM_WALL_AT=frames: that many frames in, above): "x,z,radius,heading": the first node off the ground west-ish rays meet
      float cx = 1943.1f, cz = -833.6f, radius = 150.0f, heading = 270.0f;
      std::sscanf(wall, "%f,%f,%f,%f", &cx, &cz, &radius, &heading);
      auto* world = static_cast<zcls::Node*>(mech3::world_hooks().world);
      if (const char* down = std::getenv("PM_WALL_DOWN")) {  // a,b: those nodes made inactive first, as a destroy anim's ObjectActiveState does
        std::string all = down;
        for (size_t q = 0; q <= all.size();) {
          const size_t e = all.find(',', q);
          const std::string nm = all.substr(q, e == std::string::npos ? std::string::npos : e - q);
          zcls::Node* z = zcls::list_find_by_name(6, nm.c_str());
          std::printf("  wall: %s %s\n", nm.c_str(), z ? "made inactive" : "not found");
          if (z) zcls::node_set_active(z, 0);
          if (e == std::string::npos) break;
          q = e + 1;
        }
      }
      if (const char* up = std::getenv("PM_WALL_UP")) {  // a,b: those nodes made active first, as a destroy anim's ObjectActiveState does
        std::string all = up;
        for (size_t q = 0; q <= all.size();) {
          const size_t e = all.find(',', q);
          const std::string nm = all.substr(q, e == std::string::npos ? std::string::npos : e - q);
          zcls::Node* z = zcls::list_find_by_name(6, nm.c_str());
          std::printf("  wall: %s %s\n", nm.c_str(), z ? "made active" : "not found");
          if (z) zcls::node_set_active(z, 1);
          if (e == std::string::npos) break;
          q = e + 1;
        }
      }
      if (const char* down = std::getenv("PM_WALL_DOWN"); down && std::getenv("PM_WALL_FAN"))
        for (const char* nm : {"hslid01"}) if (zcls::Node* z = zcls::list_find_by_name(6, nm)) zcls::node_set_active(z, 0), (void)down;
      if (const char* fan = std::getenv("PM_WALL_FAN")) {  // x,z,heading: a range finder's fan from the cockpit's height, 80 m out
        float fx = 2019.0f, fz = -834.0f, fh = 270.0f;
        std::sscanf(fan, "%f,%f,%f", &fx, &fz, &fh);
        static zcls::LineHits r;
        const float hi[3] = {fx, 400.0f, fz}, lo[3] = {fx, -400.0f, fz};
        float gy = 0;
        if (zcls::segment_nearest(world, hi, lo, &r) == 0 && r.count < r.total) gy = r.hit[r.count].point[1];
        std::printf("  fan from %.0f %.1f %.0f, heading %.0f\n", fx, gy, fz, fh);
        for (float side = -10.0f; side <= 10.0f; side += 5.0f)
          for (float el = -10.0f; el <= 30.0f; el += 5.0f) {
            const float a = (fh + side) * 3.14159265f / 180.0f, e = el * 3.14159265f / 180.0f;
            const float from[3] = {fx, gy + 10.0f, fz};
            const float to[3] = {fx + std::sin(a) * std::cos(e) * 80.0f, gy + 10.0f + std::sin(e) * 80.0f, fz + std::cos(a) * std::cos(e) * 80.0f};
            if (zcls::segment_nearest(world, from, to, &r) != 0 || r.count >= r.total) {
              std::printf("    side %+3.0f el %+3.0f: nothing\n", side, el);
              continue;
            }
            const zcls::LineHit& h = r.hit[r.count];
            std::string c;
            for (zcls::Node* n = h.node; n && c.size() < 120; n = n->parent_count() > 0 && n->parents ? n->parents[0] : nullptr) c += (c.empty() ? "" : " < ") + std::string(n->name());
            const float dx = h.point[0] - from[0], dy = h.point[1] - from[1], dz = h.point[2] - from[2];
            std::printf("    side %+3.0f el %+3.0f: %.1f m, at %.1f %.1f %.1f, normal %.2f %.2f %.2f, %s, flags %x, texture %s\n", side, el, std::sqrt(dx * dx + dy * dy + dz * dz),
                        h.point[0], h.point[1], h.point[2], h.normal[0], h.normal[1], h.normal[2], c.c_str(), h.node ? h.node->flags() : 0,
                        h.material && h.material->texture ? h.material->texture->name : (h.material ? "untextured" : "box"));
          }
        done = true;
        return;
      }
      if (std::getenv("PM_WALL_SURVEY")) {  // pm-tests 37's survey: collidable nodes never drawn (alpha 0), and box collision far larger than the model
        auto chain = [](zcls::Node* n) {
          std::string c;
          for (int d = 0; n && d < 4; ++d) {
            c += (d ? " < " : "") + std::string(n->name());
            n = n->parent_count() > 0 && n->parents ? n->parents[0] : nullptr;
          }
          return c;
        };
        int clear = 0, boxed = 0, airy = 0;
        for (zcls::ListItem* it = zcls::list_head(6); it; it = it->next) {
          zcls::Node* x = it->node;
          pm::zmod::Model* md = x ? x->pm_model : nullptr;
          if (!md || md->count(0x10) == 0 || md->count(0x14) == 0 || !(x->flags() & 4) || !(x->flags() & 0x10)) continue;
          if (x->flags() & 0x200000) continue;  // clutter templates: touch, never walls (cls_clutter)
          bool live = true;  // an inactive parent keeps its subtree out of the queries too
          for (zcls::Node* q = x; q && live; q = q->parent_count() > 0 && q->parents ? q->parents[0] : nullptr) live = (q->flags() & 4) != 0;
          if (!live) continue;
          float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
          for (int32_t i = 0; i < md->count(0x14); ++i)
            for (int k = 0; k < 3; ++k) lo[k] = std::min(lo[k], md->vertices[i * 3 + k]), hi[k] = std::max(hi[k], md->vertices[i * 3 + k]);
          float at[3] = {};
          zcls::node_world_point(x, at);
          bool all_clear = true;
          for (int32_t i = 0; i < md->count(0x10) && all_clear; ++i)
            for (int32_t k = 0; k < md->polygons[i].material_count; ++k)
              if (md->polygons[i].materials[k] && md->polygons[i].materials[k]->alpha != 0) all_clear = false;
          if (all_clear) {
            ++clear;
            std::printf("  survey clear %s: at %.0f %.0f %.0f, model %.1f..%.1f %.1f..%.1f %.1f..%.1f, %d polygons, flags %x\n", chain(x).c_str(), at[0], at[1], at[2],
                        lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], md->count(0x10), x->flags());
          }
          if (!(x->flags() & 0x20)) continue;
          ++boxed;
          // The box's faces, 6 x 6 points each: how many lie over 1.5 m from every vertex (air the box stops a shot in).
          int far = 0, total = 0;
          for (int face = 0; face < 6; ++face)
            for (int a = 0; a < 6; ++a)
              for (int b = 0; b < 6; ++b) {
                float pt[3];
                const int axis = face / 2, u = (axis + 1) % 3, v = (axis + 2) % 3;
                pt[axis] = face % 2 ? hi[axis] : lo[axis];
                pt[u] = lo[u] + (hi[u] - lo[u]) * (a + 0.5f) / 6, pt[v] = lo[v] + (hi[v] - lo[v]) * (b + 0.5f) / 6;
                float best = 1e18f;
                for (int32_t i = 0; i < md->count(0x14); ++i) {
                  const float* q = md->vertices + i * 3;
                  best = std::min(best, (q[0] - pt[0]) * (q[0] - pt[0]) + (q[1] - pt[1]) * (q[1] - pt[1]) + (q[2] - pt[2]) * (q[2] - pt[2]));
                }
                ++total, far += best > 2.25f;
              }
          if (far * 2 > total) {
            ++airy;
            std::printf("  survey airy %s: at %.0f %.0f %.0f, box %.1f x %.1f x %.1f, %d%% of its faces in the air, %d vertices, flags %x\n", chain(x).c_str(), at[0], at[1],
                        at[2], hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], far * 100 / total, md->count(0x14), x->flags());
          }
        }
        std::printf("  survey: %d collidable see-through, %d box-collided (%d mostly air)\n", clear, boxed, airy);
        done = true;
        return;
      }
      if (std::getenv("PM_WALL_LAVA")) {  // collidable untextured shapes against the visible lava/water surfaces over them
        struct Box { zcls::Node* n; float lo[3], hi[3]; };
        std::vector<Box> solid, surface;
        for (zcls::ListItem* it = zcls::list_head(6); it; it = it->next) {
          zcls::Node* x = it->node;
          pm::zmod::Model* md = x ? x->pm_model : nullptr;
          if (!md || md->count(0x10) == 0 || md->count(0x14) == 0 || !(x->flags() & 4)) continue;
          const zmod::Material* mt = md->polygons[0].material_count > 0 ? md->polygons[0].materials[0] : nullptr;
          Box b{x, {1e9f, 1e9f, 1e9f}, {-1e9f, -1e9f, -1e9f}};
          for (int32_t i = 0; i < md->count(0x14); ++i)
            for (int k = 0; k < 3; ++k) b.lo[k] = std::min(b.lo[k], md->vertices[i * 3 + k]), b.hi[k] = std::max(b.hi[k], md->vertices[i * 3 + k]);
          const bool flat = b.hi[1] - b.lo[1] < 0.01f;
          if (!(x->flags() & 0x10) && flat) surface.push_back(b);           // drawn, walked through: lava, water
          else if ((x->flags() & 0x10) && mt && !mt->texture) solid.push_back(b);  // collidable, untextured
        }
        int above = 0;
        for (const Box& b : solid) {
          for (const Box& f : surface) {
            if (b.hi[0] < f.lo[0] || b.lo[0] > f.hi[0] || b.hi[2] < f.lo[2] || b.lo[2] > f.hi[2]) continue;
            const zmod::Material* sm = f.n->pm_model->polygons[0].material_count ? f.n->pm_model->polygons[0].materials[0] : nullptr;
            const bool up = b.hi[1] > f.lo[1] + 0.05f;
            above += up;
            std::printf("  lava %s y %.1f..%.1f x %.0f..%.0f z %.0f..%.0f | surface %s (%s) y %.1f: %s\n", b.n->name(), b.lo[1], b.hi[1], b.lo[0],
                        b.hi[0], b.lo[2], b.hi[2], f.n->name(), sm && sm->texture ? sm->texture->name : "none", f.lo[1],
                        up ? "STANDS ABOVE" : "below or level");
          }
        }
        std::printf("  lava: %zu collidable untextured, %zu walk-through surfaces, %d pairs standing above\n", solid.size(), surface.size(), above);
        done = true;
        return;
      }
      if (const char* clear = std::getenv("PM_WALL_CLEAR")) {  // x,z,r: collidable nodes (active, 0x10) every polygon of which is see-through (alpha 0)
        float px = 0, pz = 0, pr = 150;
        std::sscanf(clear, "%f,%f,%f", &px, &pz, &pr);
        int all = 0;
        for (zcls::ListItem* it = zcls::list_head(6); it; it = it->next) {
          zcls::Node* x = it->node;
          pm::zmod::Model* md = x ? x->pm_model : nullptr;
          if (!md || md->count(0x10) == 0 || !(x->flags() & 4) || !(x->flags() & 0x10)) continue;
          bool clear_all = true;
          for (int32_t i = 0; i < md->count(0x10) && clear_all; ++i) {
            const auto& pg = md->polygons[i];
            for (int32_t k = 0; k < pg.material_count; ++k)
              if (pg.materials[k] && pg.materials[k]->alpha != 0) clear_all = false;
          }
          if (!clear_all) continue;
          float at[3] = {};
          zcls::node_world_point(x, at);
          float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
          for (int32_t i = 0; i < md->count(0x14); ++i)
            for (int k = 0; k < 3; ++k) lo[k] = std::min(lo[k], md->vertices[i * 3 + k]), hi[k] = std::max(hi[k], md->vertices[i * 3 + k]);
          ++all;
          const float cx2 = at[0] + (lo[0] + hi[0]) / 2, cz2 = at[2] + (lo[2] + hi[2]) / 2;
          if (std::fabs(cx2 - px) > pr || std::fabs(cz2 - pz) > pr) continue;
          std::printf("  clear %s < %s: centre %.1f %.1f, model x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f, %d polygons, flags %x\n", x->name(),
                      x->parent_count() > 0 && x->parents ? x->parents[0]->name() : "-", cx2, cz2, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2],
                      md->count(0x10), x->flags());
        }
        std::printf("  clear: %d collidable see-through nodes in the world\n", all);
        done = true;
        return;
      }
      if (const char* col = std::getenv("PM_WALL_COLUMN")) {  // x,z: every model over that point, its y range, flags, colour
        float px = 0, pz = 0;
        std::sscanf(col, "%f,%f", &px, &pz);
        for (zcls::ListItem* it = zcls::list_head(6); it; it = it->next) {
          zcls::Node* x = it->node;
          pm::zmod::Model* md = x ? x->pm_model : nullptr;
          if (!md || md->count(0x14) == 0) continue;
          float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
          for (int32_t i = 0; i < md->count(0x14); ++i)
            for (int k = 0; k < 3; ++k) lo[k] = std::min(lo[k], md->vertices[i * 3 + k]), hi[k] = std::max(hi[k], md->vertices[i * 3 + k]);
          if (px < lo[0] || px > hi[0] || pz < lo[2] || pz > hi[2]) continue;  // world models are in world space
          const zmod::Material* mt = md->count(0x10) > 0 && md->polygons[0].material_count > 0 ? md->polygons[0].materials[0] : nullptr;
          std::printf("  column %s < %s: y %.1f..%.1f, %d polygons, flags %x, colour %.0f %.0f %.0f alpha %u texture %s\n", x->name(),
                      x->parent_count() > 0 && x->parents ? x->parents[0]->name() : "-", lo[1], hi[1], md->count(0x10), x->flags(),
                      mt ? mt->r : -1.f, mt ? mt->g : -1.f, mt ? mt->b : -1.f, mt ? mt->alpha : 0u, mt && mt->texture ? mt->texture->name : "none");
        }
        done = true;
        return;
      }
      static zcls::LineHits r;
      auto chain = [](zcls::Node* n) {
        std::string c;
        for (int d = 0; n && d < 6; ++d) {
          c += (d ? " < " : "") + std::string(n->name());
          n = n->parent_count() > 0 && n->parents ? n->parents[0] : nullptr;
        }
        return c;
      };
      std::map<std::string, std::pair<int, std::string>> seen;  // node chain -> hits, a sample
      int grounds = 0, rays = 0;
      const float step = radius < 30.0f ? 5.0f : 10.0f;
      for (float dx = -radius; dx <= radius; dx += step)
        for (float dz = -radius; dz <= radius; dz += step) {
          const float x = cx + dx, z = cz + dz;
          static const float top = std::getenv("PM_WALL_TOP") ? std::strtof(std::getenv("PM_WALL_TOP"), nullptr) : 400.0f;
          const float hi[3] = {x, top, z}, lo[3] = {x, -400.0f, z};
          if (zcls::segment_nearest(world, hi, lo, &r) != 0 || r.count >= r.total) continue;
          const float gy = r.hit[r.count].point[1];
          ++grounds;
          zcls::Node* ground = r.hit[r.count].node;
          for (float dh = -30.0f; dh <= 30.0f; dh += 15.0f)
            for (float up : {2.0f, 6.0f, 15.0f, 30.0f}) {
              const float a = (heading + dh) * 3.14159265f / 180.0f;
              const float from[3] = {x, gy + up, z};
              // heading 0 = +z, 90 = +x (the HUD's compass)
              const float to[3] = {x + std::sin(a) * 80.0f, gy + up, z + std::cos(a) * 80.0f};
              ++rays;
              if (zcls::segment_nearest(world, from, to, &r) != 0 || r.count >= r.total) continue;
              const zcls::LineHit& h = r.hit[r.count];
              if (!h.node || h.node == ground || std::fabs(h.normal[1]) > 0.78f) continue;
              const std::string c = chain(h.node);
              auto& e = seen[c];
              if (e.first++ == 0) {
                char b[256];
                std::snprintf(b, sizeof b, "from %.0f %.1f %.0f heading %.0f, at %.1f %.1f %.1f, normal %.2f %.2f %.2f, flags %x, class %d, model %p, material %p",
                              x, gy + up, z, heading + dh, h.point[0], h.point[1], h.point[2], h.normal[0], h.normal[1], h.normal[2], h.node->flags(),
                              h.node->get<int32_t>(0x34), static_cast<const void*>(h.node->pm_model), static_cast<const void*>(h.material));
                e.second = b;
                if (h.material) {
                  const zmod::Material* mt = h.material;
                  std::snprintf(b, sizeof b, "; material alpha %u flags %x rgb %.2f %.2f %.2f texture %s",
                                mt->alpha, mt->flags, mt->r, mt->g, mt->b, mt->texture ? mt->texture->name : "none");
                  e.second += b;
                }
                if (h.owner && h.node->pm_box) {  // a clutter record: its box against its model's own extent
                  const auto* rec = static_cast<const pm::zcls::ClutterRecord*>(h.owner);
                  const float* bx = h.node->pm_box;
                  float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
                  int verts = 0;
                  auto ext = [&](zcls::Node* x, auto& self) -> void {
                    if (pm::zmod::Model* md = x->pm_model) {
                      for (int32_t i = 0; i < md->count(0x14); ++i, ++verts)
                        for (int k = 0; k < 3; ++k) lo[k] = std::min(lo[k], md->vertices[i * 3 + k]), hi[k] = std::max(hi[k], md->vertices[i * 3 + k]);
                    }
                    for (int32_t i = 0; i < x->child_count(); ++i) self(x->children[i], self);
                  };
                  ext(h.node, ext);
                  std::snprintf(b, sizeof b, "; record at %.1f %.1f %.1f, fade %.0f..%.0f m, flags %x; box %.1f %.1f %.1f .. %.1f %.1f %.1f; model (%d vertices) %.1f %.1f %.1f .. %.1f %.1f %.1f",
                                rec->matrix.m[9], rec->matrix.m[10], rec->matrix.m[11], std::sqrt(rec->near_sq), std::sqrt(rec->far_sq), rec->flags, bx[0], bx[1], bx[2], bx[3], bx[4], bx[5],
                                verts, lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
                  e.second += b;
                }
              }
            }
        }
      std::printf("  wall probe at %.0f %.0f: %d ground points, %d rays, %zu nodes\n", cx, cz, grounds, rays, seen.size());
      for (const auto& [c, e] : seen) std::printf("  wall %s: %d rays; %s\n", c.c_str(), e.first, e.second.c_str());
      done = true;
      return;
    }
    if (const char* names = std::getenv("PM_NODE")) {  // a,b,c: each named node's world place, then the run ends
      std::string all = names;
      for (size_t p = 0; p <= all.size();) {
        const size_t e = all.find(',', p);
        const std::string n = all.substr(p, e == std::string::npos ? std::string::npos : e - p);
        zcls::Node* z = zcls::list_find_by_name(6, n.c_str());
        float at[3] = {};
        if (z) zcls::node_world_point(z, at);
        std::printf("  node %s: %s %.1f %.1f %.1f\n", n.c_str(), z ? "at" : "not found", at[0], at[1], at[2]);
        auto walk = [](zcls::Node* x, int depth, auto& self) -> void {  // each model under it: polygons, with normals, normals
          if (!x || depth > 6) return;
          std::printf("    %*s%s class %d flags %x children %d\n", depth * 2, "", x->name(), x->get<int32_t>(0x34), x->flags(), x->child_count());
          if (pm::zmod::Model* md = x->pm_model) {
            int with = 0;
            for (int32_t i = 0; i < md->count(0x10); ++i) with += (md->polygons[i].flags & 0x1000) ? 1 : 0;
            float rmin = 1e9f, rmax = 0, ymin = 1e9f, ymax = -1e9f;
            for (int32_t i = 0; i < md->count(0x14); ++i) {
              const float* v = md->vertices + i * 3, r = std::sqrt(v[0] * v[0] + v[2] * v[2]);
              rmin = std::min(rmin, r), rmax = std::max(rmax, r), ymin = std::min(ymin, v[1]), ymax = std::max(ymax, v[1]);
            }
            std::vector<int> angles;  // the vertices' bearings round the model's axis, to the degree: its sides
            for (int32_t i = 0; i < md->count(0x14); ++i) {
              const float* v = md->vertices + i * 3;
              if (std::sqrt(v[0] * v[0] + v[2] * v[2]) > 0.01f) angles.push_back(static_cast<int>(std::lround(std::atan2(v[2], v[0]) * 57.29578f + 360.0)) % 360);
            }
            std::sort(angles.begin(), angles.end());
            angles.erase(std::unique(angles.begin(), angles.end()), angles.end());
            std::printf("    %*s%s: %d polygons, %d with normals, %d normals, flags %x; %d vertices, radius %.2f..%.2f, y %.2f..%.2f, %zu bearings\n", depth * 2, "", x->name(), md->count(0x10), with,
                        md->count(0x18), md->header[8], md->count(0x14), rmin, rmax, ymin, ymax, angles.size());
          }
          for (int32_t i = 0; i < x->child_count(); ++i) self(x->children[i], depth + 1, self);
        };
        walk(z, 0, walk);
        if (e == std::string::npos) break;
        p = e + 1;
      }
      std::printf("  vertex shading %d\n", zmod::draw_state().vertex_shading);
      done = true;
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
    // pm-tests item 18: there is no cow level typed at frame N (PM_COW=N[,x,z]: the player moved to x,z first);
    // the win and loss flags and each objective logged, then Exit Mission.
    if (const char* cow = std::getenv("PM_COW")) {
      static int t = 0;
      int when = 0;
      float x = 0, z = 0;
      const int n = std::sscanf(cow, "%d,%f,%f", &when, &x, &z);
      ++t;
      auto show = [](const mech3::Objective& o, int depth, auto& self) -> void {
        std::printf("  %*s%s kind %d done %d failed %d\n", 2 * depth, "", o.desc ? o.desc->c_str() : "-", o.kind, o.done, o.failed);
        if (auto* g = dynamic_cast<const mech3::ObjectiveGroup*>(&o))
          for (const auto& c : g->items) self(*c, depth + 1, self);
      };
      auto state = [&] {
        const mech3::Objectives& o = mech3::mission_objectives();
        std::printf("  t %d: ended %d aborting %d portal_reach %d\n", t, mech3::missions().ended, mech3::missions().aborting,
                    mech3::ai::valve_find("portal_reach") ? mech3::ai::valve_find("portal_reach")->get() : -1);
        for (const auto* g : {o.primary.get(), o.secondary.get(), o.final.get(), o.fail.get()})
          if (g) show(*g, 1, show);
      };
      if (n == 3 && t == when - 20) {
        const float to[3] = {x, pm_kill_ground(x, 300.0f, z), z};
        mech3::move_mech(player, to, 0.0f);
      }
      if (t == when && std::getenv("PM_COW_PRIMARY")) {  // the primaries done as their conditions would, no cheat
        auto all = [](mech3::Objective& o, auto& self) -> void {
          if (auto* g = dynamic_cast<mech3::ObjectiveGroup*>(&o))
            for (auto& c : g->items) self(*c, self);
          if (!o.done) mech3::complete(o, 1, mech3::mission_objective_hooks());
        };
        state();
        if (mech3::mission_objectives().primary) all(*mech3::mission_objectives().primary, all);
      } else if (t == when) {
        state(), std::printf("  cheat: %d\n", pm_cheat(player, "there is no cow level"));
      }
      if (t == when + 1 || t == when + 150 || t == when + 600) state();
      if (t == when + 601) mech3::missions().on_command(0x79);
      if (t > when + 601 || app.current() != &mech3::states().core) {
        std::printf("  after Exit Mission: failed %d\n", engine.mission_failed());
        done = true;
      }
      return;
    }
    if (std::getenv("PM_WEAPON_LIGHTS")) {  // pm-tests, weapon lights: both FIRE animations, by the choice
      static int t = 0;
      if (++t != 30) return;
      static std::vector<zwep::Handle> played, attached, freed;
      static zwep::HitHooks hk;
      static zwep::RunHooks rk;
      hk = zwep::hit_hooks(), rk = zwep::run_hooks();
      zwep::hit_hooks().effect = [](zwep::Handle a, const float x[9]) { played.push_back(a), hk.effect ? hk.effect(a, x) : void(); };
      zwep::run_hooks().effect_handle = [](zwep::Handle a, const float x[9]) {
        played.push_back(a);
        return rk.effect_handle ? rk.effect_handle(a, x) : zwep::Handle{};
      };
      zwep::run_hooks().anim_attached = [](zwep::Handle a, zwep::Handle n, const void* o) {
        attached.push_back(a);
        return rk.anim_attached ? rk.anim_attached(a, n, o) : zwep::Handle{};
      };
      zwep::run_hooks().anim_free = [](zwep::Handle a) { freed.push_back(a), rk.anim_free ? rk.anim_free(a) : void(); };
      int two = 0;
      for (const zwep::WeaponDef& d : zwep::state().defs) two += d.fire.animation && d.fire.animation2;
      zwep::WeaponDef* laser = zwep::find_def("Laser_Large");
      zwep::WeaponDef* lbx = zwep::find_def("LB2XAC");
      std::printf("  %d weapons with two FIRE animations; laser %p %p, LB-X %p %p flags %x\n", two,
                  laser ? laser->fire.animation : nullptr, laser ? laser->fire.animation2 : nullptr,
                  lbx ? lbx->fire.animation : nullptr, lbx ? lbx->fire.animation2 : nullptr, lbx ? lbx->fire.flags : 0u);
      if (mech3::object_lod() == 2) {  // object detail low: no FIRE animations or effects read, no impact animations
        std::printf("  object detail 2: switches %d %d\n", zwep::state().fire_effects, zwep::state().impact_animations);
        EXPECT(two == 0 && laser && !laser->fire.animation && !zwep::state().fire_effects && !zwep::state().impact_animations);
        zwep::hit_hooks() = hk, zwep::run_hooks() = rk;
        return void(done = true);
      }
      EXPECT(zwep::state().fire_effects && zwep::state().impact_animations);
      EXPECT(two >= 30);  // weapons.zrd's lasers, PPCs, autocannons, gauss: a muzzle flash and a weplight each
      EXPECT(laser && laser->fire.animation && laser->fire.animation2 && lbx && lbx->fire.animation2 && (lbx->fire.flags & 2));
      if (!laser || !lbx) return void(done = true);
      void* me = mech3::mech_node_of(player);
      float a[3];
      mech3::node_world_cached(me, a);
      const zwep::Vec3f3 from{a[0], a[1] + 5.0f, a[2]}, dir{1, 0, 0}, still{0, 0, 0};
      // A beam: choice 0 starts both and keeps them, the release frees both; choice 2 the weplight alone.
      for (const int c : {0, 2}) {
        played.clear(), freed.clear();
        laser->fire.animation_choice = c;
        zwep::Beam* b = zwep::beam_make(laser, me, nullptr, &from, &dir, 1);
        zwep::beam_start(b);
        const std::vector<zwep::Handle> want = c == 0 ? std::vector<zwep::Handle>{laser->fire.animation, laser->fire.animation2}
                                                      : std::vector<zwep::Handle>{laser->fire.animation2};
        EXPECT(played == want);
        EXPECT((laser->fire.running[0] != nullptr) == (c == 0) && laser->fire.running[1] != nullptr);
        EXPECT(laser->fire.animation_choice == 0);  // back to both for the next
        const size_t kept = (laser->fire.running[0] != nullptr) + (laser->fire.running[1] != nullptr);
        zwep::beam_stop(b);
        zwep::beam_free(b);
        EXPECT(freed.size() == kept && !laser->fire.running[0] && !laser->fire.running[1]);
        std::printf("  beam, choice %d: %zu played, %zu freed\n", c, played.size(), freed.size());
      }
      // A shot: with the ANIMATION number set and a firepoint, both go on the firepoint; without one, both in the
      // world; choice 1 the flash alone. The firepoint is let go after each.
      static uint32_t zone;
      zone = zwep::query_zone();
      for (const int k : {0, 1, 2}) {
        played.clear(), attached.clear();
        lbx->fire.animation_choice = k == 2 ? 1 : 0;
        lbx->fire.attach_node = k == 0 ? me : nullptr;
        zwep::spawn(lbx, me, &zone, &from, &dir, &still, 0, nullptr);
        const std::vector<zwep::Handle> both{lbx->fire.animation, lbx->fire.animation2}, first{lbx->fire.animation};
        EXPECT(k == 0 ? attached == both && played.empty() : attached.empty() && played == (k == 1 ? both : first));
        EXPECT(lbx->fire.attach_node == nullptr && lbx->fire.animation_choice == 0);
        std::printf("  LB-X shot %d: %zu attached, %zu in the world\n", k, attached.size(), played.size());
      }
      // Without the number (the gauss rifle), a firepoint is not used: both in the world, the node let go.
      if (zwep::WeaponDef* gauss = zwep::find_def("GaussRifle")) {
        played.clear(), attached.clear();
        gauss->fire.animation_choice = 0;
        gauss->fire.attach_node = me;
        zwep::spawn(gauss, me, &zone, &from, &dir, &still, 0, nullptr);
        EXPECT(!(gauss->fire.flags & 2) && attached.empty() && played.size() == 2 && gauss->fire.attach_node == nullptr);
        std::printf("  gauss shot with a firepoint: %zu attached, %zu in the world\n", attached.size(), played.size());
      } else {
        EXPECT(false);
      }
      zwep::hit_hooks() = hk, zwep::run_hooks() = rk;
      done = true;
      return;
    }
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
    if (std::getenv("PM_SHOT_FLIGHT")) {  // a round's flight under the real loop (pm_shot_flight)
      char name[64] = "";
      ++since;
      g_shot_result = pm_shot_flight(player, since, &g_shot_travelled, &g_shot_expected, &g_shot_seen, name, sizeof name);
      if (g_shot_result != 0) done = true;
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
  if (quick && !std::getenv("PM_FALL") && !std::getenv("PM_BASE_LIGHT") && !std::getenv("PM_CRATERS") && !std::getenv("PM_MOTIONS") && !std::getenv("PM_HOSTILES") && !std::getenv("PM_TAG_MUZZLE") && !std::getenv("PM_WEAPON_LIGHTS") && !std::getenv("PM_ALLY_TURRET") && !std::getenv("PM_GONE") && !std::getenv("PM_SPLASH") && !std::getenv("PM_ANIMSAVE") && !std::getenv("PM_ANHUR") && !std::getenv("PM_DEADROOTS") && !std::getenv("PM_INOUT") && !std::getenv("PM_TELEPORT") && !std::getenv("PM_WATER") && !std::getenv("PM_HEIGHT") && !std::getenv("PM_SHOT_FLIGHT")) EXPECT(dead == 1 || g_refit_done);
  EXPECT(pm::zmod::clip3d_state().overruns == 0);
}

// Always on: the same fall with the camera by the cockpit and outside: entering the cloud
// tints the screen to its colour by the camera's height (docs/vehicles.md, The cloud's screen tint).
// pm-tests 37, Owen's invisible wall in c3m6: an Anhur shot down over the land bridge left its root's box spanning
// where it died and its falling wreck. Our aim ray had boxed it as it flew near; PM's never boxes a turret (its
// vt+0x48 is none, 0x00406ce0, as a target's), so nothing is left to stop a shot or a mech, bug fixes on or off.
TEST(pm_c3m6_dead_anhur_leaves_no_wall) {
#if !defined(_WIN32)
  for (const bool bugs : {true, false}) {
    std::fflush(nullptr);
    const pid_t child = check::fork_child();
    if (child == 0) {
      setenv("PM_ANHUR", "1", 1);
      if (!bugs) setenv("PM_BUGS_OFF", "1", 1);
      kill_run("2,1,6", true);
      const auto name = [](zcls::Node* n) { return n ? n->name() : "nothing"; };
      std::printf("  bugs %d: killed %d, boxed alive %d, dead %d; shot meets %s at %.1f m, walk %s at %.1f m; the deck gone at frame %d\n", bugs,
                  g_anhur.killed, g_anhur.boxed_alive, g_anhur.boxed_dead, name(g_anhur.shot), g_anhur.shot_at, name(g_anhur.walk), g_anhur.walk_at,
                  g_anhur.deck_gone);
      EXPECT(g_anhur.killed && !g_anhur.boxed_alive);  // never boxed by the aim ray, alive
      std::printf("  %d solid pieces, %d with nothing drawn; xslid01 (a target, never the reticle's) boxed alive %d, dead %d\n",
                  g_anhur.solid, g_anhur.unseen, g_anhur.bridge_boxed_alive, g_anhur.bridge_boxed_dead);
      EXPECT(!g_anhur.bridge_boxed_alive && !g_anhur.bridge_boxed_dead);
      EXPECT(g_anhur.solid > 0 && g_anhur.unseen == 0);  // the wreck stays solid by its polygons, all of them drawn
      const bool shot_box = g_anhur.shot && std::strcmp(g_anhur.shot->name(), "anhur04") == 0;
      const bool walk_box = g_anhur.walk && std::strcmp(g_anhur.walk->name(), "anhur04") == 0;
      EXPECT(!g_anhur.boxed_dead && !shot_box && !walk_box);
      std::fflush(nullptr);
      _exit(check::failures ? 1 : 0);
    }
    int status = 1;
    status = check::wait_child(child);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }
#endif
}

// Show InOut in a mission (kill_run 1,1,3): PM's walls on PM's models (pm_model), each model's material refs rebuilt, the second
// triangle of each pair back-faced as PM adds them; then nothing left.
TEST(pm_show_inout_builds_pm_models) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    setenv("PM_INOUT", "1", 1);
    kill_run("1,1,3", true);
    std::printf("  %d models, %d polygons (%d back-faced), %d material refs; taken down %d\n", g_inout.models,
                g_inout.polygons, g_inout.back, g_inout.refs, g_inout.freed);
    EXPECT(g_inout.built && g_inout.models > 0 && g_inout.polygons > 0 && g_inout.refs >= g_inout.models);
    EXPECT(g_inout.back == 0);  // PM's 0x0060b996 on: its back flag (== 0) never set, the side's [0x0062fa58] 0
    EXPECT(g_inout.freed);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// The editor's TELEPORT on PM's logic in a mission (kill_run 1,1,3): the world's places listed with the nav points,
// OK at the last place moves the player and writes PM's text (its build date, the places, no nav point) into the mod;
// Cancel moves nothing and writes nothing.
TEST(pm_teleport_moves_the_player_and_saves_pms_text) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    setenv("PM_TELEPORT", "1", 1);
    kill_run("1,1,3", true);
    std::printf("  listed: %s\n  %s: at %.1f %.1f %.1f (want %.1f %.1f %.1f); saved %s\n%s", g_tp.listed.c_str(),
                g_tp.place.c_str(), g_tp.at[0], g_tp.at[1], g_tp.at[2], g_tp.want[0], g_tp.want[1], g_tp.want[2],
                g_tp.saved.c_str(), g_tp.text.c_str());
    EXPECT(g_tp.listed.find("Current Location") != std::string::npos && !g_tp.place.empty());
    EXPECT(g_tp.moved && std::fabs(g_tp.at[0] - g_tp.want[0]) < 0.5f && std::fabs(g_tp.at[2] - g_tp.want[2]) < 0.5f);
    EXPECT(g_tp.saved.find("tp") != std::string::npos && g_tp.text.find("Last updated Nov 14 1999 at 14:36:03.") != std::string::npos &&
           g_tp.text.find("\"" + g_tp.place + "\"") != std::string::npos);
    // No nav point written: every listed name but Current Location and the saved places is a nav point.
    EXPECT(g_tp.nav_names.size() == 4);  // Able, Baker, Charlie, Start
    for (const mech3::TeleportSpot& s : g_tp.nav_names) EXPECT(g_tp.text.find("\"" + s.name + "\"") == std::string::npos);
    EXPECT(g_tp.cancel_stayed);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// PM_WATER_SURVEY=1: each of the ten missions' water (pm_water_survey), a line each; first the 26 mechs' heights on
// c1 mission 1 (PM_WATER_SURVEY=heights prints only those). Nothing is checked: a survey, for docs/pm-tests.md.
TEST(pm_water_survey_env) {
#if !defined(_WIN32)
  const char* mode = std::getenv("PM_WATER_SURVEY");
  if (!mode || !std::filesystem::exists(MW3PM_GAME_DIR)) return;
  auto child_run = [](const char* which) {
    std::fflush(nullptr);
    const pid_t child = check::fork_child();
    if (child == 0) {
      kill_run(which, true);
      std::fflush(nullptr);
      _exit(0);
    }
    int st = 0;
    st = check::wait_child(child);
  };
  if (std::strcmp(mode, "heights") == 0) {
    setenv("PM_HEIGHT", "1", 1);
    for (int m = 0; m < 26; ++m) {
      setenv("PM_MECH", std::to_string(m).c_str(), 1);
      child_run("0,1,1");
    }
    unsetenv("PM_HEIGHT"), unsetenv("PM_MECH");
    return;
  }
  struct M { const char* which; const char* label; };
  for (const M& m : {M{"0,1,1", "c1 mission 1"}, M{"0,1,2", "c1 mission 2"}, M{"0,1,5", "c1 mission 5"}, M{"1,1,3", "c2 mission 3"},
                     M{"1,1,4", "c2 mission 4"}, M{"1,1,8", "c2 mission 8"}, M{"2,1,6", "c3 mission 6"}, M{"2,1,7", "c3 mission 7"},
                     M{"3,1,9", "c4 mission 9"}, M{"3,1,10", "c4 mission 10"}}) {
    setenv("PM_WATER", m.label, 1);
    child_run(m.which);
  }
  unsetenv("PM_WATER");
#endif
}

// PM's OBJECT_MOTION (0x0051cd60) takes the falling node out of its ground query, MW3's the whole anim's node: c1m2's
// turrets stand on their own platform (g3, in the turret's tree), so at each part's queries from above it MW3's way
// finds no ground (its debris fell through the world: pm-tests 37's open item, part01 to y -160) and PM's the platform.
// Checked at the queries, not by where the parts end: their random directions follow the anim table's random index,
// which earlier tests in the process move, and a part thrown off the platform's edge falls either way.
TEST(pm_c1m2_turret_debris_lands_on_its_platform) {
#if !defined(_WIN32)
  for (const bool pm_way : {true, false}) {
    std::fflush(nullptr);
    const pid_t child = check::fork_child();
    if (child == 0) {
      setenv("PM_DEADROOTS", "1", 1);
      setenv("PM_MOTION_SURVEY", "1", 1);
      if (!pm_way) setenv("PM_MOTION_MW3", "1", 1);
      kill_run("0,1,2", true);
      std::printf("  %s's way: %d ground queries, %d with the moved node out; at %d of the turrets' MW3's way finds none and PM's the platform\n",
                  pm_way ? "PM" : "MW3", g_msurvey_calls, g_msurvey_self, g_msurvey_turret_differ);
      EXPECT(g_msurvey_calls > 50 && g_msurvey_turret_differ > 20);
      EXPECT(pm_way ? g_msurvey_self == g_msurvey_calls : g_msurvey_self == 0);
      std::fflush(nullptr);
      _exit(check::failures ? 1 : 0);
    }
    int status = 1;
    status = check::wait_child(child);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }
#endif
}

TEST(pm_cloud_screen_tint) {
#if !defined(_WIN32)
  for (const char* camera : {"1", "2"}) {
    std::fflush(nullptr);
    const pid_t child = check::fork_child();
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
    status = check::wait_child(child);
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
  const pid_t child = check::fork_child();
  if (child == 0) {
    setenv("PM_HOSTILES", "60", 1);
    setenv("PM_BEAMS_ONLY", "1", 1);
    kill_run("1,1,3", true);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// pm-tests item 15: TAG, added by its node (the mech's equipment 13), finds its part and firepoint, so its beam
// starts at the mech; without the weapon calls' node lookups it fired from the world's origin, out of sight.
TEST(pm_tag_fires_from_the_mech) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    setenv("PM_TAG_MUZZLE", "1", 1);
    kill_run("1,1,3", true);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// A round under the real loop (the frame counter moved by the program, not by hand): the player's first round-firing
// weapon in c1m1, one shot followed. It must cover most of its speed times its age (PM's first-frame hold is a
// millionth of a second once, 0x005a79a0); a counter stuck at the shot's stamp would hold it at the muzzle.
TEST(pm_c1m1_a_round_flies_under_the_real_loop) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    setenv("PM_SHOT_FLIGHT", "1", 1);
    kill_run("1,1,3", true);
    std::printf("  result %d: %d frames seen, travelled %.1f of speed x age %.1f\n", g_shot_result, g_shot_seen, g_shot_travelled, g_shot_expected);
    EXPECT(g_shot_result == 1);
    EXPECT(g_shot_seen >= 3);
    EXPECT(g_shot_expected > 10.0f);
    EXPECT(g_shot_travelled > 0.5f * g_shot_expected);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  const int status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// pm-tests, weapon lights (0x005a6700, 0x005a6c60, 0x005a27b0, 0x005a2a00): PM's FIRE blocks name a muzzle flash
// and a weapon light, and a shot or beam plays both (choice 0; k the k-th), a beam keeping both running until its
// release; the ANIMATION number puts a shot's on the firepoint. MW3 played the first name alone.
TEST(pm_weapon_lights_play_both) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    setenv("PM_WEAPON_LIGHTS", "1", 1);
    kill_run("1,1,3", true);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// At object detail low (2), the mission's load turns both off (0x00492830): no FIRE animations or effects are read.
TEST(pm_weapon_lights_off_at_low_detail) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    setenv("PM_WEAPON_LIGHTS", "1", 1);
    setenv("PM_OBJECT_LOD", "2", 1);
    kill_run("1,1,3", true);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// pm-tests item 21: the current weapon destroyed with the weapon after it in the HUD's list already gone. Pirate's
// Moon refreshes the list before taking its pick (0x004b6f70 calls 0x004480a0 first), so the pick leaves the
// destroyed one out; MW3 took it stale and stayed on a destroyed weapon. PM's own, so with the bug fixes off too.
TEST(pm_selection_leaves_a_destroyed_weapon) {
#if !defined(_WIN32)
  for (const bool off : {false, true}) {
    std::fflush(nullptr);
    const pid_t child = check::fork_child();
    if (child == 0) {
      setenv("PM_GONE", "1", 1);
      if (off) setenv("PM_BUGS_OFF", "1", 1);
      kill_run("1,1,3", true);
      std::fflush(nullptr);
      _exit(check::failures ? 1 : 0);
    }
    int status = 1;
    status = check::wait_child(child);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }
#endif
}

// pm-tests item 17: Pirate's Moon's ALLY turrets (turf_) never take the player's side as a target. MW3's pick ended
// on the player's own contact (0x004a7b40), which an ALLY turret kept once its enemy left, and fired on the player.
TEST(pm_c2m3_ally_turret_keeps_off_the_player) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    setenv("PM_ALLY_TURRET", "1", 1);
    kill_run("1,1,3", true);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// Always on: a mod's splash on a weapon without one (main's mission_c1m1_modded_splash_stays_within_the_list): a
// copy of AC5 given a 200 m IMPACT_PROXIMITY, hit where the query fills zClass's 32; the splash damages at most the
// 32 the list holds (the exe's buffer), with no read past it.
TEST(pm_c1m1_modded_splash_stays_within_the_list) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    setenv("PM_SPLASH", "1", 1);
    kill_run("0,1,1", true);
    std::printf("  AC5 with a 200 m splash, %d spots tried: the query counted %d, %d listed, %d damaged\n", g_splash.tried,
                g_splash.counted, g_splash.listed, g_splash.damaged);
    EXPECT(g_splash.made);
    EXPECT(g_splash.counted == 32 && g_splash.listed == 32);
    EXPECT(g_splash.damaged >= 1 && g_splash.damaged <= 32);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// Always on, pm-tests item 30: a saved world's animation object states read back (zeff_anim_save.cpp,
// load_objects): PM's 0x3c-byte layout, which our writer now matches with PM's (test_pm_anim_save.py), and
// MW3's 0x44-byte one our builds wrote before, told apart by the entry's size. Each puts the moved and hidden node
// back where it was, active.
TEST(pm_c1m2_anim_states_load_in_both_layouts) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    setenv("PM_ANIMSAVE", "1", 1);
    kill_run("0,1,2", true);
    for (int old = 0; old < 2; ++old)
      std::printf("  %s layout: an object found %d (%s), restored %d\n", old ? "MW3's (our earlier builds')" : "PM's",
                  g_animsave.found[old], g_animsave.name[old] ? g_animsave.name[old] : "-", g_animsave.restored[old]);
    EXPECT(g_animsave.found[0] && g_animsave.restored[0]);
    EXPECT(g_animsave.found[1] && g_animsave.restored[1]);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

TEST(pm_c2m3_craters_raise_no_spikes) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    setenv("PM_CRATERS", "60,600,-2500,1000,-300", 1);
    kill_run("1,1,3", true);
    EXPECT(g_crater_spikes == 0);
    EXPECT(g_craters_made > 20);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// Always on: c1m2's base lights (pm-tests item 6). start_hwlights calls turnon_baselight, whose loop's first range test
// (2 s in) lights starbase_light by PM's LIGHT_STATE (0x0051b910): the file's range 0..150, ambient 0.45, diffuse 1,
// white, static and not a light map, and PM's mixed colours from them. MW3's handler on PM's record set only the switch.
TEST(pm_c1m2_base_light_takes_its_state) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
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
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

// Always on: c1m2's motions from SI scripts (PM's event 12, 0x0051e2c0; pm-tests, the event sweep): by 300 frames the
// base crew of the_dude_runa06 and a07 run (torso and legs, 10 nodes). Routed to MW3's handler, PM's records moved none.
TEST(pm_c1m2_motions_play) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    setenv("PM_MOTIONS", "1", 1);
    kill_run("0,1,2", true);
    EXPECT(g_motions.nodes > 100 && g_motions.moved >= 10);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
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
    const pid_t child = check::fork_child();
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
    status = check::wait_child(child);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }
#endif
}

// pm-tests 35 (Owen's playtest: Epona Rhi stood still when ordered to repair), always on: in c1m1 under PM's AI the
// player calls the MFB and is repaired, then Epona, hurt, is ordered to repair (order 12). With bug fixes on she is
// taken at the starbase (the dock-parking fix) and at a mobile dock, walks in on PM's repair state (0x004e0790),
// goes down on the point and comes away repaired; with them off, at the starbase the exe's dock has let the bases
// stop and her order is refused (as the exe). Each in a child, as the other whole-mission runs.
TEST(pm_c1m1_lancemate_repairs_at_the_dock) {
#if !defined(_WIN32)
  struct Case { const char* name; const char* at; bool bugs; };
  const Case cases[] = {{"starbase", "4733,-4200", true}, {"mobile", "4041,-3388", true}, {"starbase, fixes off", "4733,-4200", false}};
  for (const Case& k : cases) {
    std::fflush(nullptr);
    const pid_t child = check::fork_child();
    if (child == 0) {
      setenv("PM_REPAIR", k.at, 1);
      setenv("PM_REPAIR_AFTER", "1", 1);
      if (!k.bugs) setenv("PM_BUGS_OFF", "1", 1);
      g_refit_done = !k.bugs;  // refused: no refit to wait for
      kill_run("0,1,1", true);
      std::printf("  %s: order %d, docked %d, done %d, legs %.3f -> %.3f\n", k.name, g_mate_order, g_mate_docked ? 1 : 0,
                  g_mate_done ? 1 : 0, g_mate_legs0, g_mate_legs1);
      if (k.bugs) {
        EXPECT(g_mate_order == 1 && g_mate_docked && g_mate_done);
        EXPECT(g_mate_legs1 > g_mate_legs0 + 0.5f);
      } else {
        EXPECT(g_mate_order == 0 && !g_mate_docked);
      }
      std::fflush(nullptr);
      _exit(check::failures ? 1 : 0);
    }
    int status = 1;
    status = check::wait_child(child);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }
#endif
}

// Always on: mission 1's static MFB: called from the start, no base moves, the mech at the point is refitted and the dock ends.
TEST(pm_starbase_refit) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    setenv("PM_MFB", "4476,-4486", 1);
    kill_run("0,1,1", true);
    std::fflush(nullptr);
    _exit(check::failures || !g_refit_done ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

TEST(pm_leg_down) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();  // a second mission in one process crashes (pm_c2m4's note)
  if (child == 0) {
    setenv("PM_LEG", "5", 1);
    kill_run("0,1,1", true);
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

static void werewolf_run();
TEST(pm_c2m4_destroy_valve_kills_the_werewolf) {
  // Not after the Shadow Cats in one process: what a mission leaves here crashes a second one (zcls's lists).
  if (std::getenv("MW3_CAMPAIGN") || std::getenv("PM_KILL") || !std::filesystem::exists(MW3PM_GAME_DIR)) return;
#if !defined(_WIN32)
  // In a child too, so the whole-mission runs after it (pm_ia_wave_config_past_...) don't load into its world.
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    werewolf_run();
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else
  werewolf_run();
#endif
}

static void werewolf_run() {
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

// pm-tests item 18: there is no cow level on c2m3, typed at the start and mid-mission: every primary and secondary
// objective done, the win; Exit Mission (Ctrl+Q) brings the debriefing as a win and CONTINUE briefs mission 4.
static void cow_run(int when);
TEST(pm_c2m3_no_cow_level) {
  if (!std::getenv("MW3_CAMPAIGN") || !std::filesystem::exists(MW3PM_GAME_DIR)) return;
  std::vector<int> at = {30, 2400};
  if (const char* e = std::getenv("PM_COW_AT")) at = {std::atoi(e)};  // one run, typed at that Core frame
  for (const int when : at) {
#if !defined(_WIN32)
    // Each in a child, as the other whole-mission runs: a world left on zClass's lists breaks the next load.
    std::fflush(nullptr);
    const pid_t child = check::fork_child();
    if (child == 0) {
      cow_run(when);
      std::fflush(nullptr);
      _exit(check::failures ? 1 : 0);
    }
    int status = 1;
    status = check::wait_child(child);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else
    cow_run(when);
#endif
  }
}

static void cow_run(int when) {
  {
    zgame::set_game(pm::table());
    zutl::mounts() = zutl::Mounts{};
    zutl::mount(MW3PM_GAME_DIR);
    const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_cow_write";
    std::filesystem::remove_all(wd);  // ours: made just below
    std::filesystem::create_directories(wd);
    zutl::set_write_dir(wd.c_str());
    mech3::reset_states();
    if (!zsnd::state().open) zsnd::state() = zsnd::State{};
    mech3::ai::anim_owner_free(), mech3::maps_free(), mech3::mission_maps() = mech3::MissionMaps{};
    zent::budget() = zent::Budget{}, mech3::program_vehicles_new_process(), zmod::puff_state().tick_node = nullptr;
    zint::scroll_list_init();  // as a new process starts (0x005962a0): the tests before this one leave their state
    mech3::player_controls_instance() = nullptr;
    mech3::missions() = mech3::Missions{};
    mech3::mech_types_forget();
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
    int open = -1, final_open = -1, next_op = 0, next_mission = 0;
    bool won = false, debriefed = false, failed = true, briefed = false;
    {
      mech3::GameEngine engine(p, "mech3.exe -nofmv");
      engine.quiet = true;
      mech3::Mech3App app(&engine);
      mech3::set_app(&app);
      EXPECT(app.init_instance());
      mech3::Pilot* pilot = mech3::create_pilot("Cow");
      EXPECT(pilot && pilot->select_campaign(0));
      if (!pilot) return;
      pilot->record_mission(true), pilot->record_mission(true);  // missions 1 and 2 won: mission 3, c2
      auto* g = dynamic_cast<mech3::CampaignGame*>(pilot->current);
      EXPECT(g && g->mission_number() == 3);
      pilot->save();
      pilot->current = g;
      pilot->save();
      int stage = -3, core = 0;
      auto count = [](const mech3::Objective& o, auto& self) -> int {
        int n = o.done ? 0 : 1;
        if (auto* gr = dynamic_cast<const mech3::ObjectiveGroup*>(&o))
          for (const auto& c : gr->items) n += self(*c, self);
        return n;
      };
      engine.on_present = [&](int) {
        zgame::State* now = app.current();
        if (stage == -3 && now == &mech3::states().main && mech3::main_menu()) {
          const_cast<mech3::MainMenu*>(mech3::main_menu())->buttons[1].on_left_up();  // CAMPAIGN
          stage = -2;
        } else if (stage == -2) {
          auto* ps = dynamic_cast<mech3::PilotState*>(now);
          if (!ps || !ps->screen) return;
          ps->screen->refresh(false, pilot);
          ps->screen->on_button(0x18, 0, 0);  // ACCEPT: its newest save
          stage = -1;
        } else if (stage == -1) {
          mech3::BriefingScreen* b = mech3::briefing_screen();
          if (!b || now != &mech3::states().load || !b->loaded) return;
          b->salvage_seen = 1;
          b->on_button(mech3::BriefingScreen::kBtnLaunch, 0, 0);
          stage = 0;
        } else if (stage == 0 && now == &mech3::states().core && ++core == when) {
          void* me = mech3::mech_mission_player();
          EXPECT(me && pm_cheat(me, "there is no cow level"));
          const mech3::Objectives& o = mech3::mission_objectives();
          open = (o.primary ? count(*o.primary, count) : 0) + (o.secondary ? count(*o.secondary, count) : 0);
          final_open = o.final ? count(*o.final, count) : 0;
          won = mech3::missions().ended == 1 && mech3::missions().aborting == 0;
          mech3::missions().on_command(0x79);  // Exit Mission
          stage = 1;
        } else if (stage == 1 && now == &mech3::states().end_win) {
          auto* d = dynamic_cast<mech3::DebriefScreen*>(engine.screen_of(mech3::states().end_win.screen_));
          if (!d) return;
          debriefed = true;
          failed = engine.mission_failed();
          d->on_button(mech3::DebriefScreen::kContinue, 0, 0);
          stage = 2;
        } else if (stage == 2 && now == &mech3::states().load) {
          mech3::BriefingScreen* b = mech3::briefing_screen();
          if (!b || !b->loaded) return;
          engine.game_campaign(&next_op, &next_mission);
          briefed = true;
          stage = 3;
        }
      };
      Pump pump;
      pump.limit = 60000;
      pump.stop = [&] { return stage == 3; };
      app.run(pump);
      mech3::missions().cleanup(false);
      app.stop_game();
      app.exit_instance();
      mech3::set_app(nullptr);
    }
    std::printf("  no cow level in c2m3 at frame %d: %d primary and secondary open, final %d open; won %d; debriefed %d "
                "(failed %d); next briefing %d: Op %d Mission %d\n",
                when, open, final_open, won, debriefed, failed, briefed, next_op, next_mission);
    EXPECT(open == 0 && final_open == 0 && won);
    EXPECT(debriefed && !failed);
    EXPECT(briefed && next_mission == 4);
    zgame::set_game(zgame::mw3_table());
    mech3::reset_states();
    mech3::options() = mech3::Options{};
  }
}

// pm-tests 32: an Instant Action wave whose slot asks for a loadout past its mech's last (a mod's table; no
// shipped one has such a slot). The stock table for the world, wave 1's first slot moved one past by hand.
static mech3::ActionGame* ia32_game_for(mech3::ActionGame& game, int world, mech3::GameEngine& engine) {
  game.set_world(world, 1);
  game.set_mode(2);  // the world's block of ia_progression.zrd
  if (game.progression.empty() || game.progression[0].empty()) return nullptr;
  const mech3::ConfigList* list = engine.mech_loadouts(game.progression[0][0].mech);
  if (!list) return nullptr;
  game.progression[0][0].config = static_cast<int32_t>(list->configs.size());
  return &game;
}

// The spawn (0x004a0f3c) gives the slot no loadout (0x0040c920 with 0), and 0x00414cb0 refuses none: the mech
// keeps the one it was made with, its first, and fights. It isn't counted destroyed: its objective stays open.
static void ia32_spawn_run();
TEST(pm_ia_wave_config_past_the_last_spawns_with_the_first_headless) {
  if (!std::filesystem::exists(MW3PM_GAME_DIR)) return;
#if !defined(_WIN32)
  // In a child, as the other whole-mission runs: a world left on zClass's lists breaks the next load.
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    ia32_spawn_run();
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else
  ia32_spawn_run();
#endif
}

// Pirate's Moon 0x00559310 is empty (`xor eax, eax; ret`): no model is freed at Instant Action's next wave (the hook
// where MW3 called 0x005392c0) or by the purge 0x00559320 (the lab's, a mission's end that keeps the world). The port
// ran MW3's, which releases every model nothing holds. A model held by nobody stays through all three.
TEST(pm_unused_models_stay_at_the_wave_and_the_purge) {
  zgame::set_game(pm::table());
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
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    ::zmod::ModelPool keep = std::move(::zmod::models());
    ::zmod::reset_pool();  // zModel's start (the engine's, in play)
    ::zmod::Model* m = ::zmod::model_alloc();
    EXPECT(m != nullptr);
    if (m) {
      const uint32_t type = 3;
      std::memcpy(m->header, &type, 4);  // a model, held by nobody (+0x0c 0)
      const auto kept = [&] { return m->count(0) == 3 && m->count(0xc) == 0; };
      mech3::mission_hooks().fn_5392c0();
      EXPECT(kept());  // the next wave
      engine.model_purge(1, 0);
      EXPECT(kept());  // the lab's purge
      mech3::mission_hooks().fn_5394c0();
      EXPECT(kept());  // a mission's end that keeps the world
      std::printf("  a model nobody holds, after the wave and both purges: %s\n", kept() ? "kept" : "freed");
      EXPECT(::zmod::model_release(m) == 0);
    }
    ::zmod::models() = std::move(keep);
  }
  zgame::set_game(zgame::mw3_table());
}

static void ia32_spawn_run() {
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_ia32_write";
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
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::ActionGame game(nullptr);
    EXPECT(ia32_game_for(game, 1, engine));  // c2: wave 1's two enemies, the first given one past its loadouts
    game.x6c = 1;
    game.slot.mech = 13, game.slot.skin = 2;
    const int32_t past = game.progression[0][0].mech, other = game.progression[0][1].mech;
    const mech3::Config* first = engine.mech_loadouts(past)->configs[0];
    const mech3::Config* asked = engine.mech_loadouts(other)->configs[static_cast<size_t>(game.progression[0][1].config)];
    mech3::GameEngine::Launch l;
    l.type = 2, l.index = 1, l.number = 1;
    l.action = &game;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0, made = -1, open = -1, materials_used = 0, materials_lost = 0;
    bool past_first = false, past_armed = false, other_asked = false, past_alive = false;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      if (++frames != 3 && frames != 600) return;
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      if (frames == 3) made = n;
      for (int32_t q = 0; q < n; ++q) {
        mech3::LaunchedMech m;
        mech3::describe_mech(ai[q].mech, &m);
        if (frames == 3 && m.roster == past) past_first = m.config == first, past_armed = m.weapons > 0;
        // (the slot's own copy of the loadout, 0x00434880: the same by name)
        if (frames == 3 && m.roster == other)
          other_asked = m.config && !std::strcmp(static_cast<const mech3::Config*>(m.config)->name, asked->name);
        if (frames == 600 && m.roster == past) past_alive = m.anim_state != 10;
      }
      if (frames == 600) {  // 20 s: wave 1's objective, a destroy for each enemy, none met
        // Playtest W8: Instant Action's material sweep (each frame, 0x00557990) has been round the models by
        // now, and no material a model's polygon lists is freed (the world went undrawn 5 s in).
        pm::zmod::ModelPool& pool = pm::zmod::models();
        for (int32_t i = 0; i < pool.capacity; ++i) {
          const pm::zmod::Model& md = pool.at(i);
          for (int32_t k = 0; md.polygons && k < md.count(0x10); ++k)
            for (int32_t j = 0; md.polygons[k].materials && j < md.polygons[k].material_count; ++j)
              if (const zmod::Material* mat = md.polygons[k].materials[j]) ++materials_used, materials_lost += (mat->flags & 0x20) != 0;
        }
        open = 0;
        const mech3::Objectives& o = mech3::mission_objectives();
        if (o.primary)
          for (const auto& g : o.primary->items)
            if (auto* gr = dynamic_cast<const mech3::ObjectiveGroup*>(g.get()))
              for (const auto& d : gr->items) open += d->kind == mech3::Objective::kDestroy && !d->done;
      }
    };
    Pump pump;
    pump.limit = 900;
    pump.stop = [&] { return frames >= 600; };
    app.run(pump);
    EXPECT(made == 2);
    EXPECT(materials_used > 1000 && materials_lost == 0);
    EXPECT(past_first && past_armed);
    EXPECT(other_asked);
    EXPECT(past_alive);
    EXPECT(open == 2);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
}

// The Action screen's wave rows (Pirate's Moon 0x0048f300, 0x0048f4d0): each enemy's loadouts listed, the
// wave's chosen, past the last the first; a training loadout as "Unknown"; not to step through.
TEST(pm_ia_wave_rows_show_the_waves_loadout_headless) {
  if (!std::filesystem::exists(MW3PM_GAME_DIR)) return;
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_ia32_rows_" + std::to_string(getpid());  // a new pilot
  std::filesystem::create_directories(wd);
  zutl::set_write_dir(wd.c_str());
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  mech3::action_setup() = mech3::ActionSetup{};
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
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    auto label = [](mech3::CycleButton& c) { return std::string(c.labels.empty() ? "" : c.labels[0]->text); };
    auto stock = [&](int32_t mech) {  // the mech's loadouts but training's
      std::vector<std::string> v;
      if (const mech3::ConfigList* l = engine.mech_loadouts(mech))
        for (const mech3::Config* c : l->configs)
          if (c && !c->training) v.emplace_back(c->name);
      return v;
    };
    auto items = [](mech3::CycleButton& c) {
      std::vector<std::string> v;
      for (int32_t i = 0; i < c.count(); ++i) v.emplace_back(c.item(i)->text);
      return v;
    };
    // A mech with a training loadout, for the third row.
    int32_t trainee = -1, training_at = -1;
    for (int32_t m = 0; m < 25 && trainee < 0; ++m)
      if (const mech3::ConfigList* l = engine.mech_loadouts(m))
        for (size_t j = 0; j < l->configs.size(); ++j)
          if (l->configs[j] && l->configs[j]->training) {
            trainee = m, training_at = static_cast<int32_t>(j);
            break;
          }
    int stage = 0;
    bool checked = false;
    engine.on_present = [&](int) {
      if (stage == 0 && app.current() == &mech3::states().main && mech3::main_menu()) {
        const_cast<mech3::MainMenu*>(mech3::main_menu())->buttons[0].on_left_up();  // INSTANT ACTION
        stage = 1;
      } else if (stage == 1) {
        auto* ps = dynamic_cast<mech3::PilotState*>(app.current());
        if (!ps || !ps->screen) return;
        ps->screen->new_bar.set_text("Waver");
        ps->screen->new_bar.on_return();
        ps->screen->on_button(0x18, 0, 0);  // ACCEPT
        stage = 2;
      } else if (stage == 2) {
        auto* st = dynamic_cast<mech3::GameMenuState*>(app.current());
        auto* a = st && st->kind == mech3::GameMenuState::kAction ? dynamic_cast<mech3::ActionScreen*>(engine.screen_of(st->screen_)) : nullptr;
        mech3::ActionGame* g = mech3::current_pilot() ? mech3::current_pilot()->action() : nullptr;
        if (!a || !g || !app.pending_list().empty()) return;
        g->progression.assign(1, {});
        auto& w = g->progression[0];
        int32_t m0 = -1;  // a mech with four loadouts or more but training's
        for (int32_t m = 0; m < 25 && m0 < 0; ++m)
          if (stock(m).size() >= 4) m0 = m;
        const int32_t m1 = m0;
        EXPECT(m0 >= 0);
        if (m0 < 0) return;
        w.push_back({m0, static_cast<int32_t>(engine.mech_loadouts(m0)->configs.size())});  // one past its last
        w.push_back({m1, 3});
        if (trainee >= 0) w.push_back({trainee, training_at});
        a->show_wave(0);
        const std::vector<std::string> s0 = stock(m0), s1 = stock(m1);
        EXPECT(items(a->enemy_configs[0]) == s0);
        EXPECT(!s0.empty() && label(a->enemy_configs[0]) == s0[0] && a->enemy_configs[0].index == 0);
        const mech3::Config* third = engine.mech_loadouts(m1)->configs[3];
        EXPECT(items(a->enemy_configs[1]) == s1 && label(a->enemy_configs[1]) == third->name);
        for (int i = 0; i < 2; ++i) {
          mech3::CycleButton& c = a->enemy_configs[i];
          EXPECT(!c.prev.enabled && !c.next.enabled && !c.click_steps);
        }
        if (trainee >= 0) {
          mech3::CycleButton& c = a->enemy_configs[2];
          EXPECT(c.count() == static_cast<int32_t>(stock(trainee).size()) + 1 && label(c) == "Unknown");
        } else {
          std::printf("pm_ia_wave_rows: no mech has a training loadout; that row not checked\n");
        }
        checked = true;
        stage = 3;
      }
    };
    Pump pump;
    pump.limit = 400;
    pump.stop = [&] { return stage == 3; };
    app.run(pump);
    EXPECT(checked);
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  mech3::action_setup() = mech3::ActionSetup{};
}

// The Action screen keeps its allies as mechs on Pirate's Moon (0x0048ded0, restored by mech by 0x0048f970 /
// 0x0048a2e0; MW3 kept the rows), and the Load State back from the lab puts the line-up's mechs there
// (0x00407fb0), so the screen offers the lab's choices next time: the player's mech, the Mobile Field Base
// kept as it was, each lancemate's mech in its row, none in the rest.
TEST(pm_ia_screen_keeps_mechs_and_offers_the_labs_lineup_headless) {
  if (!std::filesystem::exists(MW3PM_GAME_DIR)) return;
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_ia_lab_lineup_" + std::to_string(getpid());
  std::filesystem::create_directories(wd);
  zutl::set_write_dir(wd.c_str());
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  mech3::action_setup() = mech3::ActionSetup{};
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
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    int stage = 0;
    bool checked = false;
    engine.on_present = [&](int) {
      if (stage == 0 && app.current() == &mech3::states().main && mech3::main_menu()) {
        const_cast<mech3::MainMenu*>(mech3::main_menu())->buttons[0].on_left_up();  // INSTANT ACTION
        stage = 1;
      } else if (stage == 1) {
        auto* ps = dynamic_cast<mech3::PilotState*>(app.current());
        if (!ps || !ps->screen) return;
        ps->screen->new_bar.set_text("Lancer");
        ps->screen->new_bar.on_return();
        ps->screen->on_button(0x18, 0, 0);  // ACCEPT
        stage = 2;
      } else if (stage == 2) {
        auto* st = dynamic_cast<mech3::GameMenuState*>(app.current());
        auto* a = st && st->kind == mech3::GameMenuState::kAction ? dynamic_cast<mech3::ActionScreen*>(engine.screen_of(st->screen_)) : nullptr;
        mech3::ActionGame* g = mech3::current_pilot() ? mech3::current_pilot()->action() : nullptr;
        if (!a || !g || !app.pending_list().empty()) return;
        mech3::ActionSetup& k = mech3::action_setup();
        EXPECT(&k == &g->setup);
        // Kept: the rows' mechs, the Mobile Field Base's 15 (PM's "on"), a lancemate's none -1.
        EXPECT(a->allies[0].select_value(4) >= 0 && a->allies[1].select_value(15) == 1);
        EXPECT(a->allies[2].select_value(7) == 8 && a->allies[3].select_value(-1) == 0 && a->allies[4].select_value(0) == 1);
        a->keep_lineup();
        std::printf("  kept %d %d %d %d %d\n", k.g820[0], k.g820[1], k.g820[2], k.g820[3], k.g820[4]);
        EXPECT(k.g820[0] == 4 && k.g820[1] == 15 && k.g820[2] == 7 && k.g820[3] == -1 && k.g820[4] == 0);
        // Back from the lab: the player in mech 9, two lancemates (12 and 0), an enemy (3).
        g->count = 0;
        g->slot.mech = 9;
        g->add(12, 1, nullptr, 0), g->add(0, 1, nullptr, 0), g->add(3, 0, nullptr, 0);
        mech3::action_setup_from_lineup(*g);
        EXPECT(k.g820[0] == 9 && k.g820[1] == 15 && k.g820[2] == 12 && k.g820[3] == 0 && k.g820[4] == -1);
        a->reset();
        const int32_t shown[5] = {a->allies[0].selected_value(), a->allies[1].selected_value(), a->allies[2].selected_value(),
                                  a->allies[3].selected_value(), a->allies[4].selected_value()};
        std::printf("  offered %d %d %d %d %d\n", shown[0], shown[1], shown[2], shown[3], shown[4]);
        EXPECT(shown[0] == 9 && shown[1] == 15 && shown[2] == 12 && shown[3] == 0 && shown[4] == -1);
        checked = true;
        stage = 3;
      }
    };
    Pump pump;
    pump.limit = 400;
    pump.stop = [&] { return stage == 3; };
    app.run(pump);
    EXPECT(checked);
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  mech3::action_setup() = mech3::ActionSetup{};
}

// pm-tests item 23 on Pirate's Moon, as mission_c1m1_player_knocked_down_headless on MW3: the player knocked down
// in c1m1. (1) Hits of the weapon with the largest IMPACT_FORCE from 50 m ahead build the knock past the threshold
// (0x00417a80): Stagger, a fall. (2) Dropped from 150 m, a hard landing (0x004162c0: the fall's speed over 80 m/s)
// falls one time in five (rand > 0.8, as PM's 0x00416ec9); seed 3, whose first drop stands (0.57) and second falls
// (0.96), so the run stays short. Under seed 1, MW3's, PM's c1m1 draws in another order: the fall comes on drop 25.
static int knock_run(int cause) {
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_knock_write";
  std::filesystem::create_directories(wd);
  zutl::set_write_dir(wd.c_str());
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  // The landings' rand, as a new process with a fixed clock; PM_SEED=n another seed.
  crt::rand_fixed_seed() = std::getenv("PM_SEED") ? static_cast<uint32_t>(std::atoi(std::getenv("PM_SEED"))) : 3u;
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
  int fell_at = -1, drops = 0, f = 0;
  float force = 0;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    bool done = false;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core || done) return;
      ++f;
      void* me = mech3::mission_player_mech();
      if (!me || f < 30) return;
      mech3::LaunchedMech d;
      mech3::describe_mech(me, &d);
      if (d.anim_state >= 11 && d.anim_state <= 13) {
        fell_at = f, done = true;
        return;
      }
      float at[3];
      mech3::mech_world_position(me, at);
      if (cause == 1) {
        int32_t n = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
        void* def = mech3::chain_heaviest_impact();
        force = def ? mech3::weapon_def_impact(def) : 0;
        const float from[3] = {at[0], at[1] + 5.0f, at[2] - 50.0f};
        if (n && def && f % 2 == 0) mech3::mech_hit_from(me, def, from, mech3::mech_node(ai[0].mech), 0.01f);
        done = f > 600;
        return;
      }
      // A hard landing's fall is a 20% roll (the landing keeps the mech up at 0.8); which rolls the drops meet moves
      // with the start's timing (PM has no fade state: play starts two seconds sooner), so 24 drops, one every 250
      // frames (a 150 m drop lands in about 165), where six every 500 missed one time in four.
      if ((f - 30) % 250 == 0) {
        if (++drops > 24) {
          done = true;
          return;
        }
        const float up[3] = {at[0], at[1] + 150.0f, at[2]};
        mech3::move_mech(me, up, 0.0f);
      }
    };
    Pump pump;
    pump.limit = 6000;
    pump.stop = [&] { return done; };
    app.run(pump);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  std::printf("  pm cause %d: fell at frame %d (impact force %.1f, drops %d)\n", cause, fell_at, force, drops);
  std::fflush(nullptr);
  return fell_at;
}

// pm-tests 40 (Owen 2026-11-09: "PPCs don't seem to knock enemies back the way they knock him back"): PM's c1m1
// with PM's AI, its first enemy mech hit by the PPC from the player's place. One hit, then 8 s watched: the torso's
// spin (+0xd8, 0x00464af0: 15 deg x the force held to 1 x 0.5 x the impulse's length), the rocking it drives (+0xe4
// as the quaternion +0xf0, 0x00464e80) and the torso node as drawn (its 3x3 the pose x the shake, 0x004659c0;
// the angle between it and the same without the rocking is what a player sees). Then a PPC every frame for 10
// frames: the knock (+0x53c, 6 a second off, 0x005bce40) against the fall's threshold (0x00418850).
struct PpcJolt {
  float force = 0, spin = 0, spin_expect = 0, peak_deg = 0, peak_at = -1, drawn_peak_deg = 0, half_at = -1;
  float under1_at = -1, drawn_err = 0, threshold = 0, knock_max = 0;
  int falls = 0;
};

static PpcJolt ppc_run() {
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_ppc_write";
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
  PpcJolt r;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    bool done = false;
    int f = 0, barrage = 0, after = 0;
    float hit_time = -1;
    std::vector<std::pair<float, float>> curve;  // seconds after the hit, the rocking in degrees
    void* ppc = nullptr;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core || done) return;
      ++f;
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      void* me = mech3::mission_player_mech();
      if (!me || !n || f < 60) return;
      void* m = ai[0].mech;
      if (!ppc) ppc = mech3::weapon_def_named("PPC");
      mech3::MechJolt j;
      if (!ppc || !mech3::mech_torso_jolt(m, &j)) {
        done = f > 300;
        return;
      }
      const float now = mech3::mech_game_time();
      bool airborne = false;
      const float knock = mech3::mech_knock(m, &r.threshold, &airborne);
      mech3::LaunchedMech d;
      mech3::describe_mech(m, &d);
      const auto hit = [&] {
        float at[3];
        mech3::mech_world_position(me, at);
        at[1] += 5.0f;
        mech3::mech_hit_from(m, ppc, at, mech3::mech_node(me), 0.01f);
      };
      if (hit_time < 0) {
        hit();
        hit_time = now;
        r.force = mech3::weapon_def_impact(ppc);
        mech3::MechJolt now_j;
        if (mech3::mech_torso_jolt(m, &now_j)) r.spin = now_j.spin;
        return;
      }
      if (now - hit_time <= 8.0f) {
        const float t = now - hit_time;
        r.drawn_err = std::max(r.drawn_err, j.drawn_err);
        if (j.rock_deg > r.peak_deg) r.peak_deg = j.rock_deg, r.peak_at = t;
        r.drawn_peak_deg = std::max(r.drawn_peak_deg, j.drawn_deg);
        curve.push_back({t, j.rock_deg});
        return;
      }
      if (barrage < 10) {
        hit();
        ++barrage;
        r.knock_max = std::max(r.knock_max, mech3::mech_knock(m, &r.threshold, &airborne));
      }
      if (d.anim_state >= 11 && d.anim_state <= 13) ++r.falls;
      (void)knock;
      done = barrage >= 10 && ++after > 60;
    };
    Pump pump;
    pump.limit = 3000;
    pump.stop = [&] { return done; };
    app.run(pump);
    for (const auto& [t, deg] : curve) {
      if (t > r.peak_at && r.half_at < 0 && deg < r.peak_deg * 0.5f) r.half_at = t;
      if (t > r.peak_at && r.under1_at < 0 && deg < 1.0f) r.under1_at = t;
    }
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  const float fc = r.force > 1.0f ? 1.0f : r.force;
  r.spin_expect = 0.2617994f * fc * 0.5f * r.force;
  std::printf("  pm PPC: force %.2f; spin %.4f rad/s (expect %.4f); rocking peaks %.2f deg at %.2f s (drawn %.2f, "
              "3x3 off by %.2g), half by %.2f s, under 1 deg by %.2f s; 10 at once: knock %.2f vs the fall's %.2f, "
              "fallen %d frames\n",
              r.force, r.spin, r.spin_expect, r.peak_deg, r.peak_at, r.drawn_peak_deg, r.drawn_err, r.half_at,
              r.under1_at, r.knock_max, r.threshold, r.falls);
  std::fflush(nullptr);
  return r;
}

TEST(pm_c1m1_ppc_rocks_an_enemy_headless) {
  if (!std::filesystem::exists(MW3PM_GAME_DIR)) return;
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    const PpcJolt r = ppc_run();
    const bool ok = r.force > 0 && std::fabs(r.spin - r.spin_expect) < 1e-3f && r.peak_deg > 0.5f &&
                    std::fabs(r.drawn_peak_deg - r.peak_deg) < 0.05f * r.peak_deg + 0.01f && r.drawn_err < 1e-4f &&
                    r.under1_at > r.peak_at && r.knock_max > 0;
    _exit(check::failures || !ok ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

TEST(pm_c1m1_player_knocked_down_headless) {
  if (!std::filesystem::exists(MW3PM_GAME_DIR)) return;
  for (const int cause : {1, 2}) {
#if !defined(_WIN32)
    // Each in a child, as the other whole-mission runs: a world left on zClass's lists breaks the next load.
    std::fflush(nullptr);
    const pid_t child = check::fork_child();
    if (child == 0) {
      const int fell = knock_run(cause);
      _exit(check::failures || fell <= 0 ? 1 : 0);
    }
    int status = 1;
    status = check::wait_child(child);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else
    EXPECT(knock_run(cause) > 0);
#endif
  }
}

// As mission_c1m1_ai_mech_gets_up_at_once_headless on MW3: knocked down in PM's c1m1 (3, Mech::knocked_down), an AI
// mech's get-up is queued as its fall lands (its controls' vt+0xa4, 0x004e8e60), the player's after the 5 s lie.
// The AI's first mech is powered up first when it waits powered down (a powered-down mech never gets up). Each run
// checks its own time, from going down (anim state 12/13) to the get-up at the queue's front (5/6).
static float getup_run(int who) {
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_getup_write";
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
  float knocked = -1, down_at = -1, up_queued = -1;
  int f = 0;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    bool done = false;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core || done) return;
      ++f;
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      void* m = who ? mech3::mission_player_mech() : n ? ai[0].mech : nullptr;
      if (!m || f < 60) return;
      mech3::LaunchedMech d;
      mech3::describe_mech(m, &d);
      const float now = mech3::mech_game_time();
      if (knocked < 0) {
        if (d.powering_down || d.powered_down) {
          if (f == 60) mech3::mech_power(m, 0);
          done = f > 3000;
          return;
        }
        mech3::mech_knock_down(m, 3);
        knocked = now;
        return;
      }
      if (down_at < 0 && (d.anim_state == 12 || d.anim_state == 13)) down_at = now;
      if (down_at >= 0 && up_queued < 0 && (d.action == 5 || d.action == 6)) up_queued = now;
      done = up_queued >= 0 || d.anim_state == 10 || now - knocked > 20.0f;
    };
    Pump pump;
    pump.limit = 9000;
    pump.stop = [&] { return done; };
    app.run(pump);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  const float lie = down_at >= 0 && up_queued >= 0 ? up_queued - down_at : -1.0f;
  std::printf("  pm %s: down %.2f s after the knock, get-up queued %.2f s later\n", who ? "player" : "AI",
              down_at - knocked, lie);
  std::fflush(nullptr);
  return lie;
}

TEST(pm_c1m1_ai_mech_gets_up_at_once_headless) {
  if (!std::filesystem::exists(MW3PM_GAME_DIR)) return;
  for (const int who : {0, 1}) {
    const auto ok = [who](float lie) { return lie > 0.0f && (who ? lie > 4.5f : lie < 4.0f); };
#if !defined(_WIN32)
    std::fflush(nullptr);
    const pid_t child = check::fork_child();
    if (child == 0) {
      const float lie = getup_run(who);
      _exit(check::failures || !ok(lie) ? 1 : 0);
    }
    int status = 1;
    status = check::wait_child(child);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else
    EXPECT(ok(getup_run(who)));
#endif
  }
}

// pm-tests item 25 on Pirate's Moon, as mission_c1m2_lock_drops_on_a_new_target_headless on MW3: in c1m2, locked on
// an enemy mech (A), E to the next enemy (B), an LRM volley at once without aiming at B. The target change resets the
// reticle (PM 0x0041e7e0 -> 0x00442f10 -> 0x00445580, as MW3's), so the volley is not locked: its missiles carry no
// target to home on. E is PM's command 0x48 (MW3's 0x46).
static float pm_ground_under(float x, float top, float z) {  // test_program.cpp's ground_under
  zcls::Node* w = mech3::world_hooks().world;
  if (!w) return std::nanf("");
  float probe[3] = {x, top, z};
  static zcls::GroundHits hits;
  const int32_t mode = zmod::zones().mode;
  zmod::zones().mode = 0;
  zcls::ground_query(w, probe, 1, top, &hits);
  zmod::zones().mode = mode;
  float best = std::nanf("");
  for (int i = 0; i < hits.count; ++i) {
    if (hits.hit[i].material && hits.hit[i].material->soil == 1) continue;
    if (!(hits.hit[i].height <= best)) best = hits.hit[i].height;
  }
  return best;
}

static void lock_drop_run() {
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_lock_drop_write";
  std::filesystem::create_directories(wd);
  zutl::set_write_dir(wd.c_str());
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  struct Gpu : platform::Render3D {  // a device, so the HUD is made as in play
    int32_t create_texture(int, int, const uint8_t*, uint8_t) override { return 1; }
    void release_texture(int32_t) override {}
    void update_texture(int32_t, int, int, const uint8_t*) override {}
    void set_viewport(int, int, int, int) override {}
    void clear(bool, uint8_t, uint8_t, uint8_t) override {}
    void clear_depth_rect(int, int, int, int) override {}
    void begin_scene() override {}
    void end_scene() override {}
    void draw(platform::Primitive, const platform::TLVertex*, int, const platform::DrawState&) override {}
    void set_fog_color(uint8_t, uint8_t, uint8_t) override {}
    void present() override {}
  } gpu;
  struct ModeHost : ztwod::Host {
    mech3::GameEngine* e = nullptr;
    int32_t video_mode() override { return e->video_mode(); }
    const char* message_text(uint32_t id) override { return e->message(static_cast<int32_t>(id)); }
    uint32_t format_message(char* buf, uint32_t size, uint32_t id, va_list args) override {
      const std::string t = zutl::format_message_v(static_cast<int32_t>(id), args);
      std::snprintf(buf, size, "%s", t.c_str());
      return static_cast<uint32_t>(t.size());
    }
  } host;
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
  p.sound = &sound, p.files = &files, p.input = &keys, p.render = &gpu;
  int fired = -1, before = -1, after_e = -1, mechs = 0;
  bool changed = false, locked = true, shot_lock = true;
  const void* a_contact = nullptr;
  float a[3] = {}, b[3] = {}, first[3] = {}, last[3] = {};
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    host.e = &engine;
    ztwod::set_host(&host);
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;
    l.number = 2;  // c1m2
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int f = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++f;
      void* me = mech3::mission_player_mech();
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      mechs = n;
      if (!me || n < 2) return;
      bool has = false;
      if (f == 3) {  // 200 m from A, facing it, on ground about A's height (east of A, MW3's spot, is c1m2's canyon here)
        mech3::mech_world_position(ai[0].mech, a);
        float at[3] = {}, off = 1e30f;
        for (int k8 = 0; k8 < 8; ++k8) {
          const float t = 0.7853982f * static_cast<float>(k8);
          const float x = a[0] + 200.0f * std::cos(t), z = a[2] + 200.0f * std::sin(t);
          const float y = pm_ground_under(x, a[1] + 50.0f, z);
          if (std::fabs(y - a[1]) < off) off = std::fabs(y - a[1]), at[0] = x, at[1] = y, at[2] = z;
        }
        mech3::move_mech(me, at, std::atan2(at[0] - a[0], at[2] - a[2]));  // facing it: forward is -z
      }
      // PM's mech powers up for the first seconds (no fire, no commands): the sequence after it.
      constexpr int k = 200;
      if (f == k + 10) mech3::program_hud_target(ai[0].mech, 0), mech3::program_reticle_lock(3, 3.5f, &has);
      if (f == k + 39) before = mech3::program_reticle_lock(-1, 0, &has);
      if (f == k + 40) {
        a_contact = mech3::program_view_target_contact();
        mech3::mech_command(me, 0x48);  // E: the next enemy (PM's 0x004d6710)
        float d[3], p0[3];
        if (mech3::program_view_target_contact() != a_contact && mech3::program_view_target_delta(d)) {
          mech3::mech_world_position(me, p0);
          b[0] = p0[0] + d[0], b[1] = p0[1] + d[1], b[2] = p0[2] + d[2];
          changed = true;
        }
      }
      if (f == k + 41) after_e = mech3::program_reticle_lock(-1, 0, &has);
      if (f == k + 42) fired = mech3::mech_trigger_lock_on(me, &locked);
      if (f > k + 42) {
        bool l2 = false;
        float q[3];
        if (mech3::mech_lock_on_shot(me, q, &l2)) {
          if (f == k + 43) std::copy(q, q + 3, first), shot_lock = l2;
          std::copy(q, q + 3, last);
        }
      }
    };
    Pump pump;
    pump.stop = [&] { return f >= 320; };
    pump.limit = 4000;
    app.run(pump);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
    ztwod::set_host(nullptr);
  }
  std::printf("  pm: reticle %d before E, %d after; volley fired %d locked %d, its shot locked %d; from %.0f %.0f to %.0f %.0f "
              "(A %.0f %.0f, B %.0f %.0f)\n",
              before, after_e, fired, locked, shot_lock, first[0], first[2], last[0], last[2], a[0], a[2], b[0], b[2]);
  EXPECT(mechs >= 2 && changed);
  EXPECT(before == 3 && after_e != 3);
  EXPECT(fired == 1 && !locked && !shot_lock);
}

TEST(pm_c1m2_lock_drops_on_a_new_target_headless) {
  if (!std::filesystem::exists(MW3PM_GAME_DIR)) return;
#if !defined(_WIN32)
  // In a child, as the other whole-mission runs: a world left on zClass's lists breaks the next load.
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    lock_drop_run();
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  int status = 1;
  status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else
  lock_drop_run();
#endif
}

// pm-tests item 36 (Owen 2026-11-09): "operation cwal" typed into the cheat
// box took the mouse off the reticle. The box opened with Shift+Enter and
// every key of the code pressed through the keyboard's path, L let go after
// the Enter that shuts the box (typing rolls over), the mouse moved before
// and after: the view stays the reticle's (2), the mouse moves the reticle,
// not the torso. The fix is zInput's (main, zin_kbd.cpp; test_program's
// mission_c1m2_cheat_box_keys_stay_in_the_box_headless).
namespace {
uint32_t dik_of(char c) {
  static const char* rows[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
  static const uint32_t first[] = {0x02, 0x10, 0x1e, 0x2c};
  if (c == ' ') return 0x39;
  for (int r = 0; r < 4; ++r)
    if (const char* p = std::strchr(rows[r], c)) return first[r] + static_cast<uint32_t>(p - rows[r]);
  return 0;
}
// The SDL host's characters for the keys typed (SdlZinHost: the keymap's).
struct TypedHost : zin::Host {
  uint32_t map_virtual_key(uint32_t scan) override { return scan; }
  bool keyboard_state(uint8_t* st) override {
    std::memset(st, 0, 256);
    return true;
  }
  int32_t to_ascii(uint32_t, uint32_t scan, const uint8_t*, uint16_t* out) override {
    if (scan == 0x1c) return *out = 0x0d, 1;
    for (char c = ' '; c <= 'z'; ++c)
      if ((c == ' ' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')) && dik_of(c) == scan) return *out = static_cast<uint16_t>(c), 1;
    return 0;
  }
};
struct TypedKeys : platform::Input {
  std::vector<std::vector<platform::KeyEvent>> frames;  // one read's events each
  size_t at = 0;
  uint32_t t = 0;
  int32_t mouse_dx = 0;
  void read(std::vector<platform::KeyEvent> ev) { frames.push_back(std::move(ev)); }
  platform::KeyEvent ev(uint32_t dik, bool down) { return {dik, down ? 0x80u : 0u, ++t, t}; }
  void press(uint32_t dik) { read({ev(dik, true)}), read({ev(dik, false)}); }
  void type(const char* text) {
    for (const char* c = text; *c; ++c) press(dik_of(*c));
  }
  bool open(Device d) override { return d == kKeyboard || d == kMouse; }
  void close(Device) override {}
  int32_t acquire(Device) override { return 0; }
  int32_t unacquire(Device) override { return 0; }
  int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
    uint32_t n = 0;
    if (at < frames.size()) {
      for (const platform::KeyEvent& e : frames[at]) if (n < *count) out[n++] = e;
      ++at;
    }
    *count = n;
    return 0;
  }
  int32_t mouse_state(platform::MouseState* out) override {
    *out = platform::MouseState{};
    out->x = mouse_dx;
    return 0;
  }
  int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
};
}  // namespace

int pm_controls_view(void* mech);
bool pm_cheat_fast();
void pm_controls_aim(void* mech, float out[4]);
TEST(pm_cheat_box_keeps_the_reticle) {
  if (!std::filesystem::exists(MW3PM_GAME_DIR)) return;
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_cwal_write";
  std::filesystem::create_directories(wd);
  zutl::set_write_dir(wd.c_str());
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  TypedKeys keys;
  TypedHost host;
  zin::set_host(&host);
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
  l.index = 1, l.type = 1, l.number = 3;
  engine.set_launch(l);
  app.post_intro_ = &mech3::states().load;
  int frames = 0, last_view = -1;
  bool done = false;
  std::string seen;
  float moved_before = 0, moved_after = 0, twist_after = 0;
  engine.on_present = [&](int) {
    if (app.current() != &mech3::states().core) return;
    void* me = mech3::mission_player_mech();
    if (!me || pm_controls_view(me) < 0) return;
    ++frames;
    // The mouse right for 10 frames before the code, left for 10 after.
    const bool moving = (frames >= 10 && frames < 20) || (frames >= 80 && frames < 90);
    keys.mouse_dx = moving ? (frames < 50 ? 4 : -4) : 0;
    float a[4];
    pm_controls_aim(me, a);
    if (frames == 10) moved_before = -a[0];
    if (frames == 20) moved_before += a[0];
    if (frames == 80) moved_after = a[0];
    if (frames == 90) moved_after -= a[0];
    if (frames >= 80 && frames <= 95) twist_after = std::max(twist_after, std::fabs(a[2]));
    if (pm_controls_view(me) != last_view) {
      last_view = pm_controls_view(me);
      seen += " f" + std::to_string(frames) + ":" + std::to_string(last_view);
    }
    if (frames == 30) {  // Shift+Enter, the code, then L and Enter rolled over as typing does: L let go after the box shut
      keys.read({keys.ev(0x2a, true)});
      keys.read({keys.ev(0x1c, true), keys.ev(0x1c, false)});
      keys.read({keys.ev(0x2a, false)});
      keys.type("operation cwa");
      keys.read({keys.ev(0x26, true)});
      keys.read({keys.ev(0x1c, true)});
      keys.read({keys.ev(0x26, false), keys.ev(0x1c, false)});
    }
    if (frames == 120) done = true;
  };
  Pump pump;
  pump.limit = 60000;
  pump.stop = [&] { return done; };
  app.run(pump);
  zgame::set_game(zgame::mw3_table());
  zin::set_host(nullptr);
  std::printf("  views:%s; cwal %d; the reticle moved %.3f before, %.3f after; the twist after %.3f; frames %d\n", seen.c_str(),
              pm_cheat_fast() ? 1 : 0, moved_before, moved_after, twist_after, frames);
  EXPECT(pm_cheat_fast() && seen == " f1:2");  // the reticle's view throughout
  EXPECT(moved_before > 0.1f && moved_after > 0.1f && twist_after < 0.01f);
}
