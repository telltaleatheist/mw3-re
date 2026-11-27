#include "frontend/sdl/sdl_games.h"

#include "frontend/sdl/sdl_update.h"
#include "rekit/game_folder.h"  // kGameFolderMark
#include "rekit/hash.h"
#include "rekit/data_folder.h"
#include "rekit/archive.h"
#include "rekit/family_folder.h"
#include "rekit/program_record.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <ctime>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace fs = std::filesystem;

namespace sdlfe::games {
namespace {

constexpr const char* kRootMark = "rebuild-games.txt";   // the games folder is the launcher's
constexpr const char* kStagingMark = ".rebuild-staging";  // a folder being filled, the launcher's
constexpr const char* kIncoming = ".incoming";             // where a package is unpacked before it goes in place

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
bool ends_with(const std::string& s, const char* e) {
  const size_t n = std::strlen(e);
  return s.size() >= n && s.compare(s.size() - n, n, e) == 0;
}
std::string unique_part() {
  static std::atomic<int> n{0};
  return std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(++n);
}
std::string read_text(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}
bool write_text(const fs::path& p, const std::string& text) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f << text;
  return static_cast<bool>(f);
}
bool allow_file() { return std::getenv("MW3_GAMES_API") != nullptr; }  // checks: file:// releases and downloads

// The games folder made (or found) as the launcher's: an empty or new folder gets the mark; one holding anything
// else without it is someone's own and is never written to.
bool own_root(const std::string& root, std::string* error) {
  std::error_code ec;
  if (root.empty()) return *error = "there is no games folder", false;
  fs::create_directories(root, ec);
  if (fs::exists(fs::path(root) / kRootMark, ec)) return true;
  if (!fs::is_directory(root, ec)) return *error = "the games folder " + root + " could not be made", false;
  if (!fs::is_empty(root, ec)) return *error = root + " holds files that are not the launcher's: move them and try again", false;
  if (!write_text(fs::path(root) / kRootMark, "# The MechWarrior 3 Rebuild launcher's games. Each folder is one game it installed.\n"))
    return *error = "the games folder " + root + " could not be written to", false;
  return true;
}

// A leftover of an earlier try, removed only when it carries `mark` (else it is in the way and is not ours).
bool clear_ours(const fs::path& p, const char* mark, std::string* error) {
  std::error_code ec;
  if (!fs::exists(fs::symlink_status(p, ec))) return true;
  if (!fs::is_directory(fs::symlink_status(p, ec)) || !fs::exists(p / mark, ec))
    return *error = p.string() + " is in the way and is not the launcher's: move it and try again", false;
  fs::remove_all(p, ec);
  if (ec) return *error = "an earlier try's " + p.string() + " could not be removed (" + ec.message() + ")", false;
  return true;
}

#if !defined(_WIN32) && !defined(__APPLE__)
// A program run to its end in `cwd` (an AppImage's --appimage-extract writes where it runs).
int run_in(const std::vector<std::string>& args, const std::string& cwd, int seconds, std::string* said) {
  std::vector<const char*> argv;
  for (const std::string& a : args) argv.push_back(a.c_str());
  argv.push_back(nullptr);
  const SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
  SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, cwd.c_str());
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
  SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
  SDL_Process* p = SDL_CreateProcessWithProperties(props);
  SDL_DestroyProperties(props);
  if (!p) return -1;
  int code = -1;
  bool ended = false;
  SDL_IOStream* io = SDL_GetProcessOutput(p);
  const Uint64 until = SDL_GetTicks() + static_cast<Uint64>(seconds) * 1000;
  while (!(ended = SDL_WaitProcess(p, false, &code)) && SDL_GetTicks() < until) {
    char buf[4096];
    const size_t n = io ? SDL_ReadIO(io, buf, sizeof buf) : 0;
    if (said->size() < 4096) said->append(buf, n);  // its first lines; the rest read and dropped (a full pipe stalls it)
    if (n == 0) SDL_Delay(20);
  }
  if (!ended) SDL_KillProcess(p, true), SDL_WaitProcess(p, true, &code), code = -1;
  SDL_DestroyProcess(p);
  return code;
}
#endif

// A zip's files written under `into`; the folder every entry shares (the package's own top folder) left out.
bool unzip(const std::string& file, const fs::path& into, const Progress& progress, std::string* error) {
  if (!rekit::archives_supported()) return *error = "this build cannot read zip files", false;
  std::error_code ec;
  const double total = static_cast<double>(std::max<uintmax_t>(1, fs::file_size(file, ec)));
  std::string top;
  bool one_top = true, any = false;
  {  // the shared top folder, from the listing
    rekit::ArchiveReader r;
    if (!r.open_file(file)) return *error = "the zip could not be opened (" + r.error() + ")", false;
    rekit::ArchiveReader::Entry e;
    while (r.next(&e)) {
      const std::string first = e.path.substr(0, e.path.find('/'));
      const bool deeper = e.path.find('/') != std::string::npos && e.path.find('/') + 1 < e.path.size();
      if (!any) top = first;
      any = true;
      if (first != top || (!deeper && !e.dir)) one_top = false;
    }
    if (!r.error().empty()) return *error = "the zip could not be read (" + r.error() + ")", false;
  }
  if (!any) return *error = "the zip is empty", false;
  rekit::ArchiveReader r;
  if (!r.open_file(file)) return *error = "the zip could not be opened (" + r.error() + ")", false;
  rekit::ArchiveReader::Entry e;
  std::vector<uint8_t> buf(1 << 20);
  while (r.next(&e)) {
    std::string rel = e.path;
    if (one_top) rel = rel.size() > top.size() + 1 ? rel.substr(top.size() + 1) : std::string();
    while (!rel.empty() && rel.back() == '/') rel.pop_back();
    if (rel.empty() || e.other) continue;  // the top folder itself; links and devices are never written
    // Every part a plain name: nothing written outside `into`.
    const fs::path relp = fs::path(rel);
    bool safe = !relp.is_absolute() && !relp.has_root_name() && rel.find('\\') == std::string::npos && rel.find(':') == std::string::npos;
    for (const fs::path& part : relp) safe = safe && part != ".." && part != "." && !part.empty();
    if (!safe) return *error = "the zip names a file outside its folder (" + e.path + "): not installed", false;
    const fs::path to = into / relp;
    if (e.dir) {
      fs::create_directories(to, ec);
      continue;
    }
    fs::create_directories(to.parent_path(), ec);
    std::ofstream out(to, std::ios::binary | std::ios::trunc);
    if (!out) return *error = "could not write " + to.string(), false;
    for (int64_t n; (n = r.read(buf.data(), buf.size())) > 0;) out.write(reinterpret_cast<const char*>(buf.data()), n);
    if (!out || !r.error().empty()) return *error = "could not unpack " + e.path + (r.error().empty() ? "" : " (" + r.error() + ")"), false;
    if (progress && !progress(static_cast<double>(r.position()) / total, "Unpacking")) return *error = "canceled", false;
  }
  if (!r.error().empty()) return *error = "the zip could not be read (" + r.error() + ")", false;
  return true;
}

#if defined(__APPLE__)
// The disk image attached read-only where nothing shows it, every app on it copied into `into`, detached.
bool from_dmg(const std::string& file, const fs::path& into, std::string* error) {
  std::error_code ec;
  const fs::path mnt = fs::temp_directory_path(ec) / ("MW3-Rebuild-game-mount-" + unique_part());
  fs::create_directories(mnt, ec);
  std::string said;
  if (run_program({"/usr/bin/hdiutil", "attach", "-nobrowse", "-readonly", "-noautoopen", "-mountpoint", mnt.string(), file}, 120, &said) != 0) {
    fs::remove(mnt, ec);
    return *error = "the disk image could not be opened", false;
  }
  bool any = false;
  std::string why;
  for (const auto& e : fs::directory_iterator(mnt, ec)) {
    if (e.path().extension() != ".app" || !fs::is_directory(e.path(), ec)) continue;
    std::error_code cec;
    fs::copy(e.path(), into / e.path().filename(), fs::copy_options::recursive | fs::copy_options::copy_symlinks, cec);
    if (cec) why = cec.message();
    else any = true;
  }
  std::string detach_said;
  run_program({"/usr/bin/hdiutil", "detach", mnt.string(), "-force"}, 60, &detach_said);
  fs::remove(mnt, ec);
  if (!why.empty()) return *error = "the app could not be copied off the disk image (" + why + ")", false;
  if (!any) return *error = "the disk image holds no app", false;
  return true;
}
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
// The AppImage unpacked by itself (squashfs-root in `into`), no FUSE needed; the copy run is removed after.
bool from_appimage(const std::string& file, const fs::path& into, std::string* error) {
  std::error_code ec;
  const fs::path copy = into / "package.AppImage";
  fs::copy_file(file, copy, fs::copy_options::overwrite_existing, ec);
  if (ec) return *error = "the AppImage could not be copied (" + ec.message() + ")", false;
  fs::permissions(copy, fs::perms::owner_exec | fs::perms::owner_read, fs::perm_options::add, ec);
  std::string said;
  const int code = run_in({copy.string(), "--appimage-extract"}, into.string(), 600, &said);
  fs::remove(copy, ec);
  if (code != 0 || !fs::is_directory(into / "squashfs-root", ec)) {
    while (!said.empty() && (said.back() == '\n' || said.back() == '\r')) said.pop_back();
    std::printf("mw3: launcher: games: --appimage-extract said: %s\n", said.c_str());
    return *error = "the AppImage could not be unpacked", false;
  }
  return true;
}
#endif

// A package unpacked into `into` (made, marked as the launcher's): a zip anywhere, a .dmg on the Mac, an
// .AppImage on Linux.
bool unpack(const std::string& file, const fs::path& into, const Progress& progress, std::string* error) {
  std::error_code ec;
  fs::create_directories(into, ec);
  if (!write_text(into / kStagingMark, "being installed by the launcher\n")) return *error = "could not write in " + into.string(), false;
  const std::string name = lower(fs::path(file).filename().string());
  if (ends_with(name, ".zip")) return unzip(file, into, progress, error);
#if defined(__APPLE__)
  if (ends_with(name, ".dmg")) return from_dmg(file, into, error);
#elif !defined(_WIN32)
  if (ends_with(name, ".appimage")) return from_appimage(file, into, error);
#endif
#if defined(_WIN32)
  return *error = "the launcher installs a game's .zip on Windows (not its Setup): " + fs::path(file).filename().string(), false;
#elif defined(__APPLE__)
  return *error = "the launcher installs a game's .dmg on the Mac: " + fs::path(file).filename().string(), false;
#else
  return *error = "the launcher installs a game's .AppImage on Linux: " + fs::path(file).filename().string(), false;
#endif
}

// The unpacked folder put in place of the game's: the old one (the launcher's, by its record) moved aside, the new
// one renamed in, the old removed last; a failure puts the old one back.
bool put_in_place(const std::string& root, u::Game g, const fs::path& staged, std::string* error) {
  std::error_code ec;
  const fs::path to = fs::path(root) / u::game_info(g).key, old = fs::path(root) / ("." + std::string(u::game_info(g).key) + ".old");
  if (!clear_ours(old, u::kInstalledName, error)) return false;
  const bool had = fs::exists(fs::symlink_status(to, ec));
  if (had && (!fs::is_directory(fs::symlink_status(to, ec)) || !fs::exists(to / u::kInstalledName, ec)))
    return *error = to.string() + " is not a game the launcher installed: move it and try again", false;
  if (had) {
    fs::rename(to, old, ec);
    if (ec) return *error = "the installed copy could not be moved aside (" + ec.message() + "): close the game and try again", false;
  }
  fs::rename(staged, to, ec);
  if (ec) {
    const std::string why = ec.message();
    std::error_code back;
    if (had) fs::rename(old, to, back);
    return *error = "the new copy could not be put in place (" + why + ")" + (had ? "; the installed one is kept" : ""), false;
  }
  fs::remove(to / kStagingMark, ec);
  if (had) fs::remove_all(old, ec);  // a failure leaves a stale .old, cleared by the next install
  return true;
}

void make_runnable(const std::string& program) {
#if !defined(_WIN32)
  std::error_code ec;
  if (!program.empty())
    fs::permissions(program, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, ec);
#else
  (void)program;
#endif
}

}  // namespace

std::string root(bool hidden) {
  if (const char* d = std::getenv("MW3_GAMES_DIR")) return d;
  if (hidden) return {};
#if defined(_WIN32)
  const char* base = std::getenv("LOCALAPPDATA");
  if (!base || !*base) return {};
  return (fs::path(base) / "BattleTech" / "games").string();
#elif defined(__APPLE__)
  const char* home = std::getenv("HOME");
  if (!home || !*home) return {};
  return (fs::path(home) / "Library" / "Application Support" / "BattleTech" / "games").string();
#else
  const char* xdg = std::getenv("XDG_DATA_HOME");
  const char* home = std::getenv("HOME");
  const fs::path base = xdg && *xdg ? fs::path(xdg) : home && *home ? fs::path(home) / ".local" / "share" : fs::path();
  if (base.empty()) return {};
  return (base / "BattleTech" / "games").string();
#endif
}

std::string root_moved(const std::string& root, const std::string& cfg) {
  if (root.empty() || std::getenv("MW3_GAMES_DIR")) return root;
  const fs::path base = fs::path(root).parent_path().parent_path();
  const fs::path old = base / rekit::kOldOrg / "BattleTech" / "games";
  std::error_code ec;
  if (!fs::is_directory(old, ec)) return root;
#if defined(MW3_PACKAGE_BUILD)
  const rekit::FolderMove m = rekit::move_once(old.string(), root);
  if (m.kind == rekit::FolderMove::kFailed) {
    std::fprintf(stderr, "mw3: the games folder: %s; %s stays in use\n", m.error.c_str(), old.string().c_str());
    return old.string();
  }
  if (m.kind == rekit::FolderMove::kMoved) {
    rekit::data_folder::retarget_config(cfg, {{old.string(), root}});
    rekit::remove_if_empty(old.parent_path().string(), 2);  // MicroProse/BattleTech, then MicroProse when empty
    std::printf("mw3: the games folder: %s moved to %s\n", old.string().c_str(), root.c_str());
  }
  return root;
#else
  (void)cfg;
  const bool fresh = !fs::exists(root, ec) || (fs::is_directory(root, ec) && fs::is_empty(root, ec));
  return fresh ? old.string() : root;
#endif
}

std::string program_in(const std::string& dir, u::Game g) {
  std::error_code ec;
  if (dir.empty() || !fs::is_directory(dir, ec)) return {};
  const std::string prog = u::game_info(g).program;
#if defined(_WIN32)
  const std::string file = prog + ".exe";
#else
  const std::string file = prog;
#endif
  for (const fs::path& p : {fs::path(dir) / file, fs::path(dir) / "usr" / "bin" / file, fs::path(dir) / "squashfs-root" / "usr" / "bin" / file})
    if (fs::is_regular_file(p, ec)) return p.string();
  for (const auto& e : fs::directory_iterator(dir, ec))
    if (e.path().extension() == ".app") {
      const fs::path p = e.path() / "Contents" / "MacOS" / file;
      if (fs::is_regular_file(p, ec)) return p.string();
    }
  return {};
}

bool installed(const std::string& root, u::Game g, u::InstalledGame* record, std::string* program) {
  if (root.empty()) return false;
  const fs::path dir = fs::path(root) / u::game_info(g).key;
  std::error_code ec;
  if (!fs::is_regular_file(dir / u::kInstalledName, ec)) return false;
  u::InstalledGame rec;
  if (!u::parse_installed(read_text(dir / u::kInstalledName), &rec) || rec.game != u::game_info(g).key) return false;
  const std::string prog = program_in(dir.string(), g);
  if (prog.empty()) return false;
  if (record) *record = rec;
  if (program) *program = prog;
  return true;
}

std::string program_version(const std::string& bin) {
  const std::string tag = std::string("MW3-PROGRAM") + "-VERSION:";  // not whole in this file's own strings
  std::FILE* f = std::fopen(bin.c_str(), "rb");
  if (!f) return {};
  std::string buf, out;
  std::vector<char> chunk(1 << 20);
  for (size_t n; out.empty() && (n = std::fread(chunk.data(), 1, chunk.size(), f)) > 0;) {
    buf.append(chunk.data(), n);
    for (size_t at = buf.find(tag); at != std::string::npos; at = buf.find(tag, at + 1)) {
      const size_t from = at + tag.size(), end = buf.find('\0', from);
      if (end == std::string::npos) break;  // its end in the next chunk
      const std::string v = buf.substr(from, end - from);
      if (!v.empty() && v.size() < 64 && std::all_of(v.begin(), v.end(), [](char c) { return c > ' ' && c < 127; })) {
        out = v;
        break;
      }
    }
    if (buf.size() > 256) buf.erase(0, buf.size() - 256);  // a tag across the chunks' edge
  }
  std::fclose(f);
  return out;
}

u::GameCheck check(u::Game g, const std::string& installed_tag) {
  // Both homes (2026-11-22): git.owenmorgan.com, then GitHub; the newest on either (release_lists).
  const std::vector<u::Answer> answers = ask_lists(release_lists(std::getenv("MW3_GAMES_API"), u::game_info(g).repo, 20), 15, allow_file(),
                                                   std::string("games: ") + u::game_info(g).key);
  return u::judge_game(g, installed_tag, answers, u::this_platform());
}

Done install_release(const std::string& root, u::Game g, const u::Release& r, const Progress& progress, const Meter* meter) {
  Done d;
  d.game = g;
  const std::string name = u::game_info(g).name, version = u::game_version(r.tag);
  std::string error;
  if (!own_root(root, &error)) return d.words = "Nothing was installed: " + error + ".", d;
  const u::Asset* file = u::game_asset(g, r, u::this_platform());
  if (!file) return d.words = "This release of " + name + " has no file for this computer.", d;
  std::error_code ec;
  const fs::path dl_dir = fs::temp_directory_path(ec) / "MW3-Rebuild-games";  // ours alone
  fs::create_directories(dl_dir, ec);
  const std::string path = (dl_dir / file->name).string();
  // GitHub first, git.owenmorgan.com when it fails (download_checked), each against its SHA256SUMS line.
  const Fetched f = download_checked(r, file->name, path,
                                     [&](double x, uint64_t got, bool checking) {
                                       if (meter && checking && meter->ended) meter->ended();
                                       else if (meter && !checking && meter->bytes) meter->bytes(got);
                                       return !progress || progress(x * 0.8, checking ? std::string("Checking the download") : "Downloading " + name + " " + version);
                                     },
                                     allow_file(), "games download");
  if (meter && meter->ended) meter->ended();  // a failure's words, a cancel: past the bytes
  switch (f.why) {
    case Fetched::kOk: break;
    case Fetched::kCancelled: d.cancelled = true; return d.words = "The download was canceled. Nothing was installed.", d;
    case Fetched::kNoSums:
      return d.words = "This release of " + name + " has no SHA256SUMS to check the download against, so nothing was downloaded.", d;
    case Fetched::kSumsFailed: return d.words = "Could not download the release's SHA256SUMS: " + f.error + ".", d;
    case Fetched::kNoLine: return d.words = "The release's SHA256SUMS has no line for " + file->name + ", so nothing was downloaded.", d;
    case Fetched::kStopped: return d.words = "The download stopped: " + f.error + ". Nothing was installed.", d;
    case Fetched::kMismatch:
      return d.words = "The download did not match its checksum, so nothing was installed. Try again; if it happens again, tell us.", d;
    case Fetched::kUnsigned: return d.words = "The release's SHA256SUMS could not be trusted (" + f.error + "), so nothing was downloaded.", d;
  }
  const std::string got = f.sha256;
  const fs::path staged = fs::path(root) / kIncoming;
  if (!clear_ours(staged, kStagingMark, &error)) return fs::remove(path, ec), d.words = "Nothing was installed: " + error + ".", d;
  if (progress) progress(0.85, "Installing " + name + " " + version);
  if (!unpack(path, staged, nullptr, &error)) {
    fs::remove(path, ec);
    std::string e2;
    clear_ours(staged, kStagingMark, &e2);
    return d.words = "Nothing was installed: " + error + ".", d;
  }
  fs::remove(path, ec);
  const std::string inside = program_in(staged.string(), g);
  if (inside.empty()) {
    std::string e2;
    clear_ours(staged, kStagingMark, &e2);
    return d.words = file->name + " does not hold " + name + "'s program, so nothing was installed.", d;
  }
  make_runnable(inside);
  if (!write_text(staged / u::kInstalledName, u::installed_text({u::game_info(g).key, r.tag, file->name, got, "github"})) ||
      !put_in_place(root, g, staged, &error)) {
    std::string e2;
    clear_ours(staged, kStagingMark, &e2);
    return d.words = "Nothing was installed: " + error + ".", d;
  }
  d.ok = true;
  d.program = program_in((fs::path(root) / u::game_info(g).key).string(), g);
  d.words = name + " " + version + " is installed.";
  return d;
}

FileCheck check_file(const std::string& file) {
  FileCheck c;
  c.sha256 = rekit::sha256_file_hex(file);
  std::error_code ec;
  const fs::path sums = fs::path(file).parent_path() / "SHA256SUMS";
  const std::string name = fs::path(file).filename().string();
  if (!fs::is_regular_file(sums, ec)) {
    c.words = "There is no SHA256SUMS beside " + name + ", so it can't be checked.";
    return c;
  }
  const std::string want = u::sums_line(read_text(sums), name);
  if (want.empty()) {
    c.words = "The SHA256SUMS beside " + name + " has no line for it, so it can't be checked.";
    return c;
  }
  c.kind = want == c.sha256 ? FileCheck::kMatches : FileCheck::kMismatch;
  c.words = c.kind == FileCheck::kMatches ? name + " matches its SHA256SUMS."
                                          : name + " does not match its SHA256SUMS: it is damaged or not the file it says. Nothing was installed.";
  return c;
}

Done install_file(const std::string& root, const std::string& file, const std::string& sha256, const Progress& progress) {
  Done d;
  std::string error;
  std::error_code ec;
  const std::string name = fs::path(file).filename().string();
  if (!fs::is_regular_file(file, ec)) return d.words = name + " is not there.", d;
  if (!own_root(root, &error)) return d.words = "Nothing was installed: " + error + ".", d;
  const fs::path staged = fs::path(root) / kIncoming;
  if (!clear_ours(staged, kStagingMark, &error)) return d.words = "Nothing was installed: " + error + ".", d;
  if (progress) progress(0.1, "Unpacking " + name);
  auto fail = [&](const std::string& words) {
    std::string e2;
    clear_ours(staged, kStagingMark, &e2);
    d.words = words;
    return d;
  };
  if (!unpack(file, staged, [&](double x, const std::string& w) { return !progress || progress(0.1 + x * 0.8, w); }, &error))
    return fail("Nothing was installed: " + error + ".");
  // Which game: the one program of ours in it.
  std::vector<u::Game> found;
  for (int i = 0; i < u::game_count(); ++i)  // a fetched catalog's new games too (b7)
    if (!program_in(staged.string(), u::game_at(i)).empty()) found.push_back(u::game_at(i));
  if (found.empty()) return fail(name + " holds none of the games (mw3, mw3pm or mw1), so nothing was installed.");
  if (found.size() > 1) return fail(name + " holds more than one game (the full package): pick one game's own download.");
  const u::Game g = found.front();
  d.game = g;
  const std::string prog = program_in(staged.string(), g);
  make_runnable(prog);
  std::string tag = program_version(prog);
  if (tag.empty()) tag = "unknown";
  if (!write_text(staged / u::kInstalledName, u::installed_text({u::game_info(g).key, tag, name, sha256, "file"})) ||
      !put_in_place(root, g, staged, &error))
    return fail("Nothing was installed: " + error + ".");
  d.ok = true;
  d.program = program_in((fs::path(root) / u::game_info(g).key).string(), g);
  d.words = std::string(u::game_info(g).name) + (tag == "unknown" ? std::string() : " " + u::game_version(tag)) + " is installed from " + name + ".";
  return d;
}

std::string game_folder_in(const std::string& picked, u::Game g) {
  static const char* const kNames[] = {"MechWarrior 3", "Pirate's Moon", "MechWarrior 1 Remake", "MechWarrior 1 Rebuild",
                                       "Inception Rebuild", "Inception Remake"};
  std::error_code ec;
  const fs::path p(picked), mark = p / rekit::kGameFolderMark;
  if (!fs::exists(p, ec) || fs::is_empty(p, ec)) return picked;
  if (fs::is_regular_file(mark, ec) && read_text(mark).find("game=" + std::string(u::game_info(g).key) + "\n") != std::string::npos) return picked;
  return (p / kNames[static_cast<int>(g)]).string();
}

bool claim_game_folder(const std::string& dir, u::Game g, std::string* error) {
  std::error_code ec;
  const fs::path mark = fs::path(dir) / rekit::kGameFolderMark;
  const std::string key = u::game_info(g).key;
  if (fs::is_regular_file(mark, ec))
    return read_text(mark).find("game=" + key + "\n") != std::string::npos ||
           (*error = dir + " is another game's folder: pick another", false);
  if (fs::exists(dir, ec) && !(fs::is_directory(dir, ec) && fs::is_empty(dir, ec)))  // the games folder itself included
    return *error = dir + " holds files that are not this game's: pick an empty folder", false;
  if (!own_root(dir, error)) return false;  // new or empty: the games folder's mark first
  if (!write_text(mark, "# A game's folder, made by the MechWarrior 3 Rebuild launcher or the game's setup: its program and its game files.\ngame=" + key + "\n"))
    return *error = dir + " could not be written to", false;
  return true;
}

bool kit_status(const std::string& bin, rekit::Status* out, const std::vector<std::string>& more, int seconds, std::string* why) {
  std::string said, error;
  std::vector<std::string> args = {bin, "--status"};
  args.insert(args.end(), more.begin(), more.end());
  auto no = [&](const std::string& w) {
    if (why) *why = w;
    return false;
  };
  auto first_line = [&] {  // what it printed, for the log
    std::string l = said.substr(0, said.find_first_of("\r\n"));
    if (l.size() > 160) l = l.substr(0, 160) + "...";
    return l.empty() ? std::string("nothing printed") : l;
  };
  if (bin.empty()) return no("no program");
  Ran ran = Ran::kEnded;
  const int code = run_program(args, seconds, &said, &ran, true);
  if (ran == Ran::kNotStarted) return no(std::string("it did not start (") + SDL_GetError() + ")");
  if (ran == Ran::kTimedOut) return no("no answer in " + std::to_string(seconds) + " s: stopped");
  if (code != 0) return no("it exited " + std::to_string(code) + ": " + first_line());
  const size_t a = said.find('{'), b = said.rfind('}');  // the object alone, should a log line come with it
  if (a == std::string::npos || b == std::string::npos || b <= a) return no("no status in its answer: " + first_line());
  if (!rekit::parse_status(said.substr(a, b - a + 1), out, &error)) return no("its status could not be read (" + error + ")");
  return true;
}

bool kit_setup(const std::string& bin, const std::vector<std::string>& args, int seconds,
               const std::function<bool(const rekit::SetupMessage&)>& progress, rekit::SetupMessage* out,
               const std::function<void(const rekit::SetupMessage&)>& each) {
  *out = {};
  std::vector<const char*> argv{bin.c_str(), "--setup"};
  for (const std::string& a : args) argv.push_back(a.c_str());
  argv.push_back(nullptr);
  SDL_Process* p = bin.empty() ? nullptr : SDL_CreateProcess(argv.data(), true);  // stdin and stdout ours
  if (!p) return out->summary = std::string("It did not start: ") + SDL_GetError() + ".", false;
  SDL_IOStream* io = SDL_GetProcessOutput(p);
  std::string line;
  bool answered = false, stopping = false, timed_out = false;
  Uint64 until = seconds ? SDL_GetTicks() + static_cast<Uint64>(seconds) * 1000 : 0;
  rekit::SetupMessage step;
  int code = 0;
  for (;;) {
    const bool ended = SDL_WaitProcess(p, false, &code);  // before the read: what it wrote last is still read
    size_t got = 0;
    char buf[4096];
    while (io && (got = SDL_ReadIO(io, buf, sizeof buf)) > 0)
      for (size_t i = 0; i < got; ++i) {
        if (buf[i] != '\n') {
          if (line.size() < (1u << 16)) line += buf[i];
          continue;
        }
        rekit::SetupMessage m;
        if (rekit::parse_setup_message(line, &m)) {  // a log line among them is passed over
          if (m.done) *out = m, answered = true;
          else step = m;
          if (!m.done && each) each(m);
        }
        line.clear();
      }
    if (ended) break;
    if (!progress(step) && !stopping) {
      stopping = true, until = SDL_GetTicks() + 30000;  // it stops at its next step and leaves nothing half done
      if (SDL_IOStream* in = SDL_GetProcessInput(p)) SDL_WriteIO(in, "cancel\n", 7), SDL_FlushIO(in);
    }
    if (until && SDL_GetTicks() >= until) {
      timed_out = true;
      SDL_KillProcess(p, true), SDL_WaitProcess(p, true, &code);
      break;
    }
    SDL_Delay(20);
  }
  SDL_DestroyProcess(p);
  if (answered) return true;
  out->cancelled = stopping;
  out->summary = stopping ? "Stopped." : timed_out ? "Its setup did not answer." : "Its setup ended without an answer (" + std::to_string(code) + "): see its log.";
  return false;
}

std::string originals_dir() {
  std::error_code ec;
  return (fs::temp_directory_path(ec) / "MW3-Rebuild-originals").string();
}

Done get_original(const std::string& bin, const std::vector<u::CatalogGame::Original>& sources, const std::vector<std::string>& more,
                  const std::string& folder, const Progress& progress, const Meter* meter) {
  Done d;
  std::error_code ec;
  const fs::path dir = originals_dir();
  if (fs::exists(dir, ec) && !fs::exists(dir / kOriginalMark, ec) && !fs::is_empty(dir, ec))
    return d.words = dir.string() + " is not the launcher's folder, so nothing was downloaded", d;
  fs::create_directories(dir, ec);
  if (!write_text(dir / kOriginalMark, "the launcher's downloads of original game files, removed after each\n"))
    return d.words = "could not write in " + dir.string(), d;
  std::string why;  // each source's reason, in order
  fs::path got;
  for (const u::CatalogGame::Original& s : sources) {
    const fs::path file = dir / s.name;
    std::string error;
    bool cancelled = false;
    const bool fetched = curl_download(s.url, file.string(), s.size,
                                       [&](double x, uint64_t got, bool) {
                                         if (meter && meter->bytes) meter->bytes(got);
                                         return !progress || progress(x * kOriginalSetupFrom, "Getting the original game files");
                                       },
                                       &error, &cancelled, allow_file());
    if (meter && meter->ended) meter->ended();
    if (cancelled) {
      fs::remove(file, ec), fs::remove(file.string() + ".part", ec);
      return d.cancelled = true, d.words = "canceled", d;
    }
    std::string bad;
    if (!fetched) bad = error.empty() ? "it could not be downloaded" : error;
    else if (fs::file_size(file, ec) != s.size) bad = "it is " + std::to_string(fs::file_size(file, ec)) + " bytes, not " + std::to_string(s.size);
    else if (progress && !progress(kOriginalSetupFrom, "Checking the original game files")) {
      fs::remove(file, ec);
      return d.cancelled = true, d.words = "canceled", d;
    } else if (rekit::sha256_file_hex(file.string()) != s.sha256) bad = "it does not match its SHA-256";
    if (bad.empty()) {
      got = file;
      break;
    }
    std::printf("mw3: launcher: original: %s: %s\n", s.url.c_str(), bad.c_str());
    why += (why.empty() ? "" : "; ") + bad;
    fs::remove(file, ec), fs::remove(file.string() + ".part", ec);
  }
  const auto tidy = [&]() {  // ours alone: the file, then the folder once only the mark is left
    if (!got.empty()) fs::remove(got, ec);
    std::size_t left = 0;
    for (const auto& e : fs::directory_iterator(dir, ec)) left += e.path().filename() != kOriginalMark;
    if (left == 0) fs::remove(dir / kOriginalMark, ec), fs::remove(dir, ec);
  };
  if (got.empty()) return tidy(), d.words = why.empty() ? "no source answered" : why, d;
  const auto step = [&](double from, double span) {
    return [&progress, from, span](const rekit::SetupMessage& m) { return !progress || progress(from + span * m.fraction, m.stage); };
  };
  rekit::SetupMessage look, done;
  if (meter && meter->setup) meter->setup();
  std::vector<std::string> args{"--look", got.string()};
  args.insert(args.end(), more.begin(), more.end());
  if (!kit_setup(bin, args, 0, step(kOriginalSetupFrom, 0.1), &look) || !look.ok) {
    d.cancelled = look.cancelled;
    tidy();
    return d.words = "its setup did not take the file" + (look.summary.empty() ? std::string() : ": " + look.summary), d;
  }
  if (look.in_place) return tidy(), d.ok = true, d.words = look.words, d.where = look.where, d;  // the copy in use already: nothing to install
  args = {"--install", got.string()};  // the pick itself, as PICK A FILE gives it
  if (!folder.empty()) args.push_back("--folder"), args.push_back(folder);
  args.insert(args.end(), more.begin(), more.end());
  const bool said = kit_setup(bin, args, 0, step(0.7, 0.3), &done);
  tidy();
  d.cancelled = done.cancelled;
  if (!said || !done.ok) return d.words = "its setup did not install the file" + (done.summary.empty() ? std::string() : ": " + done.summary), d;
  d.ok = true, d.words = done.summary, d.where = done.where;
  return d;
}

bool in_own_mount(const std::string& bin) {
  const char* appdir = std::getenv("APPDIR");
  if (!std::getenv("APPIMAGE")) return false;
  if (!appdir || !*appdir) return true;  // an AppImage that does not say where it is mounted: as before, watched
  const std::string base = appdir;
  return bin.rfind(base, 0) == 0;
}


// ---- Copies installed without the launcher (2026-11-24) ----

// A game's id in its --status (re-setup-kit's contract), where known: a stand-in for a stamp an AppImage hides; the
// launcher's --with names the needed games by it (b5).
const char* kit_id(u::Game g) {
  switch (g) {
    case u::Game::kMw3: return "mech3";
    case u::Game::kPm: return "mech3-pm";
    case u::Game::kMw1: return "mech1-remake";
    case u::Game::kMw1r: return "mech1";
    case u::Game::kChi: return "inception";
    default: return "";
  }
}

namespace {

#if defined(_WIN32)
// Its Setup's uninstall entry (tools/windows/installer.iss: the AppId, the same every version), where known.
const char* setup_id(u::Game g) {
  switch (g) {
    case u::Game::kMw3: return "{920C98A6-550D-41A3-839E-540B93B8F40D}_is1";
    case u::Game::kPm: return "{797310CA-3FB8-4FBB-8D21-47E588714765}_is1";
    case u::Game::kMw1: return "{C33F5AC5-1EFA-4952-81EA-A76A9A48AF28}_is1";  // mech1-remake-re's mw1/tools/package/installer.iss
    case u::Game::kMw1r: return "{56BFB0C4-541E-42DD-909E-874C8EDA0D47}_is1";  // mech1-re's tools/package/installer.iss
    case u::Game::kChi: return "{11AFD380-EF35-4002-81BC-7312A6971E0F}_is1";   // inception-re's
    default: return "";
  }
}
#endif

std::string program_file(u::Game g) {
#if defined(_WIN32)
  return std::string(u::game_info(g).program) + ".exe";
#else
  return u::game_info(g).program;
#endif
}

bool released(u::Game g, const std::string& v) {
  const std::string pre = u::game_info(g).prefix;
  return v.rfind(pre, 0) == 0 && v.size() > pre.size() && std::isdigit(static_cast<unsigned char>(v[pre.size()]));
}

std::string record_value(const std::string& text, const std::string& key) {
  for (size_t at = 0; at < text.size();) {
    const size_t end = std::min(text.find('\n', at), text.size());
    const std::string line = text.substr(at, end - at);
    if (line.rfind(key + "=", 0) == 0) return line.substr(key.size() + 1);
    at = end + 1;
  }
  return {};
}

}  // namespace

// The kit's since 2026-11-25 (rekit/program_record.h): the same file and lines.
static_assert(std::string_view(u::kInstalledName) == rekit::kInstalledMark);
std::string self_path() { return rekit::self_path(); }
bool launchers_own(const std::string& prog) { return rekit::launchers_own(prog); }
void record_self(const std::string& user_folder, u::Game g, const std::string& version) {
  if (!user_folder.empty() && u::game_tag(g, version)) rekit::record_program(user_folder, u::game_info(g).key, version);
}

std::string identify(const std::string& prog, u::Game g, bool may_run) {
  std::error_code ec;
  if (prog.empty() || !fs::is_regular_file(prog, ec)) return {};
  const std::string v = program_version(prog);
  if (!v.empty()) return released(g, v) ? v : std::string();
  if (!may_run) return {};
  rekit::Status s;
  if (!kit_status(prog, &s)) return {};
  const std::string id = kit_id(g);
  const bool ours = !id.empty() ? s.game == id : s.game != "mech3" && s.game != "mech3-pm";
  return ours ? (s.version.empty() ? std::string("unknown") : s.version) : std::string();
}

std::string program_of_pick(const std::string& pick, u::Game g) {
  std::error_code ec;
  if (pick.empty()) return {};
  const fs::path p(pick);
  const std::string file = program_file(g);
  if (fs::is_directory(p, ec)) {
    if (lower(p.extension().string()) == ".app") {
      const fs::path in = p / "Contents" / "MacOS" / file;
      return fs::is_regular_file(in, ec) ? in.string() : std::string();
    }
    std::string in = program_in(pick, g);
    if (in.empty()) in = program_in((p / u::game_info(g).key).string(), g);  // a launcher-style games folder
    return in;
  }
  if (!fs::is_regular_file(p, ec)) return {};
  const std::string name = lower(p.filename().string());
  if (name == lower(file) || ends_with(name, ".appimage")) return pick;
  return program_in(p.parent_path().string(), g);  // another file in its folder
}

std::vector<Found> found_elsewhere(u::Game g, const std::string& user_folder, bool system) {
  std::vector<Found> out;
  auto take = [&](const std::string& prog, const char* how, bool may_run) {
    for (const Found& f : out)
      if (f.program == prog) return;
    if (launchers_own(prog)) return;
    const std::string v = identify(prog, g, may_run);
    // Released builds only: a development one (v1.3.1-29-g...) is linked by hand or not at all.
    if (u::game_tag(g, v) || u::game_tag(g, u::game_info(g).prefix + v)) out.push_back({prog, v, how});
  };
  if (!user_folder.empty()) {  // the program's own record
    const std::string text = read_text(fs::path(user_folder) / kProgramRecord);
    const std::string prog = record_value(text, "program");
    if (record_value(text, "game") == u::game_info(g).key && !prog.empty())
      take(prog, "its record", ends_with(lower(prog), ".appimage"));
  }
#if defined(_WIN32)
  if (system && *setup_id(g)) {
    const std::string key = std::string("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") + setup_id(g);
    for (HKEY hive : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER})
      for (DWORD view : {static_cast<DWORD>(KEY_WOW64_64KEY), static_cast<DWORD>(KEY_WOW64_32KEY)}) {
        char buf[2048];
        DWORD size = sizeof buf;
        if (RegGetValueA(hive, key.c_str(), "InstallLocation", RRF_RT_REG_SZ | view, nullptr, buf, &size) == ERROR_SUCCESS)
          take(program_in(buf, g), "its Setup", false);
      }
  }
#elif defined(__APPLE__)
  std::vector<fs::path> apps;
  if (system) apps.push_back("/Applications");
  if (const char* home = std::getenv("HOME"); system && home && *home) apps.push_back(fs::path(home) / "Applications");
  std::error_code ec;
  for (const fs::path& dir : apps)
    for (const auto& e : fs::directory_iterator(dir, ec))
      if (e.path().extension() == ".app") {
        const fs::path prog = e.path() / "Contents" / "MacOS" / program_file(g);
        if (fs::is_regular_file(prog, ec)) take(prog.string(), "Applications", !*kit_id(g));
      }
#else
  (void)system;
#endif
  std::stable_sort(out.begin(), out.end(), [](const Found& a, const Found& b) { return u::compare_game(a.version, b.version) > 0; });
  return out;
}

bool uninstall(const std::string& root, u::Game g, std::string* error) {
  std::error_code ec;
  const fs::path dir = fs::path(root) / u::game_info(g).key, old = fs::path(root) / ("." + std::string(u::game_info(g).key) + ".old");
  u::InstalledGame rec;
  if (root.empty() || !fs::is_regular_file(dir / u::kInstalledName, ec) || !u::parse_installed(read_text(dir / u::kInstalledName), &rec) ||
      rec.game != u::game_info(g).key)
    return *error = dir.string() + " is not a game the launcher installed", false;
  if (!clear_ours(old, u::kInstalledName, error)) return false;
  fs::rename(dir, old, ec);  // aside first: a running game keeps its folder whole
  if (ec) return *error = "it could not be removed (" + ec.message() + "): close the game and try again", false;
  fs::remove_all(old, ec);  // a failure leaves .old, cleared by the next install or uninstall
  return true;
}

// ---- A linked copy uninstalled (2026-11-24) ----

namespace {

#if defined(__APPLE__)
// The system's trash, as dragging it there (NSFileManager trashItemAtURL:).
bool to_trash(const std::string& path, std::string* error) {
  auto cls = [](const char* n) { return reinterpret_cast<id>(objc_getClass(n)); };
  auto sel = [](const char* n) { return sel_registerName(n); };
  auto msg = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend);
  auto msg_s = reinterpret_cast<id (*)(id, SEL, const char*)>(objc_msgSend);
  auto msg_o = reinterpret_cast<id (*)(id, SEL, id)>(objc_msgSend);
  auto trash = reinterpret_cast<BOOL (*)(id, SEL, id, id*, id*)>(objc_msgSend);
  auto utf8 = reinterpret_cast<const char* (*)(id, SEL)>(objc_msgSend);
  id pool = msg(msg(cls("NSAutoreleasePool"), sel("alloc")), sel("init"));
  id url = msg_o(cls("NSURL"), sel("fileURLWithPath:"), msg_s(cls("NSString"), sel("stringWithUTF8String:"), path.c_str()));
  id err = nullptr;
  const bool ok = trash(msg(cls("NSFileManager"), sel("defaultManager")), sel("trashItemAtURL:resultingItemURL:error:"), url, nullptr, &err);
  if (!ok) {
    const char* w = err ? utf8(msg(err, sel("localizedDescription")), sel("UTF8String")) : nullptr;
    *error = w ? w : "the system would not move it to the trash";
  }
  msg(pool, sel("drain"));
  return ok;
}
#elif !defined(_WIN32)
// The desktop's trash (the freedesktop trash spec's home trash), on the same disk; else gio's, which knows the others.
bool to_trash(const std::string& path, std::string* error) {
  std::error_code ec;
  const char *data = std::getenv("XDG_DATA_HOME"), *home = std::getenv("HOME");
  if (data && *data ? false : !home || !*home) return *error = "there is no home folder", false;
  const fs::path trash = (data && *data ? fs::path(data) : fs::path(home) / ".local/share") / "Trash";
  fs::create_directories(trash / "files", ec), fs::create_directories(trash / "info", ec);
  const std::string base = fs::path(path).filename().string();
  std::string name = base;
  for (int n = 2; fs::exists(trash / "files" / name, ec) || fs::exists(trash / "info" / (name + ".trashinfo"), ec); ++n) name = base + "." + std::to_string(n);
  std::string enc;
  for (const unsigned char c : path) {
    char b[4];
    if (std::isalnum(c) || std::strchr("/-_.~", c)) enc += static_cast<char>(c);
    else std::snprintf(b, sizeof b, "%%%02X", c), enc += b;
  }
  const std::time_t now = std::time(nullptr);
  char when[32] = "";
  std::strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
  if (write_text(trash / "info" / (name + ".trashinfo"), "[Trash Info]\nPath=" + enc + "\nDeletionDate=" + when + "\n")) {
    fs::rename(path, trash / "files" / name, ec);
    if (!ec) return true;
    std::error_code ig;
    fs::remove(trash / "info" / (name + ".trashinfo"), ig);
  }
  const char* args[] = {"gio", "trash", path.c_str(), nullptr};
  int code = -1;
  if (SDL_Process* p = SDL_CreateProcess(args, false)) SDL_WaitProcess(p, true, &code), SDL_DestroyProcess(p);
  return code == 0 || (*error = "it could not be moved to the trash", false);
}
#endif

}  // namespace

Removal removal_of(const std::string& prog, u::Game g) {
  Removal r;
  std::error_code ec;
  if (prog.empty() || launchers_own(prog) || !fs::is_regular_file(prog, ec)) return r;
  const fs::path p = fs::weakly_canonical(prog, ec), dir = p.parent_path();
  fs::path whole;  // what would go
  if (ends_with(lower(p.string()), ".appimage")) {
    whole = p, r.how = Removal::kTrash;
  } else if (dir.filename() == "MacOS" && dir.parent_path().filename() == "Contents" && lower(dir.parent_path().parent_path().extension().string()) == ".app") {
    whole = dir.parent_path().parent_path(), r.how = Removal::kTrash;
  } else {
    fs::path un;
    for (const auto& e : fs::directory_iterator(dir, ec)) {  // Inno Setup's: unins000.exe beside its unins000.dat
      const std::string n = lower(e.path().filename().string());
      fs::path dat = e.path();
      if (n.rfind("unins", 0) == 0 && ends_with(n, ".exe") && fs::is_regular_file(dat.replace_extension(".dat"), ec) && e.path() > un) un = e.path();
    }
    if (un.empty()) return r.why = "It is in a folder of its own: delete that yourself", r;
    whole = dir, r.how = Removal::kSetup, r.path = un.string();
  }
  if (r.how == Removal::kTrash) r.path = whole.string();
  // A package of more than this game (an older launcher's), or this launcher's own: not from here.
  if (whole != p)
    for (int i = 0; i < u::game_count(); ++i)
      if (const u::Game o = u::game_at(i); o != g && fs::is_regular_file(dir / program_file(o), ec)) return {Removal::kNone, "", std::string("It is in one package with ") + u::game_info(o).name};
  const std::string self = self_path();
  if (!self.empty()) {
    const std::string at = fs::weakly_canonical(self, ec).string(), in = whole.string();
    if (at == in || at.rfind(in + static_cast<char>(fs::path::preferred_separator), 0) == 0) return {Removal::kNone, "", "It is in one package with this launcher"};
  }
  return r;
}

bool remove_linked(const std::string& prog, u::Game g, const Removal& r, std::string* error) {
  std::error_code ec;
  const Removal now = removal_of(prog, g);  // asked again at the removal
  if (now.how == Removal::kNone || now.how != r.how || now.path != r.path) return *error = "it is not where it was", false;
  const char* t = std::getenv("MW3_LAUNCHER_TRASH");  // checks
  if (t && now.how == Removal::kTrash) {
    fs::rename(now.path, fs::path(t) / fs::path(now.path).filename(), ec);
  } else if (now.how == Removal::kTrash) {
#if defined(_WIN32)
    return *error = "it is not a Windows program", false;
#else
    if (!to_trash(now.path, error)) return false;
#endif
  } else {
#if defined(_WIN32)
    // Asked already, so silent; Windows asks for an administrator itself where its Setup did.
    const std::wstring exe = fs::path(now.path).wstring(), cwd = fs::temp_directory_path(ec).wstring();
    SHELLEXECUTEINFOW x{};
    x.cbSize = sizeof x, x.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC, x.lpVerb = L"open", x.lpFile = exe.c_str();
    x.lpParameters = L"/SILENT /NORESTART", x.lpDirectory = cwd.c_str(), x.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&x)) return *error = GetLastError() == ERROR_CANCELLED ? "Windows was not allowed to" : "its uninstaller did not start", false;
    if (x.hProcess) WaitForSingleObject(x.hProcess, 300000), CloseHandle(x.hProcess);
    for (int i = 0; i < 300 && fs::exists(prog, ec); ++i) SDL_Delay(100);  // Inno's works on from a copy of itself
#else
    return *error = "its uninstaller is for Windows", false;
#endif
  }
  if (fs::exists(prog, ec)) return *error = "it is still there: close the game and try again", false;
  return true;
}

}  // namespace sdlfe::games
