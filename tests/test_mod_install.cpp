// ADD MOD (docs/modding.md, The MODS screen): mods added from archives this
// test writes with libarchive's own writer and from folders, into a test
// write dir (nothing committed, no player's folder touched).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "mech3/mod_install.h"
#include "mech3/mods.h"

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
std::string text_of(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
std::string tmp(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
// The folders in mods\, by name.
std::string mods_in(const std::string& wd) {
  std::vector<std::string> v;
  std::error_code ec;
  for (fs::directory_iterator it(wd + "/mods", ec), end; !ec && it != end; it.increment(ec)) v.push_back(it->path().filename().string());
  std::sort(v.begin(), v.end());
  std::string s;
  for (const std::string& n : v) s += n + "|";
  return s;
}
// No staging folder left behind (ours removed, the base with it when empty).
bool no_staging(const std::string& wd) {
  std::error_code ec;
  const fs::path base = fs::path(wd) / "mod-staging";
  if (!fs::exists(base, ec)) return true;
  for (fs::directory_iterator it(base, ec), end; !ec && it != end; it.increment(ec))
    if (fs::exists(it->path() / mech3::kModStagingMarker, ec)) return false;
  return true;
}

#if MW3_HAVE_LIBARCHIVE
struct Entry {
  std::string path, data;
  const char* link = nullptr;  // a symbolic link to this
};
std::string write_zip(const std::string& path, const std::vector<Entry>& entries) {
  archive* a = archive_write_new();
  archive_write_set_format_zip(a);
  archive_write_open_filename(a, path.c_str());
  for (const Entry& f : entries) {
    archive_entry* e = archive_entry_new();
    archive_entry_set_pathname(e, f.path.c_str());
    if (f.link) {
      archive_entry_set_filetype(e, AE_IFLNK);
      archive_entry_set_symlink(e, f.link);
      archive_entry_set_size(e, 0);
    } else {
      archive_entry_set_filetype(e, AE_IFREG);
      archive_entry_set_size(e, static_cast<la_int64_t>(f.data.size()));
    }
    archive_entry_set_perm(e, 0644);
    archive_write_header(a, e);
    if (!f.link) archive_write_data(a, f.data.data(), f.data.size());
    archive_entry_free(e);
  }
  archive_write_close(a);
  archive_write_free(a);
  return path;
}
#endif

// prepare + finish as the screen does, the game given when it cannot be told.
mech3::ModAddResult add(const std::string& wd, const std::string& source, const std::string& install, mech3::ModAdd* out = nullptr,
                        const mech3::ModAddLimits& limits = {}) {
  mech3::ModAdd a = mech3::mod_add_prepare(wd, source, install, limits);
  if (out) *out = a;
  mech3::ModAddResult r;
  if (!a.error.empty()) {
    r.message = a.error;
    return r;
  }
  r = mech3::mod_add_finish(wd, a, a.game == mech3::ModGame::kUnknown ? mech3::ModGame::kMw3 : a.game, install);
  return r;
}
}  // namespace

// Every path that would land outside the folder refused; the rest cleaned.
TEST(mod_install_entry_paths) {
  std::string c;
  for (const char* bad : {"../x.zbd", "zbd/../../x.zbd", "/etc/passwd", "\\\\server\\share\\x", "C:/x.zbd", "C:x.zbd",
                          "zbd\\..\\..\\x", "..", "zbd/x.zbd:stream", "a/\x01/b", ""})
    EXPECT(!mech3::mod_entry_path_ok(bad, &c));
  EXPECT(mech3::mod_entry_path_ok("./zbd//x.zbd", &c) && c == "zbd/x.zbd");
  EXPECT(mech3::mod_entry_path_ok("zbd\\c1\\a.zbd", &c) && c == "zbd/c1/a.zbd");
  EXPECT(mech3::mod_entry_path_ok("my mod/zbd/a..b.zbd", &c) && c == "my mod/zbd/a..b.zbd");  // ".." only as a whole part
  EXPECT(mech3::mod_folder_name(" Hard Rain: v2/x ") == "Hard Rain_ v2_x" && mech3::mod_folder_name("...") == "mod");
  // Names Windows cannot hold, refused on every system (a mod added on the
  // Mac goes to the PC): devices with any extension and case, a trailing
  // dot or space, < > " | ? *.
  for (const char* bad : {"zbd/CON", "zbd/con.txt", "Aux.zbd", "zbd/nul.tar.gz", "COM1.txt", "zbd/lpt9", "prn/x.txt",
                          "zbd/x.", "zbd/x ", "zbd /x.txt", "zbd/a<b", "zbd/a>b", "zbd/a\"b", "zbd/a|b", "zbd/a?b", "zbd/a*b"})
    EXPECT(!mech3::mod_entry_path_ok(bad, &c));
  for (const char* good : {"zbd/console.txt", "zbd/com10.txt", "zbd/lpt0", "zbd/com.txt", "zbd/auxiliary.zbd", "zbd/.hidden"})
    EXPECT(mech3::mod_entry_path_ok(good, &c));
  EXPECT(mech3::mod_folder_name("con") == "con_" && mech3::mod_folder_name("LPT1") == "LPT1_" && mech3::mod_folder_name("Console") == "Console");
  // The game by the names only one ships.
  EXPECT(mech3::mod_game_of({"zbd/c4b/gamez.zbd"}) == mech3::ModGame::kMw3);
  EXPECT(mech3::mod_game_of({"ZBD/C2/READERSA1.ZBD"}) == mech3::ModGame::kPm);
  EXPECT(mech3::mod_game_of({"zbd/c4/readerm9.zbd", "zbd/mechlib.zbd"}) == mech3::ModGame::kPm);
  EXPECT(mech3::mod_game_of({"zbd/mechlib.zbd"}) == mech3::ModGame::kUnknown);
  EXPECT(mech3::mod_game_of({"zbd/t1/reader.zbd", "zbd/c1/readersa1.zbd"}) == mech3::ModGame::kUnknown);
}

#if MW3_HAVE_LIBARCHIVE
// Zip slip, links and the caps: refused whole, nothing written outside,
// nothing left in mods\ or the staging folder.
TEST(mod_install_refuses_escapes_and_limits) {
  const std::string wd = tmp("mod_install_refuse"), src = tmp("mod_install_refuse_src");
  struct Case {
    const char* name;
    std::vector<Entry> entries;
    mech3::ModAddLimits limits;
    const char* says;
  };
  const std::vector<Case> cases = {
      {"slip.zip", {{"zbd/a.txt", "a"}, {"../../escaped.txt", "x"}}, {}, "outside its folder"},
      {"abs.zip", {{"/tmp/escaped_abs.txt", "x"}}, {}, "outside its folder"},
      {"drive.zip", {{"C:/escaped.txt", "x"}}, {}, "outside its folder"},
      {"link.zip", {{"zbd/a.txt", "a"}, {"zbd/link", "", "/etc/passwd"}}, {}, "link"},
      {"count.zip", {{"zbd/a.txt", "a"}, {"zbd/b.txt", "b"}, {"zbd/c.txt", "c"}}, {1 << 20, 2}, "more than 2 files"},
      {"size.zip", {{"zbd/a.txt", std::string(3000, 'a')}}, {1000, 100}, "more than 1000 bytes"},
      {"empty.zip", {}, {}, "empty"},
      {"device.zip", {{"zbd/a.txt", "a"}, {"zbd/CON.txt", "x"}}, {}, "Windows cannot hold"},
      {"wild.zip", {{"zbd/what?.txt", "x"}}, {}, "Windows cannot hold"},
      {"dot.zip", {{"zbd/notes.txt.", "x"}}, {}, "Windows cannot hold"},
      {"case.zip", {{"zbd/Mech.txt", "a"}, {"zbd/mech.txt", "b"}}, {}, "differ only in case"},
      {"setup.zip", {{"ModSetup.exe", "MZ"}}, {}, "only a program"},
  };
  for (const Case& k : cases) {
    const std::string zip = write_zip(src + "/" + k.name, k.entries);
    mech3::ModAdd a;
    const mech3::ModAddResult r = add(wd, zip, "", &a, k.limits);
    std::printf("  %s: %s\n", k.name, r.message.c_str());
    EXPECT(!r.ok && r.message.find(k.says) != std::string::npos);
    EXPECT(mods_in(wd).empty() && no_staging(wd));
  }
  std::error_code ec;
  EXPECT(!fs::exists(fs::path(wd).parent_path() / "escaped.txt", ec) && !fs::exists(fs::path(src).parent_path() / "escaped.txt", ec));
  EXPECT(!fs::exists("/tmp/escaped_abs.txt", ec));
  // A folder with a link in it: refused too.
  put(src + "/linked/zbd/a.txt", "a");
  fs::create_symlink("/etc", src + "/linked/zbd/etc", ec);
  if (!ec) {
    const mech3::ModAddResult r = add(wd, src + "/linked", "");
    EXPECT(!r.ok && r.message.find("link") != std::string::npos && mods_in(wd).empty() && no_staging(wd));
  }
  // A folder with a device name in it: refused too.
  put(src + "/devices/zbd/aux.txt", "a");
  {
    const mech3::ModAddResult r = add(wd, src + "/devices", "");
    EXPECT(!r.ok && r.message.find("Windows cannot hold") != std::string::npos && mods_in(wd).empty() && no_staging(wd));
  }
    // The game's own folders are not mods.
  put(wd + "/mods/x/mod.txt", "name=X\n");
  EXPECT(!add(wd, wd + "/mods/x", "").ok && !add(wd, wd + "/mods", "").ok && !add(wd, wd, "").ok);
  EXPECT(mods_in(wd) == "x|");
}

// Each layout: the root found, mod.txt kept or written, our marker in it,
// listed in mods_list at once, the staging folder gone.
TEST(mod_install_finds_each_layout) {
  const std::string wd = tmp("mod_install_layouts"), src = tmp("mod_install_layouts_src");
  struct Case {
    const char* source;  // a .zip written, or a folder made
    std::vector<Entry> entries;
    const char* folder;  // its folder in mods\, and a file of it
    const char* file;
    const char* for_game;
  };
  const std::vector<Case> cases = {
      {"flat.zip", {{"mod.txt", "name=Flat Mod\n"}, {"zbd/flat.txt", "f"}}, "Flat Mod", "zbd/flat.txt", "mw3"},
      {"nested.zip", {{"Pack/readme.txt", "r"}, {"Pack/Inner/MOD.TXT", "name=Nested\nfor=mw3\n"}, {"Pack/Inner/zbd/nested.txt", "n"}},
       "Nested", "zbd/nested.txt", "mw3"},
      {"Cool Mod v2.zip", {{"Cool Mod v2/zbd/t1/extra.txt", "c"}}, "Cool Mod v2", "zbd/t1/extra.txt", "mw3"},
      {"lone.zip", {{"Thing/Deeper/stuff.txt", "s"}}, "lone", "stuff.txt", "mw3"},
      {"folder", {{"Folder Mod/zbd/folder.txt", "d"}}, "folder", "zbd/folder.txt", "mw3"},
  };
  for (const Case& k : cases) {
    std::string source;
    if (std::strcmp(k.source, "folder") == 0) {
      for (const Entry& e : k.entries) put(src + "/folder/" + e.path, e.data);
      source = src + "/folder";
    } else {
      source = write_zip(src + "/" + k.source, k.entries);
    }
    mech3::ModAdd a;
    const mech3::ModAddResult r = add(wd, source, "", &a);
    const fs::path at = fs::path(wd) / "mods" / k.folder;
    const std::string mt = text_of(at / "mod.txt");
    std::printf("  %s: %s -> %s (mod.txt %d)\n", k.source, r.ok ? "ok" : r.message.c_str(), r.folder.c_str(), a.has_mod_txt);
    EXPECT(r.ok && r.folder == k.folder);
    EXPECT(fs::is_regular_file(at / k.file) && fs::is_regular_file(at / mech3::kModAddedMarker));
    EXPECT(mt.find(std::string("for=") + k.for_game) != std::string::npos || a.has_mod_txt);
    EXPECT(no_staging(wd));
  }
  // The written mod.txt: the archive's name, and listed with no conflict
  // from our marker (every added mod has one).
  EXPECT(text_of(fs::path(wd) / "mods/Cool Mod v2/mod.txt").find("name=Cool Mod v2\n") != std::string::npos);
  std::vector<std::string> all;
  for (const mech3::ModInfo& i : mech3::mods_found(wd)) all.push_back(i.folder);
  const std::vector<mech3::ModsListEntry> l = mech3::mods_list(wd, all);
  for (const mech3::ModsListEntry& e : l) EXPECT(e.shadowed.empty() && e.info.error.empty());
  EXPECT(l.size() == cases.size());
  // The nested one: only its root, not the pack's readme.
  EXPECT(!fs::exists(fs::path(wd) / "mods/Nested/readme.txt") && fs::exists(fs::path(wd) / "mods/Nested/mod.txt"));
}

// A whole install: only the files that differ, no programs, nothing of the
// player's own kind; all the same, refused.
TEST(mod_install_full_install_reduced) {
  const std::string wd = tmp("mod_install_full"), src = tmp("mod_install_full_src"), inst = tmp("mod_install_full_inst");
  put(inst + "/Mech3.exe", "original exe");
  put(inst + "/zbd/same.txt", "same");
  put(inst + "/zbd/changed.txt", "old");
  put(inst + "/zbd/c1/size.txt", "abc");
  const std::string zip = write_zip(src + "/Whole Game.zip", {{"MechWarrior 3/MECH3.EXE", "patched exe"},
                                                             {"MechWarrior 3/Ir50_32.dll", "dll"},
                                                             {"MechWarrior 3/zbd/SAME.TXT", "same"},
                                                             {"MechWarrior 3/zbd/changed.txt", "new"},
                                                             {"MechWarrior 3/zbd/c1/size.txt", "abd"},
                                                             {"MechWarrior 3/zbd/added.txt", "added"},
                                                             {"MechWarrior 3/pilots/Someone.txt", "a pilot"}});
  mech3::ModAdd a;
  const mech3::ModAddResult r = add(wd, zip, inst, &a);
  std::printf("  whole install: %s, %d the same, %d others left out\n", r.ok ? r.folder.c_str() : r.message.c_str(), a.same_dropped,
              a.other_dropped);
  EXPECT(r.ok && a.full_install && a.same_dropped == 1 && a.other_dropped == 3);
  std::vector<std::string> left;
  std::error_code ec;
  const fs::path at = fs::path(wd) / "mods" / r.folder;
  for (fs::recursive_directory_iterator it(at, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec)) left.push_back(fs::relative(it->path(), at, ec).generic_string());
  std::sort(left.begin(), left.end());
  std::string s;
  for (const std::string& f : left) s += f + "|";
  std::printf("  left: %s\n", s.c_str());
  EXPECT(s == std::string("mod.txt|") + mech3::kModAddedMarker + "|zbd/added.txt|zbd/c1/size.txt|zbd/changed.txt|");
  // The same files only: nothing to add.
  const std::string same = write_zip(src + "/Same.zip", {{"Mech3.exe", "original exe"}, {"zbd/same.txt", "same"}});
  const mech3::ModAddResult r2 = add(wd, same, inst);
  EXPECT(!r2.ok && r2.message.find("nothing to add") != std::string::npos && no_staging(wd));
}

// A name clash gets the next free name; a folder the player made is never
// touched, in mods\ or in the staging folder.
TEST(mod_install_name_clash_and_foreign_folders) {
  const std::string wd = tmp("mod_install_clash"), src = tmp("mod_install_clash_src");
  put(wd + "/mods/My Mod/mod.txt", "name=Theirs\n");
  put(wd + "/mods/My Mod/zbd/theirs.txt", "keep me");
  put(wd + "/mod-staging/notours/keep.txt", "keep me too");  // no marker: not ours
  const std::string zip = write_zip(src + "/My Mod.zip", {{"mod.txt", "name=My Mod\n"}, {"zbd/mine.txt", "mine"}});
  const mech3::ModAddResult r1 = add(wd, zip, ""), r2 = add(wd, zip, "");
  std::printf("  clash: %s, %s\n", r1.folder.c_str(), r2.folder.c_str());
  EXPECT(r1.ok && r1.folder == "My Mod (2)" && r2.ok && r2.folder == "My Mod (3)");
  EXPECT(text_of(fs::path(wd) / "mods/My Mod/zbd/theirs.txt") == "keep me" && text_of(fs::path(wd) / "mods/My Mod/mod.txt") == "name=Theirs\n");
  EXPECT(!fs::exists(fs::path(wd) / "mods/My Mod" / mech3::kModAddedMarker));
  EXPECT(text_of(fs::path(wd) / "mod-staging/notours/keep.txt") == "keep me too");
  // Case differs only: still a clash (a case-blind disk would merge them).
  const std::string zip2 = write_zip(src + "/my mod.zip", {{"mod.txt", "name=MY MOD\n"}, {"zbd/x.txt", "x"}});
  const mech3::ModAddResult r3 = add(wd, zip2, "");
  EXPECT(r3.ok && r3.folder == "MY MOD (4)");
  // A mod the game would refuse: not added, nothing left.
  const std::string bad = write_zip(src + "/bad.zip", {{"mod.txt", "name=Bad\n"}, {"zbd/broken.zbd", "not an archive"}});
  const mech3::ModAddResult r4 = add(wd, bad, "");
  std::printf("  refused: %s\n", r4.message.c_str());
  EXPECT(!r4.ok && r4.message.find("would refuse") != std::string::npos && !fs::exists(fs::path(wd) / "mods/Bad") && no_staging(wd));
}

// MW3_PM13=<folder or archive> (a real mod on this machine, never in the
// repo): added and checked, reported only.
TEST(mod_install_real_mod_env) {
  const char* src = std::getenv("MW3_PM13");
  if (!src || !*src) return;
  const std::string wd = tmp("mod_install_real");
  mech3::ModAdd a = mech3::mod_add_prepare(wd, src, std::string(MW3_GAME_DIR));
  std::printf("  prepare: %s; mod.txt %d, whole install %d, game %d, %d the same, %d others\n",
              a.error.empty() ? "ok" : a.error.c_str(), a.has_mod_txt, a.full_install, static_cast<int>(a.game), a.same_dropped,
              a.other_dropped);
  if (!a.error.empty()) return;
  int n = 0;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(a.root, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec) && ++n <= 5) std::printf("    %s\n", fs::relative(it->path(), a.root, ec).generic_string().c_str());
  std::printf("  root: %s (%d files)\n", fs::relative(a.root, a.staging, ec).generic_string().c_str(), n);
  const mech3::ModAddResult r = mech3::mod_add_finish(wd, a, a.game == mech3::ModGame::kUnknown ? mech3::ModGame::kPm : a.game,
                                                      std::string(MW3_GAME_DIR));
  std::printf("  finish: %s (%d errors)\n", r.ok ? r.folder.c_str() : r.message.c_str(), r.check_errors);
  EXPECT(no_staging(wd));
}
#endif
