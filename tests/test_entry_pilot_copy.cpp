// Mods by campaign: a main-menu pilot copied into a campaign entry
// (upgrades/mods/entry_pilot_copy, docs/mods-campaigns-plan.md, Pilots and
// saves). The originals are only read; nothing in the entry is replaced; a
// copy that fails part-way leaves nothing of its own.
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "mech3/mod_campaign.h"
#include "upgrades/mods/entry_pilot_copy.h"

namespace {
namespace fs = std::filesystem;
std::string tmp(const char* name) {
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
std::string text_of(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
// Every file under `dir` with its bytes and its time: the originals must come out the same.
std::map<std::string, std::string> snapshot(const fs::path& dir) {
  std::map<std::string, std::string> out;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec))
      out[fs::relative(it->path(), dir).generic_string()] =
          text_of(it->path()) + "@" + std::to_string(static_cast<long long>(fs::last_write_time(it->path(), ec).time_since_epoch().count()));
  return out;
}
// A write dir with two pilots (Bob with two saves and a rows file, Bob2 whose
// saves must not go with Bob's), loadouts for two mechs, and an entry folder.
struct Layout {
  std::string wd, entry, configs;
};
Layout layout(const char* name, bool marker = true) {
  Layout l;
  l.wd = tmp(name), l.entry = l.wd + "/campaigns/mw3", l.configs = l.entry + "/configs";
  put(fs::path(l.wd) / "pilots/Bob.mw3", "bob's file");
  put(fs::path(l.wd) / "pilots/Bob.mw3.rows", "bob's rows");
  put(fs::path(l.wd) / "pilots/games/Bob0102.mw3", "bob c1 m2");
  put(fs::path(l.wd) / "pilots/games/Bob0103.mw3", "bob c1 m3");
  put(fs::path(l.wd) / "pilots/games/Bob0103.mw3.rows", "bob c1 m3 rows");
  put(fs::path(l.wd) / "pilots/Bob2.mw3", "bob2's file");
  put(fs::path(l.wd) / "pilots/games/Bob20102.mw3", "bob2 c1 m2");
  put(fs::path(l.wd) / "pilots/games/Bob010203.mw3", "not a save of Bob's: six digits");
  put(fs::path(l.wd) / "pilots/notes.txt", "not a pilot");
  put(fs::path(l.wd) / "configs/Bushwacker.mw3", "bushwacker loadouts");
  put(fs::path(l.wd) / "configs/Annihilator.mw3", "annihilator loadouts");
  put(fs::path(l.wd) / "configs/Annihilator.mw3.rows", "annihilator rows");
  std::error_code ec;
  fs::create_directories(l.entry + "/pilots/games", ec);
  if (marker) put(fs::path(l.entry) / mech3::kCampaignMarker, "ours\n");
  return l;
}
}  // namespace

TEST(entry_pilot_copy_copies_the_pilot_its_saves_and_the_loadouts) {
  const Layout l = layout("pilot_copy");
  const std::vector<std::string> names = mech3::main_menu_pilots(l.wd);
  EXPECT(names.size() == 2 && names[0] == "Bob" && names[1] == "Bob2");
  const auto before = snapshot(fs::path(l.wd) / "pilots"), configs_before = snapshot(fs::path(l.wd) / "configs");
  const mech3::PilotCopy r = mech3::entry_copy_pilot(l.wd, "bob", l.entry, l.configs);  // any letter case
  std::printf("  copied %zu files, %d loadout files kept: %s\n", r.copied.size(), r.configs_kept, r.why.c_str());
  EXPECT(r.ok && r.why.empty() && r.copied.size() == 8 && r.configs_kept == 0);
  const fs::path e = l.entry;
  EXPECT(text_of(e / "pilots/Bob.mw3") == "bob's file" && text_of(e / "pilots/Bob.mw3.rows") == "bob's rows");
  EXPECT(text_of(e / "pilots/games/Bob0102.mw3") == "bob c1 m2" && text_of(e / "pilots/games/Bob0103.mw3") == "bob c1 m3");
  EXPECT(text_of(e / "pilots/games/Bob0103.mw3.rows") == "bob c1 m3 rows");
  EXPECT(text_of(e / "configs/Bushwacker.mw3") == "bushwacker loadouts" && text_of(e / "configs/Annihilator.mw3.rows") == "annihilator rows");
  // Only Bob's: not Bob2, its save, the six-digit file or the notes.
  std::error_code ec;
  EXPECT(!fs::exists(e / "pilots/Bob2.mw3", ec) && !fs::exists(e / "pilots/games/Bob20102.mw3", ec));
  EXPECT(!fs::exists(e / "pilots/games/Bob010203.mw3", ec) && !fs::exists(e / "pilots/notes.txt", ec));
  // The originals as they were, to the byte and the time.
  EXPECT(snapshot(fs::path(l.wd) / "pilots") == before && snapshot(fs::path(l.wd) / "configs") == configs_before);
  // Again: refused by the name, the entry's files untouched.
  put(e / "pilots/Bob.mw3", "played on in the entry");
  const auto entry_before = snapshot(e);
  const mech3::PilotCopy again = mech3::entry_copy_pilot(l.wd, "Bob", l.entry, l.configs);
  EXPECT(!again.ok && again.copied.empty() && again.why.find("already has a pilot named Bob") != std::string::npos);
  EXPECT(snapshot(e) == entry_before);
  // The second pilot: its own files; the loadouts the entry has now are kept, not replaced.
  put(e / "configs/Bushwacker.mw3", "the entry's own loadouts");
  const mech3::PilotCopy two = mech3::entry_copy_pilot(l.wd, "Bob2", l.entry, l.configs);
  EXPECT(two.ok && two.copied.size() == 2 && two.configs_kept == 2);
  EXPECT(text_of(e / "configs/Bushwacker.mw3") == "the entry's own loadouts" && text_of(e / "pilots/Bob.mw3") == "played on in the entry");
  EXPECT(snapshot(fs::path(l.wd) / "pilots") == before && snapshot(fs::path(l.wd) / "configs") == configs_before);
}

TEST(entry_pilot_copy_refuses_and_writes_nothing) {
  std::error_code ec;
  {  // no marker: not a folder of ours
    const Layout l = layout("pilot_copy_no_marker", false);
    const mech3::PilotCopy r = mech3::entry_copy_pilot(l.wd, "Bob", l.entry, l.configs);
    EXPECT(!r.ok && r.why.find(mech3::kCampaignMarker) != std::string::npos);
    EXPECT(fs::is_empty(fs::path(l.entry) / "pilots/games", ec) && !fs::exists(l.configs, ec));
  }
  const Layout l = layout("pilot_copy_refused");
  const auto pilots_before = snapshot(fs::path(l.wd) / "pilots");
  const auto entry_before = snapshot(l.entry);
  for (const char* bad : {"", ".", "..", "../Bob", "games/Bob0102", "a\\b", "C:Bob", "Nobody"}) {
    const mech3::PilotCopy r = mech3::entry_copy_pilot(l.wd, bad, l.entry, l.configs);
    EXPECT(!r.ok && !r.why.empty() && r.copied.empty());
  }
  EXPECT(snapshot(l.entry) == entry_before && !fs::exists(l.configs, ec));
  // A saved game of that name already in the entry (its pilot file gone): refused, the save kept.
  put(fs::path(l.entry) / "pilots/games/BOB0104.mw3", "the entry's own save");
  const mech3::PilotCopy r = mech3::entry_copy_pilot(l.wd, "Bob", l.entry, l.configs);
  EXPECT(!r.ok && r.why.find("saved game") != std::string::npos);
  EXPECT(text_of(fs::path(l.entry) / "pilots/games/BOB0104.mw3") == "the entry's own save" && !fs::exists(fs::path(l.entry) / "pilots/Bob.mw3", ec));
  EXPECT(snapshot(fs::path(l.wd) / "pilots") == pilots_before);
}

// A copy of the pilot's own files that cannot finish (a save's .rows meets a
// folder of that name in the entry) removes the files and folders it made,
// and nothing else: for the pilot and its saves any clash refuses everything.
TEST(entry_pilot_copy_part_way_leaves_only_what_was_there) {
  const Layout l = layout("pilot_copy_part");
  std::error_code ec;
  put(fs::path(l.entry) / "pilots/Old.mw3", "a pilot the entry had");
  put(fs::path(l.entry) / "pilots/games/Bob0103.mw3.rows/keep.txt", "the player's");  // a folder in the way of the last save's rows
  const auto before = snapshot(l.entry), originals = snapshot(fs::path(l.wd) / "pilots");
  const mech3::PilotCopy r = mech3::entry_copy_pilot(l.wd, "Bob", l.entry, l.configs);
  std::printf("  part-way: %s\n", r.why.c_str());
  EXPECT(!r.ok && r.copied.empty() && r.why.find("nothing was copied") != std::string::npos);
  EXPECT(snapshot(l.entry) == before);  // Bob's file and the saves copied before the failure are gone again
  EXPECT(!fs::exists(fs::path(l.entry) / "pilots/Bob.mw3", ec) && !fs::exists(l.configs, ec));
  EXPECT(fs::is_directory(fs::path(l.entry) / "pilots/games", ec) && fs::is_regular_file(fs::path(l.entry) / mech3::kCampaignMarker, ec));
  EXPECT(snapshot(fs::path(l.wd) / "pilots") == originals);
#if !defined(_WIN32)
  // With no pilots folder in the entry yet: the folders the call made go too when it fails (a save
  // that cannot be read), and stay when it works.
  const Layout m = layout("pilot_copy_fresh");
  fs::remove_all(fs::path(m.entry) / "pilots", ec);  // the test's own folder
  const fs::path save = fs::path(m.wd) / "pilots/games/Bob0103.mw3";
  fs::permissions(save, fs::perms::none, ec);
  const bool unreadable = !std::ifstream(save).good();  // (not so for root)
  const mech3::PilotCopy f = mech3::entry_copy_pilot(m.wd, "Bob", m.entry, m.configs);
  fs::permissions(save, fs::perms::owner_read | fs::perms::owner_write, ec);
  if (unreadable) EXPECT(!f.ok && !fs::exists(fs::path(m.entry) / "pilots", ec) && !fs::exists(m.configs, ec));
  if (unreadable) EXPECT(text_of(save) == "bob c1 m3");
  const mech3::PilotCopy g = mech3::entry_copy_pilot(m.wd, "Bob", m.entry, m.configs);
  EXPECT(g.ok && fs::is_regular_file(fs::path(m.entry) / "pilots/games/Bob0103.mw3", ec) && fs::is_regular_file(fs::path(m.configs) / "Bushwacker.mw3", ec));
#endif
}

// The loadouts are not so strict (lead, 2026-11-10: a player should not lose
// the pilot's copy to one stray file among the saved mechs): a mech whose
// place in the entry is taken by a stray .rows, or by a folder of its name,
// is skipped and counted; the pilot, its saves and the other loadouts copy.
TEST(entry_pilot_copy_skips_a_loadout_whose_place_is_taken) {
  const Layout l = layout("pilot_copy_skip");
  std::error_code ec;
  put(fs::path(l.configs) / "Annihilator.mw3.rows", "a stray rows file, its loadouts gone");
  put(fs::path(l.wd) / "configs/Zeus.mw3", "zeus loadouts");
  put(fs::path(l.configs) / "Zeus.mw3/keep.txt", "a folder of that name");
  put(fs::path(l.wd) / "configs/Thor.mw3", "thor loadouts");
  put(fs::path(l.configs) / "thor.MW3", "the entry's own thor");
  const mech3::PilotCopy r = mech3::entry_copy_pilot(l.wd, "Bob", l.entry, l.configs);
  std::printf("  skip: ok %d, %zu copied, %d kept, %d skipped %s\n", r.ok, r.copied.size(), r.configs_kept, r.configs_skipped, r.why.c_str());
  EXPECT(r.ok && r.configs_skipped == 2 && r.configs_kept == 1 && r.copied.size() == 6);  // the pilot's 5 and the Bushwacker
  const fs::path e = l.entry;
  EXPECT(text_of(e / "pilots/Bob.mw3") == "bob's file" && text_of(e / "pilots/games/Bob0103.mw3") == "bob c1 m3");
  EXPECT(text_of(e / "configs/Bushwacker.mw3") == "bushwacker loadouts");
  // The stray files as they were; no loadout written beside them.
  EXPECT(text_of(e / "configs/Annihilator.mw3.rows") == "a stray rows file, its loadouts gone" && !fs::exists(e / "configs/Annihilator.mw3", ec));
  EXPECT(fs::is_directory(e / "configs/Zeus.mw3", ec) && text_of(e / "configs/Zeus.mw3/keep.txt") == "a folder of that name");
  EXPECT(text_of(e / "configs/thor.MW3") == "the entry's own thor");
}
