#include <set>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zsound/zsnd_cd.h"
#include "mech3/music.h"
#include "platform/platform.h"

namespace {
struct FakeFiles : platform::Files {
  std::set<std::string> present;
  bool exists(const std::string& p) override { return present.count(p) != 0; }
};
struct FakeOutput : platform::MusicOutput {
  std::vector<std::string> log;
  bool play(const std::string& p) override { log.push_back("play " + p); return true; }
  void stop() override { log.push_back("stop"); }
  void pause() override { log.push_back("pause"); }
  void resume() override { log.push_back("resume"); }
  void set_volume(uint16_t l, uint16_t r) override { log.push_back("volume " + std::to_string(l) + " " + std::to_string(r)); }
};
// MW3 1.2's CD_TRACKS table, from zbd/reader.zbd.
std::vector<zsnd_cd::Cue> mw3_cues() {
  return {{"menu", {2}}, {"c1", {3}}, {"c2", {3}}, {"c3", {3}}, {"c4", {3}}, {"dev1", {2}}, {"dev2", {2}}};
}
struct Fixture {
  FakeFiles files;
  FakeOutput out;
  Fixture(std::set<std::string> tracks) {
    files.present = std::move(tracks);
    zsnd_cd::set_backend(&files, &out, "music");
  }
  ~Fixture() {
    zsnd_cd::close();
    zsnd_cd::set_backend(nullptr, nullptr, "music");
  }
};
}  // namespace

TEST(zsnd_cd_without_music_files_is_like_no_disc) {
  Fixture f({});
  EXPECT(!zsnd_cd::open(mw3_cues()));
  EXPECT(!zsnd_cd::play_track(2, zsnd_cd::Once));
}

TEST(zsnd_cd_finds_any_supported_format) {
  Fixture f({"music/track02.flac", "music/track03.ogg"});
  EXPECT(zsnd_cd::track_path(2) == "music/track02.flac");
  EXPECT(zsnd_cd::track_path(3) == "music/track03.ogg");
  EXPECT(zsnd_cd::track_path(4).empty());
  EXPECT(zsnd_cd::open(mw3_cues()));
  EXPECT(zsnd_cd::state().num_tracks == 3);
}

TEST(zsnd_cd_menu_cue_plays_track_2_once) {
  Fixture f({"music/track02.ogg", "music/track03.ogg"});
  zsnd_cd::open(mw3_cues());
  mech3_music::set_volume(0xffff, 0xffff);
  mech3_music::play(zsnd_cd::cue_track("menu"), zsnd_cd::Once);  // what 0x00486596 does
  EXPECT(f.out.log.back() == "play music/track02.ogg");
  f.out.log.clear();
  zsnd_cd::on_output_finished();  // mode 2 does not loop
  EXPECT(f.out.log.empty());
}

TEST(zsnd_cd_loop_mode_replays_the_track) {
  Fixture f({"music/track03.ogg"});
  zsnd_cd::open(mw3_cues());
  zsnd_cd::play_track(3, zsnd_cd::Loop);
  f.out.log.clear();
  zsnd_cd::on_output_finished();
  EXPECT(f.out.log.size() == 2 && f.out.log[1] == "play music/track03.ogg");
  EXPECT(zsnd_cd::state().mode == zsnd_cd::Restart);
}

TEST(zsnd_cd_same_track_request_is_ignored_unless_restart) {
  Fixture f({"music/track03.ogg"});
  zsnd_cd::open(mw3_cues());
  zsnd_cd::play_track(3, zsnd_cd::Loop);
  f.out.log.clear();
  EXPECT(zsnd_cd::play_track(3, zsnd_cd::Loop));
  EXPECT(f.out.log.empty());
  zsnd_cd::play_track(3, zsnd_cd::Restart);
  EXPECT(!f.out.log.empty());
}

TEST(zsnd_cd_disc_mode_plays_on_into_the_next_track) {
  Fixture f({"music/track02.ogg", "music/track03.ogg"});
  zsnd_cd::open(mw3_cues());
  zsnd_cd::play_track(2, 3);  // any mode but 0-2, 5, 6; MW3 itself only uses 2 and 6
  zsnd_cd::on_output_finished();
  EXPECT(f.out.log.back() == "play music/track03.ogg");
}

TEST(zsnd_cd_pause_and_resume) {
  Fixture f({"music/track02.ogg"});
  zsnd_cd::open(mw3_cues());
  zsnd_cd::play_track(2, zsnd_cd::Loop);
  EXPECT(zsnd_cd::pause());
  EXPECT(zsnd_cd::state().mode == zsnd_cd::Paused);
  EXPECT(!zsnd_cd::pause());
  EXPECT(zsnd_cd::resume(zsnd_cd::Loop));
  EXPECT(f.out.log.back() == "resume");
}

TEST(music_slider_off_stops_and_on_restarts) {
  Fixture f({"music/track02.ogg"});
  zsnd_cd::open(mw3_cues());
  mech3_music::set_volume(0xffff, 0xffff);
  mech3_music::play(2, zsnd_cd::Loop);
  mech3_music::set_volume(0x100, 0x7ff);
  EXPECT(!mech3_music::enabled());
  EXPECT(zsnd_cd::state().mode == zsnd_cd::Stopped);
  mech3_music::set_volume(0x800, 0);
  EXPECT(mech3_music::enabled());
  EXPECT(zsnd_cd::state().mode == zsnd_cd::Loop);
}
