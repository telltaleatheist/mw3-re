// ADD MOD (docs/modding.md, The MODS screen): mods added from archives this
// test writes with libarchive's own writer and from folders, into a test
// write dir (nothing committed, no player's folder touched).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "mech3/mod_install.h"
#include "mech3/mods.h"
#include "mech3/patch_import.h"
#include "mech3/source_archive.h"
#include "upgrades/mods/mod_export.h"

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

// EXPORT and IMPORT (docs/modding.md, IMPORT and EXPORT): a mod with JSON
// folders exported, nothing but its own files in the zip, and the zip taken
// back as the same mod; one changed, added to or cut short refused by its
// checksums.
namespace {
// Every file under a folder with its bytes, by its path from there.
std::vector<std::pair<std::string, std::string>> tree_of(const fs::path& dir) {
  std::vector<std::pair<std::string, std::string>> v;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec)) v.emplace_back(fs::relative(it->path(), dir, ec).generic_string(), text_of(it->path()));
  std::sort(v.begin(), v.end());
  return v;
}
// A zip's entries with their bytes, as IMPORT's reader reads them.
std::vector<Entry> zip_entries(const std::string& path) {
  std::vector<Entry> v;
  mech3::source::ArchiveReader r;
  if (!r.open_file(path)) return v;
  mech3::source::ArchiveReader::Entry e;
  uint8_t buf[4096];
  while (r.next(&e)) {
    if (e.dir) continue;
    Entry f{e.path, ""};
    for (int64_t n; (n = r.read(buf, sizeof buf)) > 0;) f.data.append(reinterpret_cast<const char*>(buf), static_cast<size_t>(n));
    v.push_back(f);
  }
  return v;
}
}  // namespace

TEST(mod_export_round_trip) {
  const std::string wd = tmp("mod_export_wd"), wd2 = tmp("mod_export_wd2"), out = tmp("mod_export_out");
  const fs::path mod = fs::path(wd) / "mods" / "Hard Rain";
  std::string bin;
  for (int i = 0; i < 70000; ++i) bin += static_cast<char>((i * 131) ^ (i >> 7));  // past a deflate window, every byte
  const std::vector<std::pair<std::string, std::string>> own = {
      {"mod.txt", "name=Hard Rain\nversion=1.2\nauthor=A Modder\nfor=mw3\n"},
      {"zbd/c1/rain.bin", bin},
      {"zbd/reader/fmv.zrd.json", "[\"MODDED\", \"rain\"]"},  // a JSON folder, as unzbd writes it
      {"zbd/reader/other.zrd.json", "[]"},
      {"zbd/Deep/Er/notes.txt", "kept, any case"},
  };
  for (const auto& [rel, data] : own) put(mod / rel, data);
  // Ours and the player's, never in the zip; beside the mod, never looked at.
  for (const char* rel : {"mw3-mod-added.txt", "mw3-checksums.txt", "zbd/c1/gamez.zbd.bak", "zbd/c1/anim.zbd.saving",
                          ".DS_Store", "zbd/Thumbs.db", "pilots/Owen.txt", "mw3.cfg", "game.log", "._mod.txt"})
    put(mod / rel, "not the mod's");
  put(fs::path(wd) / "mods" / "Other" / "mod.txt", "name=Other\n");
  put(fs::path(wd) / "mods" / "Hard Rain.txt", "beside it");
  put(fs::path(wd) / "mw3.cfg", "mod=Hard Rain\n");

  mech3::ModInfo info;
  info.name = "Hard Rain", info.version = "1.2";
  const std::string name = mech3::mod_export_file_name(info);
  info.name = "Rain: A/B", info.version = "";
  EXPECT(name == "Hard Rain 1.2.zip" && mech3::mod_export_file_name(info) == "Rain_ A_B.zip");
  info = {}, info.folder = "plain";
  EXPECT(mech3::mod_export_file_name(info) == "plain.zip");

  const mech3::ModExportList list = mech3::mod_export_list(mod.string());
  std::printf("  list: %zu files, %zu left out%s\n", list.files.size(), list.left_out.size(), list.error.c_str());
  EXPECT(list.error.empty() && list.files.size() == own.size() && list.left_out.size() == 10);
  const std::string zip = out + "/" + name;
  const mech3::ModExportResult r = mech3::mod_export(mod.string(), zip);
  std::printf("  export: %s\n", r.message.c_str());
  EXPECT(r.ok && r.files == 5 && r.left_out == 10 && !fs::exists(zip + ".saving"));

  // The zip: the mod's own files under its name, and the sums of each, in
  // sha256sum's form (a player can check it with sha256sum -c).
  const std::vector<Entry> in = zip_entries(zip);
  std::string sums;
  for (const Entry& e : in)
    if (e.path == "Hard Rain/mw3-checksums.txt") sums = e.data;
  EXPECT(in.size() == own.size() + 1);
  for (const auto& [rel, data] : own) {
    bool found = false;
    for (const Entry& e : in) found |= e.path == "Hard Rain/" + rel && e.data == data;
    EXPECT(found);
    const std::string line = mech3::sha256_hex(reinterpret_cast<const uint8_t*>(data.data()), data.size()) + "  " + rel + "\n";
    EXPECT(sums.find(line) != std::string::npos);
  }
  for (const Entry& e : in) EXPECT(e.path.rfind("Hard Rain/", 0) == 0 && e.path.find("not the mod") == std::string::npos);

  // Taken back by IMPORT as the same mod: every file the same, the sums
  // checked and gone, mod.txt as written.
  mech3::ModAdd a;
  const mech3::ModAddResult back = add(wd2, zip, "", &a);
  std::printf("  import: %s\n", back.ok ? back.folder.c_str() : back.message.c_str());
  EXPECT(back.ok && back.folder == "Hard Rain" && a.checksums && a.has_mod_txt);
  EXPECT(back.info.name == "Hard Rain" && back.info.version == "1.2" && back.info.author == "A Modder");
  std::vector<std::pair<std::string, std::string>> got = tree_of(fs::path(wd2) / "mods" / "Hard Rain");
  got.erase(std::remove_if(got.begin(), got.end(), [](const auto& f) { return f.first == mech3::kModAddedMarker; }), got.end());
  std::vector<std::pair<std::string, std::string>> want = own;
  std::sort(want.begin(), want.end());
  EXPECT(got == want);
  EXPECT(no_staging(wd2) && mods_in(wd2) == "Hard Rain|");
  // Exported again from there: the same files (our marker left out again).
  const mech3::ModExportResult again = mech3::mod_export((fs::path(wd2) / "mods" / "Hard Rain").string(), out + "/again.zip");
  EXPECT(again.ok && again.files == 5 && again.left_out == 1);
}

// Modder step 8 (modding.md, IMPORT and EXPORT, the pack): EXPORT checks the
// mod first and says what the game would refuse, packing it all the same; a
// folder with no mod.txt gets one in the zip (its folder's name, this game),
// with its sum, never in the folder; IMPORT takes that zip back as a mod.
TEST(mod_export_packs_checked_and_with_its_mod_txt) {
  const std::string wd = tmp("mod_pack_wd"), wd2 = tmp("mod_pack_wd2"), out = tmp("mod_pack_out");
  const fs::path bad = fs::path(wd) / "mods" / "Bad";
  put(bad / "mod.txt", "name=Bad\nfor=mw3\nmech=broken\n");
  put(bad / "zbd/a.txt", "a");
  mech3::ModExportResult r = mech3::mod_export(bad.string(), out + "/bad.zip", "", true);
  std::printf("  checked: %s\n", r.message.c_str());
  EXPECT(r.ok && r.refused == 1 && !r.made_mod_txt && fs::exists(out + "/bad.zip"));
  EXPECT(r.message.find("The game would refuse 1 thing in it; its report says what.") != std::string::npos);
  r = mech3::mod_export(bad.string(), out + "/bad2.zip");  // unchecked: as before
  EXPECT(r.ok && r.refused == -1 && r.message.find("refuse") == std::string::npos);
  // Laid out as an install, no mod.txt.
  const fs::path overlay = fs::path(wd) / "mods" / "Overlay";
  put(overlay / "zbd/b.txt", "b");
  r = mech3::mod_export(overlay.string(), out + "/overlay.zip", "", true);
  std::printf("  overlay: %s\n", r.message.c_str());
  EXPECT(r.ok && r.made_mod_txt && r.refused == 0 && !fs::exists(overlay / "mod.txt"));
  EXPECT(r.message.find("(1 file and a mod.txt)") != std::string::npos);
  std::string txt, sums;
  for (const Entry& e : zip_entries(out + "/overlay.zip")) {
    if (e.path == "Overlay/mod.txt") txt = e.data;
    if (e.path == "Overlay/mw3-checksums.txt") sums = e.data;
  }
  EXPECT(txt == "name=Overlay\nfor=mw3\n");
  EXPECT(sums.find(mech3::sha256_hex(reinterpret_cast<const uint8_t*>(txt.data()), txt.size()) + "  mod.txt\n") != std::string::npos);
  mech3::ModAdd a;
  const mech3::ModAddResult back = add(wd2, out + "/overlay.zip", "", &a);
  std::printf("  import: %s\n", back.ok ? back.folder.c_str() : back.message.c_str());
  EXPECT(back.ok && a.checksums && a.has_mod_txt && back.info.name == "Overlay" && back.info.error.empty());
}

TEST(mod_export_checksums_refuse_a_changed_zip) {
  const std::string wd = tmp("mod_export_sums_wd"), out = tmp("mod_export_sums_out");
  const fs::path mod = fs::path(wd) / "mods" / "Sums";
  put(mod / "mod.txt", "name=Sums\nfor=mw3\n");
  put(mod / "zbd/a.txt", "first");
  put(mod / "zbd/b.txt", "second");
  const std::string zip = out + "/sums.zip";
  EXPECT(mech3::mod_export(mod.string(), zip).ok);
  const std::vector<Entry> in = zip_entries(zip);
  EXPECT(in.size() == 4);
  struct Case {
    const char* what;
    std::vector<Entry> entries;
    const char* error;  // a piece of the refusal; "" taken
  };
  std::vector<Case> cases;
  auto changed = [&](const std::function<void(std::vector<Entry>&)>& f) {
    std::vector<Entry> v = in;
    f(v);
    return v;
  };
  cases.push_back({"as exported", in, ""});
  cases.push_back({"a file changed", changed([](auto& v) { for (Entry& e : v) if (e.path == "Sums/zbd/a.txt") e.data = "First"; }),
                   "zbd/a.txt does not match its checksum"});
  cases.push_back({"a file added", changed([](auto& v) { v.push_back({"Sums/zbd/c.txt", "third"}); }), "do not list (zbd/c.txt)"});
  cases.push_back({"a file gone", changed([](auto& v) { v.erase(std::remove_if(v.begin(), v.end(), [](const Entry& e) { return e.path == "Sums/zbd/b.txt"; }), v.end()); }),
                   "is missing (zbd/b.txt)"});
  cases.push_back({"mod.txt changed", changed([](auto& v) { for (Entry& e : v) if (e.path == "Sums/mod.txt") e.data += "author=someone\n"; }),
                   "mod.txt does not match"});
  cases.push_back({"a damaged list", changed([](auto& v) { for (Entry& e : v) if (e.path == "Sums/mw3-checksums.txt") e.data[3] = 'x'; }),
                   "checksums file is damaged (line 1)"});
  cases.push_back({"a list naming outside", changed([](auto& v) { for (Entry& e : v) if (e.path == "Sums/mw3-checksums.txt") e.data += std::string(64, '0') + "  ../x.txt\n"; }),
                   "names a file outside it"});
  // What a system's own zipping leaves (Finder's): not the mod's, taken.
  cases.push_back({"a Finder zip", changed([](auto& v) { v.push_back({"Sums/.DS_Store", "x"}), v.push_back({"__MACOSX/Sums/._mod.txt", "x"}); }), ""});
  // Its list written by hand in sha256sum's binary form, CRLF: taken.
  cases.push_back({"sha256sum -b, CRLF", changed([](auto& v) {
                     for (Entry& e : v)
                       if (e.path == "Sums/mw3-checksums.txt") {
                         std::string t;
                         for (char c : e.data) t += c == '\n' ? std::string("\r\n") : std::string(1, c);
                         for (size_t at = 0; (at = t.find("  ", at)) != std::string::npos;) t.replace(at, 2, " *");
                         e.data = "# made by hand\r\n" + t;
                       }
                   }), ""});
  int n = 0;
  for (const Case& k : cases) {
    const std::string wd2 = tmp(("mod_export_sums_" + std::to_string(n)).c_str());
    const std::string z = write_zip(out + "/case" + std::to_string(n++) + ".zip", k.entries);
    mech3::ModAdd a;
    const mech3::ModAddResult r = add(wd2, z, "", &a);
    std::printf("  %s: %s\n", k.what, r.ok ? "taken" : r.message.c_str());
    if (*k.error) EXPECT(!r.ok && r.message.find(k.error) != std::string::npos && mods_in(wd2).empty());
    else EXPECT(r.ok && a.checksums && !fs::exists(fs::path(wd2) / "mods/Sums" / mech3::kModChecksums));
    EXPECT(no_staging(wd2));
  }
  // A zip with no list is taken as before (every mod zip made by hand).
  const std::string wd3 = tmp("mod_export_sums_none");
  mech3::ModAdd a;
  EXPECT(add(wd3, write_zip(out + "/none.zip", {{"Sums/mod.txt", "name=Sums\n"}, {"Sums/zbd/a.txt", "x"}}), "", &a).ok && !a.checksums);
}

TEST(mod_export_refusals) {
  const std::string wd = tmp("mod_export_refuse_wd"), out = tmp("mod_export_refuse_out");
  const fs::path mod = fs::path(wd) / "mods" / "R";
  put(mod / "mod.txt", "name=R\n");
  put(mod / "zbd/a.txt", "a");
  // Never into its own folder (the next export would take the zip in).
  mech3::ModExportResult r = mech3::mod_export(mod.string(), (mod / "zbd/R.zip").string());
  EXPECT(!r.ok && r.message.find("inside the mod's own folder") != std::string::npos && !fs::exists(mod / "zbd/R.zip"));
  // Only our files and the player's: nothing to export.
  const fs::path empty = fs::path(wd) / "mods" / "E";
  put(empty / "mw3-mod-added.txt", "x");
  put(empty / "pilots/p.txt", "x");
  r = mech3::mod_export(empty.string(), out + "/e.zip");
  EXPECT(!r.ok && r.message.find("no files of its own") != std::string::npos && !fs::exists(out + "/e.zip"));
  EXPECT(!mech3::mod_export((fs::path(wd) / "mods" / "gone").string(), out + "/g.zip").ok);
  // A name Windows cannot hold: IMPORT would refuse the zip, so EXPORT does.
  const fs::path bad = fs::path(wd) / "mods" / "B";
  put(bad / "mod.txt", "name=B\n");
  put(bad / "zbd/what?.txt", "x");
  r = mech3::mod_export(bad.string(), out + "/b.zip");
  std::printf("  bad name: %s\n", r.message.c_str());
  EXPECT(!r.ok && r.message.find("Windows cannot hold (zbd/what?.txt)") != std::string::npos && !fs::exists(out + "/b.zip"));
#if !defined(_WIN32)
  // A link (it could point anywhere): refused, not followed.
  const fs::path linked = fs::path(wd) / "mods" / "L";
  put(linked / "mod.txt", "name=L\n");
  std::error_code ec;
  fs::create_symlink(fs::path(wd) / "mw3.cfg", linked / "cfg.txt", ec);
  r = mech3::mod_export(linked.string(), out + "/l.zip");
  EXPECT(!r.ok && r.message.find("a link (cfg.txt)") != std::string::npos && !fs::exists(out + "/l.zip"));
  // Two names differing only in case, where the system can hold both.
  const fs::path cased = fs::path(wd) / "mods" / "C";
  put(cased / "mod.txt", "name=C\n");
  put(cased / "a.txt", "1");
  put(cased / "A.txt", "2");
  if (text_of(cased / "a.txt") == "1") {
    r = mech3::mod_export(cased.string(), out + "/c.zip");
    EXPECT(!r.ok && r.message.find("differ only in case") != std::string::npos);
  }
#endif
  // A zip already there is replaced whole (the save box asked).
  put(out + "/old.zip", "an old file");
  r = mech3::mod_export(mod.string(), out + "/old.zip");
  EXPECT(r.ok && zip_entries(out + "/old.zip").size() == 3);
}
#endif

// The sums over data in pieces, the same as over it whole.
TEST(mod_export_sha256_in_pieces) {
  std::string data;
  for (int i = 0; i < 300; ++i) data += static_cast<char>(i * 37 + 11);
  EXPECT(mech3::sha256_hex(nullptr, 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT(mech3::sha256_hex(reinterpret_cast<const uint8_t*>("abc"), 3) ==
         "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  for (size_t n : {0, 1, 55, 56, 63, 64, 65, 119, 128, 300})
    for (size_t piece : {1, 7, 63, 64, 65, 200}) {
      mech3::Sha256 s;
      for (size_t at = 0; at < n; at += piece) s.add(reinterpret_cast<const uint8_t*>(data.data()) + at, std::min(piece, n - at));
      EXPECT(s.hex() == mech3::sha256_hex(reinterpret_cast<const uint8_t*>(data.data()), n));
    }
}
