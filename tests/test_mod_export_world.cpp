// EXPORT of a world the game does not have (playtest Z5; docs/modding.md, EXPORT of a new world): the game's
// c1 copied as ironcoast (three missions, its own build script) and added by IMPORT, and a copy of its campaign
// holding an editor save (net_01's node_00 given a speed, written by the editor's own save into a save-as copy).
// Both exported; both zips unpacked over a copy of the install, the copy last (as the mount puts it first); every
// archive there is what the mount gives the game. The original's own readers read the unpacked archives in
// tests/differential/test_export_readers.py, which runs this test with MW3_EXPORT_KEEP=<folder> to keep them.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <sys/clonefile.h>
#endif

#include "check.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/mission.h"
#include "mech3/mod_campaign.h"
#include "mech3/mod_install.h"
#include "mech3/mods.h"
#include "mech3/net_save.h"
#include "mech3/zainet_node.h"
#include "mech3/zrdrainet.h"
#include "upgrades/editor/save_as.h"
#include "upgrades/mods/campaign_entries.h"
#include "upgrades/mods/entry_archives.h"
#include "upgrades/mods/found_worlds.h"
#include "upgrades/mods/import_folder.h"
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

std::string bytes(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
std::string fresh(const std::string& d) {
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}

// The install copied for laying a zip over: every file but the movies, each a clone where the disk makes them
// (APFS: no space taken), else a copy. Laying over removes a file first, so the install is never written.
bool copy_install(const std::string& from, const std::string& to) {
  std::error_code ec;
  for (fs::recursive_directory_iterator it(from, ec), end; !ec && it != end; it.increment(ec)) {
    const fs::path rel = fs::relative(it->path(), from, ec);
    if (rel.begin()->string() == "video") {
      if (it->is_directory()) it.disable_recursion_pending();
      continue;
    }
    const fs::path dst = fs::path(to) / rel;
    if (it->is_directory()) {
      fs::create_directories(dst, ec);
      continue;
    }
    if (!it->is_regular_file()) continue;
#if defined(__APPLE__)
    if (clonefile(it->path().c_str(), dst.c_str(), 0) == 0) continue;
#endif
    if (!fs::copy_file(it->path(), dst, ec)) return false;
  }
  return !ec;
}

#if MW3_HAVE_LIBARCHIVE
// The zip's files laid over `root`, its top folder (the mod's) taken off, as a player unpacking it into a game
// folder does; the names it laid, '/'-separated.
std::vector<std::string> lay_zip(const std::string& zip, const std::string& root) {
  std::vector<std::string> laid;
  archive* a = archive_read_new();
  archive_read_support_format_zip(a);
  if (archive_read_open_filename(a, zip.c_str(), 1 << 16) != ARCHIVE_OK) return archive_read_free(a), laid;
  archive_entry* e = nullptr;
  while (archive_read_next_header(a, &e) == ARCHIVE_OK) {
    std::string name = archive_entry_pathname(e);
    const size_t slash = name.find('/');
    if (archive_entry_filetype(e) != AE_IFREG || slash == std::string::npos || name.find("..") != std::string::npos) continue;
    name = name.substr(slash + 1);
    const fs::path dst = fs::path(root) / name;
    std::error_code ec;
    fs::create_directories(dst.parent_path(), ec);
    fs::remove(dst, ec);  // a clone of the install's file goes, the install's own stays
    std::ofstream out(dst, std::ios::binary);
    char buf[1 << 16];
    for (la_ssize_t n; (n = archive_read_data(a, buf, sizeof buf)) > 0;) out.write(buf, n);
    laid.push_back(name);
  }
  archive_read_free(a);
  return laid;
}
#endif

// The world's source as a zStudio export lays it out (test_sdl_new_world.cpp's, kept apart: these are separate
// programs): c1's folder less readerm4, c1.gs with its DB the new name.
bool make_export(const std::string& install, const std::string& src) {
  std::error_code ec;
  fs::create_directories(src + "/zbd/ironcoast", ec);
  for (const auto& f : fs::directory_iterator(install + "/zbd/c1", ec))
    if (f.path().filename() != "readerm4.zbd") fs::copy_file(f.path(), src + "/zbd/ironcoast/" + f.path().filename().string(), ec);
  std::string script;
  for (std::vector<std::string> line : mech3::interp_archive_script(install + "/zbd/interp.zbd", "c1.gs")) {
    if (line.size() == 3 && line[0] == "set" && line[1] == "DB" && line[2] == "c1") line[2] = "ironcoast";
    for (size_t i = 0; i < line.size(); ++i) script += (i ? " " : "") + line[i];
    script += "\r\n";
  }
  std::ofstream(src + "/ironcoast.gs", std::ios::binary) << script;
  return script.find("set DB ironcoast\r\n") != std::string::npos && !ec;
}

// The editor's save: net_01's node_00 given speed 0.25 in readerm1, written by save_archive_edit into the
// campaign's save-as copy (the mod a save goes into, as the editor sets it for a mission entered by MODS's EDIT).
bool editor_save(const std::string& wd, std::string* copy) {
  std::string why;
  if (!mech3::save_as_make_copy(wd, "Iron Coast~ironcoast", "Iron Coast, my edit", copy, &why)) return std::printf("  copy: %s\n", why.c_str()), false;
  const char* rel = "zbd\\ironcoast\\readerm1.zbd";
  const std::string host = zutl::find(rel);
  std::vector<mech3::ArchiveEntry> entries;
  if (host.empty() || !mech3::archive_read_entries(host, &entries)) return false;
  const auto net = std::find_if(entries.begin(), entries.end(), [](const mech3::ArchiveEntry& e) { return e.name == "net_01.zrd"; });
  if (net == entries.end()) return false;
  zrdr::close_archives(true);
  zrdr::init();
  std::vector<uint8_t> out;
  if (zrdr::open_archive(host.c_str(), true)) {
    mech3::ai::NetFile f(1, "net_01.zrd");
    if (f.load() != 0)
      if (const mech3::ai::NetNodeRec* was = f.node_named("node_00")) {
        mech3::ai::NetNodeRec rec = *was;
        rec.speed_on = 1, rec.speed = 0.25f;
        out = mech3::ai::net_with_edits(net->data.data(), net->data.size(), f.tree, {{"node_00", &rec}});
      }
    f.unload();
  }
  zrdr::close_archives(true);
  if (out.empty()) return false;
  mech3::ai::net_save_into_mod() = *copy;
  const mech3::ai::NetSave s = mech3::ai::save_archive_edit(rel, "net_01.zrd", out);
  mech3::ai::net_save_into_mod().clear();
  std::printf("  the editor's save: %s%s\n", s.message.c_str(), s.error.c_str());
  return s.ok;
}

}  // namespace

TEST(mod_export_new_world_and_its_editor_copy_unpack_as_mounted) {
#if MW3_HAVE_LIBARCHIVE
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/c1/gamez.zbd", ec) || !fs::exists(install + "/zbd/interp.zbd", ec)) return;
  const char* keep_env = std::getenv("MW3_EXPORT_KEEP");
  const std::string dir = fresh(std::string(MW3_TEST_TMP) + "/mod_export_world");
  const std::string wd = fresh(dir + "/wd"), out = fresh(dir + "/out"), src = dir + "/export/Iron Coast";
  const std::string root = keep_env && *keep_env ? fresh(keep_env) : fresh(dir + "/install copy");
  const zutl::Mounts keep = zutl::mounts();
  zutl::mounts() = zutl::Mounts{};
  zutl::mounts().confined = true;
  EXPECT(make_export(install, src));
  const mech3::ModAddDone added = mech3::import_folder_run(wd, src, install, nullptr, nullptr);
  EXPECT(added.ok && added.folder == "Iron Coast");
  // Mounted as a start mounts it, then the editor's save into a copy of its campaign.
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mod_worlds().clear(), mech3::mod_ia_worlds().clear();
  mech3::mods_mount(wd, {"Iron Coast"}, {install});
  zutl::mount(install);
  std::string copy;
  EXPECT(editor_save(wd, &copy));
  const std::string world_mod = wd + "/mods/Iron Coast", copy_mod = wd + "/mods/" + copy;
  EXPECT(fs::is_regular_file(copy_mod + "/zbd/ironcoast/readerm1.zbd") && fs::is_regular_file(copy_mod + "/" + mech3::ai::kEditorMarker));
  // Both exported, checked against the install as EXPORT does.
  const mech3::ModExportResult a = mech3::mod_export(world_mod, out + "/Iron Coast.zip", install, true);
  const mech3::ModExportResult b = mech3::mod_export(copy_mod, out + "/my edit.zip", install, true);
  std::printf("  world: %s\n  copy: %s\n", a.message.c_str(), b.message.c_str());
  EXPECT(a.ok && a.refused == 0 && b.ok && b.refused == 0);
  // Unpacked over the install's copy: the world, then the copy over it.
  EXPECT(copy_install(install, root));
  const std::vector<std::string> laid_a = lay_zip(out + "/Iron Coast.zip", root), laid_b = lay_zip(out + "/my edit.zip", root);
  const auto laid = [](const std::vector<std::string>& v, const char* n) { return std::find(v.begin(), v.end(), n) != v.end(); };
  EXPECT(laid(laid_a, "zbd/ironcoast/gamez.zbd") && laid(laid_a, "ironcoast.gs") && laid(laid_b, "zbd/ironcoast/readerm1.zbd"));
  EXPECT(!laid(laid_b, mech3::ai::kEditorMarker));  // EXPORT leaves the editor's marker out
  // What the game reads with both mounted, the copy first (its campaign's), against what is unpacked.
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mod_worlds().clear(), mech3::mod_ia_worlds().clear(), mech3::mod_campaigns().clear();
  mech3::mods_mount(wd, {copy, "Iron Coast"}, {install});
  zutl::mount(install);
  int archives = 0, same = 0, entries = 0;
  std::vector<std::string> archives_laid;
  for (const std::vector<std::string>* v : {&laid_a, &laid_b})
    for (const std::string& n : *v)
      if (n.size() > 4 && n.compare(n.size() - 4, 4, ".zbd") == 0 && std::find(archives_laid.begin(), archives_laid.end(), n) == archives_laid.end())
        archives_laid.push_back(n);
  for (const std::string& n : archives_laid) {
    std::string game = n;
    std::replace(game.begin(), game.end(), '/', '\\');
    const std::string mounted = zutl::find(game.c_str()), unpacked = root + "/" + n;
    ++archives;
    std::vector<mech3::ArchiveEntry> m, u;
    const bool record = mech3::record_archive(n);
    bool equal = !mounted.empty() && bytes(mounted) == bytes(unpacked);
    if (record && !mounted.empty() && mech3::archive_read_entries(mounted, &m) && mech3::archive_read_entries(unpacked, &u)) {
      entries += static_cast<int>(u.size());
      equal = m.size() == u.size() && std::equal(m.begin(), m.end(), u.begin(), [](const mech3::ArchiveEntry& x, const mech3::ArchiveEntry& y) {
                return x.name == y.name && x.data == y.data;
              });
    }
    if (equal) ++same;
    else std::printf("  %s: unpacked %s the mounted (%s)\n", n.c_str(), "differs from", mounted.c_str());
  }
  std::printf("  unpacked over the install's copy: %d archives, %d as mounted (%d records read by entry)\n", archives, same, entries);
  EXPECT(archives == 17 && same == archives);
  // The edit is in the unpacked readerm1, the world's own other missions untouched.
  {
    std::vector<mech3::ArchiveEntry> u, c1;
    EXPECT(mech3::archive_read_entries(root + "/zbd/ironcoast/readerm1.zbd", &u) &&
           mech3::archive_read_entries(install + "/zbd/c1/readerm1.zbd", &c1));
    int changed = 0;
    for (size_t i = 0; i < u.size() && i < c1.size(); ++i) changed += u[i].name != c1[i].name || u[i].data != c1[i].data;
    EXPECT(u.size() == c1.size() && changed == 1);
    EXPECT(bytes(root + "/zbd/ironcoast/readerm2.zbd") == bytes(install + "/zbd/c1/readerm2.zbd"));
  }
  zrdr::close_archives(true);
  mech3::mod_worlds().clear(), mech3::mod_ia_worlds().clear(), mech3::mod_campaigns().clear();
  zutl::mounts() = keep;
  fs::remove_all(dir, ec);
  if (keep_env && *keep_env) std::printf("  kept: %s\n", root.c_str());
#endif
}
