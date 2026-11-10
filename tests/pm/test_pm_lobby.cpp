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

// The host's Upgrades switches in Pirate's Moon's player data: 7 words, the add-on worlds' count and values, our
// MODS block, then "UPGR" and the bits (LobbyScreen::enter). Read from that layout, not MechWarrior 3's (4 words):
// read MW3's way every joiner was told the host's game was older than 1.1.0.
TEST(pm_lobby_reads_the_hosts_upgrades) {
  auto data = [](uint32_t worlds, const std::vector<std::string>& mods, int bits) {
    std::vector<uint32_t> d = {1, 0, 0, 0, 0, 0, 0, worlds};
    for (uint32_t k = 0; k < worlds; ++k) d.push_back(0x300 + k);
    d.push_back(0x53444f4d), d.push_back(0x1234), d.push_back(0x5678), d.push_back(static_cast<uint32_t>(mods.size()));
    for (const std::string& m : mods) {
      d.push_back(static_cast<uint32_t>(m.size()));
      for (size_t k = 0; k < m.size(); k += 4) {
        uint32_t w = 0;
        for (size_t b = 0; b < 4 && k + b < m.size(); ++b) w |= static_cast<uint32_t>(static_cast<uint8_t>(m[k + b])) << (8 * b);
        d.push_back(w);
      }
    }
    if (bits >= 0) d.push_back(0x52475055), d.push_back(static_cast<uint32_t>(bits));
    return d;
  };
  uint64_t hash = 0;
  std::vector<std::string> names;
  uint32_t bits = 99;
  for (const uint32_t worlds : {0u, 1u, 3u})
    for (const auto& mods : std::vector<std::vector<std::string>>{{}, {"abcde"}, {"one", std::string(64, 'x'), "four"}}) {
      const std::vector<uint32_t> d = data(worlds, mods, 0x16);
      EXPECT(mech3::LobbyScreen::mods_of(d, &hash, &names) && hash == 0x567800001234ull && names == mods);
      bits = 99;
      EXPECT(mech3::LobbyScreen::upgrades_of(d, &bits) && bits == 0x16);
      // A host before 1.1.0: the mods block and nothing after it.
      EXPECT(!mech3::LobbyScreen::upgrades_of(data(worlds, mods, -1), &bits) && bits == 0);
      // Cut short at any word: never read past the end, never a wrong word taken for the bits.
      for (size_t cut = 0; cut < d.size(); ++cut) {
        const std::vector<uint32_t> part(d.begin(), d.begin() + static_cast<long>(cut));
        EXPECT(!mech3::LobbyScreen::upgrades_of(part, &bits) && bits == 0);
      }
    }
  // The original's data (its 7 words and the worlds, no block of ours): none.
  EXPECT(!mech3::LobbyScreen::upgrades_of({1, 0, 0, 0, 0, 0, 0, 1, 0x301}, &bits));
}
