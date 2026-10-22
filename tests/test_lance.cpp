// The lance's own (docs/ai.md 17.6): what the dead leave the player.
#include <list>

#include "check.h"
#include "mech3/mech.h"
#include "mech3/lance_orders.h"
#include "mech3/mech_control.h"
#include "mech3/mech_pocket.h"
#include "mech3/zai_params.h"

// 0x0049e540 -> 0x0049e4e0 (the AI controls' death): a pilot of the
// player's side leaves him its triggers that watch a region; one of another
// side, or with none, nothing.
TEST(lance_dead_pilots_region_triggers_go_to_the_player) {
  using namespace mech3;
  Mech player;
  PlayerControls pc;
  player.controls = &pc;
  Mech* const saved = world().mission_player;
  world().mission_player = &player;
  pc.settings.put<int32_t>(0x18, 1);
  ai::AiParams dead;
  dead.put<int32_t>(0x18, 1);
  dead.vlist = new std::list<ai::VListEntry>;
  ai::VListEntry a, b, c;
  a.region = reinterpret_cast<ai::Region*>(0x100);  // never read here
  c.region = reinterpret_cast<ai::Region*>(0x300);
  a.flags = 1, b.flags = 2, c.flags = 3;
  dead.vlist->push_back(a), dead.vlist->push_back(b), dead.vlist->push_back(c);
  player_takes_triggers(dead);
  EXPECT(pc.settings.vlist && pc.settings.vlist->size() == 2);
  EXPECT(pc.settings.vlist->front().flags == 1 && pc.settings.vlist->back().flags == 3);
  player_takes_triggers(dead);  // a second death: appended again
  EXPECT(pc.settings.vlist->size() == 4);
  dead.put<int32_t>(0x18, 2);   // the other side
  player_takes_triggers(dead);
  EXPECT(pc.settings.vlist->size() == 4);
  world().mission_player = nullptr;  // no player
  dead.put<int32_t>(0x18, 1);
  player_takes_triggers(dead);
  EXPECT(pc.settings.vlist->size() == 4);
  world().mission_player = saved;
  player.controls = nullptr;
}

// The pocket commander's death (0x0046abb0, IControls vt+0xa8): the mode
// left and deleted, the player's old controls his mech's again.
TEST(lance_pocket_commander_left_on_death) {
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
  EXPECT(c && c->pocket());
  m.controls = c;
  c->on_death();
  EXPECT(m.controls == prev && prev->owner == &m && inst == prev && !pocket_globals().active);
  m.controls = nullptr;
  inst = saved;
  delete prev;
  pocket_calls() = PocketCalls{};
}
