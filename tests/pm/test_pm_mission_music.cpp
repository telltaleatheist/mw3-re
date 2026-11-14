// Pirate's Moon's mission music (its mission start 0x004838b0, the port's CoreState::start_mission with the table's
// mission_music_to_disc_end): a disc of six tracks or more plays on from its random track to the disc's end (CD mode
// 3), 2..5 tracks one track by mission (mode 6); the mode stays in 0x00607f28 (6 at the start) for a -cd track and
// the attract movie's resume (0x00485280). MW3 plays every pick in mode 6 and resumes in 6.
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "mech3/app.h"
#include "mech3/app_states.h"
#include "mech3/options.h"
#include "pm/pm_game.h"

namespace {
struct Log : mech3::Engine {
  std::vector<std::string> keys;
  std::vector<std::vector<uint32_t>> args;
  uint32_t rand_value = 0, mission = 0;
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
  uint32_t vcall(uint32_t, uint32_t slot, std::initializer_list<zgame::Arg>) override {
    return slot == 0x4c ? mission : 0;  // the game's mission number (PM's vt+0x48)
  }
  void music_play(int32_t track, int32_t mode) override { unported("music", {track, mode}); }
  void cd_resume(int32_t mode) override { unported("resume", {mode}); }
  std::vector<uint32_t> last(const std::string& key) const {
    for (size_t i = keys.size(); i-- > 0;)
      if (keys[i] == key) return args[i];
    return {};
  }
};
using V = std::vector<uint32_t>;
}  // namespace

TEST(pm_mission_music_plays_to_the_discs_end_and_keeps_the_mode) {
  for (const bool pm : {true, false}) {
    zgame::set_game(pm ? pm::table() : zgame::mw3_table());
    Log engine;
    mech3::reset_states();
    mech3::options() = mech3::Options{};
    auto* app = new mech3::Mech3App(&engine);
    mech3::set_app(app);
    app->set_defaults();
    const uint32_t far = pm ? 3 : 6;
    engine.tracks = 2;  // a -cd track before any pick: PM's mode starts at 6
    app->set_cd_track(1);
    mech3::states().core.start_mission();
    EXPECT(engine.last("music") == (V{1, 6}));
    app->set_cd_track(0);
    engine.tracks = 12, engine.rand_value = 30;  // 30 % 12 + 1
    mech3::states().core.start_mission();
    EXPECT(engine.last("music") == (V{7, far}));
    app->set_cd_track(9);  // -cd9: PM in the mode the last pick left
    mech3::states().core.start_mission();
    EXPECT(engine.last("music") == (V{9, far}));
    mech3::states().attract.update();  // no movie: back, the CD resumed in that mode
    EXPECT(engine.last("resume") == (V{far}));
    app->set_cd_track(0);
    engine.tracks = 4, engine.mission = 5;  // 5 % 3 + 2, one track
    mech3::states().core.start_mission();
    EXPECT(engine.last("music") == (V{4, 6}));
    mech3::states().attract.update();
    EXPECT(engine.last("resume") == (V{6}));
    app->set_cd_track(2);
    mech3::states().core.start_mission();
    EXPECT(engine.last("music") == (V{2, 6}));
    mech3::set_app(nullptr);
    delete app;
  }
  zgame::set_game(pm::table());
}
