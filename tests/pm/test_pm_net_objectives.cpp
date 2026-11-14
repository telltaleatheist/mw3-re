// PM's network respawn's objectives (0x004943e0): freed, then read as the load reads them.
#include <cstdint>

#include "check.h"
#include "mech3/mission.h"

namespace {
struct Objectives : mech3::MissionHooks {
  int freed = 0, read = 0, loaded = 0;
  void objectives_free(mech3::Handle, mech3::Handle, mech3::Handle, mech3::Handle, mech3::Handle) override { ++freed; }
  void game_objectives(mech3::Handle, mech3::Handle, mech3::Handle, mech3::Handle, mech3::Handle) override {
    EXPECT(freed == 1);  // freed first
    ++read;
  }
  void objectives_load(const char*, const char*, mech3::Handle, mech3::Handle, mech3::Handle, mech3::Handle,
                       mech3::Handle) override {
    ++loaded;
  }
  int32_t game_mission_number() override { return 4; }  // past map 2: MW3 read mpinfo.zrd, which PM lacks
};
}  // namespace

// A network respawn's objectives (0x004943e0): freed, read again as the load reads them (the game's vt+0x3c), the
// ended and aborting bytes cleared.
TEST(pm_respawn_reads_the_objectives_as_the_load) {
  Objectives o;
  mech3::MissionHooks& old = mech3::mission_hooks();
  mech3::set_mission_hooks(&o);
  mech3::Missions& m = mech3::missions();
  const uint8_t ended = m.ended, aborting = m.aborting;
  m.ended = 1, m.aborting = 1;
  m.objectives_again();
  EXPECT(o.freed == 1 && o.read == 1 && o.loaded == 0);
  EXPECT(m.ended == 0 && m.aborting == 0);
  m.ended = ended, m.aborting = aborting;
  mech3::set_mission_hooks(&old);
}
