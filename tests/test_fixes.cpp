// The frame-rate fixes (zfix::Options::per_second, docs/fixes.md): the same
// second of play at 30 and 144 fps comes out the same, and the exe's
// per-frame path does not.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "check.h"
#include "gamez/zmodel/gmod_puff.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/ztimer.h"
#include "mech3/mech.h"
#include "mech3/mech_skin.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_move.h"
#include "mech3/mission_world.h"

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
  first.master = *zsnd::state().volume;
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
  static mech3::Pilot pilot;
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

}  // namespace

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
