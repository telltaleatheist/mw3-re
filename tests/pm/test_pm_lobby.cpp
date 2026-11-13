#include "check.h"
#include "mech3/mp_lobby.h"
#include "mech3/pilot.h"
#include "gamez/zutil/zutl_path.h"

#include <string>

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

// The salvage's room from the current game (Pirate's Moon's 0x00480840 / 0x00480260 through the game's vt+0x5c and
// vt+0x60; the moves themselves: pm_moved.py test_salvage --moves-only): a multiplayer round's 3 bases and its bonus
// tons, any other game's 3 and 0, a campaign's units and units x 300. Before, outside a campaign, 0 and 0: every pick
// refused.
TEST(pm_salvage_room_comes_from_the_game) {
  struct E : mech3::Engine {} e;
  mech3::Pilot*& cur = mech3::current_pilot();
  mech3::Pilot* was = cur;
  // Its files in the test's own folder (a campaign game writes the pilot), the player's never touched.
  const std::string was_wd = zutl::mounts().write;
  zutl::set_write_dir(std::string(MW3_TEST_TMP) + "/pm_salvage_room");
  // Never destroyed: a pilot's destructor frees its games and may write its file.
  auto* p = new mech3::Pilot("salvage");
  cur = p;
  mech3::Game* first = p->current;
  p->current = nullptr;
  EXPECT(e.salvage_bases() == 0 && e.salvage_tons() == 0.0f);  // no game (ours: the exe always has one)
  p->current = first;
  auto* m = new mech3::MultiplayerGame(p);
  m->bonus_tons = 450.5f;
  p->multi = m, p->current = m;
  EXPECT(e.salvage_bases() == 3 && e.salvage_tons() == 450.5f);
  p->current = new mech3::TrainingGame(p);
  EXPECT(e.salvage_bases() == 3 && e.salvage_tons() == 0.0f);
  auto* c = new mech3::CampaignGame(p, 1, 2);
  c->a8 = 4;
  p->current = c;
  EXPECT(e.salvage_bases() == 4 && e.salvage_tons() == 1200.0f);
  cur = was;
  zutl::set_write_dir(was_wd);
}
