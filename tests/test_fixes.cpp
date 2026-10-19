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
