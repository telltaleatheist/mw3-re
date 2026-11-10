// Mods (mech3/mods, docs/modding.md): mod.txt, the folders found, the mounts
// in mw3.cfg's order, the player's own files never served from a mod, a
// mod's loose .zrd over its archive's entry, nothing changed without mods.
#include <algorithm>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "zip_write.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "gamez/zutil/zutl_zip.h"
#include "gamez/zutil/zutl_json.h"
#include "gamez/zsiscript/si_script.h"
#include "gamez/zclass/cls_mechlib_json.h"
#include "upgrades/json/gamez_json.h"
#include "upgrades/reference/reference.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "upgrades/mods/anim_defs.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zsave/zsave.h"
#include "gamez/zeffect/zeff_anim_save.h"
#include "gamez/zeffect/zeff_anim_json.h"
#include "gamez/zsiscript/si_script_json.h"
#include "mech3/mech_names.h"
#include "mech3/mission.h"
#include "mech3/mods.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/mods_tables.h"
#include "mech3/patch12.h"
#include "mech3/mod_campaign.h"
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
  EXPECT(mech3::mod_parse("# a comment\nname = Hard Rain \nversion=2\r\nauthor=Me\nfor=mw3 1.2\nunknown=1\nnoequals\n", &a));
  EXPECT(a.name == "Hard Rain" && a.version == "2" && a.author == "Me" && a.game == "mw3 1.2" && a.error.empty());
  mech3::ModInfo b;
  EXPECT(!mech3::mod_parse("version=1\n", &b) && !b.error.empty());
  mech3::ModInfo c;
  EXPECT(!mech3::mod_parse("name=X\nfor=pm\n", &c) && c.error.find("pm") != std::string::npos);
  mech3::ModInfo d;
  EXPECT(mech3::mod_parse("name=X\nfor=MW3\n", &d));
  mech3::ModInfo e;  // Pirate's Moon's mod: its first word is not "mw3"
  EXPECT(!mech3::mod_parse("name=X\nfor=mw3pm 1.0\n", &e) && e.error == "it is for mw3pm 1.0, not MechWarrior 3");
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

// A tree as the text reader reads it back (lists, quoted strings, ints,
// floats always with a '.').
std::string zrd_text(const zrdr::Node& n) {
  char b[64];
  switch (n.type) {
    case zrdr::List: {
      std::string s = "(";
      for (int i = 1; i < n.list[0].i; ++i) s += (i > 1 ? " " : "") + zrd_text(n.list[i]);
      return s + ")";
    }
    case zrdr::String: return std::string("\"") + (n.s ? n.s : "") + "\"";
    case zrdr::Float:
      std::snprintf(b, sizeof b, "%.9g", static_cast<double>(n.f));
      if (std::strchr(b, 'e')) std::snprintf(b, sizeof b, "%.9f", static_cast<double>(n.f));
      if (!std::strchr(b, '.')) std::strcat(b, ".0");
      return b;
    default:
      return std::to_string(n.i);
  }
}

// Modder-friendly step 1 (docs/modding.md, Readable files read directly): a
// mod's loose weapons.zrd as text, at the reader archive's own folder, is
// what the game's weapon definitions read (zwep::init), ahead of the entry,
// also when the mod ships its own reader.zbd beside it; with none, the
// archive's; a loose one that does not read is refused (the archive's entry
// read) and --check-mod names its line.
TEST(mods_loose_weapons_zrd_read_directly) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/reader.zbd", ec)) return;
  SavedMounts keep;
  const zwep::InitHooks hooks = zwep::init_hooks();
  zwep::init_hooks() = zwep::InitHooks{};
  const std::string wd = tmp("mods_loose_weapons");
  // The shipped weapons.zrd as text, the PPC's VELOCITY changed.
  zrdr::init();
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  zutl::mount(install);
  EXPECT(zrdr::open_archive("zbd\\reader.zbd", true));
  std::string text;
  if (zrdr::Node* root = zrdr::load("weapons.zrd")) {
    zrdr::Node* v = zrdr::find(zrdr::find(root, "PPC"), "VELOCITY");
    if (v && v->type == zrdr::List && v->list[0].i > 1) v->list[1].f = 1234.5f;
    for (int i = 1; i < root->list[0].i; ++i) text += zrd_text(root->list[i]) + "\n";
    zrdr::free_node(root);
  }
  zrdr::close_archives(true);
  EXPECT(text.find("1234.5") != std::string::npos);
  auto ppc = [&](const std::vector<std::string>& mods) {
    zutl::mounts() = zutl::Mounts{};
    zutl::set_write_dir(wd);
    if (!mods.empty()) mech3::mods_mount(wd, mods, {install});
    zutl::mount(install);
    zrdr::init();
    zrdr::open_archive("zbd\\reader.zbd", true);
    float vel = -1.0f;
    if (zwep::init(nullptr, "weapons.zrd", 0, nullptr) == 0 || zwep::find_def("PPC"))
      if (const zwep::WeaponDef* d = zwep::find_def("PPC")) vel = d->velocity;
    zwep::shutdown(nullptr);
    zrdr::close_archives(true);
    return vel;
  };
  const float shipped = ppc({});
  put(wd + "/mods/loose/mod.txt", "name=Loose\n");
  put(wd + "/mods/loose/zbd/weapons.zrd", "; a modder's edit\n" + text);
  const float loose = ppc({"loose"});
  // A mod with its own copy of reader.zbd and the loose file: the loose one.
  put(wd + "/mods/both/mod.txt", "name=Both\n");
  fs::create_directories(wd + "/mods/both/zbd", ec);
  fs::copy_file(install + "/zbd/reader.zbd", wd + "/mods/both/zbd/reader.zbd", fs::copy_options::overwrite_existing, ec);
  put(wd + "/mods/both/zbd/weapons.zrd", text);
  const float both = ppc({"both"});
  // Broken (a '(' on line 3 never closed): refused, the entry read instead.
  put(wd + "/mods/broken/mod.txt", "name=Broken\n");
  put(wd + "/mods/broken/zbd/weapons.zrd", "; a modder's edit\n" + std::string("; more\n(VERSION (2)\n") + text);
  const float broken = ppc({"broken"});
  int errors = 0;
  std::string report;
  for (const std::string& l : mech3::mod_check(wd + "/mods/broken", install, &errors)) report += l + "\n";
  int clean_errors = 0;
  mech3::mod_check(wd + "/mods/loose", install, &clean_errors);
  std::printf("  PPC velocity: shipped %g, loose %g, both %g, broken %g; check: %d errors\n%s", shipped, loose, both, broken, errors,
              report.c_str());
  EXPECT(shipped > 0.0f && shipped != 1234.5f);
  EXPECT(loose == 1234.5f && both == 1234.5f && broken == shipped);
  EXPECT(errors == 1 && report.find("zbd/weapons.zrd: line 3: a '(' never closed") != std::string::npos);
  EXPECT(clean_errors == 0);
  // The line found for each kind of imbalance.
  auto why = [&](const std::string& t) {
    put(wd + "/t.zrd", t);
    std::FILE* f = std::fopen((wd + "/t.zrd").c_str(), "rb");
    const std::string w = f ? zrdr::text_error(f) : "?";
    if (f) std::fclose(f);
    return w;
  };
  EXPECT(why("(A 1)\n(B 2)\n") == "");
  EXPECT(why("(A 1)\r\n(B 2))\r\n") == "line 2: a ')' with no '(' before it");
  EXPECT(why("(A \"x\n\ny)\n") == "line 1: a quote never closed");
  EXPECT(why("(A 1\n(B 2\n") == "line 1: a '(' never closed");  // the outer one
  EXPECT(why("; (not counted\n(A 1) ; ) nor this\n") == "");
  zwep::init_hooks() = hooks;
}

// A loose dialog.zrd goes through the load hook as the entry does: the
// rebuild's own records (the MODS button, MODSDIALOG; patch12_edits.cpp) are
// in it, so a mod's edited menus keep the MODS screen.
TEST(mods_loose_dialog_zrd_keeps_our_records) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/reader.zbd", ec)) return;
  SavedMounts keep;
  const std::string wd = tmp("mods_loose_dialog");
  auto read = [&](const std::vector<std::string>& mods, bool hook, std::string* text) {
    zutl::mounts() = zutl::Mounts{};
    zutl::set_write_dir(wd);
    if (!mods.empty()) mech3::mods_mount(wd, mods, {install});
    zutl::mount(install);
    if (hook) mech3::patch12::install();
    zrdr::init();
    zrdr::open_archive("zbd\\reader.zbd", true);
    bool ours = false;
    if (zrdr::Node* root = zrdr::load("dialog.zrd")) {
      ours = zrdr::find(root, "MODSDIALOG") != nullptr;
      if (text)
        for (int i = 1; i < root->list[0].i; ++i) *text += zrd_text(root->list[i]) + "\n";
      zrdr::free_node(root);
    }
    zrdr::close_archives(true);
    if (hook) mech3::patch12::uninstall();
    return ours;
  };
  std::string text;  // the shipped entry as text, without our records
  EXPECT(!read({}, false, &text));
  EXPECT(text.find("MAINMENU") != std::string::npos);
  put(wd + "/mods/menus/mod.txt", "name=Menus\n");
  put(wd + "/mods/menus/zbd/dialog.zrd", "; a modder's menus\n" + text);
  EXPECT(!read({"menus"}, false, nullptr));  // the loose copy alone has none
  EXPECT(read({}, true, nullptr));           // the entry, through the hook
  EXPECT(read({"menus"}, true, nullptr));    // the loose copy, through the hook
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

// Mods' mechs (item 1 of the modding cut): mech= rows parsed, added from 20
// in the mods' order (19 stays the exe's "no mech", 0x13), bad or taken names refused with the
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
  EXPECT(mech3::mech_count() == 22);
  EXPECT(mech3::mech_index("MadCat_B") == 20 && mech3::mech_index("scout") == 21 && mech3::mech_index("madcat") == 9);
  EXPECT(std::string(mech3::mech_name(20)) == "madcat_b" && std::string(mech3::mech_model_name(20)) == "madcat" &&
         std::string(mech3::mech_model_name(9)) == "madcat" && std::string(mech3::mech_title(20)) == "Mad Cat B" &&
         mech3::mech_title(9) == nullptr);
  EXPECT(mech3::max_mass(20) == 75.0f && mech3::weight_class(20) == 2 && mech3::mech_tech(20) == mech3::kClan &&
         mech3::internal_points(20, 1) == 23 && mech3::mech_name_message(20) == -1);
  EXPECT(mech3::max_mass(21) == 20.0f && mech3::mech_tech(21) == mech3::kInnerSphere && std::string(mech3::mech_name(22)) == "notmech");
  // 19 (0x13) is still no mech with rows there: its name, no row, no index, no
  // definition, and a pilot's stock adds none of it (0x00471c10).
  EXPECT(std::string(mech3::mech_name(19)) == "notmech" && mech3::mech_title(19) == nullptr && mech3::mech_index("notmech") == -1);
  EXPECT(mech3::mech_dfn(19) == nullptr && mech3::max_mass(19) == 10.0f && mech3::mech_name_message(19) == -1);
  {
    mech3::Store stock;
    EXPECT(stock.add_mech(19) == nullptr && stock.add_mech(-1) == nullptr && stock.add_mech(20) != nullptr);
  }
  {  // the lab's whole stock (0x00471f40's): the mods' mechs after the exe's, never 19 or the Elemental
    mech3::Store all;
    all.fill_all();
    int mechs = 0, none = 0, mods = 0;
    for (const mech3::ConfigItem* it : all.items)
      if (it->kind() == 0) ++mechs, none += it->id() == 19 || it->id() == 7, mods += it->id() == 20 || it->id() == 21;
    EXPECT(mechs == 20 && none == 0 && mods == 2);
  }
  EXPECT(text.find("mech thor tech=clan mass=70 class=heavy: there is a mech thor already, left out") != std::string::npos);
  EXPECT(text.find("mech bad tech=clan mass=7 class=heavy: mass= must be 10 to 100 tons in steps of 5, left out") != std::string::npos);
  EXPECT(text.find("mech MADCAT_B tech=any mass=20 class=light: there is a mech MADCAT_B already, left out") != std::string::npos);
  EXPECT(text.find("mech scout added (mech 21, 20 t, the models of scout)") != std::string::npos);
  // A row whose definition or model the game would not find: left out, said.
  EXPECT(text.find("mech ghost tech=sphere mass=20 class=light: no dfn_ghost.zrd in the reader archive, left out") != std::string::npos);
  EXPECT(text.find("mech shade tech=sphere mass=20 class=light model=nothing: no mech_nothing.flt in mechlib.zbd, left out") !=
         std::string::npos);
  mech3::mech_rows_clear();
  EXPECT(mech3::mech_count() == 19 && mech3::mech_index("scout") == -1);
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

// A mech row whose definition and model a mod ships only as mech3ax's JSON (docs/modding.md): found as the game
// would find them, a folder's JSON or the archive's zip, so the row is not left out.
TEST(mods_mech_row_files_as_json) {
  SavedMounts keep;
  const std::string wd = tmp("mods_mech_json");
  put(wd + "/mods/j/mod.txt", "name=J\nmech=jsonmech tech=clan mass=70 class=heavy\nmech=zipmech tech=clan mass=70 class=heavy model=jsonmech\n"
                              "mech=nomech tech=clan mass=70 class=heavy model=jsonmech\n");
  put(wd + "/mods/j/zbd/reader/dfn_jsonmech.zrd.json", "[]");
  write_stored_zip(wd + "/mods/j/zbd/reader.zip", {{"DFN_ZIPMECH.ZRD.JSON", "[]"}});
  write_stored_zip(wd + "/mods/j/zbd/mechlib.zip", {{"mech_jsonmech.flt.json", "{}"}});
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mech_rows_clear();
  std::string text;
  for (const std::string& l : mech3::mods_mount(wd, {"j"}, {})) text += l + "\n";
  std::printf("%s", text.c_str());
  EXPECT(text.find("mech jsonmech added") != std::string::npos && text.find("mech zipmech added") != std::string::npos);
  EXPECT(text.find("mech nomech tech=clan mass=70 class=heavy model=jsonmech: no dfn_nomech.zrd in the reader archive, left out") !=
         std::string::npos);
  mech3::mech_rows_clear();
}

// Mods' weapons (item 2): weapon= rows after the plain PPC's place (38 on,
// the bug fixes on or off: a pilot's saved weapon keeps its number), their
// ammo and techs, the PPC still left out with the fixes off; a taken name
// (the PPC's too) refused; a remount starts from the exe's tables; the rows
// in the gameplay hash.
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
  EXPECT(text.find("weapon Laser_Huge added (weapon 38)") != std::string::npos &&
         text.find("weapon Rail added (weapon 39)") != std::string::npos);
  EXPECT(text.find("weapon ppc ammo=no tech=any: there is a weapon ppc already, left out") != std::string::npos);
  EXPECT(text.find("weapon AC2 ammo=yes tech=any: there is a weapon AC2 already, left out") != std::string::npos);
  for (bool bugs : {true, false}) {
    zfix::options().bugs = bugs;
    EXPECT(mech3::weapon_count() == 40);
    EXPECT(mech3::weapon_index("laser_huge") == 38 && mech3::weapon_index("Rail") == 39 && mech3::weapon_index("AC2") == 0);
    EXPECT(std::string(mech3::weapon_name(39)) == "Rail" && std::string(mech3::weapon_title(38)) == "Huge Laser" &&
           mech3::weapon_title(0) == nullptr && mech3::weapon_name_message(38) == -1);
    EXPECT(!mech3::weapon_uses_ammo(38) && mech3::weapon_uses_ammo(39) && mech3::weapon_has_tech(38, mech3::kInnerSphere) &&
           !mech3::weapon_has_tech(38, mech3::kClan) && mech3::weapon_has_tech(39, mech3::kClan));
    // The plain PPC (37): offered with the fixes on only, as without mods.
    EXPECT(mech3::weapon_index("PPC") == (bugs ? 37 : -1) && std::string(mech3::weapon_name(37)) == (bugs ? "PPC" : "noweapon"));
    EXPECT(mech3::weapon_has_tech(37, mech3::kInnerSphere) == bugs && mech3::weapon_name_message(37) == (bugs ? 0x2fd : -1));
  }
  // The lab's whole stock (0x00471f40's) with the fixes off and a row there:
  // the row's weapon (and Rail's ammo), never the plain PPC (37) nor its ammo.
  zfix::options().bugs = false;
  {
    mech3::Store all;
    all.fill_all();
    int ppc = 0, huge = 0, rail_ammo = 0;
    for (const mech3::ConfigItem* it : all.items) {
      if (it->kind() == 1 || it->kind() == 2) ppc += it->id() == 37;
      huge += it->kind() == 1 && it->id() == 38;
      rail_ammo += it->kind() == 2 && it->id() == 39;
    }
    EXPECT(ppc == 0 && huge == 1 && rail_ammo == 1);
  }
  zfix::options() = saved;
  // A remount: the exe's tables, then the rows again (not refused as taken).
  put(wd + "/mods/m/mod.txt", "name=M\nmech=scout tech=sphere mass=20 class=light\n");
  mech_files(wd + "/mods/m", {"scout"}, {"scout"});
  text.clear();
  for (const std::string& l : mech3::mods_mount(wd, {"m", "w"}, {})) text += l + "\n";
  EXPECT(text.find("mech scout added (mech 20") != std::string::npos && text.find("weapon Rail added (weapon 39)") != std::string::npos);
  for (const std::string& l : mech3::mods_mount(wd, {}, {})) text += l + "\n";
  EXPECT(mech3::mech_count() == 19 && mech3::weapon_count() == (saved.bugs ? 38 : 37) && mech3::weapon_index("Rail") == -1);
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
  mount({"a", "b"});  // alpha 20, beta 21, Gun_A 38, Gun_B 39
  EXPECT(mech3::mech_index("alpha") == 20 && mech3::mech_index("beta") == 21 && mech3::weapon_index("Gun_B") == 39);
  // A pilot: its lab game's slot on beta with Gun_B fitted, Gun_A and alpha in its stock.
  mech3::Pilot* p = mech3::create_pilot("Rows");
  EXPECT(p != nullptr);
  p->select_game(mech3::kMechLab);
  {
    auto* c = new mech3::Config(21, "mine");  // the slot's own (its dtor deletes it)
    auto* w = new mech3::WeaponItem;
    w->count = 1, w->weapon = 39, w->tech = 2;
    c->items.push_back(w);
    delete p->current->slot.config;
    p->current->slot.config = c;
    p->current->slot.mech = 21;
  }
  for (mech3::ConfigItem* it : p->store()->items) delete it;  // a stock of its own (the lab's whole one saves empty)
  p->store()->items.clear();
  p->store()->full = 0;
  auto* gun = new mech3::WeaponItem;
  gun->count = 1, gun->weapon = 38, gun->tech = 1;
  p->store()->items.push_back(gun);
  p->store()->add_mech(20);
  EXPECT(p->save());
  EXPECT(file("pilots/Rows.mw3.rows").find("mech 21 beta") != std::string::npos &&
         file("pilots/Rows.mw3.rows").find("weapon 39 Gun_B") != std::string::npos &&
         file("pilots/Rows.mw3.rows").find("mech 20 alpha") != std::string::npos);
  drop(p);
  // A loadout file of beta's with Gun_A.
  {
    mech3::ConfigList l(21, 0);
    auto* c = new mech3::Config(21, "beta_gun");
    c->player_made = 1;
    auto* w = new mech3::WeaponItem;
    w->count = 1, w->weapon = 38, w->tech = 1;
    c->items.push_back(w);
    l.configs.push_back(c);
    EXPECT(l.save_all());
    l.configs.clear();
    delete c;
  }
  EXPECT(file("configs/beta.mw3.rows").find("weapon 38 Gun_A") != std::string::npos);
  // The mods swapped round: beta 20, alpha 21, Gun_B 38, Gun_A 39; the same mechs and weapons.
  mount({"b", "a"});
  EXPECT(mech3::mech_index("beta") == 20 && mech3::weapon_index("Gun_A") == 39);
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
    EXPECT(s.mech == 20 && s.config && s.config->mech == 20 && fitted == 38 && stocked == 39 && mech_stock == 21);
    drop(p);
  }
  {
    mech3::ConfigList l(mech3::mech_index("beta"), 0);
    EXPECT(l.load_saved() && l.configs.size() == 1 && !l.keep_file);
    EXPECT(!l.configs.empty() && l.configs[0]->items.front()->id() == 39 && l.configs[0]->mech == 20);
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
  mount({"a"});  // 20 alpha only; the file's 21 is past the table
  mech3::rows_file::refusals().clear();
  p = mech3::load_pilot("Rows", false);
  EXPECT(p != nullptr && mech3::rows_file::refusals().empty());
  if (p) {  // saved again: still no sidecar (21 is no mod's row here), the number kept
    EXPECT(p->save() && file("pilots/Rows.mw3.rows") == "(none)");
    drop(p);
  }
  // A line that does not read: refused, never the raw numbers.
  mount({"a", "b"});
  put(wd + "/pilots/Rows.mw3.rows", "mech 21 beta\nweapon thirtynine Gun_B\nmech 20 alpha\n");
  mech3::rows_file::refusals().clear();
  EXPECT(mech3::load_pilot("Rows", false) == nullptr);
  EXPECT(!mech3::rows_file::refusals().empty() && mech3::rows_file::refusals()[0].find("does not read at line 2") != std::string::npos);
  fs::rename(wd + "/pilots/Rows.mw3.rows.kept", wd + "/pilots/Rows.mw3.rows", ec);
  // Rename and delete carry the sidecar; a rewrite with no rows deletes it.
  p = mech3::load_pilot("Rows", false);
  EXPECT(p != nullptr);
  if (p) {
    EXPECT(p->rename("Rows2"));
    EXPECT(file("pilots/Rows.mw3.rows") == "(none)" && file("pilots/Rows2.mw3.rows").find("mech 21 beta") != std::string::npos);
    p->select_game(mech3::kMechLab);
    p->current->slot.mech = 3;
    delete p->current->slot.config;
    p->current->slot.config = nullptr;
    for (mech3::ConfigItem* it : p->store()->items) delete it;
    p->store()->items.clear();
    EXPECT(p->save() && file("pilots/Rows2.mw3.rows") == "(none)");
    p->current->slot.mech = 20;
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
  put(wd + "/mods/pm/mod.txt", "name=P\nfor=pm 1.0\n");
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
    EXPECT(l[2].info.error == "no mod.txt" && l[3].info.error.find("not MechWarrior 3") != std::string::npos);
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
  put(wd + "/mods/w/mod.txt", "name=W\nworld=good\nworld=nofiles\nworld=../evil\nworld=T1\nworld=json\nworld=zipped\nia_world=good Good\n"
                              "ia_world=c3\nia_world=nowhere Lost\n");
  put(wd + "/mods/w/zbd/good/gamez.zbd", "x");
  put(wd + "/mods/w/zbd/json/gamez/metadata.json", "{}");  // a world as unzbd's JSON, a folder or its zip (gamez JSON)
  put(wd + "/mods/w/zbd/zipped/gamez.zip", "x");
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mod_worlds().clear(), mech3::mod_ia_worlds().clear();
  std::string log;
  for (const std::string& l : mech3::mods_mount(wd, {"w"}, {})) log += l + "\n";
  EXPECT((mech3::mod_worlds() == std::vector<std::string>{"good", "json", "zipped"}) && mech3::mission_dir_count() == 12 &&
         std::string(mech3::mission_dir(9)) == "good" && std::string(mech3::mission_dir(12)) == "none");
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

namespace {
// Two trees the same node for node: types, ints, float bits, strings, list lengths.
bool same_tree(const zrdr::Node& a, const zrdr::Node& b, std::string* where, const std::string& at = "$") {
  if (a.type != b.type) return *where = at + ": type", false;
  switch (a.type) {
    case zrdr::Int: if (a.i != b.i) return *where = at + ": int", false; return true;
    case zrdr::Float: if (std::memcmp(&a.f, &b.f, 4) != 0) return *where = at + ": float", false; return true;
    case zrdr::String: if (std::strcmp(a.s, b.s) != 0) return *where = at + ": string", false; return true;
    case zrdr::List: {
      if (zrdr::list_count(&a) != zrdr::list_count(&b)) return *where = at + ": length", false;
      for (int32_t i = 1; i < zrdr::list_count(&a); ++i)
        if (!same_tree(a.list[i], b.list[i], where, at + "[" + std::to_string(i - 1) + "]")) return false;
      return true;
    }
    default: return *where = at + ": unknown type", false;
  }
}
}  // namespace

namespace {
// mech3ax's unzbd beside the checkout (../mech3ax/target/release, as test_net_save_mech3ax.py finds it) or at
// MECH3AX_UNZBD; empty, said once, when there is none (the tests that use it then skip that part).
fs::path find_unzbd() {
  std::error_code ec;
  const fs::path p = std::getenv("MECH3AX_UNZBD") ? fs::path(std::getenv("MECH3AX_UNZBD"))
                                                  : fs::canonical(MW3_GAME_DIR, ec).parent_path().parent_path().parent_path() / "mech3ax" /
                                                        "target" / "release" / "unzbd";
#if defined(_WIN32)
  (void)p;
  std::printf("  unzbd skipped: run through the shell on the Mac and Linux only\n");
  return {};
#else
  static bool said = false;
  if (fs::is_regular_file(p, ec)) return p;
  if (!said) std::printf("  unzbd skipped: none at %s (cargo build --release in ../mech3ax, or MECH3AX_UNZBD)\n", p.string().c_str());
  said = true;
  return {};
#endif
}
// unzbd's export of `zbd` (`kind`: reader, motion, mechlib...), each file of its zip by name; false when it did not run.
bool unzbd_export(const fs::path& unzbd, const char* kind, const std::string& zbd, std::map<std::string, std::string>* out) {
  static int n = 0;
  const std::string zip = tmp("unzbd_export") + "/" + std::to_string(n++) + ".zip";
  const std::string cmd = "'" + unzbd.string() + "' mw " + kind + " '" + zbd + "' '" + zip + "' >/dev/null 2>&1";
  if (std::system(cmd.c_str()) != 0) return false;
  zutl_zip::Zip z;
  if (!z.open(zip)) return false;
  for (const zutl_zip::Zip::Entry& e : z.entries) {
    std::vector<uint8_t> d;
    std::string why;
    if (z.read(e, &d, &why)) (*out)[e.name] = std::string(d.begin(), d.end());
  }
  return true;
}
}  // namespace

// Every reader entry the install ships, written as mech3ax's JSON (to_json) and read back (parse_json): the
// same tree, every float to the bit (docs/modding.md, Readable files); and with unzbd (find_unzbd) each archive's
// own `<entry>.zrd.json` read and checked against the entry too.
TEST(mods_reader_json_round_trips_every_shipped_entry) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/reader.zbd", ec)) return;
  int entries = 0, archives = 0, floats = 0, from_unzbd = 0;
  std::string first_bad;
  for (const auto& f : fs::recursive_directory_iterator(install + "/zbd", ec)) {
    const std::string name = f.path().filename().string();
    if (name.rfind("reader", 0) != 0 || f.path().extension() != ".zbd") continue;
    zutl_zar::Archive a;
    zutl_zar::init(a);
    if (!zutl_zar::open(a, f.path().string().c_str())) continue;
    ++archives;
    static const fs::path unzbd = find_unzbd();
    std::map<std::string, std::string> exported;
    const bool against = !unzbd.empty() && unzbd_export(unzbd, "reader", f.path().string(), &exported);
    for (int i = 0; i < a.count; ++i) {
      uint32_t length = 0;
      std::FILE* file = zutl_zar::seek(a, i, &length);
      if (!file) continue;
      zrdr::Node root{};
      zrdr::read_node(file, &root);
      std::string json, why, where;
      zrdr::Node* back = zrdr::to_json(root, &json) ? zrdr::parse_json(json, &why) : nullptr;
      const bool same = back && same_tree(root, *back, &where);
      if (!same && first_bad.empty()) first_bad = name + "/" + a.entries[i].name + ": " + (back ? where : why);
      floats += static_cast<int>(std::count(json.begin(), json.end(), '.'));
      ++entries;
      if (back) zrdr::free_node(back);
      if (against) {
        zrdr::Node* theirs = zrdr::parse_json(exported[std::string(a.entries[i].name) + ".json"], &why);
        const bool ok = theirs && same_tree(root, *theirs, &where);
        if (!ok && first_bad.empty()) first_bad = std::string("unzbd ") + a.entries[i].name + ": " + (theirs ? where : why);
        from_unzbd += ok;
        if (theirs) zrdr::free_node(theirs);
      }
      zrdr::free_contents(&root);
    }
    zutl_zar::destroy(a);
  }
  std::printf("  %d archives, %d entries, about %d floats%s; first difference: %s\n", archives, entries, floats,
              from_unzbd ? (", " + std::to_string(from_unzbd) + " of unzbd's own the same").c_str() : "", first_bad.empty() ? "none" : first_bad.c_str());
  EXPECT(archives > 0 && entries > 100);
  EXPECT(first_bad.empty());
}

// What the reader's JSON refuses, each with its line and the JSON path of the value (docs/modding.md).
TEST(mods_reader_json_errors_name_the_line_and_place) {
  const std::pair<const char*, const char*> cases[] = {
      {"[1, [2, {}]]", "line 1 (at $[1][1]): an object"},
      {"[1,\n [2,\n  true]]", "line 3 (at $[1][1]): true or false"},
      {"[2147483648]", "line 1 (at $[0]): 2147483648: past the game's 32-bit integers"},
      {"[-2147483649]", "line 1 (at $[0]): -2147483649: past the game's 32-bit integers"},
      {"[1e39]", "line 1 (at $[0]): 1e39: past a float's range"},
      {"[1,\n2", "line 2 (at $): a '[' never closed"},
      {"[1 2]", "line 1 (at $): '2' where a ',' or ']' should be"},
      {"[\"a\nb\"]", "line 1 (at $[0]): a string never closed on its line"},
      {"[\"\\u4e2d\"]", "line 1 (at $[0]): a character the game's text (Windows-1252) cannot hold"},
      {"\"text\"", "line 1 (at $): the entry is not a list"},
      {"[1] [2]", "line 1 (at $): more after the entry's list"},
      {"[1.]", "line 1 (at $[0]): a '.' with no digits after it"},
  };
  for (const auto& [text, want] : cases) {
    std::string why;
    zrdr::Node* n = zrdr::parse_json(text, &why);
    std::printf("  %-22s -> %s\n", std::string(text).substr(0, 22).c_str(), why.c_str());
    EXPECT(n == nullptr);
    EXPECT(why.rfind(want, 0) == 0);
    if (n) zrdr::free_node(n);
  }
  std::string deep;
  for (int i = 0; i < 65; ++i) deep += "[";
  std::string why;
  EXPECT(zrdr::parse_json(deep, &why) == nullptr && why.find("lists nested past 64 deep") != std::string::npos);
  // What reads: null an empty list, '.' or an exponent a float, else an int; Windows-1252 text kept.
  zrdr::Node* n = zrdr::parse_json("[null, 1, 1.0, 2e0, -0.5, \"caf\\u00e9\", [\"KEY\", [3]]]", &why);
  EXPECT(n && zrdr::list_count(n) == 8);
  if (n) {
    EXPECT(n->list[1].type == zrdr::List && zrdr::list_count(&n->list[1]) == 1);
    EXPECT(n->list[2].type == zrdr::Int && n->list[2].i == 1);
    EXPECT(n->list[3].type == zrdr::Float && n->list[3].f == 1.0f && n->list[4].type == zrdr::Float && n->list[4].f == 2.0f);
    EXPECT(n->list[5].type == zrdr::Float && n->list[5].f == -0.5f);
    EXPECT(n->list[6].type == zrdr::String && std::string(n->list[6].s) == "caf\xe9");
    int32_t v = 0;
    EXPECT(zrdr::get_int(n, "KEY", &v) && v == 3);
    zrdr::free_node(n);
  }
}


// A mod's entry as mech3ax's JSON (docs/modding.md, Readable files): a loose .zrd first, then the JSON in a folder
// named as the archive (zbd/reader/fmv.zrd.json), then the archive's zip of them (zbd/reader.zip), then the archive's
// own entry. A JSON that does not read is refused, the archive's entry read instead; through the same load hook.
TEST(mods_reader_json_entry_in_a_folder_or_a_zip) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/reader.zbd", ec)) return;
  SavedMounts keep;
  const std::string wd = tmp("mods_json");
  const std::string mod = wd + "/mods/jsonmod";
  put(mod + "/mod.txt", "name=JSON\n");
  put(wd + "/mods/later/mod.txt", "name=Later\n");  // after it: never asked while the first mod has a copy, even a broken one
  put(wd + "/mods/later/zbd/reader/fmv.zrd.json", "[\"MODDED\", \"later\"]");
  static int hooked = 0;
  hooked = 0;
  auto marked = [&]() -> std::string {
    zrdr::init();
    zrdr::set_load_hook([](const char*, const char* entry, zrdr::Node*) { hooked += std::strcmp(entry, "fmv.zrd") == 0; });
    EXPECT(zrdr::open_archive("zbd\\reader.zbd", true));
    zrdr::Node* n = zrdr::load("fmv.zrd");
    const char* from = n ? zrdr::get_string(n, "MODDED") : nullptr;
    const std::string r = from ? from : n && zrdr::find(n, "INTRO") ? "archive" : "none";
    if (n) zrdr::free_node(n);
    zrdr::set_load_hook(nullptr);
    zrdr::close_archives(true);
    return r;
  };
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"jsonmod", "later"}, {install});
  zutl::mount(install);
  write_stored_zip(mod + "/zbd/reader.zip", {{"other.zrd.json", "[\"MODDED\", \"other\"]"}, {"FMV.ZRD.JSON", "[\"MODDED\", \"zip\"]"}});
  EXPECT(marked() == "zip");  // the name matched as the archive's are, any case
  put(mod + "/zbd/reader/fmv.zrd.json", "[\"MODDED\", \"folder\"]");
  EXPECT(marked() == "folder");
  put(mod + "/zbd/fmv.zrd", "(MODDED \"loose\")\n");
  EXPECT(marked() == "loose");
  fs::remove(mod + "/zbd/fmv.zrd", ec);
  put(mod + "/zbd/reader/fmv.zrd.json", "[\"MODDED\",\n {\"no\": 1}]");
  EXPECT(marked() == "archive");  // refused: the archive's entry
  fs::remove(mod + "/zbd/reader/fmv.zrd.json", ec);
  write_stored_zip(mod + "/zbd/reader.zip", {{"other.zrd.json", "[]"}});
  EXPECT(marked() == "later");  // the zip without it: the next mod's
  put(mod + "/zbd/reader.zip", "not a zip");
  EXPECT(marked() == "archive");  // refused, as a broken copy is
  EXPECT(hooked == 6);  // every tree read, the mod's and the archive's, through the hook
}

// Every motion MW3 ships, as unzbd's JSON (motion_to_json) and back (motion_from_json): the entry's own bytes, and
// read by the archive's bounded reader; with unzbd (find_unzbd), its own `<motion>.json`, each the entry too.
TEST(mods_motion_json_round_trips_every_shipped_motion) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/motion.zbd", ec)) return;
  zutl_zar::Archive a;
  zutl_zar::init(a);
  EXPECT(zutl_zar::open(a, (install + "/zbd/motion.zbd").c_str()));
  const fs::path unzbd = find_unzbd();
  std::map<std::string, std::string> exported;
  const bool theirs = !unzbd.empty() && unzbd_export(unzbd, "motion", install + "/zbd/motion.zbd", &exported);
  int motions = 0, same = 0, read = 0, from_unzbd = 0;
  std::string first_bad;
  for (int i = 0; i < a.count; ++i) {
    uint32_t length = 0;
    std::FILE* f = zutl_zar::seek(a, i, &length);
    if (!f) continue;
    std::vector<uint8_t> entry(length);
    if (std::fread(entry.data(), 1, length, f) != length) continue;
    ++motions;
    std::string json, why;
    std::vector<uint8_t> back;
    const bool ok = zsis::motion_to_json(entry.data(), entry.size(), &json) && zsis::motion_from_json(json, &back, &why) && back == entry;
    same += ok;
    if (!ok && first_bad.empty()) first_bad = std::string(a.entries[i].name) + (why.empty() ? "" : ": " + why);
    if (std::FILE* t = std::tmpfile()) {  // the bytes through the archive's reader
      std::fwrite(back.data(), 1, back.size(), t);
      std::rewind(t);
      zsis::Script sc;
      if (sc.read(t, static_cast<long>(back.size())) && sc.part_count > 0) ++read;
      for (uint32_t k = 0; k < sc.part_count; ++k) sc.parts[k]->destroy(), delete sc.parts[k];
      std::free(sc.parts), sc.parts = nullptr, sc.part_count = 0;
      std::fclose(t);
    }
    if (theirs) {
      std::vector<uint8_t> bytes;
      const bool them = zsis::motion_from_json(exported[std::string(a.entries[i].name) + ".json"], &bytes, &why) && bytes == entry;
      from_unzbd += them;
      if (!them && first_bad.empty()) first_bad = std::string("unzbd ") + a.entries[i].name + ": " + why;
    }
  }
  zutl_zar::destroy(a);
  std::printf("  %d motions, %d the same bytes back, %d read by the reader%s; first difference: %s\n", motions, same, read,
              theirs ? (", " + std::to_string(from_unzbd) + " of unzbd's the same bytes").c_str() : "", first_bad.empty() ? "none" : first_bad.c_str());
  EXPECT(motions > 200 && same == motions && read == motions && first_bad.empty());
}

// What a motion's JSON refuses, each with its line and place (docs/modding.md).
TEST(mods_motion_json_errors_name_the_line_and_place) {
  const std::string frame = "{\"translation\": {\"x\": 0, \"y\": 1.5, \"z\": 0}, \"rotation\": {\"x\": 1, \"y\": 0, \"z\": 0, \"w\": 0}}";
  const std::string part = "{\"name\": \"hip\", \"frames\": [" + frame + ", " + frame + "]}";
  const std::string good = "{\"loop_time\": 2.0, \"frame_count\": 2, \"parts\": [" + part + "]}";
  std::vector<uint8_t> bytes;
  std::string why;
  EXPECT(zsis::motion_from_json(good, &bytes, &why) && bytes.size() == 24 + 4 + 3 + 4 + 3 * 28);
  const std::pair<std::string, const char*> cases[] = {
      {"[]", "line 1 (at $): not an object"},
      {"{\"loop_time\": 2.0, \"parts\": []}", "line 1 (at $): no \"frame_count\""},
      {"{\"loop_time\": 2.0,\n \"frame_count\": 0, \"parts\": []}", "line 2 (at $.frame_count): 0: a motion has at least one frame"},
      {"{\"loop_time\": \"2\", \"frame_count\": 2, \"parts\": []}", "line 1 (at $.loop_time): not a number"},
      {"{\"loop_time\": 2.0, \"frame_count\": 2.5, \"parts\": []}", "line 1 (at $.frame_count): not a whole number"},
      {"{\"loop_time\": 1e40, \"frame_count\": 2, \"parts\": []}", "line 1 (at $.loop_time): 1e40: past a float's range"},
      {"{\"loop_time\": 2.0, \"frame_count\": 3, \"parts\": [" + part + "]}", "line 1 (at $.parts[0].frames): 2 frames, where frame_count says 3"},
      {"{\"loop_time\": 2.0, \"frame_count\": 1, \"parts\": [" + part + "]}", "line 1 (at $.parts[0].frames): 2 frames, where frame_count says 1"},
      {"{\"loop_time\": 2.0, \"frame_count\": 2, \"parts\": [{\"name\": \"hip\", \"frames\": [" + frame + ",\n {\"translation\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"rotation\": {\"x\": 1, \"y\": 0, \"z\": 0}}]}]}",
       "line 2 (at $.parts[0].frames[1].rotation): no \"w\""},
      {"{\"loop_time\": 2.0, \"frame_count\": 2, \"parts\": [{\"name\": \"\\u4e2d\", \"frames\": []}]}", "line 1 (at $.parts[0].name): a character the game's text"},
      {"{\"loop_time\": 2.0, \"frame_count\": 2, \"parts\": [{\"name\": \"hip\",\n \"frames\": [" + frame + ",", "line 2 (at $.parts[0].frames[1]): the file ends"},
      {"{\"loop_time\" 2.0}", "line 1 (at $): no ':' after the name \"loop_time\""},
  };
  for (const auto& [text, want] : cases) {
    why.clear();
    const bool ok = zsis::motion_from_json(text, &bytes, &why);
    std::printf("  -> %s\n", why.c_str());
    EXPECT(!ok && why.rfind(want, 0) == 0);
  }
}

// The bytes a JSON entry becomes, as a file for the FILE* readers: in the system's temp folder, not std::tmpfile
// (which has failed on Windows), read from its start, and gone afterwards, only the file it made.
TEST(mods_scratch_file_is_read_and_removed) {
  fs::path where;
  {
    const std::string bytes("motion\0bytes", 12);
    zutl_zip::ScratchFile t(bytes.data(), bytes.size());
    EXPECT(t.file != nullptr);
    where = t.path;
    char back[16] = {};
    EXPECT(t.file && std::fread(back, 1, sizeof back, t.file) == 12 && std::memcmp(back, bytes.data(), 12) == 0);
    std::error_code ec;
    EXPECT(fs::exists(where, ec) && fs::equivalent(where.parent_path(), fs::temp_directory_path(ec), ec));
    zutl_zip::ScratchFile empty(nullptr, 0);
    EXPECT(empty.file && empty.path != t.path);
  }
  std::error_code ec;
  EXPECT(!where.empty() && !fs::exists(where, ec));
}

// mechlib.zbd's entries from unzbd's own JSON: every model and the materials the entry's own bytes
// (docs/modding.md). unzbd beside the checkout (../mech3ax/target/release, as test_net_save_mech3ax.py finds it;
// MECH3AX_UNZBD for another), its zip read here; skipped, and said, without it.
TEST(mods_mechlib_json_is_every_entrys_bytes) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/mechlib.zbd", ec)) return;
  const fs::path unzbd = find_unzbd();
  std::map<std::string, std::string> exported;
  if (unzbd.empty()) return;
  EXPECT(unzbd_export(unzbd, "mechlib", install + "/zbd/mechlib.zbd", &exported));
  const auto json_of = [&](const std::string& name) { return exported[name]; };
  zutl_zar::Archive a;
  zutl_zar::init(a);
  EXPECT(zutl_zar::open(a, (install + "/zbd/mechlib.zbd").c_str()));
  int entries = 0, same = 0;
  std::string first_bad;
  for (int i = 0; i < a.count; ++i) {
    const std::string name = a.entries[i].name;
    if (name == "format" || name == "version") continue;
    uint32_t length = 0;
    std::FILE* f = zutl_zar::seek(a, i, &length);
    std::vector<uint8_t> entry(length);
    if (!f || std::fread(entry.data(), 1, length, f) != length) continue;
    ++entries;
    const std::string text = json_of(name + ".json");
    std::vector<uint8_t> bytes;
    std::string why;
    const bool ok = name == "materials" ? zcls::mechlib_materials_from_json(text, &bytes, &why)
                                        : zcls::mechlib_model_from_json(text, &bytes, &why);
    if (ok && bytes == entry) {
      ++same;
      continue;
    }
    if (!first_bad.empty()) continue;
    if (!ok) {
      first_bad = name + ": " + why;
      continue;
    }
    size_t at = 0;
    while (at < bytes.size() && at < entry.size() && bytes[at] == entry[at]) ++at;
    char b[160];
    std::snprintf(b, sizeof b, "%s: %zu bytes vs %zu, first difference at %zu (%02x vs %02x)", name.c_str(), bytes.size(), entry.size(), at,
                  at < bytes.size() ? bytes[at] : 0, at < entry.size() ? entry[at] : 0);
    first_bad = b;
  }
  zutl_zar::destroy(a);
  std::printf("  %d entries, %d the same bytes from unzbd's JSON; first difference: %s\n", entries, same, first_bad.empty() ? "none" : first_bad.c_str());
  EXPECT(entries > 50 && same == entries);
}

// Each world's anim.zbd from unzbd's own JSON (find_unzbd): the shipped file's bytes but past names' NULs, where
// mech3ax restores the shipped padding from tables of its own and ours is zeros (docs/modding.md).
TEST(mods_anim_json_is_every_worlds_file) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/c1/anim.zbd", ec)) return;
  const fs::path unzbd = find_unzbd();
  if (unzbd.empty()) return;
  int worlds = 0, same = 0;
  for (const char* w : {"c1", "c2", "c3", "c4", "c4b", "t1"}) {
    const std::string zbd = install + "/zbd/" + w + "/anim.zbd";
    if (!fs::exists(zbd, ec)) continue;
    ++worlds;
    std::map<std::string, std::string> exported;
    EXPECT(unzbd_export(unzbd, "anim", zbd, &exported));
    std::ifstream in(zbd, std::ios::binary);
    const std::vector<uint8_t> shipped((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<uint8_t> ours;
    std::vector<std::pair<size_t, size_t>> masks;
    std::string why;
    const bool ok = zeff::anim_from_json([&](const std::string& n) { return exported.count(n) ? exported[n] : std::string(); }, &ours, &masks, &why);
    if (!ok) std::printf("  %s: %s\n", w, why.c_str());
    if (const char* dump = std::getenv("MW3_ANIM_DUMP")) std::ofstream(std::string(dump) + "/" + w + ".zbd", std::ios::binary).write(
        reinterpret_cast<const char*>(ours.data()), static_cast<std::streamsize>(ours.size()));
    std::vector<uint8_t> a = ours, b = shipped;
    if (!ok) b.resize(std::min(b.size(), a.size()));  // the defs it read, against the shipped file's start
    for (const auto& [from, to] : masks)
      for (size_t k = from; k < to && k < a.size() && k < b.size(); ++k) a[k] = b[k] = 0;
    size_t at = 0;
    while (at < a.size() && at < b.size() && a[at] == b[at]) ++at;
    const bool equal = ok && a.size() == b.size() && at == a.size();
    same += equal;
    std::printf("  %s: %zu bytes vs %zu, %zu names masked, %s\n", w, ours.size(), shipped.size(), masks.size(),
                equal ? "the same" : at == a.size() && !ok ? "the same so far" : ("first difference at " + std::to_string(at)).c_str());
  }
  EXPECT(worlds == 6 && same == worlds);
}

// The events no shipped world uses but the runtime runs (ObjectDeleteChild, CameraState, CameraFromTo,
// FbfxCsinwaveFromTo, AnimVerbose), in a sequence added to one of t1's defs: our file against mech3ax's own writer's
// (rezbd beside unzbd) on the same JSON. Effect, which mech3ax refuses in MechWarrior 3, against its layout by hand.
TEST(mods_anim_json_unused_events_as_mech3ax_writes_them) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/t1/anim.zbd", ec)) return;
  const fs::path unzbd = find_unzbd();
  if (unzbd.empty()) return;
  const fs::path rezbd = unzbd.parent_path() / "rezbd";
  if (!fs::is_regular_file(rezbd, ec)) return (void)std::printf("  skipped: no rezbd beside unzbd\n");
  std::map<std::string, std::string> exported;
  EXPECT(unzbd_export(unzbd, "anim", install + "/zbd/t1/anim.zbd", &exported));
  const std::string def = "ams_muz-ams_blast.json";
  std::string& text = exported[def];
  zutl_json::Value v;
  std::string why;
  EXPECT(zutl_json::parse(text, &v, &why) && v.get("nodes") && !v.get("nodes")->items.empty());
  if (!v.get("nodes") || v.get("nodes")->items.empty()) return;
  const std::string node = v.get("nodes")->items[0].get("name")->text;
  const std::string ev = [&] {
    const std::string n = "\"" + node + "\"";
    return "{\"name\": \"mod_events\", \"seq_state\": \"OnCall\", \"reset_state\": \"OnCall\", \"pointer\": 1, \"events\": ["
           "{\"start\": null, \"data\": {\"ObjectDeleteChild\": {\"parent\": " + n + ", \"child\": " + n + "}}},"
           "{\"start\": {\"offset\": \"Sequence\", \"time\": 0.5}, \"data\": {\"CameraState\": {\"name\": " + n + ", \"clip_near\": 1.5, \"clip_far\": null,"
           " \"lod_multiplier\": 2.0, \"fov_h\": null, \"fov_v\": 0.75, \"zoom_h\": null, \"zoom_v\": 3.0}}},"
           "{\"start\": null, \"data\": {\"CameraFromTo\": {\"name\": " + n + ", \"clip_near\": {\"from\": 1.0, \"to\": 2.0}, \"clip_far\": null,"
           " \"lod_multiplier\": null, \"fov_h\": {\"from\": 0.5, \"to\": 0.25}, \"fov_v\": null, \"zoom_h\": null, \"zoom_v\": {\"from\": 1.0, \"to\": 1.0},"
           " \"run_time\": 2.0}}},"
           "{\"start\": null, \"data\": {\"FbfxCsinwaveFromTo\": {\"at_node\": {\"name\": " + n + ", \"pos\": {\"x\": 1.0, \"y\": 2.0, \"z\": 3.0}},"
           " \"screen_pos\": null, \"world_radius\": {\"from\": 5.0, \"to\": 9.0}, \"screen_radius\": null,"
           " \"csin\": {\"x\": {\"from\": 0.1, \"to\": 0.2}, \"y\": {\"from\": 0.3, \"to\": 0.3}, \"z\": {\"from\": 1.0, \"to\": 0.0}}, \"run_time\": 4.0}}},"
           "{\"start\": null, \"data\": {\"AnimVerbose\": {\"on\": true}}}]},";
  }();
  const size_t seqs = text.find("\"sequences\": [");
  EXPECT(seqs != std::string::npos);
  if (seqs == std::string::npos) return;
  text.insert(seqs + std::strlen("\"sequences\": ["), ev);
  std::vector<std::pair<std::string, std::string>> files(exported.begin(), exported.end());
  const std::string dir = tmp("mods_anim_rezbd");
  write_stored_zip(dir + "/t1.zip", files);
  const std::string out = dir + "/theirs.zbd";
  const std::string cmd = "'" + rezbd.string() + "' mw anim '" + dir + "/t1.zip' '" + out + "' >/dev/null 2>&1";
  EXPECT(std::system(cmd.c_str()) == 0);
  std::ifstream in(out, std::ios::binary);
  const std::vector<uint8_t> theirs((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::vector<uint8_t> ours;
  std::vector<std::pair<size_t, size_t>> masks;
  EXPECT(zeff::anim_from_json([&](const std::string& n) { return exported.count(n) ? exported[n] : std::string(); }, &ours, &masks, &why));
  std::vector<uint8_t> a = ours, b = theirs;
  for (const auto& [from, to] : masks)
    for (size_t k = from; k < to && k < a.size() && k < b.size(); ++k) a[k] = b[k] = 0;
  size_t at = 0;
  while (at < a.size() && at < b.size() && a[at] == b[at]) ++at;
  std::printf("  ours %zu bytes, rezbd's %zu, first difference at %zu (%s)\n", a.size(), b.size(), at, why.c_str());
  EXPECT(!theirs.empty() && a == b);
  // Effect: a def with an effects list and an Effect event (mech3ax's EffectC: effect index, node index, position).
  std::string with = exported[def];
  const size_t fx = with.find("\"effects\": null");
  EXPECT(fx != std::string::npos);
  if (fx == std::string::npos) return;
  with.replace(fx, std::strlen("\"effects\": null"), "\"effects\": [{\"name\": \"boom\", \"index\": 7, \"pad\": null}]");
  const size_t s2 = with.find("\"sequences\": [");
  with.insert(s2 + std::strlen("\"sequences\": ["), "{\"name\": \"fx\", \"seq_state\": \"OnCall\", \"reset_state\": \"OnCall\", \"pointer\": 1, \"events\": ["
              "{\"start\": null, \"data\": {\"Effect\": {\"name\": \"boom\", \"at_node\": {\"name\": \"" + node + "\", \"pos\": {\"x\": 1.0, \"y\": 2.0, \"z\": 3.0}}}}}]},");
  exported[def] = with;
  std::vector<uint8_t> fxd;
  EXPECT(zeff::anim_from_json([&](const std::string& n) { return exported.count(n) ? exported[n] : std::string(); }, &fxd, nullptr, &why));
  const uint8_t want_head[] = {3, 1, 0, 0, 28, 0, 0, 0, 0, 0, 0, 0, 1, 0};  // type 3, its size, effect 1
  const auto hit = std::search(fxd.begin(), fxd.end(), std::begin(want_head), std::end(want_head));
  EXPECT(hit != fxd.end());
  const uint8_t rec[] = {'b', 'o', 'o', 'm', 0};
  const auto r = std::search(fxd.begin(), fxd.end(), std::begin(rec), std::end(rec));
  EXPECT(r != fxd.end() && r + 36 <= fxd.end() && r[32] == 7 && std::all_of(r - 0x24, r, [](uint8_t c) { return c == 0; }));  // the blank first record
}

// A mod's world animations as unzbd's JSON (docs/modding.md): zbd/t1/anim/ (metadata.json and the rest), then unzbd's
// zbd/t1/anim.zip, read as the anim.zbd they stand for by the reader (0x0050a0e0); one that does not read refused and
// the anim.zbd read; a mod's own anim.zbd stopping the search. The globals' gravity tells them apart.
TEST(mods_anim_json_in_a_folder_or_a_zip) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/t1/anim.zbd", ec)) return;
  const fs::path unzbd = find_unzbd();
  if (unzbd.empty()) return;
  std::map<std::string, std::string> exported;
  EXPECT(unzbd_export(unzbd, "anim", install + "/zbd/t1/anim.zbd", &exported));
  const auto with_gravity = [&](const char* g) {
    std::map<std::string, std::string> m = exported;
    std::string& meta = m["metadata.json"];
    const size_t at = meta.find("\"gravity\": ");
    const size_t end = meta.find_first_of(",\n}", at + 11);
    meta.replace(at, end - at, std::string("\"gravity\": ") + g);
    return m;
  };
  SavedMounts keep;
  const std::string wd = tmp("mods_anim_json");
  const std::string mod = wd + "/mods/animmod", later = wd + "/mods/later";
  put(mod + "/mod.txt", "name=Anim\n");
  put(later + "/mod.txt", "name=Later\n");
  for (const auto& [n, t] : with_gravity("-9.0")) put(later + "/zbd/t1/anim/" + n, t);
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"animmod", "later"}, {install});
  zutl::mount(install);
  const auto gravity = [] {
    zeff::anim() = zeff::AnimState{};
    zeff::anim_allow_loading(1);
    zeff::anim_set_zbd_path("zbd\\t1\\anim.zbd");
    zeff::anim_read_zbd();
    const float g = zeff::anim().def_count() > 0 ? zeff::anim().block.get<float>(0x1c) : 0.0f;
    zeff::anim_free_all();
    zeff::anim() = zeff::AnimState{};
    return g;
  };
  EXPECT(gravity() == -9.0f);  // only the later mod has one
  {
    const auto m = with_gravity("-3.5");
    write_stored_zip(mod + "/zbd/t1/anim.zip", std::vector<std::pair<std::string, std::string>>(m.begin(), m.end()));
  }
  EXPECT(gravity() == -3.5f);
  for (const auto& [n, t] : with_gravity("-4.25")) put(mod + "/zbd/t1/anim/" + n, t);
  EXPECT(gravity() == -4.25f);  // the folder before the zip
  put(mod + "/zbd/t1/anim/metadata.json", "{\"mission\": \"MwT1V12De\",\n \"gravity\": true}");
  const float refused = gravity();
  fs::remove_all(mod + "/zbd/t1/anim", ec);
  fs::remove(mod + "/zbd/t1/anim.zip", ec);
  fs::copy_file(install + "/zbd/t1/anim.zbd", mod + "/zbd/t1/anim.zbd", ec);
  const float own = gravity();
  std::printf("  refused -> %.2f, own archive -> %.2f\n", refused, own);
  EXPECT(refused != -9.0f && refused < 0.0f && own == refused);  // the anim.zbd's, not the later mod's
}

namespace {
// A def's JSON with one field's value replaced (`key` its first occurrence).
std::string json_with(std::string text, const std::string& key, const std::string& value) {
  const size_t at = text.find("\"" + key + "\": ");
  if (at == std::string::npos) return text;
  const size_t from = at + key.size() + 4;
  size_t end = from;
  if (text[from] == '"') end = text.find('"', from + 1) + 1;
  else end = text.find_first_of(",\n}", from);
  return text.replace(from, end - from, value);
}
std::vector<uint8_t> bytes_of(const std::string& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
}  // namespace

// Animations a def at a time (docs/modding.md), on all six worlds: unzbd's def
// file names are our keys, def for def; one def replaced (its health changed)
// and one added (another's under a new anim name) leave every other def's
// bytes, the header and what follows the defs as shipped, the count one more,
// the replaced def where it stood and the added one last.
TEST(mods_anim_def_files_merge_on_every_world) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/c1/anim.zbd", ec)) return;
  const fs::path unzbd = find_unzbd();
  if (unzbd.empty()) return;
  int worlds = 0, good = 0;
  for (const char* w : {"c1", "c2", "c3", "c4", "c4b", "t1"}) {
    const std::string zbd = install + "/zbd/" + w + "/anim.zbd";
    if (!fs::exists(zbd, ec)) continue;
    ++worlds;
    std::map<std::string, std::string> exported;
    EXPECT(unzbd_export(unzbd, "anim", zbd, &exported));
    const std::vector<uint8_t> base = bytes_of(zbd);
    std::vector<mech3::anim_defs::DefSpan> spans;
    EXPECT(mech3::anim_defs::def_spans(base.data(), base.size(), &spans));
    zutl_json::Value meta;
    std::string why;
    EXPECT(zutl_json::parse(exported["metadata.json"], &meta, &why));
    const zutl_json::Value* names = meta.get("anim_def_names");
    bool keys = names && names->items.size() + 1 == spans.size();
    for (size_t i = 0; keys && i < names->items.size(); ++i) keys = names->items[i].text == spans[i + 1].key;
    // The scripts the defs index, by metadata's order: a def file names its script instead.
    const zutl_json::Value* scripts = meta.get("script_names");
    const auto beside = [&](const std::string& n) { return exported.count(n) ? exported[n] : std::string(); };
    // The replaced def: one with a script (ObjectMotionSiScript) where the world has one.
    size_t k = spans.size() / 2;
    for (size_t i = 1; i < spans.size(); ++i)
      if (exported[spans[i].key + ".json"].find("ObjectMotionSiScript") != std::string::npos) {
        k = i;
        break;
      }
    const size_t j = spans.size() / 3;
    const bool scripted = exported[spans[k].key + ".json"].find("ObjectMotionSiScript") != std::string::npos;
    std::string replaced = json_with(exported[spans[k].key + ".json"], "health", "123.5");
    std::string added = json_with(exported[spans[j].key + ".json"], "anim_name", "\"mod_added\"");
    for (std::string* t : {&replaced, &added})  // an index into metadata's scripts as the script's own file
      if (scripts)
        for (size_t s = 0; s < scripts->items.size(); ++s)
          for (size_t at; (at = t->find("\"index\": " + std::to_string(s) + "\n")) != std::string::npos;)
            t->replace(at, 9 + std::to_string(s).size(), "\"script\": \"" + scripts->items[s].text + "\"");
    mech3::anim_defs::DefFile a, b;
    const bool ra = zeff::anim_def_from_json(replaced, beside, &a.bytes, &a.key, &why);
    if (!ra) std::printf("  %s replaced: %s\n", w, why.c_str());
    const bool rb = zeff::anim_def_from_json(added, beside, &b.bytes, &b.key, &why);
    if (!rb) std::printf("  %s added: %s\n", w, why.c_str());
    const std::vector<uint8_t> out = mech3::anim_defs::with_defs(base, {a, b}, &why);
    std::vector<mech3::anim_defs::DefSpan> now;
    const bool walks = mech3::anim_defs::def_spans(out.data(), out.size(), &now);
    const auto part = [](const std::vector<uint8_t>& v, size_t from, size_t to) {
      return std::vector<uint8_t>(v.begin() + static_cast<std::ptrdiff_t>(from), v.begin() + static_cast<std::ptrdiff_t>(to));
    };
    bool same = ra && rb && a.key == spans[k].key && b.key != spans[j].key && walks && now.size() == spans.size() + 1;
    for (size_t i = 0; same && i < spans.size(); ++i)
      same = i == k ? part(out, now[i].at, now[i].at + now[i].size) == a.bytes
                    : part(out, now[i].at, now[i].at + now[i].size) == part(base, spans[i].at, spans[i].at + spans[i].size);
    // The header the same but the count (2 bytes); the added def last; what follows the defs the same.
    size_t head_diffs = 0;
    for (size_t i = 0; same && i < spans[0].at; ++i) head_diffs += base[i] != out[i];
    same = same && head_diffs <= 2 && now.back().key == b.key && part(out, now.back().at, now.back().at + now.back().size) == b.bytes;
    const size_t base_end = spans.back().at + spans.back().size;
    same = same && part(base, base_end, base.size()) == part(out, now.back().at + now.back().size, out.size());
    good += same;
    std::printf("  %s: %zu defs, keys as unzbd's %d, %s replaced%s, %s added -> %s\n", w, spans.size(), keys, a.key.c_str(),
                scripted ? " (its script by file)" : "", b.key.c_str(), same ? "every other def as shipped" : "DIFFERS");
    EXPECT(keys);
  }
  EXPECT(worlds == 6 && good == worlds);
}

// The mods' def files for a world (docs/modding.md, Animations a def at a time), on t1: per key the first mod wins, a
// mod's world folder before its world-less one; a mod with the world's whole JSON is no def folder; a def file that
// does not read is said and left out; a mod listed after the base's (a whole anim.zbd) only adds; and the world as the
// game reads it (zeff::anim_read_zbd) has the added defs after the base's, the replaced one in place.
TEST(mods_anim_def_files_by_the_mods_order) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/t1/anim.zbd", ec)) return;
  const fs::path unzbd = find_unzbd();
  if (unzbd.empty()) return;
  std::map<std::string, std::string> exported;
  EXPECT(unzbd_export(unzbd, "anim", install + "/zbd/t1/anim.zbd", &exported));
  const std::vector<uint8_t> shipped = bytes_of(install + "/zbd/t1/anim.zbd");
  std::vector<mech3::anim_defs::DefSpan> spans;
  EXPECT(mech3::anim_defs::def_spans(shipped.data(), shipped.size(), &spans));
  if (spans.size() < 10) return;
  const std::string k1 = spans[1].key, k2 = spans[2].key;
  const std::string& d1 = exported[k1 + ".json"];
  SavedMounts keep;
  const std::string wd = tmp("mods_anim_defs");
  const std::string a = wd + "/mods/a", b = wd + "/mods/b", c = wd + "/mods/c";
  for (const std::string& m : {a, b, c}) put(m + "/mod.txt", "name=M\n");
  put(a + "/zbd/t1/anim/" + k1 + ".json", json_with(d1, "health", "11.0"));          // a's world folder: k1 at 11
  put(a + "/zbd/anim/" + k1 + ".json", json_with(d1, "health", "12.0"));             // a's world-less: passed over
  put(a + "/zbd/anim/new_one.json", json_with(d1, "anim_name", "\"a_added\""));      // added
  put(a + "/zbd/anim/broken.json", "{ not json");                                     // said, left out
  put(b + "/zbd/t1/anim/" + k1 + ".json", json_with(d1, "health", "13.0"));          // after a: passed over
  put(b + "/zbd/t1/anim/" + k2 + ".json", json_with(exported[k2 + ".json"], "health", "14.0"));  // replaced
  // c: the whole anim.zbd (the base), listed after a and b; its world-less def files only add.
  fs::create_directories(c + "/zbd/t1", ec);
  fs::copy_file(install + "/zbd/t1/anim.zbd", c + "/zbd/t1/anim.zbd", ec);
  put(c + "/zbd/anim/" + spans[3].key + ".json", json_with(exported[spans[3].key + ".json"], "health", "15.0"));  // the base's own: kept
  put(c + "/zbd/anim/c_new.json", json_with(d1, "anim_name", "\"c_added\""));
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"a", "b", "c"}, {install});
  zutl::mount(install);
  std::vector<std::string> notes;
  const std::vector<mech3::anim_defs::DefFile> files = mech3::anim_defs::mod_def_files("zbd\\t1\\anim.zbd", shipped, &notes);
  std::string keys;
  for (const auto& f : files) keys += f.key + (f.from.find("/mods/a/") != std::string::npos ? "@a " : f.from.find("/mods/b/") != std::string::npos ? "@b " : "@c ");
  std::printf("  files: %s\n", keys.c_str());
  for (const std::string& n : notes) std::printf("  note: %s\n", n.c_str());
  EXPECT(files.size() == 4 && notes.size() == 4);
  if (files.size() == 4) {
    EXPECT(files[0].key == k1 && files[0].from.find("/mods/a/zbd/t1/anim/") != std::string::npos);
    EXPECT(files[1].key.find("-a_added") != std::string::npos && files[1].from.find("/mods/a/zbd/anim/") != std::string::npos);
    EXPECT(files[2].key == k2 && files[2].from.find("/mods/b/") != std::string::npos);
    EXPECT(files[3].key.find("c_added") != std::string::npos);
  }
  // As the game reads the world: 2 added after the base's defs.
  zeff::anim() = zeff::AnimState{};
  zeff::anim_allow_loading(1);
  zeff::anim_set_zbd_path("zbd\\t1\\anim.zbd");
  zeff::anim_read_zbd();
  const int32_t count = zeff::anim().def_count();
  std::string last = count > 0 && zeff::anim().defs ? zeff::anim().defs[count - 1].name() : "";
  std::printf("  read: %d defs (shipped %zu), the last %s\n", count, spans.size(), last.c_str());
  EXPECT(count == static_cast<int32_t>(spans.size()) + 2 && last == "c_added");
  // A saved running animation (Running%03d) put back only on a def of its saved name: one saved under another
  // set of mods, its index now another def's, is dropped, the def as it was (a matching one goes on as before:
  // test_anim_save.py, on the missions' worlds).
  if (count > 6) {
    const bool had = zsave::manager() != nullptr;
    if (!had) zsave::init();  // its temporary files
    zsave::Context ctx{};
    uint8_t h[0x68] = {};
    const int32_t index = 5, none = -1;
    std::memcpy(h, &index, 4);
    std::strncpy(reinterpret_cast<char*>(h + 8), zeff::anim().defs[6].name(), 0x20);
    for (int at : {0x28, 0x2c, 0x3c}) std::memcpy(h + at, &none, 4);
    h[0x4c] = 2;
    const zeff::AnimDef& d = zeff::anim().defs[index];
    uint8_t was[0x13c];
    std::memcpy(was, d.raw, sizeof was);
    zeff::running_load(ctx, "Running005", h, sizeof h, nullptr);
    const bool same = std::memcmp(was, d.raw, sizeof was) == 0;
    std::printf("  a run saved as def 6's at index 5: %s\n", same ? "dropped, the def as it was" : "PUT ON IT");
    EXPECT(same);
    if (!had) zsave::shutdown();
  }
  zeff::anim_free_all();
  zeff::anim() = zeff::AnimState{};
}

// The def files on the MODS list and in --check-mod (docs/modding.md, Animations a def at a time, step 2): on t1,
// enabled c (a whole anim.zbd), a (k1 for every world), b (k1 for t1, and a new one): a's and b's animations only add
// to t1 (c's is the base), b's k1 is a's; --check-mod reads each def file alone and names one that does not read.
TEST(mods_anim_def_files_on_the_list_and_checked) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/t1/anim.zbd", ec)) return;
  const fs::path unzbd = find_unzbd();
  if (unzbd.empty()) return;
  std::map<std::string, std::string> exported;
  EXPECT(unzbd_export(unzbd, "anim", install + "/zbd/t1/anim.zbd", &exported));
  const std::vector<uint8_t> shipped = bytes_of(install + "/zbd/t1/anim.zbd");
  std::vector<mech3::anim_defs::DefSpan> spans;
  EXPECT(mech3::anim_defs::def_spans(shipped.data(), shipped.size(), &spans));
  if (spans.size() < 3) return;
  const std::string k1 = spans[1].key;
  const std::string& d1 = exported[k1 + ".json"];
  SavedMounts keep;
  const std::string wd = tmp("mods_anim_defs_list");
  const std::string a = wd + "/mods/a", b = wd + "/mods/b", c = wd + "/mods/c", d = wd + "/mods/d";
  for (const std::string& m : {a, b, c, d}) put(m + "/mod.txt", "name=" + m.substr(m.size() - 1) + "\n");
  fs::create_directories(c + "/zbd/t1", ec);
  fs::copy_file(install + "/zbd/t1/anim.zbd", c + "/zbd/t1/anim.zbd", ec);
  put(a + "/zbd/anim/" + k1 + ".json", json_with(d1, "health", "11.0"));
  put(b + "/zbd/t1/anim/" + k1 + ".json", json_with(d1, "health", "12.0"));
  put(b + "/zbd/t1/anim/b_new.json", json_with(d1, "anim_name", "\"b_added\""));
  put(d + "/zbd/anim/broken.json", json_with(d1, "activation", "\"Sometimes\""));
  const std::vector<mech3::ModsListEntry> list = mech3::mods_list(wd, {"c", "a", "b"});
  const auto entry = [&](const char* f) -> const mech3::ModsListEntry* {
    for (const auto& e : list)
      if (e.info.folder == f) return &e;
    return nullptr;
  };
  const mech3::ModsListEntry *ea = entry("a"), *eb = entry("b");
  EXPECT(ea && eb);
  if (!ea || !eb) return;
  const std::string na = mech3::mods_row_note(*ea, list), nb = mech3::mods_row_note(*eb, list);
  std::printf("  a: %s\n  b: %s\n", na.c_str(), nb.c_str());
  EXPECT(ea->anims_taken.empty() && ea->anim_bases == std::vector<std::string>({"t1"}) && ea->conflicts_with == std::vector<std::string>({"c"}));
  EXPECT(eb->anims_taken == std::vector<std::string>({"t1 " + k1}) && eb->anim_bases == std::vector<std::string>({"t1"}));
  EXPECT(na.find("only add to t1") != std::string::npos && nb.find("1 animation") != std::string::npos);
  int errors = 0;
  std::string report;
  for (const std::string& l : mech3::mod_check(b, install, &errors)) report += l + "\n";
  EXPECT(errors == 0 && report.find("the animation " + k1 + " for t1") != std::string::npos &&
         report.find("b_added for t1") != std::string::npos);
  report.clear();
  for (const std::string& l : mech3::mod_check(d, install, &errors)) report += l + "\n";
  std::printf("  d: %s", report.substr(report.find("zbd/anim/broken.json")).c_str());
  EXPECT(errors == 1 && report.find("activation") != std::string::npos && report.find("left out") != std::string::npos);
}

// Both ways (docs/modding.md, Animations a def at a time, step 3): a mod with anim=defs keeps a whole anim.zbd for the
// original (here one whose gravity differs, at zbd\t1\ and in original\) that ours passes over for the install's,
// its def files merged in; without anim=defs the same file is the base. The MODS list takes no base from such a mod
// and its original\ files are none of its own; --build-anim writes original\zbd\t1\anim.zbd from the install's.
TEST(mods_anim_defs_only_and_build_anim) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/t1/anim.zbd", ec)) return;
  const fs::path unzbd = find_unzbd();
  if (unzbd.empty()) return;
  std::map<std::string, std::string> exported;
  EXPECT(unzbd_export(unzbd, "anim", install + "/zbd/t1/anim.zbd", &exported));
  std::vector<uint8_t> shipped = bytes_of(install + "/zbd/t1/anim.zbd");
  std::vector<mech3::anim_defs::DefSpan> spans;
  EXPECT(mech3::anim_defs::def_spans(shipped.data(), shipped.size(), &spans));
  if (spans.size() < 3) return;
  // A whole file of the mod's own: the shipped one, its gravity (block +0x1c) -2.5.
  std::vector<uint8_t> own = shipped;
  uint32_t sources;
  std::memcpy(&sources, own.data() + 8, 4);
  const float g = -2.5f;
  std::memcpy(own.data() + 0x0c + sources * 0x54 + 0x1c, &g, 4);
  SavedMounts keep;
  const std::string wd = tmp("mods_anim_defs_only");
  const std::string m = wd + "/mods/both";
  put(m + "/mod.txt", "name=Both\nanim=defs\n");
  fs::create_directories(m + "/zbd/t1", ec);
  fs::create_directories(m + "/original/zbd/t1", ec);
  std::ofstream(m + "/zbd/t1/anim.zbd", std::ios::binary).write(reinterpret_cast<const char*>(own.data()), own.size());
  std::ofstream(m + "/original/zbd/t1/anim.zbd", std::ios::binary).write(reinterpret_cast<const char*>(own.data()), own.size());
  put(m + "/zbd/t1/anim/added.json", json_with(exported[spans[1].key + ".json"], "anim_name", "\"both_added\""));
  const auto read = [&](float* gravity) {
    zutl::mounts() = zutl::Mounts{};
    zutl::set_write_dir(wd);
    mech3::mods_mount(wd, {"both"}, {install});
    zutl::mount(install);
    zeff::anim() = zeff::AnimState{};
    zeff::anim_allow_loading(1);
    zeff::anim_set_zbd_path("zbd\\t1\\anim.zbd");
    zeff::anim_read_zbd();
    const int32_t n = zeff::anim().def_count();
    *gravity = n > 0 ? zeff::anim().block.get<float>(0x1c) : 0.0f;
    zeff::anim_free_all();
    zeff::anim() = zeff::AnimState{};
    return n;
  };
  float with_defs = 0, plain = 0;
  const int32_t n1 = read(&with_defs);
  put(m + "/mod.txt", "name=Both\n");
  const int32_t n2 = read(&plain);
  std::printf("  anim=defs: %d defs, gravity %.2f; without: %d defs, gravity %.2f\n", n1, with_defs, n2, plain);
  EXPECT(n1 == static_cast<int32_t>(spans.size()) + 1 && with_defs != g);  // the install's base, its def added
  EXPECT(n2 == static_cast<int32_t>(spans.size()) + 1 && plain == g);      // its own whole file the base
  put(m + "/mod.txt", "name=Both\nanim=defs\n");
  put(wd + "/mods/later/mod.txt", "name=Later\n");
  put(wd + "/mods/later/zbd/anim/" + spans[2].key + ".json", exported[spans[2].key + ".json"]);
  put(wd + "/mods/later/original/zbd/t1/anim.zbd", "x");  // both have one: never shared, never ours
  const std::vector<mech3::ModsListEntry> list = mech3::mods_list(wd, {"both", "later"});
  for (const auto& e : list)
    if (e.info.folder == "later") EXPECT(e.anim_bases.empty() && e.conflicts_with.empty() && e.shadowed.empty());
  std::vector<std::string> lines;
  EXPECT(mech3::anim_defs::build_for_original(m, install, &lines) == 0);
  for (const std::string& l : lines) std::printf("  %s\n", l.c_str());
  const std::vector<uint8_t> built = bytes_of(m + "/original/zbd/t1/anim.zbd");
  std::vector<mech3::anim_defs::DefSpan> now;
  EXPECT(mech3::anim_defs::def_spans(built.data(), built.size(), &now) && now.size() == spans.size() + 1 && now.back().key.find("both_added") != std::string::npos);
  EXPECT(fs::exists(m + "/original/zbd/t1/anim.zbd.bak"));  // the one there before
}

// The nodes a def needs (docs/modding.md, Animations a def at a time, step 4), on c1's world: a node file (mechlib's
// smallest model from unzbd, renamed mod_muz.flt, mechlib's materials list as its own) and a def file for a one-node
// effect on it: read into the world, the def bound to it by the fixup, which runs to the end; a def file naming a node
// no world has is left out (the fixup would stop at it), the world's defs bound as shipped.
TEST(mods_anim_node_files_bind_their_defs) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/c1/gamez.zbd", ec)) return;
  const fs::path unzbd = find_unzbd();
  if (unzbd.empty()) return;
  std::map<std::string, std::string> anims, mechlib;
  EXPECT(unzbd_export(unzbd, "anim", install + "/zbd/c1/anim.zbd", &anims));
  EXPECT(unzbd_export(unzbd, "mechlib", install + "/zbd/mechlib.zbd", &mechlib));
  // The smallest model, as the node file, with mechlib's materials as its own list.
  std::string model, model_name;
  for (const auto& [n, t] : mechlib)
    if (n.size() > 9 && n.compare(n.size() - 9, 9, ".flt.json") == 0 && (model.empty() || t.size() < model.size())) model = t, model_name = n;
  zutl_json::Value doc, mats;
  std::string why;
  EXPECT(zutl_json::parse(model, &doc, &why) && zutl_json::parse(mechlib["materials.json"], &mats, &why));
  const zutl_json::Value* nodes0 = doc.get("nodes");
  const std::string root0 = nodes0 && !nodes0->items.empty() && nodes0->items[0].get("name") ? nodes0->items[0].get("name")->text : "";
  EXPECT(!root0.empty());
  std::string node_file = model;
  node_file.replace(node_file.find("\"" + root0 + "\""), root0.size() + 2, "\"mod_muz.flt\"");
  node_file.replace(node_file.rfind('}'), 1, ",\n\"materials\": " + mechlib["materials.json"] + "\n}");
  // A one-node effect def: no node or object lists, its root its own node.
  std::string def;
  zutl_json::Value meta;
  EXPECT(zutl_json::parse(anims["metadata.json"], &meta, &why));
  for (const zutl_json::Value& n : meta.get("anim_def_names")->items) {
    zutl_json::Value d;
    if (!zutl_json::parse(anims[n.text + ".json"], &d, &why)) continue;
    const zutl_json::Value *nm = d.get("name"), *rt = d.get("anim_root_name"), *ns = d.get("nodes"), *os = d.get("objects");
    const auto empty = [](const zutl_json::Value* v) { return !v || v->kind == zutl_json::Value::Null || v->items.empty(); };
    if (nm && rt && nm->text == rt->text && empty(ns) && empty(os) && anims[n.text + ".json"].find("ObjectMotionSiScript") == std::string::npos) {
      def = anims[n.text + ".json"];
      break;
    }
  }
  EXPECT(!def.empty());
  if (def.empty() || root0.empty()) return;
  const auto renamed = [&](const std::string& node, const char* anim) {
    std::string t = json_with(def, "name", "\"" + node + "\"");
    t = json_with(t, "anim_root_name", "\"" + node + "\"");
    return json_with(t, "anim_name", std::string("\"") + anim + "\"");
  };
  SavedMounts keep;
  const std::string wd = tmp("mods_anim_node_files"), m = wd + "/mods/fx";
  put(m + "/mod.txt", "name=Effects\n");
  put(m + "/zbd/anim/mod_muz.flt.json", node_file);
  put(m + "/zbd/anim/mod_muz-mod_blast.json", renamed("mod_muz.flt", "mod_blast"));
  put(m + "/zbd/anim/nowhere.json", renamed("no_such_node.flt", "bad_one"));
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"fx"}, {install});
  zutl::mount(install);
  // The program's hooks for what the fixup makes (puffers, sounds, effects), as stand-ins; restored after.
  const zeff::AnimHooks hooks_were = zeff::anim_hooks();
  static int puffer;
  zeff::anim_hooks().create_puffer = [] { return static_cast<void*>(&puffer); };
  zeff::anim_hooks().find_sound = [](const char*) -> void* { return nullptr; };
  zeff::anim_hooks().effect_index = [](const char*) { return 0; };
  zcls::lists() = zcls::Lists{};
  EXPECT(zcls::read_zbd((install + "/zbd/c1/gamez.zbd").c_str()) == 0);
  zeff::anim() = zeff::AnimState{};
  zeff::anim_allow_loading(1);
  zeff::anim_set_zbd_path("zbd\\c1\\anim.zbd");
  zeff::anim_set_world(zcls::list_head(zcls::kWorlds) ? zcls::list_head(zcls::kWorlds)->node : nullptr);
  const int r = zeff::anim_read_zbd();
  const int32_t count = zeff::anim().def_count();
  const zeff::AnimDef* last = count > 0 && zeff::anim().defs ? &zeff::anim().defs[count - 1] : nullptr;
  int bound = 0;
  for (int32_t i = 1; i < count; ++i) bound += zeff::anim().defs[i].node != nullptr;
  std::printf("  node file from %s; read %d, %d defs (%d bound), the last %s on %s\n", model_name.c_str(), r, count, bound,
              last ? last->name() : "-", last && last->node ? last->node->name() : "nothing");
  EXPECT(r == 0 && count == static_cast<int32_t>(meta.get("anim_def_names")->items.size()) + 2);  // def 0, the shipped, mod_blast
  EXPECT(last && std::strcmp(last->name(), "mod_blast") == 0 && last->node && std::strcmp(last->node->name(), "mod_muz.flt") == 0);
  EXPECT(bound == count - 1);
  EXPECT(!zeff::anim_find("bad_one"));
  for (int32_t i = 1; i < count; ++i)  // the stand-in puffers are no real ones: not freed
    for (int k = 1; k < zeff::anim().defs[i].count(0x10c); ++k) zeff::anim().defs[i].puffers[k].made = nullptr;
  zeff::anim_free_all();
  zeff::anim() = zeff::AnimState{};
  zcls::lists() = zcls::Lists{};
  zeff::anim_hooks() = hooks_were;
  // --check-mod reads the node file (both halves, no world needed) and names one without its materials;
  // --build-anim says the original needs the node in gamez.zbd.
  int errors = 0;
  std::string report;
  for (const std::string& l : mech3::mod_check(m, install, &errors)) report += l + "\n";
  const bool node_ok = report.find("a node of its own") != std::string::npos;
  put(wd + "/mods/broken/mod.txt", "name=B\n");
  put(wd + "/mods/broken/zbd/anim/x.flt.json", model);
  int broken_errors = 0;
  std::string broken;
  for (const std::string& l : mech3::mod_check(wd + "/mods/broken", install, &broken_errors)) broken += l + "\n";
  std::vector<std::string> lines;
  mech3::anim_defs::build_for_original(m, install, &lines);
  bool noted = false;
  for (const std::string& l : lines) noted = noted || l.find("mod_muz.flt.json is a node file") != std::string::npos;
  std::printf("  check: node %d, broken %d (%s)\n", node_ok, broken_errors, broken.find("no materials") != std::string::npos ? "no materials" : "?");
  EXPECT(node_ok && broken_errors == 1 && broken.find("no materials list") != std::string::npos && noted);
}

// What a world's anim JSON refuses, with the file, line and place (docs/modding.md).
TEST(mods_anim_json_errors_name_the_place) {
  const auto def = [](const std::string& seq_events, const std::string& extra = "") {
    return "{\"name\": \"box\", \"anim_name\": \"hit_box\", \"anim_root_name\": \"box\", \"has_callbacks\": false,"
           " \"auto_reset_node_states\": false, \"local_nodes_only\": false, \"proximity_damage\": false, \"active\": true,"
           " \"low_priority\": false, \"activation\": \"OnCall\", \"execution\": \"None\", \"network_log\": null, \"save_log\": null,"
           " \"reset_time\": null, \"health\": 0.0, \"activ_prereq_min_to_satisfy\": 0, \"objects\": null,"
           " \"nodes\": [{\"name\": \"box\", \"ptr\": 1}], \"lights\": null, \"puffers\": null, \"dynamic_sounds\": null, \"static_sounds\": null,"
           " \"effects\": null, \"activ_prereqs\": null, \"anim_refs\": null, \"reset_state\": null," + extra +
           " \"sequences\": [{\"name\": \"s\", \"seq_state\": \"Initial\", \"reset_state\": \"Initial\", \"pointer\": 1,\n \"events\": [" +
           seq_events + "]}], \"ptrs\": null}";
  };
  const std::string meta = "{\"mission\": \"MwT1V12De\", \"gravity\": -9.8, \"anim_def_names\": [\"box\"], \"script_names\": [], \"anim_list\": []}";
  const auto run = [&](const std::string& m, const std::string& d) {
    std::vector<uint8_t> zbd;
    std::string why;
    const bool ok = zeff::anim_from_json([&](const std::string& n) { return n == "metadata.json" ? m : n == "box.json" ? d : std::string(); }, &zbd,
                                         nullptr, &why);
    std::printf("  -> %s\n", ok ? "read" : why.c_str());
    return ok ? std::string() : why;
  };
  EXPECT(run(meta, def("{\"start\": null, \"data\": {\"ObjectActiveState\": {\"node\": \"box\", \"state\": true}}}")).empty());
  const std::pair<std::string, const char*> cases[] = {
      {def("{\"start\": null, \"data\": {\"Teleport\": {}}}"), "box.json: line 2 (at $.sequences[0].events[0].data): not an event this format has: Teleport"},
      {def("{\"start\": null, \"data\": {\"ObjectActiveState\": {\"node\": \"lid\", \"state\": true}}}"),
       "(at $.sequences[0].events[0].data.ObjectActiveState.node): no node \"lid\" in the def's nodes"},
      {def("{\"start\": null, \"data\": {\"Loop\": {\"Forever\": 1}}}"), "(at $.sequences[0].events[0].data.Loop): not Count or RunTime"},
      {def("{\"start\": {\"offset\": \"Later\", \"time\": 1.0}, \"data\": {\"Endif\": {}}}"), "(at $.sequences[0].events[0].start.offset): not one of"},
      {def("{\"start\": null, \"data\": {\"Sound\": {\"name\": \"bang\", \"at_node\": null}}}"),
       "(at $.sequences[0].events[0].data.Sound.name): no sound \"bang\" in the def's static sounds"},
      {def("", "\"activ_prereqs\": [{\"Sometimes\": {}}],"), "(at $.activ_prereqs[0]): not Animation, Object or Parent"},
      {def("{\"start\": null, \"data\": {\"CallAnimation\": {\"name\": \"a_name_far_longer_than_thirty_one_chars\", \"operand_node\": null,"
           " \"wait_for_completion\": null, \"parameters\": null}}}"),
       "(at $.sequences[0].events[0].data.CallAnimation.name): longer than 31 characters"},
  };
  for (const auto& [d, want] : cases) {
    std::string d2 = d;
    if (const size_t dup = d2.find("\"activ_prereqs\": null,"); dup != std::string::npos && d2.find("\"activ_prereqs\": [") != std::string::npos)
      d2.erase(dup, std::strlen("\"activ_prereqs\": null,"));
    EXPECT(run(meta, d2).find(want) != std::string::npos);
  }
  EXPECT(run("{\"mission\": \"MwC9V10\", \"gravity\": -9.8, \"anim_def_names\": [], \"script_names\": [], \"anim_list\": []}", "")
             .find("metadata.json: line 1 (at $.mission): not a MechWarrior 3 world mech3ax knows") != std::string::npos);
  EXPECT(run(meta, "").find("box.json: not there") != std::string::npos);
}

// Each world's gamez.zbd from unzbd's own JSON (find_unzbd): the shipped file's bytes. While the models and nodes are
// not read, the header's known fields and the texture directory and materials are compared.
TEST(mods_gamez_json_is_every_worlds_file) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/c1/gamez.zbd", ec)) return;
  const fs::path unzbd = find_unzbd();
  if (unzbd.empty()) return;
  int worlds = 0, same = 0;
  for (const char* w : {"c1", "c2", "c3", "c4", "c4b", "t1"}) {
    const std::string zbd = install + "/zbd/" + w + "/gamez.zbd";
    if (!fs::exists(zbd, ec)) continue;
    ++worlds;
    std::map<std::string, std::string> exported;
    EXPECT(unzbd_export(unzbd, "gamez", zbd, &exported));
    std::ifstream in(zbd, std::ios::binary);
    const std::vector<uint8_t> shipped((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<uint8_t> ours;
    std::string why;
    const bool ok = zcls::gamez_from_json([&](const std::string& n) { return exported.count(n) ? exported[n] : std::string(); }, &ours, &why);
    std::vector<uint8_t> a = ours, b = shipped;
    if (!ok) {
      b.resize(std::min(b.size(), a.size()));
      for (size_t k : {32})  // the offset past what is read
        for (size_t c = 0; c < 4 && k + c < a.size(); ++c) a[k + c] = b[k + c] = 0;
    }
    size_t at = 0;
    while (at < a.size() && at < b.size() && a[at] == b[at]) ++at;
    const bool equal = a.size() == b.size() && at == a.size();
    same += ok && equal;
    std::printf("  %s: %zu bytes vs %zu, %s%s%s\n", w, ours.size(), shipped.size(),
                equal ? (ok ? "the same" : "the same so far") : ("first difference at " + std::to_string(at)).c_str(), ok ? "" : "; ", ok ? "" : why.c_str());
    EXPECT(equal);
  }
  EXPECT(worlds == 6);
}

// The pull reader for files too big for one tree (gamez's models.json): members and items stepped through, an item
// read whole as a small tree, its path kept; its errors name the line and place as the tree reader's do.
TEST(mods_json_pull_walks_and_names_the_place) {
  const std::string doc = "{\"skip\": [1, {\"a\": 2}], \"models\": [\n {\"n\": 1},\n {\"n\": 2},\n {\"n\": 3}\n]}";
  zutl_json::Pull p(doc);
  std::string key;
  std::vector<std::string> seen;
  int sum = 0;
  EXPECT(p.begin_object());
  while (p.member(&key)) {
    if (key != "models") {
      EXPECT(p.skip());
      continue;
    }
    EXPECT(p.begin_list());
    while (p.item()) {
      zutl_json::Value v;
      EXPECT(p.value(&v));
      seen.push_back(p.path() + "@" + std::to_string(v.line));
      sum += std::atoi(v.get("n")->text.c_str());
    }
  }
  EXPECT(p.why().empty() && p.end());
  EXPECT(sum == 6 && seen.size() == 3 && seen[0] == "$.models[0]@2" && seen[2] == "$.models[2]@4");
  const std::pair<std::string, const char*> bad[] = {
      {"{\"models\": [{\"n\": 1}\n {\"n\": 2}]}", "line 2 (at $.models[0]): '{' where a ',' or ']' should be"},
      {"{\"models\": [{\"n\": 1},\n {\"n\": }]}", "line 2 (at $.models[1].n): '}' where a value should be"},
      {"{\"models\" [1]}", "line 1 (at $): no ':' after the name \"models\""},
      {"{\"models\": [1,", "line 1 (at $.models[1]): the file ends where a value should be"},
  };
  for (const auto& [text, want] : bad) {
    zutl_json::Pull q(text);
    bool ok = q.begin_object();
    while (ok && q.member(&key)) {
      if (!(ok = q.begin_list())) break;
      zutl_json::Value v;
      while (ok && q.item()) ok = q.value(&v);
    }
    std::printf("  -> %s\n", q.why().c_str());
    EXPECT(q.why().find(want) != std::string::npos);
  }
}

// A mod's world as unzbd's JSON (docs/modding.md): zbd/t1/gamez/ (the five files), then unzbd's zbd/t1/gamez.zip,
// opened as the gamez.zbd they stand for (0x004ff6a0, which both the load and the re-read use); one that does not
// read refused and the gamez.zbd opened; a mod's own gamez.zbd stopping the search. The header's last free node,
// from metadata.json, tells them apart; the rest of the file is the shipped one's (mods_gamez_json_is_every_worlds_file).
TEST(mods_gamez_json_in_a_folder_or_a_zip) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/t1/gamez.zbd", ec)) return;
  const fs::path unzbd = find_unzbd();
  if (unzbd.empty()) return;
  std::map<std::string, std::string> exported;
  EXPECT(unzbd_export(unzbd, "gamez", install + "/zbd/t1/gamez.zbd", &exported));
  const auto with_last_free = [&](int n) {
    std::map<std::string, std::string> m = exported;
    std::string& meta = m["metadata.json"];
    const size_t at = meta.find("\"node_last_free\": ");
    const size_t end = meta.find_first_of(",\n}", at + 18);
    meta.replace(at, end - at, "\"node_last_free\": " + std::to_string(n));
    return m;
  };
  SavedMounts keep;
  const std::string wd = tmp("mods_gamez_json");
  const std::string mod = wd + "/mods/worldmod", later = wd + "/mods/later";
  put(mod + "/mod.txt", "name=World\n");
  put(later + "/mod.txt", "name=Later\n");
  for (const auto& [n, t] : with_last_free(5000)) put(later + "/zbd/t1/gamez/" + n, t);
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"worldmod", "later"}, {install});
  zutl::mount(install);
  const auto last_free = [] {
    zcls::ZbdHeader h{};
    std::FILE* f = zcls::open_zbd("zbd\\t1\\gamez.zbd", &h);
    if (!f) return -2;
    std::fclose(f);
    return h.node_last_free;
  };
  const int shipped = [&] {
    std::ifstream in(install + "/zbd/t1/gamez.zbd", std::ios::binary);
    zcls::ZbdHeader h{};
    in.read(reinterpret_cast<char*>(&h), sizeof h);
    return h.node_last_free;
  }();
  EXPECT(last_free() == 5000);  // only the later mod has one
  {
    const auto m = with_last_free(4000);
    write_stored_zip(mod + "/zbd/t1/gamez.zip", std::vector<std::pair<std::string, std::string>>(m.begin(), m.end()));
  }
  EXPECT(last_free() == 4000);
  for (const auto& [n, t] : with_last_free(3000)) put(mod + "/zbd/t1/gamez/" + n, t);
  EXPECT(last_free() == 3000);  // the folder before the zip
  put(mod + "/zbd/t1/gamez/metadata.json", "{\"material_array_size\": 1000,\n \"model_array_size\": \"many\"}");
  const int refused = last_free();
  fs::remove_all(mod + "/zbd/t1/gamez", ec);
  fs::remove(mod + "/zbd/t1/gamez.zip", ec);
  fs::copy_file(install + "/zbd/t1/gamez.zbd", mod + "/zbd/t1/gamez.zbd", ec);
  const int own = last_free();
  std::printf("  shipped %d, refused -> %d, own archive -> %d\n", shipped, refused, own);
  EXPECT(refused == shipped && own == shipped);
}

// What a world's JSON refuses, with the file, line and place (docs/modding.md).
TEST(mods_gamez_json_errors_name_the_place) {
  const std::string box = "{\"a\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"b\": {\"x\": 0, \"y\": 0, \"z\": 0}}";
  const auto node = [&](const std::string& data, const std::string& parents = "[]", const std::string& flags = "{\"active\": true}") {
    return "{\"name\": \"n\", \"flags\": " + flags + ", \"update_flags\": 0, \"zone_id\": -1, \"model_index\": -1, \"area_partition\": null,"
           " \"parent_indices\": " + parents + ", \"child_indices\": [], \"node_bbox\": " + box + ", \"model_bbox\": " + box +
           ", \"child_bbox\": " + box + ", \"field192\": 0, \"field196\": 0, \"field200\": 0, \"field204\": 0,\n \"data\": " + data +
           ", \"data_ptr\": 0, \"parent_array_ptr\": 0, \"child_array_ptr\": 0}";
  };
  std::map<std::string, std::string> base = {
      {"metadata.json", "{\"material_array_size\": 4, \"model_array_size\": 4, \"node_array_size\": 4, \"node_last_free\": -1}"},
      {"textures.json", "[{\"name\": \"rock.tif\"}]"},
      {"materials.json", "[{\"Textured\": {\"texture_index\": 0}}]"},
      {"models.json", "[]"},
      {"nodes.json", "[" + node("{\"Display\": {\"origin_x\": 0, \"origin_y\": 0, \"resolution_x\": 640, \"resolution_y\": 480,"
                                " \"clear_color\": {\"r\": 0, \"g\": 0, \"b\": 1}}}") + "]"},
  };
  const auto run = [&](const std::map<std::string, std::string>& m) {
    std::vector<uint8_t> zbd;
    std::string why;
    const bool ok = zcls::gamez_from_json([&](const std::string& n) { return m.count(n) ? m.at(n) : std::string(); }, &zbd, &why);
    std::printf("  -> %s\n", ok ? "read" : why.c_str());
    return ok ? std::string() : why;
  };
  EXPECT(run(base).empty());
  const auto with = [&](const char* f, const std::string& t) {
    auto m = base;
    m[f] = t;
    return m;
  };
  const std::pair<std::map<std::string, std::string>, const char*> cases[] = {
      {with("materials.json", "[{\"Textured\": {\"texture_index\": 3}}]"), "materials.json: line 1 (at $[0].Textured.texture_index): 3: past 0..0"},
      {with("materials.json", "[{\"Shiny\": {}}]"), "materials.json: line 1 (at $[0]): not Textured or Colored"},
      {with("metadata.json", "{\"material_array_size\": 1, \"model_array_size\": 4, \"node_array_size\": 4, \"node_last_free\": -1}"),
       "materials.json: line 1 (at $): 1 materials, where the array holds 1"},
      {with("models.json", "[{\"model_type\": \"Blob\"}]"), "models.json: line 1 (at $[0].model_type): not one of Default, Facade, Points"},
      {with("nodes.json", "[" + node("{\"Portal\": {}}") + "]"), "nodes.json: line 2 (at $[0].data): not a node class this format has: Portal"},
      {with("nodes.json", "[" + node("\"Empty\"", "[0, 1]") + "]"), "nodes.json: line 1 (at $[0]): an empty node has one parent"},
      {with("nodes.json", "[" + node("\"Empty\"", "[0]", "{\"glowing\": true}") + "]"), "nodes.json: line 1 (at $[0].flags.glowing): not a flag this format has"},
      {with("nodes.json", "[" + node("{\"Display\": {\"origin_x\": 0}}") + "]"), "nodes.json: line 2 (at $[0].data.Display): no \"origin_y\""},
      {with("nodes.json", "[" + node("\"Empty\"", "[0]") + ",\n ]"), "nodes.json: line 3 (at $[1]): ']' where a value should be"},
      {with("textures.json", ""), "textures.json: line 1 (at $): the file ends where a value should be"},
  };
  for (const auto& [m, want] : cases) EXPECT(run(m).find(want) != std::string::npos);
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
  put(good + "/zbd/reader/fmv.zrd.json", "[\"INTRO\", [\"PLAYAVI\", \"intro\"]]");
  write_stored_zip(good + "/zbd/reader.zip", {{"fmv.zrd.json", "[\"INTRO\", [\"PLAYAVI\", \"intro\"]]"}, {"notes.txt", "x"}});
  const std::string frame = "{\"translation\": {\"x\": 0, \"y\": 1.5, \"z\": 0}, \"rotation\": {\"x\": 1, \"y\": 0, \"z\": 0, \"w\": 0}}";
  const std::string motion = "{\"loop_time\": 2.0, \"frame_count\": 1, \"parts\": [{\"name\": \"hip\", \"frames\": [" + frame + "]}]}";
  put(good + "/zbd/motion/madcat_walk.json", motion);
  write_stored_zip(good + "/zbd/motion.zip", {{"madcat_run.json", motion}, {"manifest.json", "[]"}});
  put(good + "/zbd/mechlib/materials.json", "[{\"Colored\": {\"color\": {\"r\": 1, \"g\": 2, \"b\": 3}, \"alpha\": 255}}]");
  write_stored_zip(good + "/zbd/mechlib.zip", {{"materials.json", "[]"}});
  // the least world: one texture, one material, no models, one empty node under... none: a world node alone
  const std::string world_node = "[{\"name\": \"world1\", \"flags\": {\"active\": true}, \"update_flags\": 0, \"zone_id\": -1, \"model_index\": -1,"
      " \"area_partition\": null, \"parent_indices\": [], \"child_indices\": [], \"node_bbox\": {\"a\": {\"x\": 0, \"y\": 0, \"z\": 0},"
      " \"b\": {\"x\": 0, \"y\": 0, \"z\": 0}}, \"model_bbox\": {\"a\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"b\": {\"x\": 0, \"y\": 0, \"z\": 0}},"
      " \"child_bbox\": {\"a\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"b\": {\"x\": 0, \"y\": 0, \"z\": 0}}, \"field192\": 0, \"field196\": 0,"
      " \"field200\": 0, \"field204\": 0, \"data\": \"Empty\", \"data_ptr\": 0, \"parent_array_ptr\": 0, \"child_array_ptr\": 0}]";
  put(good + "/zbd/t1/gamez/metadata.json", "{\"material_array_size\": 4, \"model_array_size\": 4, \"node_array_size\": 4, \"node_last_free\": -1}");
  put(good + "/zbd/t1/gamez/textures.json", "[{\"name\": \"rock.tif\"}]");
  put(good + "/zbd/t1/gamez/materials.json", "[{\"Textured\": {\"texture_index\": 0}}]");
  put(good + "/zbd/t1/gamez/models.json", "[]");
  put(good + "/zbd/t1/gamez/nodes.json", "[]");
  put(good + "/zbd/t1/anim/metadata.json",
      "{\"mission\": \"MwT1V12De\", \"gravity\": -9.8, \"anim_def_names\": [], \"script_names\": [], \"anim_list\": []}");
  if (fs::exists(install + "/zbd/c1/readerm1.zbd", ec)) {
    fs::create_directories(good + "/zbd/c1", ec);
    fs::copy_file(install + "/zbd/c1/readerm1.zbd", good + "/zbd/c1/readerm1.zbd", ec);
  }
  int errors = -1;
  std::string text;
  for (const std::string& l : mech3::mod_check(good, install, &errors)) text += l + "\n";
  std::printf("%s", text.c_str());
  EXPECT(errors == 0 && text.find("mod.txt: Good 1") != std::string::npos && text.find("zbd/fmv.zrd: added") != std::string::npos);
  EXPECT(text.find("zbd/reader/fmv.zrd.json: added") != std::string::npos && text.find("zbd/reader.zip: added") != std::string::npos);
  EXPECT(text.find("zbd/motion/madcat_walk.json: added") != std::string::npos && text.find("zbd/motion.zip: added") != std::string::npos);
  EXPECT(text.find("zbd/mechlib/materials.json: added") != std::string::npos && text.find("zbd/mechlib.zip: added") != std::string::npos);
  EXPECT(text.find("its animations read from unzbd's JSON") != std::string::npos);
  EXPECT(text.find("its world read from unzbd's JSON") != std::string::npos);
  if (fs::exists(install + "/zbd/c1/readerm1.zbd", ec))
    EXPECT(text.find("zbd/c1/readerm1.zbd: replaces the game's") != std::string::npos && text.find("0 errors") != std::string::npos);
  // Bad: no name, a world without files, an unknown Instant Action world, a
  // garbage archive, a reader archive with a broken entry, a broken .zrd.
  const std::string bad = root + "/bad";
  put(bad + "/mod.txt", "version=1\nworld=nofiles\nia_world=nowhere\n");
  put(bad + "/zbd/rimage.zbd", "not an archive at all");
  put(bad + "/zbd/broken.zrd", "(UNCLOSED\n");
  put(bad + "/pilots/x.pil", "x");
  put(bad + "/zbd/reader/fmv.zrd.json", "[\"INTRO\",\n [\"PLAYAVI\", true]]");
  write_stored_zip(bad + "/zbd/reader.zip", {{"fmv.zrd.json", "[1"}});
  put(bad + "/zbd/c1/readerm1.zip", "not a zip");
  put(bad + "/zbd/motion/madcat_walk.json", "{\"loop_time\": 2.0,\n \"frame_count\": 0, \"parts\": []}");
  write_stored_zip(bad + "/zbd/motion.zip", {{"madcat_run.json", "{\"loop_time\": -1.0, \"frame_count\": 1, \"parts\": []}"}});
  put(bad + "/zbd/mechlib/materials.json", "[{\"Colored\": {\"color\": {\"r\": 1, \"g\": 2, \"b\": 3},\n \"alpha\": 300}}]");
  write_stored_zip(bad + "/zbd/mechlib.zip", {{"mech_x.flt.json", "{\"nodes\": []}"}});
  put(bad + "/zbd/c2/gamez/metadata.json", "{\"material_array_size\": 4}");
  write_stored_zip(bad + "/zbd/c1/gamez.zip", {{"metadata.json", "{\"material_array_size\": 4, \"model_array_size\": 4, \"node_array_size\": 4,"
                                                                 " \"node_last_free\": -1}"}, {"textures.json", "[]"}, {"materials.json", "[]"},
                                                {"models.json", "[]"}, {"nodes.json", world_node}});
  write_stored_zip(bad + "/zbd/t1/anim.zip", {{"metadata.json", "{\"mission\": \"MwT1V12De\", \"gravity\": -9.8, \"anim_def_names\": [\"gone\"],"
                                                                " \"script_names\": [], \"anim_list\": []}"}});
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
  EXPECT(text.find("error: zbd/reader/fmv.zrd.json: line 2 (at $[1][1]): true or false") != std::string::npos &&
         text.find("error: zbd/reader.zip:fmv.zrd.json: line 1 (at $): a '[' never closed") != std::string::npos &&
         text.find("error: zbd/c1/readerm1.zip: not a zip the game can read") != std::string::npos);
  EXPECT(text.find("error: zbd/motion/madcat_walk.json: line 2 (at $.frame_count): 0: a motion has at least one frame") != std::string::npos &&
         text.find("error: zbd/motion.zip:madcat_run.json: line 1 (at $.loop_time): not above 0") != std::string::npos);
  EXPECT(text.find("error: zbd/mechlib/materials.json: line 2 (at $[0].Colored.alpha): 300: past 0..255") != std::string::npos &&
         text.find("error: zbd/mechlib.zip:mech_x.flt.json: line 1 (at $): no \"models\"") != std::string::npos);
  EXPECT(text.find("error: zbd/t1/anim.zip: gone.json: not there") != std::string::npos);
  EXPECT(text.find("error: zbd/c1/gamez.zip: nodes.json: line 1 (at $[0]): an empty node has one parent") != std::string::npos);
  EXPECT(text.find("error: zbd/c2/gamez/: metadata.json: line 1 (at $): no \"model_array_size\"") != std::string::npos);
  EXPECT(errors == 18 && text.find("error: zbd/mechlib.zbd: its version, format or materials cannot be read") != std::string::npos &&
         text.find("error: zbd/motion.zbd: motion walk cannot be read") != std::string::npos);
  EXPECT(text.find("error: mod.txt: mod.txt has no name= line") != std::string::npos &&
         text.find("error: world nofiles: no zbd/nofiles/gamez.zbd") != std::string::npos &&
         text.find("error: ia_world nowhere: no such world") != std::string::npos &&
         text.find("error: zbd/rimage.zbd: not a texture library the game can read") != std::string::npos &&
         text.find("error: zbd/c1/readerm1.zbd: entry mission.zrd cannot be read") != std::string::npos &&
         text.find("error: zbd/broken.zrd: line 1: a '(' never closed") != std::string::npos &&
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
         text.find("error: zbd/broken.zrd: line 1: a '(' never closed") != std::string::npos);
}

// Custom campaigns (docs/custom-campaigns.md): the file's lines, every one
// read or the whole campaign refused, naming the line.
TEST(mods_campaign_file) {
  mech3::ModCampaign c;
  EXPECT(mech3::campaign_parse("\xef\xbb\xbfname=Hard Rain\nstock=default\nmovie_start=campaign\n# a comment\n"
                               "mission=t1 1\nop=Operation 1 bases=3\nmission=c1 1 brief_c11 c1m1\nmission=mymod_w1 2\n"
                               "op=Operation 2\nmission=c4 4 - c4m4\nmission=c4b 4 bases=2\nmovie_end=c4win\n", &c));
  EXPECT(c.name == "Hard Rain" && c.stock == "default" && c.movie_start == "campaign" && c.movie_end == "c4win" && c.missions.size() == 5);
  if (c.missions.size() == 5) {
    const auto& m = c.missions;
    EXPECT(m[0].world == "t1" && m[0].number == 1 && m[0].op.empty() && m[0].bases == -1 && m[0].line == 5);
    EXPECT(m[1].world == "c1" && m[1].briefing == "brief_c11" && m[1].movie == "c1m1" && m[1].op == "Operation 1" && m[1].bases == 3);
    EXPECT(m[2].world == "mymod_w1" && m[2].number == 2 && m[2].bases == -1 && m[2].briefing.empty());
    EXPECT(m[3].op == "Operation 2" && m[3].bases == -1 && m[3].briefing.empty() && m[3].movie == "c4m4");
    EXPECT(m[4].world == "c4b" && m[4].bases == 2 && m[4].line == 11);
  }
  auto refused = [](const std::string& text, const std::string& why) {
    mech3::ModCampaign r;
    const bool ok = mech3::campaign_parse(text, &r);
    if (ok || r.error != why) std::printf("  campaign error: %s\n", r.error.c_str());
    return !ok && r.error == why;
  };
  EXPECT(refused("name=X\nmisson=c1 1\n", "line 2: misson=c1 1: no such key (name, stock, movie_start, movie_end, op, mission)"));
  EXPECT(refused("name=X\nmission c1 1\n", "line 2: mission c1 1: no key=value here"));
  EXPECT(refused("name=X\nmission=c1\n", "line 2: mission=c1: mission=<world> <number> [briefing] [movie] [bases=N]"));
  EXPECT(refused("name=X\nmission=c1 0\n", "line 2: mission=c1 0: 0: the mission's number is 1 to 99 (its m<n> block)"));
  EXPECT(refused("name=X\nmission=c1 1x\n", "line 2: mission=c1 1x: 1x: the mission's number is 1 to 99 (its m<n> block)"));
  EXPECT(refused("name=X\nmission=../c1 1\n", "line 2: mission=../c1 1: the world is a folder's name, one word"));
  EXPECT(refused("name=X\nmission=c1 1 a b c\n", "line 2: mission=c1 1 a b c: mission=<world> <number> [briefing] [movie] [bases=N]"));
  EXPECT(refused("name=X\nmission=c1 1 bases=4\n", "line 2: mission=c1 1 bases=4: bases=4: the field bases are 0 to 3"));
  EXPECT(refused("name=X\nop=bases=3\nmission=c1 1\n", "line 2: op=bases=3: an operation needs its heading"));
  EXPECT(refused("name=X\nop=A\nop=B\nmission=c1 1\n", "line 2: an op= with no mission after it"));
  EXPECT(refused("name=X\nmission=c1 1\nop=A\n", "line 3: an op= with no mission after it"));
  EXPECT(refused("name=X\nstock=mine.txt\nmission=c1 1\n",
                 "line 2: stock=mine.txt: only stock=default (the original's new-campaign stock) for now"));
  EXPECT(refused("name=X\nname=Y\nmission=c1 1\n", "line 2: name=Y: a second name= line"));
  EXPECT(refused("name=X\nmovie_end=c4 win\nmission=c1 1\n", "line 2: movie_end=c4 win: a movie is fmv.zrd's name, one word"));
  EXPECT(refused("mission=c1 1\n", "no name= line"));
  EXPECT(refused("name=X\n# none\n", "no mission= line"));
}

// Several campaigns per mod, each its own pilots' folder; one that does not
// read, or names a world no enabled mod or the game has, left out whole.
TEST(mods_campaigns_mounted) {
  SavedMounts keep;
  const std::string wd = tmp("mods_campaigns"), install = wd + "/install";
  put(install + "/zbd/c1/gamez.zbd", "x");
  put(install + "/zbd/c4/gamez.zbd", "x");
  put(wd + "/mods/a/mod.txt", "name=A\nworld=aworld\ncampaign=one.txt\ncampaign=camps/two.txt\ncampaign=again/ONE.txt\n"
                              "campaign=../x.txt\ncampaign=gone.txt\ncampaign=bad.txt\ncampaign=lost.txt\ncampaign=noc2.txt\n"
                              "campaign=uses_b.txt\ncampaign=my camp.txt\n");
  put(wd + "/mods/a/zbd/aworld/gamez.zbd", "x");
  put(wd + "/mods/a/one.txt", "name=One\nmission=c1 1\nmission=aworld 1\n");
  put(wd + "/mods/a/camps/two.txt", "name=Two\nmission=c4 4\n");
  put(wd + "/mods/a/again/ONE.txt", "name=One again\nmission=c1 1\n");
  put(wd + "/mods/a/bad.txt", "name=Bad\nmission=c1 1\nmision=c1 2\n");
  put(wd + "/mods/a/lost.txt", "name=Lost\nmission=c1 1\nmission=nowhere 1\n");
  put(wd + "/mods/a/noc2.txt", "name=No c2\nmission=c2 1\n");  // a game world the install lacks
  put(wd + "/mods/a/uses_b.txt", "name=Uses B\nmission=bworld 2\n");
  put(wd + "/mods/b/mod.txt", "name=B\nworld=bworld\n");
  put(wd + "/mods/b/zbd/bworld/gamez.zbd", "x");
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mod_worlds().clear(), mech3::mod_ia_worlds().clear();
  std::string log;
  for (const std::string& l : mech3::mods_mount(wd, {"a", "b"}, {install})) log += l + "\n";
  const auto& cs = mech3::mod_campaigns();
  EXPECT(cs.size() == 3);
  if (cs.size() == 3) {
    EXPECT(cs[0].name == "One" && cs[0].id == "one" && cs[0].mod == "a" && mech3::campaign_pilots_dir(cs[0]) == "pilots/mods/a/one");
    EXPECT(cs[1].name == "Two" && cs[1].id == "two" && mech3::campaign_pilots_dir(cs[1]) == "pilots/mods/a/two");
    EXPECT(cs[2].name == "Uses B");
  }
  auto has = [&](const char* s) {
    if (log.find(s) == std::string::npos) std::printf("  missing from the log: %s\n", s);
    return log.find(s) != std::string::npos;
  };
  EXPECT(has("campaign again/ONE.txt: the mod has a campaign ONE already, left out"));
  EXPECT(has("campaign ../x.txt: campaign=../x.txt: a file inside the mod, left out"));
  EXPECT(has("campaign gone.txt: campaign=gone.txt: no such file in the mod, left out"));
  EXPECT(has("campaign bad.txt: line 3: mision=c1 2: no such key"));
  EXPECT(has("campaign my camp.txt: campaign=my camp.txt: its name (its pilots' folder) is letters, digits, _ and -, up to 32, left out"));
  EXPECT(has("mod a: campaign lost.txt: line 3: no world nowhere in an enabled mod or the game, the campaign left out"));
  EXPECT(has("mod a: campaign noc2.txt: line 2: no world c2 in an enabled mod or the game, the campaign left out"));
  EXPECT(has("mod a: campaign \"One\" (one.txt, 2 missions) added"));
  // b off: its world gone, the campaign on it left out; a remount starts over.
  mech3::mod_worlds().clear();
  log.clear();
  for (const std::string& l : mech3::mods_mount(wd, {"a"}, {install})) log += l + "\n";
  EXPECT(mech3::mod_campaigns().size() == 2 && has("campaign uses_b.txt: line 2: no world bworld"));
  mech3::mod_worlds().clear(), mech3::mod_ia_worlds().clear(), mech3::mod_campaigns().clear();
}

TEST(mods_check_mod_reads_campaigns) {
  const std::string root = tmp("mods_check_campaigns"), install = root + "/install", mod = root + "/m";
  put(install + "/zbd/c1/gamez.zbd", "x");
  put(mod + "/mod.txt", "name=M\nworld=mw\ncampaign=good.txt\ncampaign=bad.txt\ncampaign=more/GOOD.txt\n");
  put(mod + "/mw.gs", "x");
  put(mod + "/zbd/mw/gamez.zbd", "x");
  put(mod + "/good.txt", "name=Good\nmission=c1 1\nmission=mw 1\nmission=other 3\n");
  put(mod + "/bad.txt", "name=Bad\nmission=c1\n");
  put(mod + "/more/GOOD.txt", "name=Again\nmission=c1 1\n");
  int errors = 0;
  std::string text;
  for (const std::string& l : mech3::mod_check(mod, install, &errors)) text += l + "\n";
  auto has = [&](const char* s) {
    if (text.find(s) == std::string::npos) std::printf("  missing from the report: %s\n", s);
    return text.find(s) != std::string::npos;
  };
  EXPECT(has("campaign good.txt \"Good\": 3 missions\n"));
  EXPECT(has("warning: campaign good.txt: line 4: no world other in the mod or the game: it plays only with a mod that adds it\n"));
  EXPECT(text.find("line 2: no world") == std::string::npos && text.find("line 3: no world") == std::string::npos);
  EXPECT(has("error: campaign bad.txt: line 2: mission=c1: mission=<world> <number>"));
  EXPECT(has("error: campaign more/GOOD.txt: the mod has a campaign GOOD already"));
  EXPECT(errors == 3);  // and the stub zbd/mw/gamez.zbd
  if (errors != 3) std::printf("%s", text.c_str());
}

// Ours (docs/modding.md, The MODS screen): the screen's two tables from the
// write dir's mods, each campaign= a CUSTOM CAMPAIGNS row (a mod of campaigns
// only not in MODS), MODS in mw3.cfg's order with the conflicts' marks and
// notes, ON/OFF and UP/DOWN on the order, and the conflict following it.
TEST(mods_screen_tables_order_and_conflicts) {
  const std::string wd = tmp("mods_screen");
  put(wd + "/mods/camp/mod.txt", "name=Camp\nauthor=Someone\nversion=1.2\ncampaign=lost.txt\ncampaign=bad.txt\n");
  put(wd + "/mods/camp/lost.txt", "name=Lost Frontier\nmission=c1 1\n");
  put(wd + "/mods/camp/bad.txt", "mission=c1\n");
  put(wd + "/mods/campmech/mod.txt", "name=Camp Mech\ncampaign=drills.txt\nmech=Scout tech=clan mass=25 class=light\n");
  put(wd + "/mods/campmech/drills.txt", "name=Drills\nmission=t1 1\n");
  put(wd + "/mods/a/mod.txt", "name=A\n");
  put(wd + "/mods/a/zbd/x.zbd", "a's");
  put(wd + "/mods/b/mod.txt", "name=B\nauthor=Another\n");
  put(wd + "/mods/b/zbd/x.zbd", "b's");
  put(wd + "/mods/off/mod.txt", "name=Off\n");
  std::vector<std::string> on = {"a", "b", "camp"};
  mech3::ModsTables t = mech3::mods_tables(wd, on, {"camp"});
  std::string rows;
  for (const mech3::ModsCampaignRow& c : t.campaigns)
    rows += c.mod + ":" + c.name + (c.mounted ? "+" : "") + (c.error.empty() ? "" : "!") + " ";
  std::printf("  campaigns: %s\n", rows.c_str());
  EXPECT(rows == "camp:Lost Frontier+ camp:bad.txt+! campmech:Drills ");
  if (t.campaigns.size() == 3) {
    EXPECT(t.campaigns[1].error.find("line 1") != std::string::npos);
    EXPECT(mech3::mods_campaign_details(t.campaigns[0]) == "Someone, version 1.2");
    // The row: its name, " !" when it cannot be played or its mod is off here.
    EXPECT(mech3::mods_campaign_row_text(t.campaigns[0]) == "Lost Frontier" &&
           mech3::mods_campaign_row_text(t.campaigns[1]) == "bad.txt !" &&
           mech3::mods_campaign_row_text(t.campaigns[2]) == "Drills !");
  }
  rows.clear();
  for (const mech3::ModsListEntry& m : t.mods) rows += mech3::mods_row_text(m) + "|";
  std::printf("  mods: %s\n", rows.c_str());
  EXPECT(rows == "[x] A|[x] B !|[ ] Camp Mech|[ ] Off|");
  if (t.mods.size() == 4) {
    EXPECT(mech3::mods_row_note(t.mods[0], t.mods).empty());
    EXPECT(mech3::mods_row_note(t.mods[1], t.mods) == "Shares 1 file with A, which wins");
  }
  // UP on B: B first, so A is the one shadowed.
  EXPECT(mech3::mods_move(on, "b", -1) && (on == std::vector<std::string>{"b", "a", "camp"}));
  EXPECT(!mech3::mods_move(on, "b", -1) && !mech3::mods_move(on, "camp", 1) && !mech3::mods_move(on, "off", 1));
  t = mech3::mods_tables(wd, on, {"camp"});
  EXPECT(t.mods.size() == 4 && t.mods[0].info.folder == "b" && t.mods[1].info.folder == "a" &&
         mech3::mods_row_note(t.mods[1], t.mods) == "Shares 1 file with B, which wins" &&
         mech3::mods_row_text(t.mods[0]) == "[x] B");
  // OFF takes it out of the order, ON puts it at the end; saved as mw3.cfg's mod= lines.
  mech3::mods_toggle(on, "b");
  mech3::mods_toggle(on, "off");
  EXPECT((on == std::vector<std::string>{"a", "camp", "off"}));
  EXPECT(mech3::mods_save_enabled(wd, on) && mech3::mods_enabled(wd) == on);
  t = mech3::mods_tables(wd, mech3::mods_enabled(wd), {"camp"});
  rows.clear();
  for (const mech3::ModsListEntry& m : t.mods) rows += mech3::mods_row_text(m) + "|";
  EXPECT(rows == "[x] A|[x] Off|[ ] B|[ ] Camp Mech|");
}

// The modding reference's JSON forms (docs/modding-reference.md): every member our readers look up in
// mech3ax's JSON (zutl_json's get and member report each), by form, while they read unzbd's export of
// the shipped files: the motions, mechlib, every world's anim.zbd and gamez.zbd. Equal to each registry.
namespace {
std::map<std::string, std::set<std::string>>& json_keys() {
  static std::map<std::string, std::set<std::string>> m;
  return m;
}
}  // namespace

TEST(reference_json_forms_keys_are_the_readers) {
  namespace ref = upgrades::reference;
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/c1/gamez.zbd", ec)) return;
  const fs::path unzbd = find_unzbd();
  if (unzbd.empty()) return;
  json_keys().clear();
  ref::observer() = [](const char* context, const char* key, const void*) { json_keys()[context].insert(key); };
  const char* was = ref::context();
  std::map<std::string, std::string> ex;
  std::string why;
  std::vector<uint8_t> out;
  ref::context() = "json:motion";
  if (unzbd_export(unzbd, "motion", install + "/zbd/motion.zbd", &ex))
    for (const auto& [n, t] : ex)
      if (n.size() > 5 && n.compare(n.size() - 5, 5, ".json") == 0 && n != "manifest.json") zsis::motion_from_json(t, &out, &why);
  ex.clear();
  ref::context() = "json:mechlib";
  if (unzbd_export(unzbd, "mechlib", install + "/zbd/mechlib.zbd", &ex))
    for (const auto& [n, t] : ex) {
      if (n == "manifest.json" || n.size() < 5 || n.compare(n.size() - 5, 5, ".json") != 0) continue;
      if (n == "materials.json") zcls::mechlib_materials_from_json(t, &out, &why);
      else zcls::mechlib_model_from_json(t, &out, &why);
    }
  for (const char* w : {"c1", "c2", "c3", "c4", "c4b", "t1"}) {
    for (const char* kind : {"anim", "gamez"}) {
      ex.clear();
      const std::string zbd = install + "/zbd/" + w + "/" + kind + ".zbd";
      if (!fs::exists(zbd, ec) || !unzbd_export(unzbd, kind, zbd, &ex)) continue;
      auto file = [&](const std::string& n) { return ex.count(n) ? ex[n] : std::string(); };
      if (std::string(kind) == "anim") {
        ref::context() = "json:anim";
        zeff::anim_from_json(file, &out, nullptr, &why);
      } else {
        ref::context() = "json:gamez";
        zcls::gamez_from_json(file, &out, &why);
      }
    }
  }
  ref::observer() = nullptr;
  ref::context() = was;
  int forms = 0;
  for (const ref::File& f : ref::files()) {
    if (std::string(f.id).rfind("json:", 0) != 0) continue;
    ++forms;
    std::set<std::string> reg, missing, dead;
    for (const ref::Key& k : f.keys) reg.insert(k.key);
    const std::set<std::string>& read = json_keys()[f.id];
    for (const std::string& k : read) if (!reg.count(k)) missing.insert(k);
    for (const std::string& k : reg) if (!read.count(k)) dead.insert(k);
    std::string m, d;
    for (const std::string& k : missing) m += k + " ";
    for (const std::string& k : dead) d += k + " ";
    std::printf("  %s: %zu keys read; not in the registry: %s; never read: %s\n", f.id, read.size(), m.empty() ? "none" : m.c_str(),
                d.empty() ? "none" : d.c_str());
    EXPECT(missing.empty() && dead.empty() && !read.empty());
  }
  EXPECT(forms == 4);
}
