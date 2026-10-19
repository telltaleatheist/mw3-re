#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zreader/zreader.h"
#include "mech3/app.h"
#include "mech3/app_states.h"
#include "mech3/options.h"

namespace {
// An engine that remembers the calls it gets, by key.
struct Log : mech3::Engine {
  std::vector<std::string> keys;
  std::vector<std::vector<uint32_t>> args;
  uint32_t rand_value = 0;
  int32_t tracks = 0;
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> a) override {
    keys.emplace_back(key);
    std::vector<uint32_t> v;
    for (const auto& x : a) v.push_back(x.v);
    args.push_back(v);
    return 0;
  }
  uint32_t rand() override { return rand_value; }
  int32_t cd_track_count() override { return tracks; }
  void music_play(int32_t track, int32_t mode) override {
    unported("music", {track, mode});
  }
  std::vector<uint32_t> last(const std::string& key) const {
    for (size_t i = keys.size(); i-- > 0;)
      if (keys[i] == key) return args[i];
    return {};
  }
};

struct Fixture {
  Log engine;
  mech3::Mech3App* app;
  Fixture() {
    mech3::reset_states();
    mech3::options() = mech3::Options{};
    app = new mech3::Mech3App(&engine);
    mech3::set_app(app);
    app->set_defaults();
  }
  ~Fixture() { delete app; }
};
}  // namespace

TEST(app_command_line_switches) {
  Fixture f;
  mech3::parse_command_line(f.engine, *f.app, "mech3.exe -nofmv -cd7");
  EXPECT(f.app->fmv() == 0);
  EXPECT(f.app->cd_track() == 7);
  mech3::parse_command_line(f.engine, *f.app, "mech3.exe -cd");
  EXPECT(f.app->cd_track() == 0);
  // Prefix matches: -nofmvx is -nofmv, -cd12abc is track 12.
  f.app->fmv() = 1;
  mech3::parse_command_line(f.engine, *f.app, "x  -nofmvx -cd12abc");
  EXPECT(f.app->fmv() == 0);
  EXPECT(f.app->cd_track() == 12);
}

TEST(app_seasick_is_fast_menu) {
  Fixture f;
  struct Engine : Log {
    void set_fast_menu(int32_t on) override { mech3::set_fast_menu(on); }
  } e;
  mech3::parse_command_line(e, *f.app, "mech3.exe -seasick");
  EXPECT(mech3::fast_menu() == 1);
  EXPECT((mech3::options().game_options & mech3::kFastMenu) != 0);
}

TEST(app_state_numbers) {
  Fixture f;
  mech3::States& s = mech3::states();
  EXPECT(f.app->state(1) == &s.intro);
  EXPECT(f.app->state(5) == &s.core);
  EXPECT(f.app->state(11) == nullptr);
  EXPECT(f.app->state(12) == &s.main);  // the post-intro state, Main by default
  EXPECT(f.app->first_state_ == &s.intro);
}

// On a disc of more than five tracks the mission music is rand() % tracks + 1,
// which can be the data track 1 (docs/discoveries.md).
TEST(app_mission_music_can_pick_the_data_track) {
  Fixture f;
  f.engine.tracks = 12;
  f.engine.rand_value = 24;  // 24 % 12 + 1
  mech3::states().core.start_mission();
  const std::vector<uint32_t> want = {1, 6};
  EXPECT(f.engine.last("music") == want);
  f.app->set_cd_track(9);  // -cd9 wins while under the track count
  mech3::states().core.start_mission();
  const std::vector<uint32_t> want9 = {9, 6};
  EXPECT(f.engine.last("music") == want9);
}

TEST(app_main_menu_idles_into_the_attract_movie) {
  Fixture f;
  mech3::MainState& m = mech3::states().main;
  m.reset_idle();
  EXPECT(m.idle_ == 60.0f);
  m.idle_ = 0.0f;  // frame_dt() is 0 here: the idle time is used up
  f.app->activate();
  m.update();
  EXPECT(f.app->pending() == 1);
  EXPECT(f.app->pending_list().front().state == &mech3::states().attract);
  EXPECT(m.idle_ == 60.0f);
}

// GameIntensity: a complete intensity.zrd entry replaces the built-in preset.
TEST(options_game_intensity_from_the_reader) {
  struct Hooks : mech3::OptionsHooks {
    zrdr::Node* root = nullptr;
    zrdr::Node* reader_load(const char*) override { return root; }
  } hooks;
  // ("high" (2.0 3 0.5 1 0)) built as the reader keeps it: list[0] is the count.
  zrdr::Node* root = zrdr::new_nodes(zrdr::List, 1);
  root->list = zrdr::alloc_list(3);
  root->list[0] = zrdr::Node{zrdr::Int, {3}};
  char* key = static_cast<char*>(std::malloc(5));
  std::strcpy(key, "high");
  root->list[1].type = zrdr::String;
  root->list[1].s = key;
  zrdr::Node& entry = root->list[2];
  entry.type = zrdr::List;
  entry.list = zrdr::alloc_list(6);
  entry.list[0] = zrdr::Node{zrdr::Int, {6}};
  entry.list[1].type = zrdr::Float;
  entry.list[1].f = 2.0f;
  entry.list[2] = zrdr::Node{zrdr::Int, {3}};
  entry.list[3].type = zrdr::Float;
  entry.list[3].f = 0.5f;
  entry.list[4] = zrdr::Node{zrdr::Int, {1}};
  entry.list[5] = zrdr::Node{zrdr::Int, {0}};
  hooks.root = root;
  mech3::options() = mech3::Options{};
  mech3::set_options_hooks(&hooks);
  mech3::set_game_intensity(2);
  mech3::set_options_hooks(nullptr);
  const mech3::Options& o = mech3::options();
  EXPECT(o.game_intensity == 2);
  EXPECT(o.intensity[0] == 2.0f && o.intensity[1] == 3.0f && o.intensity[2] == 0.5f);
  EXPECT(o.intensity_flag[0] == 1 && o.intensity_flag[1] == 0);
  // Without the file, the preset ("low": 0.5 1.2 0.7, flags on), even for -1.
  mech3::set_game_intensity(-1);
  EXPECT(o.game_intensity == -1);
  EXPECT(o.intensity[0] == 0.5f && o.intensity_flag[0] == 1);
}
