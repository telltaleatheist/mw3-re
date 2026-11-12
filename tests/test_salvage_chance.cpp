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
#include "mech3/mission.h"
#include "mech3/mods_menu.h"
#include "upgrades/editor/form.h"
#include "upgrades/editor/mission_settings.h"
#include "upgrades/editor/mods_edit.h"
#include <iterator>
#include "upgrades/mods/mod_report.h"
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

// The check, the report and the MODS list: a mod of the one line is usable and its report says what it sets;
// a line that does not read is a warning and that line left out; of two ticked mods that set it the later
// one's note says it shares its salvage chance with the first, which wins.
TEST(salvage_chance_check_report_and_shared_note) {
  const std::string wd = tmp_dir("salvage_chance_report");
  put(wd + "/mods/Gen/mod.txt", "name=Generous\nfor=mw3\nsalvage_chance=100\n");
  put(wd + "/mods/Mean/mod.txt", "name=Mean\nfor=mw3\nsalvage_chance=0\nmission_salvage=c1 1 5\n");
  put(wd + "/mods/Odd/mod.txt", "name=Odd\nfor=mw3\nsalvage_chance=lots\nmission_salvage=c1 1 50\n");
  int errors = -1, refused = -1;
  std::string lines;
  for (const std::string& l : mech3::mod_check(wd + "/mods/Gen", MW3_GAME_DIR, &errors)) lines += l + "\n";
  EXPECT(errors == 0 && lines.find("salvage_chance 100: ok") != std::string::npos && lines.find("warning:") == std::string::npos);
  std::string report;
  for (const std::string& l : mech3::mod_report(wd + "/mods/Gen", MW3_GAME_DIR, "", &refused)) report += l + "\n";
  EXPECT(refused == 0);
  EXPECT(report.find("It sets how often a kill's salvage is offered: 100 in 100; the original's is 1 in 2.") != std::string::npos);
  EXPECT(report.find("It changes nothing in the game") == std::string::npos);
  lines.clear();
  for (const std::string& l : mech3::mod_check(wd + "/mods/Odd", MW3_GAME_DIR, &errors)) lines += l + "\n";
  EXPECT(errors == 0 && lines.find("warning: salvage_chance lots") != std::string::npos && lines.find("mission_salvage c1 1 50: ok") != std::string::npos);
  const std::vector<mech3::ModsListEntry> list = mech3::mods_list(wd, {"Gen", "Mean", "Odd"});
  EXPECT(list.size() == 3 && list[0].info.folder == "Gen" && list[0].salvage_taken == 0 && list[0].conflicts_with.empty());
  EXPECT(list[1].salvage_taken == 1 && list[1].conflicts_with == std::vector<std::string>{"Gen"});
  EXPECT(list[2].salvage_taken == 1 && list[2].conflicts_with == std::vector<std::string>{"Mean"});  // its mission's line; its bad value shares nothing
  EXPECT(mech3::mods_row_note_shared(list[1], list) == "Shares its salvage chance with Generous, which wins");
}

// mod.txt's line for a mission: put in its old line's place (the world in any case), else after the last
// mission_salvage= line, else at the end; taken out; every other line and its line end as it was.
TEST(mission_settings_mod_txt_line) {
  using mech3::mod_txt_with_mission_salvage;
  EXPECT(mod_txt_with_mission_salvage("name=Mine\n", "c1", 2, 75) == "name=Mine\nmission_salvage=c1 2 75\n");
  EXPECT(mod_txt_with_mission_salvage("name=Mine", "c1", 2, 75) == "name=Mine\nmission_salvage=c1 2 75\n");
  EXPECT(mod_txt_with_mission_salvage("name=Mine\r\nbase=mw3\r\n", "c1", 2, 0) == "name=Mine\r\nbase=mw3\r\nmission_salvage=c1 2 0\r\n");
  const std::string three = "name=Mine\nmission_salvage=c1 1 10\nMission_Salvage = C1 2 20\nmission_salvage=c2 2 30\nweapon=X ammo=no tech=any\n";
  EXPECT(mod_txt_with_mission_salvage(three, "c1", 2, 75) ==
         "name=Mine\nmission_salvage=c1 1 10\nmission_salvage=c1 2 75\nmission_salvage=c2 2 30\nweapon=X ammo=no tech=any\n");
  EXPECT(mod_txt_with_mission_salvage(three, "c3", 1, 5) ==
         "name=Mine\nmission_salvage=c1 1 10\nMission_Salvage = C1 2 20\nmission_salvage=c2 2 30\nmission_salvage=c3 1 5\nweapon=X ammo=no tech=any\n");
  EXPECT(mod_txt_with_mission_salvage(three, "c1", 2, -1) == "name=Mine\nmission_salvage=c1 1 10\nmission_salvage=c2 2 30\nweapon=X ammo=no tech=any\n");
  EXPECT(mod_txt_with_mission_salvage(three, "c4", 4, -1) == three);
  EXPECT(mech3::mod_txt_mission_salvage(three, "c1", 2) == 20 && mech3::mod_txt_mission_salvage(three, "C2", 2) == 30 &&
         mech3::mod_txt_mission_salvage(three, "c2", 1) == -1);
  // What it writes, the mod's reader takes.
  mech3::ModInfo info;
  EXPECT(mech3::mod_parse(mod_txt_with_mission_salvage(three, "c1", 2, 75), &info) && info.mission_salvage.size() == 3 &&
         info.mission_salvage[1] == "c1 2 75");
}

// The form (the editor's MISSION SETTINGS row): only in a mission opened from MODS; the value typed is
// checked on the form; OK writes the mission's line into the editor's own copy (the old mod.txt kept), which
// the next mount reads; empty takes it out; a mod the editor did not make is never written.
TEST(mission_settings_form_writes_the_missions_line) {
  Rules keep;
  const zutl::Mounts mounts = zutl::mounts();
  const mech3::Missions was = mech3::missions();
  const std::string wd = tmp_dir("mission_settings_form");
  const fs::path mod = fs::path(wd) / "mods" / "mine", other = fs::path(wd) / "mods" / "theirs";
  put(mod / "mod.txt", "name=Mine\nbase=mw3\n");
  put(mod / "mw3-editor.txt", "the editor's own (a copy it made): the only kind it writes (save_as.h)\n");
  put(other / "mod.txt", "name=Theirs\nsalvage_chance=30\n");
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"mine", "theirs"}, {MW3_GAME_DIR});
  mech3::missions().current = 1, mech3::missions().number = 3;  // c2 mission 3
  mech3::EditorForm& f = mech3::mission_settings_form();
  mech3::mission_settings_open(0);  // no session: a line under the menu, no form
  EXPECT(!mech3::form_shown(&f));
  mech3::mods_edit_begin("mine", nullptr);
  mech3::mission_settings_open(0);
  EXPECT(mech3::form_shown(&f) && f.title == "Mission settings" && f.fields.front().label == "Mission: c2 3");
  EXPECT(f.field("salvage chance") && f.field("salvage chance")->text->empty());
  EXPECT(f.fields.back().label.rfind("Now: 30 in 100 (theirs)", 0) == 0);
  for (const char* bad : {"101", "lots", "-5", "7.5"}) {
    *f.field("salvage chance")->text = bad;
    EXPECT(f.press("OK") && mech3::form_shown(&f) && f.fields.back().label.find("0 to 100") != std::string::npos);
  }
  std::ifstream same(mod / "mod.txt", std::ios::binary);
  EXPECT(std::string((std::istreambuf_iterator<char>(same)), std::istreambuf_iterator<char>()) == "name=Mine\nbase=mw3\n");
  *f.field("salvage chance")->text = " 85 ";
  EXPECT(f.press("OK") && !mech3::form_shown(&f) && mech3::mission_settings_last() == "mission_salvage=c2 3 85");
  auto text = [](const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  };
  EXPECT(text(mod / "mod.txt") == "name=Mine\nbase=mw3\nmission_salvage=c2 3 85\n" && text(mod / "mod.txt.bak") == "name=Mine\nbase=mw3\n");
  // The next start: the mission's own number over the other mod's, whichever is ticked first.
  mech3::mods_mount(wd, {"theirs", "mine"}, {MW3_GAME_DIR});
  std::string from;
  EXPECT(mech3::salvage_chance_for("c2", 3, &from) == 85 && from == "mine" && mech3::salvage_chance_for("c2", 2) == 30);
  // Opened again: its own value shown; emptied: the line taken out.
  mech3::mission_settings_open(0);
  EXPECT(*f.field("salvage chance")->text == "85" && f.fields.back().label.rfind("Now: 85 in 100 (mine)", 0) == 0);
  f.field("salvage chance")->text->clear();
  EXPECT(f.press("OK") && !mech3::form_shown(&f) && mech3::mission_settings_last() == "removed");
  EXPECT(text(mod / "mod.txt") == "name=Mine\nbase=mw3\n");
  mech3::mods_edit_end();
  // A mod the editor did not make: refused on the form, its mod.txt as it was.
  mech3::mods_edit_begin("theirs", nullptr);
  mech3::mission_settings_open(0);
  *f.field("salvage chance")->text = "5";
  EXPECT(f.press("OK") && mech3::form_shown(&f) && f.fields.back().label.find("never written") != std::string::npos);
  EXPECT(text(other / "mod.txt") == "name=Theirs\nsalvage_chance=30\n" && !fs::exists(other / "mod.txt.bak"));
  f.press("Cancel");
  mech3::mods_edit_end();
  mech3::missions().current = was.current, mech3::missions().number = was.number;
  zutl::mounts() = mounts;
}
