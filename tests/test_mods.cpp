// Mods (mech3/mods, docs/modding.md): mod.txt, the folders found, the mounts
// in mw3.cfg's order, the player's own files never served from a mod, a
// mod's loose .zrd over its archive's entry, nothing changed without mods.
#include <filesystem>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/mech_names.h"
#include "mech3/mission.h"
#include "mech3/mods.h"
#include "mech3/mod_rows_file.h"
#include "mech3/mech_config.h"
#include "mech3/pilot.h"
#include "mech3/mech_reader.h"

namespace {
namespace fs = std::filesystem;
struct SavedMounts {
  zutl::Mounts saved = zutl::mounts();
  ~SavedMounts() { zutl::mounts() = saved; }
};
void put(const fs::path& p, const std::string& text) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream(p) << text;
}
// The same file, whatever the separators (Windows' test folder has \\).
bool same(const std::string& a, const std::string& b) {
  std::error_code ec;
  return !a.empty() && fs::equivalent(a, b, ec);
}
// A mod's mech files as the mount looks for them: a loose dfn_<name>.zrd for
// each row and a mechlib.zbd holding mech_<model>.flt for each model (a
// mod's mechlib is the whole archive: the first mod's hides the others').
void mech_files(const std::string& mod, std::initializer_list<const char*> dfns, std::initializer_list<const char*> models) {
  for (const char* d : dfns) put(mod + "/zbd/dfn_" + d + ".zrd", "(version (2))\n");
  if (models.size() == 0) return;
  std::error_code ec;
  fs::create_directories(mod + "/zbd", ec);
  zutl_zar::Archive a;
  zutl_zar::init(a);
  if (zutl_zar::create(a, (mod + "/zbd/mechlib.zbd").c_str()))
    for (const char* m : models) zutl_zar::add(a, ("mech_" + std::string(m) + ".flt").c_str(), "x", 1);
  zutl_zar::close(a);
  zutl_zar::destroy(a);
}
std::string tmp(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
}  // namespace

TEST(mods_mod_txt) {
  mech3::ModInfo a;
  EXPECT(mech3::mod_parse("# a comment\nname = Hard Rain \nversion=2\r\nauthor=Me\nfor=mw3pm 1.0\nunknown=1\nnoequals\n", &a));
  EXPECT(a.name == "Hard Rain" && a.version == "2" && a.author == "Me" && a.game == "mw3pm 1.0" && a.error.empty());
  mech3::ModInfo b;
  EXPECT(!mech3::mod_parse("version=1\n", &b) && !b.error.empty());
  mech3::ModInfo c;
  EXPECT(!mech3::mod_parse("name=X\nfor=mw3 1.2\n", &c) && c.error.find("mw3 1.2") != std::string::npos);  // MechWarrior 3's
  mech3::ModInfo d;
  EXPECT(mech3::mod_parse("name=X\nfor=MW3PM\n", &d));
  mech3::ModInfo e;  // MechWarrior 3's mod: its first word is not "mw3pm"
  EXPECT(!mech3::mod_parse("name=X\nfor=mw3 1.2\n", &e) && e.error == "it is for mw3 1.2, not Pirate's Moon");
}

TEST(mods_mount_in_order_and_game_data_only) {
  SavedMounts keep;
  const std::string root = tmp("mods_mount");
  const std::string wd = root + "/wd", install = root + "/install", patch = wd + "/patch12";
  put(install + "/zbd/a.zbd", "install");
  put(install + "/zbd/b.zbd", "install");
  put(wd + "/mods/one/mod.txt", "name=One\n");
  put(wd + "/mods/one/zbd/a.zbd", "one");
  put(wd + "/mods/one/zbd/new.zbd", "one");
  put(wd + "/mods/one/pilots/me.pil", "one");
  put(wd + "/mods/one/mw3.cfg", "install=/elsewhere\n");
  put(wd + "/mods/two/mod.txt", "name=Two\nversion=3\n");
  put(wd + "/mods/two/zbd/a.zbd", "two");
  put(wd + "/mods/two/zbd/b.zbd", "two");
  put(wd + "/mods/bad/readme.txt", "no mod.txt");
  put(wd + "/pilots/keep.pil", "player");
  const auto found = mech3::mods_found(wd);
  EXPECT(found.size() == 3 && found[0].folder == "bad" && !found[0].error.empty() && found[1].name == "One" &&
         found[2].version == "3");
  // Without mods: nothing mounted beyond the install.
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  EXPECT(mech3::mods_mount(wd, {}, {patch, install}).empty());
  zutl::mount(install);
  EXPECT(zutl::mounts().mods.empty() && zutl::mounts().read.size() == 1);
  EXPECT(zutl::find("zbd\\a.zbd") == install + "/zbd/a.zbd");
  // two, then one (mw3.cfg's order), the unusable and unknown left out.
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  const auto log = mech3::mods_mount(wd, {"two", "one", "bad", "none", "two"}, {patch, install});
  zutl::mount(install);
  std::string text;
  for (const std::string& l : log) text += l + "\n";
  std::printf("%s", text.c_str());
  EXPECT(zutl::mounts().mods.size() == 2 && zutl::mounts().read.size() == 3);
  EXPECT(same(zutl::find("zbd\\a.zbd"), wd + "/mods/two/zbd/a.zbd"));     // the first named wins
  EXPECT(same(zutl::find("zbd\\b.zbd"), wd + "/mods/two/zbd/b.zbd"));
  EXPECT(same(zutl::find("zbd\\new.zbd"), wd + "/mods/one/zbd/new.zbd"));  // added
  EXPECT(zutl::find("pilots\\me.pil").empty());                        // a mod serves no pilot
  EXPECT(zutl::find("pilots\\keep.pil") == wd + "/pilots/keep.pil");   // the player's own
  EXPECT(zutl::find("mw3.cfg").empty() || zutl::find("mw3.cfg").find("/mods/") == std::string::npos);
  EXPECT(zutl::list("pilots\\*.pil") == std::vector<std::string>{"keep.pil"});
  EXPECT(text.find("zbd/a.zbd replaces the game's") != std::string::npos && text.find("zbd/new.zbd added") != std::string::npos &&
         text.find("pilots/me.pil: the player's own kind of file, ignored") != std::string::npos &&
         text.find("mw3.cfg: the player's own") != std::string::npos && text.find("mod bad: no mod.txt") != std::string::npos &&
         text.find("mod none: not in") != std::string::npos);
  // Writes go to the write dir, never into a mod or the install.
  EXPECT(zutl::write_path("zbd\\a.zbd") == wd + "/zbd/a.zbd");
  EXPECT(mech3::mods_found(wd).size() == 3);
}

TEST(mods_loose_zrd_beats_the_archives_entry) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/reader.zbd", ec)) return;
  SavedMounts keep;
  const std::string wd = tmp("mods_loose");
  put(wd + "/mods/fmvmod/mod.txt", "name=FMV\n");
  put(wd + "/mods/fmvmod/zbd/fmv.zrd", "(MODDED 1)\n");
  auto marked = [&]() {
    zrdr::init();
    EXPECT(zrdr::open_archive("zbd\\reader.zbd", true));
    zrdr::Node* n = zrdr::load("fmv.zrd");
    const bool mod = n && zrdr::find(n, "MODDED") != nullptr;
    const bool intro = n && zrdr::find(n, "INTRO") != nullptr;
    if (n) zrdr::free_node(n);
    zrdr::close_archives(true);
    return mod ? 1 : intro ? 0 : -1;
  };
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  zutl::mount(install);
  EXPECT(marked() == 0);  // no mods: the archive's own
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"fmvmod"}, {install});
  zutl::mount(install);
  EXPECT(marked() == 1);  // the mod's loose copy instead
  // A broken loose copy: the archive's entry after all.
  put(wd + "/mods/fmvmod/zbd/fmv.zrd", "(BROKEN\n");
  EXPECT(marked() == 0);
}

TEST(mods_gameplay_hash_ignores_looks_and_words) {
  const std::string wd = tmp("mods_hash");
  put(wd + "/mods/tex/mod.txt", "name=Textures\n");
  put(wd + "/mods/tex/zbd/rmechtex.zbd", "pixels");
  put(wd + "/mods/tex/zbd/sounds.zbd", "noise");
  put(wd + "/mods/tex/strings.txt", "1234=Hallo\n");
  put(wd + "/mods/tex/music/track02.ogg", "music");
  put(wd + "/mods/world/mod.txt", "name=World\n");
  put(wd + "/mods/world/zbd/c1/gamez.zbd", "world");
  put(wd + "/mods/world2/mod.txt", "name=World 2\n");
  put(wd + "/mods/world2/zbd/c1/gamez.zbd", "other world");
  EXPECT(mech3::mods_gameplay_hash(wd, {}) == 0);
  EXPECT(mech3::mods_gameplay_hash(wd, {"tex"}) == 0);  // looks and words only
  const uint64_t a = mech3::mods_gameplay_hash(wd, {"world"}), b = mech3::mods_gameplay_hash(wd, {"world2"});
  EXPECT(a != 0 && b != 0 && a != b);
  EXPECT(mech3::mods_gameplay_hash(wd, {"tex", "world"}) == a);
  EXPECT(mech3::mods_gameplay_hash(wd, {"world", "world2"}) != mech3::mods_gameplay_hash(wd, {"world2", "world"}));
  EXPECT(mech3::mod_file_is_gameplay("zbd/c1/reader.zbd") && mech3::mod_file_is_gameplay("zbd/fmv.zrd") &&
         !mech3::mod_file_is_gameplay("zbd/rtexture.zbd") && !mech3::mod_file_is_gameplay("pilots/x.pil") &&
         !mech3::mod_file_is_gameplay("video/intro.m1v"));
}

// Mods' mechs (item 1 of the modding cut): mech= rows parsed, added from 26
// in the mods' order (25 stays Pirate's Moon's "no mech", 0x19), bad or taken names refused with the
// reason; the rows count toward the gameplay hash as data (their spacing,
// comments and titles do not), a mod of rows alone changes gameplay.
TEST(mods_mech_rows_after_the_exes_and_in_the_gameplay_hash) {
  mech3::ModMech r;
  std::string why;
  EXPECT(mech3::mech_row_parse("madcat_b tech=clan mass=75 class=heavy model=madcat title=Mad Cat  B", &r, &why));
  EXPECT(r.name == "madcat_b" && r.model == "madcat" && r.title == "Mad Cat  B" && r.tech == mech3::kClan && r.mass == 75 &&
         r.weight_class == 2);
  EXPECT(mech3::mech_row_parse("scout tech=sphere mass=20 class=light", &r, &why) && r.model == "scout" && r.title == "scout");
  EXPECT(!mech3::mech_row_parse("scout tech=sphere mass=22 class=light", &r, &why) && why.find("steps of 5") != std::string::npos);
  EXPECT(!mech3::mech_row_parse("scout tech=pirate mass=20 class=light", &r, &why) && why.find("tech=") != std::string::npos);
  EXPECT(!mech3::mech_row_parse("scout tech=any mass=20 class=huge", &r, &why) && why.find("class=") != std::string::npos);
  EXPECT(!mech3::mech_row_parse("scout tech=any mass=20 class=light speed=9", &r, &why) && why == "unknown key speed");
  EXPECT(!mech3::mech_row_parse("sc/out tech=any mass=20 class=light", &r, &why));
  SavedMounts keep;
  const std::string wd = tmp("mods_mechs");
  put(wd + "/mods/a/mod.txt", "name=A\nmech=madcat_b tech=clan mass=75 class=heavy model=madcat title=Mad Cat B\n"
                              "mech=thor tech=clan mass=70 class=heavy\nmech=bad tech=clan mass=7 class=heavy\n");
  put(wd + "/mods/b/mod.txt", "name=B\nmech=MADCAT_B tech=any mass=20 class=light\nmech=scout tech=sphere mass=20 class=light\n"
                              "mech=ghost tech=sphere mass=20 class=light\nmech=shade tech=sphere mass=20 class=light model=nothing\n");
  mech_files(wd + "/mods/a", {"madcat_b"}, {"madcat", "scout"});
  mech_files(wd + "/mods/b", {"MADCAT_B", "scout", "shade"}, {});
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mech_rows_clear();
  std::string text;
  for (const std::string& l : mech3::mods_mount(wd, {"a", "b"}, {})) text += l + "\n";
  EXPECT(mech3::mech_count() == 28);
  EXPECT(mech3::mech_index("MadCat_B") == 26 && mech3::mech_index("scout") == 27 && mech3::mech_index("madcat") == mech3::kMadCat);
  EXPECT(std::string(mech3::mech_name(26)) == "madcat_b" && std::string(mech3::mech_model_name(26)) == "madcat" &&
         std::string(mech3::mech_model_name(mech3::kMadCat)) == "madcat" && std::string(mech3::mech_title(26)) == "Mad Cat B" &&
         mech3::mech_title(mech3::kMadCat) == nullptr);
  EXPECT(mech3::max_mass(26) == 75.0f && mech3::weight_class(26) == 2 && mech3::mech_tech(26) == mech3::kClan &&
         mech3::internal_points(26, 1) == 23 && mech3::mech_name_message(26) == -1);
  EXPECT(mech3::max_mass(27) == 20.0f && mech3::mech_tech(27) == mech3::kInnerSphere && std::string(mech3::mech_name(28)) == "notmech");
  // 25 (0x19) is still no mech with rows there: its name, no row, no index, no
  // definition, and a pilot's stock adds none of it (0x00471c10).
  EXPECT(std::string(mech3::mech_name(25)) == "notmech" && mech3::mech_title(25) == nullptr && mech3::mech_index("notmech") == -1);
  EXPECT(mech3::mech_dfn(25) == nullptr && mech3::max_mass(25) == 10.0f && mech3::mech_name_message(25) == -1);
  {
    mech3::Store stock;
    EXPECT(stock.add_mech(25) == nullptr && stock.add_mech(-1) == nullptr && stock.add_mech(26) != nullptr);
  }
  {  // the lab's whole stock: Pirate's Moon's 25 (the Elemental too), then the mods', never 25
    mech3::Store all;
    all.fill_all();
    int mechs = 0, none = 0, mods = 0, elemental = 0;
    for (const mech3::ConfigItem* it : all.items)
      if (it->kind() == 0)
        ++mechs, none += it->id() == 25, mods += it->id() == 26 || it->id() == 27, elemental += it->id() == mech3::kElemental;
    EXPECT(mechs == 27 && none == 0 && mods == 2 && elemental == 1);
  }
  EXPECT(text.find("mech thor tech=clan mass=70 class=heavy: there is a mech thor already, left out") != std::string::npos);
  EXPECT(text.find("mech bad tech=clan mass=7 class=heavy: mass= must be 10 to 100 tons in steps of 5, left out") != std::string::npos);
  EXPECT(text.find("mech MADCAT_B tech=any mass=20 class=light: there is a mech MADCAT_B already, left out") != std::string::npos);
  EXPECT(text.find("mech scout added (mech 27, 20 t, the models of scout)") != std::string::npos);
  // A row whose definition or model the game would not find: left out, said.
  EXPECT(text.find("mech ghost tech=sphere mass=20 class=light: no dfn_ghost.zrd in the reader archive, left out") != std::string::npos);
  EXPECT(text.find("mech shade tech=sphere mass=20 class=light model=nothing: no mech_nothing.flt in mechlib.zbd, left out") !=
         std::string::npos);
  mech3::mech_rows_clear();
  EXPECT(mech3::mech_count() == 25 && mech3::mech_index("scout") == -1);
  // The gameplay hash: the rows as data.
  put(wd + "/mods/rows/mod.txt", "name=Rows\nmech=scout tech=sphere mass=20 class=light title=Scout\n");
  put(wd + "/mods/rows2/mod.txt", "# the same row\nname=Rows 2\n  mech = scout  tech=sphere   mass=20 class=light title=Another\n");
  put(wd + "/mods/rows3/mod.txt", "name=Rows 3\nmech=scout tech=sphere mass=25 class=light title=Scout\n");
  put(wd + "/mods/none/mod.txt", "name=None\ndescription=mech=scout\n");
  const uint64_t h = mech3::mods_gameplay_hash(wd, {"rows"});
  EXPECT(h != 0 && mech3::mods_gameplay_hash(wd, {"rows2"}) == h && mech3::mods_gameplay_hash(wd, {"rows3"}) != h);
  EXPECT(mech3::mods_gameplay_hash(wd, {"none"}) == 0);
  mech3::mods_identity_set(wd, {"none", "rows"});
  EXPECT(mech3::mods_identity().hash == h && mech3::mods_identity().names == std::vector<std::string>{"Rows"});
  mech3::mods_identity() = mech3::ModsIdentity{};
}

// Mods' weapons (item 2): weapon= rows after Pirate's Moon's 56 (56 on, the
// bug fixes on or off), their ammo and techs, the PPC (8) Pirate's Moon's own
// either way; a taken name (the PPC's too) refused; a remount starts from
// the exe's tables; the rows in the gameplay hash.
TEST(mods_weapon_rows_after_the_ppcs_place) {
  mech3::ModWeapon r;
  std::string why;
  EXPECT(mech3::weapon_row_parse("Laser_Huge ammo=no tech=sphere title=Huge  Laser", &r, &why) && r.name == "Laser_Huge" &&
         !r.ammo && r.tech == mech3::kInnerSphere && r.title == "Huge  Laser");
  EXPECT(mech3::weapon_row_parse("Rail ammo=yes tech=none", &r, &why) && r.ammo && r.tech == 0 && r.title == "Rail");
  EXPECT(!mech3::weapon_row_parse("Rail ammo=maybe tech=none", &r, &why) && why == "ammo= must be yes or no");
  EXPECT(!mech3::weapon_row_parse("Rail ammo=yes tech=clan heat=3", &r, &why) && why == "unknown key heat");
  SavedMounts keep;
  const zfix::Options saved = zfix::options();
  const std::string wd = tmp("mods_weapons");
  put(wd + "/mods/w/mod.txt", "name=W\nweapon=Laser_Huge ammo=no tech=sphere title=Huge Laser\nweapon=Rail ammo=yes tech=clan\n"
                              "weapon=ppc ammo=no tech=any\nweapon=AC2 ammo=yes tech=any\n");
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  std::string text;
  for (const std::string& l : mech3::mods_mount(wd, {"w"}, {})) text += l + "\n";
  EXPECT(text.find("weapon Laser_Huge added (weapon 56)") != std::string::npos &&
         text.find("weapon Rail added (weapon 57)") != std::string::npos);
  EXPECT(text.find("weapon ppc ammo=no tech=any: there is a weapon ppc already, left out") != std::string::npos);
  EXPECT(text.find("weapon AC2 ammo=yes tech=any: there is a weapon AC2 already, left out") != std::string::npos);
  for (bool bugs : {true, false}) {
    zfix::options().bugs = bugs;
    EXPECT(mech3::weapon_count() == 58);
    EXPECT(mech3::weapon_index("laser_huge") == 56 && mech3::weapon_index("Rail") == 57 && mech3::weapon_index("AC2") == 0);
    EXPECT(std::string(mech3::weapon_name(57)) == "Rail" && std::string(mech3::weapon_title(56)) == "Huge Laser" &&
           mech3::weapon_title(0) == nullptr && mech3::weapon_name_message(56) == -1);
    EXPECT(!mech3::weapon_uses_ammo(56) && mech3::weapon_uses_ammo(57) && mech3::weapon_has_tech(56, mech3::kInnerSphere) &&
           !mech3::weapon_has_tech(56, mech3::kClan) && mech3::weapon_has_tech(57, mech3::kClan));
    // The plain PPC: Pirate's Moon's own (8), with the fixes on or off.
    EXPECT(mech3::weapon_index("PPC") == 8 && std::string(mech3::weapon_name(8)) == "PPC" && mech3::weapon_name_message(8) == 0x2fd);
  }
  // The lab's whole stock (0x00471f40's) with a row there: the row's weapon (and Rail's ammo).
  zfix::options().bugs = false;
  {
    mech3::Store all;
    all.fill_all();
    int huge = 0, rail_ammo = 0;
    for (const mech3::ConfigItem* it : all.items) {
      huge += it->kind() == 1 && it->id() == 56;
      rail_ammo += it->kind() == 2 && it->id() == 57;
    }
    EXPECT(huge == 1 && rail_ammo == 1);
  }
  zfix::options() = saved;
  // A remount: the exe's tables, then the rows again (not refused as taken).
  put(wd + "/mods/m/mod.txt", "name=M\nmech=scout tech=sphere mass=20 class=light\n");
  mech_files(wd + "/mods/m", {"scout"}, {"scout"});
  text.clear();
  for (const std::string& l : mech3::mods_mount(wd, {"m", "w"}, {})) text += l + "\n";
  EXPECT(text.find("mech scout added (mech 26") != std::string::npos && text.find("weapon Rail added (weapon 57)") != std::string::npos);
  for (const std::string& l : mech3::mods_mount(wd, {}, {})) text += l + "\n";
  EXPECT(mech3::mech_count() == 25 && mech3::weapon_count() == 56 && mech3::weapon_index("Rail") == -1);
  // The gameplay hash: the weapon rows as data.
  put(wd + "/mods/w2/mod.txt", "name=W2\nweapon=Rail ammo=yes tech=clan title=Other\n");
  put(wd + "/mods/w3/mod.txt", "name=W3\nweapon=Rail ammo=no tech=clan\n");
  const uint64_t h = mech3::mods_gameplay_hash(wd, {"w2"});
  EXPECT(h != 0 && mech3::mods_gameplay_hash(wd, {"w3"}) != h);
}

// Saves that hold a mod's mechs and weapons (item 3): a pilot's file and a
// loadout file keep the numbers and name the rows in <file>.rows; with the
// two mods swapped round they load with the same mechs and weapons; with
// the mod off, refused by name and left as they were; with no sidecar, a
// number past the table read as it is; a line that does not read refused; a
// sidecar ignored by a file with no rows, and deleted when one is written
// with none; the pilot's rename and delete carry it.
TEST(mods_saves_name_their_rows_beside_them) {
  SavedMounts keep;
  const std::string wd = tmp("mods_rows_files");
  put(wd + "/mods/a/mod.txt", "name=A\nmech=alpha tech=clan mass=50 class=medium\nweapon=Gun_A ammo=yes tech=any\n");
  put(wd + "/mods/b/mod.txt", "name=B\nmech=beta tech=clan mass=60 class=heavy\nweapon=Gun_B ammo=no tech=any\n");
  mech_files(wd + "/mods/a", {"alpha"}, {"alpha", "beta"});
  mech_files(wd + "/mods/b", {"beta"}, {"alpha", "beta"});
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  auto mount = [&](std::vector<std::string> order) { mech3::mods_mount(wd, order, {}); };
  auto file = [&](const std::string& rel) {
    std::ifstream f(wd + "/" + rel, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return f ? ss.str() : std::string("(none)");
  };
  auto drop = [](mech3::Pilot* p) {  // out of the list, not written: as a new start would find the file
    mech3::pilots().remove(p);
    p->dirty = 0;
    delete p;
  };
  mount({"a", "b"});  // alpha 26, beta 27, Gun_A 56, Gun_B 57
  EXPECT(mech3::mech_index("alpha") == 26 && mech3::mech_index("beta") == 27 && mech3::weapon_index("Gun_B") == 57);
  // A pilot: its lab game's slot on beta with Gun_B fitted, Gun_A and alpha in its stock.
  mech3::Pilot* p = mech3::create_pilot("Rows");
  EXPECT(p != nullptr);
  p->select_game(mech3::kMechLab);
  {
    auto* c = new mech3::Config(27, "mine");  // the slot's own (its dtor deletes it)
    auto* w = new mech3::WeaponItem;
    w->count = 1, w->weapon = 57, w->tech = 2;
    c->items.push_back(w);
    delete p->current->slot.config;
    p->current->slot.config = c;
    p->current->slot.mech = 27;
  }
  for (mech3::ConfigItem* it : p->store()->items) delete it;  // a stock of its own (the lab's whole one saves empty)
  p->store()->items.clear();
  p->store()->full = 0;
  auto* gun = new mech3::WeaponItem;
  gun->count = 1, gun->weapon = 56, gun->tech = 1;
  p->store()->items.push_back(gun);
  p->store()->add_mech(26);
  EXPECT(p->save());
  EXPECT(file("pilots/Rows.mw3.rows").find("mech 27 beta") != std::string::npos &&
         file("pilots/Rows.mw3.rows").find("weapon 57 Gun_B") != std::string::npos &&
         file("pilots/Rows.mw3.rows").find("mech 26 alpha") != std::string::npos);
  drop(p);
  // A loadout file of beta's with Gun_A.
  {
    mech3::ConfigList l(27, 0);
    auto* c = new mech3::Config(27, "beta_gun");
    c->player_made = 1;
    auto* w = new mech3::WeaponItem;
    w->count = 1, w->weapon = 56, w->tech = 1;
    c->items.push_back(w);
    l.configs.push_back(c);
    EXPECT(l.save_all());
    l.configs.clear();
    delete c;
  }
  EXPECT(file("configs/beta.mw3.rows").find("weapon 56 Gun_A") != std::string::npos);
  // The mods swapped round: beta 26, alpha 27, Gun_B 56, Gun_A 57; the same mechs and weapons.
  mount({"b", "a"});
  EXPECT(mech3::mech_index("beta") == 26 && mech3::weapon_index("Gun_A") == 57);
  p = mech3::load_pilot("Rows", false);
  EXPECT(p != nullptr);
  if (p) {
    p->select_game(mech3::kMechLab);
    const mech3::GameSlot& s = p->current->slot;
    int32_t fitted = -1, stocked = -1, mech_stock = -1;
    if (s.config && !s.config->items.empty()) fitted = s.config->items.front()->id();
    for (const mech3::ConfigItem* it : p->store()->items) {
      if (it->kind() == 1) stocked = it->id();
      if (it->kind() == 0) mech_stock = it->id();
    }
    std::printf("  swapped: slot mech %d (%s), its loadout's mech %d, fitted %d, stock weapon %d, stock mech %d\n", s.mech,
                mech3::mech_name(s.mech), s.config ? s.config->mech : -9, fitted, stocked, mech_stock);
    EXPECT(s.mech == 26 && s.config && s.config->mech == 26 && fitted == 56 && stocked == 57 && mech_stock == 27);
    drop(p);
  }
  {
    mech3::ConfigList l(mech3::mech_index("beta"), 0);
    EXPECT(l.load_saved() && l.configs.size() == 1 && !l.keep_file);
    EXPECT(!l.configs.empty() && l.configs[0]->items.front()->id() == 57 && l.configs[0]->mech == 26);
    for (mech3::Config* c : l.configs) delete c;
    l.configs.clear();
  }
  // Mod b off: beta and Gun_B gone; the pilot refused by name, its file left alone.
  const std::string before = file("pilots/Rows.mw3");
  mech3::rows_file::refusals().clear();
  mount({"a"});
  EXPECT(mech3::load_pilot("Rows", false) == nullptr && mech3::find_pilot("Rows") == nullptr);
  EXPECT(file("pilots/Rows.mw3") == before && file("pilots/Rows.mw3.rows").find("beta") != std::string::npos);
  EXPECT(!mech3::rows_file::refusals().empty() &&
         mech3::rows_file::refusals()[0].find("the mod mech beta, which no enabled mod adds") != std::string::npos);
  // beta's loadout file: alpha's mod alone has no beta, so no list asks for it;
  // with b back and a off, Gun_A is refused and the file kept.
  mount({"b"});
  {
    mech3::ConfigList l(mech3::mech_index("beta"), 0);
    EXPECT(l.load_saved() && l.configs.empty() && l.keep_file);
    EXPECT(!l.save_all() && file("configs/beta.mw3.rows").find("Gun_A") != std::string::npos);
  }
  // No sidecar and a row number past the table: read as it is, as the exe reads it (test_pilot's damaged files).
  mount({"a", "b"});
  std::error_code ec;
  fs::rename(wd + "/pilots/Rows.mw3.rows", wd + "/pilots/Rows.mw3.rows.kept", ec);
  mount({"a"});  // 26 alpha only; the file's 27 is past the table
  mech3::rows_file::refusals().clear();
  p = mech3::load_pilot("Rows", false);
  EXPECT(p != nullptr && mech3::rows_file::refusals().empty());
  if (p) {  // saved again: still no sidecar (27 is no mod's row here), the number kept
    EXPECT(p->save() && file("pilots/Rows.mw3.rows") == "(none)");
    drop(p);
  }
  // A line that does not read: refused, never the raw numbers.
  mount({"a", "b"});
  put(wd + "/pilots/Rows.mw3.rows", "mech 27 beta\nweapon fiftyseven Gun_B\nmech 26 alpha\n");
  mech3::rows_file::refusals().clear();
  EXPECT(mech3::load_pilot("Rows", false) == nullptr);
  EXPECT(!mech3::rows_file::refusals().empty() && mech3::rows_file::refusals()[0].find("does not read at line 2") != std::string::npos);
  fs::rename(wd + "/pilots/Rows.mw3.rows.kept", wd + "/pilots/Rows.mw3.rows", ec);
  // Rename and delete carry the sidecar; a rewrite with no rows deletes it.
  p = mech3::load_pilot("Rows", false);
  EXPECT(p != nullptr);
  if (p) {
    EXPECT(p->rename("Rows2"));
    EXPECT(file("pilots/Rows.mw3.rows") == "(none)" && file("pilots/Rows2.mw3.rows").find("mech 27 beta") != std::string::npos);
    p->select_game(mech3::kMechLab);
    p->current->slot.mech = 3;
    delete p->current->slot.config;
    p->current->slot.config = nullptr;
    for (mech3::ConfigItem* it : p->store()->items) delete it;
    p->store()->items.clear();
    EXPECT(p->save() && file("pilots/Rows2.mw3.rows") == "(none)");
    p->current->slot.mech = 26;
    EXPECT(p->save() && file("pilots/Rows2.mw3.rows") != "(none)");
    EXPECT(mech3::delete_pilot("Rows2") && file("pilots/Rows2.mw3") == "(none)" && file("pilots/Rows2.mw3.rows") == "(none)");
  }
  // A sidecar beside a file with no rows is ignored (a stale one from elsewhere).
  p = mech3::create_pilot("Plain");
  EXPECT(p && p->save());
  put(wd + "/pilots/Plain.mw3.rows", "nonsense\n");
  drop(p);
  EXPECT(mech3::load_pilot("Plain", false) != nullptr);
  while (!mech3::pilots().empty()) mech3::delete_pilot(mech3::pilots().front()->name);
  mech3::mods_mount(wd, {}, {});
}

// The launcher's MODS list (item 5, its model): the enabled mods first in
// mw3.cfg's order, the rest by folder name; a folder that is no usable mod
// listed with why; an enabled mod's files and rows an earlier enabled one
// holds, with that mod named; the check's count on request; the new order
// written as mw3.cfg's mod= lines at the old ones' place, every other line kept.
TEST(mods_list_for_the_launcher) {
  const std::string wd = tmp("mods_list");
  put(wd + "/mods/y/mod.txt", "name=Y\nmech=scout tech=sphere mass=20 class=light\n");
  put(wd + "/mods/y/zbd/a.zbd", "y's");
  put(wd + "/mods/x/mod.txt", "name=X\nmech=Scout tech=clan mass=25 class=light\nweapon=Rail ammo=yes tech=any\n");
  put(wd + "/mods/x/zbd/A.zbd", "x's");
  put(wd + "/mods/x/zbd/b.zbd", "x's own");
  put(wd + "/mods/z/mod.txt", "name=Z\n");
  put(wd + "/mods/z/zbd/b.zbd", "z's");
  put(wd + "/mods/broken/zbd/c.zbd", "no mod.txt");
  put(wd + "/mods/pm/mod.txt", "name=P\nfor=mw3 1.2\n");  // MechWarrior 3's
  put(wd + "/mw3.cfg", "install=/somewhere\nmod=y\nmod=x\nwidemenus=1\nmod=gone\n");
  EXPECT((mech3::mods_enabled(wd) == std::vector<std::string>{"y", "x", "gone"}));
  const std::vector<mech3::ModsListEntry> l = mech3::mods_list(wd, mech3::mods_enabled(wd));
  std::string order;
  for (const mech3::ModsListEntry& e : l) order += e.info.folder + (e.on ? "+ " : " ");
  std::printf("  list: %s\n", order.c_str());
  EXPECT(order == "y+ x+ broken pm z ");
  if (l.size() == 5) {
    EXPECT(l[0].shadowed.empty() && l[0].conflicts_with.empty());
    EXPECT((l[1].shadowed == std::vector<std::string>{"zbd/a.zbd"}) && (l[1].rows_taken == std::vector<std::string>{"mech scout"}) &&
           (l[1].conflicts_with == std::vector<std::string>{"y"}));
    EXPECT(l[2].info.error == "no mod.txt" && l[3].info.error.find("not Pirate's Moon") != std::string::npos);
    EXPECT(!l[4].on && l[4].shadowed.empty() && l[0].check_errors == -1);  // z is off: nothing shared counted
  }
  // z on after x: its b.zbd is x's.
  const std::vector<mech3::ModsListEntry> l2 = mech3::mods_list(wd, {"x", "z"}, true, "");
  EXPECT(l2.size() == 5 && (l2[1].shadowed == std::vector<std::string>{"zbd/b.zbd"}) &&
         (l2[1].conflicts_with == std::vector<std::string>{"x"}) && l2[0].check_errors >= 0 && l2[1].check_errors >= 0);
  // Saved: mod= lines at the first old one's place, the rest kept.
  EXPECT(mech3::mods_save_enabled(wd, {"x", "z"}));
  std::ifstream f(wd + "/mw3.cfg");
  std::stringstream ss;
  ss << f.rdbuf();
  EXPECT(ss.str() == "install=/somewhere\nmod=x\nmod=z\nwidemenus=1\n");
  EXPECT(mech3::mods_save_enabled(wd, {}) && mech3::mods_enabled(wd).empty());
  // The write goes through mw3.cfg.new renamed over it: when that cannot be
  // written (a folder in its place here, a full disk in play) the save fails
  // and mw3.cfg is left whole.
  EXPECT(mech3::mods_save_enabled(wd, {"x"}));
  std::error_code ec;
  fs::create_directories(wd + "/mw3.cfg.new", ec);
  EXPECT(!mech3::mods_save_enabled(wd, {"z"}));
  EXPECT((mech3::mods_enabled(wd) == std::vector<std::string>{"x"}) && fs::is_directory(wd + "/mw3.cfg.new", ec));  // not ours: kept
  fs::remove(wd + "/mw3.cfg.new", ec);
  EXPECT(mech3::mods_save_enabled(wd, {"z"}) && (mech3::mods_enabled(wd) == std::vector<std::string>{"z"}) &&
         !fs::exists(wd + "/mw3.cfg.new", ec));
}

TEST(mods_strings_over_the_message_table) {
  SavedMounts keep;
  const std::string wd = tmp("mods_strings");
  put(wd + "/mods/fr/mod.txt", "name=French\n");
  put(wd + "/mods/fr/strings.txt",
      "\xef\xbb\xbf# French\n100=Campagne\n0x65=D\xc3\xa9marrer\nID_QUIT=Quitter\nnot a line\n999999999999=x\n"
      "102=Two\\nlines \\\\ here\n103=\xe2\x98\x83 snowman\n");
  put(wd + "/mods/en/mod.txt", "name=English fixes\n");
  put(wd + "/mods/en/strings.txt", "100=Campaign!\n104=Only here\n");
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"fr", "en"}, {});
  std::map<int32_t, std::string> text = {{100, "CAMPAIGN"}, {101, "START"}, {105, "QUIT"}, {106, "kept"}};
  const std::map<std::string, int32_t> ids = {{"ID_QUIT", 105}};
  std::string log;
  for (const std::string& l : mech3::mods_apply_strings(text, ids)) log += l + "\n";
  std::printf("%s", log.c_str());
  EXPECT(text[100] == "Campagne");                   // the first mod's wins
  EXPECT(text[101] == "D\xe9marrer");                // hex id, UTF-8 to Windows-1252
  EXPECT(text[105] == "Quitter");                    // by the message's name
  EXPECT(text[102] == "Two\r\nlines \\ here");       // \n and \\ .
  EXPECT(text.count(103) == 0);                      // a snowman the game cannot show: left out
  EXPECT(text[104] == "Only here" && text[106] == "kept");
  EXPECT(log.find("line 5: no id= here") != std::string::npos && log.find("line 6: no id= here") != std::string::npos &&
         log.find("line 8: a character the game cannot show") != std::string::npos);
  // No mods: nothing changes.
  zutl::mounts() = zutl::Mounts{};
  std::map<int32_t, std::string> same = {{100, "CAMPAIGN"}};
  EXPECT(mech3::mods_apply_strings(same, ids).empty() && same[100] == "CAMPAIGN");
}

TEST(mods_worlds_declared) {
  mech3::ModInfo m;
  EXPECT(mech3::mod_parse("name=W\nworld=mymap\nworld=other\nia_world=mymap  My  Map \nia_world=c2\n", &m));
  EXPECT((m.worlds == std::vector<std::string>{"mymap", "other"}) && m.ia_worlds.size() == 2 &&
         m.ia_worlds[0].first == "mymap" && m.ia_worlds[0].second == "My  Map" && m.ia_worlds[1].second.empty());
  SavedMounts keep;
  const std::string wd = tmp("mods_worlds");
  put(wd + "/mods/w/mod.txt", "name=W\nworld=good\nworld=nofiles\nworld=../evil\nworld=T1\nia_world=good Good\n"
                              "ia_world=c3\nia_world=nowhere Lost\n");
  put(wd + "/mods/w/zbd/good/gamez.zbd", "x");
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mod_worlds().clear(), mech3::mod_ia_worlds().clear();
  std::string log;
  for (const std::string& l : mech3::mods_mount(wd, {"w"}, {})) log += l + "\n";
  EXPECT(mech3::mod_worlds() == std::vector<std::string>{"good"} && mech3::mission_dir_count() == 10 &&
         std::string(mech3::mission_dir(9)) == "good" && std::string(mech3::mission_dir(10)) == "none");
  EXPECT(mech3::mod_ia_worlds().size() == 2 && mech3::mod_ia_worlds()[0].name == "Good" && mech3::mod_ia_worlds()[1].name == "c3");
  EXPECT(log.find("world nofiles: no zbd/nofiles/gamez.zbd") != std::string::npos &&
         log.find("world ../evil: no zbd") != std::string::npos && log.find("world T1: there is a world T1 already") != std::string::npos &&
         log.find("Instant Action world nowhere is no world") != std::string::npos);
  mech3::mod_worlds().clear(), mech3::mod_ia_worlds().clear();
  EXPECT(mech3::mission_dir_count() == 9 && std::string(mech3::mission_dir(9)) == "none");
}

// M3: a reader archive's broken entries are refused, not read on into the
// next ones or allocated as their counts say; good ones read as before.
TEST(mods_broken_reader_entries_are_refused) {
  SavedMounts keep;
  const std::string dir = tmp("mods_broken_zrd");
  const std::string path = dir + "/broken.zbd";
  auto le = [](std::vector<uint8_t>& v, int32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(x >> (8 * i)));
  };
  // (list of 3: int 7, string "ok")
  std::vector<uint8_t> good;
  le(good, 4), le(good, 3), le(good, 1), le(good, 7), le(good, 3), le(good, 2), good.push_back('o'), good.push_back('k');
  std::vector<uint8_t> huge;  // a list claiming a billion children
  le(huge, 4), le(huge, 1000000000), le(huge, 1), le(huge, 7);
  std::vector<uint8_t> type;  // a node of type 9
  le(type, 4), le(type, 2), le(type, 9), le(type, 0);
  std::vector<uint8_t> deep;  // 100 nested lists of 2
  for (int i = 0; i < 100; ++i) le(deep, 4), le(deep, 2);
  le(deep, 1), le(deep, 0);
  std::vector<uint8_t> past;  // a list of 3 holding one int: the rest is the next entry's
  le(past, 4), le(past, 3), le(past, 1), le(past, 5);
  zutl_zar::Archive a;
  zutl_zar::init(a);
  EXPECT(zutl_zar::create(a, path.c_str()));
  zutl_zar::add(a, "good.zrd", good.data(), static_cast<uint32_t>(good.size()));
  zutl_zar::add(a, "huge.zrd", huge.data(), static_cast<uint32_t>(huge.size()));
  zutl_zar::add(a, "type.zrd", type.data(), static_cast<uint32_t>(type.size()));
  zutl_zar::add(a, "deep.zrd", deep.data(), static_cast<uint32_t>(deep.size()));
  zutl_zar::add(a, "past.zrd", past.data(), static_cast<uint32_t>(past.size()));
  zutl_zar::add(a, "after.zrd", good.data(), static_cast<uint32_t>(good.size()));
  EXPECT(zutl_zar::close(a));
  zutl_zar::destroy(a);
  zutl::mounts() = zutl::Mounts{};
  zrdr::init();
  EXPECT(zrdr::open_archive(path.c_str(), true));
  zrdr::Node* g = zrdr::load("good.zrd");
  EXPECT(g && zrdr::list_count(g) == 3 && g->list[1].i == 7 && std::string(g->list[2].s) == "ok");
  if (g) zrdr::free_node(g);
  for (const char* bad : {"huge.zrd", "type.zrd", "deep.zrd", "past.zrd"}) {
    zrdr::Node* n = zrdr::load(bad);
    std::printf("  %s: %s\n", bad, n ? "read" : "refused");
    EXPECT(n == nullptr);
    if (n) zrdr::free_node(n);
  }
  zrdr::Node* after = zrdr::load("after.zrd");  // the reader is fine after them
  EXPECT(after && zrdr::list_count(after) == 3);
  if (after) zrdr::free_node(after);
  zrdr::close_archives(true);
}

// A world's own files in a mod, in a folder past the 127 characters zEffect's path
// holds: none is a zar archive, and each once came back refused as one.
TEST(mods_check_mod_reads_animations_at_any_depth) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/c1/anim.zbd", ec)) return;
  SavedMounts keep;
  std::string mod = tmp("mods_long");
  while (mod.size() < 200) mod += "/a_folder_with_a_long_name";
  put(mod + "/mod.txt", "name=Deep\nversion=1\n");
  fs::create_directories(mod + "/zbd/c1", ec);
  for (const char* f : {"anim.zbd", "gamez.zbd", "texture2.zbd"}) {
    fs::copy_file(install + "/zbd/c1/" + f, mod + "/zbd/c1/" + f, ec);
    EXPECT(!ec);
  }
  int errors = -1;
  std::string text;
  for (const std::string& l : mech3::mod_check(mod, install, &errors)) text += l + "\n";
  EXPECT(text.find("animations read") != std::string::npos);
  EXPECT(errors == 0);
}

// M3: mw3 --check-mod's report on a good mod and on one with every kind of
// fault the game would refuse.
TEST(mods_check_mod_reports_what_the_game_would_refuse) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  SavedMounts keep;
  const std::string root = tmp("mods_check");
  // Good: a world reader copied from the install, a loose .zrd, strings.
  const std::string good = root + "/good";
  put(good + "/mod.txt", "name=Good\nversion=1\nia_world=c1 Again\n");
  put(good + "/zbd/fmv.zrd", "(INTRO (PLAYAVI intro))\n");
  put(good + "/strings.txt", "100=Hello\n");
  if (fs::exists(install + "/zbd/c1/readerm1.zbd", ec)) {
    fs::create_directories(good + "/zbd/c1", ec);
    fs::copy_file(install + "/zbd/c1/readerm1.zbd", good + "/zbd/c1/readerm1.zbd", ec);
  }
  int errors = -1;
  std::string text;
  for (const std::string& l : mech3::mod_check(good, install, &errors)) text += l + "\n";
  std::printf("%s", text.c_str());
  EXPECT(errors == 0 && text.find("mod.txt: Good 1") != std::string::npos && text.find("zbd/fmv.zrd: added") != std::string::npos);
  if (fs::exists(install + "/zbd/c1/readerm1.zbd", ec))
    EXPECT(text.find("zbd/c1/readerm1.zbd: replaces the game's") != std::string::npos && text.find("0 errors") != std::string::npos);
  // Bad: no name, a world without files, an unknown Instant Action world, a
  // garbage archive, a reader archive with a broken entry, a broken .zrd.
  const std::string bad = root + "/bad";
  put(bad + "/mod.txt", "version=1\nworld=nofiles\nia_world=nowhere\n");
  put(bad + "/zbd/rimage.zbd", "not an archive at all");
  put(bad + "/zbd/broken.zrd", "(UNCLOSED\n");
  put(bad + "/pilots/x.pil", "x");
  {
    std::vector<uint8_t> huge = {4, 0, 0, 0, 0, 0xca, 0x9a, 0x3b};  // a list of a billion
    zutl_zar::Archive a;
    zutl_zar::init(a);
    fs::create_directories(bad + "/zbd/c1", ec);
    EXPECT(zutl_zar::create(a, (bad + "/zbd/c1/readerm1.zbd").c_str()));
    zutl_zar::add(a, "mission.zrd", huge.data(), static_cast<uint32_t>(huge.size()));
    EXPECT(zutl_zar::close(a));
    zutl_zar::destroy(a);
  }
  {  // a mechlib whose materials table is cut short, a motion of 4 bytes
    zutl_zar::Archive a;
    zutl_zar::init(a);
    EXPECT(zutl_zar::create(a, (bad + "/zbd/mechlib.zbd").c_str()));
    const uint32_t version = 27, format = 1, count = 5;
    zutl_zar::add(a, "version", &version, 4);
    zutl_zar::add(a, "format", &format, 4);
    zutl_zar::add(a, "materials", &count, 4);
    EXPECT(zutl_zar::close(a));
    zutl_zar::destroy(a);
    zutl_zar::init(a);
    EXPECT(zutl_zar::create(a, (bad + "/zbd/motion.zbd").c_str()));
    zutl_zar::add(a, "walk", &count, 4);
    EXPECT(zutl_zar::close(a));
    zutl_zar::destroy(a);
  }
  text.clear();
  for (const std::string& l : mech3::mod_check(bad, install, &errors)) text += l + "\n";
  std::printf("%s", text.c_str());
  EXPECT(errors == 8 && text.find("error: zbd/mechlib.zbd: its version, format or materials cannot be read") != std::string::npos &&
         text.find("error: zbd/motion.zbd: motion walk cannot be read") != std::string::npos);
  EXPECT(text.find("error: mod.txt: mod.txt has no name= line") != std::string::npos &&
         text.find("error: world nofiles: no zbd/nofiles/gamez.zbd") != std::string::npos &&
         text.find("error: ia_world nowhere: no such world") != std::string::npos &&
         text.find("error: zbd/rimage.zbd: not a texture library the game can read") != std::string::npos &&
         text.find("error: zbd/c1/readerm1.zbd: entry mission.zrd cannot be read") != std::string::npos &&
         text.find("error: zbd/broken.zrd: not a text .zrd the reader can read") != std::string::npos &&
         text.find("pilots/x.pil: the player's own kind of file") != std::string::npos);
  EXPECT(mech3::mod_check(root + "/none", install, &errors).size() == 1 && errors == 1);
  // An install overlay: no mod.txt is a note, not an error; its files are still checked.
  const std::string overlay = root + "/overlay";
  put(overlay + "/zbd/fmv.zrd", "(INTRO (PLAYAVI intro))\n");
  put(overlay + "/zbd/broken.zrd", "(UNCLOSED\n");
  text.clear();
  for (const std::string& l : mech3::mod_check(overlay, install, &errors)) text += l + "\n";
  std::printf("%s", text.c_str());
  EXPECT(errors == 1 && text.find("no mod.txt: checked as an install overlay") != std::string::npos &&
         text.find("error: no mod.txt") == std::string::npos && text.find("zbd/fmv.zrd: added") != std::string::npos &&
         text.find("error: zbd/broken.zrd: not a text .zrd the reader can read") != std::string::npos);
}
