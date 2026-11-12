// The salvage chance (upgrades/mods/salvage_chance, docs/modding.md, The salvage chance): a mod's odds for the
// two coins of a kill's salvage (0x00472ec0 a vehicle's weapons, 0x00472f00 a legged mech whole). Unset, the
// original's rand() & 1; set, the same one draw judged against N in 100; mod.txt's two keys, the first ticked
// mod's value, a mission's own line over the mod-wide one.
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/cheats.h"
#include "mech3/mech_salvage.h"
#include "mech3/mods.h"
#include "mech3/pilot.h"
#include "upgrades/mods/salvage_chance.h"

namespace {
namespace fs = std::filesystem;
std::string tmp_dir(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
void put(const fs::path& p, const std::string& text) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream(p, std::ios::binary) << text;
}
int32_t mechs(const mech3::Store& s) {
  int32_t n = 0;
  for (const mech3::ConfigItem* it : s.items) n += it->kind() == 0 ? it->count : 0;
  return n;
}
// A mech that lost a leg: whole on the coin. A vehicle with no weapon manager: add_vehicle_salvage is true
// when the coin is lost (nothing read) and false when it is won (its weapons asked for, none).
bool legged_comes(const mech3::SalvageCalls& c) {
  mech3::SalvageMech m;
  m.has_data = true, m.type = 0, m.leg_gone[1] = true;
  mech3::Store s;
  mech3::add_salvage(s, m, c);
  return mechs(s) == 1;
}
bool vehicle_comes(const mech3::SalvageCalls& c) {
  mech3::SalvageMech v;
  mech3::Store s;
  return !mech3::add_vehicle_salvage(s, v, c);
}
struct Rules {
  mech3::SalvageRules rules = mech3::salvage_rules();
  int32_t now = mech3::salvage_chance_now();
  ~Rules() { mech3::salvage_rules() = rules, mech3::salvage_chance_now() = now; }
};
}  // namespace

// Unset (no function, or -1): each draw's low bit, as the original, for every value rand() gives; one draw a
// decision either way.
TEST(salvage_chance_unset_is_the_originals_coin) {
  mech3::cheats_clear();
  for (int pass = 0; pass < 2; ++pass) {
    int32_t r = 0, draws = 0, wrong = 0;
    mech3::SalvageCalls c;
    c.rand = [&] { return ++draws, r; };
    if (pass) c.chance = [] { return -1; };
    for (r = 0; r < 0x8000; ++r) wrong += legged_comes(c) != ((r & 1) != 0), wrong += vehicle_comes(c) != ((r & 1) != 0);
    EXPECT(wrong == 0 && draws == 2 * 0x8000);
  }
  // A mech with both legs draws nothing.
  int32_t draws = 0;
  mech3::SalvageCalls c;
  c.rand = [&] { return ++draws, 0; };
  c.chance = [] { return 0; };
  mech3::SalvageMech m;
  m.has_data = true, m.type = 0;
  mech3::Store s;
  mech3::add_salvage(s, m, c);
  EXPECT(mechs(s) == 1 && draws == 0);
}

// Set: 0 never, 100 always, N the share of rand()'s 32768 values to within one step; one draw a decision.
TEST(salvage_chance_set_judges_the_same_draw) {
  mech3::cheats_clear();
  for (const int32_t n : {0, 1, 25, 50, 75, 99, 100}) {
    int32_t r = 0, draws = 0, legged = 0, vehicles = 0;
    mech3::SalvageCalls c;
    c.rand = [&] { return ++draws, r; };
    c.chance = [n] { return n; };
    for (r = 0; r < 0x8000; ++r) legged += legged_comes(c), vehicles += vehicle_comes(c);
    EXPECT(draws == 2 * 0x8000 && legged == vehicles);
    const int32_t want = 0x8000 * n / 100;
    EXPECT(legged >= want - 1 && legged <= want + 328);  // whole hundredths of 32768: at most one hundredth over
    if (n == 0) EXPECT(legged == 0);
    if (n == 100) EXPECT(legged == 0x8000);
    if (n == 50) EXPECT(legged == 0x4000);
  }
  // Food for thought still wins both, without a draw.
  int32_t draws = 0;
  mech3::SalvageCalls c;
  c.rand = [&] { return ++draws, 0; };
  c.chance = [] { return 0; };
  mech3::cheats().max_salvage = true;
  EXPECT(legged_comes(c) && vehicle_comes(c) && draws == 0);
  mech3::cheats_clear();
}

TEST(salvage_chance_values_read) {
  int32_t n = -5;
  EXPECT(mech3::salvage_chance_parse("0", &n) && n == 0);
  EXPECT(mech3::salvage_chance_parse("100", &n) && n == 100);
  EXPECT(mech3::salvage_chance_parse("37", &n) && n == 37);
  for (const char* bad : {"", "101", "-1", "50%", "half", "5 0", "1e2", "0x10", "1000000000000"}) {
    n = 7;
    EXPECT(!mech3::salvage_chance_parse(bad, &n) && n == 7);
  }
  mech3::MissionSalvage m;
  EXPECT(mech3::mission_salvage_parse("c1 2 75", &m) && m.world == "c1" && m.number == 2 && m.chance == 75);
  EXPECT(mech3::mission_salvage_parse("  My_World\t10   0 ", &m) && m.world == "My_World" && m.number == 10 && m.chance == 0);
  for (const char* bad : {"", "c1", "c1 2", "c1 2 101", "c1 0 50", "c1 two 50", "c1 2 50 extra", "c1 -2 50"})
    EXPECT(!mech3::mission_salvage_parse(bad, &m));
  // mod.txt: the keys kept as written; a mod of the one line is a mod.
  mech3::ModInfo info;
  EXPECT(mech3::mod_parse("name=Generous\nsalvage_chance=100\n", &info) && info.salvage_chance == "100" && info.mission_salvage.empty());
  mech3::ModInfo two;
  EXPECT(mech3::mod_parse("name=Two\nmission_salvage=c1 1 0\nMission_Salvage = c2 3 100\n", &two) && two.mission_salvage.size() == 2 &&
         two.salvage_chance.empty());
  EXPECT(mech3::salvage_chance_sentence(info) == "It sets how often a kill's salvage is offered: 100 in 100; the original's is 1 in 2.");
  mech3::ModInfo one;
  mech3::mod_parse("name=One\nmission_salvage=c1 2 75\n", &one);
  EXPECT(mech3::salvage_chance_sentence(one) == "For c1 mission 2 it sets it to 75 in 100. The original's is 1 in 2.");
  EXPECT(mech3::salvage_chance_sentence(two) == "For 2 missions it sets its own odds of a kill's salvage. The original's is 1 in 2.");
  mech3::ModInfo none;
  mech3::mod_parse("name=None\n", &none);
  EXPECT(mech3::salvage_chance_sentence(none).empty());
}

// The mount: the first ticked mod's value wins, a mission's line wins over the mod-wide value whichever mod
// it is from, a line that does not read is left out, and a mount of nothing leaves nothing set.
TEST(salvage_chance_first_ticked_mod_and_the_missions_line) {
  Rules keep;
  const zutl::Mounts mounts = zutl::mounts();
  const std::string wd = tmp_dir("salvage_chance_mount");
  put(wd + "/mods/A/mod.txt", "name=A\nfor=mw3\nsalvage_chance=80\nmission_salvage=c1 2 10\nmission_salvage=c9 x 10\n");
  put(wd + "/mods/B/mod.txt", "name=B\nfor=mw3\nsalvage_chance=20\nmission_salvage=C1 2 90\nmission_salvage=c2 1 0\n");
  put(wd + "/mods/Bad/mod.txt", "name=Bad\nfor=mw3\nsalvage_chance=many\n");
  put(wd + "/mods/Plain/mod.txt", "name=Plain\nfor=mw3\n");
  auto mount = [&](const std::vector<std::string>& on) {
    zutl::mounts() = zutl::Mounts{};
    zutl::set_write_dir(wd);
    mech3::mods_mount(wd, on, {MW3_GAME_DIR});
  };
  std::string from;
  mount({"A", "B"});
  EXPECT(mech3::salvage_chance_for("c1", 1, &from) == 80 && from == "A");
  EXPECT(mech3::salvage_chance_for("C1", 2, &from) == 10 && from == "A");  // A's line, B's for the same mission not used
  EXPECT(mech3::salvage_chance_for("c2", 1, &from) == 0 && from == "B");   // B's line over A's mod-wide value
  EXPECT(mech3::salvage_chance_for("c9", 1) == 80);                        // the line that did not read: left out
  EXPECT(mech3::salvage_rules().missions.size() == 2);
  mount({"B", "A"});
  EXPECT(mech3::salvage_chance_for("c1", 1, &from) == 20 && from == "B");
  EXPECT(mech3::salvage_chance_for("c1", 2, &from) == 90 && from == "B");
  mount({"Bad", "Plain", "B"});
  EXPECT(mech3::salvage_chance_for("c4", 4, &from) == 20 && from == "B");  // the bad value sets nothing
  mount({"Plain"});
  EXPECT(mech3::salvage_chance_for("c1", 1) == -1 && mech3::salvage_rules().missions.empty());
  mount({"A"});
  mount({});  // as a plain start: nothing mounted, nothing set
  EXPECT(mech3::salvage_chance_for("c1", 2) == -1 && mech3::salvage_rules().chance == -1);
  zutl::mounts() = mounts;
}
