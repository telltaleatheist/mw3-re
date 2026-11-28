// The launcher's tests (docs/launcher.md): its screens, the update check and games on demand, each game a
// stand-in script answering as the game would (docs/launcher-split.md, The tests that start mw3 beside the launcher).
#include <sstream>
#include "frontend/sdl/launcher_job.h"
#include "frontend/sdl/launcher_settings.h"
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <cctype>
#include <cmath>
#include <chrono>
#include <csignal>
#include <thread>
#include <mutex>
#if !defined(_WIN32)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#include <cstdlib>
namespace {
int setenv(const char* name, const char* value, int) { return _putenv_s(name, value); }
int unsetenv(const char* name) { return _putenv_s(name, ""); }
}  // namespace
#endif
#include <set>
#include <string>
#include <vector>

#include "check.h"
#include "rekit/hash.h"
#include "test_keys.h"
#if MW3_HAVE_LIBARCHIVE
#if defined(_WIN32)  // archive_entry.h brings in <windows.h>: none of its macros (max, near, small) past here
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#include <archive.h>
#include <archive_entry.h>
#if defined(_WIN32)
#undef near
#undef far
#undef small
#endif
#endif
#include "frontend/sdl/sdl_games.h"
#include "frontend/sdl/sdl_catalog.h"
#include "upgrades/update/sources.h"
#include "upgrades/update/whats_new.h"
#include <archive.h>
#include <archive_entry.h>
#include "frontend/sdl/sdl_update.h"
#include "frontend/sdl/font_search.h"
#include "frontend/sdl/launcher.h"
#include "frontend/sdl/vulkan_check.h"
#include "frontend/sdl/log_start.h"
#include "frontend/sdl/text_fit.h"
#include <SDL3_ttf/SDL_ttf.h>
#include "frontend/sdl/launcher_own.h"
#include "rekit/status.h"
#include "frontend/sdl/system_pickers.h"
#include "platform/pointer_hands_off.h"

#include <cstdio>
#include <map>





#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
extern char** environ;




#endif



// No system picker opens in a run no player is at (system_pickers.h): every ask answered at once as cancelled
// (SDL's: a list whose first is null), the opener never reached; in a player's run the opener is. SDL's own opener is
// swapped for a stand-in first, so this test can never put a picker on the screen, whatever the guard does.
TEST(sdl_no_picker_opens_in_a_hands_off_run) {
  auto& opener = sdlfe::picker_opener();
  const auto real = opener;
  int opened = 0;
  opener = [&](const sdlfe::PickerAsk&) { ++opened; };
  struct Got {
    int calls = 0;
    bool cancelled = false;
  };
  auto done = [](void* user, const char* const* list, int) {
    auto* g = static_cast<Got*>(user);
    ++g->calls;
    g->cancelled = list && !list[0];
  };
  const bool hands_off = platform::pointer_hands_off();
  platform::set_pointer_hands_off(true);
  Got g;
  int refused = 0;
  for (sdlfe::PickerKind k : {sdlfe::PickerKind::kOpenFile, sdlfe::PickerKind::kOpenFolder, sdlfe::PickerKind::kSaveFile})
    refused += !sdlfe::open_picker({k, done, &g});
  const int opened_off = opened;
  platform::set_pointer_hands_off(false);
  Got p;
  const bool asked = sdlfe::open_picker({sdlfe::PickerKind::kOpenFile, done, &p});
  platform::set_pointer_hands_off(hands_off);
  opener = real;
  std::printf("  hands-off: %d of 3 refused, %d answered cancelled, %d opened; a player's: opened %d\n", refused, g.calls,
              opened_off, opened - opened_off);
  EXPECT(refused == 3 && g.calls == 3 && g.cancelled && opened_off == 0);
  EXPECT(asked && opened == 1 && p.calls == 0);
}




// On every system: the signed release lists' tests below run on Windows too.
namespace {
// Signed fixtures (test keys only, test_keys.h): <file>.sig by the test keys `seeds`, one line each.
void sign_fixture(const std::string& file, std::initializer_list<uint8_t> seeds) {
  std::ifstream in(file, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::string sig;
  for (uint8_t s : seeds) sig += test_keys::sign(text, s);
  std::ofstream(file + ".sig", std::ios::binary) << sig;
}
// A sources.json (docs/launcher.md, Where the launcher looks) at `file`: the catalog's addresses and the test keys.
void write_sources(const std::string& file, const std::vector<std::string>& catalog, std::initializer_list<uint8_t> seeds) {
  std::string c, k;
  for (const std::string& s : catalog) c += (c.empty() ? "\"" : ", \"") + s + "\"";
  for (uint8_t s : seeds) k += (k.empty() ? "\"" : ", \"") + test_keys::public_key(s) + "\"";
  std::ofstream(file) << "{\"sources\": 1, \"catalog\": [" << c << "], \"keys\": [" << k << "]}\n";
}
}  // namespace

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
// No screen goes black unseen (after item 21's lab, resolution.md section 9):
// the program itself, one run a screen, all at once, each shot through the
// capture path the shots use (--shot). Each shot's share of lit pixels must be
// over a floor, and where a screen has a known colour (the lab's and the
// debriefing's green), that share too. About 20 s.
namespace {
struct ScreenShot {
  const char* name;
  std::vector<std::string> args;
  bool vmode13 = false;
  double lit_floor = 0.3, green_floor = 0.0;
  int width = 1280, height = 960;
};
pid_t spawn_bin(const char* bin, const std::vector<std::string>& args, const std::string& log) {
  std::vector<std::string> a = {bin};
  a.insert(a.end(), args.begin(), args.end());
  std::vector<char*> argv;
  for (std::string& s : a) argv.push_back(s.data());
  argv.push_back(nullptr);
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_addopen(&fa, 1, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  posix_spawn_file_actions_adddup2(&fa, 1, 2);
  pid_t pid = 0;
  const int err = posix_spawn(&pid, bin, &fa, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&fa);
  return err == 0 ? pid : -1;
}
// A stand-in mw3's first line: the launcher's asks of MechWarrior 3, answered as mw3 answers them (docs/launcher.md,
// The setup kit's contract): --status a ready copy in <write dir>/install with 1.2 data, lacking music (and saying so)
// until the write dir has a track; --setup --describe the find screen's words; any other setup verb refused. So the
// stand-in's own steps (its log, its args.txt) are only a start's. Its last line is our program version
// (kVersionLine), without which the launcher never runs a program for its status.
const std::string kAsksAsMw3 = R"sh(case "$1" in --status|--setup)
  W=; P=; for A in "$@"; do [ "$P" = --write-dir ] && W=$A; P=$A; done
  case "$1 $2" in
  '--status '*) L=; N=
    if [ ! -e "$W/music/track02.wav" ] && [ ! -e "$W/music/track02.ogg" ]; then L='"music"'
      N="\"No music yet: the original played it from the CD's audio tracks, which an install does not have. ADD FROM MY DISC takes it from your disc image (a .cue and its .bin, also in a .zip), or put your own rip of the music in $W/music as track02.ogg, track03.ogg and so on.\""; fi
    echo "{\"kit\": 1, \"game\": \"mech3\", \"version\": \"v0.0.0-test\", \"files\": {\"state\": \"ready\", \"where\": \"$W/install\", \"data\": \"1.2\", \"lacks\": [$L]}, \"first_run\": {\"pending\": []}, \"needs\": [], \"setup\": 1, \"notes\": [$N], \"user\": \"$W\", \"play\": 1}";;
  '--setup --describe') echo '{"done": true, "ok": true, "into": "'"$W"'/install", "title": "MechWarrior 3", "pick_words": "a disc image, a download, or Mech3.exe", "find_sub": "A disc image, a download (.zip .iso), or Mech3.exe in an installed copy", "find_words": "MechWarrior 3 needs your own copy of the game. Show us your disc, a disc image or download (.iso, .cue, .zip), or a folder where it'"'"'s installed. We copy what the game needs into your user folder; your own copy is left untouched."}';;
  *) echo '{"done": true, "ok": false, "error": "the stand-in answers --status and --setup --describe only"}';;
  esac
  exit 0;;
esac
)sh";
const std::string kVersionLine = std::string("# MW3-PROGRAM") + "-VERSION:v0.0.0-test" + std::string(1, '\0') + "\n";
// mw3launcher, the launcher's own program (docs/launcher.md, The two programs).
// A stale mw3pm beside the build's launcher would make PIRATE'S MOON a live
// entry and move every key walk: none, unless a test names its stand-in.
// A check run asking MW3_CATALOG_URL's catalogs (tests that are not about signing) has them signed by test key 1
// and a sources file naming that key, unless the test gives its own MW3_SOURCES_FILE.
pid_t spawn_launcher(const std::vector<std::string>& args, const std::string& log) {
  const bool named = std::getenv("MW3_PM_BIN") != nullptr;
  if (!named) setenv("MW3_PM_BIN", "", 1);
  const char* cat = std::getenv("MW3_CATALOG_URL");
  const bool sources = cat && !std::getenv("MW3_SOURCES_FILE");
  if (sources) {
    for (std::string s = cat; !s.empty();) {
      const size_t bar = s.find('|');
      const std::string url = s.substr(0, bar);
      s = bar == std::string::npos ? std::string() : s.substr(bar + 1);
      if (url.rfind("file://", 0) == 0 && std::filesystem::is_regular_file(url.substr(7))) sign_fixture(url.substr(7), {1});
    }
    const std::string file = log + ".sources.json";
    write_sources(file, {"file:///nowhere/catalog.json"}, {1});
    setenv("MW3_SOURCES_FILE", file.c_str(), 1);
  }
  const pid_t pid = spawn_bin(MW3_LAUNCHER_BIN, args, log);
  if (sources) unsetenv("MW3_SOURCES_FILE");
  if (!named) unsetenv("MW3_PM_BIN");
  return pid;
}
// waitpid with a limit: a launcher that never quits is killed and fails its
// test, where a bare wait would hang the whole run.
bool waited(pid_t pid, int* status, int seconds = 120) {
  if (pid <= 0) return false;
  for (int i = 0; i < seconds * 20; ++i) {
    const pid_t got = waitpid(pid, status, WNOHANG);
    if (got == pid) return true;
    if (got < 0) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  std::printf("the launcher ran past %d s: killed\n", seconds);
  kill(pid, SIGKILL);
  waitpid(pid, status, 0);
  return false;
}
}  // namespace



#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)


#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid
#endif

#if !defined(_WIN32)  // posix_spawn, waitpid, kill
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
#endif

#if !defined(_WIN32)
// The launcher's first-start notes (lead's launcher list, item 5): the
// data's version and the music, in the launcher's words (SETUP's buttons);
// nothing to say for 1.2 data with music. 1.0's data and a mix need no note
// either: they load as 1.2 (docs/patch12.md); only data of an unknown
// version is told about the patch. The music is any CD track's file.
// The launcher's window is 16:9 unless --window gave a size (2026-11-10):
// the game's own 1280x960 default is not handed to it.
TEST(launcher_window_is_16_9_by_default) {
  sdlfe::LauncherOptions lo;
  EXPECT(lo.win_w == 1280 && lo.win_h == 720);
  lo.window_from(false, 1280, 960);  // the program's default, no --window
  EXPECT(lo.win_w * 9 == lo.win_h * 16 && lo.win_w == 1280 && lo.win_h == 720);
  lo.window_from(true, 800, 600);  // --window 800x600
  EXPECT(lo.win_w == 800 && lo.win_h == 600);
  lo.window_from(true, 0, 0);  // a size that did not parse: kept
  EXPECT(lo.win_w == 800 && lo.win_h == 600);
}


// The launcher's MW3 line with a long install folder (lead, after the PC's
// shot: it ran past the button's sides): shortened from the middle to the
// width, "Ready, official: data 1.2, program v1.1.0, " kept whole, the folder's end kept longer.
TEST(launcher_long_path_fits_the_button) {
  auto chars = [](const std::string& t) { return static_cast<int>(t.size()); };  // a width per character
  const std::string ready = "Ready, official: data 1.2, program v1.1.0, ";
  const std::string path = "C:\\Users\\SomebodyWithALongName\\Documents\\My Games\\MicroProse\\game\\mw3-1.2";
  const std::string fit = sdlfe::fit_middle(ready + path, ready.size(), 70, chars);  // about the plate's 500 px at 12 px
  std::printf("  %s\n", fit.c_str());
  EXPECT(static_cast<int>(fit.size()) <= 70 && fit.rfind(ready, 0) == 0 && fit.find("...") != std::string::npos);
  EXPECT(fit.size() > 64 && fit.compare(fit.size() - 7, 7, "mw3-1.2") == 0);  // its name, and as much as fits
  EXPECT(sdlfe::fit_middle("short", 0, 50, chars) == "short");
  EXPECT(sdlfe::fit_middle(ready + path, ready.size(), 10, chars) == ready + "...");  // too narrow: the words alone
}


// The launcher's narrow spaces (lead, setup.png: "No MechWarrior 3was
// found"): DIN Condensed's space is 2 pixels at 13 and its digits draw a
// pixel past their advance, so the space after a digit vanished. A space
// under a fifth of the size is doubled; with the Mac's font, the blank gap
// after "3" is then as wide as the font's own between "a" and "g".
// An Arch install's launcher had none of the fixed paths: empty buttons. The
// package's font is drawn from; without it, a walk finds what the system has.
TEST(launcher_font_search_finds_any_system_face) {
  const std::string root = std::string(MW3_TEST_TMP) + "/font_search";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root + "/noto/deep");
  std::filesystem::create_directories(root + "/misc");
  auto touch = [&](const char* f) { std::ofstream(root + f) << "x"; };
  for (const char* f : {"/noto/NotoColorEmoji.ttf", "/noto/NotoSans-Italic.ttf", "/noto/deep/NotoSans-Regular.ttf", "/misc/readme.txt",
                        "/misc/AAA-Symbols-Bold.ttf"})
    touch(f);
  const std::vector<std::string> files = sdlfe::scan_fonts({root, root + "/absent"});
  EXPECT(files.size() == 4);
  EXPECT(sdlfe::pick_font(files, {"Liberation.ttf"}) == root + "/noto/deep/NotoSans-Regular.ttf");  // upright text
  touch("/noto/deep/NotoSans-Bold.ttf");
  const std::vector<std::string> more = sdlfe::scan_fonts({root});
  EXPECT(sdlfe::pick_font(more, {"Liberation.ttf"}) == root + "/noto/deep/NotoSans-Bold.ttf");  // bold before upright
  EXPECT(sdlfe::pick_font(more, {"Liberation.ttf", "notosans-regular.TTF"}) == root + "/noto/deep/NotoSans-Regular.ttf");  // named first
  EXPECT(sdlfe::pick_font({root + "/noto/NotoColorEmoji.ttf"}, {}).empty());
  EXPECT(sdlfe::pick_font({}, {"x.ttf"}).empty());
  // A script's own face has no letters of ours: passed over (NotoSansArabic-Bold sorts first).
  const std::vector<std::string> faces = {"/f/NotoSansArabic-Bold.ttf", "/f/NotoSansArabic-Regular.ttf", "/f/Noto-Regular.ttf", "/f/Text-Bold.ttf"};
  auto latin = [](const std::string& f) { return f.find("Arabic") == std::string::npos; };
  EXPECT(sdlfe::pick_font(faces, {}) == "/f/NotoSansArabic-Bold.ttf");
  EXPECT(sdlfe::pick_font(faces, {}, latin) == "/f/Text-Bold.ttf");
  EXPECT(sdlfe::has_latin(std::string(MW3_ASSETS_FOR_TESTS) + "/launcher/font.ttf"));
  EXPECT(!sdlfe::has_latin(root + "/noto/NotoColorEmoji.ttf"));  // not a font at all
  EXPECT(!sdlfe::font_folders().empty() && sdlfe::font_folders().back() == "/usr/share/fonts");
  std::filesystem::remove_all(root);
}

TEST(launcher_narrow_spaces_widen) {
  EXPECT(sdlfe::widen_spaces("MechWarrior 3 was", 2, 13) == "MechWarrior  3  was");
  EXPECT(sdlfe::widen_spaces("MechWarrior 3 was", 3, 13) == "MechWarrior 3 was");  // wide enough: as is
  EXPECT(sdlfe::widen_spaces("none", 1, 13) == "none");
  const char* din = "/System/Library/Fonts/Supplemental/DIN Condensed Bold.ttf";
  if (!std::filesystem::exists(din) || !TTF_Init()) return;
  TTF_Font* f = TTF_OpenFont(din, 13.0f);
  EXPECT(f != nullptr);
  if (!f) return;
  auto gap = [&](const std::string& s) {  // blank columns between the first two inked runs
    SDL_Surface* t = TTF_RenderText_Blended(f, s.c_str(), 0, SDL_Color{255, 255, 255, 255});
    SDL_Surface* c = t ? SDL_ConvertSurface(t, SDL_PIXELFORMAT_ARGB8888) : nullptr;
    int blank = 0, best = -1;
    bool run = false;
    for (int x = 0; c && x < c->w && best < 0; ++x) {
      bool ink = false;
      for (int y = 0; y < c->h; ++y) ink |= (static_cast<const uint32_t*>(c->pixels)[y * (c->pitch / 4) + x] >> 24) > 32;
      if (ink && run && blank) best = blank;
      else if (ink) run = true, blank = 0;
      else if (run) ++blank;
    }
    if (c) SDL_DestroySurface(c);
    if (t) SDL_DestroySurface(t);
    return best;
  };
  int adv = 0;
  TTF_GetGlyphMetrics(f, ' ', nullptr, nullptr, nullptr, nullptr, &adv);
  const int before = gap("3 w"), after = gap(sdlfe::widen_spaces("3 w", adv, 13)), word = gap("a g");
  std::printf("  DIN 13: space %d px; the gap after 3 %d, widened %d, a to g %d\n", adv, before, after, word);
  EXPECT(adv * 5 < 13 && after > before && after >= word - 1);
  TTF_CloseFont(f);
  TTF_Quit();
}

// The launcher (ours, docs/launcher.md), drawn once into a BMP: with a stand-in
// MechWarrior 3 found (kAsksAsMw3: ready, 1.2 data, no music), the games' screen;
// with none (MW3_FIND_INSTALL=1, an empty write dir), the setup screen; the
// progress screen. Each a lit 854x480 canvas (16:9), and the log says what it
// found. The games folder is the test's own, empty (2026-11-27: the computer's
// installed games, read before, made it pass or fail by what was installed).
TEST(sdl_program_launcher_screens) {
  const std::string dir = std::string(MW3_TEST_TMP) + "/launcher";
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir + "/wd", ec);
  std::filesystem::create_directories(dir + "/wd2", ec);
  std::filesystem::create_directories(dir + "/games", ec);
  std::filesystem::create_directories(dir + "/wd/install", ec);  // the stand-in's install (its status's "where")
  {
    std::ofstream sh(dir + "/mw3");
    sh << "#!/bin/sh\n" << kAsksAsMw3 << "exit 0\n" << kVersionLine;
  }
  std::filesystem::permissions(dir + "/mw3", std::filesystem::perms::owner_all, ec);
  setenv("MW3_GAME_BIN", (dir + "/mw3").c_str(), 1);
  setenv("MW3_GAMES_DIR", (dir + "/games").c_str(), 1);
  for (const char* other : {"MW3_MW1_BIN", "MW3_MW1R_BIN", "MW3_CHI_BIN", "MW3_CHIRM_BIN"}) setenv(other, "", 1);
  // The first entry hovered (the PC's fresh install): a square plate lights its edge (at edge_x, edge_y,
  // the plate's top, the 640 block 107 in) in amber. The main screen's panels (2026-11-24): MW3's PLAY, at the
  // right of the first, lights too, no ripple.
  struct Shot {
    const char* screen;
    bool none;
    const char* says;
    const char* hover;
    int edge_x, edge_y;
  };
  for (const Shot& s : {Shot{"main", false, "screen main: MECHWARRIOR 3, MECHWARRIOR 3 SETTINGS, PIRATE'S MOON, PIRATE'S MOON SETTINGS, MECHWARRIOR 1 ORIGINAL, MECHWARRIOR 1 ORIGINAL SETTINGS, MECHWARRIOR 1, MECHWARRIOR 1 SETTINGS, INCEPTION ORIGINAL, INCEPTION ORIGINAL SETTINGS, CHECK FOR UPDATES, QUIT\n", "hover MECHWARRIOR 3: edge\n", 712, 110},
                        Shot{"setup", true, "screen find: FIND THE GAME, PICK A FILE, PICK A FOLDER, BACK\n", "hover FIND THE GAME: edge\n", 427, 222},
                        Shot{"pick", true, "screen find: FIND THE GAME, PICK A FILE, PICK A FOLDER, BACK\n", "hover FIND THE GAME: edge\n", 427, 222},
                        Shot{"work", false, "launcher: Found MechWarrior 3", "hover CANCEL: edge\n", 427, 372},
                        Shot{"notes", false, "screen notes: ADD FROM MY DISC, OK\n", "hover ADD FROM MY DISC: edge\n", 427, 318},
                        Shot{"disc", false, "screen notes: ADD FROM MY DISC, OK\n", "hover ADD FROM MY DISC: edge\n", 427, 318}}) {
    const std::string bmp = dir + "/" + s.screen + ".bmp", log = dir + "/" + s.screen + ".log";
    if (s.none) setenv("MW3_FIND_INSTALL", "1", 1);
    const pid_t pid = spawn_launcher({"--write-dir", dir + (s.none ? "/wd2" : "/wd"), "--launcher-shot", bmp, "--launcher-screen", s.screen}, log);
    if (s.none) unsetenv("MW3_FIND_INSTALL");
    int status = 0;
    EXPECT(waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    std::ifstream f(log);
    const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT(text.find("/launcher/font.ttf\n") != std::string::npos);  // the package's font, no system one asked
    EXPECT(text.find("BattleTech/games/") == std::string::npos);  // the computer's own installed games never read
    SDL_Surface* img = SDL_LoadBMP(bmp.c_str());
    int lit = 0, n = 0;
    bool edge = s.edge_x < 0;
    if (img) {
      SDL_Surface* c = SDL_ConvertSurface(img, SDL_PIXELFORMAT_ARGB8888);
      if (c && s.edge_x >= 0) {
        const uint32_t p = static_cast<const uint32_t*>(c->pixels)[s.edge_y * (c->pitch / 4) + s.edge_x];
        edge = (p >> 16 & 0xff) > 230 && (p >> 8 & 0xff) > 150 && (p >> 8 & 0xff) < 200 && (p & 0xff) < 100;  // amber
      }
      for (int y = 0; c && y < c->h; y += 7)
        for (int x = 0; x < c->w; x += 7) {
          const uint32_t p = static_cast<const uint32_t*>(c->pixels)[y * (c->pitch / 4) + x];
          ++n, lit += std::max({p >> 16 & 0xff, p >> 8 & 0xff, p & 0xff}) > 40;
        }
      std::printf("launcher %s: %dx%d, %d of %d lit, says \"%s\" %d\n", s.screen, img->w, img->h, lit, n, s.says,
                  text.find(s.says) != std::string::npos);
      if (c) SDL_DestroySurface(c);
    }
    EXPECT(img && img->w == 854 && img->h == 480 && lit > n / 5);  // 16:9 (2026-11-08): more dark sky, 24% lit at least
    EXPECT(text.find(s.says) != std::string::npos);
    EXPECT(text.find(s.hover) != std::string::npos && edge);
    // The setup screens name the game they set up, not the launcher (2026-11-22: the kit names no game); the cards keep its title.
    EXPECT((text.find("launcher: header MECHWARRIOR 3\n") != std::string::npos) == (std::string(s.screen) != "main"));
    if (img) SDL_DestroySurface(img);
  }
  for (const char* v : {"MW3_GAME_BIN", "MW3_GAMES_DIR", "MW3_MW1_BIN", "MW3_MW1R_BIN", "MW3_CHI_BIN", "MW3_CHIRM_BIN"}) unsetenv(v);
}

// The mouse on the launcher (2026-11-24): the panels scroll under it with the wheel and by the scrollbar, the button
// it is over follows the list, a click opens a game's page and its BACK comes back to the list as it was scrolled, and
// a scrollbar drag let go over PLAY starts nothing.
TEST(sdl_program_launcher_follows_the_mouse) {
  const std::string dir = std::string(MW3_TEST_TMP) + "/launcher_mouse";
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir + "/wd", ec);
  setenv("MW3_LAUNCHER_FRAMES", "400", 1);
  // A step prints what it found under the mouse as it came; the moves by a pixel let the list settle (it scrolls smoothly).
  const std::string settle = "move 712,130;move 712,131;move 712,132;move 712,133;";
  setenv("MW3_LAUNCHER_MOUSE",
         ("click 427,395;" + settle + "move 1,1;"                     // BEFORE YOU PLAY's OK; over MW3's PLAY
          "move 712,130;wheel -3;" + settle + "move 2,2;"                           // scrolled: another game's button
          "move 712,130;wheel 20;" + settle + "move 3,3;"                           // back at the top
          "down 808,130;move 808,131;up 712,130;" + settle + "move 4,4;"  // the scrollbar taken, let go over PLAY: no press
          "down 808,130;move 808,260;up 808,260;" + settle + "move 5,5;"  // dragged down
          "click 577,345;move 427,400;click 427,400;" + settle + "move 6,6")  // a gear, its page's BACK
             .c_str(),
         1);
  const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--window", "854x480", "--nosound", "--hidden"}, dir + "/launcher.log");
  unsetenv("MW3_LAUNCHER_MOUSE"), unsetenv("MW3_LAUNCHER_FRAMES");
  int status = 0;
  EXPECT(waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0);
  std::ifstream f(dir + "/launcher.log");
  std::vector<std::string> over;  // what each step found under the mouse as it came, and the screens between
  for (std::string line; std::getline(f, line);) {
    if (const size_t at = line.find("mouse: "); at != std::string::npos) over.push_back(line.substr(at + 7));
    else if (line.find("launcher: screen ") != std::string::npos) over.push_back(line.substr(line.find("screen "), 11));
  }
  for (const std::string& o : over) std::printf("  %s\n", o.c_str());
  const auto at = [&](const char* step) {
    for (const std::string& o : over)
      if (o.rfind(std::string("move ") + step + ", ", 0) == 0) return o.substr(o.find("over ") + 5);
    return std::string("no step");
  };
  EXPECT(at("1,1") == "MECHWARRIOR 3" && at("3,3") == "MECHWARRIOR 3" && at("4,4") == "MECHWARRIOR 3");
  for (const char* scrolled : {"2,2", "5,5"}) EXPECT(at(scrolled) != "MECHWARRIOR 3" && at(scrolled) != "nothing" && at(scrolled) != "no step");
  EXPECT(at("6,6") == at("5,5"));
  EXPECT(std::count(over.begin(), over.end(), "screen game") == 1 && std::count(over.begin(), over.end(), "screen main") == 2);
  EXPECT(std::none_of(over.begin(), over.end(), [](const std::string& o) { return o.rfind("screen work", 0) == 0; }));
}

























// mw3launcher starts the game as its own program (docs/launcher.md, The two
// programs): MECHWARRIOR 3 (--launcher-play) runs MW3_GAME_BIN, here a
// stand-in with the install and this start's options, as `script` says
// (it keeps its arguments; READY writes the ready file, DONE its end's
// mark, which it also writes when it reaches the script's end). The launcher stays
// open beside the game from its first frame (2026-11-26; it closed there from
// 2026-11-01) and, with --launcher-play, quits when the game exits; a game that
// ends first or is slow says so on its status line. Hidden, without sound.
namespace {
struct LauncherRun {
  std::string log;
  std::vector<std::string> args;
  bool exited = false;      // the launcher's process ended, with 0
  bool game_alive = false;  // the stand-in had not finished when it did
  bool game_finished = false;  // and finished on its own after
};
// `env` set for the launcher's run only; `before` gets the test's folder
// (its write dir is <dir>/wd) to lay out files first.
LauncherRun run_launcher_with(const std::string& name, const std::string& script, const char* ready_ms = nullptr,
                              const std::vector<std::pair<std::string, std::string>>& env = {},
                              const std::function<void(const std::string&)>& before = nullptr, bool play = true) {
  LauncherRun out;
  const std::string dir = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir + "/wd", ec);
  if (before) before(dir);
  const std::string game = dir + "/mw3", done = dir + "/done";
  {
    std::ofstream sh(game);
    std::string body = script;
    for (size_t at; (at = body.find("READY")) != std::string::npos;)
      body.replace(at, 5, "while [ $# -gt 0 ]; do if [ \"$1\" = --ready-file ]; then : > \"$2\"; fi; shift; done");
    for (size_t at; (at = body.find("DONE")) != std::string::npos;) body.replace(at, 4, ": > \"" + done + "\"");
    for (size_t at; (at = body.find("STATUS")) != std::string::npos;)  // the game's status line, beside the ready file
      body.replace(at, 6, "R=; P=; for A in \"$@\"; do [ \"$P\" = --ready-file ] && R=$A; P=$A; done; "
                          "printf 'Importing your movies (first start only): 3 of 19...' > \"$R.status\"");
    sh << "#!/bin/sh\n" << kAsksAsMw3 << "printf '%s\\n' \"$@\" > \"" << dir << "/args.txt\"\n" << body << "\n: > \"" << done << "\"\n" << kVersionLine;
  }
  std::filesystem::permissions(game, std::filesystem::perms::owner_all, ec);
  setenv("MW3_GAME_BIN", game.c_str(), 1);
  if (ready_ms) setenv("MW3_LAUNCHER_READY_MS", ready_ms, 1);
  bool importer = false;
  for (const auto& [k, v] : env) setenv(k.c_str(), v.c_str(), 1), importer = importer || k == "MW3_IMPORTER";
  if (!importer) setenv("MW3_NO_IMPORT", "1", 1);  // no movie conversion in the launcher unless a stand-in is given
  std::vector<std::string> args = {"--write-dir", dir + "/wd", "--window", "640x480", "--nosound", "--hidden"};
  if (play) args.insert(args.begin() + 2, "--launcher-play");
  const pid_t pid = spawn_launcher(args, dir + "/launcher.log");
  for (const auto& [k, v] : env) unsetenv(k.c_str());
  unsetenv("MW3_NO_IMPORT");
  unsetenv("MW3_LAUNCHER_READY_MS");
  unsetenv("MW3_GAME_BIN");
  int status = 0;
  out.exited = waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0;
  out.game_alive = !std::filesystem::exists(done, ec);
  for (int i = 0; i < 100 && !std::filesystem::exists(done, ec); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
  out.game_finished = std::filesystem::exists(done, ec);
  std::ifstream f(dir + "/launcher.log");
  out.log.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  std::ifstream af(dir + "/args.txt");
  for (std::string l; std::getline(af, l);) out.args.push_back(l);
  return out;
}
bool has(const std::string& text, const char* what) { return text.find(what) != std::string::npos; }
}  // namespace

TEST(sdl_program_launcher_starts_mw3_as_its_own_program) {
  // The first frame at once, then 3 s of play: the launcher stays open beside the game, then quits at its end.
  const LauncherRun r = run_launcher_with("launcher_game", "READY\nsleep 3");
  std::printf("launcher -> game: %zu arguments; the launcher ended %d after the game %d\n", r.args.size(), r.exited,
              !r.game_alive && r.game_finished);
  const std::string dir = std::string(MW3_TEST_TMP) + "/launcher_game";
  // No copy passed (b6 piece 2, 2026-11-26): the game finds the one its status shows itself.
  EXPECT(r.args.size() == 8 && r.args[0] == "--write-dir" && r.args[1] == dir + "/wd" && r.args[2] == "--window" &&
         r.args[3] == "640x480" && r.args[4] == "--nosound" && r.args[5] == "--hidden" && r.args[6] == "--ready-file");
  EXPECT(r.exited && !r.game_alive && r.game_finished);
  EXPECT(has(r.log, "the game's first frame") && has(r.log, "the launcher stays open") && !has(r.log, "the launcher closes"));
  EXPECT(has(r.log, "MechWarrior 3 exited (0)"));
}


// Quit while a game runs (here the frames' end, 1 s in, the game 3 s): the game goes on. In an AppImage the
// launcher waits for it first: its end would unmount what the game is running from (docs/linux.md, Packaging).
TEST(sdl_program_launcher_waits_for_the_game_in_an_appimage) {
  const LauncherRun plain = run_launcher_with("launcher_game_quit", "READY\nsleep 3", nullptr, {{"MW3_LAUNCHER_FRAMES", "60"}});
  EXPECT(plain.exited && plain.game_alive && plain.game_finished && !has(plain.log, "MechWarrior 3 exited"));
  const LauncherRun r = run_launcher_with("launcher_game_image", "READY\nsleep 3", nullptr,
                                          {{"APPIMAGE", "/x.AppImage"}, {"MW3_LAUNCHER_FRAMES", "60"}});
  EXPECT(r.exited && !r.game_alive && r.game_finished);
  EXPECT(has(r.log, "the game's first frame") && has(r.log, "MechWarrior 3 exited (0)"));
}


TEST(sdl_program_launcher_stays_when_the_game_fails_or_is_slow) {
  // An error before any frame: the launcher up with its status line, until
  // the game's end (and, with --launcher-play, quits then).
  const LauncherRun bad = run_launcher_with("launcher_game_bad", "sleep 1\nDONE\nexit 3");
  std::printf("a failed start: the launcher saw the exit %d, stayed %d\n", has(bad.log, "MechWarrior 3 exited (3)"),
              !has(bad.log, "the launcher closes"));
  EXPECT(bad.exited && has(bad.log, "MechWarrior 3 exited (3)") && !has(bad.log, "the game's first frame"));
  // No frame within the wait (0.5 s here, 20 s in play): said, still
  // watching; the frame after it is the game's start all the same.
  const LauncherRun slow = run_launcher_with("launcher_game_slow", "sleep 2\nREADY\nsleep 2", "500");
  std::printf("a slow start: said %d, then the first frame seen %d, the game's end waited for %d\n",
              has(slow.log, "no first frame from the game after"), has(slow.log, "the launcher stays open"), !slow.game_alive);
  EXPECT(slow.exited && !slow.game_alive);
  EXPECT(has(slow.log, "no first frame from the game after") && has(slow.log, "the launcher stays open"));
  // A game that ends by starting itself again as a new process (Windows' restart, log_start.h's exit code), before
  // or after its first frame: the game goes on, the launcher stays without a word of an error, its panel no longer
  // following it (and --launcher-play quits, no game left to watch).
  const LauncherRun again = run_launcher_with("launcher_game_again", "sleep 1\nDONE\nexit " + std::to_string(sdlfe::kStartedAgainExit));
  std::printf("a game that started itself again: said %d, no error %d\n", has(again.log, "started itself again"),
              !has(again.log, "exited (93)"));
  EXPECT(again.exited && has(again.log, "MechWarrior 3 started itself again") && has(again.log, "no longer watched"));
  EXPECT(!has(again.log, "exited (93)") && !has(again.log, "closed with an error") && !has(again.log, "the launcher closes"));
  const LauncherRun later = run_launcher_with("launcher_game_again_later", "READY\nsleep 1\nDONE\nexit " + std::to_string(sdlfe::kStartedAgainExit));
  EXPECT(later.exited && has(later.log, "the launcher stays open") && has(later.log, "no longer watched"));
  EXPECT(!has(later.log, "closed with an error") && !has(later.log, "the launcher closes"));
}



// A game with a newer release splits its card's button (2026-11-22: "the main button can be play, and there
// can be a divider on the end"): PLAY, then that game's UPDATE as the next button, its end. Pirate's Moon and MW1
// Remake installed older, their releases newer.
TEST(sdl_program_launcher_splits_play_with_its_update) {
  const auto lay = [](const std::string& d) {
    namespace fs = std::filesystem;
    for (const auto& [key, prog, tag] : {std::tuple<std::string, std::string, std::string>{"pm", "mw3pm", "pm-v1.2.3"},
                                        {"mw1", "mw1", "mw1-v1.0.7"}}) {
      fs::create_directories(d + "/games/" + key);
      std::ofstream(d + "/games/" + key + "/" + prog) << "#!/bin/sh\nexit 0\n";
      fs::permissions(d + "/games/" + key + "/" + prog, fs::perms::owner_all);
      std::ofstream(d + "/games/" + key + "/rebuild-game.txt") << "# x\ngame=" << key << "\ntag=" << tag << "\nfile=x\nsha256=x\nfrom=github\n";
    }
    fs::create_directories(d + "/api");
    fs::create_directories(d + "/wd/music");  // music there: no notes screen before the cards
    std::ofstream(d + "/wd/music/track02.wav") << "x";
#if defined(__APPLE__)
    const std::string tail = "-mac.dmg";
#else
    const std::string tail = "-x86_64.AppImage";
#endif
    for (const auto& [repo, tag, stem] : {std::tuple<std::string, std::string, std::string>{"mech3-pm-re", "pm-v1.2.5", "PM-Rebuild-"},
                                         {"mech1-remake-re", "mw1-v1.0.9", "MW1-Remake-"}})
      std::ofstream(d + "/api/" + repo + ".json") << "[{\"tag_name\": \"" << tag << "\", \"draft\": false, \"prerelease\": false, \"body\": \"x\", "
                                                  << "\"assets\": [{\"name\": \"" << stem << tag << tail << "\", \"size\": 1000, \"browser_download_url\": \"file://" << d
                                                  << "/x\"}, {\"name\": \"SHA256SUMS\", \"size\": 10, \"browser_download_url\": \"file://" << d << "/s\"}]}]";
  };
  const std::string dir = std::string(MW3_TEST_TMP) + "/launcher_split";
  const LauncherRun r = run_launcher_with("launcher_split", "exit 0", nullptr,
                                          {{"MW3_GAMES_API", "file://" + dir + "/api/{repo}.json"}, {"MW3_GAMES_DIR", dir + "/games"},
                                           {"MW3_LAUNCHER_FRAMES", "240"}},
                                          lay, false);
  const size_t at = r.log.rfind("screen main: ");
  const std::string line = at == std::string::npos ? std::string() : r.log.substr(at, r.log.find('\n', at) - at);
  std::printf("split: %s\n", line.c_str());
  EXPECT(line.find("PIRATE'S MOON, PIRATE'S MOON UPDATE, ") != std::string::npos);
  EXPECT(line.find("PIRATE'S MOON UPDATE, PIRATE'S MOON SETTINGS, MECHWARRIOR 1 ORIGINAL, MECHWARRIOR 1 ORIGINAL SETTINGS, MECHWARRIOR 1, MECHWARRIOR 1 UPDATE, MECHWARRIOR 1 SETTINGS, ") != std::string::npos);
  EXPECT(line.find("MECHWARRIOR 3 UPDATE") == std::string::npos);  // beside this program: no update of its own
}

// CHECK FOR UPDATES on a launcher left open (2026-11-24): everything asked again in place. The launcher's and Pirate's
// Moon's lists have nothing new at the start, then (before the press) something: the start finds nothing, the button finds both, the panel gets its UPDATE, and the box offers the launcher or all of it. UPDATE ALL
// takes the games first (here a download that is not there: it stops, the launcher not touched).
TEST(sdl_program_launcher_rechecks_while_open) {
  namespace fs = std::filesystem;
  std::error_code ec;
#if defined(__APPLE__)
  const std::string tail = "-mac.dmg";
#else
  const std::string tail = "-x86_64.AppImage";
#endif
  auto rel = [&](const std::string& tag, const std::string& stem) {
    return "[{\"tag_name\": \"" + tag + "\", \"draft\": false, \"prerelease\": false, \"body\": \"x\", \"assets\": [{\"name\": \"" + stem + tail +
           "\", \"size\": 1000, \"browser_download_url\": \"file:///nowhere/x\"}, {\"name\": \"SHA256SUMS\", \"size\": 10, \"browser_download_url\": \"file:///nowhere/s\"}]}]";
  };
  struct Case {
    const char* name;
    bool launcher, game, all;
  };
  for (const Case& c : {Case{"both", true, true, false}, Case{"all", true, true, true}, Case{"game", false, true, false}, Case{"none", false, false, false}}) {
    const std::string name = std::string("launcher_recheck_") + c.name, d = std::string(MW3_TEST_TMP) + "/" + name;
    std::vector<std::thread> feeds;
    std::vector<std::pair<std::string, std::string>> env = {{"MW3_GAMES_API", "file://" + d + "/api/{repo}.json"}, {"MW3_GAMES_DIR", d + "/games"},
                                                            {"MW3_UPDATE_API", "file://" + d + "/api/launcher.json"}, {"MW3_LAUNCHER_FRAMES", "400"},
                                                            {"MW3_LAUNCHER_NAV", "next,prev,prev,press"}, {"MW3_LAUNCHER_NAV_LATE", d + "/go"}};
    if (c.all) env.push_back({"MW3_LAUNCHER_UPDATE_ALL", "1"});
    const LauncherRun r = run_launcher_with(
        name, "exit 0", nullptr, env,
        [&](const std::string& dir) {
          fs::create_directories(dir + "/games/pm"), fs::create_directories(dir + "/api"), fs::create_directories(dir + "/wd/music");
          std::ofstream(dir + "/wd/music/track02.wav") << "x";
          std::ofstream(dir + "/games/pm/mw3pm") << "#!/bin/sh\nexit 0\n";
          fs::permissions(dir + "/games/pm/mw3pm", fs::perms::owner_all);
          std::ofstream(dir + "/games/pm/rebuild-game.txt") << "# x\ngame=pm\ntag=pm-v1.2.3\nfile=x\nsha256=x\nfrom=github\n";
          std::ofstream(dir + "/games/rebuild-games.txt") << "# x\n";
          for (const char* repo : {"mech3-re", "mech1-remake-re", "mech1-re", "inception-re", "inception-remake-re"}) std::ofstream(dir + "/api/" + repo + ".json") << "[]";
          std::ofstream(dir + "/api/mech3-pm-re.json") << rel("pm-v1.2.3", "PM-Rebuild-pm-v1.2.3");
          std::ofstream(dir + "/api/launcher.json") << rel("v0.0.1", "MW3-Rebuild-0.0.1");
          feeds.emplace_back([dir, c, rel] {  // once the start has had its answers: the new releases, then the press
            for (int i = 0; i < 400; ++i) {
              std::ifstream f(dir + "/launcher.log");
              const std::string log((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
              if (has(log, "Pirate's Moon 1.2.3 is the newest") && has(log, "update check at the start: ")) break;
              std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            if (c.game) std::ofstream(dir + "/api/mech3-pm-re.json") << rel("pm-v1.2.5", "PM-Rebuild-pm-v1.2.5");
            if (c.launcher) std::ofstream(dir + "/api/launcher.json") << rel("v98.1.0", "MW3-Rebuild-98.1.0");
            std::ofstream(dir + "/go") << "x";
          });
        },
        false);
    for (std::thread& t : feeds) t.join();
    const size_t pressed = r.log.find("launcher: update check at the start");
    const std::string before = r.log.substr(0, std::min(pressed, r.log.size())), after = r.log.substr(std::min(pressed, r.log.size()));
    const size_t at = r.log.find("launcher: updates: ");
    std::printf("  %s: %s\n", c.name, at == std::string::npos ? "(nothing said)" : r.log.substr(at + 19, r.log.find('\n', at) - at - 19).c_str());
    EXPECT(r.exited && has(before, "launcher: ") && !has(before, "PIRATE'S MOON UPDATE") && has(after, "CHECK FOR UPDATES, QUIT\n"));
    EXPECT(has(after, "PIRATE'S MOON UPDATE, ") == c.game && has(after, "UPDATE AVAILABLE 98.1.0") == c.launcher);
    EXPECT(has(after, "launcher: updates: Ready to update: Pirate's Moon 1.2.5.\n") == c.game);
    EXPECT(has(after, "launcher: updates: Everything is up to date.\n") == (!c.game && !c.launcher));
    EXPECT(has(after, "UPDATE AVAILABLE 98.1.0, UPDATE ALL, UPDATE LAUNCHER, NOT NOW\n") == c.launcher);
    EXPECT(has(after, "screen work") == c.all && !has(r.log, "Downloaded and checked") && !has(after, "launcher: update: Could not"));
    if (c.all) {  // Pirate's Moon's failed download ended it, on the games, the launcher not asked for
      const size_t work = std::min(after.find("screen work"), after.size());  // none: the check below fails, the run goes on
      std::printf("    %s\n", after.substr(work, 600).c_str());
      EXPECT(work < after.size() && has(after.substr(work), "screen main: "));
      EXPECT(has(after, "job: Updates: game:pm 0.0 MB, launcher 0.0 MB (0.0 MB in all)") && has(after, "job: ended at "));  // one bar for them all
    }
    if (!check::failures) fs::remove_all(d, ec);
  }
}



// Copies installed without the launcher (2026-11-24, docs/launcher.md): Pirate's Moon's own record in its user folder
// links it at the start (a development build's never); its gear's page links another by hand, unlinks one (not
// linked again by itself) and uninstalls the launcher's copy, with its game files only when asked and only a folder
// our copying made (here the files are links to game/pm, which stays whole).
// INSTALL FROM A DOWNLOADED FILE's check (sdlfe::games::check_file, rekit's SHA-256 since b6): the file's sum, and
// the SHA256SUMS beside it matched, mismatched or missing.
TEST(sdl_games_check_file_reads_its_sums) {
  namespace fs = std::filesystem;
  using FC = sdlfe::games::FileCheck;
  const std::string dir = std::string(MW3_TEST_TMP) + "/games_check_file";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  const std::string file = dir + "/Game-x.zip", abc = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
  std::ofstream(file, std::ios::binary) << "abc";
  const FC none = sdlfe::games::check_file(file);
  EXPECT(none.kind == FC::kNoSums && none.sha256 == abc && none.words.find("no SHA256SUMS") != std::string::npos);
  std::ofstream(dir + "/SHA256SUMS") << abc << "  Game-x.zip\n";
  const FC ok = sdlfe::games::check_file(file);
  EXPECT(ok.kind == FC::kMatches && ok.sha256 == abc);
  std::ofstream(dir + "/SHA256SUMS") << std::string(64, '0') << "  Game-x.zip\n";
  EXPECT(sdlfe::games::check_file(file).kind == FC::kMismatch);
  std::ofstream(dir + "/SHA256SUMS") << abc << "  Other.zip\n";
  EXPECT(sdlfe::games::check_file(file).kind == FC::kNoSums);
  if (!check::failures) fs::remove_all(dir, ec);
}


// The panels (2026-11-24, docs/launcher.md): a game whose repo has no release says COMING SOON (MW1's ORIGINAL
// here), one whose repo has one offers DOWNLOAD (Inception Remake, its own panel once it has a release), and a game
// on the setup kit's contract beside the launcher (Inception Rebuild, a stand-in) shows its --status; its PLAY starts
// it plain.
TEST(sdl_program_launcher_family_cards) {
  namespace fs = std::filesystem;
  const std::string dir = std::string(MW3_TEST_TMP) + "/launcher_families";
  const auto lay = [](const std::string& d) {
    fs::create_directories(d + "/api");
    fs::create_directories(d + "/games");
    fs::create_directories(d + "/wd/music");  // music there: no notes screen before the cards
    std::ofstream(d + "/wd/music/track02.wav") << "x";
#if defined(__APPLE__)
    const std::string tail = "-mac.dmg";
#else
    const std::string tail = "-x86_64.AppImage";
#endif
    for (const char* repo : {"mech3-pm-re", "mech1-remake-re", "mech1-re", "inception-re"}) std::ofstream(d + "/api/" + repo + ".json") << "[]";
    std::ofstream(d + "/api/inception-remake-re.json")
        << "[{\"tag_name\": \"chirm-v0.1.0\", \"draft\": false, \"prerelease\": false, \"body\": \"x\", \"assets\": [{\"name\": "
        << "\"CHI-Remake-chirm-v0.1.0" << tail << "\", \"size\": 1000, \"browser_download_url\": \"file://" << d << "/x\"}, "
        << "{\"name\": \"SHA256SUMS\", \"size\": 10, \"browser_download_url\": \"file://" << d << "/s\"}]}]";
    std::ofstream sh(d + "/chi");
    sh << "#!/bin/sh\nif [ \"$1\" = --status ]; then echo '{\"kit\": 1, \"game\": \"inception\", \"version\": \"0.1.0\", "
          "\"files\": {\"state\": \"ready\", \"where\": \"/games/chi\"}, \"first_run\": {\"pending\": []}, \"needs\": []}'; exit 0; fi\n"
       << "printf '%s\\n' \"$#\" > \"" << d << "/chi-ran\"\nexit 0\n";
    sh.close();
    fs::permissions(d + "/chi", fs::perms::owner_all);
  };
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir + "/wd", ec);
  lay(dir);
  // Drawn once (its repos asked first): each game's line.
  setenv("MW3_GAMES_API", ("file://" + dir + "/api/{repo}.json").c_str(), 1), setenv("MW3_GAMES_DIR", (dir + "/games").c_str(), 1);
  setenv("MW3_CHI_BIN", (dir + "/chi").c_str(), 1);
  const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--launcher-shot", dir + "/main.bmp", "--launcher-screen", "main"}, dir + "/shot.log");
  for (const char* v : {"MW3_GAMES_API", "MW3_GAMES_DIR", "MW3_CHI_BIN"}) unsetenv(v);
  int status = 0;
  EXPECT(waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0);
  std::ifstream sf(dir + "/shot.log");
  const std::string shot((std::istreambuf_iterator<char>(sf)), std::istreambuf_iterator<char>());
  auto entry = [&](const std::string& name) {
    const size_t at = shot.find("entry " + name + ": ");
    return at == std::string::npos ? std::string() : shot.substr(at, shot.find('\n', at) - at);
  };
  std::printf("families:\n  %s\n  %s\n  %s\n", entry("MECHWARRIOR 1 ORIGINAL").c_str(), entry("INCEPTION ORIGINAL").c_str(),
              entry("INCEPTION REMAKE").c_str());
  EXPECT(entry("MECHWARRIOR 1 ORIGINAL") == "entry MECHWARRIOR 1 ORIGINAL: Coming soon");
  EXPECT(entry("INCEPTION ORIGINAL") == "entry INCEPTION ORIGINAL: Ready \xc2\xb7 0.1.0: /games/chi");
  EXPECT(entry("INCEPTION REMAKE").rfind("entry INCEPTION REMAKE: Not installed: DOWNLOAD gets 0.1.0", 0) == 0);
  EXPECT(has(shot, "games: chi: status ready"));
  EXPECT(has(shot, "INCEPTION ORIGINAL, INCEPTION ORIGINAL SETTINGS, INCEPTION REMAKE, INCEPTION REMAKE SETTINGS, CHECK FOR UPDATES"));  // the remake's panel, its release out
  // Run: Inception's PLAY (MW3's PLAY, then it: the games with no release greyed and skipped) starts the stand-in
  // plain. (The remake's own panel comes when its repo answers, on a thread: the shot above checks it.)
  const LauncherRun r = run_launcher_with("launcher_families_run", "exit 0", nullptr,
                                          {{"MW3_GAMES_API", "file://" + dir + "/api/{repo}.json"}, {"MW3_GAMES_DIR", dir + "/games"},
                                           {"MW3_CHI_BIN", dir + "/chi"}, {"MW3_LAUNCHER_FRAMES", "240"},
                                           {"MW3_LAUNCHER_NAV", "next,next,next,press"}},  // MW3, its gear, Inception
                                          [&](const std::string& d) { fs::create_directories(d + "/wd/music"), std::ofstream(d + "/wd/music/track02.wav") << "x"; },
                                          false);
  std::ifstream ran(dir + "/chi-ran");
  std::string args;
  std::getline(ran, args);
  std::printf("  started %d with %s arguments\n", fs::exists(dir + "/chi-ran"), args.c_str());
  EXPECT(r.exited && has(r.log, "focus INCEPTION ORIGINAL\n") && has(r.log, "Inception Rebuild exited (0)"));
  EXPECT(args == "0");  // plain: its own setup opens when its game files are missing
}

// The games catalog fetched (b7, docs/launcher.md, The games catalog): a stand-in's catalog adds a game (its badge
// fetched beside it), flips Inception's remake to released and removes MechWarrior 1 Rebuild (not installed: gone)
// and Inception (installed: shown, and it still plays); an entry under another owner and one for a newer launcher
// left out. Then offline with that cache, and offline with none (the built-in catalog).
TEST(sdl_program_launcher_takes_a_fetched_catalog) {
  namespace fs = std::filesystem;
  const std::string dir = std::string(MW3_TEST_TMP) + "/launcher_catalog";
  std::error_code ec;
  fs::remove_all(dir, ec);
  for (const char* d : {"/api", "/games", "/cat/art", "/wd/music", "/wd2/music"}) fs::create_directories(dir + d, ec);
  for (const char* w : {"/wd", "/wd2"}) std::ofstream(dir + w + "/music/track02.wav") << "x";
#if defined(__APPLE__)
  const std::string tail = "-mac.dmg";
#else
  const std::string tail = "-x86_64.AppImage";
#endif
  for (const char* repo : {"mech3-pm-re", "mech1-remake-re", "mech1-re", "inception-re", "inception-remake-re"}) std::ofstream(dir + "/api/" + repo + ".json") << "[]";
  std::ofstream(dir + "/api/battletech-re.json")
      << "[{\"tag_name\": \"btech-v0.1.0\", \"draft\": false, \"prerelease\": false, \"body\": \"x\", \"assets\": [{\"name\": "
      << "\"BT-btech-v0.1.0" << tail << "\", \"size\": 1000, \"browser_download_url\": \"file://" << dir << "/x\"}, "
      << "{\"name\": \"SHA256SUMS\", \"size\": 10, \"browser_download_url\": \"file://" << dir << "/s\"}]}]";
  fs::copy_file(std::string(MW3_ASSETS_FOR_TESTS) + "/launcher/badge_mw3.png", dir + "/cat/art/badge_bt.png", ec);
  auto entry = [](const std::string& key, const std::string& family, int order, const std::string& state, const std::string& name,
                  const std::string& title, const std::string& button, const std::string& repo, const std::string& prefix,
                  const std::string& program, const std::string& stem, const std::string& badge, const std::string& min = "1.5.2") {
    return "{\"key\": \"" + key + "\", \"family\": \"" + family + "\", \"order\": " + std::to_string(order) + ", \"state\": \"" + state +
           "\", \"name\": \"" + name + "\", \"title\": \"" + title + "\", \"heading\": \"" + title + "\", \"button\": \"" + button +
           "\", \"description\": \"A line.\", \"repo\": \"" + repo + "\", \"tag_prefix\": \"" + prefix + "\", \"program\": \"" + program +
           "\", \"assets\": {\"windows\": \"" + stem + "-{tag}-win64.zip\", \"mac\": \"" + stem + "-{tag}-mac.dmg\", \"linux\": \"" + stem +
           "-{tag}-x86_64.AppImage\"}, \"badge\": \"" + badge + "\", \"min_launcher\": \"" + min + "\"}";
  };
  std::ofstream(dir + "/cat/games.json")
      << "{\"catalog\": 1, \"revision\": 2, \"families\": [{\"key\": \"bt\", \"name\": \"BattleTech\", \"order\": 4}], \"games\": ["
      << entry("mw1r", "mw1", 3, "removed", "MechWarrior 1 Rebuild", "MECHWARRIOR 1: REVERSE ENGINEERED", "MECHWARRIOR 1 ORIGINAL", "mech1-re",
               "mw1r-v", "mw1r", "MW1-Rebuild", "badge_mw1o.png")
      << ", " << entry("chi", "inception", 5, "removed", "Inception Rebuild", "THE CRESCENT HAWK'S INCEPTION", "INCEPTION ORIGINAL",
                       "inception-re", "chi-v", "chi", "CHI-Rebuild", "badge_inception.png")
      << ", {\"panel_before_release\": false, " << entry("chirm", "inception", 6, "released", "Inception Remake", "INCEPTION REMAKE", "INCEPTION REMAKE",
                                                           "inception-remake-re", "chirm-v", "chirm", "CHI-Remake", "badge_inception.png").substr(1)
      << ", " << entry("btech", "bt", 7, "released", "BattleTech Rebuild", "BATTLETECH", "BATTLETECH", "battletech-re", "btech-v", "btech", "BT", "badge_bt.png")
      << ", " << entry("evil", "bt", 8, "released", "E", "E", "EVIL", "someoneelse/evil-re", "e-v", "e", "E", "badge_bt.png")
      << ", " << entry("future", "bt", 9, "released", "F", "F", "FUTURE", "future-re", "f-v", "f", "F", "badge_bt.png", "99.0.0") << "]}";
  {  // its badge vouched for by the catalog (badge_sha256): fetched only then
    std::ifstream in(dir + "/cat/games.json");
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    const std::string at = "\"badge\": \"badge_bt.png\"";
    text.replace(text.find(at), at.size(), at + ", \"badge_sha256\": \"" + rekit::sha256_file_hex(dir + "/cat/art/badge_bt.png") + "\"");
    std::ofstream(dir + "/cat/games.json") << text;
  }
  {
    std::ofstream sh(dir + "/chi");
    sh << "#!/bin/sh\nif [ \"$1\" = --status ]; then echo '{\"kit\": 1, \"game\": \"inception\", \"version\": \"0.1.0\", "
          "\"files\": {\"state\": \"ready\", \"where\": \"/games/chi\"}, \"first_run\": {\"pending\": []}, \"needs\": []}'; exit 0; fi\n"
       << "printf '%s\\n' \"$#\" > \"" << dir << "/chi-ran\"\nexit 0\n";
  }
  fs::permissions(dir + "/chi", fs::perms::owner_all);
  auto read = [](const std::string& f) {
    std::ifstream in(f);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  };
  // The others' programs not taken from beside the launcher (a build folder may hold them).
  static const char* const kOthers[] = {"MW3_PM_BIN", "MW3_MW1_BIN", "MW3_MW1R_BIN", "MW3_CHIRM_BIN"};
  auto shot = [&](const std::string& wd, const std::string& url, const std::string& name) {
    setenv("MW3_GAMES_API", ("file://" + dir + "/api/{repo}.json").c_str(), 1), setenv("MW3_GAMES_DIR", (dir + "/games").c_str(), 1);
    setenv("MW3_CHI_BIN", (dir + "/chi").c_str(), 1), setenv("MW3_CATALOG_URL", url.c_str(), 1);
    for (const char* v : kOthers) setenv(v, "", 1);
    const pid_t pid = spawn_launcher({"--write-dir", wd, "--launcher-shot", dir + "/" + name + ".bmp", "--launcher-screen", "main"}, dir + "/" + name + ".log");
    for (const char* v : {"MW3_GAMES_API", "MW3_GAMES_DIR", "MW3_CHI_BIN", "MW3_CATALOG_URL"}) unsetenv(v);
    for (const char* v : kOthers) unsetenv(v);
    int status = 0;
    EXPECT(waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    return read(dir + "/" + name + ".log");
  };
  auto line = [](const std::string& log, const std::string& head) {
    const size_t at = log.find(head);
    return at == std::string::npos ? std::string() : log.substr(at, log.find('\n', at) - at);
  };
  const std::string fetched = shot(dir + "/wd", "file://" + dir + "/cat/games.json", "fetched");
  std::printf("fetched catalog:\n  %s\n  %s\n  %s\n  %s\n", line(fetched, "catalog: revision").c_str(), line(fetched, "entry BATTLETECH").c_str(),
              line(fetched, "entry INCEPTION ORIGINAL").c_str(), line(fetched, "entry INCEPTION REMAKE").c_str());
  EXPECT(has(fetched, "catalog: revision 2 cached") && has(fetched, "catalog: revision 2, fetched (7 games)"));
  EXPECT(has(fetched, "left out: games (evil): its repo someoneelse/evil-re is not under telltaleatheist"));
  EXPECT(has(fetched, "left out: games (future): it needs launcher 99.0.0 or newer"));
  EXPECT(line(fetched, "entry BATTLETECH").rfind("entry BATTLETECH: Not installed: DOWNLOAD gets 0.1.0", 0) == 0);
  EXPECT(line(fetched, "entry INCEPTION REMAKE") == "entry INCEPTION REMAKE: Coming soon");  // released in the catalog, its repo has none yet
  EXPECT(line(fetched, "entry INCEPTION ORIGINAL") == "entry INCEPTION ORIGINAL: Ready \xc2\xb7 0.1.0: /games/chi");
  const std::string main_screen = line(fetched, "screen main: ");
  std::printf("  %s\n", main_screen.c_str());
  EXPECT(has(main_screen, "INCEPTION ORIGINAL, INCEPTION ORIGINAL SETTINGS, INCEPTION REMAKE, BATTLETECH, BATTLETECH SETTINGS, CHECK FOR UPDATES"));
  EXPECT(!main_screen.empty() && !has(main_screen, "MECHWARRIOR 1 ORIGINAL"));  // removed, not installed: no panel
  EXPECT(fs::exists(dir + "/wd/catalog/games.json") && fs::exists(dir + "/wd/catalog/badge_bt.png"));
  EXPECT(!has(fetched, "not found\n"));  // its badge drawn from the cache
  // Offline with that cache: the same.
  const std::string cached = shot(dir + "/wd", "file://" + dir + "/missing.json", "cached");
  EXPECT(has(cached, "missing.json: ") && has(cached, "catalog: revision 2, fetched (7 games)") && line(cached, "screen main: ") == main_screen);
  // Offline with none: the built-in one, as before.
  const std::string none = shot(dir + "/wd2", "file://" + dir + "/missing.json", "builtin");
  std::printf("  offline, no cache: %s\n", line(none, "catalog: the built-in").c_str());
  std::printf("  %s\n", line(none, "screen main: ").c_str());
  EXPECT(has(none, "catalog: the built-in one (revision 1)") && has(line(none, "screen main: "), "MECHWARRIOR 1 ORIGINAL, MECHWARRIOR 1, INCEPTION ORIGINAL, INCEPTION ORIGINAL SETTINGS, CHECK FOR UPDATES"));
  EXPECT(!has(none, "BATTLETECH") && !has(line(none, "screen main: "), "INCEPTION REMAKE"));  // coming_soon, no release: no panel
  // Inception, removed but installed, still plays: its PLAY (MW3's PLAY, its gear, then Inception: the others greyed).
  const LauncherRun r = run_launcher_with("launcher_catalog_run", "exit 0", nullptr,
                                          {{"MW3_GAMES_API", "file://" + dir + "/api/{repo}.json"}, {"MW3_GAMES_DIR", dir + "/games"},
                                           {"MW3_CHI_BIN", dir + "/chi"}, {"MW3_CATALOG_URL", "file://" + dir + "/cat/games.json"},
                                           {"MW3_LAUNCHER_FRAMES", "240"}, {"MW3_LAUNCHER_NAV", "next,next,next,press"}, {"MW3_PM_BIN", ""},
                                           {"MW3_MW1_BIN", ""}, {"MW3_MW1R_BIN", ""}, {"MW3_CHIRM_BIN", ""}},
                                          [&](const std::string& d) { fs::create_directories(d + "/wd/music"), std::ofstream(d + "/wd/music/track02.wav") << "x"; },
                                          false);
  std::printf("  removed, installed: started %d\n", static_cast<int>(fs::exists(dir + "/chi-ran")));
  for (size_t at = 0; (at = r.log.find("focus ", at)) != std::string::npos; ++at) std::printf("    %s\n", r.log.substr(at, r.log.find('\n', at) - at).c_str());
  EXPECT(r.exited && has(r.log, "catalog: revision 2, fetched (7 games)") && has(r.log, "focus INCEPTION ORIGINAL\n") && has(r.log, "Inception Rebuild exited (0)"));
  if (!check::failures) fs::remove_all(dir, ec);
}

// A kit game that serves its setup (its status says `"setup": 1`; re-setup-kit's rekit/sdl/setup_serve.h) has its game
// files found and copied in on the launcher's own screens; its own setup window never opens. A stand-in answers.
TEST(sdl_program_launcher_sets_a_kit_game_up) {
  namespace fs = std::filesystem;
  std::error_code ec;
  auto stand_in = [](const std::string& root, const char* found, const char* look, const char* key = "chi", const char* id = "inception") {
    std::error_code err;
    fs::remove_all(root, err);
    fs::create_directories(root + "/api"), fs::create_directories(root + "/games");
    for (const char* repo : {"mech3-re", "mech3-pm-re", "mech1-remake-re", "mech1-re", "inception-re", "inception-remake-re"})
      std::ofstream(root + "/api/" + repo + ".json") << "[]";
    std::ofstream sh(root + "/" + key);
    sh << "#!/bin/sh\nR='" << root << "'\necho \"$*\" >> \"$R/asked\"\n"
       << "case \"$1 $2\" in\n"
          "'--status ') NOTE=; if [ -f \"$R/done\" ]; then ST=ready; elif [ -f \"$R/searched\" ]; then ST=ready; NOTE='found by the search, not set up yet'; else ST=missing; fi\n"
          "  echo '{\"kit\": 1, \"game\": \"" << id << "\", \"version\": \"0.2.0\", \"files\": {\"state\": \"'$ST'\"}, \"first_run\": "
          "{\"pending\": []}, \"needs\": [], \"note\": \"'\"$NOTE\"'\", \"setup\": 1}';;\n"
          "'--setup --describe') echo '{\"done\": true, \"ok\": true, \"title\": \"T\", \"pick_words\": \"a download\", \"find_sub\": "
          "\"A download (.zip)\", \"find_words\": \"Show us your own copy.\"}';;\n"
          "'--setup --search') echo '{\"done\": true, \"ok\": true" << found << "}';;\n"
          "'--setup --look') echo 'a log line'; echo '{\"done\": false, \"at\": 500000, \"stage\": \"Looking\", \"message\": \"\"}'\n"
          "  echo '{\"done\": true, " << look << "}';;\n"
          "'--setup --install') echo '{\"done\": false, \"at\": 250000, \"stage\": \"Copying\", \"message\": \"x\"}'; : > \"$R/done\"\n"
          "  echo '{\"done\": true, \"ok\": true, \"where\": \"/user/install\", \"summary\": \"Inception is set up.\"}';;\n"
          "*) echo \"plain $#\" >> \"$R/started\";;\nesac\nexit 0\n";
    sh.close();
    fs::permissions(root + "/" + key, fs::perms::owner_all);
  };
  auto run = [](const std::string& root, const char* name, const std::string& nav, std::vector<std::pair<std::string, std::string>> more) {
    std::vector<std::pair<std::string, std::string>> env{{"MW3_GAMES_API", "file://" + root + "/api/{repo}.json"}, {"MW3_GAMES_DIR", root + "/games"},
                                                         {"MW3_LAUNCHER_FRAMES", "400"}, {"MW3_LAUNCHER_NAV", nav}};
    if (!std::filesystem::exists(root + "/games/chi")) env.push_back({"MW3_CHI_BIN", root + "/chi"});  // else the launcher's own copy
    for (auto& e : more) env.push_back(e);
    return run_launcher_with(name, "exit 0", nullptr, env, [](const std::string& d) {
      fs::create_directories(d + "/wd/music"), std::ofstream(d + "/wd/music/track02.wav") << "x";
    }, false);
  };
  auto read = [](const std::string& f) {
    std::ifstream in(f);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  };
  const std::string found = ", \"path\": \"/copy/CHI.zip\"", look = "\"ok\": true, \"words\": \"A download of Inception.\", \"into\": \"/user/install\"";
  // PLAY, FIND THE GAME (the first button): its search's find on the plate, INSTALL, then the game, plain.
  const std::string a = std::string(MW3_TEST_TMP) + "/launcher_kit_a";
  stand_in(a, found.c_str(), look.c_str());
  const LauncherRun r = run(a, "launcher_kit_a_run", "next,next,next,press,next,press", {{"MW3_LAUNCHER_GO", "1"}});
  const std::string asked = read(a + "/asked"), started = read(a + "/started");
  std::printf("kit setup: asked %d lines, started \"%s\"\n", static_cast<int>(std::count(asked.begin(), asked.end(), '\n')), started.c_str());
  EXPECT(r.exited && has(r.log, "screen find: FIND THE GAME, PICK A FILE, PICK A FOLDER, BACK") && has(r.log, "found plate: CHI.zip (A download of Inception.)"));
  EXPECT(has(r.log, "game files into /user/install") && has(r.log, "work: INSTALLING INCEPTION REBUILD") && has(r.log, "Inception is set up."));
  EXPECT(asked.find("--setup --search\n--setup --look /copy/CHI.zip\n--setup --install /copy/CHI.zip\n--status\n") != std::string::npos);
  EXPECT(started == "plain 0\n");
  // Nothing found: the picker, whose file goes to the game as it is (no folder taken for it); CHANGE FOLDER's folder asked for.
  const std::string b = std::string(MW3_TEST_TMP) + "/launcher_kit_b", to = b + "/drive";
  stand_in(b, "", look.c_str());
  fs::create_directories(to);
  std::ofstream(b + "/BTECH.EXE") << "x";
  const LauncherRun p = run(b, "launcher_kit_b_run", "next,next,next,press,next,press",
                            {{"MW3_LAUNCHER_GO", "1"}, {"MW3_LAUNCHER_PICKER", "path:" + b + "/BTECH.EXE"}, {"MW3_LAUNCHER_FILES_TO", to}});
  const std::string asked_b = read(b + "/asked");
  EXPECT(p.exited && has(p.log, "picker asked for") && has(asked_b, ("--setup --look " + b + "/BTECH.EXE\n").c_str()));
  EXPECT(has(asked_b, ("--setup --install " + b + "/BTECH.EXE --folder " + to + "\n").c_str()));  // empty: the game's as it is
  // Refused: why on the find screen; nothing installed or started.
  const std::string c = std::string(MW3_TEST_TMP) + "/launcher_kit_c";
  stand_in(c, found.c_str(), "\"ok\": false, \"summary\": \"No copy of it there.\"");
  const LauncherRun n = run(c, "launcher_kit_c_run", "next,next,next,press,next,press", {{"MW3_LAUNCHER_GO", "1"}});
  EXPECT(n.exited && has(n.log, "No copy of it there.") && !has(read(c + "/asked"), "--install") && read(c + "/started").empty());
  // Its files in already (2026-11-24): the gear's GAME FILES opens a picker, never the search, which finds the copy in
  // use and goes home; the pick, a .zip, is copied in. The same .zip given to LINK A COPY is game files too.
  const std::string in_use = ", \"path\": \"/user/install\"";
  std::string gone[2];
  int which = 0;
  for (const char* nav : {"next,next,next,next,press,next,press,next,press", "next,next,next,next,press,next,next,next,press"}) {
    const std::string e = gone[which] = std::string(MW3_TEST_TMP) + "/launcher_kit_ready" + std::to_string(which);
    stand_in(e, in_use.c_str(), look.c_str());
    std::ofstream(e + "/done") << "x";
    std::ofstream(e + "/CHI.zip") << "x";
    if (which == 1) {  // installed by the launcher: LINK A COPY is there
      fs::create_directories(e + "/games/chi");
      fs::rename(e + "/chi", e + "/games/chi/chi");
      std::ofstream(e + "/games/chi/rebuild-game.txt") << "# x\ngame=chi\ntag=chi-v1.0.0\nfile=x\nsha256=x\nfrom=github\n";
      std::ofstream(e + "/games/rebuild-games.txt") << "# x\n";
    }
    const LauncherRun g = run(e, ("launcher_kit_ready_run" + std::to_string(which)).c_str(), nav,
                              {{"MW3_LAUNCHER_GO", "1"}, {"MW3_LAUNCHER_PICKER", "path:" + e + "/CHI.zip"}});
    const std::string asked_e = read(e + "/asked");
    EXPECT(g.exited && has(g.log, which ? "screen game: GAME FILES, UNINSTALL, LINK A COPY ALREADY INSTALLED, INSTALL FROM A DOWNLOADED FILE, BACK, SETTINGS, WHAT'S NEW\n" : "screen game: GAME FILES, BACK, SETTINGS, WHAT'S NEW\n") && has(g.log, "screen find: PICK A FILE, PICK A FOLDER, BACK\n"));
    EXPECT(has(g.log, "the file picker asked for") && !has(asked_e, "--search") && has(asked_e, ("--setup --install " + e + "/CHI.zip\n").c_str()));
    EXPECT(has(g.log, "Inception is set up."));
    ++which;
  }
  // Found by its own search, not set up (2026-11-25): MechWarrior 1 Rebuild says ready with that note, and PLAY goes
  // through the launcher's screens all the same; set up after, it starts.
  const std::string s = std::string(MW3_TEST_TMP) + "/launcher_kit_searched";
  stand_in(s, found.c_str(), look.c_str(), "mw1r", "mech1");
  std::ofstream(s + "/searched") << "x";
  const LauncherRun q = run(s, "launcher_kit_searched_run", "next,next,next,press,next,press", {{"MW3_LAUNCHER_GO", "1"}, {"MW3_CHI_BIN", ""}, {"MW3_MW1R_BIN", s + "/mw1r"}});
  const std::string asked_s = read(s + "/asked"), started_s = read(s + "/started");
  std::printf("kit setup, found by its search: started \"%s\"\n%s", started_s.c_str(), asked_s.c_str());
  EXPECT(q.exited && has(q.log, "Game files found, not set up yet") && has(q.log, "screen find: FIND THE GAME, PICK A FILE, PICK A FOLDER, BACK"));
  EXPECT(has(q.log, "work: INSTALLING MECHWARRIOR 1 REBUILD") && has(asked_s, "--setup --install /copy/CHI.zip\n--status\n") && started_s == "plain 0\n");
  // A program is a game's own by its --status: Inception's is not MechWarrior 1 Rebuild's, whatever mw3.cfg says.
  const std::string w = std::string(MW3_TEST_TMP) + "/launcher_kit_wrong";
  stand_in(w, found.c_str(), look.c_str());
  for (const char* game : {"mw1r", "chi"}) {
    const LauncherRun l = run_launcher_with("launcher_kit_wrong_run", "exit 0", nullptr,
                                            {{"MW3_GAMES_API", "file://" + w + "/api/{repo}.json"}, {"MW3_GAMES_DIR", w + "/games"}, {"MW3_LAUNCHER_FRAMES", "60"}},
                                            [&](const std::string& d) {
                                              fs::create_directories(d + "/wd/music"), std::ofstream(d + "/wd/music/track02.wav") << "x";
                                              std::ofstream(d + "/wd/mw3.cfg") << "link_" << game << "=" << w << "/chi\n";
                                            }, false);
    const bool refused = has(l.log, ("the linked " + w + "/chi is gone or not this game's now").c_str());
    std::printf("kit setup: Inception linked as %s: refused %d\n", game, refused);
    EXPECT(l.exited && refused == !std::strcmp(game, "mw1r"));
  }
  for (const std::string& d : {a, b, c, gone[0], gone[1], s, w})
    if (!check::failures) fs::remove_all(d, ec);
}

// A status the launcher could not have is never an older program (mech1-remake-re #2, 2026-11-26: Remake 1.1.3 was
// told it was older than launcher 1.7.0). MECHWARRIOR 1 pressed with the Remake's stand-in: one without our version
// beside the launcher (never run), one too slow for the ask (MW3_STATUS_SECONDS), one slow only the first time (PLAY's
// ask answers and the press goes on), an AppImage linked (asked though its version is packed away), and one that
// serves no setup (the older message, still).
TEST(sdl_program_launcher_says_an_unknown_status_is_not_older) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const std::string root = std::string(MW3_TEST_TMP) + "/launcher_unknown";
  fs::remove_all(root, ec);
  for (const char* d : {"/api", "/games", "/inst/mw3", "/inst/pm"}) fs::create_directories(root + d, ec);
  for (const char* repo : {"mech3-re", "mech3-pm-re", "mech1-remake-re", "mech1-re", "inception-re", "inception-remake-re"})
    std::ofstream(root + "/api/" + repo + ".json") << "[]";
  auto program = [&](const std::string& name, const std::string& game, const std::string& files, const std::string& before,
                     const char* setup, bool tagged) {
    std::ofstream sh(root + "/" + name);
    sh << "#!/bin/sh\ncase \"$1\" in --status) " << before << "echo '{\"kit\": 1, \"game\": \"" << game
       << "\", \"version\": \"v0.0.0-test\", \"files\": " << files << ", \"first_run\": {\"pending\": []}, \"needs\": [], \"setup\": "
       << setup << ", \"play\": 1}';;\n--setup) echo '{\"done\": true, \"ok\": true, \"title\": \"T\", \"pick_words\": \"x\", "
       << "\"find_sub\": \"x\", \"find_words\": \"x\"}';;\nesac\nexit 0\n";
    if (tagged) sh << "# MW3-PROGRAM" << "-VERSION:" << (game == "mech1-remake" ? "mw1-v1.1.3" : "v0.0.0-test") << '\0' << "\n";
    sh.close();
    fs::permissions(root + "/" + name, fs::perms::owner_all, ec);
  };
  const std::string missing = "{\"state\": \"missing\"}";
  program("mw3", "mech3", "{\"state\": \"ready\", \"where\": \"" + root + "/inst/mw3\", \"data\": \"1.2\"}", "", "1", true);
  program("mw3pm", "mech3-pm", "{\"state\": \"ready\", \"where\": \"" + root + "/inst/pm\", \"data\": \"1.0\"}", "", "1", true);
  program("untagged", "mech1-remake", missing, "", "1", false);
  program("slow", "mech1-remake", missing, "sleep 4; ", "1", true);
  program("once", "mech1-remake", missing, "[ -f \"$0.asked\" ] || { : > \"$0.asked\"; sleep 4; }; ", "1", true);
  program("mw1.AppImage", "mech1-remake", missing, "", "1", false);
  program("old", "mech1-remake", missing, "", "0", true);
  const std::string mw3_files = "{\"state\": \"ready\", \"where\": \"" + root + "/inst/mw3\", \"data\": \"1.2\"}";
  program("mw3_once", "mech3", mw3_files, "[ -f \"$0.asked\" ] || { : > \"$0.asked\"; sleep 4; }; ", "1", true);
  program("mw3_untagged", "mech3", mw3_files, "", "1", false);
  // MECHWARRIOR 1 pressed (--launcher-mw1), or MECHWARRIOR 3 (`mw3` given: --launcher-play, no Remake)
  auto run = [&](const char* name, const std::string& mw1, const std::string& link, const std::string& mw3 = "") {
    const std::string dir = root + "/run_" + name;  // beside the stand-ins, not in their names
    fs::create_directories(dir + "/wd", ec);
    if (!link.empty()) std::ofstream(dir + "/wd/mw3.cfg") << "link_mw1=" << link << "\n";
    const std::vector<std::pair<std::string, std::string>> env{
        {"MW3_GAMES_API", "file://" + root + "/api/{repo}.json"}, {"MW3_GAMES_DIR", root + "/games"},
        {"MW3_GAME_BIN", mw3.empty() ? root + "/mw3" : mw3},
        {"MW3_PM_BIN", root + "/mw3pm"}, {"MW3_PM_WRITE_DIR", dir + "/pmwd"}, {"MW3_MW1_BIN", mw1}, {"MW3_NO_IMPORT", "1"},
        {"MW3_LAUNCHER_FRAMES", "300"}, {"MW3_STATUS_SECONDS", "2"}};
    for (const auto& [k, v] : env) setenv(k.c_str(), v.c_str(), 1);
    const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--window", "640x480", "--nosound", "--hidden",
                                      mw3.empty() ? "--launcher-mw1" : "--launcher-play"}, dir + "/launcher.log");
    for (const auto& [k, v] : env) unsetenv(k.c_str());
    int status = 0;
    const bool exited = waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    std::ifstream f(dir + "/launcher.log");
    const std::string log((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const bool older = has(log, "older than the launcher") || has(log, "cannot be given its MechWarrior");
    std::printf("unknown status, %s: exited %d, checking %d, did not answer %d, its screens %d, older %d\n", name, exited,
                has(log, "Checking MechWarrior"), has(log, "did not answer the launcher"),
                has(log, "its game files, on the launcher's screens"), older);
    if (!exited) std::printf("its log's end:\n%s\n", log.substr(log.size() > 1500 ? log.size() - 1500 : 0).c_str());
    EXPECT(exited);
    return log;
  };
  for (const char* name : {"untagged", "slow"}) {  // asked again at PLAY, and still none: said, never "older"
    const std::string log = run(name, root + "/" + name, "");
    EXPECT(has(log, "Checking MechWarrior 1 Remake..."));
    EXPECT(has(log, "MechWarrior 1 Remake did not answer the launcher (see mw3launcher.log)."));
    EXPECT(!has(log, "older than the launcher") && !has(log, "cannot be given its MechWarrior"));
    EXPECT(has(log, std::strcmp(name, "slow") ? "carries no version of ours" : "no answer in 2 s"));
  }
  {  // none at the start, PLAY's ask answers: the press goes on to its setup's screens
    const std::string log = run("once", root + "/once", "");
    EXPECT(has(log, "no answer in 2 s") && has(log, "Checking MechWarrior 1 Remake..."));
    EXPECT(has(log, "games: mw1: its game files, on the launcher's screens"));
    EXPECT(!has(log, "did not answer the launcher") && !has(log, "older than the launcher"));
  }
  {  // a linked AppImage: no version readable in its file, asked because its status said it is the Remake
    const std::string log = run("appimage", "", root + "/mw1.AppImage");
    EXPECT(has(log, ("games: mw1: linked v0.0.0-test at " + root + "/mw1.AppImage").c_str()));
    EXPECT(has(log, "games: mw1: status missing") && has(log, "games: mw1: its game files, on the launcher's screens"));
    EXPECT(!has(log, "Checking MechWarrior 1 Remake...") && !has(log, "older than the launcher"));
  }
  {  // one that answers without a served setup is older: its update offered or said, as before
    const std::string log = run("old", root + "/old", "");
    EXPECT(has(log, "games: mw1: status missing") && has(log, "cannot be given its MechWarrior (1989)"));
    EXPECT(!has(log, "Checking MechWarrior 1 Remake..."));
  }
  {  // MechWarrior 3's start ask gave none: asked once more on its own thread, then found, and PLAY starts it
    const std::string log = run("mw3_once", "", "", root + "/mw3_once");
    EXPECT(has(log, "games: mw3: status no answer (no answer in 2 s: stopped)") && has(log, "games: mw3: status ready"));
    EXPECT(has(log, ("Found MechWarrior 3 1.2 at " + root + "/inst/mw3.").c_str()) && has(log, "MechWarrior 3 exited (0)"));
    EXPECT(!has(log, "older than the launcher") && !has(log, "did not answer the launcher"));
    EXPECT(!has(log, "Checking MechWarrior 3..."));  // known before PLAY: the retry's, not PLAY's own ask
  }
  {  // MechWarrior 3's PLAY with its status unknown: asked again, and still none said (older_game's callers)
    const std::string log = run("mw3_untagged", "", "", root + "/mw3_untagged");
    EXPECT(has(log, "Checking MechWarrior 3...") && has(log, "MechWarrior 3 did not answer the launcher (see mw3launcher.log)."));
    EXPECT(!has(log, "older than the launcher") && !has(log, "does not set up its game files here"));
  }
  if (!check::failures) fs::remove_all(root, ec);
}

// A game installed here keeps its catalog entry when a later fetched catalog drops it (2026-11-26; DEPLOY.md: entries
// are set to removed, never deleted, but a deletion by mistake must not take an installed game's panel away). Catalog
// revision 2 adds BattleTech (fetched only), installed in the games folder; revision 3 leaves it out: its panel stays
// and plays, from <cache>/kept/btech.json. Once it is not installed any more, its kept entry is forgotten.
TEST(sdl_program_launcher_keeps_an_installed_games_entry) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const std::string dir = std::string(MW3_TEST_TMP) + "/launcher_kept";
  fs::remove_all(dir, ec);
  for (const char* d : {"/api", "/games/btech", "/wd/music"}) fs::create_directories(dir + d, ec);
  std::ofstream(dir + "/wd/music/track02.wav") << "x";
  for (const char* repo : {"mech3-pm-re", "mech1-remake-re", "mech1-re", "inception-re", "inception-remake-re", "battletech-re"})
    std::ofstream(dir + "/api/" + repo + ".json") << "[]";
  std::ofstream(dir + "/v2.json")
      << "{\"catalog\": 1, \"revision\": 2, \"families\": [{\"key\": \"bt\", \"name\": \"BattleTech\", \"order\": 4}], \"games\": [{\"key\": "
         "\"btech\", \"family\": \"bt\", \"order\": 7, \"state\": \"released\", \"name\": \"BattleTech Rebuild\", \"title\": \"BATTLETECH\", "
         "\"heading\": \"BATTLETECH\", \"button\": \"BATTLETECH\", \"description\": \"A line.\", \"repo\": \"battletech-re\", \"tag_prefix\": "
         "\"btech-v\", \"program\": \"btech\", \"assets\": {\"windows\": \"BT-{tag}-win64.zip\", \"mac\": \"BT-{tag}-mac.dmg\", \"linux\": "
         "\"BT-{tag}-x86_64.AppImage\"}, \"badge\": \"badge_mw3.png\", \"min_launcher\": \"1.5.2\"}]}";
  std::ofstream(dir + "/v3.json") << "{\"catalog\": 1, \"revision\": 3, \"families\": [], \"games\": []}";
  std::ofstream(dir + "/games/btech/rebuild-game.txt") << "game=btech\ntag=btech-v0.1.0\nfrom=github\n";
  {
    std::ofstream sh(dir + "/games/btech/btech");
    sh << "#!/bin/sh\nif [ \"$1\" = --status ]; then echo '{\"kit\": 1, \"game\": \"btech\", \"version\": \"0.1.0\", \"files\": {\"state\": "
          "\"ready\", \"where\": \"/games/btech\"}, \"first_run\": {\"pending\": []}, \"needs\": []}'; exit 0; fi\n"
       << ": > \"" << dir << "/btech-ran\"\nexit 0\n";
  }
  fs::permissions(dir + "/games/btech/btech", fs::perms::owner_all, ec);
  auto run = [&](const char* name, const char* catalog, const std::string& nav) {
    const std::vector<std::pair<std::string, std::string>> env{
        {"MW3_GAMES_API", "file://" + dir + "/api/{repo}.json"}, {"MW3_GAMES_DIR", dir + "/games"}, {"MW3_CATALOG_URL", "file://" + dir + "/" + catalog},
        {"MW3_LAUNCHER_FRAMES", "240"}, {"MW3_LAUNCHER_NAV", nav}, {"MW3_NO_IMPORT", "1"}, {"MW3_PM_BIN", ""}, {"MW3_MW1_BIN", ""},
        {"MW3_MW1R_BIN", ""}, {"MW3_CHI_BIN", ""}, {"MW3_CHIRM_BIN", ""}};
    for (const auto& [k, v] : env) setenv(k.c_str(), v.c_str(), 1);
    const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--window", "640x480", "--nosound", "--hidden"}, dir + "/" + name + ".log");
    for (const auto& [k, v] : env) unsetenv(k.c_str());
    int status = 0;
    EXPECT(waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    std::ifstream f(dir + "/" + std::string(name) + ".log");
    const std::string log((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const size_t at = log.find("screen main: ");
    std::printf("kept entry, %s: %s\n", name, at == std::string::npos ? "no main screen" : log.substr(at, log.find('\n', at) - at).c_str());
    for (const char* what : {"catalog: ", "focus ", "BattleTech Rebuild "})
      for (size_t a = 0; (a = log.find(what, a)) != std::string::npos; ++a)
        std::printf("    %s\n", log.substr(a, log.find('\n', a) - a).c_str());
    return log;
  };
  const std::string kept = dir + "/wd/catalog/kept/btech.json";
  const std::string a = run("v2", "v2.json", "");  // installed under revision 2: its entry copied beside the cache
  EXPECT(has(a, "catalog: revision 2, fetched") && has(a, "catalog: btech's entry kept beside the cache") && fs::exists(kept, ec));
  EXPECT(has(a, "entry BATTLETECH: "));
  const std::string b = run("v3", "v3.json", "next,press");  // left out under revision 3: kept, and its PLAY starts it
  EXPECT(has(b, "catalog: revision 3 cached") && has(b, "catalog: btech kept: installed here"));
  EXPECT(has(b, "entry BATTLETECH: ") && has(b, "focus BATTLETECH\n") && has(b, "BattleTech Rebuild exited (0)") &&
         fs::exists(dir + "/btech-ran", ec));
  fs::remove(dir + "/games/btech/rebuild-game.txt", ec);  // uninstalled: forgotten, no panel
  const std::string c = run("gone", "v3.json", "");
  EXPECT(has(c, "catalog: btech forgotten") && !has(c, "entry BATTLETECH") && !fs::exists(kept, ec));
  if (!check::failures) fs::remove_all(dir, ec);
}

// A live check (MW3_LIVE_CHECK=1, 2026-11-27): a hidden run asks the real addresses, as a player's start does, so a
// release is checked without a window. Here every real address is https://127.0.0.1/ (nothing listens: refused at
// once, no network): a signed catalog naming that home cached by a first run, and a sources file naming a catalog
// there. The same hidden run without the flag asks none of them (curl never started for them); with it, the
// catalog's, a game's and the launcher's own releases are asked. The other check-run rules hold: its own games folder.
TEST(sdl_program_launcher_live_check_asks_the_real_addresses) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const std::string dir = std::string(MW3_TEST_TMP) + "/launcher_live";
  fs::remove_all(dir, ec);
  for (const char* d : {"/games", "/wd/music"}) fs::create_directories(dir + d, ec);
  std::ofstream(dir + "/wd/music/track02.wav") << "x";
  std::ofstream(dir + "/local.json")
      << "{\"catalog\": 1, \"revision\": 9, \"homes\": [{\"key\": \"lo\", \"releases\": \"https://127.0.0.1/{repo}.json\", \"order\": 1}], "
         "\"families\": [{\"key\": \"bt\", \"name\": \"BattleTech\", \"order\": 4}], \"games\": [{\"key\": \"btech\", \"family\": \"bt\", "
         "\"order\": 7, \"state\": \"released\", \"name\": \"BattleTech Rebuild\", \"title\": \"BATTLETECH\", \"heading\": \"BATTLETECH\", "
         "\"button\": \"BATTLETECH\", \"description\": \"A line.\", \"repo\": \"battletech-re\", \"tag_prefix\": \"btech-v\", \"program\": "
         "\"btech\", \"assets\": {\"windows\": \"BT-{tag}-win64.zip\", \"mac\": \"BT-{tag}-mac.dmg\", \"linux\": \"BT-{tag}-x86_64.AppImage\"}, "
         "\"badge\": \"badge_mw3.png\", \"min_launcher\": \"1.5.2\"}]}";
  const std::string sources = dir + "/sources.json";
  write_sources(sources, {"https://127.0.0.1/catalog.json"}, {1});
  struct Got {
    std::string log, asked;
  };
  auto run = [&](const char* name, std::vector<std::pair<std::string, std::string>> env) {
    const std::string args = dir + "/" + name + ".args";
    env.insert(env.end(), {{"MW3_LAUNCHER_FRAMES", "60"}, {"MW3_NO_IMPORT", "1"}, {"MW3_CURL_ARGS_TO", args}, {"MW3_PM_BIN", ""},
                           {"MW3_MW1_BIN", ""}, {"MW3_MW1R_BIN", ""}, {"MW3_CHI_BIN", ""}, {"MW3_CHIRM_BIN", ""}});
    for (const auto& [k, v] : env) setenv(k.c_str(), v.c_str(), 1);
    const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--window", "640x480", "--nosound", "--hidden"}, dir + "/" + name + ".log");
    for (const auto& [k, v] : env) unsetenv(k.c_str());
    int status = 0;
    EXPECT(waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    std::ifstream f(dir + "/" + std::string(name) + ".log"), a(args);
    Got g{std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()),
          std::string((std::istreambuf_iterator<char>(a)), std::istreambuf_iterator<char>())};
    for (size_t at = 0; (at = g.log.find("127.0.0.1", at)) != std::string::npos; at = g.log.find('\n', at)) {
      const size_t from = g.log.rfind('\n', at) + 1;
      std::printf("  %s: %s\n", name, g.log.substr(from, g.log.find('\n', at) - from).c_str());
    }
    return g;
  };
  // The fixture catalog cached (a stand-in's file://, signed by test key 1 as spawn_launcher does).
  const Got cache = run("cache", {{"MW3_CATALOG_URL", "file://" + dir + "/local.json"}, {"MW3_GAMES_DIR", dir + "/games"}});
  EXPECT(has(cache.log, "catalog: revision 9, fetched") && has(cache.log, "catalog: homes: lo\n"));
  const auto asked = [](const Got& g, const std::string& url) { return g.asked.find("\n" + url + "\n") != std::string::npos; };
  // A check run as before: nothing real asked.
  const Got plain = run("plain", {{"MW3_SOURCES_FILE", sources}, {"MW3_GAMES_DIR", dir + "/games"}});
  EXPECT(has(plain.log, "catalog: homes: lo\n") && has(plain.log, "catalog at https://127.0.0.1/catalog.json: not asked in a check run\n"));
  EXPECT(has(plain.log, "games: not asked in a check run\n") && has(plain.log, "update check at the start: not asked in a check run\n"));
  EXPECT(plain.asked.find("127.0.0.1") == std::string::npos && !has(plain.log, "a live check"));
  // A live check: the catalog, a game's releases and the launcher's own asked at their real addresses; its own games
  // folder still (none given: never the computer's).
  const Got live = run("live", {{"MW3_SOURCES_FILE", sources}, {"MW3_LIVE_CHECK", "1"}});
  EXPECT(has(live.log, "catalog: a live check: the real addresses asked\n") && has(live.log, "catalog: homes: lo\n"));
  EXPECT(asked(live, "https://127.0.0.1/catalog.json") && !has(live.log, "not asked in a check run"));
  EXPECT(asked(live, "https://127.0.0.1/mech3-pm-re.json"));  // a game's own (the games' thread may close before the last)
  EXPECT(asked(live, "https://127.0.0.1/mw3-re.json") && asked(live, "https://127.0.0.1/mech3-re.json"));
  EXPECT(live.log.find("BattleTech/games/") == std::string::npos);
  if (!check::failures) fs::remove_all(dir, ec);
}

// The homes in use (2026-11-26, docs/launcher.md, Where the launcher looks), said at the start: the built-in list with
// no catalog fetched; a fetched one's (revision 4, a stand-in host) once cached; a later one with a bad list leaves the
// built-in one in force. A game's "original" is said, and fetched only after its DOWNLOAD (not here).
TEST(sdl_program_launcher_uses_the_catalogs_homes) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const std::string dir = std::string(MW3_TEST_TMP) + "/launcher_homes";
  fs::remove_all(dir, ec);
  for (const char* d : {"/api", "/games", "/wd/music"}) fs::create_directories(dir + d, ec);
  std::ofstream(dir + "/wd/music/track02.wav") << "x";
  for (const char* repo : {"mech3-pm-re", "mech1-remake-re", "mech1-re", "inception-re", "inception-remake-re", "battletech-re", "revenge-re"})
    std::ofstream(dir + "/api/" + repo + ".json") << "[]";
  const auto catalog = [&](int revision, const std::string& homes) {
    return "{\"catalog\": 1, \"revision\": " + std::to_string(revision) + ", \"homes\": " + homes +
           ", \"families\": [{\"key\": \"bt\", \"name\": \"BattleTech\", \"order\": 4}], \"games\": [{\"key\": \"btech\", \"family\": \"bt\", "
           "\"order\": 7, \"state\": \"released\", \"name\": \"BattleTech Rebuild\", \"title\": \"BATTLETECH\", \"heading\": \"BATTLETECH\", "
           "\"button\": \"BATTLETECH\", \"description\": \"A line.\", \"repo\": \"battletech-re\", \"tag_prefix\": \"btech-v\", \"program\": "
           "\"btech\", \"assets\": {\"windows\": \"BT-{tag}-win64.zip\", \"mac\": \"BT-{tag}-mac.dmg\", \"linux\": \"BT-{tag}-x86_64.AppImage\"}, "
           "\"badge\": \"badge_mw3.png\", \"min_launcher\": \"1.5.2\", \"original\": [{\"url\": \"https://files.example.org/bt.zip\", \"sha256\": \"" +
           std::string(64, 'a') + "\", \"size\": 10, \"name\": \"bt.zip\"}]}]}";
  };
  std::ofstream(dir + "/v4.json") << catalog(4, "[{\"key\": \"re\", \"releases\": \"https://releases.reverse-engineered.com/{repo}.json\", \"order\": 1}]");
  std::ofstream(dir + "/v5.json") << catalog(5, "[{\"key\": \"re\", \"releases\": \"http://releases.reverse-engineered.com/{repo}.json\", \"order\": 1}]");
  auto run = [&](const char* name, const char* cat) {
    std::vector<std::pair<std::string, std::string>> env{
        {"MW3_GAMES_API", "file://" + dir + "/api/{repo}.json"}, {"MW3_GAMES_DIR", dir + "/games"}, {"MW3_LAUNCHER_FRAMES", "60"},
        {"MW3_NO_IMPORT", "1"}, {"MW3_PM_BIN", ""}, {"MW3_MW1_BIN", ""}, {"MW3_MW1R_BIN", ""}, {"MW3_CHI_BIN", ""}, {"MW3_CHIRM_BIN", ""}};
    if (cat) env.push_back({"MW3_CATALOG_URL", "file://" + dir + "/" + cat});
    for (const auto& [k, v] : env) setenv(k.c_str(), v.c_str(), 1);
    const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--window", "640x480", "--nosound", "--hidden"}, dir + "/" + name + ".log");
    for (const auto& [k, v] : env) unsetenv(k.c_str());
    int status = 0;
    EXPECT(waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    std::ifstream f(dir + "/" + std::string(name) + ".log");
    const std::string log((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    for (size_t a = 0; (a = log.find("catalog: ", a)) != std::string::npos; ++a)
      std::printf("  %s: %s\n", name, log.substr(a, log.find('\n', a) - a).c_str());
    return log;
  };
  const std::string a = run("builtin", nullptr);
  EXPECT(has(a, "catalog: homes: gh, om\n") && !has(a, "names an original"));
  const std::string b = run("v4", "v4.json");
  EXPECT(has(b, "catalog: revision 4, fetched") && has(b, "catalog: homes: re\n"));
  EXPECT(has(b, "catalog: btech names an original download\n") && !has(b, "files.example.org"));
  const std::string c = run("v5", "v5.json");
  EXPECT(has(c, "catalog: revision 5, fetched") && has(c, "left out: homes[0]: http://releases.reverse-engineered.com/{repo}.json is not https://") &&
         has(c, "catalog: homes: gh, om\n") && has(c, "entry BATTLETECH: "));
  if (!check::failures) fs::remove_all(dir, ec);
}

// Signed catalogs (2026-11-26, docs/launcher.md, Where the launcher looks): sources.json's addresses asked in order,
// each with its .sig, the newest signed by a key the launcher knows cached (the first on a tie); one missing, unsigned,
// signed by another key or not its own text passed over for the next; none left, the built-in catalog. A signed
// catalog's next_keys learned (<cache>/keys.txt) so a catalog signed only by the next key is taken there, not in a
// fresh write dir; its catalog_sources asked first on the next start. sources.json missing: its built-in copy, which
// lists no key yet, so nothing fetched is taken, and a check run never asks its real addresses.
TEST(sdl_program_launcher_takes_only_a_signed_catalog) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const std::string dir = std::string(MW3_TEST_TMP) + "/launcher_signed_catalog";
  fs::remove_all(dir, ec);
  for (const char* d : {"/api", "/games", "/src"}) fs::create_directories(dir + d, ec);
  for (const char* repo : {"mech3-pm-re", "mech1-remake-re", "mech1-re", "inception-re", "inception-remake-re", "battletech-re", "revenge-re"})
    std::ofstream(dir + "/api/" + repo + ".json") << "[]";
  const auto src = [&](const char* n) { return dir + "/src/" + n + ".json"; };
  const auto url = [&](const char* n) { return "file://" + src(n); };
  const auto read = [](const std::string& f) {
    std::ifstream in(f, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  };
  // A catalog from `from` (its description says which), at `revision`, with `extra` fields; signed by `seeds`.
  const auto put = [&](const char* n, int revision, std::initializer_list<uint8_t> seeds, const std::string& extra = "") {
    std::ofstream(src(n)) << "{\"catalog\": 1, \"revision\": " << revision << extra
                          << ", \"families\": [{\"key\": \"bt\", \"name\": \"BattleTech\", \"order\": 4}], \"games\": [{\"key\": \"btech\", "
                             "\"family\": \"bt\", \"order\": 7, \"state\": \"released\", \"name\": \"BattleTech Rebuild\", \"title\": \"BATTLETECH\", "
                             "\"heading\": \"BATTLETECH\", \"button\": \"BATTLETECH\", \"description\": \"From "
                          << n << ".\", \"repo\": \"battletech-re\", \"tag_prefix\": \"btech-v\", \"program\": \"btech\", \"assets\": {\"windows\": "
                             "\"BT-{tag}-win64.zip\", \"mac\": \"BT-{tag}-mac.dmg\", \"linux\": \"BT-{tag}-x86_64.AppImage\"}, \"badge\": "
                             "\"badge_mw3.png\", \"min_launcher\": \"1.5.2\"}]}";
    fs::remove(src(n) + ".sig", ec);
    if (seeds.size()) sign_fixture(src(n), seeds);
  };
  const std::string sources = dir + "/sources.json";
  auto run = [&](const char* name, const std::string& wd, const char* sources_file = nullptr) {
    std::vector<std::pair<std::string, std::string>> env{
        {"MW3_GAMES_API", "file://" + dir + "/api/{repo}.json"}, {"MW3_GAMES_DIR", dir + "/games"}, {"MW3_LAUNCHER_FRAMES", "30"},
        {"MW3_NO_IMPORT", "1"}, {"MW3_PM_BIN", ""}, {"MW3_MW1_BIN", ""}, {"MW3_MW1R_BIN", ""}, {"MW3_CHI_BIN", ""}, {"MW3_CHIRM_BIN", ""},
        {"MW3_SOURCES_FILE", sources_file ? sources_file : sources}};
    fs::create_directories(wd, ec);
    for (const auto& [k, v] : env) setenv(k.c_str(), v.c_str(), 1);
    const pid_t pid = spawn_launcher({"--write-dir", wd, "--window", "640x480", "--nosound", "--hidden"}, dir + "/" + name + ".log");
    for (const auto& [k, v] : env) unsetenv(k.c_str());
    int status = 0;
    EXPECT(waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    const std::string log = read(dir + "/" + name + ".log");
    for (size_t a = 0; (a = log.find("catalog at ", a)) != std::string::npos; ++a)
      std::printf("  %s: %s\n", name, log.substr(a + 11, log.find('\n', a) - a - 11).c_str());
    return log;
  };
  const auto cached = [&](const std::string& wd) { return read(wd + "/catalog/games.json"); };
  int fresh = 0;
  const auto wd = [&]() { return dir + "/wd" + std::to_string(++fresh); };
  write_sources(sources, {url("a"), url("b"), url("c")}, {1});
  // In order, each failing in turn; all three answering, the first (a tie).
  put("a", 2, {1}), put("b", 2, {1}), put("c", 2, {1});
  std::string w = wd(), log = run("all", w);
  EXPECT(cached(w) == read(src("a")) && read(w + "/catalog/games.json.sig") == read(src("a") + ".sig"));
  EXPECT(log.find("catalog at " + url("a")) < log.find("catalog at " + url("b")) && log.find("catalog at " + url("b")) < log.find("catalog at " + url("c")));
  fs::remove(src("a"), ec);
  w = wd(), run("no-a", w);
  EXPECT(cached(w) == read(src("b")));
  fs::remove(src("b"), ec);
  w = wd(), run("no-b", w);
  EXPECT(cached(w) == read(src("c")));
  fs::remove(src("c"), ec);
  w = wd(), log = run("none", w);
  EXPECT(!fs::exists(w + "/catalog/games.json") && has(log, "catalog: the built-in one (revision 1)"));
  put("a", 2, {1}), put("b", 3, {1});  // the newest wins (one may lag)
  w = wd(), run("newest", w);
  EXPECT(cached(w) == read(src("b")));
  // Signatures: none, another key's, another text's, not one; each passed over for b.
  put("b", 2, {1});
  put("a", 2, {});
  w = wd(), log = run("unsigned", w);
  EXPECT(cached(w) == read(src("b")) && has(log, ("catalog at " + url("a") + ": not used: no signature").c_str()));
  put("a", 2, {9});
  w = wd(), log = run("other-key", w);
  EXPECT(cached(w) == read(src("b")) && has(log, ("catalog at " + url("a") + ": not used: its signature does not check").c_str()));
  put("a", 2, {1});
  std::ofstream(src("a"), std::ios::app) << " ";  // its text changed after signing
  w = wd(), run("changed", w);
  EXPECT(cached(w) == read(src("b")));
  put("a", 2, {1});
  std::ofstream(src("a") + ".sig") << "not a signature\n";
  w = wd(), run("garbage", w);
  EXPECT(cached(w) == read(src("b")));
  // Key rotation: a (key 1) names key 2 next; a signed by key 2 alone is then taken there, not in a fresh write dir;
  // signed by both, anywhere.
  fs::remove(src("b"), ec), fs::remove(src("c"), ec);
  put("a", 2, {1}, ", \"next_keys\": [\"" + test_keys::public_key(2) + "\"]");
  const std::string rot = wd();
  log = run("rotate-1", rot);
  EXPECT(cached(rot) == read(src("a")) && has(log, "catalog: its keys taken (1 next, 0 revoked)") && read(rot + "/catalog/keys.txt") == test_keys::public_key(2) + "\n");
  put("a", 3, {2});
  log = run("rotate-2", rot);
  EXPECT(cached(rot) == read(src("a")) && has(log, "catalog: revision 3 cached"));
  w = wd(), log = run("rotate-fresh", w);
  EXPECT(!fs::exists(w + "/catalog/games.json") && has(log, "its signature does not check"));
  put("a", 3, {2, 1});
  w = wd(), run("rotate-both", w);
  EXPECT(cached(w) == read(src("a")));
  // The catalog's own addresses: a names d; on the next start d is asked first, and its newer catalog taken.
  put("a", 2, {1}, ", \"catalog_sources\": [\"" + url("d") + "\"]");
  const std::string follow = wd();
  log = run("follow-1", follow);  // said after the fetch: the cached catalog's address first
  EXPECT(cached(follow) == read(src("a")) && has(log, ("catalog: asked at " + url("d") + ", " + url("a") + ", " + url("b") + ", " + url("c") + "\n").c_str()));
  put("d", 4, {1});
  log = run("follow-2", follow);
  EXPECT(cached(follow) == read(src("d")) && log.find("catalog at " + url("d")) < log.find("catalog at " + url("a")));
  // An older launcher's cache (1.7.1 and before fetched it unsigned, no games.json.sig): not used at the start, and
  // replaced by a signed catalog of the same revision, or a lower one.
  fs::remove(src("d"), ec), fs::remove(src("a"), ec);
  for (int old_rev : {3, 5}) {
    const std::string old = wd();
    fs::create_directories(old + "/catalog", ec);
    put("old", old_rev, {});
    fs::copy_file(src("old"), old + "/catalog/games.json", fs::copy_options::overwrite_existing, ec);
    fs::remove(src("a"), ec);
    log = run(("unsigned-cache-" + std::to_string(old_rev)).c_str(), old);
    EXPECT(has(log, "catalog: the cache is not signed: not used\n") && has(log, "catalog: the built-in one (revision 1)") && cached(old) == read(src("old")));
    put("a", 3, {1});
    log = run(("unsigned-cache-" + std::to_string(old_rev) + "-signed").c_str(), old);
    EXPECT(cached(old) == read(src("a")) && read(old + "/catalog/games.json.sig") == read(src("a") + ".sig") && has(log, "catalog: revision 3 cached") &&
           has(log, "catalog: revision 3, fetched"));
  }
  // Revocation: a (key 1) names key 2; a (key 2) revokes key 1, the shipped one: a catalog by key 1 alone is then
  // refused there, and key 1 can no longer vouch for a key 3. Then a (keys 2 and 3, the next named) revokes key 2,
  // a learned one: key 2 alone refused, key 3 taken. The revocations kept across starts (keys.txt).
  const std::string rev = wd();
  put("a", 2, {1}, ", \"next_keys\": [\"" + test_keys::public_key(2) + "\"]");
  run("revoke-1", rev);
  put("a", 3, {2}, ", \"revoked_keys\": [\"" + test_keys::public_key(1) + "\"]");
  log = run("revoke-2", rev);
  EXPECT(cached(rev) == read(src("a")) && has(log, "catalog: its keys taken (0 next, 1 revoked)"));
  EXPECT(read(rev + "/catalog/keys.txt") == test_keys::public_key(2) + "\nrevoked " + test_keys::public_key(1) + "\n");
  const std::string by_two = read(src("a"));
  put("a", 4, {1}, ", \"next_keys\": [\"" + test_keys::public_key(3) + "\"]");  // the leaked key minting a successor
  log = run("revoke-3", rev);
  EXPECT(cached(rev) == by_two && has(log, ("catalog at " + url("a") + ": not used: its signature does not check").c_str()));
  EXPECT(read(rev + "/catalog/keys.txt") == test_keys::public_key(2) + "\nrevoked " + test_keys::public_key(1) + "\n");
  put("a", 4, {2, 3}, ", \"next_keys\": [\"" + test_keys::public_key(3) + "\"], \"revoked_keys\": [\"" + test_keys::public_key(2) + "\"]");
  log = run("revoke-4", rev);
  EXPECT(cached(rev) == read(src("a")) && has(log, "catalog: its keys taken (1 next, 1 revoked)") && !has(log, "the cache is not signed"));
  put("a", 5, {2});
  log = run("revoke-5", rev);
  EXPECT(!has(log, "catalog: revision 5 cached") && has(log, "catalog: revision 4, fetched"));  // its cache, signed by key 3, still in use
  put("a", 5, {3});
  log = run("revoke-6", rev);
  EXPECT(has(log, "catalog: revision 5 cached"));
  // Art (2026-11-26): a badge the launcher does not carry is fetched from art/ beside the catalog only with the
  // catalog's badge_sha256, and kept only when it is that; a wrong PNG not kept, one with no sum never asked for; art
  // an older launcher cached unchecked removed at the start. The catalog and .sig asked with no-cache headers.
  fs::remove(src("b"), ec), fs::remove(src("c"), ec), fs::remove(src("d"), ec);
  fs::create_directories(dir + "/src/art", ec);
  const std::string good_png = std::string(MW3_ASSETS_FOR_TESTS) + "/launcher/badge_mw3.png", other_png = std::string(MW3_ASSETS_FOR_TESTS) + "/launcher/badge_pm.png";
  const std::string good_sum = rekit::sha256_file_hex(good_png);
  const auto put_badge = [&](int revision, const std::string& sum) {
    put("a", revision, {});
    std::string text = read(src("a"));
    const std::string at = "\"badge\": \"badge_mw3.png\"";
    text.replace(text.find(at), at.size(), "\"badge\": \"badge_new.png\"" + (sum.empty() ? std::string() : ", \"badge_sha256\": \"" + sum + "\""));
    std::ofstream(src("a"), std::ios::binary) << text;
    sign_fixture(src("a"), {1});
  };
  fs::copy_file(good_png, dir + "/src/art/badge_new.png", fs::copy_options::overwrite_existing, ec);
  put_badge(2, good_sum);
  const std::string art = wd();
  const std::string args_file = dir + "/curl-args.txt";
  setenv("MW3_CURL_ARGS_TO", args_file.c_str(), 1);
  log = run("art-good", art);
  unsetenv("MW3_CURL_ARGS_TO");
  EXPECT(rekit::sha256_file_hex(art + "/catalog/badge_new.png") == good_sum && !has(log, "art badge_new.png"));
  {
    const std::string a = read(args_file);
    const auto asked_fresh = [&](const std::string& u) {  // each request's arguments end with its address
      const size_t at = a.find("\n" + u + "\n");
      const size_t from = at == std::string::npos ? at : a.rfind("\nMW3-Rebuild-launcher\n", at);
      return at != std::string::npos && from != std::string::npos &&
             a.substr(from, at - from).find("\n--header\nCache-Control: no-cache\n--header\nPragma: no-cache\n") != std::string::npos;
    };
    EXPECT(asked_fresh(url("a")) && asked_fresh(url("a") + ".sig") && !asked_fresh("file://" + dir + "/src/art/badge_new.png"));
  }
  fs::copy_file(other_png, dir + "/src/art/badge_new.png", fs::copy_options::overwrite_existing, ec);  // another PNG served
  w = wd(), log = run("art-wrong", w);
  EXPECT(has(log, "catalog: art badge_new.png not kept: not the catalog's sha256") && !fs::exists(w + "/catalog/badge_new.png") &&
         has(log, "catalog: revision 2 cached"));
  put_badge(2, "");
  w = wd(), log = run("art-no-sum", w);
  EXPECT(has(log, "catalog: art badge_new.png not fetched: the catalog gives no sha256 for it") && !fs::exists(w + "/catalog/badge_new.png"));
  put_badge(3, good_sum);
  fs::copy_file(other_png, art + "/catalog/badge_new.png", fs::copy_options::overwrite_existing, ec);  // an unchecked one in the cache
  fs::remove(dir + "/src/art/badge_new.png", ec);
  log = run("art-cached", art);
  EXPECT(has(log, "catalog: art badge_new.png in the cache: not its sha256: removed") && !fs::exists(art + "/catalog/badge_new.png"));
  // sources.json missing: the built-in copy (the release key; no test key), nothing taken, its real addresses not
  // asked.
  w = wd(), log = run("builtin", w, (dir + "/missing-sources.json").c_str());
  EXPECT(has(log, ("launcher: sources: the built-in copy (" + dir + "/missing-sources.json: it is not there)\n").c_str()));
  EXPECT(!has(log, "catalog: sources.json lists no key") && !fs::exists(w + "/catalog/games.json"));
  EXPECT(has(log, "catalog at https://reverse-engineered.com/launcher/catalog.json: not asked in a check run"));
  if (!check::failures) fs::remove_all(dir, ec);
}

// A game's original files fetched after its DOWNLOAD (2026-11-26, docs/launcher.md, Games on demand): its catalog
// entry lists sources, its status (a stand-in, setup served) says its files are missing, so the first source that is
// whole and its SHA-256 is downloaded and handed to its setup (--look, then --install the file), no question asked;
// the file and the launcher's temp folder are gone afterwards. A wrong sum, a cut file, a missing one (the next tried),
// a setup refusing the file: the game installed as before, the reason said and logged. No "original": nothing fetched.
TEST(sdl_program_launcher_gets_a_games_original_files) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const std::string base = std::string(MW3_TEST_TMP) + "/launcher_original";
  fs::remove_all(base, ec);
  const std::string good(300000, 'o'), other(300000, 'x');
  const std::string good_sum = rekit::sha256_hex(reinterpret_cast<const uint8_t*>(good.data()), good.size());
  const std::string other_sum = rekit::sha256_hex(reinterpret_cast<const uint8_t*>(other.data()), other.size());
  struct Case {
    const char* name;
    std::string sources;  // the entry's "original", "" for none
    bool refuse;          // its setup says no to the file
    bool set_up;          // the files in, through its setup
    const char* said;     // in the log
    bool foreign = false;  // the temp folder's name taken by someone's own folder (no mark): never written to
    bool ready = false;    // its files in already: nothing fetched
  };
  auto src = [&](const std::string& file, const std::string& sum, size_t size) {
    return "{\"url\": \"file://" + base + "/src/" + file + "\", \"sha256\": \"" + sum + "\", \"size\": " + std::to_string(size) + ", \"name\": \"BT disc.zip\"}";
  };
  const Case cases[] = {
      {"good", "[" + src("good.zip", good_sum, good.size()) + "]", false, true, "original: btech: set up at "},
      {"sum", "[" + src("good.zip", other_sum, good.size()) + "]", false, false, "it does not match its SHA-256"},
      {"cut", "[" + src("good.zip", good_sum, good.size() * 2) + "]", false, false, "bytes, not 600000"},
      {"next", "[" + src("gone.zip", good_sum, good.size()) + ", " + src("good.zip", good_sum, good.size()) + "]", false, true, "original: btech: set up at "},
      {"refused", "[" + src("good.zip", good_sum, good.size()) + "]", true, false, "its setup did not install the file: Not a copy of BattleTech."},
      {"none", "", false, false, ""},
      {"foreign", "[" + src("good.zip", good_sum, good.size()) + "]", false, false, "MW3-Rebuild-originals is not the launcher's folder", true},
      {"ready", "[" + src("good.zip", good_sum, good.size()) + "]", false, true, "original: btech: not fetched: it has its game files", false, true},
  };
  for (const Case& c : cases) {
    const std::string dir = base + "/" + c.name;
    for (const char* d : {"/api", "/games", "/wd/music", "/tmp", "/rel", "/pkg"}) fs::create_directories(dir + d, ec);
    fs::create_directories(base + "/src", ec);
    std::ofstream(base + "/src/good.zip", std::ios::binary) << good;
    std::ofstream(dir + "/wd/music/track02.wav") << "x";
    for (const char* repo : {"mech3-pm-re", "mech1-remake-re", "mech1-re", "inception-re", "inception-remake-re", "revenge-re"})
      std::ofstream(dir + "/api/" + repo + ".json") << "[]";
    if (c.foreign) fs::create_directories(dir + "/tmp/MW3-Rebuild-originals", ec), std::ofstream(dir + "/tmp/MW3-Rebuild-originals/mine.txt") << "mine";
    if (c.ready) std::ofstream(dir + "/files-in") << "x";
    // The stand-in game, as its release's .zip holds it.
    std::string sh = "#!/bin/sh\nR='" + dir + "'\necho \"$*\" >> \"$R/asked\"\ncase \"$1 $2\" in\n"
                     "'--status ') if [ -f \"$R/files-in\" ]; then ST=ready; else ST=missing; fi\n"
                     "  echo '{\"kit\": 1, \"game\": \"btech\", \"version\": \"0.1.0\", \"files\": {\"state\": \"'$ST'\"}, \"first_run\": {\"pending\": []}, "
                     "\"needs\": [], \"setup\": 1}';;\n"
                     "'--setup --look') if [ -f \"$3\" ]; then echo '{\"done\": true, \"ok\": true, \"words\": \"A copy of BattleTech.\", \"into\": \"'\"$R\"'/files\"}';"
                     " else echo '{\"done\": true, \"ok\": false, \"summary\": \"No such file.\"}'; fi;;\n"
                     "'--setup --install') cp \"$3\" \"$R/handed\"\n";
    sh += c.refuse ? "  echo '{\"done\": true, \"ok\": false, \"summary\": \"Not a copy of BattleTech.\"}';;\n"
                   : "  : > \"$R/files-in\"; echo '{\"done\": false, \"at\": 500000, \"stage\": \"Copying\", \"message\": \"\"}'\n"
                     "  echo '{\"done\": true, \"ok\": true, \"where\": \"'\"$R\"'/files\", \"summary\": \"BattleTech is set up.\"}';;\n";
    sh += "*) : > \"$R/played\";;\nesac\nexit 0\n";
    {
      archive* a = archive_write_new();
      archive_write_set_format_zip(a);
      archive_write_open_filename(a, (dir + "/rel/BT-btech-v0.1.0-test.zip").c_str());
      archive_entry* e = archive_entry_new();
      archive_entry_set_pathname(e, "btech");
      archive_entry_set_size(e, static_cast<la_int64_t>(sh.size()));
      archive_entry_set_filetype(e, AE_IFREG);
      archive_entry_set_perm(e, 0755);
      archive_write_header(a, e);
      archive_write_data(a, sh.data(), sh.size());
      archive_entry_free(e);
      archive_write_close(a);
      archive_write_free(a);
    }
    const std::string zip = dir + "/rel/BT-btech-v0.1.0-test.zip";
    std::ifstream zf(zip, std::ios::binary);
    const std::string zbytes((std::istreambuf_iterator<char>(zf)), std::istreambuf_iterator<char>());
    std::ofstream(dir + "/rel/SHA256SUMS") << rekit::sha256_hex(reinterpret_cast<const uint8_t*>(zbytes.data()), zbytes.size()) << "  BT-btech-v0.1.0-test.zip\n";
    std::ofstream(dir + "/api/battletech-re.json")
        << "[{\"tag_name\": \"btech-v0.1.0\", \"draft\": false, \"prerelease\": false, \"body\": \"x\", \"assets\": [{\"name\": \"BT-btech-v0.1.0-test.zip\", "
           "\"size\": " << zbytes.size() << ", \"browser_download_url\": \"file://" << zip << "\"}, {\"name\": \"SHA256SUMS\", \"size\": 10, "
           "\"browser_download_url\": \"file://" << dir << "/rel/SHA256SUMS\"}]}]";
    std::ofstream(dir + "/catalog.json")
        << "{\"catalog\": 1, \"revision\": 4, \"families\": [{\"key\": \"bt\", \"name\": \"BattleTech\", \"order\": 4}], \"games\": [{\"key\": \"btech\", "
           "\"family\": \"bt\", \"order\": 7, \"state\": \"released\", \"name\": \"BattleTech Rebuild\", \"title\": \"BATTLETECH\", \"heading\": "
           "\"BATTLETECH\", \"button\": \"BATTLETECH\", \"description\": \"A line.\", \"repo\": \"battletech-re\", \"tag_prefix\": \"btech-v\", "
           "\"program\": \"btech\", \"assets\": {\"windows\": \"BT-{tag}-test.zip\", \"mac\": \"BT-{tag}-test.zip\", \"linux\": \"BT-{tag}-test.zip\"}, "
           "\"badge\": \"badge_mw3.png\", \"min_launcher\": \"1.5.2\""
        << (c.sources.empty() ? std::string() : ", \"original\": " + c.sources) << "}]}";
    const std::vector<std::pair<std::string, std::string>> env{
        {"MW3_GAMES_API", "file://" + dir + "/api/{repo}.json"}, {"MW3_GAMES_DIR", dir + "/games"}, {"MW3_CATALOG_URL", "file://" + dir + "/catalog.json"},
        {"MW3_LAUNCHER_FRAMES", "600"}, {"MW3_LAUNCHER_GAME_INSTALL", "btech"}, {"MW3_NO_IMPORT", "1"}, {"TMPDIR", dir + "/tmp"}, {"MW3_PM_BIN", ""},
        {"MW3_MW1_BIN", ""}, {"MW3_MW1R_BIN", ""}, {"MW3_CHI_BIN", ""}, {"MW3_CHIRM_BIN", ""}};
    const char* tmp_was = std::getenv("TMPDIR");
    const std::string tmp_keep = tmp_was ? tmp_was : "";
    for (const auto& [k, v] : env) setenv(k.c_str(), v.c_str(), 1);
    const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--window", "640x480", "--nosound", "--hidden"}, dir + "/launcher.log");
    for (const auto& [k, v] : env) unsetenv(k.c_str());
    if (tmp_was) setenv("TMPDIR", tmp_keep.c_str(), 1);
    int status = 0;
    EXPECT(waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    std::ifstream lf(dir + "/launcher.log");
    const std::string log((std::istreambuf_iterator<char>(lf)), std::istreambuf_iterator<char>());
    std::ifstream af(dir + "/asked");
    const std::string asked((std::istreambuf_iterator<char>(af)), std::istreambuf_iterator<char>());
    std::ifstream hf(dir + "/handed", std::ios::binary);
    const std::string handed((std::istreambuf_iterator<char>(hf)), std::istreambuf_iterator<char>());
    std::printf("original, %s:\n", c.name);
    for (const char* what : {"original: ", "games: btech now", "focus ", "BattleTech is set up", "could not be fetched"})
      for (size_t a = 0; (a = log.find(what, a)) != std::string::npos; ++a) std::printf("    %s\n", log.substr(a, log.find('\n', a) - a).c_str());
    EXPECT(has(log, "games: btech now btech-v0.1.0 at "));  // the game itself installed every time
    if (c.foreign) EXPECT(fs::exists(dir + "/tmp/MW3-Rebuild-originals/mine.txt", ec) && !fs::exists(dir + "/tmp/MW3-Rebuild-originals/BT disc.zip", ec));
    else EXPECT(!fs::exists(dir + "/tmp/MW3-Rebuild-originals", ec));  // the download and its folder gone
    if (c.sources.empty() || c.ready) {
      EXPECT(has(log, c.said) && !has(log, "GETTING THE ORIGINAL") && !has(asked, "--setup"));
    } else {
      EXPECT(has(log, c.said) && has(log, "work: GETTING THE ORIGINAL GAME FILES"));
      EXPECT(fs::exists(dir + "/files-in", ec) == c.set_up);
      const bool handed_it = c.set_up || c.refuse;
      EXPECT(handed_it ? handed == good && has(asked, ("--setup --look " + dir + "/tmp/MW3-Rebuild-originals/BT disc.zip").c_str())
                       : !has(asked, "--setup --install"));
      if (c.set_up) EXPECT(has(log, "BattleTech is set up."));
      else EXPECT(has(log, "BattleTech Rebuild is installed. Its original game files could not be fetched ("));
    }
    EXPECT(!fs::exists(dir + "/played", ec));  // DOWNLOAD never starts the game
  }
  if (!check::failures) fs::remove_all(base, ec);
}

// A DOWNLOAD whose game needs another not installed (2.0.1, docs/launcher.md, A game's needs): asked once with how much
// may come ("Up to": the packages and the need's original files, its game files not known), then the need installed
// as its own DOWNLOAD would be (its package, its original files through its setup), then the game, one job. Not
// pressed: nothing installed. CANCEL while the need's original files come: the need stays, the game is not installed.
TEST(sdl_program_launcher_installs_what_a_game_needs) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const std::string base = std::string(MW3_TEST_TMP) + "/launcher_needs";
  fs::remove_all(base, ec);
  const std::string good(300000, 'o');
  const std::string good_sum = rekit::sha256_hex(reinterpret_cast<const uint8_t*>(good.data()), good.size());
  auto read = [](const std::string& f) {
    std::ifstream in(f, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  };
  std::atomic<bool> stop{false};
  auto feed = [&stop](const std::string& path, const std::string& bytes, size_t chunk, int ms) {  // a slow server (FIFO)
    return std::thread([&stop, path, bytes, chunk, ms] {
      int fd = -1;
      while (!stop && (fd = ::open(path.c_str(), O_WRONLY | O_NONBLOCK)) < 0) std::this_thread::sleep_for(std::chrono::milliseconds(20));
      if (fd < 0) return;
      ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) & ~O_NONBLOCK);
      for (size_t at = 0; at < bytes.size() && !stop; at += chunk) {
        if (::write(fd, bytes.data() + at, std::min(chunk, bytes.size() - at)) < 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
      }
      ::close(fd);
    });
  };
  const auto pipe_was = std::signal(SIGPIPE, SIG_IGN);
  struct Case {
    const char* name;
    bool go;      // INSTALL ALL pressed
    bool cancel;  // CANCEL while the need's original files come (served slowly)
  };
  for (const Case& c : {Case{"all", true, false}, Case{"not_pressed", false, false}, Case{"cancel", true, true}}) {
    const std::string dir = base + "/" + c.name;
    for (const char* d : {"/api", "/games", "/wd/music", "/tmp", "/rel"}) fs::create_directories(dir + d, ec);
    std::ofstream(dir + "/wd/music/track02.wav") << "x";
    for (const char* repo : {"mech3-pm-re", "mech1-remake-re", "mech1-re", "inception-re", "inception-remake-re", "revenge-re"})
      std::ofstream(dir + "/api/" + repo + ".json") << "[]";
    // Each stand-in game in its release's .zip: its status (files in or not), its setup taking the original files.
    uint64_t packages = 0;
    for (const char* key : {"btneed", "btech"}) {
      const std::string k = key, repo = k == "btech" ? "battletech-re" : "btneed-re";
      const std::string sh = "#!/bin/sh\nR='" + dir + "'\necho \"" + k + " $*\" >> \"$R/asked\"\ncase \"$1 $2\" in\n"
                             "'--status ') if [ -f \"$R/" + k + "-in\" ]; then ST=ready; else ST=missing; fi\n"
                             "  echo '{\"kit\": 1, \"game\": \"" + k + "\", \"version\": \"0.1.0\", \"files\": {\"state\": \"'$ST'\", \"where\": \"'\"$R\"'/" + k +
                             "\"}, \"first_run\": {\"pending\": []}, \"needs\": [], \"setup\": 1}';;\n"
                             "'--setup --look') echo '{\"done\": true, \"ok\": true, \"words\": \"A copy.\", \"into\": \"'\"$R\"'/" + k + "\"}';;\n"
                             "'--setup --install') : > \"$R/" + k + "-in\"; echo '{\"done\": true, \"ok\": true, \"where\": \"'\"$R\"'/" + k +
                             "\", \"summary\": \"" + k + " is set up.\"}';;\n*) : > \"$R/played\";;\nesac\nexit 0\n";
      const std::string zip = dir + "/rel/" + k + "-v0.1.0-test.zip";
      archive* a = archive_write_new();
      archive_write_set_format_zip(a);
      archive_write_open_filename(a, zip.c_str());
      archive_entry* e = archive_entry_new();
      archive_entry_set_pathname(e, key);
      archive_entry_set_size(e, static_cast<la_int64_t>(sh.size()));
      archive_entry_set_filetype(e, AE_IFREG);
      archive_entry_set_perm(e, 0755);
      archive_write_header(a, e);
      archive_write_data(a, sh.data(), sh.size());
      archive_entry_free(e);
      archive_write_close(a);
      archive_write_free(a);
      const std::string zbytes = read(zip);
      packages += zbytes.size();
      std::ofstream(dir + "/rel/" + k + "-SHA256SUMS") << rekit::sha256_hex(reinterpret_cast<const uint8_t*>(zbytes.data()), zbytes.size()) << "  " << k << "-v0.1.0-test.zip\n";
      std::ofstream(dir + "/api/" + repo + ".json")
          << "[{\"tag_name\": \"" << k << "-v0.1.0\", \"draft\": false, \"prerelease\": false, \"body\": \"x\", \"assets\": [{\"name\": \"" << k
          << "-v0.1.0-test.zip\", \"size\": " << zbytes.size() << ", \"browser_download_url\": \"file://" << zip << "\"}, {\"name\": \"SHA256SUMS\", "
          << "\"size\": 10, \"browser_download_url\": \"file://" << dir << "/rel/" << k << "-SHA256SUMS\"}]}]";
    }
    const std::string src = dir + "/need-disc.zip";
    std::thread server;
    if (c.cancel) {
      EXPECT(::mkfifo(src.c_str(), 0600) == 0);
      server = feed(src, good, 4096, 150);
    } else {
      std::ofstream(src, std::ios::binary) << good;
    }
    auto game = [](const char* key, const char* name, const char* repo, int order, const std::string& more) {
      return std::string("{\"key\": \"") + key + "\", \"family\": \"bt\", \"order\": " + std::to_string(order) + ", \"state\": \"released\", \"name\": \"" + name +
             "\", \"title\": \"T\", \"heading\": \"H\", \"button\": \"B\", \"description\": \"A line.\", \"repo\": \"" + repo + "\", \"tag_prefix\": \"" + key +
             "-v\", \"program\": \"" + key + "\", \"assets\": {\"windows\": \"{tag}-test.zip\", \"mac\": \"{tag}-test.zip\", \"linux\": \"{tag}-test.zip\"}, \"badge\": \"badge_mw3.png\", \"min_launcher\": \"1.5.2\"" + more + "}";
    };
    std::ofstream(dir + "/catalog.json")
        << "{\"catalog\": 1, \"revision\": 4, \"families\": [{\"key\": \"bt\", \"name\": \"BattleTech\", \"order\": 4}], \"games\": ["
        << game("btneed", "Need Rebuild", "btneed-re", 7,
                ", \"original\": [{\"url\": \"file://" + src + "\", \"sha256\": \"" + good_sum + "\", \"size\": 300000, \"name\": \"need disc.zip\"}]")
        << ", " << game("btech", "BattleTech Rebuild", "battletech-re", 8, ", \"needs\": [\"btneed\"]") << "]}";
    std::vector<std::pair<std::string, std::string>> env{
        {"MW3_GAMES_API", "file://" + dir + "/api/{repo}.json"}, {"MW3_GAMES_DIR", dir + "/games"}, {"MW3_CATALOG_URL", "file://" + dir + "/catalog.json"},
        {"MW3_LAUNCHER_FRAMES", c.cancel ? "900" : "600"}, {"MW3_LAUNCHER_GAME_INSTALL", "btech"}, {"MW3_NO_IMPORT", "1"}, {"TMPDIR", dir + "/tmp"},
        {"MW3_PM_BIN", ""}, {"MW3_MW1_BIN", ""}, {"MW3_MW1R_BIN", ""}, {"MW3_CHI_BIN", ""}, {"MW3_CHIRM_BIN", ""}};
    if (c.go) env.push_back({"MW3_LAUNCHER_NEEDS_GO", "1"});
    if (c.cancel) env.push_back({"MW3_LAUNCHER_CANCEL_AT", "original:btneed"});
    const char* tmp_was = std::getenv("TMPDIR");
    const std::string tmp_keep = tmp_was ? tmp_was : "";
    for (const auto& [k, v] : env) setenv(k.c_str(), v.c_str(), 1);
    const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--window", "640x480", "--nosound", "--hidden"}, dir + "/launcher.log");
    for (const auto& [k, v] : env) unsetenv(k.c_str());
    if (tmp_was) setenv("TMPDIR", tmp_keep.c_str(), 1);
    int status = 0;
    EXPECT(waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    stop = true;
    if (server.joinable()) server.join();
    stop = false;
    const std::string log = read(dir + "/launcher.log"), asked = read(dir + "/asked");
    std::printf("needs, %s:\n", c.name);
    for (const char* what : {"needs: ", "job: BattleTech", "games: bt", "original: bt", "CANCEL pressed"})
      for (size_t a = 0; (a = log.find(what, a)) != std::string::npos; ++a) std::printf("    %s\n", log.substr(a, log.find('\n', a) - a).c_str());
    char up_to[64];
    std::snprintf(up_to, sizeof up_to, "Up to %.1f MB.", static_cast<double>(packages + good.size()) / 1e6);
    EXPECT(has(log, (std::string("needs: BattleTech Rebuild needs Need Rebuild. Install both? ") + up_to).c_str()));
    const size_t need_at = log.find("games: btneed now btneed-v0.1.0 at "), game_at = log.find("games: btech now btech-v0.1.0 at ");
    if (!c.go) {
      EXPECT(need_at == std::string::npos && game_at == std::string::npos && !has(log, "job: "));
    } else if (!c.cancel) {
      EXPECT(need_at != std::string::npos && game_at != std::string::npos && need_at < game_at);  // the need first
      EXPECT(has(log, "original: btneed: set up at ") && fs::exists(dir + "/btneed-in", ec));
      // One job for the press: the need's package, its original files and their setup, then the game.
      EXPECT(has(log, "job: BattleTech Rebuild: game:btneed ") && has(log, ", original:btneed 0.3 MB, setup:btneed, game:btech "));
      size_t jobs = 0;
      for (size_t a = 0; (a = log.find(" in all)\n", a)) != std::string::npos; ++a) ++jobs;  // each job's first line
      EXPECT(jobs == 1 && !has(log, "job: Need Rebuild: "));
    } else {
      EXPECT(has(log, "CANCEL pressed during original:btneed") && need_at != std::string::npos && game_at == std::string::npos);
      EXPECT(has(log, "original: btneed: not set up: ") && !fs::exists(dir + "/btneed-in", ec));
    }
    EXPECT(!fs::exists(dir + "/played", ec));  // DOWNLOAD never starts a game
  }
  std::signal(SIGPIPE, pipe_was);
  if (!check::failures) fs::remove_all(base, ec);
}

// One bar for a DOWNLOAD (2026-11-27, docs/launcher.md, One bar for a press), served slowly (FIFOs fed at a pace):
// the game's .zip and its original files (the catalog's size and note) one rising total, each part named on the smaller
// line, the speed once settled, then the setup; a status that says the game has its files leaves the original out
// (said, the total smaller); CANCEL while the original comes keeps the game installed and leaves no .part.
TEST(sdl_program_launcher_shows_one_bar_for_a_download) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const std::string base = std::string(MW3_TEST_TMP) + "/launcher_one_bar";
  fs::remove_all(base, ec);
  fs::create_directories(base + "/src", ec);
  const std::string good(300000, 'o');
  const std::string good_sum = rekit::sha256_hex(reinterpret_cast<const uint8_t*>(good.data()), good.size());
  std::ofstream(base + "/src/good.zip", std::ios::binary) << good;
  std::string pad(200000, '\0');  // the game's .zip about 0.2 MB: bytes that do not compress
  uint32_t x = 12345;
  for (char& c : pad) x = x * 1103515245u + 12345u, c = static_cast<char>(x >> 24);
  auto read = [](const std::string& f) {
    std::ifstream in(f, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  };
  struct Run {
    std::string log;
    bool exited = false;
    std::string dir;
  };
  // A slow server: `bytes` into the FIFO `path`, `chunk` at a time every `ms`, once a reader opens it (curl's file://).
  std::atomic<bool> stop{false};
  auto feed = [&stop](const std::string& path, const std::string& bytes, size_t chunk, int ms) {
    return std::thread([&stop, path, bytes, chunk, ms] {
      int fd = -1;
      while (!stop && (fd = ::open(path.c_str(), O_WRONLY | O_NONBLOCK)) < 0) std::this_thread::sleep_for(std::chrono::milliseconds(20));
      if (fd < 0) return;
      ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) & ~O_NONBLOCK);
      for (size_t at = 0; at < bytes.size() && !stop; at += chunk) {
        if (::write(fd, bytes.data() + at, std::min(chunk, bytes.size() - at)) < 0) break;  // its reader gone (a CANCEL)
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
      }
      ::close(fd);
    });
  };
  const auto pipe_was = std::signal(SIGPIPE, SIG_IGN);  // a CANCEL closes the reading end
  auto run = [&](const char* name, bool has_files, int original_ms, const char* cancel_at) {
    Run r;
    const std::string dir = r.dir = base + "/" + name;
    for (const char* d : {"/api", "/games", "/wd/music", "/tmp", "/rel"}) fs::create_directories(dir + d, ec);
    std::ofstream(dir + "/wd/music/track02.wav") << "x";
    if (has_files) std::ofstream(dir + "/files-in") << "x";
    for (const char* repo : {"mech3-pm-re", "mech1-remake-re", "mech1-re", "inception-re", "inception-remake-re", "revenge-re"})
      std::ofstream(dir + "/api/" + repo + ".json") << "[]";
    const std::string sh = "#!/bin/sh\nR='" + dir + "'\necho \"$*\" >> \"$R/asked\"\ncase \"$1 $2\" in\n"
                           "'--status ') if [ -f \"$R/files-in\" ]; then ST=ready; else ST=missing; fi\n"
                           "  echo '{\"kit\": 1, \"game\": \"btech\", \"version\": \"0.1.0\", \"files\": {\"state\": \"'$ST'\"}, \"first_run\": {\"pending\": []}, "
                           "\"needs\": [], \"setup\": 1}';;\n"
                           "'--setup --look') echo '{\"done\": true, \"ok\": true, \"words\": \"A copy of BattleTech.\", \"into\": \"'\"$R\"'/files\"}';;\n"
                           "'--setup --install') echo '{\"done\": false, \"at\": 500000, \"stage\": \"Copying the game files\", \"message\": \"\"}'; sleep 1\n"
                           "  : > \"$R/files-in\"; echo '{\"done\": true, \"ok\": true, \"where\": \"'\"$R\"'/files\", \"summary\": \"BattleTech is set up.\"}';;\n"
                           "*) : > \"$R/played\";;\nesac\nexit 0\n";
    const std::string zip = dir + "/rel/BT-btech-v0.1.0-test.zip";
    archive* a = archive_write_new();
    archive_write_set_format_zip(a);
    archive_write_open_filename(a, (zip + ".real").c_str());
    for (const auto& [path, bytes] : {std::pair<std::string, const std::string*>{"btech", &sh}, {"pad.bin", &pad}}) {
      archive_entry* e = archive_entry_new();
      archive_entry_set_pathname(e, path.c_str());
      archive_entry_set_size(e, static_cast<la_int64_t>(bytes->size()));
      archive_entry_set_filetype(e, AE_IFREG);
      archive_entry_set_perm(e, 0755);
      archive_write_header(a, e);
      archive_write_data(a, bytes->data(), bytes->size());
      archive_entry_free(e);
    }
    archive_write_close(a);
    archive_write_free(a);
    const std::string zbytes = read(zip + ".real");
    const std::string src = dir + "/good.zip";
    EXPECT(::mkfifo(zip.c_str(), 0600) == 0 && ::mkfifo(src.c_str(), 0600) == 0);
    std::ofstream(dir + "/rel/SHA256SUMS") << rekit::sha256_hex(reinterpret_cast<const uint8_t*>(zbytes.data()), zbytes.size()) << "  BT-btech-v0.1.0-test.zip\n";
    std::ofstream(dir + "/api/battletech-re.json")
        << "[{\"tag_name\": \"btech-v0.1.0\", \"draft\": false, \"prerelease\": false, \"body\": \"x\", \"assets\": [{\"name\": \"BT-btech-v0.1.0-test.zip\", "
           "\"size\": " << zbytes.size() << ", \"browser_download_url\": \"file://" << zip << "\"}, {\"name\": \"SHA256SUMS\", \"size\": 10, "
           "\"browser_download_url\": \"file://" << dir << "/rel/SHA256SUMS\"}]}]";
    std::ofstream(dir + "/catalog.json")
        << "{\"catalog\": 1, \"revision\": 4, \"families\": [{\"key\": \"bt\", \"name\": \"BattleTech\", \"order\": 4}], \"games\": [{\"key\": \"btech\", "
           "\"family\": \"bt\", \"order\": 7, \"state\": \"released\", \"name\": \"BattleTech Rebuild\", \"title\": \"BATTLETECH\", \"heading\": "
           "\"BATTLETECH\", \"button\": \"BATTLETECH\", \"description\": \"A line.\", \"repo\": \"battletech-re\", \"tag_prefix\": \"btech-v\", "
           "\"program\": \"btech\", \"assets\": {\"windows\": \"BT-{tag}-test.zip\", \"mac\": \"BT-{tag}-test.zip\", \"linux\": \"BT-{tag}-test.zip\"}, "
           "\"badge\": \"badge_mw3.png\", \"min_launcher\": \"1.5.2\", \"original\": [{\"url\": \"file://" << src << "\", \"sha256\": \""
        << good_sum << "\", \"size\": " << good.size() << ", \"name\": \"BT disc.zip\", \"note\": \"a test server\"}]}]}";
    std::vector<std::pair<std::string, std::string>> env{
        {"MW3_GAMES_API", "file://" + dir + "/api/{repo}.json"}, {"MW3_GAMES_DIR", dir + "/games"}, {"MW3_CATALOG_URL", "file://" + dir + "/catalog.json"},
        {"MW3_LAUNCHER_FRAMES", "1200"}, {"MW3_LAUNCHER_GAME_INSTALL", "btech"}, {"MW3_NO_IMPORT", "1"}, {"TMPDIR", dir + "/tmp"}, {"MW3_PM_BIN", ""}, {"MW3_MW1_BIN", ""}, {"MW3_MW1R_BIN", ""}, {"MW3_CHI_BIN", ""}, {"MW3_CHIRM_BIN", ""}};
    if (cancel_at) env.push_back({"MW3_LAUNCHER_CANCEL_AT", cancel_at});
    const char* tmp_was = std::getenv("TMPDIR");
    const std::string tmp_keep = tmp_was ? tmp_was : "";
    for (const auto& [k, v] : env) setenv(k.c_str(), v.c_str(), 1);
    stop = false;
    std::thread zip_feed = feed(zip, zbytes, 16000, 150), src_feed = feed(src, good, 16000, original_ms);
    const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--window", "640x480", "--nosound", "--hidden"}, dir + "/launcher.log");
    for (const auto& [k, v] : env) unsetenv(k.c_str());
    if (tmp_was) setenv("TMPDIR", tmp_keep.c_str(), 1);
    int status = 0;
    r.exited = waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    stop = true;
    zip_feed.join(), src_feed.join();
    r.log = read(dir + "/launcher.log");
    std::printf("one bar, %s:\n", name);
    std::string last;
    for (size_t at = 0; (at = r.log.find("launcher: job: ", at)) != std::string::npos; ++at) {
      const std::string l = r.log.substr(at + 15, r.log.find('\n', at) - at - 15);
      const size_t bar = l.find(" | ");
      const std::string part = bar == std::string::npos ? l : l.substr(bar);
      if (part != last) std::printf("    %s\n", l.c_str());
      last = part;
    }
    return r;
  };
  // Every logged line's total, its fraction never falling, the parts named.
  auto lines = [](const std::string& log) {
    std::vector<std::string> out;
    for (size_t at = 0; (at = log.find("launcher: job: ", at)) != std::string::npos; ++at) out.push_back(log.substr(at + 15, log.find('\n', at) - at - 15));
    return out;
  };
  {  // the game, then its original files: one total, rising
    const Run r = run("total", false, 300, nullptr);  // the original's 0.3 MB over some 6 s: seconds of speed past the settle
    const std::vector<std::string> ls = lines(r.log);
    double last = -1;
    bool rising = true, totals = true, speed = false;
    std::string zip_mb;
    for (const std::string& l : ls) {
      const size_t open = l.rfind(" ("), of = l.find(" of ");
      if (l.find(" | ") != std::string::npos && open != std::string::npos) {
        const double f = std::atof(l.c_str() + open + 2);
        if (f < last) std::printf("    fell: %s\n", l.c_str());
        rising = rising && f >= last, last = f;
      }
      if (l.rfind("BattleTech Rebuild: ", 0) == 0 && of != std::string::npos) totals = totals && l.compare(of, 13, " of 0.5 MB \xc2\xb7") == 0;
      speed = speed || l.find(" KB/s \xc2\xb7 ") != std::string::npos;
    }
    EXPECT(r.exited && has(r.log, "job: BattleTech Rebuild: game:btech 0.2 MB, original:btech 0.3 MB, setup:btech (0.5 MB in all)"));
    EXPECT(rising && totals && speed);
    EXPECT(has(r.log, " | the game (0.2 MB) (") && has(r.log, " | the original game files from a test server (0.3 MB) (") &&
           has(r.log, "BattleTech Rebuild: Copying the game files | setting up ("));
    EXPECT(has(r.log, "job: ended at 0.5 of 0.5 MB: ") && has(r.log, "(1.000)\n") && has(r.log, "BattleTech is set up.") && fs::exists(r.dir + "/files-in"));
  }
  {  // its status says it has its files: the original left out, said
    const Run r = run("has-files", true, 150, nullptr);
    EXPECT(r.exited && has(r.log, "job: original:btech left out: BattleTech Rebuild has its game files already: nothing more to download (0.2 MB in all)"));
    EXPECT(has(r.log, "job: ended at 0.2 of 0.2 MB: ") && has(r.log, "| BattleTech Rebuild has its game files already: nothing more to download (1.000)"));
    EXPECT(!has(r.log, "work: GETTING THE ORIGINAL GAME FILES"));
  }
  {  // CANCEL while the original comes: the game installed, no .part, nothing set up
    const Run r = run("cancel", false, 400, "original:btech");
    EXPECT(r.exited && has(r.log, "CANCEL pressed during original:btech") && has(r.log, "games: btech now btech-v0.1.0 at "));
    bool part_left = false;
    for (const auto& e : fs::recursive_directory_iterator(r.dir + "/tmp", ec)) part_left = part_left || e.path().extension() == ".part" || e.path().filename() == "BT disc.zip";
    EXPECT(!part_left && !fs::exists(r.dir + "/files-in") && fs::exists(r.dir + "/games"));
    EXPECT(has(r.log, "job: ended at ") && !has(r.log, "BattleTech is set up."));
  }
  std::signal(SIGPIPE, pipe_was);
  if (!check::failures) fs::remove_all(base, ec);
}

// A catalog fetched while the launcher is open (2026-11-27, docs/launcher.md, A catalog fetched while open): put in use
// as soon as it lands and checks, never one start later; the list on screen at once, the new cards swapped in. A
// stand-in serves a newer catalog (signed by test key 1) adding a game. "open": the start's fetch on its thread (as a
// player's start; MW3_CATALOG_START=late), BATTLETECH added in that run. "install": one served while btech's original
// files are set up (its setup's --install puts revision 3 there and takes 3 s, a fetch every second) waits for the
// end, then adds BATTLETECH TWO. "check": no fetch at the start; CHECK FOR UPDATES fetches and adds BATTLETECH.
// "relaunch" (Mac, Linux): the launcher's own update installed (a DMG's app or the AppImage, MW3_UPDATE_TARGET) and
// started as it starts after an update (no arguments); the new one (a stand-in app running this build's launcher)
// finds the catalog newer than at the update and shows its card in its first run.
TEST(sdl_program_launcher_takes_a_catalog_while_open) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const std::string base = std::string(MW3_TEST_TMP) + "/launcher_catalog_open";
  fs::remove_all(base, ec);
  fs::create_directories(base, ec);
  auto read = [](const std::string& f) {
    std::ifstream in(f, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  };
  auto line = [](const std::string& log, const std::string& head, bool last = false) {
    const size_t at = last ? log.rfind(head) : log.find(head);
    return at == std::string::npos ? std::string() : log.substr(at, log.find('\n', at) - at);
  };
  auto at = [](const std::string& log, const std::string& s) { return log.find(s); };
  const std::string good(200000, 'o');
  const std::string good_sum = rekit::sha256_hex(reinterpret_cast<const uint8_t*>(good.data()), good.size());
  std::ofstream(base + "/good.zip", std::ios::binary) << good;
  auto entry = [](const char* key, int order, const char* button, const char* repo, const std::string& original) {
    return std::string("{\"key\": \"") + key + "\", \"family\": \"bt\", \"order\": " + std::to_string(order) +
           ", \"state\": \"released\", \"name\": \"" + button + " Rebuild\", \"title\": \"" + button + "\", \"heading\": \"" + button +
           "\", \"button\": \"" + button + "\", \"description\": \"A line.\", \"repo\": \"" + repo + "\", \"tag_prefix\": \"" + key +
           "-v\", \"program\": \"" + key + "\", \"assets\": {\"windows\": \"BT-{tag}-test.zip\", \"mac\": \"BT-{tag}-test.zip\", \"linux\": "
           "\"BT-{tag}-test.zip\"}, \"badge\": \"badge_mw3.png\", \"min_launcher\": \"1.5.2\"" +
           (original.empty() ? std::string() : ", \"original\": " + original) + "}";
  };
  // Revision 2 adds BATTLETECH (its original files listed); 3 adds BATTLETECH TWO beside it.
  const std::string original = "[{\"url\": \"file://" + base + "/good.zip\", \"sha256\": \"" + good_sum + "\", \"size\": " +
                               std::to_string(good.size()) + ", \"name\": \"BT disc.zip\"}]";
  auto catalog = [&](int revision) {
    std::string games = entry("btech", 7, "BATTLETECH", "battletech-re", original);
    if (revision >= 3) games += ", " + entry("btwo", 8, "BATTLETECH TWO", "battletech-two-re", "");
    return "{\"catalog\": 1, \"revision\": " + std::to_string(revision) +
           ", \"families\": [{\"key\": \"bt\", \"name\": \"BattleTech\", \"order\": 4}], \"games\": [" + games + "]}";
  };
  std::ofstream(base + "/cat2.json") << catalog(2);
  std::ofstream(base + "/cat3.json") << catalog(3);
  sign_fixture(base + "/cat2.json", {1});
  sign_fixture(base + "/cat3.json", {1});
  // btech's release: a stand-in game whose setup's --install serves revision 3 and takes 3 s.
  auto release = [&](const std::string& dir) {
    for (const char* repo : {"mech3-pm-re", "mech1-remake-re", "mech1-re", "inception-re", "inception-remake-re", "revenge-re", "battletech-two-re"})
      std::ofstream(dir + "/api/" + repo + ".json") << "[]";
    std::string sh = "#!/bin/sh\nR='" + dir + "'\necho \"$*\" >> \"$R/asked\"\ncase \"$1 $2\" in\n"
                     "'--status ') if [ -f \"$R/files-in\" ]; then ST=ready; else ST=missing; fi\n"
                     "  echo '{\"kit\": 1, \"game\": \"btech\", \"version\": \"0.1.0\", \"files\": {\"state\": \"'$ST'\"}, \"first_run\": {\"pending\": []}, "
                     "\"needs\": [], \"setup\": 1}';;\n"
                     "'--setup --look') echo '{\"done\": true, \"ok\": true, \"words\": \"A copy of BattleTech.\", \"into\": \"'\"$R\"'/files\"}';;\n"
                     "'--setup --install') cp '" + base + "/cat3.json' \"$R/served.json\"; cp '" + base + "/cat3.json.sig' \"$R/served.json.sig\"\n"
                     "  echo '{\"done\": false, \"at\": 500000, \"stage\": \"Copying\", \"message\": \"\"}'; sleep 3; : > \"$R/files-in\"\n"
                     "  echo '{\"done\": true, \"ok\": true, \"where\": \"'\"$R\"'/files\", \"summary\": \"BattleTech is set up.\"}';;\n"
                     "*) : > \"$R/played\";;\nesac\nexit 0\n";
    const std::string zip = dir + "/rel/BT-btech-v0.1.0-test.zip";
    archive* a = archive_write_new();
    archive_write_set_format_zip(a);
    archive_write_open_filename(a, zip.c_str());
    archive_entry* e = archive_entry_new();
    archive_entry_set_pathname(e, "btech");
    archive_entry_set_size(e, static_cast<la_int64_t>(sh.size()));
    archive_entry_set_filetype(e, AE_IFREG);
    archive_entry_set_perm(e, 0755);
    archive_write_header(a, e);
    archive_write_data(a, sh.data(), sh.size());
    archive_entry_free(e);
    archive_write_close(a);
    archive_write_free(a);
    const std::string zbytes = read(zip);
    std::ofstream(dir + "/rel/SHA256SUMS") << rekit::sha256_hex(reinterpret_cast<const uint8_t*>(zbytes.data()), zbytes.size()) << "  BT-btech-v0.1.0-test.zip\n";
    std::ofstream(dir + "/api/battletech-re.json")
        << "[{\"tag_name\": \"btech-v0.1.0\", \"draft\": false, \"prerelease\": false, \"body\": \"x\", \"assets\": [{\"name\": \"BT-btech-v0.1.0-test.zip\", "
           "\"size\": " << zbytes.size() << ", \"browser_download_url\": \"file://" << zip << "\"}, {\"name\": \"SHA256SUMS\", \"size\": 10, "
           "\"browser_download_url\": \"file://" << dir << "/rel/SHA256SUMS\"}]}]";
  };
  struct Run {
    std::string log;
    bool exited = false;
  };
  auto run = [&](const char* name, const std::vector<std::pair<std::string, std::string>>& extra, int served, const std::string& frames) {
    const std::string dir = base + "/" + name;
    for (const char* d : {"/api", "/games", "/wd/music", "/tmp", "/rel"}) fs::create_directories(dir + d, ec);
    std::ofstream(dir + "/wd/music/track02.wav") << "x";
    release(dir);
    if (served) std::ofstream(dir + "/served.json") << read(base + "/cat" + std::to_string(served) + ".json");
    std::vector<std::pair<std::string, std::string>> env{
        {"MW3_GAMES_API", "file://" + dir + "/api/{repo}.json"}, {"MW3_GAMES_DIR", dir + "/games"}, {"MW3_CATALOG_URL", "file://" + dir + "/served.json"},
        {"MW3_LAUNCHER_FRAMES", frames}, {"MW3_NO_IMPORT", "1"}, {"TMPDIR", dir + "/tmp"}, {"MW3_PM_BIN", ""}, {"MW3_MW1_BIN", ""},
        {"MW3_MW1R_BIN", ""}, {"MW3_CHI_BIN", ""}, {"MW3_CHIRM_BIN", ""}};
    env.insert(env.end(), extra.begin(), extra.end());
    const char* tmp_was = std::getenv("TMPDIR");
    const std::string tmp_keep = tmp_was ? tmp_was : "";
    for (const auto& [k, v] : env) setenv(k.c_str(), v.c_str(), 1);
    const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--window", "640x480", "--nosound", "--hidden"}, dir + "/launcher.log");
    for (const auto& [k, v] : env) unsetenv(k.c_str());
    if (tmp_was) setenv("TMPDIR", tmp_keep.c_str(), 1);
    int status = 0;
    Run r;
    r.exited = waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    r.log = read(dir + "/launcher.log");
    std::printf("catalog while open, %s:\n", name);
    for (const char* what : {"catalog: the built-in", "catalog: fetched again", "catalog: a newer one waits", "(fetched while open", "BattleTech is set up", "focus ", " frames, "})
      for (size_t a = 0; (a = r.log.find(what, a)) != std::string::npos; ++a) std::printf("    %s\n", r.log.substr(a, r.log.find('\n', a) - a).c_str());
    std::printf("    first %s\n    last  %s\n", line(r.log, "screen main: ").c_str(), line(r.log, "screen main: ", true).c_str());
    return r;
  };
  {  // open: the start's fetch lands while the built-in list is on screen
    const Run r = run("open", {{"MW3_CATALOG_START", "late"}}, 2, "600");
    EXPECT(r.exited && has(r.log, "catalog: the built-in one (revision 1)") && has(r.log, "catalog: fetched again (the start)"));
    EXPECT(has(r.log, "catalog: revision 2 in use (fetched while open"));
    EXPECT(!has(line(r.log, "screen main: "), "BATTLETECH") && has(line(r.log, "screen main: ", true), "BATTLETECH, BATTLETECH SETTINGS, CHECK FOR UPDATES"));
    std::printf("    %s\n", line(r.log, "games: btech: ", true).c_str());
    EXPECT(has(r.log, "/api/battletech-re.json: answered") && has(r.log, "games: btech: "));  // its releases asked once it is known
  }
  {  // install: revision 3 served during btech's setup waits for its end
    const Run r = run("install", {{"MW3_LAUNCHER_GAME_INSTALL", "btech"}, {"MW3_CATALOG_EVERY_S", "1"}}, 2, "1500");
    const size_t waits = at(r.log, "catalog: a newer one waits: an install, an update or a setup is running");
    const size_t set_up = at(r.log, "BattleTech is set up."), used = at(r.log, "catalog: revision 3 in use (fetched while open");
    EXPECT(r.exited && has(r.log, "catalog: revision 2, fetched"));  // the start's (a check run's, before the window)
    EXPECT(waits != std::string::npos && set_up != std::string::npos && used != std::string::npos && waits < set_up && set_up < used);
    EXPECT(has(line(r.log, "screen main: ", true), "BATTLETECH, BATTLETECH SETTINGS, BATTLETECH TWO, CHECK FOR UPDATES"));
  }
  {  // check: nothing at the start; CHECK FOR UPDATES fetches and adds the card
    const Run r = run("check", {{"MW3_CATALOG_START", "none"}, {"MW3_LAUNCHER_NAV", "next,prev,prev,press"}}, 2, "600");
    EXPECT(r.exited && has(r.log, "focus CHECK FOR UPDATES\n") && !has(r.log, "catalog: fetched again (the start)"));
    EXPECT(has(r.log, "catalog: fetched again (CHECK FOR UPDATES)") && has(r.log, "catalog: revision 2 in use (fetched while open"));
    EXPECT(at(r.log, "catalog: fetched again (CHECK FOR UPDATES)") < at(r.log, "catalog: revision 2 in use (fetched while open"));
    EXPECT(!has(line(r.log, "screen main: "), "BATTLETECH") && has(line(r.log, "screen main: ", true), "BATTLETECH, BATTLETECH SETTINGS, CHECK FOR UPDATES"));
  }
#if defined(__APPLE__) || defined(__linux__)
  {  // relaunch: the launcher's own update installed and started; the new one shows the catalog newer than at the update
    const std::string dir = base + "/relaunch";
    for (const char* d : {"/api", "/games", "/wd/music", "/tmp", "/rel", "/pkg"}) fs::create_directories(dir + d, ec);
    std::ofstream(dir + "/wd/music/track02.wav") << "x";
    release(dir);
    // The new launcher: this build's, started as after an update (no arguments of its own); it serves revision 2 first
    // (the catalog moved on while the update ran), and is told not to update again.
    const std::string start = "#!/bin/sh\nR='" + dir + "'\ncp '" + base + "/cat2.json' \"$R/served.json\"; cp '" + base + "/cat2.json.sig' \"$R/served.json.sig\"\n"
                              "unset MW3_LAUNCHER_UPDATE_GO MW3_UPDATE_TARGET MW3_UPDATE_API MW3_UPDATE_NO_RESTART MW3_LAUNCHER_NAV\n"
                              "MW3_CATALOG_START=late MW3_LAUNCHER_FRAMES=600 '" + std::string(MW3_LAUNCHER_BIN) + "' --write-dir \"$R/wd\" --window 640x480 "
                              "--nosound --hidden > \"$R/relaunched.log\" 2>&1\necho $? > \"$R/relaunched.done\"\n";
#if defined(__APPLE__)
    const std::string ours = "MW3-Rebuild-98.1.0-mac.dmg";
    const std::string target = dir + "/Applications/MW3 Rebuild.app";
    fs::create_directories(target + "/Contents/MacOS", ec);
    std::ofstream(target + "/Contents/MacOS/mw3launcher") << "#!/bin/sh\necho old\n";
    fs::create_directories(dir + "/dmg-src/MW3 Rebuild.app/Contents/MacOS", ec);
    std::ofstream(dir + "/dmg-src/MW3 Rebuild.app/Contents/MacOS/mw3launcher") << start;
    fs::permissions(dir + "/dmg-src/MW3 Rebuild.app/Contents/MacOS/mw3launcher", fs::perms::owner_all);
    std::ofstream(dir + "/dmg-src/MW3 Rebuild.app/Contents/Info.plist") << "<plist/>\n";
    const std::string make = "/usr/bin/hdiutil create -quiet -fs HFS+ -volname 'MW3 Rebuild' -srcfolder '" + dir + "/dmg-src' -format UDZO '" + dir + "/pkg/" + ours + "'";
    EXPECT(std::system(make.c_str()) == 0);
#else
    const std::string ours = "MW3-Rebuild-98.1.0-x86_64.AppImage";
    const std::string target = dir + "/MW3-Rebuild.AppImage";
    std::ofstream(target) << "#!/bin/sh\necho old\n";
    std::ofstream(dir + "/pkg/" + ours) << start;
#endif
    const std::string bytes = read(dir + "/pkg/" + ours);
    std::ofstream(dir + "/pkg/SHA256SUMS") << rekit::sha256_hex(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()) << "  " << ours << "\n";
    sign_fixture(dir + "/pkg/SHA256SUMS", {1});
    std::ofstream(dir + "/update.json")
        << "[{\"tag_name\": \"v98.1.0\", \"draft\": false, \"prerelease\": false, \"body\": \"x\", \"assets\": [{\"name\": \"" << ours << "\", \"size\": "
        << bytes.size() << ", \"browser_download_url\": \"file://" << dir << "/pkg/" << ours << "\"}, {\"name\": \"SHA256SUMS\", "
        << "\"browser_download_url\": \"file://" << dir << "/pkg/SHA256SUMS\"}, {\"name\": \"SHA256SUMS.sig\", \"browser_download_url\": \"file://"
        << dir << "/pkg/SHA256SUMS.sig\"}]}]";
    write_sources(dir + "/sources.json", {"file:///nowhere/catalog.json"}, {1});
    // Its temp on the system's volume, as a player's: macOS refuses a DMG mounted on an external volume's folder.
    const std::string sys_tmp = (fs::temp_directory_path(ec) / ("mw3-relaunch-" + std::to_string(getpid()))).string();
    fs::create_directories(sys_tmp, ec);
    const std::vector<std::pair<std::string, std::string>> env{
        {"MW3_GAMES_API", "file://" + dir + "/api/{repo}.json"}, {"MW3_GAMES_DIR", dir + "/games"}, {"MW3_CATALOG_URL", "file://" + dir + "/served.json"},
        {"MW3_SOURCES_FILE", dir + "/sources.json"}, {"MW3_UPDATE_API", "file://" + dir + "/update.json"}, {"MW3_LAUNCHER_UPDATE_GO", "1"},
        {"MW3_LAUNCHER_NAV", "next,prev,prev,press"},  // CHECK FOR UPDATES, then its box's UPDATE LAUNCHER
        {"MW3_UPDATE_TARGET", target}, {"MW3_LAUNCHER_FRAMES", "600"}, {"MW3_NO_IMPORT", "1"}, {"TMPDIR", sys_tmp}, {"MW3_PM_BIN", ""},
        {"MW3_MW1_BIN", ""}, {"MW3_MW1R_BIN", ""}, {"MW3_CHI_BIN", ""}, {"MW3_CHIRM_BIN", ""}};
    const char* tmp_was = std::getenv("TMPDIR");
    const std::string tmp_keep = tmp_was ? tmp_was : "";
    for (const auto& [k, v] : env) setenv(k.c_str(), v.c_str(), 1);
    const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--window", "640x480", "--nosound", "--hidden"}, dir + "/launcher.log");
    for (const auto& [k, v] : env) unsetenv(k.c_str());
    if (tmp_was) setenv("TMPDIR", tmp_keep.c_str(), 1);
    int status = 0;
    EXPECT(waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    for (int i = 0; i < 1200 && !fs::exists(dir + "/relaunched.done", ec); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const std::string old_log = read(dir + "/launcher.log"), log = read(dir + "/relaunched.log");
    fs::remove_all(sys_tmp, ec);
    std::printf("catalog while open, relaunch:\n    %s\n    %s\n", line(old_log, "launcher: update: ").c_str(), line(old_log, "catalog: the built-in").c_str());
    for (const char* what : {"catalog: the built-in", "catalog: fetched again", "(fetched while open"})
      std::printf("    new: %s\n", line(log, what).c_str());
    std::printf("    new: first %s\n    new: last  %s\n", line(log, "screen main: ").c_str(), line(log, "screen main: ", true).c_str());
    if (!has(old_log, "starts again now"))
      for (const char* what : {"update", "UPDATE", "SHA256", "catalog"})
        for (size_t a = 0; (a = old_log.find(what, a)) != std::string::npos; a = old_log.find('\n', a))
          std::printf("    old: %s\n", old_log.substr(old_log.rfind('\n', a) + 1, old_log.find('\n', a) - old_log.rfind('\n', a) - 1).c_str());
    EXPECT(has(old_log, "launcher: update: Installed. The BattleTech launcher starts again now.") && !has(old_log, "BATTLETECH"));
    EXPECT(has(old_log, "job: The BattleTech launcher: launcher ") && has(old_log, " | the launcher's update ("));  // its one bar
    EXPECT(read(dir + "/relaunched.done") == "0\n" && has(log, "catalog: the built-in one (revision 1)"));
    // The update counted by the new launcher (2.1.0): the old one's note taken once; 98.1.0 is not this build, so dropped.
    EXPECT(has(log, "counts: an update note to 98.1.0, but this is ") && !fs::exists(dir + "/wd/launcher-updated.txt", ec));
    EXPECT(has(log, "catalog: revision 2 in use (fetched while open") && !has(line(log, "screen main: "), "BATTLETECH") &&
           has(line(log, "screen main: ", true), "BATTLETECH, BATTLETECH SETTINGS, CHECK FOR UPDATES"));
  }
#endif
  if (!check::failures) fs::remove_all(base, ec);
}

// A present that does not wait for the display (macOS: a window not on screen) does not run away: 1.5.0 drew some 950
// frames a second behind a fullscreen game, and its drawables took tens of gigabytes. MW3_LAUNCHER_UNPACED stands in.
TEST(sdl_program_launcher_never_runs_away) {
  const LauncherRun r = run_launcher_with("launcher_unpaced", "exit 0", nullptr, {{"MW3_LAUNCHER_UNPACED", "1"}, {"MW3_LAUNCHER_FRAMES", "600"}},
                                          [](const std::string& d) {
                                            std::filesystem::create_directories(d + "/wd/music");
                                            std::ofstream(d + "/wd/music/track02.wav") << "x";
                                          }, false);
  const size_t at = r.log.find("600 frames, ");
  const double fps = at == std::string::npos ? -1 : std::atof(r.log.c_str() + at + 12);
  std::printf("unpaced: %.1f frames a second\n", fps);
  EXPECT(r.exited && fps > 10 && fps < 600);  // a tenth over the display's, at most a 500 Hz one's
}




// No Vulkan on Linux (a player on Arch, 1.1.1: the game's fatal box, "No supported SDL_GPU backend
// found!", until lib32-vulkan-intel brought the 64-bit driver along): the launcher, whose window needs
// no Vulkan, says what to install at its start and on PLAY, and never starts the game without it;
// TRY AGAIN checks again. MW3_VULKAN_CHECK answers for the checks (any system).
TEST(sdl_vulkan_help_names_the_packages) {
  const std::string none = sdlfe::vulkan_help(sdlfe::Vulkan::kOk);
  const std::string loader = sdlfe::vulkan_help(sdlfe::Vulkan::kNoLoader), driver = sdlfe::vulkan_help(sdlfe::Vulkan::kNoDriver);
  auto has = [](const std::string& t, const char* w) { return t.find(w) != std::string::npos; };
  EXPECT(none.empty());
  for (const std::string* t : {&loader, &driver})
    for (const char* w : {"64-bit", "not the lib32", "vulkan-icd-loader", "vulkan-intel", "vulkan-radeon", "nvidia-utils",
                          "libvulkan1", "mesa-vulkan-drivers", "vulkan-loader"})
      EXPECT(has(*t, w));
  EXPECT(has(loader, "no Vulkan library") && has(driver, "no Vulkan driver"));
  setenv("MW3_VULKAN_CHECK", "noloader", 1);
  EXPECT(sdlfe::vulkan_check() == sdlfe::Vulkan::kNoLoader);
  setenv("MW3_VULKAN_CHECK", "nodriver", 1);
  EXPECT(sdlfe::vulkan_check() == sdlfe::Vulkan::kNoDriver);
  unsetenv("MW3_VULKAN_CHECK");
#if !defined(__linux__)
  EXPECT(sdlfe::vulkan_check() == sdlfe::Vulkan::kOk);  // only Linux draws through Vulkan
#endif
}

TEST(sdl_program_launcher_says_what_vulkan_needs_and_holds_play) {
  auto count = [](const std::string& log, const std::string& w) {
    int n = 0;
    for (size_t at = 0; (at = log.find(w, at)) != std::string::npos; ++at) ++n;
    return n;
  };
  const auto env = std::vector<std::pair<std::string, std::string>>{
      {"MW3_VULKAN_CHECK", "nodriver"}, {"MW3_LAUNCHER_FRAMES", "60"}, {"MW3_LAUNCHER_NAV", "next,press"}};
  const LauncherRun r = run_launcher_with("launcher_vulkan", "READY\nsleep 1", nullptr, env);
  const int said = count(r.log, "needs Vulkan graphics"), again = count(r.log, "Vulkan checked again: still missing");
  std::printf("  said %d times, checked again %d, the game started %d, exited %d\n", said, again, !r.args.empty(), r.exited);
  EXPECT(r.exited && r.args.empty() && !r.game_finished);  // the game never started
  EXPECT(said >= 2 && again == 1);                           // at the start, on PLAY and on TRY AGAIN
}

// The system's picker blocks while it comes up (about a second on the Mac, 9 s
// seen on a cold start; docs/launcher.md, The picker's wait): the launcher
// shows a frame that says so first, then asks. MW3_LAUNCHER_PICKER=cancel
// answers the ask without a window; the frame at the ask is saved.
TEST(sdl_program_launcher_says_the_picker_is_opening_before_it_asks) {
  const std::string dir = std::string(MW3_TEST_TMP) + "/launcher_picker_notice";
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir + "/wd", ec);
  const std::string shot = dir + "/ask.bmp";
  // INSTALL FROM A DOWNLOADED FILE pressed on the main screen (b5: before, the raw port's PLAY ORIGINAL asked for its
  // folder here); music there, so no first notes come before it.
  std::filesystem::create_directories(dir + "/wd/music", ec);
  std::ofstream(dir + "/wd/music/track02.wav") << "x";
  setenv("MW3_LAUNCHER_INSTALL_FILE", "1", 1);
  setenv("MW3_GAMES_DIR", (dir + "/games").c_str(), 1);  // never this computer's own games folder
  setenv("MW3_LAUNCHER_PICKER", "cancel", 1);
  setenv("MW3_LAUNCHER_PICKER_SHOT", shot.c_str(), 1);
  setenv("MW3_LAUNCHER_FRAMES", "20", 1);
  const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--window", "640x480", "--nosound", "--hidden"}, dir + "/launcher.log");
  unsetenv("MW3_LAUNCHER_FRAMES");
  unsetenv("MW3_LAUNCHER_PICKER_SHOT");
  unsetenv("MW3_LAUNCHER_PICKER");
  unsetenv("MW3_GAMES_DIR"), unsetenv("MW3_LAUNCHER_INSTALL_FILE");
  int status = 0;
  const bool exited = waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0;
  std::ifstream f(dir + "/launcher.log");
  const std::string log((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  const size_t frame = log.find("first frame shown"), notice = log.find("the picker's notice shown"),
               asked = log.find("the file picker asked for"), answered = log.find("the picker answered (cancelled)");
  std::printf("the picker's notice: exited %d, first frame %d, notice %d, asked %d, answered %d\n", exited, frame != std::string::npos,
              notice != std::string::npos, asked != std::string::npos, answered != std::string::npos);
  EXPECT(exited);
  EXPECT(frame != std::string::npos && notice != std::string::npos && asked != std::string::npos && answered != std::string::npos);
  EXPECT(frame < notice && notice < asked && asked < answered);  // a frame on screen before the blocking call
  EXPECT(log.find("frames, ") != std::string::npos);            // and the launcher draws on after the answer
  // The frame at the ask carries the notice: its box's amber edge and dark inside, low in the middle.
  SDL_Surface* bmp = SDL_LoadBMP(shot.c_str());
  EXPECT(bmp != nullptr);
  if (bmp) {
    Uint8 r = 0, g = 0, b = 0, a = 0;
    EXPECT(bmp->w == 854 && bmp->h == 480);
    SDL_ReadSurfacePixel(bmp, 854 / 2 - 151, 480 - 26, &r, &g, &b, &a);
    std::printf("the notice's edge %d %d %d\n", r, g, b);
    EXPECT(r > 120 && r < 160 && g > 85 && g < 115 && b > 35 && b < 65);
    SDL_ReadSurfacePixel(bmp, 854 / 2 - 145, 480 - 38, &r, &g, &b, &a);
    EXPECT(r < 30 && g < 30 && b < 30);
    SDL_DestroySurface(bmp);
  }
  std::filesystem::remove_all(dir, ec);
}




// The game's own status before its first frame (its first start's movie
// import, sdl_child.h set_ready_status): on the launcher's status line in
// place of the wait's "not shown a picture yet".
TEST(sdl_program_launcher_shows_the_games_status) {
  const LauncherRun r = run_launcher_with("launcher_game_status", "STATUS\nsleep 2\nREADY\nsleep 1", "500");
  std::printf("a game importing: said %d, waited quietly %d, then its first frame %d\n",
              has(r.log, "the game says: Importing your movies (first start only): 3 of 19..."),
              !has(r.log, "no first frame from the game after"), has(r.log, "the launcher stays open"));
  EXPECT(r.exited && has(r.log, "the game says: Importing your movies (first start only): 3 of 19...") &&
         !has(r.log, "no first frame from the game after") && has(r.log, "the launcher stays open"));
}

// The launcher's sounds (lead's launcher list, item 4; docs/launcher.md): its own rollover and select since b6
// (assets/launcher's hover.wav and click.wav, 2026-11-26; before, the game's sfx_button2 and sfx_button6 from the
// install's sound archive), with no install found. Skipped where the machine has no audio device.
TEST(sdl_program_launcher_plays_the_menus_sounds) {
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return;
  const bool device = SDL_GetNumAudioDrivers() > 0 && std::string(SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "") != "dummy";
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  if (!device) return;
  const std::string dir = std::string(MW3_TEST_TMP) + "/launcher_sounds";
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir + "/wd", ec);
  const std::string log = dir + "/sounds.log";
  setenv("MW3_LAUNCHER_MUTE", "1", 1);  // played at no volume (the machine may be streaming)
  setenv("MW3_LAUNCHER_FRAMES", "5", 1);
  setenv("MW3_FIND_INSTALL", "1", 1);  // no game files: the sounds need none
  const std::string home_was = std::getenv("HOME") ? std::getenv("HOME") : "";
  std::filesystem::create_directories(dir + "/home", ec);
  setenv("HOME", (dir + "/home").c_str(), 1);  // nothing of this computer's for a search to find
  const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--window", "640x480", "--hidden"}, log);
  setenv("HOME", home_was.c_str(), 1);
  unsetenv("MW3_FIND_INSTALL");
  unsetenv("MW3_LAUNCHER_FRAMES");
  unsetenv("MW3_LAUNCHER_MUTE");
  int status = 0;
  EXPECT(pid > 0 && waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
  std::ifstream f(log);
  const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  const size_t at = text.find("the menu's sounds from ");
  std::printf("launcher sounds: %s\n", at == std::string::npos ? "none" : text.substr(at, text.find('\n', at) - at).c_str());
  const size_t eol = at == std::string::npos ? at : text.find('\n', at);
  EXPECT(at != std::string::npos && text.substr(at, eol - at).find("assets/launcher (2 of 2)") != std::string::npos);
  EXPECT(text.find("Found MechWarrior 3 at") == std::string::npos);  // and no game files were found
}



#if !defined(_WIN32)

#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
#endif
#endif  // !_WIN32: the updater's own tests, from here to the Mac's install, run on Windows too (curl.exe, file://)

// The updater's fetch (docs/launcher.md, Updates, sitting 1): the system's curl as a child, its arguments an array.
// A file:// fixture read only when allowed (checks), a missing one said in curl's words, an http address refused.
// 2026-11-23: the launcher's checks flashed a console for each curl on Windows. A helper program starts
// quietly (on Windows SDL's background flag, CREATE_NO_WINDOW) and its real exit code still comes back (the flag
// alone reports 0, and curl's code is how a failed check is told).
TEST(sdl_update_helpers_start_quietly_with_their_exit_code) {
#if defined(_WIN32)
  const char* comspec = std::getenv("ComSpec");
  const std::string shell = comspec ? comspec : "C:\\Windows\\System32\\cmd.exe";
  const char* args[] = {shell.c_str(), "/c", "exit 3", nullptr};
#else
  const char* args[] = {"/bin/sh", "-c", "exit 3", nullptr};
#endif
  const SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, const_cast<char**>(args));
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
  SDL_Process* p = sdlfe::start_quiet(props);
  SDL_DestroyProperties(props);
  EXPECT(p != nullptr);
  if (!p) return;
#if defined(_WIN32)
  EXPECT(SDL_GetBooleanProperty(SDL_GetProcessProperties(p), SDL_PROP_PROCESS_BACKGROUND_BOOLEAN, false));  // no console
#endif
  int code = -1;
  EXPECT(sdlfe::wait_quiet(p, true, &code));
  std::printf("  exit code %d\n", code);
  EXPECT(code == 3);
  SDL_DestroyProcess(p);
}

TEST(sdl_update_fetch_by_curl) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path dir = fs::path(MW3_TEST_TMP) / "update_fetch";
  fs::create_directories(dir, ec);
  std::ofstream(dir / "releases.json") << "[{\"tag_name\":\"v9.9.9\",\"draft\":false,\"prerelease\":false,\"body\":\"New.\",\"assets\":[]}]";
  std::string out, error;
  const std::string url = "file://" + (dir / "releases.json").string();
  EXPECT(sdlfe::curl_get(url, 10, &out, &error, true) && out.find("v9.9.9") != std::string::npos);
  EXPECT(!sdlfe::curl_get(url, 10, &out, &error, false) && error == "not an https address");
  EXPECT(!sdlfe::curl_get("file://" + (dir / "gone.json").string(), 10, &out, &error, true) && !error.empty() && out.empty());
  std::printf("  a missing file: %s\n", error.c_str());
  EXPECT(!sdlfe::curl_get("http://localhost:9/x", 10, &out, &error, false) && !error.empty());  // https only
  std::printf("  http: %s\n", error.c_str());
  fs::remove_all(dir, ec);
}

// The updater's download (sitting 3) on file:// fixtures: this platform's package and the release's SHA256SUMS
// fetched, the package checked against its line; a wrong sum, no sums, no line, no package and a cancel each
// install nothing, leave nothing behind, and say why.
TEST(sdl_update_download_and_its_check) {
  namespace fs = std::filesystem;
  namespace u = upgrades::update;
  std::error_code ec;
  const fs::path dir = fs::path(MW3_TEST_TMP) / "update_download";
  fs::remove_all(dir, ec);
  fs::create_directories(dir / "release", ec);
  const char* names[] = {"MW3-Rebuild-Setup-9.0.0.exe", "MW3-Rebuild-9.0.0-mac.dmg", "MW3-Rebuild-9.0.0-x86_64.AppImage"};
  const std::string ours = names[u::this_platform() == u::Platform::kWindows ? 0 : u::this_platform() == u::Platform::kMac ? 1 : 2];
  const std::string bytes(300000, 'p');
  std::ofstream(dir / "release" / ours, std::ios::binary) << bytes;
  const std::string sum = rekit::sha256_hex(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
  const auto sums = [&](const std::string& text) { std::ofstream(dir / "release" / "SHA256SUMS") << text; };
  const auto url = [&](const std::string& n) { return "file://" + (dir / "release" / n).string(); };
  u::Release r;
  r.tag = "v9.0.0";
  r.assets = {{"mw3-9.0.0-win64.zip", url("nothing"), 1}, {ours, url(ours), bytes.size()}, {"SHA256SUMS", url("SHA256SUMS"), 0}};
  r.unsigned_sums = true;  // a home whose unsigned SHA256SUMS are taken (GitHub's); sdl_update_download_takes_signed_sums
  setenv("MW3_UPDATE_API", "file:///fixture", 1);  // checks: file:// assets allowed
  const std::string to = (dir / "download").string();
  const auto leftovers = [&]() {
    int n = 0;
    for (const auto& e : fs::directory_iterator(to, ec)) ++n, (void)e;
    return n;
  };
  // Good.
  sums(sum + "  " + ours + "\n");
  int calls = 0;
  sdlfe::Downloaded d = sdlfe::download_update(r, to, [&](double, uint64_t, bool) { return ++calls, true; });
  std::printf("  good: %s (%d progress calls)\n", d.words.c_str(), calls);
  EXPECT(d.ok && d.path == (fs::path(to) / ours).string() && fs::file_size(d.path, ec) == bytes.size() && calls > 0 && leftovers() == 1);
  // A wrong sum: removed.
  sums(std::string(64, 'f') + "  " + ours + "\n");
  d = sdlfe::download_update(r, to, nullptr);
  EXPECT(!d.ok && d.words.find("did not match its checksum") != std::string::npos && leftovers() == 0);
  // SHA256SUMS without its line; no SHA256SUMS on the release; no package for this platform.
  sums(sum + "  other.dmg\n");
  d = sdlfe::download_update(r, to, nullptr);
  EXPECT(!d.ok && d.words.find("has no line for " + ours) != std::string::npos && leftovers() == 0);
  u::Release no_sums = r;
  no_sums.assets.pop_back();
  EXPECT(sdlfe::download_update(no_sums, to, nullptr).words.find("no SHA256SUMS") != std::string::npos);
  u::Release no_package = r;
  no_package.assets.erase(no_package.assets.begin() + 1);
  EXPECT(sdlfe::download_update(no_package, to, nullptr).words.rfind("This release has no ", 0) == 0);
  // Cancelled at once: nothing kept, said so.
  sums(sum + "  " + ours + "\n");
  d = sdlfe::download_update(r, to, [](double, uint64_t, bool) { return false; });
  std::printf("  canceled: %s\n", d.words.c_str());
  EXPECT(!d.ok && d.cancelled && d.words.find("canceled") != std::string::npos && leftovers() == 0);
  // The package gone from the release's place: curl's reason.
  fs::remove(dir / "release" / ours, ec);
  d = sdlfe::download_update(r, to, nullptr);
  std::printf("  gone: %s\n", d.words.c_str());
  EXPECT(!d.ok && d.words.rfind("The download stopped: ", 0) == 0 && leftovers() == 0);
  unsetenv("MW3_UPDATE_API");
  fs::remove_all(dir, ec);
}

// The lists asked for a repo (2026-11-26: the catalog in use's homes, the built-in's GitHub then git.owenmorgan.com's
// Forgejo, the first downloaded from first); in checks MW3_UPDATE_API / MW3_GAMES_API name one list (a trusted first
// home) or two ("UNTRUSTED|TRUSTED").
TEST(sdl_release_lists_both_homes) {
  const auto l = sdlfe::release_lists(nullptr, "mech3-pm-re", 20);
  EXPECT(l.size() == 2 && l[0].url == "https://api.github.com/repos/telltaleatheist/mech3-pm-re/releases?per_page=20" && l[0].download_first &&
         l[0].unsigned_sums && l[1].url == "https://git.owenmorgan.com/api/v1/repos/telltaleatheist/mech3-pm-re/releases?limit=40" &&
         !l[1].download_first && !l[1].unsigned_sums);
  const auto one = sdlfe::release_lists("file:///t/{repo}.json", "mech1-re", 20);
  EXPECT(one.size() == 1 && one[0].url == "file:///t/mech1-re.json" && one[0].download_first && one[0].unsigned_sums);
  const auto two = sdlfe::release_lists("file:///b/{repo}.json|file:///g/{repo}.json", "mech1-re", 20);
  EXPECT(two.size() == 2 && two[0].url == "file:///b/mech1-re.json" && !two[0].download_first && !two[0].unsigned_sums &&
         two[1].url == "file:///g/mech1-re.json" && two[1].download_first && two[1].unsigned_sums);
}

// The lists and the catalog's addresses from a homes list (2026-11-26, docs/launcher.md, Two homes): each home by
// order, then GitHub; the catalog in use's (the built-in: git.owenmorgan.com) as before; none, GitHub alone.
TEST(sdl_homes_lists_and_catalog_addresses) {
  namespace u = upgrades::update;
  // Each home's address with {repo} and {count} filled, in order; the first downloaded from first; the unsigned
  // SHA256SUMS taken only from a list sources.json names (the built-in copy: GitHub's).
  const std::vector<u::CatalogHome> homes{{"a", "https://releases.reverse-engineered.com/{repo}/list.json?n={count}", 1},
                                          {"gh", "https://api.github.com/repos/telltaleatheist/{repo}/releases?per_page={count}", 2}};
  const auto l = sdlfe::home_lists(homes, "mech1-re", 20);
  EXPECT(l.size() == 2 && l[0].url == "https://releases.reverse-engineered.com/mech1-re/list.json?n=20" && l[0].download_first && !l[0].unsigned_sums);
  EXPECT(l[1].url == "https://api.github.com/repos/telltaleatheist/mech1-re/releases?per_page=20" && !l[1].download_first && l[1].unsigned_sums);
  EXPECT(sdlfe::home_lists({}, "mech1-re", 20).empty());
  const auto in_use = sdlfe::home_lists(u::active_homes(), "mech3-pm-re", 40);
  EXPECT(in_use.size() == 2 && in_use[0].url == "https://api.github.com/repos/telltaleatheist/mech3-pm-re/releases?per_page=40" && in_use[0].download_first &&
         in_use[1].url == "https://git.owenmorgan.com/api/v1/repos/telltaleatheist/mech3-pm-re/releases?limit=40" && !in_use[1].unsigned_sums);
  // The catalog's addresses: the catalog in use's own first, then sources.json's, each once.
  const auto c = sdlfe::catalog::addresses({"https://b.example.org/c.json", "https://a.example.org/c.json"},
                                           {"https://a.example.org/c.json", "https://s.example.org/c.json"});
  EXPECT(c.size() == 3 && c[0] == "https://b.example.org/c.json" && c[1] == "https://a.example.org/c.json" && c[2] == "https://s.example.org/c.json");
}

// Three homes (2026-11-26): the lists of two homes and GitHub judged, the release's other_homes in the homes' order;
// GitHub's file and the first home's gone, the second home's is downloaded; the second's gone too, nothing.
TEST(sdl_update_download_falls_back_to_the_second_home) {
  namespace fs = std::filesystem;
  namespace u = upgrades::update;
  std::error_code ec;
  const fs::path dir = fs::path(MW3_TEST_TMP) / "update_download_second_home";
  fs::remove_all(dir, ec);
  const char* names[] = {"MW3-Rebuild-Setup-9.0.0.exe", "MW3-Rebuild-9.0.0-mac.dmg", "MW3-Rebuild-9.0.0-x86_64.AppImage"};
  const std::string ours = names[u::this_platform() == u::Platform::kWindows ? 0 : u::this_platform() == u::Platform::kMac ? 1 : 2];
  const std::string bytes(150000, 'h');
  const std::string sum = rekit::sha256_hex(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
  // Forward slashes: Windows' backslashes in the JSON list below would be bad escapes.
  const auto url = [&](const char* home, const std::string& n) { return "file://" + (dir / home / n).generic_string(); };
  const auto list = [&](const char* home) {
    return "[{\"tag_name\": \"v9.0.0\", \"draft\": false, \"prerelease\": false, \"body\": \"x\", \"assets\": [{\"name\": \"" + ours +
           "\", \"size\": " + std::to_string(bytes.size()) + ", \"browser_download_url\": \"" + url(home, ours) +
           "\"}, {\"name\": \"SHA256SUMS\", \"browser_download_url\": \"" + url(home, "SHA256SUMS") + "\"}]}]";
  };
  const auto lay = [&]() {
    fs::remove_all(dir, ec);
    for (const char* home : {"gh", "h1", "h2"}) {
      fs::create_directories(dir / home, ec);
      std::ofstream(dir / home / ours, std::ios::binary) << bytes;
      std::ofstream(dir / home / "SHA256SUMS") << sum << "  " << ours << "\n";
    }
  };
  // Each home's unsigned SHA256SUMS taken here (sdl_update_download_takes_signed_sums checks which are).
  const u::Verdict v = u::judge("v1.0.0", {{list("h1"), "", false, true}, {list("h2"), "", false, true}, {list("gh"), "", true, true}}, u::this_platform());
  EXPECT(v.kind == u::Verdict::kNewer && v.release.assets.size() == 2 && v.release.assets[0].url == url("gh", ours));
  EXPECT(v.release.other_homes.size() == 2 && v.release.other_homes[0][0].url == url("h1", ours) && v.release.other_homes[1][0].url == url("h2", ours));
  setenv("MW3_UPDATE_API", "file:///fixture", 1);  // checks: file:// assets allowed
  const std::string to = (dir / "download").string();
  lay();
  fs::remove(dir / "gh" / ours, ec), fs::remove(dir / "h1" / ours, ec);
  sdlfe::Downloaded d = sdlfe::download_update(v.release, to, nullptr);
  std::printf("  github's and the first home's gone: %s\n", d.ok ? "downloaded" : d.words.c_str());
  EXPECT(d.ok && fs::file_size(d.path, ec) == bytes.size() && rekit::sha256_file_hex(d.path) == sum);
  lay();
  for (const char* home : {"gh", "h1", "h2"}) fs::remove(dir / home / ours, ec);
  d = sdlfe::download_update(v.release, to, nullptr);
  EXPECT(!d.ok && !fs::exists(fs::path(to) / ours));
  unsetenv("MW3_UPDATE_API");
  if (!check::failures) fs::remove_all(dir, ec);
}

// Signed sums (2026-11-26, docs/launcher.md, Signed data): a home whose unsigned SHA256SUMS are not taken (not
// GitHub's) has its file downloaded only when its SHA256SUMS.sig checks against a key the launcher knows; unsigned or
// another key's: refused and said. GitHub's (unsigned_sums) taken unsigned.
TEST(sdl_update_download_takes_signed_sums) {
  namespace fs = std::filesystem;
  namespace u = upgrades::update;
  std::error_code ec;
  const fs::path dir = fs::path(MW3_TEST_TMP) / "update_download_signed";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  const char* names[] = {"MW3-Rebuild-Setup-9.0.0.exe", "MW3-Rebuild-9.0.0-mac.dmg", "MW3-Rebuild-9.0.0-x86_64.AppImage"};
  const std::string ours = names[u::this_platform() == u::Platform::kWindows ? 0 : u::this_platform() == u::Platform::kMac ? 1 : 2];
  const std::string bytes(120000, 's');
  const std::string sum = rekit::sha256_hex(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
  std::ofstream(dir / ours, std::ios::binary) << bytes;
  std::ofstream(dir / "SHA256SUMS") << sum << "  " << ours << "\n";
  const auto url = [&](const std::string& n) { return "file://" + (dir / n).string(); };
  const u::Sources keep = u::active_sources();
  u::Sources s = keep;
  s.keys = {test_keys::public_key(1)};
  u::use_sources(s);
  setenv("MW3_UPDATE_API", "file:///fixture", 1);
  const std::string to = (dir / "download").string();
  const auto get = [&](bool trusted, bool with_sig) {
    u::Release r;
    r.tag = "v9.0.0";
    r.assets = {{ours, url(ours), bytes.size()}, {"SHA256SUMS", url("SHA256SUMS"), 0}};
    if (with_sig) r.assets.push_back({"SHA256SUMS.sig", url("SHA256SUMS.sig"), 0});
    r.unsigned_sums = trusted;
    return sdlfe::download_update(r, to, nullptr);
  };
  sign_fixture((dir / "SHA256SUMS").string(), {1});
  sdlfe::Downloaded d = get(false, true);
  std::printf("  signed, untrusted home: %s\n", d.ok ? "downloaded" : d.words.c_str());
  EXPECT(d.ok && rekit::sha256_file_hex(d.path) == sum);
  d = get(false, false);
  std::printf("  unsigned, untrusted home: %s\n", d.words.c_str());
  EXPECT(!d.ok && d.words == "The release's SHA256SUMS could not be trusted (its SHA256SUMS is not signed), so nothing was downloaded." &&
         !fs::exists(fs::path(to) / ours));
  d = get(true, false);
  EXPECT(d.ok);
  sign_fixture((dir / "SHA256SUMS").string(), {9});
  d = get(false, true);
  std::printf("  another key's, untrusted home: %s\n", d.words.c_str());
  EXPECT(!d.ok && d.words.find("its SHA256SUMS.sig does not check against the launcher's keys") != std::string::npos && !fs::exists(fs::path(to) / ours));
  EXPECT(get(true, true).ok);  // GitHub's: its unsigned sums taken whatever the .sig
  // Another home's signed sums vouch for GitHub's file when GitHub has none; unsigned ones from it do not.
  sign_fixture((dir / "SHA256SUMS").string(), {1});
  u::Release r;
  r.tag = "v9.0.0";
  r.assets = {{ours, url(ours), bytes.size()}};
  r.unsigned_sums = true;
  r.other_homes = {{{ours, url("gone"), bytes.size()}, {"SHA256SUMS", url("SHA256SUMS"), 0}, {"SHA256SUMS.sig", url("SHA256SUMS.sig"), 0}}};
  r.other_unsigned_sums = {false};
  EXPECT(sdlfe::download_update(r, to, nullptr).ok);
  r.other_homes[0].pop_back();
  d = sdlfe::download_update(r, to, nullptr);
  EXPECT(!d.ok && !fs::exists(fs::path(to) / ours));
  unsetenv("MW3_UPDATE_API");
  u::use_sources(keep);
  if (!check::failures) fs::remove_all(dir, ec);
}

// Both homes (2026-11-22): a release in both downloads from GitHub's copy (here gh/) and falls back to the
// backup's (bk/, git.owenmorgan.com) only when GitHub's fails: its file gone, or not its checksum; GitHub's
// SHA256SUMS missing, the backup's line used for GitHub's file; the homes' lines disagreeing, nothing taken; a cancel
// never tries the other home.
TEST(sdl_update_download_falls_back_to_the_backup) {
  namespace fs = std::filesystem;
  namespace u = upgrades::update;
  std::error_code ec;
  const fs::path dir = fs::path(MW3_TEST_TMP) / "update_download_homes";
  fs::remove_all(dir, ec);
  const char* names[] = {"MW3-Rebuild-Setup-9.0.0.exe", "MW3-Rebuild-9.0.0-mac.dmg", "MW3-Rebuild-9.0.0-x86_64.AppImage"};
  const std::string ours = names[u::this_platform() == u::Platform::kWindows ? 0 : u::this_platform() == u::Platform::kMac ? 1 : 2];
  const std::string bytes(200000, 'q');
  const std::string sum = rekit::sha256_hex(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
  const auto put = [&](const char* home, const std::string& name, const std::string& text) {
    fs::create_directories(dir / home, ec);
    std::ofstream(dir / home / name, std::ios::binary) << text;
  };
  const auto url = [&](const char* home, const std::string& n) { return "file://" + (dir / home / n).string(); };
  const auto lay = [&]() {
    fs::remove_all(dir, ec);
    for (const char* home : {"gh", "bk"}) put(home, ours, bytes), put(home, "SHA256SUMS", sum + "  " + ours + "\n");
  };
  u::Release r;
  r.tag = "v9.0.0";
  r.assets = {{ours, url("gh", ours), bytes.size()}, {"SHA256SUMS", url("gh", "SHA256SUMS"), 0}};
  r.other_homes = {{{ours, url("bk", ours), bytes.size()}, {"SHA256SUMS", url("bk", "SHA256SUMS"), 0}}};
  r.unsigned_sums = true, r.other_unsigned_sums = {true};  // both homes' unsigned SHA256SUMS taken here
  setenv("MW3_UPDATE_API", "file:///fixture", 1);
  const std::string to = (dir / "download").string();
  // Both there: GitHub's (the backup's file removed afterwards would not matter; its copy is never read).
  lay();
  fs::remove(dir / "bk" / ours, ec);
  sdlfe::Downloaded d = sdlfe::download_update(r, to, nullptr);
  EXPECT(d.ok);
  // GitHub's file gone: the backup's.
  lay();
  fs::remove(dir / "gh" / ours, ec);
  d = sdlfe::download_update(r, to, nullptr);
  std::printf("  github's gone: %s\n", d.words.c_str());
  EXPECT(d.ok && fs::file_size(d.path, ec) == bytes.size());
  // GitHub's file damaged: not its checksum, the backup's taken.
  lay();
  put("gh", ours, std::string(200000, 'x'));
  d = sdlfe::download_update(r, to, nullptr);
  EXPECT(d.ok && rekit::sha256_file_hex(d.path) == sum);
  // GitHub without SHA256SUMS: its file checked against the backup's line (the backup's file gone, so GitHub's served).
  lay();
  fs::remove(dir / "gh" / "SHA256SUMS", ec), fs::remove(dir / "bk" / ours, ec);
  d = sdlfe::download_update(r, to, nullptr);
  EXPECT(d.ok);
  // No line for it anywhere (GitHub's SHA256SUMS listed but gone, the backup's without the file): nothing taken,
  // GitHub's reason said.
  lay();
  fs::remove(dir / "gh" / "SHA256SUMS", ec), put("bk", "SHA256SUMS", sum + "  other.dmg\n");
  d = sdlfe::download_update(r, to, nullptr);
  std::printf("  no line: %s\n", d.words.c_str());
  EXPECT(!d.ok && d.words.rfind("Could not download the release's SHA256SUMS: ", 0) == 0 && !fs::exists(fs::path(to) / ours));
  // Neither home lists a SHA256SUMS: nothing downloaded.
  u::Release no_sums = r;
  no_sums.assets.pop_back(), no_sums.other_homes[0].pop_back();
  d = sdlfe::download_update(no_sums, to, nullptr);
  EXPECT(!d.ok && d.words.find("no SHA256SUMS") != std::string::npos && !fs::exists(fs::path(to) / ours));
  // Both files gone: GitHub's reason.
  lay();
  fs::remove(dir / "gh" / ours, ec), fs::remove(dir / "bk" / ours, ec);
  d = sdlfe::download_update(r, to, nullptr);
  EXPECT(!d.ok && d.words.rfind("The download stopped: ", 0) == 0 && !fs::exists(fs::path(to) / ours));
  // Cancelled on GitHub's: the backup is not tried.
  lay();
  int calls = 0;
  d = sdlfe::download_update(r, to, [&](double, uint64_t, bool) { return ++calls, false; });
  EXPECT(!d.ok && d.cancelled && calls == 1);
  unsetenv("MW3_UPDATE_API");
  fs::remove_all(dir, ec);
}

#if !defined(_WIN32)  // the rest of the launcher's tests: posix_spawn, FIFOs
#if defined(__APPLE__)
// A disk image's detach behind an install (2.0.1, sdl_games.h detach_soon) and the leftovers cleared at a start
// (clear_old_mounts): detach_soon returns at once, then the image is detached and its mount folder removed; at a start, a
// mount folder of ours from long ago is detached when attached and removed when empty, and one made just now (an install
// under way), one with files in it, and anything not named as ours are left alone.
TEST(sdl_games_detaches_disk_images_behind_the_install) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path root = fs::path(MW3_TEST_TMP) / "detach_soon";
  fs::remove_all(root, ec);
  fs::create_directories(root / "dmg-src", ec);
  std::ofstream(root / "dmg-src" / "readme.txt") << "x";
  const std::string dmg = (root / "test.dmg").string();
  if (std::system(("/usr/bin/hdiutil create -quiet -fs HFS+ -volname 'MW3 Detach' -srcfolder '" + (root / "dmg-src").string() + "' -format UDZO '" + dmg + "'").c_str()) != 0) {
    std::printf("  no hdiutil here: skipped\n");
    return;
  }
  // The temp folder on the system's volume, as a player's: macOS refuses a mount on an external volume's folder.
  const fs::path tmp = fs::temp_directory_path(ec) / ("mw3-detach-" + std::to_string(getpid()));
  fs::remove_all(tmp, ec);
  fs::create_directories(tmp, ec);
  const auto attach = [&](const fs::path& mnt) {
    fs::create_directories(mnt, ec);
    return std::system(("/usr/bin/hdiutil attach -quiet -nobrowse -readonly -noautoopen -mountpoint '" + mnt.string() + "' '" + dmg + "'").c_str()) == 0;
  };
  const auto mounted = [](const fs::path& mnt) {
    std::string all;
    FILE* p = popen("/sbin/mount", "r");
    char buf[1024];
    while (p && std::fgets(buf, sizeof buf, p)) all += buf;
    if (p) pclose(p);
    return all.find(" on " + mnt.string() + " ") != std::string::npos || all.find(" on /private" + mnt.string() + " ") != std::string::npos;
  };
  const auto gone_soon = [&](const fs::path& mnt) {
    for (int i = 0; i < 300 && (mounted(mnt) || fs::exists(mnt, ec)); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return !mounted(mnt) && !fs::exists(mnt, ec);
  };
  const std::string now = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  // detach_soon: back at once, the image detached and its folder removed soon after.
  const fs::path a = tmp / ("MW3-Rebuild-game-mount-" + now + "-1");
  EXPECT(attach(a) && mounted(a) && fs::exists(a / "readme.txt", ec));
  const auto t0 = std::chrono::steady_clock::now();
  sdlfe::games::detach_soon(a.string());
  const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::printf("  detach_soon returned in %.2f s\n", took);
  EXPECT(took < 2.0);
  EXPECT(gone_soon(a));
  // clear_old_mounts, TMPDIR the test's folder.
  const fs::path old_attached = tmp / "MW3-Rebuild-update-mount-1-1", old_empty = tmp / "MW3-Rebuild-game-mount-2-1",
                 fresh = tmp / ("MW3-Rebuild-game-mount-" + now + "-2"), old_full = tmp / "MW3-Rebuild-game-mount-3-1",
                 unnumbered = tmp / "MW3-Rebuild-game-mount-mine", other = tmp / "Somebody-mount-1-1";
  EXPECT(attach(old_attached));
  for (const fs::path& d : {old_empty, fresh, old_full, unnumbered, other}) fs::create_directories(d, ec);
  std::ofstream(old_full / "keep.txt") << "mine";
  const char* tmp_was = std::getenv("TMPDIR");
  const std::string tmp_keep = tmp_was ? tmp_was : "";
  setenv("TMPDIR", tmp.c_str(), 1);
  const int cleared = sdlfe::games::clear_old_mounts();
  if (tmp_was) setenv("TMPDIR", tmp_keep.c_str(), 1);
  else unsetenv("TMPDIR");
  std::printf("  clear_old_mounts: %d cleared\n", cleared);
  EXPECT(cleared == 2 && !fs::exists(old_empty, ec));
  EXPECT(gone_soon(old_attached));
  EXPECT(fs::exists(fresh, ec) && fs::exists(old_full / "keep.txt", ec) && fs::exists(unnumbered, ec) && fs::exists(other, ec));
  for (const fs::path& m : {a, old_attached})
    if (mounted(m)) std::system(("/usr/bin/hdiutil detach -quiet -force '" + m.string() + "'").c_str());  // never left attached
  fs::remove_all(tmp, ec);
  if (!check::failures) fs::remove_all(root, ec);
}

// The Mac's install (sitting 4): a real DMG holding a new app, attached read-only, its app swapped in for the old
// one (MW3_UPDATE_TARGET in place of the running app), detached; a target that is not our app, or a DMG without one,
// changes nothing. The settings folder beside it untouched.
TEST(sdl_update_installs_the_mac_app) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path root = fs::path(MW3_TEST_TMP) / "update_mac";
  fs::remove_all(root, ec);
  const auto put = [&](const fs::path& p, const std::string& t) {
    fs::create_directories(p.parent_path(), ec);
    std::ofstream(p) << t;
  };
  const auto text = [](const fs::path& p) {
    std::ifstream f(p);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  };
  const fs::path old_app = root / "Applications" / "MW3 Rebuild.app";
  put(old_app / "Contents" / "MacOS" / "mw3launcher", "#!/bin/sh\necho old\n");
  put(root / "Library" / "MicroProse" / "MechWarrior 3" / "mw3.cfg", "mod=one\n");
  put(root / "dmg-src" / "MW3 Rebuild.app" / "Contents" / "MacOS" / "mw3launcher", "#!/bin/sh\necho new\n");
  put(root / "dmg-src" / "MW3 Rebuild.app" / "Contents" / "Info.plist", "<plist/>\n");
  const std::string dmg = (root / "MW3-Rebuild-9.0.0-mac.dmg").string();
  const std::string make = "/usr/bin/hdiutil create -quiet -fs HFS+ -volname 'MW3 Rebuild' -srcfolder '" + (root / "dmg-src").string() +
                           "' -format UDZO '" + dmg + "'";
  if (std::system(make.c_str()) != 0) {
    std::printf("  no hdiutil here: skipped\n");
    return;
  }
  // Not our app: refused, nothing changed.
  put(root / "Elsewhere" / "Other.app" / "Contents" / "MacOS" / "other", "theirs");
  setenv("MW3_UPDATE_TARGET", (root / "Elsewhere" / "Other.app").c_str(), 1);
  sdlfe::Installed in = sdlfe::install_update(dmg);
  EXPECT(!in.ok && in.words.find("cannot replace itself") != std::string::npos && text(root / "Elsewhere" / "Other.app" / "Contents" / "MacOS" / "other") == "theirs");
  // Ours: swapped, detached, the DMG gone, the settings untouched.
  setenv("MW3_UPDATE_TARGET", old_app.c_str(), 1);
  in = sdlfe::install_update(dmg);
  std::printf("  mac install: %s (restart %s)\n", in.words.c_str(), in.restart.c_str());
  EXPECT(in.ok && text(old_app / "Contents" / "MacOS" / "mw3launcher") == "#!/bin/sh\necho new\n" && fs::exists(old_app / "Contents" / "Info.plist"));
  EXPECT(in.restart == (old_app / "Contents" / "MacOS" / "mw3launcher").string() && !fs::exists(dmg) &&
         !fs::exists(root / "Applications" / "MW3 Rebuild.app.old") && !fs::exists(root / "Applications" / "MW3 Rebuild.app.new"));
  EXPECT(text(root / "Library" / "MicroProse" / "MechWarrior 3" / "mw3.cfg") == "mod=one\n");
  std::string mounted;
  for (int i = 0; i < 300; ++i) {  // detached behind the swap (2.0.1, detach_soon): soon, not before install_update returns
    mounted.clear();
    FILE* p = popen("/sbin/mount", "r");
    char buf[512];
    while (p && std::fgets(buf, sizeof buf, p)) mounted += buf;
    if (p) pclose(p);
    if (mounted.find("MW3-Rebuild-update-mount") == std::string::npos) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  EXPECT(mounted.find("MW3-Rebuild-update-mount") == std::string::npos);  // detached
  // A DMG without our app: nothing installed.
  fs::remove_all(root / "dmg-src" / "MW3 Rebuild.app", ec);
  put(root / "dmg-src" / "readme.txt", "no app");
  std::system(make.c_str());
  in = sdlfe::install_update(dmg);
  EXPECT(!in.ok && in.words.find("holds no MW3 Rebuild app") != std::string::npos && text(old_app / "Contents" / "MacOS" / "mw3launcher") == "#!/bin/sh\necho new\n");
  unsetenv("MW3_UPDATE_TARGET");
  fs::remove_all(root, ec);
}
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
// CHECK FOR UPDATES on the main screen (sitting 1; in place since 2026-11-24): the releases list asked
// (MW3_UPDATE_API, a fixture), its verdict in a box when newer, else on the status line: a newer package release, none newer, an answer that isn't one; PM's releases and drafts left out.
TEST(sdl_program_launcher_checks_for_updates) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path dir = fs::path(MW3_TEST_TMP) / "launcher_updates";
  fs::remove_all(dir, ec);
  fs::create_directories(dir / "wd", ec);
  std::ofstream(dir / "wd" / "mw3.cfg") << "install=" << (dir / "install").string() << "\n";
  const auto rel = [](const char* tag, bool draft) {
    return std::string("{\"tag_name\":\"") + tag + "\",\"draft\":" + (draft ? "true" : "false") +
           ",\"prerelease\":false,\"body\":\"## Fixes\\r\\n- **Lasers** hit again\\n- The `c1m2` crash\",\"assets\":[]}";
  };
  // The newer release with this platform's package and its SHA256SUMS (sitting 3: DOWNLOAD AND UPDATE pressed).
  namespace u = upgrades::update;
  const char* names[] = {"MW3-Rebuild-Setup-98.1.0.exe", "MW3-Rebuild-98.1.0-mac.dmg", "MW3-Rebuild-98.1.0-x86_64.AppImage"};
  const std::string ours = names[u::this_platform() == u::Platform::kWindows ? 0 : u::this_platform() == u::Platform::kMac ? 1 : 2];
  const std::string bytes(5000, 'n');
  std::ofstream(dir / ours, std::ios::binary) << bytes;
  std::ofstream(dir / "SHA256SUMS") << rekit::sha256_hex(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()) << "  " << ours << "\n";
  // Its SHA256SUMS signed by test key 1 (the sources file's), so a home whose unsigned sums are not taken (the
  // first of two lists) has it downloaded too; newer-unsigned's carries no .sig.
  sign_fixture((dir / "SHA256SUMS").string(), {1});
  write_sources((dir / "sources.json").string(), {"file:///nowhere/catalog.json"}, {1});
  std::string plain = rel("v98.1.0", false);
  plain.replace(plain.find("\"assets\":[]"), 11, "\"assets\":[{\"name\":\"" + ours + "\",\"size\":5000,\"browser_download_url\":\"file://" +
                                                 (dir / ours).string() + "\"},{\"name\":\"SHA256SUMS\",\"browser_download_url\":\"file://" +
                                                 (dir / "SHA256SUMS").string() + "\"}]");
  std::string newest = plain;
  newest.replace(newest.rfind("}]"), 2, "},{\"name\":\"SHA256SUMS.sig\",\"browser_download_url\":\"file://" + (dir / "SHA256SUMS.sig").string() + "\"}]");
  std::ofstream(dir / "newer.json") << "[" << rel("pm-v99.0.0", false) << "," << rel("v99.0.0", true) << "," << newest << "]";
  std::ofstream(dir / "newer-unsigned.json") << "[" << plain << "]";
  setenv("MW3_SOURCES_FILE", (dir / "sources.json").c_str(), 1);
  std::string older = newest;  // an older release with its bundle (one without is never the update)
  older.replace(older.find("v98.1.0"), 7, "v0.0.1");
  std::ofstream(dir / "older.json") << "[" << older << "]";
  std::ofstream(dir / "broken.json") << "<html>API rate limit exceeded</html>";
  setenv("MW3_LAUNCHER_FRAMES", "240", 1);
  setenv("MW3_LAUNCHER_NAV", "next,prev,prev,press", 1);  // MECHWARRIOR 3, then back round: QUIT, UPDATES
  unsetenv("MW3_PM_BIN");
  // `name`: one list, or two as the backup's and GitHub's ("older|newer"; "missing" answers nothing).
  auto run = [&](const char* spec) {
    std::string api, name = spec;
    for (size_t at = 0, bar; at <= name.size(); at = bar + 1) {
      bar = name.find('|', at);
      if (bar == std::string::npos) bar = name.size();
      api += (at ? "|" : "") + ("file://" + (dir / (name.substr(at, bar - at) + ".json")).string());
    }
    std::replace(name.begin(), name.end(), '|', '_');
    const bool is_newer = name.find("newer") != std::string::npos;  // 98.1.0 on either home
    const bool refused = name.find("unsigned") != std::string::npos;  // its download refused: unsigned on the untrusted home
    setenv("MW3_UPDATE_API", api.c_str(), 1);
    if (is_newer) setenv("MW3_LAUNCHER_UPDATE_GO", "1", 1);
    else unsetenv("MW3_LAUNCHER_UPDATE_GO");
    const std::string tmp_was = std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "";
    fs::create_directories(dir / "tmp", ec);
    setenv("TMPDIR", (dir / "tmp").c_str(), 1);  // the download's folder in the test's own place
    const std::string log = (dir / (std::string(name) + ".log")).string();
    const pid_t pid = spawn_launcher({"--write-dir", (dir / "wd").string(), "--window", "640x480", "--hidden", "--nosound"}, log);
    int status = 0;
    EXPECT(pid > 0 && waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (tmp_was.empty()) unsetenv("TMPDIR");
    else setenv("TMPDIR", tmp_was.c_str(), 1);
    if (is_newer) {
      EXPECT(fs::exists(dir / "tmp" / "MW3-Rebuild-update" / ours, ec) != refused);
      fs::remove(dir / "tmp" / "MW3-Rebuild-update" / ours, ec);
    }
    std::ifstream f(log);
    const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (text.find("launcher: update: ") == std::string::npos)  // where the steps went
      for (size_t a = text.find("focus "); a != std::string::npos; a = text.find("focus ", a + 1))
        std::printf("    %s\n", text.substr(a, text.find('\n', a) - a).c_str());
    const size_t at = text.find("launcher: update: ");
    const std::string said = at == std::string::npos ? "(none)" : text.substr(at + 18, text.find('\n', at) - at - 18);
    std::printf("  %s: %s\n", spec, said.c_str());
    if (refused)
      EXPECT(text.find("QUIT, UPDATE AVAILABLE 98.1.0, UPDATE LAUNCHER, NOT NOW\n") != std::string::npos &&
             text.find("its SHA256SUMS is not signed") != std::string::npos && text.find("Downloaded and checked") == std::string::npos);
    else if (is_newer)  // sitting 2: the notes and the question
      EXPECT(text.find("QUIT, UPDATE AVAILABLE 98.1.0, UPDATE LAUNCHER, NOT NOW\n") != std::string::npos &&  // the box over the games
             text.find("update notes: Fixes\n") != std::string::npos && text.find("update notes: \xe2\x80\xa2 Lasers hit again\n") != std::string::npos &&
             text.find("update notes: \xe2\x80\xa2 The c1m2 crash\n") != std::string::npos &&
             text.find("launcher: update: Downloaded and checked: " + ours + ".\n") != std::string::npos &&
             text.find("cannot replace itself") != std::string::npos);  // run from the build, not an app or an AppImage: refused
    else
      EXPECT(text.find("UPDATE LAUNCHER") == std::string::npos && text.find("screen update:") == std::string::npos);
    return said;
  };
  EXPECT(run("newer").rfind("MechWarrior 3 Rebuild 98.1.0 is out (this is ", 0) == 0);
  EXPECT(run("older").rfind("This is the newest MechWarrior 3 Rebuild (", 0) == 0);
  EXPECT(run("broken").find("the releases list could not be read") != std::string::npos);
  // Both homes (2026-11-22), the backup's list then GitHub's: the newest on either; one home down is fine.
  EXPECT(run("older|newer").rfind("MechWarrior 3 Rebuild 98.1.0 is out (this is ", 0) == 0);  // newest on GitHub only
  EXPECT(run("newer|older").rfind("MechWarrior 3 Rebuild 98.1.0 is out (this is ", 0) == 0);  // on the backup only
  EXPECT(run("newer|missing").rfind("MechWarrior 3 Rebuild 98.1.0 is out (this is ", 0) == 0);  // GitHub down
  EXPECT(run("missing|older").rfind("This is the newest MechWarrior 3 Rebuild (", 0) == 0);  // the backup down
  EXPECT(run("missing|broken").find("Could not check for updates") == 0);
  // Signed data (2026-11-26): the newest only on the first list, its SHA256SUMS unsigned: shown, not downloaded.
  EXPECT(run("newer-unsigned|older").rfind("MechWarrior 3 Rebuild 98.1.0 is out (this is ", 0) == 0);  // and run() finds it refused
  for (const char* k : {"MW3_LAUNCHER_FRAMES", "MW3_LAUNCHER_NAV", "MW3_UPDATE_API", "MW3_LAUNCHER_UPDATE_GO", "MW3_SOURCES_FILE"}) unsetenv(k);
  fs::remove_all(dir, ec);
}


// A quit never waits on the network (lead 2026-11-23): with both start checks stuck on a list that never answers
// (a FIFO nobody writes, so curl waits in its open for good), the launcher quits in moments, not after the curl
// timeouts (20 s for its own check, 15 s for each game's), the stuck curls stopped (stop_asking).
TEST(sdl_program_launcher_quits_without_waiting_on_the_network) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path dir = fs::path(MW3_TEST_TMP) / "launcher_quit_stalled";
  fs::remove_all(dir, ec);
  fs::create_directories(dir / "wd", ec);
  std::ofstream(dir / "wd" / "mw3.cfg") << "install=" << (dir / "install").string() << "\n";
  const fs::path fifo = dir / "never.json";
  EXPECT(::mkfifo(fifo.c_str(), 0600) == 0);
  setenv("MW3_UPDATE_API", ("file://" + fifo.string()).c_str(), 1);
  setenv("MW3_GAMES_API", ("file://" + fifo.string()).c_str(), 1);
  setenv("MW3_GAMES_DIR", (dir / "games").c_str(), 1);
  setenv("MW3_LAUNCHER_FRAMES", "30", 1);  // quit about half a second in, both checks still waiting
  unsetenv("MW3_PM_BIN");
  const std::string log = (dir / "quit.log").string();
  const auto t0 = std::chrono::steady_clock::now();
  const pid_t pid = spawn_launcher({"--write-dir", (dir / "wd").string(), "--window", "640x480", "--hidden", "--nosound"}, log);
  int status = 0;
  const bool ended = pid > 0 && waited(pid, &status, 60);
  const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  for (const char* k : {"MW3_UPDATE_API", "MW3_GAMES_API", "MW3_GAMES_DIR", "MW3_LAUNCHER_FRAMES"}) unsetenv(k);
  std::ifstream f(log);
  const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  std::printf("  quit after %.1f s; stopped: %d\n", s, has(text, "the launcher is closing"));
  EXPECT(ended && WIFEXITED(status) && WEXITSTATUS(status) == 0);
  EXPECT(s < 5.0);
  EXPECT(has(text, "update check at file://") && has(text, "the launcher is closing"));  // its own check: stopped
  fs::remove_all(dir, ec);
}
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
// The movies on a wide screen (W1, docs/fmv.md section 8): INTRO, C4WIN and
// a 4:3 briefing imported (mw3import, FFmpeg), their shapes measured from the
// pictures (INTRO letterboxed throughout; C4WIN letterboxed, then its 4:3
// ending; C1M1 4:3), then hidden 1920x1080 shots on a 16:9 screen mode:
// INTRO across the whole width with bars above and below, C1M1 at the
// screen's height with black sides. Skipped without FFmpeg in the importer.
namespace {
}  // namespace


#endif
#endif

#if !defined(_WIN32)  // posix_spawn
#endif

#if !defined(_WIN32)  // posix_spawn's std::system
#endif



#if !defined(_WIN32) && !defined(MW3_PM_SUITE)  // posix_spawn, waitpid; MechWarrior 3's c1 pages (not Pirate's Moon's suite)
#endif

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
#endif

// One bar for a press (2026-11-27, docs/launcher.md, One bar for a press), on a fake clock: two parts of known sizes
// give one rising total; the speed settles after "starting..."; the time left; a part dropped (its game has its files)
// leaves the total and says why; a stall says "waiting for the server..."; no size known, MB only; a setup's share of
// the bar after the bytes.
TEST(launcher_job_one_bar_for_a_press) {
  using sdlfe::job::Job;
  const std::string dot = " \xc2\xb7 ";
  {  // two files, 300,000 and 200,000 bytes: one total, rising across the part's change
    Job j("BattleTech", {{"game:btech", "the game", 300000}, {"original:btech", "the original game files from file", 200000}});
    double last = -1;
    bool rising = true;
    for (int s = 0; s <= 30; ++s) {
      const double t = s * 0.5;
      if (s <= 15) j.bytes("game:btech", 20000ull * s, t);
      else j.bytes("original:btech", 13333ull * (s - 15) + (s == 30 ? 5 : 0), t);
      const sdlfe::job::View v = j.view(t);
      rising = rising && v.fraction >= last, last = v.fraction;
      if (s == 1 || s == 10 || s == 16 || s == 30) std::printf("  t %.1f: %.3f %s | %s\n", t, v.fraction, v.line.c_str(), v.part.c_str());
      if (s == 1) EXPECT(v.line == "BattleTech: 0.0 of 0.5 MB" + dot + "starting..." && v.part == "the game (0.3 MB)");
      if (s == 10) EXPECT(v.line == "BattleTech: 0.2 of 0.5 MB" + dot + "40 KB/s" + dot + "less than a minute left");
      if (s == 16) EXPECT(v.line == "BattleTech: 0.3 of 0.5 MB" + dot + "starting..." && v.part == "the original game files from file (0.2 MB)");
    }
    EXPECT(rising && j.total() == 500000 && j.got() == 500000 && std::fabs(last - 1.0) < 1e-9);
  }
  {  // the lead's example: 287 of 668 MB at 4.1 MB/s, about 2 min left
    Job k("MechWarrior 3", {{"game:mw3", "the game", 43000000}, {"original:mw3", "the original game files from the Internet Archive", 625000000},
                            {"setup:mw3", "setting up", 0, true}});
    k.bytes("game:mw3", 43000000, 0);
    for (int s = 0; s <= 6; ++s) k.bytes("original:mw3", 219400000ull + 4100000ull * s, 1 + s);
    const sdlfe::job::View w = k.view(7);
    std::printf("  %s | %s (%.3f)\n", w.line.c_str(), w.part.c_str(), w.fraction);
    EXPECT(w.line == "MechWarrior 3: 287 of 668 MB" + dot + "4.1 MB/s" + dot + "about 2 min left");
    EXPECT(w.part == "the original game files from the Internet Archive (625 MB)");
    EXPECT(std::fabs(w.fraction - 0.85 * 287.0 / 668.0) < 0.002);
    // Its check, then the setup's share by its own fractions.
    k.stage("original:mw3", "Checking the original game files", 0, 8);
    EXPECT(k.view(8).line == "MechWarrior 3: Checking the original game files" && std::fabs(k.view(8).fraction - 0.85) < 1e-9);
    k.stage("setup:mw3", "Copying the game files", 0.5, 9);
    std::printf("  %s | %s (%.3f)\n", k.view(9).line.c_str(), k.view(9).part.c_str(), k.view(9).fraction);
    EXPECT(k.view(9).line == "MechWarrior 3: Copying the game files" && k.view(9).part == "setting up" && std::fabs(k.view(9).fraction - 0.925) < 1e-9);
  }
  {  // the game's status says it has its files: the original and its setup dropped, said; the bar jumps forward
    Job j("MechWarrior 3", {{"game:mw3", "the game", 43000000}, {"original:mw3", "the original game files from the Internet Archive", 625000000},
                            {"setup:mw3", "setting up", 0, true}});
    j.bytes("game:mw3", 21500000, 0);
    EXPECT(j.total() == 668000000 && j.view(0).line.rfind("MechWarrior 3: 22 of 668 MB", 0) == 0);
    j.stage("game:mw3", "Unpacking", 0, 1);
    j.drop("original:mw3", "MechWarrior 3 has its game files already: nothing more to download");
    j.drop("setup:mw3", "MechWarrior 3 has its game files already: nothing more to download");
    const sdlfe::job::View v = j.view(2);
    std::printf("  dropped: %s | %s (%.3f)\n", v.line.c_str(), v.part.c_str(), v.fraction);
    EXPECT(j.total() == 43000000 && !j.has("original:mw3") && v.fraction == 1.0 && v.part == "MechWarrior 3 has its game files already: nothing more to download");
  }
  {  // a stall: waiting for the server before curl's minute ends it
    Job j("PM", {{"game:pm", "the game", 10000000}});
    j.bytes("game:pm", 0, 0);
    j.bytes("game:pm", 1000000, 1);
    j.bytes("game:pm", 2000000, 3);
    j.bytes("game:pm", 2000000, 12);
    EXPECT(j.view(12).line.find("waiting") == std::string::npos);
    j.bytes("game:pm", 2000000, 13.5);
    std::printf("  stalled: %s\n", j.view(13.5).line.c_str());
    EXPECT(j.view(13.5).line == "PM: 2.0 of 10 MB" + dot + "waiting for the server...");
    j.bytes("game:pm", 2100000, 14);
    EXPECT(j.view(14).line.find("waiting") == std::string::npos);
  }
  {  // no size known: MB done only
    Job j("Revenge", {{"game:rev", "the game", 0}});
    j.bytes("game:rev", 0, 0);
    j.bytes("game:rev", 1500000, 3);
    EXPECT(j.view(3).line == "Revenge: 1.5 MB" && j.view(3).part == "the game");
  }
  EXPECT(sdlfe::job::left_words(30) == "less than a minute left" && sdlfe::job::left_words(4800) == "about 1 h 20 min left" &&
         sdlfe::job::speed_words(850000) == "850 KB/s" && sdlfe::job::mb(624970304) == "625 MB");
}

#if !defined(_WIN32)
// A dropped download continues where it stopped (2026-11-27): a stand-in server on this computer sends half of a
// 3 MB file and closes the connection; the download asks again 5 s later with a Range from the part's end, gets the
// rest (206), and the whole file arrives byte for byte, in two requests.
TEST(launcher_download_resumes_after_a_dropped_connection) {
  std::string body(3 * 1000 * 1000, '\0');
  for (size_t i = 0; i < body.size(); ++i) body[i] = static_cast<char>((i * 131 + i / 7) & 0xff);
  const int srv = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in a{};
  a.sin_family = AF_INET, a.sin_addr.s_addr = htonl(INADDR_LOOPBACK), a.sin_port = 0;
  EXPECT(srv >= 0 && bind(srv, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0 && listen(srv, 4) == 0);
  socklen_t al = sizeof a;
  getsockname(srv, reinterpret_cast<sockaddr*>(&a), &al);
  const int port = ntohs(a.sin_port);
  std::vector<std::string> asked;
  std::thread server([&] {
    for (int n = 0; n < 2; ++n) {
      const int c = accept(srv, nullptr, nullptr);
      if (c < 0) return;
      std::string req;
      char buf[2048];
      while (req.find("\r\n\r\n") == std::string::npos) {
        const ssize_t got = recv(c, buf, sizeof buf, 0);
        if (got <= 0) break;
        req.append(buf, static_cast<size_t>(got));
      }
      asked.push_back(req);
      size_t from = 0;
      if (const size_t r = req.find("Range: bytes="); r != std::string::npos) from = std::strtoull(req.c_str() + r + 13, nullptr, 10);
      std::string head, data;
      if (from == 0) {  // the first: all promised, half sent, then closed
        head = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\nAccept-Ranges: bytes\r\n\r\n";
        data = body.substr(0, body.size() / 2);
      } else {
        head = "HTTP/1.1 206 Partial Content\r\nContent-Length: " + std::to_string(body.size() - from) + "\r\nContent-Range: bytes " +
               std::to_string(from) + "-" + std::to_string(body.size() - 1) + "/" + std::to_string(body.size()) + "\r\n\r\n";
        data = body.substr(from);
      }
      const std::string out = head + data;
      for (size_t at = 0; at < out.size();) {
        const ssize_t sent = send(c, out.data() + at, out.size() - at, 0);
        if (sent <= 0) break;
        at += static_cast<size_t>(sent);
      }
      close(c);
    }
  });
  const std::string dir = std::string(MW3_TEST_TMP) + "/resume";
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  // SDL_getenv reads SDL's own copy of the environment once one exists (an earlier test in this process made it).
  setenv("MW3_DOWNLOAD_LOCAL_HTTP", "1", 1), SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "MW3_DOWNLOAD_LOCAL_HTTP", "1", true);
  std::string error;
  bool cancelled = false;
  const bool ok = sdlfe::curl_download("http://127.0.0.1:" + std::to_string(port) + "/game.zip", dir + "/game.zip", body.size(),
                                       [](double, uint64_t, bool) { return true; }, &error, &cancelled);
  unsetenv("MW3_DOWNLOAD_LOCAL_HTTP"), SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "MW3_DOWNLOAD_LOCAL_HTTP");
  shutdown(srv, SHUT_RDWR), close(srv);
  server.join();
  std::ifstream f(dir + "/game.zip", std::ios::binary);
  const std::string got((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  std::printf("resume: %s, %zu requests, %zu bytes%s%s\n", ok ? "whole" : "failed", asked.size(), got.size(), error.empty() ? "" : ": ", error.c_str());
  EXPECT(ok && !cancelled && got == body);
  EXPECT(asked.size() == 2 && asked[1].find("Range: bytes=" + std::to_string(body.size() / 2) + "-") != std::string::npos);
  EXPECT(!std::filesystem::exists(dir + "/game.zip.part"));
}
#endif

#if !defined(_WIN32)
// Update counts (2.1.0, docs/launcher.md, Update counts), against a stand-in server: a game's UPDATE, installed and
// checked, sends one GET to the catalog's "counts" (MW3_COUNTS_TO here) with the game, both versions, the launcher's
// and where the package came from (github; mirror when GitHub's copy fails); a first install, send_update_counts
// false, or a catalog without the field send nothing. The launcher's own update is sent by the new launcher from the
// old one's note (taken once; one to another version is dropped). Every curl_get says MW3-Rebuild-launcher/<version>.
TEST(sdl_program_launcher_counts_updates) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const std::string base = std::string(MW3_TEST_TMP) + "/launcher_counts";
  fs::remove_all(base, ec);
  fs::create_directories(base, ec);
  auto read = [](const std::string& f) {
    std::ifstream in(f, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  };
  std::string version;  // this build's, as --version says it, without its "v"
  if (FILE* p = popen((std::string("'") + MW3_LAUNCHER_BIN + "' --version").c_str(), "r")) {
    char buf[256];
    if (std::fgets(buf, sizeof buf, p)) version = buf;
    pclose(p);
  }
  version = version.substr(version.rfind(' ') + 1);
  while (!version.empty() && (version.back() == '\n' || version.back() == '\r')) version.pop_back();
  if (!version.empty() && version[0] == 'v') version.erase(0, 1);
  EXPECT(!version.empty());
  // The stand-in counts server: every request kept, 204 for each.
  const int srv = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in a{};
  a.sin_family = AF_INET, a.sin_addr.s_addr = htonl(INADDR_LOOPBACK), a.sin_port = 0;
  EXPECT(srv >= 0 && bind(srv, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0 && listen(srv, 8) == 0);
  socklen_t al = sizeof a;
  getsockname(srv, reinterpret_cast<sockaddr*>(&a), &al);
  const int port = ntohs(a.sin_port);
  std::mutex mu;
  std::vector<std::string> asked;
  std::thread server([&] {
    for (;;) {
      const int c = accept(srv, nullptr, nullptr);
      if (c < 0) return;
      std::string req;
      char buf[2048];
      while (req.find("\r\n\r\n") == std::string::npos) {
        const ssize_t got = recv(c, buf, sizeof buf, 0);
        if (got <= 0) break;
        req.append(buf, static_cast<size_t>(got));
      }
      {
        std::lock_guard<std::mutex> l(mu);
        asked.push_back(req);
      }
      const char* reply = "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";
      send(c, reply, std::strlen(reply), 0);
      close(c);
    }
  });
  struct Case {
    const char* name;
    bool installed;     // btech 0.1.0 installed: DOWNLOAD is an UPDATE to 0.2.0
    bool field;         // the catalog names counts
    bool off;           // send_update_counts false in the player's settings
    bool mirror;        // GitHub's copy missing: the download from the other home
    const char* note;   // the old launcher's note, its to= ("" none; "SELF" this build's version)
    const char* want;   // the request's line, "" none
  };
  const std::string self = "kind=self&game=launcher&from=1.9.9&to=" + version + "&launcher=" + version + "&src=github";
  const std::string upd = "kind=update&game=btech&from=0.1.0&to=0.2.0&launcher=" + version;
  const std::string want_github = upd + "&src=github", want_mirror = upd + "&src=mirror";
  const Case cases[] = {
      {"update", true, true, false, false, "", want_github.c_str()},
      {"mirror", true, true, false, true, "", want_mirror.c_str()},
      {"install", false, true, false, false, "", ""},
      {"off", true, true, true, false, "", ""},
      {"no_field", true, false, false, false, "", ""},
      {"self", false, true, false, false, "SELF", self.c_str()},
      {"self_other", false, true, false, false, "0.0.1", ""},
  };
  for (const Case& c : cases) {
    const std::string dir = base + "/" + c.name;
    for (const char* d : {"/api", "/backup", "/games", "/wd/music", "/tmp", "/rel"}) fs::create_directories(dir + d, ec);
    std::ofstream(dir + "/wd/music/track02.wav") << "x";
    if (c.off) std::ofstream(dir + "/wd/launcher-settings.json") << "{\"send_update_counts\": false}";
    if (*c.note)
      std::ofstream(dir + "/wd/launcher-updated.txt") << "from=1.9.9\nto=" << (std::strcmp(c.note, "SELF") == 0 ? version : std::string(c.note)) << "\nsrc=github\n";
    for (const char* repo : {"mech3-pm-re", "mech1-remake-re", "mech1-re", "inception-re", "inception-remake-re", "revenge-re"})
      std::ofstream(dir + "/api/" + repo + ".json") << "[]", std::ofstream(dir + "/backup/" + repo + ".json") << "[]";
    const std::string sh = "#!/bin/sh\nif [ \"$1\" = --status ]; then echo '{\"kit\": 1, \"game\": \"btech\", \"version\": \"0.1.0\", \"files\": "
                           "{\"state\": \"ready\", \"where\": \"" + dir + "\"}, \"first_run\": {\"pending\": []}, \"needs\": []}'; exit 0; fi\n"
                           ": > '" + dir + "/played'\nexit 0\n";
    if (c.installed) {
      fs::create_directories(dir + "/games/btech", ec);
      std::ofstream(dir + "/games/rebuild-games.txt") << "# the launcher's games folder\n";  // as an install leaves it
      std::ofstream(dir + "/games/btech/rebuild-game.txt") << "game=btech\ntag=btech-v0.1.0\nfrom=github\n";
      std::ofstream(dir + "/games/btech/btech") << sh;
      fs::permissions(dir + "/games/btech/btech", fs::perms::owner_all);
    }
    const std::string zip = dir + "/rel/btech-v0.2.0-test.zip";
    {
      archive* z = archive_write_new();
      archive_write_set_format_zip(z);
      archive_write_open_filename(z, zip.c_str());
      archive_entry* e = archive_entry_new();
      archive_entry_set_pathname(e, "btech");
      archive_entry_set_size(e, static_cast<la_int64_t>(sh.size()));
      archive_entry_set_filetype(e, AE_IFREG);
      archive_entry_set_perm(e, 0755);
      archive_write_header(z, e);
      archive_write_data(z, sh.data(), sh.size());
      archive_entry_free(e);
      archive_write_close(z);
      archive_write_free(z);
    }
    const std::string zbytes = read(zip);
    std::ofstream(dir + "/rel/SHA256SUMS") << rekit::sha256_hex(reinterpret_cast<const uint8_t*>(zbytes.data()), zbytes.size()) << "  btech-v0.2.0-test.zip\n";
    auto list = [&](const std::string& file_url) {
      return "[{\"tag_name\": \"btech-v0.2.0\", \"draft\": false, \"prerelease\": false, \"body\": \"x\", \"assets\": [{\"name\": \"btech-v0.2.0-test.zip\", "
             "\"size\": " + std::to_string(zbytes.size()) + ", \"browser_download_url\": \"" + file_url + "\"}, {\"name\": \"SHA256SUMS\", \"size\": 10, "
             "\"browser_download_url\": \"file://" + dir + "/rel/SHA256SUMS\"}]}]";
    };
    std::ofstream(dir + "/api/battletech-re.json") << list(c.mirror ? "file://" + dir + "/rel/gone.zip" : "file://" + zip);  // GitHub's
    std::ofstream(dir + "/backup/battletech-re.json") << list("file://" + zip);
    std::ofstream(dir + "/catalog.json")
        << "{\"catalog\": 1, \"revision\": 4, " << (c.field ? "\"counts\": \"https://reverse-engineered.com/launcher/hit\", " : "")
        << "\"families\": [{\"key\": \"bt\", \"name\": \"BattleTech\", \"order\": 4}], \"games\": [{\"key\": \"btech\", \"family\": \"bt\", "
           "\"order\": 7, \"state\": \"released\", \"name\": \"BattleTech Rebuild\", \"title\": \"T\", \"heading\": \"H\", \"button\": \"B\", "
           "\"description\": \"A line.\", \"repo\": \"battletech-re\", \"tag_prefix\": \"btech-v\", \"program\": \"btech\", \"assets\": {\"windows\": "
           "\"{tag}-test.zip\", \"mac\": \"{tag}-test.zip\", \"linux\": \"{tag}-test.zip\"}, \"badge\": \"badge_mw3.png\", \"min_launcher\": \"1.5.2\"}]}";
    std::vector<std::pair<std::string, std::string>> env{
        {"MW3_GAMES_API", c.mirror ? "file://" + dir + "/backup/{repo}.json|file://" + dir + "/api/{repo}.json" : "file://" + dir + "/api/{repo}.json"},
        {"MW3_GAMES_DIR", dir + "/games"}, {"MW3_CATALOG_URL", "file://" + dir + "/catalog.json"}, {"MW3_LAUNCHER_FRAMES", "300"},
        {"MW3_NO_IMPORT", "1"}, {"TMPDIR", dir + "/tmp"}, {"MW3_COUNTS_TO", "http://127.0.0.1:" + std::to_string(port) + "/hit"},
        {"MW3_DOWNLOAD_LOCAL_HTTP", "1"}, {"MW3_CURL_ARGS_TO", dir + "/curl-args"}, {"MW3_PM_BIN", ""}, {"MW3_MW1_BIN", ""},
        {"MW3_MW1R_BIN", ""}, {"MW3_CHI_BIN", ""}, {"MW3_CHIRM_BIN", ""}};
    if (!*c.note) env.push_back({"MW3_LAUNCHER_GAME_INSTALL", "btech"});
    const char* tmp_was = std::getenv("TMPDIR");
    const std::string tmp_keep = tmp_was ? tmp_was : "";
    size_t before;
    {
      std::lock_guard<std::mutex> l(mu);
      before = asked.size();
    }
    for (const auto& [k, v] : env) setenv(k.c_str(), v.c_str(), 1);
    const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--window", "640x480", "--nosound", "--hidden"}, dir + "/launcher.log");
    for (const auto& [k, v] : env) unsetenv(k.c_str());
    if (tmp_was) setenv("TMPDIR", tmp_keep.c_str(), 1);
    int status = 0;
    EXPECT(waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    std::vector<std::string> mine;
    {
      std::lock_guard<std::mutex> l(mu);
      mine.assign(asked.begin() + static_cast<long>(before), asked.end());
    }
    const std::string log = read(dir + "/launcher.log"), args = read(dir + "/curl-args");
    std::printf("counts, %s: %zu request(s)\n", c.name, mine.size());
    for (const std::string& r : mine) std::printf("    %s\n", r.substr(0, r.find("\r\n")).c_str());
    for (const char* what : {"counts: ", "games: btech now", "Nothing was installed"})
      for (size_t at = 0; (at = log.find(what, at)) != std::string::npos; ++at) std::printf("    %s\n", log.substr(at, log.find('\n', at) - at).c_str());
    if (*c.want) {
      EXPECT(mine.size() == 1);
      if (!mine.empty()) {
        EXPECT(mine[0].rfind(std::string("GET /hit?") + c.want + " HTTP/1.1\r\n", 0) == 0);
        EXPECT(has(mine[0], ("\r\nUser-Agent: MW3-Rebuild-launcher/" + version + "\r\n").c_str()));
      }
      EXPECT(has(log, "counts: sent http://127.0.0.1:"));
    } else {
      EXPECT(mine.empty());
    }
    if (c.installed && !*c.note) EXPECT(has(log, "games: btech now btech-v0.2.0 at "));
    if (c.off) EXPECT(has(log, "not sent (send_update_counts is off)"));
    if (!c.field) EXPECT(has(log, "not sent (the catalog names no counts address)"));
    if (std::strcmp(c.name, "self_other") == 0) EXPECT(has(log, ("counts: an update note to 0.0.1, but this is " + version + ": not sent").c_str()));
    if (*c.note) EXPECT(!fs::exists(dir + "/wd/launcher-updated.txt", ec));  // taken: never twice
    // The catalog fetch's User-Agent: exactly MW3-Rebuild-launcher/<version>.
    EXPECT(has(args, ("\n--user-agent\nMW3-Rebuild-launcher/" + version + "\n").c_str()) && !has(args, "\n--user-agent\nMW3-Rebuild-launcher\n"));
    EXPECT(!fs::exists(dir + "/played", ec));
  }
  shutdown(srv, SHUT_RDWR), close(srv);
  server.join();
  if (!check::failures) fs::remove_all(base, ec);
}
#endif

#if !defined(_WIN32)
// A game's --status read whatever else it prints (2.1.1, mech1-remake-re #2): its errors share the stream, so a line
// holding a '{' before the status (here {"x": "y"}, on stdout and on stderr) or text with a '}' after it must not
// shift the object; the status is found from its own {"kit": to its matching '}'. A plain answer reads as before.
TEST(sdl_games_reads_a_status_among_other_text) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const std::string dir = std::string(MW3_TEST_TMP) + "/status_slice";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  const std::string status =
      "{\"kit\": 1, \"game\": \"btech\", \"version\": \"0.1.0\", \"files\": {\"state\": \"ready\", \"where\": \"C:\\\\Games\\\\{x}\"}, "
      "\"first_run\": {\"pending\": []}, \"needs\": []}";
  struct Case {
    const char* name;
    std::string before, after, err;  // printed before and after the status, and on stderr first
  };
  const Case cases[] = {
      {"plain", "", "", ""},
      {"before", "log: {\"x\": \"y\"}\nsome text\n", "", ""},
      {"after", "", "\ndone {\"x\": \"y\"} }\n", ""},
      {"stderr", "", "", "warning: {\"x\": \"y\"}\n"},
      {"both", "{\"x\": \"y\"}\n", "\n}\n", "{oops\n"},
  };
  for (const Case& c : cases) {
    const std::string bin = dir + "/" + c.name;
    {
      std::ofstream sh(bin);
      sh << "#!/bin/sh\n";
      if (!c.err.empty()) sh << "printf '%s' '" << c.err << "' >&2\n";
      sh << "printf '%s' '" << c.before << status << c.after << "'\nexit 0\n";
    }
    fs::permissions(bin, fs::perms::owner_all);
    rekit::Status s;
    std::string why;
    const bool ok = sdlfe::games::kit_status(bin, &s, {}, 10, &why);
    std::printf("status, %s: %s%s\n", c.name, ok ? "read" : "not read: ", why.c_str());
    EXPECT(ok && s.game == "btech" && s.version == "0.1.0" && s.state == rekit::Status::kReady && s.where == "C:\\Games\\{x}");
  }
  if (!check::failures) fs::remove_all(dir, ec);
}
#endif

// The launcher's settings file (2026-11-27, docs/launcher-config.md): the shipped file is the table's text (every key at
// its default; MW3_WRITE_SETTINGS=1 writes it); a player's values clamped to their ranges, wrong types and unknown keys
// falling back with a line each, a broken file changing nothing, and nothing of trust read from it.
TEST(launcher_settings_shipped_file_is_the_table) {
  namespace st = sdlfe::settings;
  const std::string path = std::string(MW3_ASSETS_FOR_TESTS) + "/launcher/" + st::kFileName;
  if (std::getenv("MW3_WRITE_SETTINGS")) std::ofstream(path, std::ios::binary) << st::shipped_text();
  std::ifstream f(path, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  EXPECT(text == st::shipped_text());
  st::reset();
  std::vector<std::string> lines;
  st::apply(text, "shipped", &lines);
  EXPECT(lines.size() == 1 && lines[0] == "settings: shipped: " + std::to_string(st::keys().size()) + " read");
  for (const st::Key& k : st::keys()) EXPECT(st::get(k.id) == k.def && k.def >= k.min && k.def <= k.max);
  EXPECT(static_cast<int>(st::keys().size()) == st::kCount);
  for (int i = 0; i < st::kCount; ++i) EXPECT(st::keys()[i].id == i);
  // Today's values, as launcher-config.md lists them.
  EXPECT(st::get(st::kCatalogEveryHours) == 4 && st::get(st::kReleasesCount) == 20 && st::get(st::kUpdateCount) == 40 &&
         st::get(st::kSlowBytesPerSecond) == 1024 && st::get(st::kWindowWidth) == 1280 && st::get(st::kWindowHeight) == 720 &&
         st::get(st::kSetupShare) == 0.15 && st::get(st::kHoverGain) == 0.7 && sdlfe::job::Job::stall() == 10);
}

TEST(launcher_settings_clamp_and_fall_back) {
  namespace st = sdlfe::settings;
  const auto said = [](const std::vector<std::string>& lines, const std::string& what) {
    for (const std::string& l : lines)
      if (l.find(what) != std::string::npos) return true;
    return false;
  };
  st::reset();
  std::vector<std::string> l;
  st::apply(R"({"version": 1, "_note": "skipped", "catalog_every_hours": 1, "releases_count": 500, "bar_setup_share": -2,
                "download_connect_seconds": 30.6, "vsync": false, "sound_hover_volume": "loud", "catalog_url": "https://x.example",
                "signing_keys": ["k"], "https_only": false, "window_width": 1e9})",
            "mine", &l);
  for (const std::string& s : l) std::printf("  %s\n", s.c_str());
  EXPECT(st::get(st::kCatalogEveryHours) == 4 && said(l, "catalog_every_hours 1 is outside 4 to 168 (or 0): 4 used"));
  EXPECT(st::get(st::kReleasesCount) == 40 && said(l, "releases_count 500 is outside 10 to 40: 40 used"));
  EXPECT(st::get(st::kSetupShare) == 0 && said(l, "bar_setup_share -2 is outside 0 to 0.5: 0 used"));
  EXPECT(st::get(st::kConnectSeconds) == 31 && said(l, "download_connect_seconds 30.6 is a whole number: 31 used"));
  EXPECT(st::get(st::kVsync) == 0);
  EXPECT(st::get(st::kHoverGain) == 0.7 && said(l, "sound_hover_volume is not a number: 0.7 used"));
  EXPECT(st::get(st::kWindowWidth) == 7680);
  EXPECT(said(l, "catalog_url is not a setting: ignored") && said(l, "signing_keys is not a setting: ignored") &&
         said(l, "https_only is not a setting: ignored") && !said(l, "_note") && said(l, "mine: 6 read"));
  // 0 turns the catalog's refetch off; 2 does not go under the servers' 4 h floor.
  l.clear(), st::apply(R"({"catalog_every_hours": 0})", "mine", &l);
  EXPECT(st::get(st::kCatalogEveryHours) == 0 && l.size() == 1);
  // A file that does not read changes nothing; a newer version is read for the keys this launcher knows.
  st::reset(), l.clear();
  st::apply(R"({"vsync": 0,)", "broken", &l);
  EXPECT(st::get(st::kVsync) == 1 && l.size() == 1 && said(l, "broken does not read") && said(l, "nothing in it used"));
  l.clear(), st::apply(R"([1, 2])", "list", &l);
  EXPECT(said(l, "list does not read (not an object)"));
  l.clear(), st::apply(R"({"version": 2, "vsync": 0})", "newer", &l);
  EXPECT(st::get(st::kVsync) == 0 && said(l, "version 2 is newer than this launcher's 1"));
  // load: the shipped then the player's, a missing file silent; what differs from the defaults listed.
  const std::string d = std::string(MW3_TEST_TMP) + "/launcher_settings";
  std::filesystem::remove_all(d), std::filesystem::create_directories(d + "/shipped"), std::filesystem::create_directories(d + "/mine");
  std::ofstream(d + "/shipped/" + st::kFileName) << st::shipped_text();
  std::ofstream(d + "/mine/" + st::kFileName) << R"({"frames_without_vsync": 30})";
  l = st::load(d + "/shipped", d + "/mine");
  for (const std::string& s : l) std::printf("  %s\n", s.c_str());
  EXPECT(l.size() == 3 && said(l, "frames_without_vsync 30 (default 60)") && st::get(st::kFramesWithoutVsync) == 30);
  EXPECT(st::load(d + "/none", "").empty() && st::get(st::kFramesWithoutVsync) == 60);
  st::reset();
}

// The program reads the player's file from its data folder at the start: vsync off and 30 frames a second, a value
// clamped, an address ignored; the log says each.
TEST(sdl_program_launcher_reads_its_settings) {
  const LauncherRun r = run_launcher_with("launcher_settings_run", "READY", nullptr, {{"MW3_LAUNCHER_FRAMES", "20"}},
                                          [](const std::string& dir) {
                                            std::filesystem::create_directories(dir + "/wd");
                                            std::ofstream(dir + "/wd/launcher-settings.json")
                                                << R"({"vsync": 0, "frames_without_vsync": 30, "game_first_frame_seconds": 9999, "catalog_url": "https://x.example"})";
                                          },
                                          false);
  EXPECT(r.exited);
  EXPECT(has(r.log, "settings: ") && has(r.log, "launcher-settings.json: 3 read") && has(r.log, "catalog_url is not a setting: ignored"));
  EXPECT(has(r.log, "game_first_frame_seconds 9999 is outside 5 to 300: 300 used"));
  EXPECT(has(r.log, "paced by a timed sleep to 30 a second"));
  if (!r.exited) std::printf("%s\n", r.log.c_str());
}

// WHAT'S NEW (2.2.0, docs/launcher.md, A game's page): the versions the page shows. One a tag over both homes (the
// first with words), a game's own tags only, no drafts or prereleases, newest first; with an update waiting, every
// version after the installed one up to the newest; else the installed one's, or the newest's when none is.
TEST(whats_new_picks_the_versions_between) {
  namespace u = upgrades::update;
  auto rel = [](const char* tag, const char* notes, bool draft = false, bool pre = false) {
    u::Release r;
    r.tag = tag, r.notes = notes, r.draft = draft, r.prerelease = pre;
    return r;
  };
  const std::vector<u::Release> homes = {
      rel("pm-v1.2.0", ""),  // the mirror's copy without words: GitHub's below has them
      rel("pm-v1.3.0", "three"), rel("pm-v1.2.10", "two ten"), rel("pm-v1.2.9", "two nine"),
      rel("pm-v1.4.0", "draft", true), rel("pm-v1.4.0-rc1", "rc", false, true), rel("v9.0.0", "the launcher's"),
      rel("pm-v1.2.0", "two"), rel("pm-v1.2.10", "the second home's")};
  const std::vector<u::Notes> all = u::collect_notes(homes, [](const u::Release& r) { return u::game_tag(u::Game::kPm, r.tag); });
  std::string tags;
  for (const u::Notes& n : all) tags += n.tag + "=" + n.text + " ";
  std::printf("  collected: %s\n", tags.c_str());
  EXPECT(tags == "pm-v1.3.0=three pm-v1.2.10=two ten pm-v1.2.9=two nine pm-v1.2.0=two ");
  auto show = [&](const char* installed, const char* newest) {
    std::string s;
    for (const u::Notes& n : u::notes_to_show(all, installed, newest)) s += n.tag + " ";
    return s;
  };
  EXPECT(show("pm-v1.2.0", "pm-v1.3.0") == "pm-v1.3.0 pm-v1.2.10 pm-v1.2.9 ");  // three behind: all three
  EXPECT(show("pm-v1.2.9", "pm-v1.2.10") == "pm-v1.2.10 ");
  EXPECT(show("pm-v1.3.0", "pm-v1.3.0") == "pm-v1.3.0 ");  // up to date: its own
  EXPECT(show("pm-v1.2.10", "") == "pm-v1.2.10 ");           // offline: its own
  EXPECT(show("", "pm-v1.3.0") == "pm-v1.3.0 ");             // not installed: the newest
  EXPECT(show("pm-v1.1.0", "") == "" && show("", "") == "");  // not known
  EXPECT(show("pm-v1.3.0-12-gabc-dirty", "pm-v1.3.0") == "pm-v1.3.0 ");  // a build after a release counts as it
}

// WHAT'S NEW's lines from a release's Markdown, as the repos write it (no repo's own rules): headings, items (a
// nested one indented), a wrapped paragraph or item joined, marks dropped, links as their text; the Download and
// Before you install sections, the SHA256SUMS paragraph and the bug-report one left out; no blank under a heading.
TEST(whats_new_reads_release_markdown) {
  namespace u = upgrades::update;
  const std::string md =
      "MechWarrior 3 on its own.\r\n\r\n"
      "## Download\n\n- **Windows:** `MW3-alone-Setup-v1.7.0.exe`\n\n### For Macs\n\nthe dmg\n\n"
      "## What's new\n\n"
      "- **A dropped download picks up** where it\n  stopped, see [the notes](https://x.example/n.md).\n"
      "  - a nested one\n"
      "* A star item with a_name kept\n\n"
      "A paragraph wrapped\nover two lines.\n\n"
      "## Before you install\n\n- **Upgrading:** press UPDATE.\n\n"
      "## Fixes\n\nNothing in play changes.\n\n"
      "Full notes: [release-notes-1.7.0.md](https://x.example). `SHA256SUMS` and `SHA256SUMS.sig` are under Assets.\n\n"
      "**Reporting a bug:** attach `mw3launcher.log`.\n\n";
  const std::vector<u::NoteLine> lines = u::whats_new_lines(md);
  static const char* const kinds = "THIB";
  std::string got;
  for (const u::NoteLine& l : lines) got += std::string(1, kinds[l.kind]) + "|" + l.text + "\n";
  std::printf("%s", got.c_str());
  EXPECT(got ==
         "T|MechWarrior 3 on its own.\n"
         "B|\n"
         "H|What's new\n"
         "I|\xe2\x80\xa2 A dropped download picks up where it stopped, see the notes.\n"
         "I|    \xe2\x80\xa2 a nested one\n"
         "I|\xe2\x80\xa2 A star item with a_name kept\n"
         "B|\n"
         "T|A paragraph wrapped over two lines.\n"
         "B|\n"
         "H|Fixes\n"
         "T|Nothing in play changes.\n");
  EXPECT(u::whats_new_lines("").empty() && u::whats_new_lines("\n\n## Download\n\n- x\n").empty());
}

// The notes cached for an offline start read back as they were written: quotes, backslashes, new lines, tabs, a
// control character and UTF-8 alike.
TEST(whats_new_cache_reads_back) {
  namespace u = upgrades::update;
  const std::vector<u::Notes> in = {{"mw1-v1.1.5", "## A \"quoted\" C:\\path\n\n- tab\there\r\n\x01 end \xe2\x80\xa2"}, {"mw1-v1.1.4", ""}};
  std::vector<u::Notes> out;
  EXPECT(u::read_notes_cache(u::notes_cache(in), &out));
  EXPECT(out.size() == 2 && out[0].tag == in[0].tag && out[0].text == in[0].text && out[1].tag == in[1].tag && out[1].text.empty());
  EXPECT(!u::read_notes_cache("{not a list", &out));
}

// The page lands on WHAT'S NEW once after an update: only from a version it knew to a newer one.
TEST(whats_new_lands_once_after_an_update) {
  namespace u = upgrades::update;
  EXPECT(u::land_on_whats_new("pm-v1.2.0", "pm-v1.3.0"));
  EXPECT(!u::land_on_whats_new("pm-v1.3.0", "pm-v1.3.0"));  // seen: once
  EXPECT(!u::land_on_whats_new("", "pm-v1.3.0"));           // first known (a first install, or the first 2.2.0 start)
  EXPECT(!u::land_on_whats_new("pm-v1.3.0", ""));           // uninstalled
  EXPECT(!u::land_on_whats_new("pm-v1.3.0", "pm-v1.2.0"));  // an older copy linked
}

// Through the launcher: Inception installed at chi-v1.0.0, its page last seen at chi-v0.9.0, a release list with
// chi-v1.1.0 newer. The gear lands on WHAT'S NEW (the update to 1.0.0), showing 1.1.0's notes over nothing else
// (an update waiting: the versions after the installed one), and seen.txt now says 1.0.0. Opened again, it does
// not land; the notes are cached for an offline start.
TEST(sdl_program_launcher_whats_new_after_an_update) {
  namespace fs = std::filesystem;
  const std::string list =
      R"([{"tag_name": "chi-v1.1.0", "body": "## What's new\n\n- **Faster maps.**\n\n## Download\n\n- chi.zip", "assets": [)"
      R"({"name": "CHI-Rebuild-chi-v1.1.0-mac.dmg", "browser_download_url": "https://x.example/a", "size": 1},)"
      R"({"name": "CHI-Rebuild-chi-v1.1.0-x86_64.AppImage", "browser_download_url": "https://x.example/b", "size": 1},)"
      R"({"name": "CHI-Rebuild-chi-v1.1.0-win64.zip", "browser_download_url": "https://x.example/c", "size": 1}]},)"
      R"( {"tag_name": "chi-v1.0.0", "body": "The first release.", "assets": []},)"
      R"( {"tag_name": "chi-v0.9.0", "body": "A test release.", "assets": []}])";
  const std::string dir = std::string(MW3_TEST_TMP) + "/launcher_whats_new";
  auto read = [](const std::string& f) {
    std::ifstream in(f);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  };
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir + "/api"), fs::create_directories(dir + "/games/chi"), fs::create_directories(dir + "/wd/whats-new");
  for (const char* repo : {"mech3-re", "mech3-pm-re", "mech1-remake-re", "mech1-re", "inception-remake-re"}) std::ofstream(dir + "/api/" + repo + ".json") << "[]";
  std::ofstream(dir + "/api/inception-re.json") << list;
  {
    std::ofstream sh(dir + "/games/chi/chi");
    sh << "#!/bin/sh\ncase \"$1\" in --status) echo '{\"kit\": 1, \"game\": \"inception\", \"version\": \"1.0.0\", \"files\": {\"state\": \"ready\"}, "
          "\"first_run\": {\"pending\": []}, \"needs\": [], \"setup\": 1}';; esac\nexit 0\n";
  }
  fs::permissions(dir + "/games/chi/chi", fs::perms::owner_all);
  std::ofstream(dir + "/games/chi/rebuild-game.txt") << "# x\ngame=chi\ntag=chi-v1.0.0\nfile=x\nsha256=x\nfrom=github\n";
  std::ofstream(dir + "/games/rebuild-games.txt") << "# x\n";
  std::ofstream(dir + "/wd/whats-new/seen.txt") << "chi\tchi-v0.9.0\nmw3\tv1.0.0\n";
  setenv("MW3_GAMES_API", ("file://" + dir + "/api/{repo}.json").c_str(), 1);
  setenv("MW3_GAMES_DIR", (dir + "/games").c_str(), 1);
  for (const char* other : {"MW3_GAME_BIN", "MW3_MW1_BIN", "MW3_MW1R_BIN", "MW3_CHI_BIN", "MW3_CHIRM_BIN"}) setenv(other, "", 1);
  auto shot = [&](const char* name) {  // the gear's page drawn once, the checks answered first (a shot's way)
    const std::string log = dir + "/" + name + ".log";
    int status = 0;
    const pid_t pid = spawn_launcher({"--write-dir", dir + "/wd", "--launcher-shot", dir + "/" + name + ".bmp", "--launcher-screen", "game:chi"}, log);
    const bool ok = waited(pid, &status) && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    return std::make_pair(ok, read(log));
  };
  const auto [a_ok, a] = shot("first");
  const std::string seen = read(dir + "/wd/whats-new/seen.txt"), cache = read(dir + "/wd/whats-new/chi.json");
  std::printf("  seen after: %s", seen.c_str());
  EXPECT(a_ok && has(a, "page chi: WHAT'S NEW, updated from chi-v0.9.0 to chi-v1.0.0\n"));
  EXPECT(has(a, "screen game: BACK, SETTINGS, WHAT'S NEW\n"));
  EXPECT(has(a, "whats new: version: VERSION 1.1.0\n") && has(a, "whats new: item: \xe2\x80\xa2 Faster maps.\n"));
  EXPECT(!has(a, "VERSION 1.0.0") && !has(a, "chi.zip"));
  EXPECT(seen == "chi\tchi-v1.0.0\nmw3\tv1.0.0\n" && has(cache, "\"tag_name\": \"chi-v1.1.0\""));
  if (!a_ok || !has(a, "VERSION 1.1.0"))  // what the page and the checks said
    for (size_t at = 0; at < a.size();) {
      const size_t nl = std::min(a.find('\n', at), a.size());
      const std::string line = a.substr(at, nl - at);
      if (has(line, "whats new") || has(line, "games:") || has(line, "page chi")) std::printf("    %s\n", line.c_str());
      at = nl + 1;
    }
  const auto [b_ok, b] = shot("again");
  for (const char* k : {"MW3_GAMES_API", "MW3_GAMES_DIR", "MW3_GAME_BIN", "MW3_MW1_BIN", "MW3_MW1R_BIN", "MW3_CHI_BIN", "MW3_CHIRM_BIN"}) unsetenv(k);
  EXPECT(b_ok && !has(b, "WHAT'S NEW, updated") && has(b, "screen game: GAME FILES"));
}
