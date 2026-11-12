// Archives by their changed entries (upgrades/mods/entry_archives.h; docs/modding.md): IMPORT's cut and the
// mount's merge, on archives made here in the test's folders.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "pm_side.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/data_version.h"
#include "mech3/mod_install.h"
#include "mech3/mods.h"
#include "mech3/mods_menu.h"
#include "upgrades/mods/entry_archives.h"
#include "upgrades/mods/mod_export.h"
#include "upgrades/mods/mod_report.h"
#include "upgrades/mods/mp_mods.h"

namespace {
namespace fs = std::filesystem;
using Entries = std::vector<std::pair<std::string, std::string>>;

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
// An entry the reader takes: a list of one text, in zrd's binary form (a type, then the value; a list's count
// with itself; a text's length and letters).
std::string zrd(const std::string& text) {
  auto u32 = [](uint32_t v) { return std::string(reinterpret_cast<const char*>(&v), 4); };
  return u32(4) + u32(2) + u32(3) + u32(static_cast<uint32_t>(text.size())) + text;
}
void archive(const fs::path& p, const Entries& made) {
  Entries entries;
  for (const auto& [name, text] : made) entries.emplace_back(name, zrd(text));
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  const bool confined = zutl::mounts().confined;  // the test's own file, by its host path
  zutl::mounts().confined = false;
  zutl_zar::Archive a;
  zutl_zar::init(a);
  zutl_zar::create(a, p.string().c_str());
  for (const auto& [name, data] : entries) zutl_zar::add(a, name.c_str(), data.data(), static_cast<uint32_t>(data.size()));
  zutl_zar::close(a);
  zutl_zar::destroy(a);
  zutl::mounts().confined = confined;
}
// "name=data|" for each entry in the table's order; "(unreadable)".
std::string read(const std::string& host) {
  std::vector<mech3::ArchiveEntry> all;
  if (!mech3::archive_read_entries(host, &all)) return "(unreadable)";
  std::string s;
  for (const mech3::ArchiveEntry& e : all) s += e.name + "=" + (e.data.size() >= 16 ? std::string(e.data.begin() + 16, e.data.end()) : "?") + "|";
  return s;
}
std::map<std::string, std::string> snapshot(const std::string& dir) {
  std::map<std::string, std::string> m;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec)) {
      std::ifstream f(it->path(), std::ios::binary);
      std::stringstream ss;
      ss << f.rdbuf();
      m[fs::relative(it->path(), dir, ec).generic_string()] = ss.str();
    }
  return m;
}
std::string names(const std::string& dir) {
  std::string s;
  for (const auto& [rel, data] : snapshot(dir)) s += rel + "|";
  return s;
}
struct KeepMounts {
  zutl::Mounts saved = zutl::mounts();
  ~KeepMounts() { zutl::mounts() = saved; }
};
const Entries kReader = {{"a.zrd", "A"}, {"b.zrd", "B"}, {"c.zrd", "C"}};
void build_install(const std::string& inst) {
  archive(inst + "/zbd/reader.zbd", kReader);
  archive(inst + "/zbd/c1/readerm1.zbd", {{"x.zrd", "X"}, {"y.zrd", "Y"}});
  put(inst + "/zbd/c1/gamez.zbd", "a world");
}
// Mounted as the program does: the mods, then the install; what the game then reads as zbd/reader.zbd.
std::string mounted_reader(const std::string& wd, const std::vector<std::string>& mods, const std::string& inst, std::string* log = nullptr,
                           const char* rel = "zbd/reader.zbd") {
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  zutl::mounts().confined = true;
  std::string l;
  for (const std::string& line : mech3::mods_mount(wd, mods, {inst})) l += line + "\n";
  zutl::mount(inst);
  if (log) *log = l;
  return read(zutl::find(rel));
}
}  // namespace

TEST(entry_archives_names_and_the_cut_of_one_archive) {
  TestGame game;  // Pirate\'s Moon\'s table in mw3pmtests (pm_side.h)
  EXPECT(mech3::record_archive("zbd/reader.zbd") && mech3::record_archive("ZBD\\C1\\ReaderM2.zbd") && mech3::record_archive("zbd/c4b/readermp3.zbd"));
  EXPECT(!mech3::record_archive("zbd/c1/gamez.zbd") && !mech3::record_archive("zbd/mechlib.zbd") && !mech3::record_archive("reader.zbd") &&
         !mech3::record_archive("zbd/a/b/reader.zbd") && !mech3::record_archive("zbd/reader.zbd.entries") && !mech3::record_archive("zbd/reader.zip"));
  EXPECT(mech3::entries_archive_of("zbd/c1/ReaderM1.ZBD.Entries") == "zbd/c1/ReaderM1.ZBD" && mech3::entries_archive_of("zbd/reader.zbd").empty() &&
         mech3::entries_archive_of("zbd/notes.entries").empty());
  const std::string dir = tmp("entry_archives_cut");
  archive(dir + "/own.zbd", kReader);
  // Changed, the same, new, and one of the game's not there.
  archive(dir + "/m.zbd", {{"d.zrd", "D"}, {"B.ZRD", "B1"}, {"a.zrd", "A"}});
  mech3::EntriesCut c = mech3::entries_cut(dir + "/m.zbd", dir + "/own.zbd");
  EXPECT(c.what == mech3::EntriesCut::kCut && c.changed == 1 && c.added == 1 && c.same == 1 && c.missing == 1);
  EXPECT(!fs::exists(dir + "/m.zbd") && read(dir + "/m.zbd.entries") == "d.zrd=D|B.ZRD=B1|");  // its own order and spelling
  // Every entry the game's: dropped.
  archive(dir + "/same.zbd", {{"c.zrd", "C"}, {"a.zrd", "A"}});
  c = mech3::entries_cut(dir + "/same.zbd", dir + "/own.zbd");
  EXPECT(c.what == mech3::EntriesCut::kSame && c.same == 2 && !fs::exists(dir + "/same.zbd") && !fs::exists(dir + "/same.zbd.entries"));
  // Left whole, as it was: a table that does not read (either side), two entries of one name, an .entries file there already.
  const auto left = [&](const char* file, const char* own) {
    const std::string before = snapshot(dir).at(file);
    const mech3::EntriesCut r = mech3::entries_cut(dir + "/" + file, dir + "/" + own);
    std::printf("  %s: %s\n", file, r.why.c_str());
    return r.what == mech3::EntriesCut::kLeft && !r.why.empty() && snapshot(dir).at(file) == before && !fs::exists(dir + "/" + file + ".entries");
  };
  put(dir + "/bad.zbd", "not an archive");
  archive(dir + "/ok.zbd", {{"a.zrd", "A2"}});
  archive(dir + "/twice.zbd", {{"a.zrd", "A2"}, {"A.zrd", "A3"}});
  EXPECT(left("bad.zbd", "own.zbd") && left("ok.zbd", "bad.zbd") && left("ok.zbd", "none.zbd") && left("twice.zbd", "own.zbd"));
  put(dir + "/ok.zbd.entries", "the player's");
  const mech3::EntriesCut there = mech3::entries_cut(dir + "/ok.zbd", dir + "/own.zbd");
  EXPECT(there.what == mech3::EntriesCut::kLeft && fs::exists(dir + "/ok.zbd") && snapshot(dir).at("ok.zbd.entries") == "the player's");
  // An entry whose length runs past the file: the table does not read.
  std::string cut = snapshot(dir).at("own.zbd");
  cut[cut.size() - (TEST_IS_PM ? 12 : 8) - 0x94 + 7] = 0x7f;  // the last entry's length, far past the file (PM's table ends in a CRC too)
  put(dir + "/long.zbd", cut);
  EXPECT(read(dir + "/long.zbd") == "(unreadable)" && left("long.zbd", "own.zbd"));
}

// IMPORT: two mods made for the original, each a whole reader.zbd with other entries changed. Each is added as
// the entries it changes; mounted together both apply, the first winning an entry both change; the install and
// the picked folders are never changed. With the program's confinement on.
TEST(entry_archives_two_imported_mods_both_apply) {
  TestGame game;  // Pirate\'s Moon\'s table in mw3pmtests (pm_side.h)
  const std::string wd = tmp("entry_archives_wd"), inst = tmp("entry_archives_inst"), src = tmp("entry_archives_src");
  build_install(inst);
  const std::string one = src + "/One", two = src + "/Two";
  archive(one + "/zbd/reader.zbd", {{"a.zrd", "A"}, {"b.zrd", "B1"}, {"c.zrd", "C"}, {"d.zrd", "D1"}});
  archive(one + "/zbd/c1/readerm1.zbd", {{"x.zrd", "X"}, {"y.zrd", "Y"}});            // every entry the game's
  archive(one + "/zbd/new/reader.zbd", {{"n.zrd", "N"}});                              // an archive the game lacks: whole
  put(one + "/video/intro.avi", "a movie");                                            // not a reader archive: as it is
  archive(two + "/zbd/reader.zbd", {{"a.zrd", "A"}, {"b.zrd", "B2"}, {"c.zrd", "C2"}});
  put(two + "/zbd/c1/readerm1.zbd", "not an archive");                                 // its table does not read: whole
  const auto before = std::make_pair(snapshot(inst), snapshot(src));
  KeepMounts keep;
  zutl::mounts().confined = true;
  const mech3::ModAddDone a = mech3::mod_add_run(wd, one, inst, [](const std::string&) { return TEST_OUR_ANSWER; }, nullptr), b = mech3::mod_add_run(wd, two, inst, [](const std::string&) { return TEST_OUR_ANSWER; }, nullptr);
  std::printf("  %s\n  %s\n", a.message.c_str(), b.message.c_str());
  EXPECT(a.ok && a.cut.archives == 1 && a.cut.entries == 2 && a.cut.same == 1 && a.cut.whole == 1);
  EXPECT(b.ok && b.cut.archives == 1 && b.cut.entries == 2 && b.cut.same == 0 && b.cut.whole == 1);
  const std::string m1 = wd + "/mods/" + a.folder, m2 = wd + "/mods/" + b.folder;
  std::printf("  %s\n  %s\n", names(m1).c_str(), names(m2).c_str());
  EXPECT(names(m1) == std::string("mod.txt|") + mech3::kModAddedMarker + "|video/intro.avi|zbd/new/reader.zbd|zbd/reader.zbd.entries|");
  EXPECT(read(m1 + "/zbd/reader.zbd.entries") == "b.zrd=B1|d.zrd=D1|" && read(m2 + "/zbd/reader.zbd.entries") == "b.zrd=B2|c.zrd=C2|");
  EXPECT(fs::exists(m2 + "/zbd/c1/readerm1.zbd") == false);  // the unreadable one: left out by the check, as before this
  EXPECT(snapshot(inst) == before.first && snapshot(src) == before.second);
  // Mounted: the game's archive with both mods' entries, the first's winning.
  std::string log;
  EXPECT(mounted_reader(wd, {a.folder, b.folder}, inst, &log) == "a.zrd=A|b.zrd=B1|c.zrd=C2|d.zrd=D1|");
  std::printf("%s", log.c_str());
  EXPECT(log.find("mods: zbd/reader.zbd: 4 changed entries laid over the game's") != std::string::npos);
  EXPECT(zutl::find("zbd/reader.zbd").find(std::string("/") + mech3::kModMergedFolder + "/") != std::string::npos);
  EXPECT(read(zutl::find("zbd/c1/readerm1.zbd")) == "x.zrd=X|y.zrd=Y|");  // no mod holds it: the game's own
  EXPECT(mounted_reader(wd, {b.folder, a.folder}, inst) == "a.zrd=A|b.zrd=B2|c.zrd=C2|d.zrd=D1|");
  EXPECT(mounted_reader(wd, {b.folder}, inst) == "a.zrd=A|b.zrd=B2|c.zrd=C2|");
  // Built once: the same mods again find it there.
  EXPECT(mounted_reader(wd, {a.folder, b.folder}, inst, &log) == "a.zrd=A|b.zrd=B1|c.zrd=C2|d.zrd=D1|" &&
         log.find("as built before") != std::string::npos);
  EXPECT(snapshot(inst) == before.first);
  // A mod's entries changed (a newer copy of it): built again.
  archive(m2 + "/zbd/reader.zbd.entries", {{"c.zrd", "C3"}});
  EXPECT(mounted_reader(wd, {a.folder, b.folder}, inst, &log) == "a.zrd=A|b.zrd=B1|c.zrd=C3|d.zrd=D1|" && log.find("as built before") == std::string::npos);
  // The list says what two such mods share by entry: only b.zrd.
  const std::vector<mech3::ModsListEntry> list = mech3::mods_list(wd, {a.folder, b.folder});
  archive(m2 + "/zbd/reader.zbd.entries", {{"b.zrd", "B2"}, {"c.zrd", "C2"}});
  for (const mech3::ModsListEntry& e : mech3::mods_list(wd, {a.folder, b.folder}))
    if (e.info.folder == b.folder) {
      EXPECT(e.shadowed == std::vector<std::string>{"zbd/reader.zbd: b.zrd"});
      const std::string note = mech3::mods_row_note_shared(e, mech3::mods_list(wd, {a.folder, b.folder}));
      std::printf("  %s\n", note.c_str());
      EXPECT(note == "Shares 1 entry in 1 file (b.zrd) with One, which wins");
    }
  // The check and the report on the added mod.
  int errors = 0;
  mech3::ModFindings facts;
  mech3::mod_check(m1, inst, &errors, &facts);
  std::string said;
  for (const std::string& l : mech3::mod_sentences(facts)) said += l + "\n";
  std::printf("%s", said.c_str());
  EXPECT(errors == 0 && facts.cut == std::vector<std::string>{"zbd/reader.zbd: b.zrd, d.zrd"} && facts.cut_entries == 2);
  EXPECT(said.find("Of 1 of those files it holds only the entries it changes (2 entries)") != std::string::npos &&
         said.find("In zbd/reader.zbd: b.zrd, d.zrd.") != std::string::npos && said.find("EXPORT puts the whole files into its zip") != std::string::npos);
}

// A whole archive put in mods\ by hand wins whole from its place: before the cut mods nothing is built for it;
// after one, the cut mod's entries lie over it. Entries with no archive of the game's under them are left out, said.
TEST(entry_archives_whole_archives_and_no_base) {
  TestGame game;  // Pirate\'s Moon\'s table in mw3pmtests (pm_side.h)
  const std::string wd = tmp("entry_archives_whole_wd"), inst = tmp("entry_archives_whole_inst");
  build_install(inst);
  put(wd + "/mods/cut/mod.txt", "name=Cut\n");
  archive(wd + "/mods/cut/zbd/reader.zbd.entries", {{"b.zrd", "B1"}});
  archive(wd + "/mods/cut/zbd/c9/readerm1.zbd.entries", {{"q.zrd", "Q"}});  // a world the game does not have
  put(wd + "/mods/hand/mod.txt", "name=By hand\n");
  archive(wd + "/mods/hand/zbd/reader.zbd", {{"a.zrd", "A9"}, {"b.zrd", "B9"}});
  KeepMounts keep;
  std::string log;
  EXPECT(mounted_reader(wd, {"hand", "cut"}, inst, &log) == "a.zrd=A9|b.zrd=B9|");
  EXPECT(zutl::find("zbd/reader.zbd").find("/mods/hand/") != std::string::npos);
  EXPECT(log.find("mods: zbd/c9/readerm1.zbd: no game's archive to lay its changed entries over") != std::string::npos);
  EXPECT(mounted_reader(wd, {"cut", "hand"}, inst) == "a.zrd=A9|b.zrd=B1|");
  // The list says so: after a mod's whole archive, a cut mod's entries of it are that mod's.
  for (const mech3::ModsListEntry& e : mech3::mods_list(wd, {"hand", "cut"}))
    if (e.info.folder == "cut") EXPECT(mech3::mods_row_note_shared(e, mech3::mods_list(wd, {"hand", "cut"})) == "Shares 1 entry in 1 file (b.zrd) with By hand, which wins");
  EXPECT(mounted_reader(wd, {"cut"}, inst) == "a.zrd=A|b.zrd=B1|c.zrd=C|");
  // Without a write folder, or with no such mod mounted: nothing built, nothing mounted in front.
  const std::string none = mech3::entries_merge(wd, {wd + "/mods/hand"}, {inst}, nullptr);
  EXPECT(none.empty() && mech3::entries_merge("", {wd + "/mods/cut"}, {inst}, nullptr).empty());
  // No mounted mod kept as entries: nothing is written at all, and nothing mounted in front.
  {
    const std::string wd2 = tmp("entry_archives_plain_wd");
    put(wd2 + "/mods/hand/mod.txt", "name=By hand\n");
    archive(wd2 + "/mods/hand/zbd/reader.zbd", {{"a.zrd", "A9"}});
    EXPECT(mounted_reader(wd2, {"hand"}, inst) == "a.zrd=A9|" && !fs::exists(wd2 + "/" + mech3::kModMergedFolder));
    EXPECT(zutl::mounts().read.front().find("/mods/hand") != std::string::npos);
  }
  // A game's archive that cannot take the entries (its table does not read): not built, said, and the game reads
  // its own file as it would with no such mod; never a part of one in front.
  {
    const std::string bad = tmp("entry_archives_bad_inst");
    put(bad + "/zbd/reader.zbd", "not an archive");
    std::string l;
    EXPECT(mounted_reader(wd, {"cut"}, bad, &l) == "(unreadable)");
    std::printf("%s", l.c_str());
    EXPECT(zutl::find("zbd/reader.zbd") == bad + "/zbd/reader.zbd" &&
           l.find("mods: zbd/reader.zbd: the game's archive could not take the mods' entries (its table); the mods' changed entries of it are not applied, "
                  "the game's own is read") != std::string::npos);
    std::error_code ec;
    for (fs::recursive_directory_iterator it(wd + "/" + mech3::kModMergedFolder, ec), end; !ec && it != end; it.increment(ec))
      EXPECT(it->path().extension() != ".part");
  }
  // The folders of earlier builds: only ours (the marker) go, never the one kept, never the player's.
  const std::string base = wd + "/" + mech3::kModMergedFolder;
  put(base + "/old/" + mech3::kModMergedMarker, "ours");
  put(base + "/old/zbd/reader.zbd", "x");
  put(base + "/kept/" + mech3::kModMergedMarker, "ours");
  put(base + "/theirs/file.txt", "the player's");
  EXPECT(mech3::entries_merged_clear(wd, "kept") >= 1);
  EXPECT(!fs::exists(base + "/old") && fs::exists(base + "/kept") && fs::exists(base + "/theirs/file.txt"));
}

#ifdef MW3_HAVE_LIBARCHIVE
// Both ways: EXPORT of a mod kept as changed entries puts the whole archive into the zip (the game's own with
// the entries laid over it), so the zip is ordinary files; the mod's folder and the install are not changed.
// Imported again it is cut to the same entries; on an install without that archive it stays whole.
TEST(entry_archives_export_repacks_the_whole_archive) {
  TestGame game;  // Pirate\'s Moon\'s table in mw3pmtests (pm_side.h)
  const std::string wd = tmp("entry_archives_export_wd"), inst = tmp("entry_archives_export_inst"), src = tmp("entry_archives_export_src");
  const std::string out = tmp("entry_archives_export_out");
  build_install(inst);
  archive(src + "/One/zbd/reader.zbd", {{"a.zrd", "A"}, {"b.zrd", "B1"}, {"c.zrd", "C"}, {"d.zrd", "D1"}});
  KeepMounts keep;
  zutl::mounts().confined = true;
  const mech3::ModAddDone a = mech3::mod_add_run(wd, src + "/One", inst, [](const std::string&) { return TEST_OUR_ANSWER; }, nullptr);
  const std::string mod = wd + "/mods/" + a.folder;
  EXPECT(a.ok && read(mod + "/zbd/reader.zbd.entries") == "b.zrd=B1|d.zrd=D1|");
  const auto before = std::make_pair(snapshot(mod), snapshot(inst));
  const mech3::ModExportResult r = mech3::mod_export(mod, out + "/one.zip", inst, true);
  std::printf("  %s\n", r.message.c_str());
  EXPECT(r.ok && r.repacked == 1 && r.message.find("Its 1 archive kept as changed entries is whole in the zip.") != std::string::npos);
  EXPECT(snapshot(mod) == before.first && snapshot(inst) == before.second);
  // The zip on an install that has no zbd\reader.zbd: the archive stays whole, and is the game's with the two entries.
  const std::string bare = tmp("entry_archives_export_bare"), wd2 = tmp("entry_archives_export_wd2");
  put(bare + "/zbd/c1/gamez.zbd", "a world");
  const mech3::ModAddDone whole = mech3::mod_add_run(wd2, out + "/one.zip", bare, [](const std::string&) { return TEST_OUR_ANSWER; }, nullptr);
  std::printf("  %s\n  %s\n", whole.message.c_str(), names(wd2 + "/mods/" + whole.folder).c_str());
  EXPECT(whole.ok && read(wd2 + "/mods/" + whole.folder + "/zbd/reader.zbd") == "a.zrd=A|b.zrd=B1|c.zrd=C|d.zrd=D1|");
  // On the player's own install: cut to the same two entries again.
  const std::string wd3 = tmp("entry_archives_export_wd3");
  const mech3::ModAddDone again = mech3::mod_add_run(wd3, out + "/one.zip", inst, [](const std::string&) { return TEST_OUR_ANSWER; }, nullptr);
  EXPECT(again.ok && read(wd3 + "/mods/" + again.folder + "/zbd/reader.zbd.entries") == "b.zrd=B1|d.zrd=D1|");
  // Without the game's own archive to lay them over: not exported, said; no zip left.
  const mech3::ModExportResult none = mech3::mod_export(mod, out + "/none.zip", bare, false);
  std::printf("  %s\n", none.message.c_str());
  EXPECT(!none.ok && none.message.find("zbd/reader.zbd cannot be made whole") != std::string::npos && names(out) == "one.zip|");
}
#endif

// Mods in multiplayer (upgrades/mods/mp_mods.h): the lobby's number is over the mounted result, so a player who
// imported a mod (its archives cut to their changed entries) and one who laid its whole archives into a mod
// folder by hand hold the same number and the same per-mod one; a changed entry, or one the whole archive
// lacks, is a different number, and the refusal names the mod.
TEST(mp_mods_hash_importer_and_hand_laid_match) {
  TestGame game;
  const std::string for_us = std::string("for=") + (TEST_IS_PM ? "mw3pm" : "mw3") + "\n";
  const std::string wa = tmp("mp_hash_wa"), wb = tmp("mp_hash_wb"), inst = tmp("mp_hash_inst"), src = tmp("mp_hash_src");
  build_install(inst);
  const mech3::ModsIdentity keep_id = mech3::mods_identity();
  KeepMounts keep;
  zutl::mounts().confined = true;
  const auto lay = [&](const std::string& root, const Entries& reader) {
    put(root + "/mod.txt", "name=Big Guns\n" + for_us);
    archive(root + "/zbd/reader.zbd", reader);
    archive(root + "/zbd/c1/readerm1.zbd", {{"y.zrd", "Y"}, {"x.zrd", "X"}});  // every entry the game's, in another order
    put(root + "/zbd/thing.dat", "new data");                                  // gameplay, not a reader archive
    put(root + "/zbd/skin.png", "a picture");                                  // not gameplay
  };
  const Entries changed = {{"a.zrd", "A"}, {"b.zrd", "B1"}, {"c.zrd", "C"}, {"d.zrd", "D1"}};
  lay(src + "/Big Guns", changed);
  const mech3::ModAddDone a = mech3::mod_add_run(wa, src + "/Big Guns", inst, nullptr, nullptr);
  std::printf("  %s\n  imported: %s\n", a.message.c_str(), names(wa + "/mods/" + a.folder).c_str());
  EXPECT(a.ok && a.cut.archives == 1 && a.cut.same == 1);
  if (!a.ok) return;
  const auto identity = [&](const std::string& wd, const std::string& folder) {
    mech3::mods_identity_set(wd, {folder}, inst);
    return mech3::mods_identity();
  };
  const mech3::ModsIdentity imp = identity(wa, a.folder);
  // By hand: the same files, whole, the table in another order and the folders spelt otherwise.
  lay(wb + "/mods/hand", {{"d.zrd", "D1"}, {"c.zrd", "C"}, {"B.ZRD", "B1"}, {"a.zrd", "A"}});
  const mech3::ModsIdentity hand = identity(wb, "hand");
  std::printf("  imported %016llx, by hand %016llx (the folders' own: %016llx, %016llx)\n", static_cast<unsigned long long>(imp.mp_hash),
              static_cast<unsigned long long>(hand.mp_hash), static_cast<unsigned long long>(imp.hash), static_cast<unsigned long long>(hand.hash));
  EXPECT(imp.mp_hash != 0 && imp.mp_hash == hand.mp_hash && imp.hash != hand.hash);
  EXPECT(imp.mp_names == std::vector<std::string>{"Big Guns"} && hand.mp_names == imp.mp_names && imp.hashes.size() == 1 && imp.hashes == hand.hashes);
  EXPECT(mech3::mp_mods_difference(mech3::MpMods{imp.mp_names, imp.hashes}, mech3::MpMods{hand.mp_names, hand.hashes}).empty());
  // And they play the same archive.
  const std::string ra = mounted_reader(wa, {a.folder}, inst), rb = mounted_reader(wb, {"hand"}, inst);
  EXPECT(ra == "a.zrd=A|b.zrd=B1|c.zrd=C|d.zrd=D1|" && rb == "d.zrd=D1|c.zrd=C|B.ZRD=B1|a.zrd=A|");
  // One entry otherwise: another number, and the mod named.
  lay(wb + "/mods/hand", {{"a.zrd", "A"}, {"b.zrd", "B2"}, {"c.zrd", "C"}, {"d.zrd", "D1"}});
  const mech3::ModsIdentity other = identity(wb, "hand");
  EXPECT(other.mp_hash != 0 && other.mp_hash != imp.mp_hash && other.mp_names == imp.mp_names && other.hashes != imp.hashes);
  const std::string said = mech3::mp_mods_difference(mech3::MpMods{imp.mp_names, imp.hashes}, mech3::MpMods{other.mp_names, other.hashes});
  std::printf("  %s\n", said.c_str());
  EXPECT(said == "Your copy of Big Guns differs from the host's.");
  // A whole archive without one of the game's entries hides it; the imported one keeps the game's: not the same game.
  lay(wb + "/mods/hand", {{"a.zrd", "A"}, {"b.zrd", "B1"}, {"d.zrd", "D1"}});
  EXPECT(identity(wb, "hand").mp_hash != imp.mp_hash);
  // Nothing the mounted game would read otherwise: no number, not named (the folder's own number is not 0).
  const std::string wc = tmp("mp_hash_wc");
  put(wc + "/mods/same/mod.txt", "name=Same\n" + for_us);
  archive(wc + "/mods/same/zbd/reader.zbd", kReader);
  put(wc + "/mods/same/zbd/c1/gamez.zbd", "a world");
  const mech3::ModsIdentity same = identity(wc, "same");
  EXPECT(same.mp_hash == 0 && same.mp_names.empty() && same.hashes.empty() && same.hash != 0);
  // No install given (a test's, or a bare start): every file counts, as the folder's own number does.
  mech3::mods_identity_set(wb, {"hand"});
  EXPECT(mech3::mods_identity().mp_hash != 0);
  mech3::mods_identity() = keep_id;
}

#if !TEST_IS_PM  // MechWarrior 3's two versions; Pirate's Moon has one
// Owen, 2026-11-11: "they shouldn't need the same version of the original game. Ours is built on patch 1.2."
// The lobby's number is against the game as played: on 1.0's files the rebuild plays 1.2 (mech3/patch12.h), so
// the same mod gives one number on a 1.0 install and on a 1.2 one, imported or laid by hand. On the real
// installs (game/mw3-1.0 and game/mw3-1.2, only read): 1.2's own archives laid over 1.0 count for nothing; a mod
// made from 1.2's reader.zbd and c4b mission 5 with one entry really changed matches all four ways and is named
// when another player's copy changes that entry otherwise.
TEST(mp_mods_hash_same_on_10_and_12_installs) {
  const std::string root = std::string(MW3_SOURCE_DIR) + "/game", v10 = root + "/mw3-1.0", v12 = root + "/mw3-1.2";
  if (!fs::exists(v10 + "/zbd/reader.zbd") || !fs::exists(v12 + "/zbd/reader.zbd")) return;
  const mech3::ModsIdentity keep_id = mech3::mods_identity();
  KeepMounts keep;
  zutl::mounts().confined = true;
  // Every reader archive the patch touched: 1.2's whole file over 1.0's is the game as played already.
  int count = 0, archives = 0;
  const mech3::DataFile* all = mech3::data_files(&count);
  for (int i = 0; i < count; ++i) {
    if (!mech3::record_archive(all[i].path)) continue;
    ++archives;
    std::string key, bytes;
    const bool counts = mech3::mp_mounted_part(v12 + "/" + all[i].path, all[i].path, v10, &key, &bytes);
    if (counts) std::printf("  %s over 1.0's still counts (%zu bytes): %.200s\n", all[i].path, bytes.size(), bytes.c_str());
    EXPECT(!counts);
    EXPECT(!mech3::mp_mounted_part(v12 + "/" + all[i].path, all[i].path, v12, &key, &bytes));
    // And 1.0's own whole file over a 1.2 install is not 1.2: it counts.
    if (all[i].size_10) EXPECT(mech3::mp_mounted_part(v10 + "/" + all[i].path, all[i].path, v12, &key, &bytes));
  }
  std::printf("  %d reader archives of the patch's\n", archives);
  EXPECT(archives >= 10);
  // The mod: 1.2's two archives, one entry of reader.zbd changed.
  const auto remake = [&](const std::string& from, const std::string& to, const char* entry, const std::string& text) {
    std::vector<mech3::ArchiveEntry> in;
    EXPECT(mech3::archive_read_entries(from, &in));
    std::error_code ec;
    fs::create_directories(fs::path(to).parent_path(), ec);
    const bool confined = zutl::mounts().confined;
    zutl::mounts().confined = false;
    zutl_zar::Archive a;
    zutl_zar::init(a);
    zutl_zar::create(a, to.c_str());
    bool found = false;
    for (const mech3::ArchiveEntry& e : in) {
      const bool it = entry && e.name == entry;
      found = found || it;
      const std::string data = it ? zrd(text) : std::string(e.data.begin(), e.data.end());
      zutl_zar::add(a, e.name.c_str(), data.data(), static_cast<uint32_t>(data.size()));
    }
    zutl_zar::close(a);
    zutl_zar::destroy(a);
    zutl::mounts().confined = confined;
    EXPECT(found || !entry);
  };
  const auto lay = [&](const std::string& dir, const std::string& text) {
    put(dir + "/mod.txt", "name=Big Guns\nfor=mw3\n");
    remake(v12 + "/zbd/reader.zbd", dir + "/zbd/reader.zbd", "sounds.zrd", text);
    remake(v12 + "/zbd/c4b/readerm5.zbd", dir + "/zbd/c4b/readerm5.zbd", nullptr, "");
  };
  const std::string src = tmp("mp_ver_src");
  lay(src + "/Big Guns", "changed");
  const auto identity = [&](const std::string& wd, const std::string& folder, const std::string& inst) {
    mech3::mods_identity_set(wd, {folder}, inst);
    return mech3::mods_identity();
  };
  std::vector<mech3::ModsIdentity> ids;
  const char* names_of[] = {"imported on 1.0", "imported on 1.2", "by hand on 1.0", "by hand on 1.2"};
  for (int i = 0; i < 4; ++i) {
    const std::string wd = tmp(("mp_ver_w" + std::to_string(i)).c_str()), inst = i % 2 ? v12 : v10;
    std::string folder = "hand";
    if (i < 2) {
      const mech3::ModAddDone d = mech3::mod_add_run(wd, src + "/Big Guns", inst, nullptr, nullptr);
      std::printf("  %s: %s (%d archives cut, %d entries, %d same, %d whole)\n", names_of[i], d.message.c_str(), d.cut.archives, d.cut.entries, d.cut.same, d.cut.whole);
      EXPECT(d.ok);
      if (!d.ok) return;
      folder = d.folder;
    } else {
      lay(wd + "/mods/hand", "changed");
    }
    ids.push_back(identity(wd, folder, inst));
    std::printf("  %s: %016llx\n", names_of[i], static_cast<unsigned long long>(ids.back().mp_hash));
  }
  for (int i = 0; i < 4; ++i) {
    EXPECT(ids[i].mp_hash != 0 && ids[i].mp_hash == ids[0].mp_hash && ids[i].hashes == ids[0].hashes);
    EXPECT(ids[i].mp_names == std::vector<std::string>{"Big Guns"});
  }
  // Another player's copy changes that entry otherwise, on the other version's files: named.
  const std::string wo = tmp("mp_ver_other");
  lay(wo + "/mods/hand", "changed otherwise");
  const mech3::ModsIdentity other = identity(wo, "hand", v10);
  EXPECT(other.mp_hash != 0 && other.mp_hash != ids[1].mp_hash);
  EXPECT(mech3::mp_mods_difference(mech3::MpMods{ids[1].mp_names, ids[1].hashes}, mech3::MpMods{other.mp_names, other.hashes}) ==
         "Your copy of Big Guns differs from the host's.");
  mech3::mods_identity() = keep_id;
}
#endif
