// Pirate's Moon's network game: the debug start's parts (ours) that PM's rules read.
#include <memory>

#include "check.h"
#include "gamez/znetwork/znet_dplay.h"
#include "gamez/znetwork/znet_session.h"
#include "mech3/app_mech.h"
#include "mech3/mech_net.h"

// pm-tests 31: the debug start's stand-in lobby (--mission <dir>mp<N>, the soak's) answers PM's round winner
// (0x00437970) as the tally does without teams: one player alone first by score (here the kills). Unset, the
// host's clock end always went to overtime and the soak's game never ended.
TEST(pm_debug_lobby_has_a_round_winner) {
  const mech3::NetCalls saved = mech3::net_calls();
  znet::clear_lists();
  for (uint32_t id = 1; id <= 4; ++id) {
    auto r = std::make_unique<znet::PlayerRecord>();
    r->id = id;
    znet::lists().players.push_back(std::move(r));
  }
  mech3::program_net_debug_lobby();
  mech3::NetCalls& c = mech3::net_calls();
  EXPECT(c.round_winner && c.lobby_kill);
  if (c.round_winner && c.lobby_kill) {
    EXPECT(!c.round_winner());  // all at 0: a tie, overtime
    c.lobby_kill(2);
    EXPECT(c.round_winner());   // P2 alone first
    c.lobby_kill(3);
    EXPECT(!c.round_winner());  // P2 and P3 at 1
    c.lobby_kill(3);
    EXPECT(c.round_winner());   // P3
  }
  mech3::net_calls() = saved;
  znet::clear_lists();
}
