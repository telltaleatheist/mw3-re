#include "upgrades/mods/entry_archives.h"
#include "gamez/zutil/zutl_path.h"
#include "upgrades/mods/mod_export.h"
#include "upgrades/mods/mod_report.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>

#include "gamez/zutil/zutl_path.h"
#include "mech3/net_save.h"
#include "mech3/mod_install.h"
#include "mech3/patch_import.h"

#ifdef MW3_HAVE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

namespace mech3 {
namespace fs = std::filesystem;

namespace {

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool ends_with(const std::string& s, const char* tail) {
  const std::string t = tail;
  return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
}

// What the systems leave in a folder they showed: never the mod's.
bool system_note(const std::string& rel) {
  const size_t slash = rel.rfind('/');
  const std::string name = lower(slash == std::string::npos ? rel : rel.substr(slash + 1));
  return name == ".ds_store" || name == "thumbs.db" || name == "desktop.ini" || name.rfind("._", 0) == 0;
}

// Ours or the player's, not the mod's: the markers, the checksums (made
// anew), what a mod may never serve, a backup or a half-written file.
bool left_out(const std::string& rel) {
  const std::string l = lower(rel);
  // The editor's marker too (net_save.h): a copy exported and imported elsewhere is an imported mod there,
  // never written by that player's editor; their edits make their own copy of it.
  return l == lower(kModAddedMarker) || l == lower(kModStagingMarker) || l == lower(kModChecksums) || l == lower(kModLeftOutFile) || l == lower(ai::kEditorMarker) ||
         !zutl::mod_may_serve(rel) || ends_with(l, ".bak") || ends_with(l, ".saving") || system_note(rel);
}

std::string sum_of(const fs::path& p, std::string* why) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return *why = "cannot read " + p.filename().string(), "";
  Sha256 s;
  char buf[65536];
  while (f) {
    f.read(buf, sizeof buf);
    s.add(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(f.gcount()));
  }
  if (f.bad()) return *why = "cannot read " + p.filename().string(), "";
  return s.hex();
}

}  // namespace

ModExportList mod_export_list(const std::string& mod_dir) {
  ModExportList out;
  std::error_code ec;
  const fs::path root(mod_dir);
  if (!fs::is_directory(root, ec)) return out.error = "its folder is not there any more", out;
  const ModAddLimits limits;
  int64_t bytes = 0;
  std::map<std::string, std::string> seen;  // in lower case, as first found
  for (fs::recursive_directory_iterator it(root, fs::directory_options::none, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string rel = fs::relative(it->path(), root, ec).generic_string();
    if (it->is_symlink(ec)) return out.error = "it holds a link (" + rel + "), which a mod cannot have", out;
    if (it->is_directory(ec)) {
      if (!zutl::mod_may_serve(rel + "/")) {  // pilots\ and the like: the player's, never looked into
        out.left_out.push_back(rel + "/");
        it.disable_recursion_pending();
      }
      continue;
    }
    if (!it->is_regular_file(ec)) return out.error = "it holds a special file (" + rel + ")", out;
    if (left_out(rel)) {
      out.left_out.push_back(rel);
      continue;
    }
    std::string clean;
    if (!mod_entry_path_ok(rel, &clean) || clean != rel)
      return out.error = "it holds a name Windows cannot hold (" + rel + "): rename it first", out;
    if (const auto [at, fresh] = seen.emplace(lower(rel), rel); !fresh)
      return out.error = "it holds two files whose names differ only in case (" + at->second + ", " + rel + ")", out;
    bytes += static_cast<int64_t>(it->file_size(ec));
    out.files.push_back(rel);
  }
  if (ec) return out.error = "cannot read its folder: " + ec.message(), out;
  if (static_cast<int64_t>(out.files.size()) > limits.max_files)
    return out.error = "it holds more than " + std::to_string(limits.max_files) + " files", out;
  if (bytes > limits.max_bytes) return out.error = "it holds more than " + std::to_string(limits.max_bytes >> 20) + " MB", out;
  if (out.files.empty()) return out.error = "it holds no files of its own", out;
  std::sort(out.files.begin(), out.files.end());
  std::sort(out.left_out.begin(), out.left_out.end());
  return out;
}

std::string mod_export_file_name(const ModInfo& info) {
  const std::string name = info.name.empty() ? info.folder : info.name;
  return mod_folder_name(info.version.empty() ? name : name + " " + info.version) + ".zip";
}

ModExportResult mod_export(const std::string& mod_dir, const std::string& zip_path, const std::string& install, bool check) {
  ModExportResult r;
  std::error_code ec;
  const fs::path root(mod_dir), zip(zip_path);
  {
    const std::string m = fs::weakly_canonical(root, ec).generic_string();
    const std::string z = fs::weakly_canonical(zip, ec).generic_string();
    if (z.rfind(m + "/", 0) == 0) return r.message = "Not exported: the zip cannot go inside the mod's own folder.", r;
  }
  const ModExportList list = mod_export_list(mod_dir);
  if (!list.error.empty()) return r.message = "Not exported: " + list.error + ".", r;
#ifndef MW3_HAVE_LIBARCHIVE
  return r.message = "Not exported: this build cannot write archives.", r;
#else
  ModInfo info;
  {
    std::ifstream f(root / "mod.txt", std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    mod_parse(ss.str(), &info);
  }
  info.folder = root.filename().string();
  std::string top = mod_folder_name(info.name.empty() ? info.folder : info.name);
  // Ours (modder step 8): the check first, said and never stopping the pack.
  if (check) {
    ModFindings facts;
    mod_check(mod_dir, install, &r.refused, &facts);
    r.warnings = static_cast<int>(facts.notes.size());
  }
  // A folder laid out as an install without a mod.txt: one in the zip, so IMPORT takes it as a mod of this game.
  const bool own_txt = fs::is_regular_file(root / "mod.txt", ec);
  const std::string made_txt = own_txt ? std::string() : "name=" + info.folder + "\nfor=" + mods_game() + "\n";
  const fs::path saving = fs::path(zip_path + ".saving");
  archive* a = archive_write_new();
  archive_write_set_format_zip(a);
  archive_write_set_format_option(a, "zip", "compression", "deflate");
  archive_write_set_format_option(a, "zip", "hdrcharset", "UTF-8");
  std::string why;
  auto fail = [&](const std::string& w) {
    if (why.empty()) why = w;
  };
  if (archive_write_open_filename(a, saving.string().c_str()) != ARCHIVE_OK) fail("cannot write " + saving.filename().string());
  const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  // `as`: its name in the zip when that is not the file's own (the file's time is still the file's).
  auto put = [&](const std::string& rel, const std::function<bool(archive*)>& data, int64_t size, const std::string& as = std::string()) {
    if (!why.empty()) return;
    archive_entry* e = archive_entry_new();
    archive_entry_set_pathname_utf8(e, (top + "/" + (as.empty() ? rel : as)).c_str());
    archive_entry_set_filetype(e, AE_IFREG);
    archive_entry_set_perm(e, 0644);
    archive_entry_set_size(e, size);
    // The file's own time (the checksums': now), not zip's 1980.
    std::error_code tec;
    const auto t = fs::last_write_time(root / fs::path(rel), tec);
    const auto sys = std::chrono::system_clock::now() + std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                                            t - fs::file_time_type::clock::now());  // no file_clock::to_sys in libc++
    archive_entry_set_mtime(e, tec ? now : std::chrono::system_clock::to_time_t(sys), 0);
    if (archive_write_header(a, e) != ARCHIVE_OK) fail(std::string("cannot write ") + rel + ": " + archive_error_string(a));
    else if (!data(a)) fail("cannot write " + rel + " (is the disk full?)");
    archive_entry_free(e);
  };
  std::string sums;
  std::vector<char> buf(1 << 16);
  for (const std::string& rel : list.files) {
    const fs::path p = root / fs::path(rel);
    // Both ways (entry_archives.h): an archive the mod holds as its changed entries goes into the zip whole, the
    // game's own with those entries laid over it, as the mount builds it: the exported mod is ordinary files,
    // which the original game takes too. The mod's folder is not changed.
    if (const std::string of = entries_archive_of(rel); !of.empty()) {
      const std::string own = install.empty() ? std::string() : zutl::any_case(install, of);
      std::string w;
      std::vector<uint8_t> whole;
      if (!own.empty()) {
        std::ifstream base(own, std::ios::binary);
        whole = entries_laid_over(std::vector<uint8_t>((std::istreambuf_iterator<char>(base)), std::istreambuf_iterator<char>()), p.string(), nullptr, &w);
      } else {
        w = "the game's own " + of + " is not there to lay its changed entries over";
      }
      if (whole.empty()) {
        fail(of + " cannot be made whole: " + w);
        break;
      }
      Sha256 s;
      s.add(whole.data(), whole.size());
      put(rel, [&](archive* wr) { return archive_write_data(wr, whole.data(), whole.size()) == static_cast<la_ssize_t>(whole.size()); },
          static_cast<int64_t>(whole.size()), of);
      sums += s.hex() + "  " + of + "\n";
      ++r.repacked;
      continue;
    }
    std::ifstream f(p, std::ios::binary);
    const int64_t size = static_cast<int64_t>(fs::file_size(p, ec));
    if (!f || ec) {
      fail("cannot read " + rel);
      break;
    }
    Sha256 s;
    int64_t put_n = 0;
    put(rel, [&](archive* w) {
      while (f) {
        f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const auto n = static_cast<size_t>(f.gcount());
        if (!n) break;
        s.add(reinterpret_cast<const uint8_t*>(buf.data()), n);
        if (archive_write_data(w, buf.data(), n) != static_cast<la_ssize_t>(n)) return false;
        put_n += static_cast<int64_t>(n);
      }
      return !f.bad() && put_n == size;  // a file that changed while it was read is not taken half
    }, size);
    sums += s.hex() + "  " + rel + "\n";
  }
  if (!own_txt && why.empty()) {
    put("mod.txt", [&](archive* w) { return archive_write_data(w, made_txt.data(), made_txt.size()) == static_cast<la_ssize_t>(made_txt.size()); },
        static_cast<int64_t>(made_txt.size()));
    Sha256 s;
    s.add(reinterpret_cast<const uint8_t*>(made_txt.data()), made_txt.size());
    sums += s.hex() + "  mod.txt\n";
    r.made_mod_txt = true;
  }
  put(kModChecksums, [&](archive* w) { return archive_write_data(w, sums.data(), sums.size()) == static_cast<la_ssize_t>(sums.size()); },
      static_cast<int64_t>(sums.size()));
  if (archive_write_close(a) != ARCHIVE_OK) fail(std::string("cannot finish the zip: ") + archive_error_string(a));
  archive_write_free(a);
  if (!why.empty()) {
    fs::remove(saving, ec);
    return r.message = "Not exported: " + why + ".", r;
  }
  fs::rename(saving, zip, ec);
  if (ec) {
    fs::remove(saving, ec);
    return r.message = "Not exported: cannot write " + zip.filename().string() + ".", r;
  }
  r.ok = true;
  r.files = static_cast<int>(list.files.size()), r.left_out = static_cast<int>(list.left_out.size());
  r.message = "Exported " + (info.name.empty() ? info.folder : info.name) + " to " + zip.filename().string() + " (" +
              std::to_string(r.files) + (r.files == 1 ? " file" : " files") + (r.made_mod_txt ? " and a mod.txt" : "") + ").";
  if (r.repacked) r.message += " Its " + std::to_string(r.repacked) + (r.repacked == 1 ? " archive kept as changed entries is whole in the zip." : " archives kept as changed entries are whole in the zip.");
  if (r.refused > 0) r.message += " The game would refuse this mod; its report says why.";
  else if (r.warnings > 0)
    r.message += " The check has " + std::to_string(r.warnings) + (r.warnings == 1 ? " warning about it; its report says what." : " warnings about it; its report lists them.");
  return r;
#endif
}

std::string mod_checksums_verify(const std::string& root_dir, bool* had) {
  *had = false;
  std::error_code ec;
  const fs::path root(root_dir);
  fs::path list;
  for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
    if (lower(it->path().filename().string()) == lower(kModChecksums)) list = it->path();
  if (list.empty() || !fs::is_regular_file(list, ec)) return "";
  *had = true;
  std::ifstream f(list, std::ios::binary);
  std::map<std::string, std::string> want;  // lower-case path: its sum
  std::map<std::string, std::string> named;  // lower-case path: as listed
  std::string line;
  int n = 0;
  while (std::getline(f, line)) {
    ++n;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    // sha256sum's "<64 hex>  <path>" (or " *<path>", its binary mark)
    if (line.size() < 67 || line[64] != ' ' || (line[65] != ' ' && line[65] != '*') ||
        !std::all_of(line.begin(), line.begin() + 64, [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); }))
      return "its checksums file is damaged (line " + std::to_string(n) + ")";
    std::string rel;
    if (!mod_entry_path_ok(line.substr(66), &rel) || rel.empty())
      return "its checksums file names a file outside it (line " + std::to_string(n) + ")";
    want[lower(rel)] = lower(line.substr(0, 64));
    named[lower(rel)] = rel;
  }
  std::set<std::string> found;
  for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec)) continue;
    const std::string rel = fs::relative(it->path(), root, ec).generic_string();
    const std::string l = lower(rel);
    if (it->path() == list || system_note(rel)) continue;
    const auto w = want.find(l);
    if (w == want.end()) return "it holds a file its checksums do not list (" + rel + ")";
    std::string why;
    const std::string got = sum_of(it->path(), &why);
    if (!why.empty()) return why;
    if (got != w->second) return "it is damaged: " + rel + " does not match its checksum";
    found.insert(l);
  }
  if (ec) return "cannot read it: " + ec.message();
  for (const auto& [l, rel] : named)
    if (!found.count(l)) return "a file its checksums list is missing (" + rel + ")";
  fs::remove(list, ec);
  return "";
}

}  // namespace mech3
