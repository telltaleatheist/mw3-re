// The frame-rate fixes (zfix::Options::per_second, docs/fixes.md): the same
// second of play at 30 and 144 fps comes out the same, and the exe's
// per-frame path does not.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "check.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zmodel/gmod_puff.h"
#include "gamez/zrender/zrndr_blit.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/ztimer.h"
#include "mech3/hud.h"
#include "mech3/message_panel.h"
#include "mech3/mech_hud.h"
#include "mech3/lance_orders.h"
#include "mech3/mech.h"
#include "mech3/mech_control.h"
#include "mech3/mech_pocket.h"
#include "mech3/mech_skin.h"
#include "mech3/mech_skis.h"
#include "mech3/zai_attack.h"
#include "mech3/zai_brain.h"
#include "mech3/zrdraiv.h"
#include "mech3/zrdrvalve.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_move.h"
#include "mech3/mech_place.h"
#include "mech3/mission_world.h"
#include "mech3/weapon.h"
#include "gamez/zweapon/zweapon.h"

namespace {

float lava_heat(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  mech3::heat_calls().heat_option = [] { return true; };
  mech3::Foot feet[2];
  mech3::Leg legs[2];
  mech3::Mech m{};
  for (int i = 0; i < 2; ++i) legs[i].foot = &feet[i], m.legs[i] = &legs[i];
  feet[0].soil = 8;
  const int frames = static_cast<int>(fps / 2);  // half a second, under the overheat line
  for (int i = 0; i < frames; ++i) mech3::update_heat(m, 1.0f / fps);
  mech3::heat_calls().heat_option = nullptr;
  zfix::options().per_second = true;
  return m.heat;
}

float smoke_speed(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  zmod::PuffState& s = zmod::puff_state();
  s.particles.clear();
  s.wind = {};
  zmod::Particle p{};
  p.vel[0] = 10.0f, p.life = 100.0f, p.friction = 1.0f, p.wind_factor = 1.0f;
  s.particles.push_back(p);
  ztimer::state().dt = 1.0f / fps;
  for (int i = 0; i < static_cast<int>(fps); ++i) zmod::puff_tick();
  const float v = s.particles.front().vel[0];
  s.particles.clear();
  zfix::options().per_second = true;
  return v;
}

}  // namespace

TEST(fix_lava_heat_per_second) {
  EXPECT(std::fabs(lava_heat(30, true) - 0.75f) < 1e-3f);
  EXPECT(std::fabs(lava_heat(144, true) - 0.75f) < 1e-3f);
  EXPECT(std::fabs(lava_heat(30, false) - 0.75f) < 1e-3f);  // the exe at 30 fps
  EXPECT(lava_heat(144, false) > 3.5f * 0.75f / 2.0f);       // and nearly 5x at 144
}

TEST(fix_smoke_drag_per_second) {
  const float want = 10.0f * std::exp(-1.0f);
  EXPECT(std::fabs(smoke_speed(30, true) - want) < 0.05f);
  EXPECT(std::fabs(smoke_speed(144, true) - want) < 0.05f);
  EXPECT(smoke_speed(144, false) == 10.0f);  // the exe's table: no drag at all
}

// The Mad Cat's and Supernova's damage masks land on their damage textures
// (mcat_d01.., supern_d01..) with the fix, and two / one slots early without.
TEST(fix_damage_lists_line_up) {
  const std::string path = std::string(MW3_GAME_DIR) + "/zbd/reader.zbd";
  if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
    std::fclose(f);
  } else {
    std::printf("  (skipped: no %s)\n", path.c_str());
    return;
  }
  zrdr::init();
  EXPECT(zrdr::open_archive(path.c_str(), true));
  struct Case { const char* file; int first_d; };  // index of the first _d texture
  for (const Case& c : {Case{"dfn_madcat.zrd", 17}, Case{"dfn_supernova.zrd", 15}}) {
    for (bool fix : {true, false}) {
      zfix::options().bugs = fix;
      mech3::SkinSet* set = mech3::read_skins(zrdr::load(c.file));
      EXPECT(set && set->damage);
      if (!set || !set->damage) continue;
      const auto& prim = set->skins[0]->entries;
      const auto& dmg = set->damage->entries;
      EXPECT(std::strncmp(prim[c.first_d]->name, c.file[4] == 'm' ? "mcat_d01" : "supern_d01", 8) == 0);
      const bool lined = dmg.size() == prim.size() && dmg[c.first_d]->name && !dmg[c.first_d - 1]->name;
      EXPECT(lined == fix);
      delete set;
    }
  }
  zfix::options().bugs = true;
  zrdr::close_archives(true);
}

namespace {
// Counts what the sound code asks of the device; every call succeeds.
struct CountingOutput : platform::SoundOutput {
  int plays = 0;
  int32_t create() override { return 0; }
  void release_device() override {}
  int32_t set_cooperative_level(uint32_t, uint32_t) override { return 0; }
  int32_t get_caps(uint32_t*) override { return 0; }
  int32_t create_buffer(const platform::BufferDesc&, platform::SoundBuffer*) override { return 0; }
  int32_t duplicate_buffer(platform::SoundBuffer, platform::SoundBuffer*) override { return 0; }
  void release(platform::SoundBuffer) override {}
  int32_t get_format(platform::SoundBuffer, platform::WaveFormat*) override { return 0; }
  int32_t set_format(platform::SoundBuffer, const platform::WaveFormat&) override { return 0; }
  int32_t get_status(platform::SoundBuffer, uint32_t* s) override { return *s = 0, 0; }
  int32_t restore(platform::SoundBuffer) override { return 0; }
  int32_t lock(platform::SoundBuffer, uint32_t, uint32_t, void**, uint32_t*, void**, uint32_t*) override { return 0; }
  int32_t unlock(platform::SoundBuffer, void*, uint32_t, void*, uint32_t) override { return 0; }
  int32_t play(platform::SoundBuffer, uint32_t) override { return ++plays, 0; }
  int32_t stop(platform::SoundBuffer) override { return 0; }
  int32_t set_current_position(platform::SoundBuffer, uint32_t) override { return 0; }
  int32_t get_current_position(platform::SoundBuffer, uint32_t*, uint32_t*) override { return 0; }
  int32_t get_volume(platform::SoundBuffer, int32_t*) override { return 0; }
  int32_t set_volume(platform::SoundBuffer, int32_t) override { return 0; }
  int32_t set_pan(platform::SoundBuffer, int32_t) override { return 0; }
  int32_t get_frequency(platform::SoundBuffer, uint32_t*) override { return 0; }
  int32_t set_frequency(platform::SoundBuffer, uint32_t) override { return 0; }
};

// A fade from 0 to -30 dB, a second of 30 fps steps: ended or still running.
bool fade_ends(bool fix) {
  zfix::options().bugs = fix;
  CountingOutput out;
  zsnd::hooks().output = &out;
  zsnd::Sound s;
  s.voice.buffer = 1, s.voice.sound = &s;
  auto* f = new zsnd::Fade{-3000.0f, 0.0f, &s.voice, 0};
  zsnd::state().fades.push_back(f);
  for (int i = 0; i < 60; ++i) zsnd::fades_update(1.0f / 30.0f);
  const bool ended = zsnd::state().fades.empty();
  if (!ended) zsnd::state().fades.clear(), delete f;
  for (zsnd::Fade* x : zsnd::state().free_fades) delete x;
  zsnd::state().free_fades.clear();
  zsnd::hooks().output = nullptr;
  zfix::options().bugs = true;
  return ended;
}

// Pause kept one voice of a sound that also has a quieter duplicate: how many
// plays the unpause asks for.
int unpause_plays(bool fix) {
  zfix::options().bugs = fix;
  CountingOutput out;
  zsnd::hooks().output = &out;
  zsnd::Sound s;
  zsnd::Voice dup;
  s.voice.buffer = 1, s.voice.sound = &s;
  dup.buffer = 2, dup.sound = &s, dup.volume = -500;
  s.dups.push_back(&dup);
  zsnd::Snapshot snap;
  zsnd::Record first;
  first.master = zsnd::state().master();
  snap.push_back(first);
  zsnd::Record r;
  r.voice = &s.voice, r.sound = &s;
  snap.push_back(r);
  snap.push_back(r);  // the exe's snapshot repeats a record for a voice it skips
  zsnd::resume_snapshot(&snap);
  zsnd::hooks().output = nullptr;
  zfix::options().bugs = true;
  return out.plays;
}
}  // namespace

TEST(fix_sound_fade_to_a_partial_volume_ends) {
  EXPECT(fade_ends(true));
  EXPECT(!fade_ends(false));
}

TEST(fix_unpause_resumes_only_the_kept_voices) {
  EXPECT(unpause_plays(true) == 1);
  EXPECT(unpause_plays(false) == 4);  // both voices, once per record
}

// The wingmen's six ammo groups keep their names, each line playable twice.
TEST(fix_ammo_groups_keep_their_names) {
  const std::string path = std::string(MW3_GAME_DIR) + "/zbd/reader.zbd";
  if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
    std::fclose(f);
  } else {
    std::printf("  (skipped: no %s)\n", path.c_str());
    return;
  }
  zrdr::init();
  EXPECT(zrdr::open_archive(path.c_str(), true));
  zrdr::Node* groups = zrdr::find(zrdr::load("sounds.zrd"), "SOUND_GROUPS");
  EXPECT(groups != nullptr);
  int found = 0, named_play_count = 0;
  for (bool fix : {true, false}) {
    zfix::options().bugs = fix;
    for (int32_t i = 1; groups && i < groups->list[0].i; ++i) {
      zsnd::Group* g = zsnd::read_group(&groups->list[i], nullptr);
      if (!g) continue;
      if (g->name && std::strcmp(g->name, "dom_low_ammo") == 0) {
        ++found;
        EXPECT(fix && g->entries.size() == 4 && g->entries[0].count == 2 && g->entries[3].count == 2);
      }
      if (g->name && std::strcmp(g->name, "PLAY_COUNT") == 0) ++named_play_count;
      delete g;
    }
  }
  zfix::options().bugs = true;
  EXPECT(found == 1);
  EXPECT(named_play_count == 6);
  zrdr::close_archives(true);
}

// The movement steps (mech_move, mech_skis, mech_collide).
namespace {

// Animations without motion data: no root motion.
struct StillAnim : mech3::MechAnim {
  void velocity(mech3::Vec3*) override {}
};
struct StillAction : mech3::MechAction {
  void hip(float* p, float q[4]) override {
    p[0] = p[1] = p[2] = 0.0f;
    q[0] = 1.0f, q[1] = q[2] = q[3] = 0.0f;
  }
};

mech3::Mech* bare_mech() {
  static mech3::Mech m;
  static mech3::MechControls c;
  static mech3::Torso torso;
  static mech3::Head head;
  static mech3::Hip hip;
  static mech3::Leg legs[2];
  static mech3::Foot feet[2];
  static mech3::MechData pilot;
  static StillAnim anims[13];
  m = mech3::Mech{};
  c = mech3::MechControls{};
  c.owner = &m;
  m.controls = &c;
  m.torso = &torso, m.head = &head, m.hip = &hip, m.pilot = &pilot;
  for (int i = 0; i < 2; ++i) feet[i] = mech3::Foot{}, legs[i].foot = &feet[i], m.legs[i] = &legs[i];
  for (int i = 0; i < 13; ++i) anims[i] = StillAnim{}, anims[i].loop_time = 1.0f, m.anims[i] = &anims[i];
  // No parts to pose.
  mech3::move_calls().jet_pose = [](mech3::Mech&) {};
  mech3::move_calls().action_pose = [](mech3::Mech&, mech3::MechAction&, const mech3::Vec3&, const float*) {};
  m.remote = true;
  m.height = 10.0f;
  m.heat = 0.0f;
  return &m;
}

float carried_momentum(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  mech3::Mech& m = *bare_mech();
  static StillAction act;
  act = StillAction{};
  act.loop_time = 10.0f;
  m.action_objects[5] = &act;
  m.actions.push_back(5);
  m.anim_state = 12;
  m.vec_3c = {10.0f, 0.0f, 0.0f};
  mech3::move_calls().skis_update = [](mech3::Mech&, float) { return false; };
  for (int i = 0; i < static_cast<int>(fps); ++i) m.update_action(1.0f / fps);
  mech3::move_calls().skis_update = nullptr;
  zfix::options().per_second = true;
  return m.momentum.x;
}

float blend_after_half_second(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  mech3::Mech& m = *bare_mech();
  m.anim_state = 5;
  m.detail = 0x10;
  static float blend;
  blend = -1.0f;
  mech3::move_calls().skis_update = [](mech3::Mech&, float) { return false; };
  mech3::move_calls().gait_pose = [](mech3::Mech&, mech3::MechAnim&, float b) { blend = b; };
  const float dt = 1.0f / fps;
  for (int i = 0; i <= static_cast<int>(fps / 2); ++i) {
    m.move(dt, m.anims[4], 0.5f);
    ++m.gait_frames;
    m.gait_steps = m.gait_steps + zfix::frame_steps(dt);
  }
  mech3::move_calls() = mech3::MoveCalls{};
  zfix::options().per_second = true;
  return blend;
}

float jet_climb(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  mech3::Mech& m = *bare_mech();
  m.equipment[11] = 1;
  m.controls->jump = 1.0f;
  const float g = mech3::environment().gravity;
  mech3::environment().gravity = 1000.0f;  // to reach the cap
  float sum = 0;
  for (int i = 0; i < static_cast<int>(fps); ++i) {
    m.impulse = {0, 0, 0};
    m.jet_fuel = 1.0f;
    m.jump_jets(1.0f / fps);
    sum += m.impulse.y;
  }
  mech3::environment().gravity = g;
  zfix::options().per_second = true;
  return sum;
}

float touch_damage(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  mech3::Mech& m = *bare_mech();
  m.collider.touches.push_back(mech3::CollisionHit{});
  static float sum;
  sum = 0;
  mech3::collide_calls().touch = [](mech3::Handle, const mech3::Vec3*, const mech3::CollisionHit&, float d) {
    sum += d;
    return 0.0f;
  };
  m.frame_dt = 1.0f / fps;
  for (int i = 0; i < static_cast<int>(fps); ++i) m.touch_response();
  mech3::collide_calls() = mech3::CollideCalls{};
  zfix::options().per_second = true;
  return sum;
}

float wading_speed(float depth, bool fixed) {
  zfix::options().deep_water = fixed;
  mech3::Mech& m = *bare_mech();
  m.foot(0)->water_depth = depth, m.foot(1)->water_depth = depth;
  m.anim_state = 1;
  m.detail = 0x10;
  mech3::move_calls().move = [](mech3::Mech&, float, mech3::MechAnim*, float) {};
  m.select_gait(1.0f / 30);
  mech3::move_calls() = mech3::MoveCalls{};
  zfix::options().deep_water = true;
  return m.speed_target;
}

// Half a second holding the throttle-up key: the target's climb.
float throttle_after_half_second(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  static mech3::Mech m;
  static mech3::PlayerControls c;
  m = mech3::Mech{};
  c = mech3::PlayerControls{};
  c.owner = &m;
  m.controls = &c;
  mech3::controls_calls().command = [](int cmd) { return cmd == 0x18 ? 2u : 0u; };  // CMD_THROTTLE_UP held
  mech3::controls_calls().joy_present = [] { return false; };
  for (int i = 0; i < static_cast<int>(fps / 2); ++i) c.update(1.0f / fps);
  mech3::controls_calls() = mech3::ControlsCalls{};
  zfix::options().per_second = true;
  return c.throttle_target;
}

}  // namespace

TEST(fix_throttle_keys_per_second) {
  EXPECT(std::fabs(throttle_after_half_second(30, true) - 0.375f) < 1e-5f);  // 0.025 a frame at 30 fps
  EXPECT(std::fabs(throttle_after_half_second(144, true) - 0.375f) < 1e-3f);
  EXPECT(throttle_after_half_second(30, false) == throttle_after_half_second(30, true));
  EXPECT(throttle_after_half_second(144, false) == 1.0f);  // the exe: 0.025 a frame, full in 0.28 s
}

TEST(fix_action_momentum_per_second) {
  const float want = 10.0f * std::pow(0.6f, 30.0f);
  EXPECT(std::fabs(carried_momentum(30, true) - want) < want * 1e-3f);
  EXPECT(std::fabs(carried_momentum(144, true) - want) < want * 1e-2f);
  EXPECT(carried_momentum(30, false) == carried_momentum(30, true));  // the exe's step at 30 fps
  EXPECT(carried_momentum(144, false) < want * 1e-6f);               // gone far sooner at 144
}

TEST(fix_gait_blend_in_per_second) {
  EXPECT(std::fabs(blend_after_half_second(30, true) - 0.75f) < 1e-5f);
  EXPECT(std::fabs(blend_after_half_second(144, true) - 0.75f) < 0.02f);
  EXPECT(blend_after_half_second(30, false) == blend_after_half_second(30, true));
  EXPECT(blend_after_half_second(144, false) == 1.0f);  // the exe: done after 20 frames, 0.14 s
}

TEST(fix_jet_cap_per_second) {
  EXPECT(std::fabs(jet_climb(30, true) - 180.0f) < 0.1f);  // 6 a frame at 30 fps
  EXPECT(std::fabs(jet_climb(144, true) - 180.0f) < 0.1f);
  EXPECT(jet_climb(30, false) == jet_climb(30, true));
  EXPECT(jet_climb(144, false) > 800.0f);  // the exe: 6 a frame at any rate
}

TEST(fix_touch_damage_per_second) {
  EXPECT(std::fabs(touch_damage(30, true) - 600.0f) < 0.1f);
  EXPECT(std::fabs(touch_damage(144, true) - 600.0f) < 0.1f);
  EXPECT(touch_damage(30, false) == touch_damage(30, true));
  EXPECT(touch_damage(144, false) == 20.0f * 144);
}

TEST(fix_deep_water_curve) {
  // Fixed: full speed at a tenth of the height down to 0.7 at full depth, no step.
  EXPECT(std::fabs(wading_speed(1.0f, true) - 1.0f) < 1e-5f);
  EXPECT(std::fabs(wading_speed(5.5f, true) - 0.85f) < 1e-5f);
  EXPECT(std::fabs(wading_speed(9.99f, true) - 0.7f) < 1e-3f);
  EXPECT(std::fabs(wading_speed(12.0f, true) - 0.7f) < 1e-5f);
  // The exe: near a standstill just under full depth, then back to 0.7.
  EXPECT(wading_speed(9.99f, false) < 0.01f);
  EXPECT(std::fabs(wading_speed(12.0f, false) - 0.7f) < 1e-5f);
}

// A foot just behind its ski's middle: with the fix the middle probe is its
// contact (and its water); the exe's unset width picks the back probe.
// Delayed updates (fixes.md, ai.md section 13): a lone timer whose update
// starts another keeps it with the fix; the exe loses it.
TEST(fix_lone_delayed_update_keeps_the_next) {
  using namespace mech3::ai;
  for (const bool bugs : {false, true}) {
    zfix::options().bugs = bugs;
    valves_end();
    Valve* a = valve_get("a");
    Valve* b = valve_get("b");
    Valve* c = valve_get("c");
    a->add(new DelayUpdateAction(b, 1.0f));  // the game time is 0: due at 1
    b->add(new DelayUpdateAction(c, 1.0f));
    a->set(1);
    valves_tick(2.0f);  // b set; its update starts c's timer while the lone one runs
    valves_tick(3.0f);
    EXPECT(b->get() == 1);
    EXPECT(c->get() == (bugs ? 1 : 0));
    valves_end();
  }
  zfix::options().bugs = true;
}

// Valves that update themselves (FIX (bug), fixes.md, ai.md section 8): an
// update of itself, or a loop of updates, recursed until the stack ran out
// in the exe; here a valve firing inside its own reactions is skipped, so
// each reaction round the loop runs once per set.
namespace {
struct CountFires : mech3::ai::Reaction {
  int* n;
  explicit CountFires(int* c) : n(c) {}
  void fire(mech3::ai::Valve&) override { ++*n; }
};
}  // namespace

TEST(fix_valves_that_update_themselves_fire_once) {
  using namespace mech3::ai;
  valves_end();
  valve_reentries() = 0;
  int na = 0, nb = 0, nc = 0;
  Valve* a = valve_get("a");
  Valve* b = valve_get("b");
  Valve* c = valve_get("c");
  a->add(new UpdateAction(a));  // itself
  a->add(new CountFires(&na));
  EXPECT(a->set(1) == 0 && na == 1 && !a->firing);
  b->add(new UpdateAction(c));  // b -> c -> a -> b
  c->add(new UpdateAction(a));
  a->add(new UpdateAction(b));
  b->add(new CountFires(&nb));
  c->add(new CountFires(&nc));
  na = 0;
  EXPECT(b->set(1) == 0 && na == 1 && nb == 1 && nc == 1);
  EXPECT(!a->firing && !b->firing && !c->firing && valve_reentries() == 3);
  EXPECT(c->set(1) == 0 && na == 2 && nb == 2 && nc == 2);  // again from another valve
  valves_end();
  valve_reentries() = 0;
}

TEST(fix_foot_uses_its_middle_probe) {
  mech3::Skis s{};
  for (int i = 0; i < 6; ++i) s.local[i].z = (i % 3 - 1) * 6.0f;  // front -6, middle 0, back 6
  s.in_water[1] = 1;
  float surface = 0.0f;
  mech3::GroundHit* hit = nullptr;
  s.height_at(0, 1.0f, &hit);
  EXPECT(hit == &s.contact[1] && s.water_at(0, 1.0f, &surface));
  zfix::options().bugs = false;
  s.height_at(0, 1.0f, &hit);
  EXPECT(hit == &s.contact[2] && !s.water_at(0, 1.0f, &surface));
  zfix::options().bugs = true;
}

namespace {
// One aiv.zrd record of a mission archive, parsed with the fixes on or off.
// Returns the record (owned by the caller) or null without the game files.
mech3::ai::AivRecord* aiv_record(const char* archive, const char* name, bool fix) {
  const std::string path = std::string(MW3_GAME_DIR) + "/zbd/" + archive;
  if (std::FILE* f = std::fopen(path.c_str(), "rb")) std::fclose(f);
  else return nullptr;
  zfix::options().bugs = fix;
  zrdr::init();
  zrdr::open_archive(path.c_str(), true);
  auto* r = new mech3::ai::AivRecord;
  r->parse(zrdr::find(zrdr::load("aiv.zrd"), name), name);
  zfix::options().bugs = true;
  return r;
}
void close_aiv(mech3::ai::AivRecord* r) {
  delete r;
  mech3::ai::aiv_file_reset();
  zrdr::close_archives(true);
}
}  // namespace

// Alan's two misspelt records get his radio voice; the exe gives them none.
TEST(fix_alan_misspelt_gets_his_voice) {
  for (bool fix : {true, false}) {
    mech3::ai::AivRecord* r = aiv_record("c1/readeria1.zbd", "Alan_Mattilla", fix);
    if (!r) return std::printf("  (skipped: no game files)\n"), void();
    EXPECT((r->sound_tag && std::strcmp(r->sound_tag, "alan") == 0) == fix);
    close_aiv(r);
  }
}

// Enemy_12's loose orders on c4 are read; the exe leaves it with none.
TEST(fix_loose_orders_are_read) {
  for (bool fix : {true, false}) {
    mech3::ai::AivRecord* r = aiv_record("c4/readeria1.zbd", "Enemy_12", fix);
    if (!r) return std::printf("  (skipped: no game files)\n"), void();
    EXPECT((r->vlist && !r->vlist->empty()) == fix);
    close_aiv(r);
  }
}

// ZAPS_HUD's second value is the static's hold time (mech3/weapon.cpp); the
// exe reads the first value twice (0x004af71b).
TEST(fix_zaps_hud_reads_its_hold) {
  const std::string path = std::string(MW3_GAME_DIR) + "/zbd/reader.zbd";
  if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
    std::fclose(f);
  } else {
    std::printf("  (skipped: no %s)\n", path.c_str());
    return;
  }
  zrdr::init();
  EXPECT(zrdr::open_archive(path.c_str(), true));
  zrdr::Node* b = zrdr::find(zrdr::load("weapons.zrd"), "BALLISTICS");
  EXPECT(b != nullptr);
  zrdr::Node* erppc = nullptr;
  for (int32_t i = 1; b && i + 1 < zrdr::list_count(b); i += 2)
    if (b->list[i].type == zrdr::String && std::strcmp(b->list[i].s, "ERPPC") == 0) erppc = &b->list[i + 1];
  EXPECT(erppc != nullptr);
  for (bool fix : {true, false}) {
    zfix::options().bugs = fix;
    zwep::WeaponDef d;
    d.name = "ERPPC";
    d.period = 1.0f;
    if (erppc) mech3::make_weapon_type(erppc, &d);
    EXPECT(d.type && d.type->zaps_hud == 0.5f && d.type->zaps_hud_2 == (fix ? 2.0f : 0.5f));
    mech3::free_weapon_type(&d);
  }
  zfix::options().bugs = true;
  zrdr::close_archives(true);
}

namespace {

// TAG's recharge (U_RECHARGE_TIME 60: 166.7 a second) over `seconds` at `fps`,
// hot by the BeamWeapon's factor min(1, max(0.5, 0.5 / heat)).
int32_t tag_recharge(float fps, float heat, float seconds, bool per_second) {
  zfix::options().per_second = per_second;
  const float rate = 10000.0f * 1.0f / 60.0f;
  const float dt = 1.0f / fps;
  float f = heat > 0.5f ? 0.5f / heat : 1.0f;
  if (f < 0.5f) f = 0.5f;
  float carry = 0;
  int32_t n = 0;
  for (int i = 0; i < static_cast<int>(fps * seconds + 0.5f); ++i) n += mech3::beam_recharge(rate, f * dt, dt, &carry);
  zfix::options().per_second = true;
  return n;
}

}  // namespace

TEST(fix_beam_recharge_per_second) {
  EXPECT(tag_recharge(30, 0, 10, false) == 1500);   // the exe at 30 fps: 5 a frame
  EXPECT(tag_recharge(30, 0, 10, true) == 1500);    // unchanged there
  EXPECT(tag_recharge(60, 0, 10, false) == 1200);   // the exe: 2 a frame
  EXPECT(tag_recharge(144, 0, 10, false) == 1440);  // 1 a frame
  EXPECT(tag_recharge(200, 0, 10, false) == 0);     // never refills
  EXPECT(tag_recharge(144, 0.6f, 10, false) == 0);  // nor at 144 fps once a little hot
  for (float fps : {60.0f, 144.0f, 200.0f, 500.0f}) {
    EXPECT(std::abs(tag_recharge(fps, 0, 10, true) - 1500) <= 1);
    EXPECT(std::abs(tag_recharge(fps, 1.0f, 10, true) - tag_recharge(30, 1.0f, 10, true)) <= 1);
  }
}

// The HUD's +0x22e3c (hud.md section 3): with a video mode hud.zrd has no
// block for, the set-up reads whatever the block slot held. The fix clears it
// in the constructor; here the HUD is built over garbage and the set-up runs
// with no block (the exe would read through 0xcdcdcdcd).
TEST(fix_hud_block_cleared) {
  zfix::options().bugs = true;
  void* storage = ::operator new(sizeof(mech3::Hud));
  std::memset(storage, 0xcd, sizeof(mech3::Hud));
  auto* hud = new (storage) mech3::Hud(nullptr);  // the default hooks: no hud.zrd, video mode 0
  EXPECT(hud->block == nullptr);
  hud->~Hud();
  ::operator delete(storage);
}


// FIX (A): an alpha blend into the program's keyed layer records its coverage
// instead of mixing with the key; over a drawn pixel it is the plain blend.
TEST(fix_layer_blend_keeps_its_alpha) {
  const uint16_t key = 0xf81f, grey = 0x8410, red = 0xf800;
  uint16_t layer[4] = {key, key, red, key};
  uint8_t cover_a[4] = {};
  uint16_t cover_c[4] = {};
  zrndr::Target2D& t = zrndr::target2d();
  const zrndr::Target2D saved = t;
  t.base = reinterpret_cast<uint8_t*>(layer), t.width = 4, t.height = 1, t.pitch = 8, t.format = 6;
  uint16_t px[4] = {grey, grey, grey, grey};
  uint8_t al[4] = {0x80, 0x80, 0x80, 0x02};
  zimg::Texture img{};
  img.width = 4, img.height = 1, img.pixels = px, img.alpha = al;
  zrndr::layer_cover() = {cover_a, cover_c, key};
  zrndr::blit16(&img, 0, 0, 0, nullptr);
  EXPECT(layer[0] == grey && cover_a[0] == 0x80 && cover_c[0] == grey);  // the grey, half see-through
  EXPECT(layer[2] != red && layer[2] != grey && cover_a[2] == 0);         // blended over the red: opaque
  EXPECT(layer[3] == key && cover_a[3] == 0);                            // below the threshold: untouched
  zrndr::blit16(&img, 0, 0, 0, nullptr);                                  // again over the half-covered pixel
  EXPECT(cover_a[0] == 0x80 + 0x80 * 0x7f / 255 && cover_c[0] == layer[0]);
  zrndr::layer_cover() = {};
  uint16_t plain[1] = {key};
  t.base = reinterpret_cast<uint8_t*>(plain), t.width = 1, t.pitch = 2;
  img.width = 1;
  zrndr::blit16(&img, 0, 0, 0, nullptr);
  EXPECT(plain[0] != grey && plain[0] != key);  // the original path: mixed with the key
  t = saved;
}

// FIX (bug): the lead point (0x0041d940) with no group selected reads the
// group's weapon through a null pointer in the original; here none, false.
TEST(fix_lead_point_without_a_group) {
  struct NoGroup : mech3::HudHooks {
    int32_t wm_group_index(void*) override { return -1; }
    void* mech_weapons(mech3::Mech*) override { return this; }
    void* mech_target_slot(mech3::Mech*) override { return nullptr; }
    void* slot_contact(void*) override { return nullptr; }
  } hooks;
  mech3::set_hud_hooks(&hooks);
  float out[3] = {1, 2, 3};
  EXPECT(!mech3::lead_point(nullptr, out));
  EXPECT(out[0] == 1 && out[1] == 2 && out[2] == 3);
  mech3::set_hud_hooks(nullptr);
}

// FIX (bug): the HUD's timer (0x0043c390) past an hour: 3700.25 s is
// 01:01:40, not the original's 01:01:3640.
namespace mech3 {
float mouse_y_accel(float current, bool inverted);  // options_menu.h (its Handle clashes with mech.h's here)
}

// The game options' invert box (0x00438100): the exe writes -1.3 or 1.3
// over MouseAccelY; fixed, the player's sensitivity keeps its size.
TEST(fix_inverted_mouse_keeps_sensitivity) {
  zfix::options().bugs = true;
  EXPECT(mech3::mouse_y_accel(2.5f, true) == -2.5f);
  EXPECT(mech3::mouse_y_accel(-2.5f, false) == 2.5f);
  EXPECT(mech3::mouse_y_accel(-0.7f, true) == -0.7f);
  EXPECT(mech3::mouse_y_accel(0.0f, true) == -1.3f);
  zfix::options().bugs = false;
  EXPECT(mech3::mouse_y_accel(2.5f, true) == -1.3f);
  EXPECT(mech3::mouse_y_accel(-2.5f, false) == 1.3f);
  zfix::options().bugs = true;
}

namespace mech3 {
void credit_line_offset(zrdr::Node* line, int32_t off[2]);  // credits.h (its Handle clashes with mech.h's here)
}

// The credits' "Other Voices" heading, `"offset" (20)` (0x00429200): the exe
// keeps the line above's (30 0); fixed, the headings' (20 20).
TEST(fix_credits_other_voices_offset) {
  char key[] = "offset";
  zrdr::Node one[2], line[3];
  one[0].type = zrdr::Int, one[0].i = 2;
  one[1].type = zrdr::Int, one[1].i = 20;
  line[0].type = zrdr::Int, line[0].i = 3;
  line[1].type = zrdr::String, line[1].s = key;
  line[2].type = zrdr::List, line[2].list = one;
  zrdr::Node node;
  node.type = zrdr::List, node.list = line;
  int32_t off[2] = {30, 0};
  zfix::options().bugs = true;
  mech3::credit_line_offset(&node, off);
  EXPECT(off[0] == 20 && off[1] == 20);
  zfix::options().bugs = false;
  off[0] = 30, off[1] = 0;
  mech3::credit_line_offset(&node, off);
  EXPECT(off[0] == 30 && off[1] == 0);
  zfix::options().bugs = true;
}

// TARGET's distance past 1 km (0x00441850): the exe runs "1.50km" straight
// on after the name; fixed, " (1.50km)" as " (500m)" under it.
TEST(fix_target_distance_km_spaced) {
  struct Far : mech3::HudHooks {
    int turret = 0;
    bool ready = false;  // no target while the HUD is built
    bool target_valid(void*) override { return ready; }
    void* slot_contact(void*) override { return &turret; }
    float contact_distance(void*) override { return 1500.0f; }
    void* contact_mech(void*) override { return nullptr; }
    void* contact_vehicle(void*) override { return &turret; }
    const char* object_name(void*) override { return "Turret"; }
  } hooks;
  mech3::set_hud_hooks(&hooks);
  void* storage = ::operator new(sizeof(mech3::Hud));
  std::memset(storage, 0, sizeof(mech3::Hud));
  auto* hud = new (storage) mech3::Hud(nullptr);
  hud->mech = reinterpret_cast<mech3::Mech*>(&hooks);  // only passed back to the hooks
  hud->target.hud = hud;
  hooks.ready = true;
  zfix::options().bugs = true;
  hud->target.data_text();
  EXPECT(std::strcmp(hud->target.texts[1].text, "Turret (1.50km)") == 0);
  zfix::options().bugs = false;
  hud->target.data_text();
  EXPECT(std::strcmp(hud->target.texts[1].text, "Turret1.50km") == 0);
  zfix::options().bugs = true;
  hud->mech = nullptr;
  hud->~Hud();
  ::operator delete(storage);
  mech3::set_hud_hooks(nullptr);
}

// The coolant gauge's easing (0x004456f0): a tenth of the way each call in
// the exe; fixed, a tenth per 1/30 s, so two 60 fps frames make one 30 fps step.
TEST(fix_coolant_gauge_per_second) {
  mech3::set_hud_hooks(nullptr);
  void* storage = ::operator new(sizeof(mech3::Hud));
  std::memset(storage, 0, sizeof(mech3::Hud));
  auto* hud = new (storage) mech3::Hud(nullptr);
  zfix::options().per_second = true;
  mech3::coolant_shown() = 1.0f;
  hud->gauges.set_coolant(0.0f, 1.0f / 60.0f);
  hud->gauges.set_coolant(0.0f, 1.0f / 60.0f);
  EXPECT(std::fabs(mech3::coolant_shown() - 0.9f) < 1e-5f);
  zfix::options().per_second = false;
  mech3::coolant_shown() = 1.0f;
  hud->gauges.set_coolant(0.0f, 1.0f / 60.0f);
  hud->gauges.set_coolant(0.0f, 1.0f / 60.0f);
  EXPECT(std::fabs(mech3::coolant_shown() - 0.81f) < 1e-5f);
  zfix::options().per_second = true;
  mech3::coolant_shown() = 1.0f;
  hud->~Hud();
  ::operator delete(storage);
}

// The message panel's slide (0x0040f8d0): ftol(60 x the frame's seconds)
// pixels a frame, 0 above 60 fps; fixed, the fraction carried (60 a second).
TEST(fix_message_panel_slides_above_60fps) {
  mech3::set_hud_hooks(nullptr);
  mech3::message_panel_destroy();
  mech3::message_panel_create();
  mech3::MessagePanel* p = mech3::message_panel();
  p->on_y = p->off_y - 100;  // room to slide (no fonts here: the text has no height)
  const int32_t start = p->y;
  zfix::options().per_second = true;
  mech3::message_panel_add_for(5.0f, "%s", "Objective complete");
  for (int i = 0; i < 30; ++i) p->slide(1.0f / 120.0f);  // a quarter second
  EXPECT(start - p->y >= 14 && start - p->y <= 15);
  mech3::message_panel_reset();
  zfix::options().per_second = false;
  mech3::message_panel_add_for(5.0f, "%s", "Objective complete");
  for (int i = 0; i < 30; ++i) p->slide(1.0f / 120.0f);
  EXPECT(p->y == start);  // the exe's: 0 a frame
  zfix::options().per_second = true;
  mech3::message_panel_destroy();
}

TEST(fix_hud_timer_past_an_hour) {
  struct Clock : mech3::HudHooks {
    float mission_time(bool) override { return 3700.25f; }
  } hooks;
  zfix::options().bugs = true;
  mech3::set_hud_hooks(&hooks);
  void* storage = ::operator new(sizeof(mech3::Hud));
  std::memset(storage, 0, sizeof(mech3::Hud));
  auto* hud = new (storage) mech3::Hud(nullptr);
  hud->mech = reinterpret_cast<mech3::Mech*>(&hooks);  // only passed back to the hooks
  hud->active = 0;
  hud->target.hide();  // no tracking
  hud->data_update(0.033f);
  EXPECT(std::strcmp(hud->timer.text, "01:01:40") == 0);
  zfix::options().bugs = false;
  hud->data_update(0.033f);
  EXPECT(std::strcmp(hud->timer.text, "01:01:3640") == 0);
  zfix::options().bugs = true;
  hud->mech = nullptr;
  hud->~Hud();
  ::operator delete(storage);
  mech3::set_hud_hooks(nullptr);
}

// A mech placed at y -50 on a bridge over a canyon floor at -100: the exe's
// step of y + 10 reaches only -90, so it takes the floor; the fixed step of
// 10 keeps it on the bridge (0x004161c0, mission-start.md section 11).
static float place_on_bridge(bool fix) {
  zfix::options().bugs = fix;
  static mech3::Mech m;
  static mech3::PlayerControls c;
  static mech3::Skis s;
  m = mech3::Mech{}, c = mech3::PlayerControls{}, s = mech3::Skis{};
  static float matrix[12];
  c.owner = &m, m.controls = &c, s.mech = &m, m.skis = &s, m.transform = matrix;
  static float placed_y;
  mech3::place_calls().ground = [](float, float, float, zcls::GroundHits* out) {
    *out = zcls::GroundHits{};
    out->count = 2;
    out->hit[0].height = -100.0f, out->hit[1].height = -50.0f;
    return 0;
  };
  mech3::place_calls().attach = [](mech3::Handle) {};
  mech3::collide_calls().set_position = [](mech3::Mech&, const mech3::Vec3* p) { placed_y = p->y; };
  mech3::update_calls().moved = [](mech3::Mech&) {};
  const float pos[3] = {0.0f, -50.0f, 0.0f};
  m.place(pos, 0.0f);
  mech3::place_calls() = mech3::PlaceCalls{};
  mech3::collide_calls().set_position = nullptr;
  mech3::update_calls().moved = nullptr;
  zfix::options().bugs = true;
  return placed_y;
}

TEST(fix_place_steps_ten_above) {
  EXPECT(std::fabs(place_on_bridge(false) - -99.99f) < 1e-3f);  // the exe: under the bridge
  EXPECT(std::fabs(place_on_bridge(true) - -49.99f) < 1e-3f);   // fixed: on it
}

// TAG (weapons.md "TAG, end to end"; Owen 2026-10-20). The frames the hold
// needs on the target at a frame rate: the exe's float sum takes 61 at 60
// fps and 145 at 144; in whole milliseconds, one second exactly.
static int tag_hold_frames(int fps, bool fix) {
  zfix::options().bugs = fix;
  float t = 0;
  int n = 0;
  do t = static_cast<float>(1.0 / fps) + t, ++n;
  while (!zwep::designate_held(t, 1.0f));
  zfix::options().bugs = true;
  return n;
}

// The frames a TAG beam is cast, held from a full charge (the start frame
// and each update until the one that stops it): the exe's 32 / 62 / 146 at
// 30 / 60 / 144 fps; fixed, 1.067 s at any rate.
static int tag_beam_frames(int fps, bool fix) {
  zfix::options().bugs = fix;
  static mech3::WeaponType t;
  static mech3::WeaponManager m(nullptr);
  zwep::WeaponDef def{};
  t.fire_time = 1.0f;
  def.type = &t, def.period = 10000.0f, def.flags = zwep::kDesignate;
  mech3::weapon_calls().beam_stop = [](mech3::Handle) {};
  int frames = 0;
  {
    mech3::BeamWeapon w(&def, nullptr, &m);
    w.bin->amount -= 1;  // the trigger's
    w.firing = true;
    const float dt = static_cast<float>(1.0 / fps);
    for (frames = 1; frames < 10 * fps; ++frames) {
      w.update(dt);
      if (!w.firing) break;
    }
  }
  mech3::weapon_calls().beam_stop = nullptr;
  zfix::options().bugs = true;
  return frames;
}

TEST(fix_tag_hold_and_beam_in_milliseconds) {
  const int fps[3] = {30, 60, 144};
  const int exe_hold[3] = {30, 61, 145}, exe_beam[3] = {32, 62, 146};
  for (int i = 0; i < 3; ++i) {
    EXPECT(tag_hold_frames(fps[i], false) == exe_hold[i]);
    EXPECT(tag_beam_frames(fps[i], false) == exe_beam[i]);
    EXPECT(tag_hold_frames(fps[i], true) == fps[i]);  // one second
    const int beam = tag_beam_frames(fps[i], true);
    // 1.067 s (the exe's at 30 fps) to within a frame: ~67 ms of slack at any rate
    EXPECT(std::fabs(beam * 1000.0 / fps[i] - 1066.7) < 1000.0 / fps[i]);
    EXPECT(std::fabs((beam - fps[i]) * 1000.0 / fps[i] - 66.7) < 1000.0 / fps[i]);
  }
}

// The strike homes on the marked mech: fixed, the missiles' lock is the
// unit's live position and velocity; the exe's, the point it stood on.
TEST(fix_tag_strike_homes_on_the_unit) {
  static const zwep::Vec3f3 unit_at{5, 0, -300}, unit_vel{0, 0, -10};
  static const zwep::Vec3f3* seen_at;
  static const zwep::Vec3f3* seen_vel;
  mech3::WeaponCalls& c = mech3::weapon_calls();
  c.strike_target = [](mech3::Handle, zwep::LockRef* out) {
    out->entity = reinterpret_cast<mech3::Handle>(0x1234), out->at = &unit_at, out->velocity = &unit_vel;
    return true;
  };
  c.spawn = [](zwep::WeaponDef*, mech3::Handle, mech3::Handle, const mech3::Vec3f*, const mech3::Vec3f*,
               const mech3::Vec3f*, int32_t, int32_t, bool) -> mech3::Handle {
    const zwep::LockRef* l = zwep::fire_state().lock_target;
    seen_at = l ? l->at : nullptr, seen_vel = l ? l->velocity : nullptr;
    return nullptr;
  };
  mech3::WeaponType t;
  zwep::WeaponDef missile{};
  missile.type = &t;
  t.strike_weapon = &missile;
  const zwep::Vec3f3 dir{0, 0, -1}, at{0, 0, -300};
  for (int fix = 0; fix < 2; ++fix) {
    zfix::options().bugs = fix != 0;
    seen_at = seen_vel = nullptr;
    t.strike(nullptr, dir, at, reinterpret_cast<mech3::Handle>(0x99));
    if (fix) EXPECT(seen_at == &unit_at && seen_vel == &unit_vel);
    else EXPECT(seen_at == &t.strike_point && seen_vel == nullptr);
  }
  zfix::options().bugs = true;
  c.strike_target = nullptr, c.spawn = nullptr;
}

// The brain's home points (FIX (bug), docs/fixes.md, ai.md 17.2): a fight's
// part going before the own order's part stacked on it. In the exe the own
// part keeps the fight's freed home as its under and puts it back on +0xe4
// (the leash's home) when it goes; here it hands over the fight's own under.
// Run under ASan the old way read freed memory at the end.
namespace {
struct HomeControls : mech3::ai::BrainControls {
  zgame::Handle vehicle() override { return 1; }
  void stop() override {}
  void smooth(float) override {}
  bool hit_lately() override { return false; }
  void fire(bool) override {}
  bool reversing() override { return false; }
  float throttle_now() override { return 0; }
  void set_throttle(float) override {}
  void set_turn(float) override {}
  float turn_goal() override { return 0; }
  void aim_started() override {}
};
struct HomeHooks : mech3::ai::BrainHooks {
  float at[3] = {};
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
  void position(zgame::Handle, float out[3]) override { std::memcpy(out, at, 12); }
};
}  // namespace

TEST(fix_ai_home_points_out_of_order) {
  using namespace mech3::ai;
  HomeHooks hooks;
  set_brain_hooks(&hooks);
  {
    BuiltinNet net(7, 0);
    AiParams rec;
    Brain b(&net, rec);
    HomeControls c;
    b.controls = &c;
    static const float base[3] = {1, 2, 3};
    b.xe4 = base;
    VehicleNet vn(0);
    Order fight, own;
    fight.owner = own.owner = &b;
    // The fight's part (its home at 10), then the own order's (at 20) on it.
    hooks.at[0] = 10;
    auto* a = new AttackPart(&fight, nullptr);
    hooks.at[0] = 20;
    auto* v = new VehiclePart(&own, &vn);
    EXPECT(b.xe4 == v->home && v->prev == a->home && a->prev == base);
    // The fight's goes first: the top stays, the own part's under is now the base.
    delete a;
    EXPECT(b.xe4 == v->home && v->prev == base);
    // The own part goes: +0xe4 is the base again, not the freed home (the
    // leash reads it: engage_checks; ASan reports the exe's way here).
    delete v;
    const volatile float leash_x = b.xe4 ? b.xe4[0] : 0.0f;
    EXPECT(b.xe4 == base && leash_x == 1.0f && b.homes.empty());
    // In stack order it is the exe's: each part puts back the +0xe4 it found.
    hooks.at[0] = 30;
    auto* a2 = new AttackPart(&fight, nullptr);
    auto* v2 = new VehiclePart(&own, &vn);
    delete v2;
    EXPECT(b.xe4 == a2->home);
    delete a2;
    EXPECT(b.xe4 == base);
  }
  set_brain_hooks(nullptr);
}

// The station keeping's run (FIX (bug), docs/fixes.md, ai.md 17.2): with
// +0x78 below the +0x24 its state took (0.9 of an older +0x78), a vehicle
// between the two sends phase 0 to a phase that hands it straight back,
// for ever in the exe. Here the run stops.
namespace {
struct StationHooks : HomeHooks {
  bool entity_active(zgame::Handle) override { return true; }
  zgame::Handle bearer(zgame::Handle v) override { return v; }
  void position(zgame::Handle v, float out[3]) override {
    out[0] = v == 2 ? 7.0f : 0.0f, out[1] = 0, out[2] = 0;  // the other 7 m ahead (+x)
  }
  void forward(zgame::Handle, float* x, float* z) override { *x = 1, *z = 0; }
  void velocity(zgame::Handle, float out[3]) override { out[0] = out[1] = out[2] = 0; }
  float move_speed(zgame::Handle) override { return 0; }
};
}  // namespace

TEST(fix_ai_station_keeping_no_endless_phases) {
  using namespace mech3::ai;
  StationHooks hooks;
  set_brain_hooks(&hooks);
  {
    BuiltinNet net(7, 0);
    AiParams rec;
    Brain b(&net, rec);
    HomeControls c;
    b.controls = &c;
    b.params.put<float>(0x74, 1600.0f);
    b.params.put<float>(0x78, 10.0f);
    VehicleNet vn(2);
    Order& o = b.order;
    o.net.reset(vn.make_part(o));
    o.state.reset(vn.own_order()->make_state(o));
    auto* st = dynamic_cast<StationState*>(o.state.get());
    EXPECT(st && st->close == 9.0f);
    b.params.put<float>(0x78, 5.0f);  // now below the 9 it took
    st->run(1.0f / 30);  // the exe: phase 0 -> 1 -> 0 -> 1 ... for ever
    EXPECT(b.order.state && st->phase <= 3);
    b.order.state.reset();
    b.order.net.reset();
  }
  set_brain_hooks(nullptr);
}

// The pocket commander's release (FIX (bug), fixes.md, ai.md 17.6a): when the
// player's mech goes while the mode is up, the exe's 0x0046ad80 deletes the
// mode and answers yes, and the mech (whose controls still are the mode)
// deletes it again. Here the mode is left, answers yes, and is deleted once,
// by the mech's side; the player's controls stay and are his again.
TEST(fix_pocket_commander_freed_once) {
  using namespace mech3;
  pocket_calls() = PocketCalls{};
  pocket_calls().view_state = [](PocketControls&, int32_t) {};  // no camera on a bare mech
  Mech m;
  auto* prev = new PlayerControls();
  prev->construct(1);
  prev->owner = &m;
  m.controls = prev;
  PlayerControls*& inst = player_controls_instance();
  PlayerControls* const saved = inst;
  inst = prev;
  MechControls* c = lance_dispatch(prev, 2);
  PocketControls* p = c ? c->pocket() : nullptr;
  EXPECT(p && inst == p && pocket_globals().active == p);
  m.controls = c;
  const bool yes = p && p->release();  // 0x0041520b: then the mech deletes its controls
  EXPECT(yes && inst == prev && prev->owner == &m && !pocket_globals().active);
  if (yes) delete p;  // once (ASan: the exe's way frees it twice)
  m.controls = nullptr;
  inst = saved;
  delete prev;
  pocket_calls() = PocketCalls{};
}
