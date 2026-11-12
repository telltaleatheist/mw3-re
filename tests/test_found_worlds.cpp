// A world the game does not have (docs/modding.md; upgrades/mods/found_worlds.h, playtest Z2): found by its
// files, written into the mod.txt IMPORT makes (world=, ia_world=, a campaign of its missions), mounted as a
// world with its campaign and Instant Action entry. The test's worlds are made in its own folders: the game's
// c1 files copied under a new name, as a zStudio rebuild of a world would be. Nothing of a player's touched.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/mission.h"
#include "mech3/mod_campaign.h"
#include "mech3/mod_install.h"
#include "mech3/mods.h"
#include "upgrades/mods/found_worlds.h"
#include "upgrades/mods/import_folder.h"
#include "upgrades/mods/mod_report.h"

#if MW3_HAVE_LIBARCHIVE
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <archive.h>
#include <archive_entry.h>
#endif

namespace {
namespace fs = std::filesystem;

void put(const fs::path& p, const std::string& text) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream(p, std::ios::binary) << text;
}
std::string tmp(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
std::string text(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
bool have_c1() { return fs::is_regular_file(std::string(MW3_GAME_DIR) + "/zbd/c1/gamez.zbd"); }
// The made-up world: c1's own archives copied under `world` (its missions 1 to 3, an Instant Action map and a
// multiplayer map, its textures; no anim.zbd: its script, "source c1.gs", takes c1's).
void make_world(const fs::path& zbd, const std::string& world) {
  std::error_code ec;
  fs::create_directories(zbd / world, ec);
  for (const char* f : {"gamez.zbd", "reader.zbd", "rtexture.zbd", "readerm1.zbd", "readerm2.zbd", "readerm3.zbd", "readeria1.zbd", "readermp1.zbd"})
    fs::copy_file(fs::path(MW3_GAME_DIR) / "zbd" / "c1" / f, zbd / world / f, ec);
}
std::string joined(const std::vector<std::string>& v) {
  std::string s;
  for (const std::string& l : v) s += l + "\n";
  return s;
}
struct KeepMounts {
  zutl::Mounts saved = zutl::mounts();
  std::vector<std::string> worlds = mech3::mod_worlds();
  std::vector<mech3::ModIaWorld> ia = mech3::mod_ia_worlds();
  std::vector<mech3::ModCampaign> campaigns = mech3::mod_campaigns();
  ~KeepMounts() {
    zutl::mounts() = saved, mech3::mod_worlds() = worlds, mech3::mod_ia_worlds() = ia, mech3::mod_campaigns() = campaigns;
  }
};
#if MW3_HAVE_LIBARCHIVE
// A zip of every file under `dir`, at `inside/` in it.
void zip_folder(const std::string& zip, const fs::path& dir, const std::string& inside) {
  archive* a = archive_write_new();
  archive_write_set_format_zip(a);
  archive_write_open_filename(a, zip.c_str());
  std::error_code ec;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec)) continue;
    const std::string data = text(it->path());
    archive_entry* e = archive_entry_new();
    archive_entry_set_pathname(e, (inside + "/" + fs::relative(it->path(), dir, ec).generic_string()).c_str());
    archive_entry_set_filetype(e, AE_IFREG);
    archive_entry_set_size(e, static_cast<la_int64_t>(data.size()));
    archive_entry_set_perm(e, 0644);
    archive_write_header(a, e);
    archive_write_data(a, data.data(), data.size());
    archive_entry_free(e);
  }
  archive_write_close(a);
  archive_write_free(a);
}
#endif
}  // namespace

// Found by its files: a folder none of the game's nine (any case) with a gamez.zbd; its numbered archives in
// number order; its script at the root or in the mod's interp.zbd; what it lacks.
TEST(found_worlds_by_their_files) {
  const std::string m = tmp("found_worlds_files");
  for (const char* f : {"zbd/Iron_Coast/gamez.zbd", "zbd/Iron_Coast/readerm10.zbd", "zbd/Iron_Coast/readerm2.zbd", "zbd/Iron_Coast/ReaderM1.zbd",
                        "zbd/Iron_Coast/readeria1.zbd", "zbd/Iron_Coast/readermp3.zbd", "zbd/Iron_Coast/readermx.zbd", "zbd/Iron_Coast/anim.zbd",
                        "zbd/C1/gamez.zbd", "zbd/c4B/gamez.zbd", "zbd/no.world/gamez.zbd", "zbd/empty/readerm1.zbd", "zbd/json/gamez/metadata.json",
                        "Iron_Coast.gs"})
    put(fs::path(m) / f, "x");
  const std::vector<mech3::FoundWorld> w = mech3::found_worlds(m);
  std::string got;
  for (const auto& f : w) got += f.folder + " ";
  std::printf("  found: %s\n", got.c_str());
  EXPECT(got == "Iron_Coast json ");  // not C1, c4B (the game's), no.world (a dot), empty (no gamez)
  EXPECT(w.size() == 2 && w[0].missions == std::vector<int>({1, 2, 10}) && w[0].ia_maps == std::vector<int>({1}) &&
         w[0].mp_maps == std::vector<int>({3}));
  EXPECT(w.size() == 2 && w[0].script_root && !w[0].script_archive && w[0].lacks == std::vector<std::string>({"reader.zbd", "rtexture.zbd"}));
  EXPECT(w.size() == 2 && !w[1].script_root && w[1].missions.empty() && w[1].lacks.size() == 3);
  const std::vector<std::string> s = mech3::found_world_sentences(w[0]);
  std::printf("%s", joined(s).c_str());
  EXPECT(s.size() == 2 && s[0] == "The world Iron_Coast has 3 missions, 1 Instant Action map and 1 multiplayer map." &&
         s[1] == "It has no reader.zbd (its world records (world.zrd)) and rtexture.zbd (its textures).");
  const std::vector<std::string> j = mech3::found_world_sentences(w[1]);
  EXPECT(j.size() == 3 && j[0] == "The world json has no missions or maps of its own." &&
         j[1] == "Its build script json.gs is in neither the mod's zbd\\interp.zbd nor its folder: nothing on it can load.");
  EXPECT(mech3::found_worlds(m + "/nothing").empty() && mech3::game_world_name("DEV2") && !mech3::game_world_name("c5"));
  // A script that takes another world's scenery: "set DB c2" (or "source c2.gs"); then anim.zbd is not its own to have.
  put(m + "/json.gs", "# a world on c2's scenery\nset   DB c2\n");
  const std::vector<mech3::FoundWorld> d = mech3::found_worlds(m);
  EXPECT(d.size() == 2 && d[1].reads_from == "c2" && d[1].lacks == std::vector<std::string>({"reader.zbd", "rtexture.zbd"}));
  const std::vector<std::string> ds = d.size() == 2 ? mech3::found_world_sentences(d[1]) : std::vector<std::string>{};
  EXPECT(ds.size() == 3 && ds[1] == "Its script takes c2's scenery (its gamez.zbd and animations); its missions, world records and textures are its own." &&
         ds[2] == "It has no reader.zbd (its world records (world.zrd)) and rtexture.zbd (its textures).");
  put(m + "/json.gs", "source C2.gs\n");
  EXPECT(mech3::found_worlds(m)[1].reads_from == "C2");
  put(m + "/json.gs", "set DB json\n");  // its own
  EXPECT(mech3::found_worlds(m)[1].reads_from.empty());
}

// interp.zbd's directory: the game's own lists its 44 scripts; a file that is not one, or whose count it cannot
// hold, lists none. A mod's interp.zbd holding <world>.gs is the world's script.
TEST(found_worlds_script_in_the_mods_interp_zbd) {
  if (!have_c1()) return;
  const std::vector<std::string> n = mech3::interp_archive_names(std::string(MW3_GAME_DIR) + "/zbd/interp.zbd");
  std::printf("  %zu scripts, the first %s\n", n.size(), n.empty() ? "-" : n[0].c_str());
  EXPECT(n.size() == 44 && std::find(n.begin(), n.end(), "c1.gs") != n.end() && std::find(n.begin(), n.end(), "c4b.gs") != n.end());
  const std::string m = tmp("found_worlds_interp");
  put(m + "/junk.zbd", "not an archive at all");
  EXPECT(mech3::interp_archive_names(m + "/junk.zbd").empty() && mech3::interp_archive_names(m + "/none.zbd").empty());
  {  // the signature, a version, a count of a million: the file cannot hold it
    std::ofstream f(m + "/big.zbd", std::ios::binary);
    const uint32_t head[3] = {0x08971119u, 7, 1000000};
    f.write(reinterpret_cast<const char*>(head), sizeof head);
  }
  EXPECT(mech3::interp_archive_names(m + "/big.zbd").empty());
  // The world's script inside the mod's interp.zbd (the game's archive renamed in: it names c1.gs).
  put(m + "/zbd/c1x/gamez.zbd", "x");
  std::error_code ec;
  fs::copy_file(std::string(MW3_GAME_DIR) + "/zbd/interp.zbd", m + "/zbd/interp.zbd", ec);
  put(m + "/zbd/c1/gamez.zbd", "x");
  std::vector<mech3::FoundWorld> w = mech3::found_worlds(m);
  EXPECT(w.size() == 1 && w[0].folder == "c1x" && !w[0].script_archive && !w[0].script_root);
  const std::vector<std::vector<std::string>> c1 = mech3::interp_archive_script(m + "/zbd/interp.zbd", "C1.GS");
  EXPECT(!c1.empty() && c1[0] == std::vector<std::string>({"set", "DB", "c1"}) && mech3::interp_archive_script(m + "/zbd/interp.zbd", "c9.gs").empty());
  // A zStudio export's interp.zbd holds the new world's script by its name; here the test's own archive of one entry.
  {
    std::ofstream f(m + "/zbd/interp.zbd", std::ios::binary | std::ios::trunc);
    const uint32_t head[3] = {0x08971119u, 7, 1};
    f.write(reinterpret_cast<const char*>(head), sizeof head);
    char entry[128] = "C1X.gs";
    f.write(entry, sizeof entry);
  }
  w = mech3::found_worlds(m);
  EXPECT(w.size() == 1 && w[0].script_archive);
}

// The lines IMPORT writes: world=, ia_world= when it has Instant Action maps, and a campaign file of its missions
// in number order, named after the world by the campaign's rule, a name no file at the root has.
TEST(found_worlds_mod_txt_lines_and_campaign_file) {
  const std::string m = tmp("found_worlds_lines");
  for (const char* f : {"zbd/iron.coast/gamez.zbd", "zbd/Iron Coast/gamez.zbd", "zbd/Iron-Coast/gamez.zbd", "zbd/Iron-Coast/readerm3.zbd",
                        "zbd/Iron-Coast/readerm1.zbd", "zbd/Iron-Coast/readeria2.zbd", "zbd/bare/gamez.zbd", "zbd/bare/readermp1.zbd"})
    put(fs::path(m) / f, "x");
  put(m + "/IRON-COAST.txt", "the mod's own read-me");  // taken: the campaign file gets another name
  std::vector<std::string> written, log;
  const std::vector<std::string> lines = mech3::found_worlds_mod_txt(m, mech3::found_worlds(m), &written, &log);
  std::printf("%s%s", joined(lines).c_str(), joined(log).c_str());
  EXPECT(joined(lines) == "world=bare\nworld=Iron-Coast\nia_world=Iron-Coast Iron-Coast\ncampaign=Iron-Coast_2.txt\n");
  EXPECT(written == std::vector<std::string>({"Iron-Coast_2.txt"}) && text(m + "/IRON-COAST.txt") == "the mod's own read-me");
  const std::string c = text(m + "/Iron-Coast_2.txt");
  EXPECT(c.find("name=Iron-Coast\nmission=Iron-Coast 1\nmission=Iron-Coast 3\n") != std::string::npos);
  mech3::ModCampaign mc;
  EXPECT(mech3::campaign_load(m, "m", "Iron-Coast_2.txt", &mc) && mc.missions.size() == 2 && mc.missions[1].number == 3 && mc.id == "Iron-Coast_2");
  EXPECT(log == std::vector<std::string>({"world Iron-Coast: campaign Iron-Coast_2.txt of its 2 missions"}));
}

// IMPORT of a new world three ways (a folder, a zip, a game folder a mod was laid over), none with a mod.txt:
// the mod.txt the game writes names the world, its Instant Action map and its campaign; the check and INFO say
// what it found; the mount numbers the world 9 and takes its campaign and Instant Action entry.
TEST(found_worlds_imported_from_a_folder_a_zip_and_a_game_folder) {
  if (!have_c1()) return;
  KeepMounts keep;
  const std::string src = tmp("found_worlds_import_src");
  const fs::path folder = fs::path(src) / "Iron Coast";
  make_world(folder / "zbd", "ironcoast");
  put(folder / "ironcoast.gs", "source c1.gs\n");
  // A game folder a mod was laid over: the player's install (made up, small) and the same with the world in it.
  const std::string inst = tmp("found_worlds_import_inst"), laid = src + "/Laid Over";
  for (const std::string& d : {inst, laid}) put(d + "/Mech3.exe", "the game"), put(d + "/zbd/reader.txt", "the reader");
  make_world(fs::path(laid) / "zbd", "ironcoast");
  put(fs::path(laid) / "ironcoast.gs", "source c1.gs\n");
  std::vector<std::pair<std::string, std::string>> picks = {{"a folder", folder.string()}, {"a game folder", laid}};
#if MW3_HAVE_LIBARCHIVE
  zip_folder(src + "/Iron Coast.zip", folder, "Iron Coast");
  picks.insert(picks.begin() + 1, {"a zip", src + "/Iron Coast.zip"});
#endif
  for (const auto& [kind, pick] : picks) {
    const std::string wd = tmp("found_worlds_import_wd");
    const mech3::ModAddDone d = mech3::import_folder_run(wd, pick, inst, [](const std::string&) { return 1; }, nullptr);
    std::printf("  %s: %s\n", kind.c_str(), d.message.c_str());
    EXPECT(d.ok);
    if (!d.ok) continue;
    const fs::path at = fs::path(wd) / "mods" / d.folder;
    const std::string mt = text(at / "mod.txt");
    std::printf("%s", mt.c_str());
    EXPECT(mt.find("\nworld=ironcoast\nia_world=ironcoast ironcoast\ncampaign=ironcoast.txt\n") != std::string::npos);
    EXPECT(text(at / "ironcoast.txt").find("name=ironcoast\nmission=ironcoast 1\nmission=ironcoast 2\nmission=ironcoast 3\n") != std::string::npos);
    int errors = 0;
    mech3::ModFindings fx;
    const std::vector<std::string> check = mech3::mod_check(at.string(), MW3_GAME_DIR, &errors, &fx);
    const std::string said = joined(mech3::mod_sentences(fx));
    std::printf("%s", said.c_str());
    EXPECT(errors == 0 && fx.worlds == std::vector<std::string>({"ironcoast"}) && fx.campaigns.size() == 1);
    EXPECT(said.find("The world ironcoast has 3 missions, 1 Instant Action map and 1 multiplayer map.\n") != std::string::npos &&
           said.find("Its script takes c1's scenery (its gamez.zbd and animations); its missions, world records and textures are its own.\n") !=
               std::string::npos && said.find("It has no") == std::string::npos);
    // Mounted: world 9, its campaign of three missions, its Instant Action entry.
    zutl::mounts() = zutl::Mounts{};
    zutl::set_write_dir(wd);
    mech3::mod_worlds().clear(), mech3::mod_ia_worlds().clear();
    const std::string log = joined(mech3::mods_mount(wd, {d.folder}, {MW3_GAME_DIR}));
    zutl::mount(MW3_GAME_DIR);
    EXPECT(mech3::mission_dir_count() == 10 && std::string(mech3::mission_dir(9)) == "ironcoast");
    EXPECT(mech3::mod_ia_worlds().size() == 1 && mech3::mod_ia_worlds()[0].folder == "ironcoast");
    EXPECT(mech3::mod_campaigns().size() == 1 && mech3::mod_campaigns()[0].missions.size() == 3 && mech3::mod_campaigns()[0].id == "ironcoast");
    EXPECT(zutl::find("zbd\\ironcoast\\readerm2.zbd").find("/mods/") != std::string::npos);
    if (mech3::mod_campaigns().size() != 1) std::printf("%s", log.c_str());
  }
  // A mod with its own mod.txt is the modder's: never written; a world folder it does not name is a warning.
  const fs::path own = fs::path(src) / "Own";
  make_world(own / "zbd", "ironcoast");
  put(own / "mod.txt", "name=Own\n");
  const std::string wd = tmp("found_worlds_import_wd");
  const mech3::ModAddDone d = mech3::import_folder_run(wd, own.string(), inst, nullptr, nullptr);
  EXPECT(d.ok && text(fs::path(wd) / "mods" / d.folder / "mod.txt") == "name=Own\n");
  int errors = 0;
  const std::string check = joined(mech3::mod_check((fs::path(wd) / "mods" / d.folder).string(), MW3_GAME_DIR, &errors));
  std::printf("%s", check.c_str());
  EXPECT(errors == 0 && check.find("zbd/ironcoast") != std::string::npos && check.find("that mod.txt does not name") != std::string::npos);
}

// The mount leaves out what could not load, and says why: a campaign or Instant Action entry on a mod world with
// no build script anywhere the game reads (the load would abort, and the next world's after it: a crash at the
// launch), and a campaign line whose mission has no readerm<n>.zbd (the launch would give up with nothing said).
TEST(found_worlds_mount_leaves_out_what_cannot_load) {
  if (!have_c1()) return;
  KeepMounts keep;
  const std::string wd = tmp("found_worlds_mount");
  const fs::path mod = fs::path(wd) / "mods" / "coast";
  make_world(mod / "zbd", "coast");
  put(mod / "mod.txt", "name=Coast\nworld=coast\nia_world=coast Coast\ncampaign=coast.txt\ncampaign=far.txt\ncampaign=own.txt\n");
  put(mod / "coast.txt", "name=Coast\nmission=coast 1\nmission=coast 2\n");
  put(mod / "far.txt", "name=Far\nmission=coast 1\nmission=coast 7\n");  // no readerm7
  put(mod / "own.txt", "name=Own\nmission=c1 1\nmission=c1 9\n");        // the game's world, no readerm9
  auto mount = [&] {
    zutl::mounts() = zutl::Mounts{};
    zutl::set_write_dir(wd);
    mech3::mod_worlds().clear(), mech3::mod_ia_worlds().clear(), mech3::mod_campaigns().clear();
    const std::string log = joined(mech3::mods_mount(wd, {"coast"}, {MW3_GAME_DIR}));
    zutl::mount(MW3_GAME_DIR);
    return log;
  };
  auto ids = [] {
    std::string s;
    for (const auto& c : mech3::mod_campaigns()) s += c.id + " ";
    return s;
  };
  std::string log = mount();
  std::printf("%s", log.c_str());
  EXPECT(mech3::mission_dir_count() == 10 && mech3::mod_ia_worlds().empty() && ids().empty());
  EXPECT(log.find("Instant Action world coast: no coast.gs (its build script), left out") != std::string::npos);
  EXPECT(log.find("campaign coast.txt: line 2: its world's build script coast.gs is in no interp.zbd or folder the game reads, the campaign left out") !=
         std::string::npos);
  EXPECT(log.find("campaign own.txt: line 3: there is no zbd/c1/readerm9.zbd (the mission's own archive), the campaign left out") != std::string::npos);
  // Its script at the mod's root: the world's entries played; the line without its archive still left out.
  put(mod / "coast.gs", "source c1.gs\n");
  log = mount();
  std::printf("%s", log.c_str());
  EXPECT(mech3::mod_ia_worlds().size() == 1 && ids() == "coast ");
  EXPECT(log.find("campaign far.txt: line 3: there is no zbd/coast/readerm7.zbd (the mission's own archive), the campaign left out") != std::string::npos);
  // In the mod's interp.zbd instead (a zStudio export's way), by its name in any case.
  fs::remove(mod / "coast.gs");
  {
    std::ofstream f(mod / "zbd" / "interp.zbd", std::ios::binary);
    const uint32_t head[3] = {0x08971119u, 7, 1};
    f.write(reinterpret_cast<const char*>(head), sizeof head);
    char entry[128] = "Coast.GS";
    f.write(entry, sizeof entry);
  }
  log = mount();
  EXPECT(mech3::mod_ia_worlds().size() == 1 && ids() == "coast ");
}
