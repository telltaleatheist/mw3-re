#include "mech3/game_setup.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>

#include "gamez/zutil/zutl_zar.h"
#include "mech3/data_version.h"
#include "mech3/disc_install.h"
#include "mech3/patch_import.h"
#include "platform/cue_audio.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mech3::setup {
namespace fs = std::filesystem;
namespace {

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
  return s;
}

std::string mb(uint64_t bytes) { return std::to_string((bytes + (1u << 19)) >> 20) + " MB"; }

bool slurp(const fs::path& p, std::vector<uint8_t>* out) {
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) return false;
  out->clear();
  uint8_t buf[1 << 16];
  size_t got;
  while ((got = std::fread(buf, 1, sizeof buf, f)) > 0) out->insert(out->end(), buf, buf + got);
  std::fclose(f);
  return true;
}

// The 28 data files (data_version.h) in `overlay` (may be empty) before `dir`,
// each by size and CRC: "1.2", "1.0", "1.0 with 1.2 files" or "unknown".
std::string version_of(const std::string& dir, const std::string& overlay) {
  int n = 0, of_10 = 0, of_12 = 0, in_10 = 0, other = 0;
  const DataFile* files = data_files(&n);
  std::vector<uint8_t> d;
  for (int i = 0; i < n; ++i) {
    const DataFile& f = files[i];
    if (f.size_10) ++in_10;
    std::error_code ec;
    const fs::path in_overlay = fs::path(overlay) / f.path;
    const bool got = (!overlay.empty() && fs::exists(in_overlay, ec) && slurp(in_overlay, &d)) || slurp(fs::path(dir) / f.path, &d);
    if (!got) continue;
    const uint32_t crc = zutl_zar::crc32(0, d.data(), d.size());
    if (d.size() == f.size_12 && crc == f.crc_12) ++of_12;
    else if (f.size_10 && d.size() == f.size_10 && crc == f.crc_10) ++of_10;
    else ++other;
  }
  if (other) return "unknown";
  if (of_12 == n) return "1.2";
  if (of_10 == in_10 && of_12 == 0) return "1.0";
  if (of_10 + of_12 >= in_10) return "1.0 with 1.2 files";
  return "unknown";
}

std::string version_words(const std::string& v) {
  if (v == "1.2" || v == "1.0") return "MechWarrior 3 " + v;
  if (v == "1.0 with 1.2 files") return "MechWarrior 3 (partly patched to 1.2)";
  return "MechWarrior 3 (a version we don't know, perhaps modded)";
}

ProgressFn forward_only(const ProgressFn& p) { return p ? p : [](const Progress&) { return true; }; }

}  // namespace

Result install_from_disc(const std::string& source, const std::string& out_dir, const std::string& patch,
                         const std::string& music_dir, const ProgressFn& progress) {
  const ProgressFn p = forward_only(progress);
  const DiscInstall d = mech3::install_from_disc(
      source, out_dir, patch, music_dir,
      [&](const std::string& stage, double f, const std::string& msg) { return p(Progress{stage, f, msg}); });
  Result r;
  r.ok = d.ok, r.cancelled = d.failure == DiscInstall::kCancelled;
  r.install_dir = out_dir, r.version = d.version;
  if (d.ok && d.version == "unknown") r.version = version_of(out_dir, "");  // not the 1.0 disc's: by size and CRC
  r.files = d.files, r.movies = d.movies, r.music_tracks = d.music, r.bytes = d.bytes;
  for (const std::string& l : d.log) r.detail += l + "\n";
  if (d.ok) {
    r.summary = version_words(r.version) + " is installed in " + out_dir + " (" + std::to_string(d.files) +
                " files and " + std::to_string(d.movies) + " movies, " + mb(d.bytes) + ").";
    if (d.music)
      r.summary += d.music == 1 ? " The disc's music track was added too."
                                : " The disc's " + std::to_string(d.music) + " music tracks were added too.";
    if (r.version == "1.0") r.warnings.push_back("This is version 1.0. Applying the official 1.2 patch is recommended.");
    if (r.version == "unknown")
      r.warnings.push_back("Some of its files are not the ones on the MechWarrior 3 1.0 disc, so this may be "
                           "another release or a modified copy. It may still play.");
    for (const std::string& w : d.warnings)
      if (w.rfind("music: ", 0) == 0) r.warnings.push_back("The music could not be copied: " + w.substr(7) + ".");
    return r;
  }
  r.detail += d.error + "\n";
  switch (d.failure) {
    case DiscInstall::kCancelled: r.summary = "The installation was cancelled. Nothing was changed."; break;
    case DiscInstall::kUnreadable:
      r.summary = "That could not be read as a disc, a disc image (.iso) or a cue sheet (.cue). Check that the disc "
                  "is in the drive, or that the image file is complete.";
      break;
    case DiscInstall::kNotMw3:
      r.summary = "That does not look like the MechWarrior 3 disc: it has no MechWarrior 3 installer data on it.";
      break;
    case DiscInstall::kDamaged:
      r.summary = "Part of the disc could not be read correctly, so nothing was installed. Clean the disc and try "
                  "again, or make a new image of it.";
      break;
    case DiscInstall::kFolderInUse:
      r.summary = "The folder " + out_dir + " already has other files in it. Choose an empty folder.";
      break;
    case DiscInstall::kCannotWrite:
      r.summary = "The game could not be written to " + out_dir + ". Is the disk full, or the folder read-only?";
      break;
    case DiscInstall::kPatch:
      r.summary = "The game was not installed, because the 1.2 patch given with it is not the official patch or is "
                  "damaged. Try again without the patch, or with another copy of it.";
      break;
    case DiscInstall::kNone: r.summary = "The installation failed."; break;
  }
  return r;
}

Result apply_patch(const std::string& source, const std::string& write_dir, const ProgressFn& progress) {
  const ProgressFn p = forward_only(progress);
  Result r;
  if (!p(Progress{"Reading the patch", 0, source})) {
    r.cancelled = true;
    r.summary = "Cancelled. Nothing was changed.";
    return r;
  }
  if (write_dir.empty()) {
    r.summary = "There is no settings folder to put the patch in.";
    return r;
  }
  const PatchImport i = import_patch_12(source, write_dir + "/patch12");
  for (const std::string& l : i.log) r.detail += l + "\n";
  r.files = i.found;
  if (i.ok) {
    p(Progress{"Done", 1, ""});
    r.ok = true, r.version = "1.2";
    r.summary = "The 1.2 patch is applied: " + std::to_string(i.found) +
                " of the game's files are now 1.2's. Your MechWarrior 3 folder itself was not changed.";
    return r;
  }
  r.detail += i.error + "\n";
  r.summary = i.found == 0
                  ? "That is not the official MechWarrior 3 1.2 patch (none of its files were found). Choose the "
                    "patch's .zip, or the folder you unpacked it into."
                  : "That patch is incomplete or damaged: " + std::to_string(i.found) +
                        " of its 28 files were found. Nothing was changed. Try another copy of the patch.";
  return r;
}

Result add_music(const std::string& cue, const std::string& music_dir, const ProgressFn& progress) {
  const ProgressFn p = forward_only(progress);
  Result r;
  std::vector<platform::CueTrack> tracks;
  std::string err;
  if (!platform::read_cue(cue, &tracks, &err)) {
    r.detail = err;
    r.summary = "That file could not be read as a cue sheet (.cue).";
    return r;
  }
  const auto audio = std::count_if(tracks.begin(), tracks.end(), [](const platform::CueTrack& t) { return t.audio; });
  if (audio == 0) {
    r.summary = "That disc image has no CD audio tracks, so there is no music in it to add. (Only an image that kept "
                "the audio, such as a .cue with its .bin files, has the music.)";
    return r;
  }
  const std::string tmp = music_dir + ".new";
  std::error_code ec;
  fs::remove_all(tmp, ec);
  const int32_t n = platform::write_cue_audio(cue, tmp, nullptr, &err, [&](uint64_t got, uint64_t all) {
    if (!p(Progress{"Writing the music", all ? static_cast<double>(got) / static_cast<double>(all) : 0, ""}))
      r.cancelled = true;
    return !r.cancelled;
  });
  if (r.cancelled || n <= 0) {
    fs::remove_all(tmp, ec);
    r.detail = err;
    r.summary = r.cancelled ? "Cancelled. The music was not changed."
                            : "The music could not be copied from that disc image. Check that its .bin files are "
                              "beside the .cue.";
    return r;
  }
  fs::create_directories(music_dir, ec);
  for (const auto& e : fs::directory_iterator(tmp, ec)) {
    const fs::path to = fs::path(music_dir) / e.path().filename();
    fs::remove(to, ec);
    fs::rename(e.path(), to, ec);
  }
  fs::remove_all(tmp, ec);
  p(Progress{"Done", 1, ""});
  r.ok = true, r.music_tracks = n;
  r.summary = std::to_string(n) + (n == 1 ? " music track was" : " music tracks were") + " added.";
  if (!err.empty()) r.warnings.push_back("Some of the tracks could not be copied."), r.detail = err;
  return r;
}

Result check_install(const std::string& dir, const std::string& write_dir) {
  Result r;
  r.install_dir = dir;
  std::error_code ec;
  if (!fs::exists(fs::path(dir) / "zbd" / "reader.zbd", ec)) {
    r.summary = "No MechWarrior 3 was found in " + dir + ": it has no zbd folder with the game's data.";
    return r;
  }
  const std::string overlay = write_dir.empty() ? std::string() : write_dir + "/patch12";
  r.version = version_of(dir, fs::is_directory(overlay, ec) ? overlay : std::string());
  r.ok = true;
  r.summary = "Found " + version_words(r.version) + " at " + dir + ".";
  if (r.version == "1.0" || r.version == "1.0 with 1.2 files")
    r.warnings.push_back("Applying the official 1.2 patch is recommended.");
  if (!fs::is_directory(fs::path(dir) / "video", ec))
    r.warnings.push_back("It has no video folder, so the game will play without its movies.");
  return r;
}

Result probe_source(const std::string& path) {
  Result r;
  std::error_code ec;
  // An installed copy, not a disc.
  if (fs::is_directory(path, ec) && fs::exists(fs::path(path) / "zbd" / "reader.zbd", ec)) {
    r = check_install(path, "");
    if (r.ok) r.summary = "This is an installed copy of " + version_words(r.version) + ", not a disc. Use it as your install instead.";
    return r;
  }
  Disc disc;
  std::string err;
  if (!disc.open(path, &err)) {
    r.detail = err;
    r.summary = "That could not be read as a disc, a disc image (.iso) or a cue sheet (.cue).";
    return r;
  }
  const Disc::File* hdr = disc.find("DATA1.HDR");
  if (!hdr) {
    r.summary = "That does not look like the MechWarrior 3 disc: it has no MechWarrior 3 installer data on it.";
    return r;
  }
  std::vector<uint8_t> h(static_cast<size_t>(hdr->size));
  std::vector<std::pair<std::string, uint64_t>> files;
  if (!disc.read(*hdr, 0, h.size(), h.data()) || !list_installshield(h, &files, &err)) {
    r.detail = err;
    r.summary = "The disc's installer data could not be read. The disc or image may be damaged.";
    return r;
  }
  uint64_t reader = 0;
  bool expansion = false;
  for (const auto& [p, size] : files) {
    const std::string l = lower(p);
    if (l == "zbd/reader.zbd") reader = size;
    if (l.find("readersa1.zbd") != std::string::npos) expansion = true;  // Pirate's Moon's standalone missions
  }
  if (expansion) {
    r.summary = "This is the Pirate's Moon expansion disc. Pirate's Moon is not supported yet: use the MechWarrior 3 disc.";
    return r;
  }
  if (!reader) {
    r.summary = "That does not look like the MechWarrior 3 disc: its installer has no game data.";
    return r;
  }
  int n = 0;
  const DataFile* d = data_files(&n);
  r.version = reader == d[0].size_10 ? "1.0" : reader == d[0].size_12 ? "1.2" : "unknown";  // d[0]: zbd/reader.zbd
  for (const Disc::File& f : disc.files()) {
    const std::string l = lower(f.path);
    if (l.rfind("video/", 0) == 0 && l.find('/', 6) == std::string::npos) ++r.movies, r.bytes += f.size;
  }
  for (const auto& f : files) r.bytes += f.second;
  r.files = static_cast<int>(files.size());
  if (!disc.cue.empty()) {
    std::vector<platform::CueTrack> tracks;
    if (platform::read_cue(disc.cue, &tracks, &err))
      for (const platform::CueTrack& t : tracks) r.music_tracks += t.audio;
  }
  r.ok = true;
  const std::string kind = !disc.cue.empty() ? "disc image (.cue)" : disc.kind() == "folder" ? "disc" : "disc image";
  r.summary = version_words(r.version) + " " + kind;
  if (r.music_tracks)
    r.summary += ", with " + std::to_string(r.music_tracks) + (r.music_tracks == 1 ? " music track" : " music tracks");
  r.summary += ": about " + mb(r.bytes) + " to install.";
  if (disc.cue.empty() && kind != "disc")
    r.warnings.push_back("An .iso image has no CD audio, so the game's music is not in it.");
  return r;
}

std::vector<DiscSource> find_discs() {
  std::vector<DiscSource> out;
  auto has_disc = [](const fs::path& root) {
    std::error_code ec;
    return fs::exists(root / "DATA1.HDR", ec) || fs::exists(root / "data1.hdr", ec);
  };
#if defined(_WIN32)
  const DWORD drives = GetLogicalDrives();
  for (int i = 0; i < 26; ++i) {
    if (!(drives & (1u << i))) continue;
    const char root[4] = {static_cast<char>('A' + i), ':', '\\', 0};
    if (GetDriveTypeA(root) != DRIVE_CDROM) continue;
    char label[MAX_PATH + 1] = {};
    // An empty drive fails here (no disc), and is skipped.
    if (!GetVolumeInformationA(root, label, sizeof label, nullptr, nullptr, nullptr, nullptr, 0)) continue;
    if (has_disc(root)) out.push_back({root, label[0] ? label : root});
  }
#else
  std::vector<fs::path> parents;
#if defined(__APPLE__)
  parents.push_back("/Volumes");
#else
  if (const char* user = std::getenv("USER")) {
    parents.push_back(fs::path("/media") / user);
    parents.push_back(fs::path("/run/media") / user);
  }
  parents.push_back("/media");
  parents.push_back("/mnt");
#endif
  std::error_code ec;
  for (const fs::path& parent : parents)
    for (const auto& e : fs::directory_iterator(parent, fs::directory_options::skip_permission_denied, ec))
      if (e.is_directory(ec) && has_disc(e.path())) {
        const std::string p = e.path().string();
        if (std::none_of(out.begin(), out.end(), [&](const DiscSource& s) { return s.path == p; }))
          out.push_back({p, e.path().filename().string()});
      }
#endif
  return out;
}

}  // namespace mech3::setup
