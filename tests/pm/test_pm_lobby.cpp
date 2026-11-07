#include "check.h"
#include "mech3/mp_lobby.h"
#include "mech3/pilot.h"

// PM fills multiplayer's store at the lobby's lock, not in the game (0x0049eb20): pm-tests.md.
TEST(pm_lobby_store_ready) {
  mech3::MultiplayerGame g(nullptr);
  EXPECT(!g.store.full && g.store.items.empty());
  mech3::lobby_store_ready(g.store);
  EXPECT(g.store.full && !g.store.items.empty());
  const size_t n = g.store.items.size();
  bool mechs = false;
  for (const auto* it : g.store.items) mechs |= it->kind() == 0;
  EXPECT(!mechs);
  mech3::lobby_store_ready(g.store);  // a second lock: filled again, not added to
  EXPECT(g.store.items.size() == n);
}
