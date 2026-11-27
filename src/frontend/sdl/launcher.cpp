#include "frontend/sdl/system_pickers.h"
#include "frontend/sdl/vulkan_check.h"
#include "frontend/sdl/launcher.h"
#include "frontend/sdl/launcher_ui.h"
#include "frontend/sdl/log_start.h"
#include "frontend/sdl/sdl_focus.h"
#include "frontend/sdl/launcher_folder.h"
#include "frontend/sdl/font_search.h"
#include "frontend/sdl/text_fit.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <cstring>
#include <memory>
#include <mutex>
#include <utility>
#include <thread>

#include "frontend/sdl/launcher_own.h"
#include "frontend/sdl/sdl_update.h"
#include "frontend/sdl/sdl_catalog.h"
#include "frontend/sdl/sdl_games.h"
#include "upgrades/update/games.h"

namespace sdlfe {

namespace {
// How long the launcher waits for the game's first frame before it says so
// on its status line: 20 s (MW3_LAUNCHER_READY_MS for the checks).
double ready_wait_ms() {
  const char* e = SDL_getenv("MW3_LAUNCHER_READY_MS");
  return e ? std::atof(e) : 20000.0;
}
namespace fs = std::filesystem;
// 16:9 (2026-11-08): a shot's size, and the layout's units (launcher_ui.h).
constexpr int W = 854, H = 480;
// ADD FROM MY DISC's line under its name: short enough for the plate at its font (it was cut in the middle).
constexpr char kAddDiscSub[] = "A disc in a drive, its image, a download (.zip) or any file of it";
// The setup screens' game names, by header_game(): the header of a game's own start (2026-11-22); in the launcher
// the line under BATTLETECH (2026-11-24).
// A screen's game under BATTLETECH, by upgrades::update::Game: its catalog entry's heading (games.json).
const char* header_name(int game) { return upgrades::update::catalog_game(upgrades::update::game_at(game)).heading.c_str(); }
constexpr int kCopyRows = 4;  // GAME FILES' list: the copies shown (the rest by PICK A FILE)
// A panel's words' width (assets/launcher/ui/launcher.rcss, .panel .about), its lines chosen by it.
constexpr int kRowWidth = 640 - 2 - 28 - 2 * 112;  // a game page's row: its words beside two buttons
constexpr int kAboutWidth = 752 - 2 - 12 - 76 - 16 - 16 - 224 - 14;
// The launcher's name in its header and window (2026-11-22: "Battletech" at the top, for the whole family).
constexpr const char* kLauncherTitle = "BATTLETECH";

// ---- art ----

std::string assets_dir() {
  std::error_code ec;
  if (const char* base = SDL_GetBasePath()) {
    const fs::path p = fs::path(base) / "assets" / "launcher";
    if (fs::is_directory(p, ec)) return p.string();
  }
#ifdef MW3_ASSETS_DIR
  const fs::path p = fs::path(MW3_ASSETS_DIR) / "launcher";
  if (fs::is_directory(p, ec)) return p.string();
#endif
  return {};
}
// The font: assets/launcher/font.ttf, which every package carries, else a
// condensed system face (launcher.md, Open), else on Linux whatever a walk of
// the font folders finds (an install without the fonts at these places).
std::string font_path(const std::string& dir) {
  std::error_code ec;
  std::vector<std::string> tries;
  if (!dir.empty()) tries.push_back((fs::path(dir) / "font.ttf").string());
  tries.insert(tries.end(), {"C:/Windows/Fonts/bahnschrift.ttf", "C:/Windows/Fonts/arialbd.ttf",
                             "/System/Library/Fonts/Supplemental/DIN Condensed Bold.ttf",
                             "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
                             // Linux (docs/linux.md): a narrow face first, at Debian/Ubuntu's,
                             // Fedora's and Arch's places, then a plain bold one.
                             "/usr/share/fonts/truetype/liberation/LiberationSansNarrow-Bold.ttf",
                             "/usr/share/fonts/liberation-narrow/LiberationSansNarrow-Bold.ttf",
                             "/usr/share/fonts/liberation/LiberationSansNarrow-Bold.ttf",
                             "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf",
                             "/usr/share/fonts/dejavu-sans-fonts/DejaVuSansCondensed-Bold.ttf",
                             "/usr/share/fonts/TTF/DejaVuSansCondensed-Bold.ttf",
                             "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
                             "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf",
                             "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
                             "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"});
  for (const std::string& t : tries)
    if (fs::exists(t, ec)) return t;
#if !defined(_WIN32) && !defined(__APPLE__)
  return sdlfe::pick_font(sdlfe::scan_fonts(sdlfe::font_folders()),
                          {"LiberationSansNarrow-Bold.ttf", "DejaVuSansCondensed-Bold.ttf", "NotoSans-CondensedBold.ttf",
                           "LiberationSans-Bold.ttf", "DejaVuSans-Bold.ttf", "NotoSans-Bold.ttf"}, sdlfe::has_latin);
#else
  return {};
#endif
}

// The panels' small lines (2026-11-24): assets/launcher/font_body.ttf (Saira Condensed Medium), else the lettering's.
std::string body_font_path(const std::string& dir) {
  std::error_code ec;
  if (!dir.empty() && fs::exists(fs::path(dir) / "font_body.ttf", ec)) return (fs::path(dir) / "font_body.ttf").string();
  return font_path(dir);
}

// A card's status from a ready line ("Ready, official: data 1.2, program v1.1.3, ") and the folder: the verdict,
// the versions, the folder, a line each.
std::vector<std::string> status_lines(const std::string& ready, const std::string& dir) {
  std::string r = ready;
  while (!r.empty() && (r.back() == ' ' || r.back() == ',')) r.pop_back();
  const size_t colon = r.find(": ");
  if (colon == std::string::npos) return {r, dir};
  return {r.substr(0, colon), r.substr(colon + 2), dir};
}
// ---- text ----

// Words measured as the screens draw them (launcher_ui.h), in the layout's units.
struct Text {
  LauncherUi* ui = nullptr;
  bool body = false;  // the panels' small lines' font
  int width(const std::string& s, int size) const { return ui->width(s, size, body); }  // a narrow space as it is drawn
  int line_skip(int size) const { return ui->line_skip(size, body); }
  // `s` shortened from the middle with "..." until it is `width` across; its
  // first `keep` characters stay whole (a folder's start and end both show).
  std::string fit(const std::string& s, int size, int width, size_t keep = 0) const {
    return fit_middle(s, keep, width, [&](const std::string& t) { return this->width(t, size); });
  }
  // `s` as lines no wider than `width` (its own '\n's kept).
  std::vector<std::string> wrap(const std::string& s, int size, int width) const {
    std::vector<std::string> lines;
    std::string line, word;
    auto flush_word = [&] {
      if (word.empty()) return;
      const std::string tryl = line.empty() ? word : line + " " + word;
      if (!line.empty() && this->width(tryl, size) > width) lines.push_back(line), line = word;
      else line = tryl;
      word.clear();
    };
    for (char ch : s) {
      if (ch == '\n') flush_word(), lines.push_back(line), line.clear();
      else if (ch == ' ') flush_word();
      else word += ch;
    }
    flush_word();
    if (!line.empty()) lines.push_back(line);
    return lines;
  }
};



// The pickers' filters: every file shown (2026-11-24: a filter hid the downloads, or every zip from ADD FROM
// MY DISC); what a pick is comes from its content.
// The first shows every file and is the one a picker opens with; the others narrow a full folder.
const SDL_DialogFileFilter kGameFiles[] = {{"All files", "*"},
                                           {"Downloads (.zip .7z .rar)", "zip;7z;rar;tar;gz;tgz;xz"},
                                           {"Disc and disk images (.iso .cue .adf)", "iso;cue;bin;img;adf;ima;dsk"},
                                           {"Programs (.exe)", "exe"}};
constexpr int kGameFilesCount = 4;
// INSTALL FROM A DOWNLOADED FILE's: a game's own download for this computer (sdl_games.h).
const SDL_DialogFileFilter kPackageFiles[] = {{"A game's download (.zip, .dmg, .AppImage)", "*"}};
const SDL_DialogFileFilter kDiscFiles[] = {{"All files", "*"},
                                           {"Downloads (.zip .7z .rar)", "zip;7z;rar;tar;gz;tgz;xz"},
                                           {"Disc images (.cue .iso)", "cue;iso;bin;img"}};
constexpr int kDiscFilesCount = 3;

// The find screen's words (2026-11-02): what the game needs, and that
// nothing of the player's is changed.
const char* const kFindWords =
    "MechWarrior 3 needs your own copy of the game. Show us your disc, a disc image or download (.iso, .cue, .zip), "
    "or a folder where it's installed. We copy what the game needs into your user folder; your disc and any "
    "installed copy are left untouched.";
const char* const kFindWordsPm =
    "Pirate's Moon needs your own copy of the game. Show us its disc, a disc image or download (.iso, .cue, .zip), "
    "or a folder where it's installed. We copy what the game needs into its own user folder; your disc and any "
    "installed copy are left untouched.";

// The menu's sounds, the launcher's own (b6, 2026-11-26; assets/launcher/README.md): hover.wav on a rollover at 0.7
// and click.wav on a select, the gains of the main menu's (sounds.zrd's MAINMENU set). Before b6 they were the game's
// own sfx_button2 and sfx_button6, read from the install's sound archive.
struct Sounds {
  enum { kRollover, kSelect };
  SDL_AudioStream* stream[2] = {};
  Uint8* audio[2] = {};
  Uint32 len[2] = {};
  ~Sounds() {
    for (int i = 0; i < 2; ++i) {
      if (stream[i]) SDL_DestroyAudioStream(stream[i]);
      SDL_free(audio[i]);
    }
  }
  bool load(const std::string& dir) {
    static const char* const kFiles[2] = {"hover.wav", "click.wav"};
    static const float kGain[2] = {0.7f, 1.0f};
    int got = 0;
    for (int i = 0; i < 2 && !dir.empty(); ++i) {
      SDL_AudioSpec spec{};
      if (!SDL_LoadWAV((fs::path(dir) / kFiles[i]).string().c_str(), &spec, &audio[i], &len[i])) continue;
      stream[i] = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
      // MW3_LAUNCHER_MUTE (checks): loaded and played at no volume.
      if (stream[i]) SDL_SetAudioStreamGain(stream[i], SDL_getenv("MW3_LAUNCHER_MUTE") ? 0.0f : kGain[i]), ++got;
    }
    if (got) std::printf("mw3: launcher: the menu's sounds from %s (%d of 2)\n", dir.c_str(), got);
    return got > 0;
  }
  void play(int i) {
    if (!stream[i]) return;
    SDL_ClearAudioStream(stream[i]);
    SDL_PutAudioStreamData(stream[i], audio[i], static_cast<int>(len[i]));
    SDL_ResumeAudioStreamDevice(stream[i]);
  }
};

// ---- the screens ----

struct Button {
  int id = 0;
  std::string group;  // where the document has it (<x buttons="group">, assets/launcher/ui/launcher.rml)
  std::string label, sub;
  bool enabled = true;
  size_t sub_keep = 0;  // the sub's start kept whole when it is shortened to fit
  const char* kind = "plate";  // plate (the other screens' steel), amber, flat (the bottom row), gear (a panel's, a
                               // cog), seg (a split button's end, 2026-11-22: a game's UPDATE joined to its PLAY)
  std::string name;  // what the log and the checks call it ("" the label): a panel's PLAY by its game
  const std::string& log_name() const { return name.empty() ? label : name; }
};

enum Id {
  kMw3 = 1, kPm, kQuit, kMw1,
  kFind = 10, kPoint, kBack,
  kPatch,  // the 1.2 patch's zip: unreachable since the patch is built in (patch12.h), its code kept
  kAddDisc = 20,
  kGo = 30, kCancel, kNotesOk, kPickOther,
  kPickFile, kPickFolder, kPlayNoMusic,  // ours: PICK A FILE; PICK A FOLDER; the no-music dialog's PLAY WITHOUT MUSIC (2026-11-24)
  kInstallTo, kPlaceGo,  // ours: CHANGE FOLDER (the game's folder, picked) and the card INSTALL's go once asked where
  kData = 40, kDataPick, kDataUsual, kMoveGo, kRemoveOld, kKeepOld,
  kUpdates = 49,
  kSelfUpdate = 50,  // the header's UPDATE AVAILABLE <v> when the start's check found one (2026-11-23)
  kUpdateLauncher = 78, kUpdateAll,  // CHECK FOR UPDATES' box (2026-11-24): the launcher alone, or the games first
  kUpdateGo = 80, kUpdateNotNow,  // the UPDATE screen: UPDATE and BACK (2026-11-23: no paging arrows)
  kVulkanAgain = 90,  // ours: the Vulkan check again (docs/linux.md)
  kGameInstall = 110,  // ours, games on demand (sdl_games.h): + the game (upgrades::update::Game), INSTALL
  kGameUpdate = 120,   // + the game: UPDATE
  kInstallFile = 130, kInstallFileGo,  // INSTALL FROM A DOWNLOADED FILE; its ADD ANYWAY for a file without SHA256SUMS
  kKitPlay = 150,  // + the game: PLAY for a game on the setup kit's contract (MW1 Rebuild, Inception's two)
  kCopy = 160,      // + the row (up to kCopyRows): GAME FILES' list of the copies on this computer (2026-11-24), one taken
  kCopyPick = 168,
  kGear = 170,       // + the game: a panel's gear, its game's page (2026-11-24, docs/launcher.md, A game's page)
  kGameLink = 180,   // the page's LINK AN INSTALL (a picker), UNLINK, UNINSTALL, BACK
  kGameUnlink, kGameUninstall, kGameFilesPick, kGameBack,
  kGameRemove,  // UNINSTALL of the linked copy (2026-11-24)
  kUninstallGo, kUninstallAll, kUninstallBack,  // UNINSTALL's confirm: the program, the program and its game files, KEEP IT  // its PICK A FILE: the one picker (1.3.4 had it at kCopy + 1, the second row's id: that row opened the picker)
};

// A copy of a game's files on this computer, as GAME FILES lists them (2026-11-24: "the user should be able to pick
// what's used. Some copies have music and some don't"): where, its data's version, whether it is the one in use, and
// what the game would lack with it (the music and movies the user folder holds count for every copy).
struct Copy {
  std::string path, version;
  bool in_use = false;
  std::vector<std::string> missing;
};

struct Install {
  bool ok = false;
  std::string dir, version, summary;
  // Found by the search (the installer's places, a drop-in), not set up yet: MECHWARRIOR 3 offers to copy it in
  // first (2026-11-21: once found, the original may be deleted).
  bool elsewhere = false;
};

// An operation on a worker thread (MW3's setup calls, launcher_mw3_setup.h, run on the caller's).
struct Work {
  std::thread th;
  std::mutex m;
  own::Progress progress;
  own::Result result;
  std::atomic<bool> cancel{false}, done{false};
  std::string title;
  enum Kind { kOther, kLook, kInstallIt, kAddDisc, kConvert, kMoveData, kRemoveData, kUpdateDownload, kGameInstall, kGameFileCheck, kUninstall, kKitLook, kKitInstall, kOriginal } kind = kOther;  // a look, an install, ADD FROM MY DISC, the movies
  ~Work() {
    cancel = true;
    if (th.joinable()) th.join();
  }
  void start(std::string t, std::function<own::Result(const own::ProgressFn&)> op) {
    title = std::move(t);
    std::printf("mw3: launcher: work: %s\n", title.c_str());  // the checks follow it
    th = std::thread([this, op] {
      own::Result r = op([this](const own::Progress& p) {
        std::lock_guard<std::mutex> g(m);
        if (p.stage != progress.stage) std::printf("mw3: launcher: work stage: %s\n", p.stage.c_str());  // the checks: its words
        progress = p;
        return !cancel.load();
      });
      std::lock_guard<std::mutex> g(m);
      result = std::move(r);
      done = true;
    });
  }
};

// A picked file or folder from SDL's dialogs (their answers come through a
// callback, on another thread on some systems).
struct Pick {
  std::mutex m;
  int what = 0;  // the button that asked
  std::string path;
  bool answered = false, cancelled = false;
  static void SDLCALL done(void* user, const char* const* list, int) {
    auto* p = static_cast<Pick*>(user);
    std::lock_guard<std::mutex> g(p->m);
    p->answered = true;
    if (list && list[0]) p->path = list[0];
    else p->cancelled = true;
  }
};

class Launcher {
 public:
  explicit Launcher(const LauncherOptions& o) : o_(o) {
    sdlfe::stop_asking(false);  // an earlier one's quit set it
  }
  ~Launcher() {
    // A quit never waits on the network (lead 2026-11-23): the checks' curls stopped before their threads are
    // joined (games_check_, self_check_), else a home that hangs held the quit for its timeouts.
    sdlfe::stop_asking();
    // A status still being asked on its thread (asks_collect): its program stopped by stop_asking in moments, not
    // left running past the launcher; never waited for longer than a second.
    for (int n = 0; n < 100 && std::any_of(std::begin(later_), std::end(later_), [](const auto& a) { return a && !a->done; }); ++n)
      SDL_Delay(10);
    games_check_.reset();
    self_check_.reset();
    SDL_RemoveEventWatch(&Launcher::exposed, this);
    ui_.detach();
    if (ren_) SDL_DestroyRenderer(ren_);
    if (win_) SDL_DestroyWindow(win_);
    for (std::thread& t : status_ask_)
      if (t.joinable()) t.join();
  }
  LauncherChoice run();

 private:
  void find_install();
  void find_install_older();  // the older programs' path (b2): below
  void find_mw1(bool start = false);
  void run_mw1_remake();          // MW1: the remake's start
  std::string mw1_needs() const;  // MW1: what it still needs, "" when nothing
  void mw1_others(bool* mw3, bool* pm) const;  // MW1: whether it has MechWarrior 3 and Pirate's Moon
  void start_game(bool pm = false);
  void start_pm();
  bool mw3_ready();
  void mw3_notes_seen();
  bool notes_once(const std::vector<std::string>& notes, std::string* crc_kept, const char* key);
  void find_pm();
  void find_pm_older();
  bool mw1_served() const { return kit_known_[2] && kit_status_[2].setup >= rekit::kServedSetup; }  // b5: its own setup
  std::vector<std::string> with_args(int i);  // --with <id>=<where> for each game game i needs (b5)
  void mw1_too_old();                         // an older Remake without its MechWarrior (1989): its update offered
  void older_game(int i, int play = 0);       // b6: MechWarrior 3 or Pirate's Moon without a served setup: the same
  // The mouse is followed as the list scrolls under it (after a wheel or a drag, until the next screen).
  bool follow_ = false, ui_stale_ = true;
  // GAME FILES on Pirate's Moon (files_pm_): its settings folder as mw3pm
  // finds it, its usual one ("BattleTech/Pirate's Moon", made by mw3pm, not
  // here), what it lacks, and the folder in use.
  std::string pm_usual() const;
  std::string pm_wd();
  std::string files_wd() { return files_pm_ ? pm_wd() : o_.write_dir; }
  // Each game's music and converted movies: its own folder's when it has one, else the user folder (media_dir).
  std::string media() const { return own::media_dir(install_.ok ? install_.dir : std::string(), o_.write_dir); }
  std::string pm_media() { return own::media_dir(pm_dir_, pm_wd()); }
  std::string files_usual() const { return files_pm_ ? pm_usual() : own::usual_folder(); }
  void pm_refresh();
  void pm_refresh_older();
  std::string pm_in_use() const;
  void use_pm(const std::string& dir, const std::string& summary);
  std::vector<std::string> pm_notes(own::Result* check = nullptr);
  void watch_game();
  Uint64 display_frame() const;
  void layout();
  UiModel model();
  void draw();
  void sync_ui();
  void save_frame(const std::string& path);
  void mouse_steps();
  static bool SDLCALL exposed(void* user, SDL_Event* e) {
    auto* l = static_cast<Launcher*>(user);
    if (e->type == SDL_EVENT_WINDOW_EXPOSED && e->window.data1 == 1 && l->ren_ && l->win_ && SDL_GetWindowID(l->win_) == e->window.windowID)
      l->draw(), SDL_RenderPresent(l->ren_);
    return true;
  }
  int header_game() const;
  static constexpr int kNotesTop = 162;  // BEFORE YOU PLAY's first note
  struct NotesLayout {
    int size = 14, gap = 14;        // the notes' font and the space after each
    int width = 470;                // their lines' width
    int status_y = 296;             // the status line's first row
    int add_y = -1, ok_y = 384;     // ADD FROM MY DISC (-1: not there) and OK
    bool fits = true;               // above the frame's bottom at this size
  };
  NotesLayout notes_layout();
  void activate(int id);
  void ask_file(int what, const SDL_DialogFileFilter* filters, int n);
  void ask_folder(int what);
  void dropped(const std::string& path);
  void open_picker();
  void picked(int what, const std::string& path);
  void finish_work();
  void look_at(const std::string& path);
  void search();
  void use_install(const std::string& dir, const std::string& version, const std::string& summary);
  bool data_elsewhere();
  void ask_move(const std::string& to);
  void start_move();
  void moved(const own::Result& r);
  void play();
  int movies_to_convert() const;
  void convert_movies(std::function<void()> then);
  void show_vulkan();  // ours: what to install for Vulkan, TRY AGAIN and BACK
  void notes_refresh();
  void notes_refresh_older();
  std::string in_use() const;
  void add_disc(const std::string& source);
  void nav(int action);  // 1 next, -1 previous, 2 press, 3 back (keys and a pad)

  LauncherOptions o_;
  SDL_Window* win_ = nullptr;
  SDL_Renderer* ren_ = nullptr;
  LauncherUi ui_;
  Text text_{&ui_, false};
  Text body_{&ui_, true};  // the panels' small lines (font_body.ttf)
  enum Screen { kMain, kSetupScreen, kConfirm, kWorking, kNotes, kDataScreen, kUpdateScreen, kCopies, kGamePage } screen_ = kMain;
  std::vector<Copy> copies_;  // kCopies' rows (find_copies)
  void find_copies();
  std::vector<std::string> notes_;  // the first start's (launcher_notes)
  std::vector<Button> buttons_;
  int hot_ = -1, focus_ = -1;
  int frames_ = 0;                    // frames shown, and their draw and present times (the log)
  bool vsync_ = false;
  int quit_after_ = SDL_getenv("MW3_LAUNCHER_FRAMES") ? std::atoi(SDL_getenv("MW3_LAUNCHER_FRAMES")) : 0;
  double draw_ms_ = 0, present_ms_ = 0;
  Install install_;
  std::string pm_dir_;  // Pirate's Moon's folder, kept as pm_install= in mw3.cfg
  std::string pm_found_;  // the search's Pirate's Moon, not taken: offered when PIRATE'S MOON is chosen
  bool have_files() const { return files_pm_ ? !pm_dir_.empty() : install_.ok; }
  std::string files_dir() const { return files_pm_ ? pm_dir_ : install_.dir; }
  bool files_pm_ = false;  // GAME FILES' screens are Pirate's Moon's 
  bool pm_lacks_music_ = false, pm_lacks_sounds_ = false;  // pm_refresh
  std::shared_ptr<upgrades::update::Verdict> update_;  // the last CHECK FOR UPDATES' verdict
  std::vector<std::string> update_lines_;  // its release's notes wrapped to the screen (show_update)
  std::shared_ptr<std::string> update_file_;  // after UPDATE: the program to start next (empty: none)
  static constexpr int kUpdateWidth = 500;
  void show_update();
  void read_update_notes();
  // Ours, games on demand (docs/launcher.md, Games on demand): each game's program beside this one (a package or
  // build that carries it), else the one the launcher installed under games_root_; each repo's answer on its card.
  void resolve_games();
  void start_games_check();
  // CHECK FOR UPDATES (2026-11-24): both start checks asked again, the home screen staying; their answers said, and a
  // box when the launcher itself has one.
  void recheck();
  void recheck_done();
  bool rechecking_ = false, update_box_ = false, nav_late_ = false;
  upgrades::update::Verdict::Kind self_kind_ = upgrades::update::Verdict::kFailed;  // the last answer about the launcher
  std::string self_words_ = "Could not check for updates";
  std::vector<int> game_updates() const;  // the games whose UPDATE is there
  std::vector<int> update_queue_;         // UPDATE ALL: the games still to go, then the launcher
  bool update_queue_self_ = false;
  void next_update();
  void take_games_check();
  void install_game(int i);
  void install_game_now(int i);
  void ask_place(int i);
  std::string files_out();   // where "Use these game files?" puts them: files_into_/install, else the user folder's
  void files_confirm_text();
  void install_file_go();
  void game_installed(const sdlfe::games::Done& d);
  bool get_original(int i);  // after its DOWNLOAD: its original files from its catalog entry, when it lacks them
  void got_original(const own::Result& r);
  std::string game_words(int i) const;  // a card's line for a game not installed
  std::string& game_bin(int i) {
    return i == 0 ? o_.game_bin : i == 1 ? o_.pm_bin : i == 2 ? o_.mw1_bin : i == 3 ? o_.mw1r_bin : i == 4 ? o_.chi_bin : i == 5 ? o_.chirm_bin : more_bin_[i];
  }
  std::string more_bin_[upgrades::update::kMaxGames];  // a fetched catalog's new games (b7): their own copies or linked
  // The games on the setup kit's contract (re-setup-kit, rekit/status.h): MW1 Rebuild and Inception's two. Their
  // --status read at the start and after each run or install; PLAY starts them plain (their own setup opens when
  // their game files are missing).
  static bool kit_game(int i) { return i >= 3; }  // a fetched catalog's new games too: the kit's contract only
  // Removed from the catalog and not on this computer: its repo is not asked, its panel not shown (b7).
  bool removed_away(int i) {
    return upgrades::update::catalog_game(upgrades::update::game_at(i)).state == upgrades::update::CatalogGame::kRemoved && game_bin(i).empty();
  }
  void kit_refresh(int i);
  void run_kit(int i, bool set_up = false);
  // The setup of one that serves it (rekit/sdl/setup_serve.h), on the find screens as MW3's: its words, its search, its
  // look at a pick, its install; the recipe is the game's (2026-11-24).
  bool kit_set_up(int i) const;  // its game files are ready, and its setup has been through
  bool kit_serves(int i) const {  // MechWarrior 1 Remake's too (b5)
    return (kit_game(i) || i == 2) && kit_known_[i] && kit_status_[i].setup >= rekit::kServedSetup;
  }
  void open_kit_files(int i, bool play);
  void kit_look(const std::string& pick);  // "" its own search first
  void kit_looked(const own::Result& r);
  void kit_install();
  void kit_ready(const std::string& summary);
  int kit_files_ = -1;      // the game the find screens are for, else MW3's or Pirate's Moon's
  bool kit_play_ = false;   // PLAY asked for them: the game starts once they are in
  rekit::SetupMessage kit_ask_;                     // its words
  std::shared_ptr<rekit::SetupMessage> kit_found_;  // the last look's or install's answer
  rekit::Status kit_status_[upgrades::update::kMaxGames];
  bool kit_known_[upgrades::update::kMaxGames] = {};
  // Each game's --status (b2, docs/launcher-split.md): MechWarrior 3's and Pirate's Moon's too, for their panels.
  // At the start all are asked at once (starting_: each on its own thread, collected before the first frame); after,
  // when a panel's lines are made, once a frame at most (status_fresh_, cleared at each frame).
  bool starting_ = false, status_fresh_[upgrades::update::kMaxGames] = {};
  std::thread status_ask_[upgrades::update::kMaxGames];
  rekit::Status status_said_[upgrades::update::kMaxGames];
  bool status_ok_[upgrades::update::kMaxGames] = {};
  std::string status_why_[upgrades::update::kMaxGames];  // the start's asks: why one gave no status
  std::vector<std::string> status_args(int i);
  void status_collect();
  // A panel's lines ask again, but not while it is being asked, nor after it gave none: a program that hangs would
  // hold every frame (mech1-remake-re #2). An event (a run's end, an install, PLAY) asks it again.
  void status_refresh(int i) {
    if (!status_fresh_[i] && !checking(i) && kit_why_[i].empty()) kit_refresh(i);
  }
  // A status unknown is never an older program (mech1-remake-re #2, 2026-11-26): it is asked again on its own thread,
  // "Checking ..." meanwhile (the Remake's at the start too), and what was pressed is pressed again once it answers;
  // one that still gives none is said, its reason in the log. Nothing waits on such an ask, a quit neither: its answer
  // is its own (later_), taken each frame (asks_collect); the program is stopped as the launcher closes.
  struct LaterAsk {
    std::atomic<bool> done{false};
    std::string bin, why;
    rekit::Status said;
    bool ok = false;
  };
  std::shared_ptr<LaterAsk> later_[upgrades::update::kMaxGames];
  std::string kit_why_[upgrades::update::kMaxGames];  // why its last ask gave no status ("" when it gave one)
  int play_after_[upgrades::update::kMaxGames] = {};  // the button pressed while it was asked (0: none)
  bool said_after_[upgrades::update::kMaxGames] = {};  // a press asked: no answer is said
  bool checking(int i) const { return later_[i] != nullptr; }
  bool any_checking() const {
    for (const auto& a : later_)
      if (a) return true;
    return false;
  }
  bool may_ask(int i);
  bool ask_later(int i, int play = 0, bool said = false);
  void asks_collect(bool lay = true);
  void asks_wait();
  void not_answered(int i, int play);
  void log_status(int i);
  std::string asked_name(int i) const;
  std::string program_tag(int i);
  // MechWarrior 3's or Pirate's Moon's program answers for its files ("setup": 1) about the copy in use (`dir`, ""
  // any). Else, an older program or the game's own find screen (no program of its own to ask), the launcher checks
  // the files itself as before: the older programs' path, which ends at b6 (docs/launcher-split.md).
  bool status_for(int i, const std::string& dir = {}) const;
  // GAME FILES' operations (b3): through the game's served setup when its program answers for its files (the
  // screens stay MechWarrior 3's and Pirate's Moon's own); else the older path, in this program (ends at b6).
  bool files_served() const { return status_for(files_pm_ ? 1 : 0); }
  std::vector<std::string> served_args(std::vector<std::string> ask, int game = -1);
  void served_work(int kind, const std::string& title, std::vector<std::string> ask, int game = -1);
  bool movies_pending();  // b4: the movies to convert before the game (its status's pending step, or our count)
  std::shared_ptr<rekit::SetupMessage> served_said_;  // the served ask's answer (look: words, in place, path)
  bool served_work_ = false;                          // the work running is a served ask
  int served_game_ = 0;                               // whose (0 MechWarrior 3, 1 Pirate's Moon)
  void found_plate(const std::string& path, const std::string& words);
  void find_copies_served();
  bool games_on_ = false;  // mw3launcher (not mw3's or mw3pm's own find screen)
  std::string games_root_;
  // Ours (2026-11-22): each game's own folder, picked: its program and its game files there, the user folder
  // keeping pilots, saves, settings and mods. mw3.cfg's games_<key>=; empty: the games folder (program) and the
  // user folder (game files), as before.
  std::string games_place_[upgrades::update::kMaxGames];
  std::string game_root(int i) const { return games_place_[i].empty() ? games_root_ : games_place_[i]; }
  int place_game_ = -1;   // the card whose INSTALL asked where (confirm 7)
  std::string place_;     // that INSTALL's folder: the game's, the picked one, or "" for the games folder
  std::string files_into_;  // "Use these game files?"'s folder (CHANGE FOLDER); "" the user folder
  std::string files_words_, confirm_into_;  // what was found, and where it goes (drawn fitted to one line)
  bool beside_[upgrades::update::kMaxGames] = {}, has_installed_[upgrades::update::kMaxGames] = {};
  upgrades::update::InstalledGame installed_[upgrades::update::kMaxGames];
  // Which copy PLAY starts (2026-11-24): the package's own beside this program, the launcher's (games folder or the
  // player's picked folder), or one installed without the launcher and linked (mw3.cfg link_<key>=, found at the
  // start or picked on the game's page; unlinked_<key>= keeps one the player unlinked from being linked again).
  enum Source { kNoCopy, kBeside, kOwnCopy, kLinked } source_[upgrades::update::kMaxGames] = {};
  std::string linked_[upgrades::update::kMaxGames], linked_tag_[upgrades::update::kMaxGames];
  std::string tag_of(int i) const { return source_[i] == kOwnCopy ? installed_[i].tag : source_[i] == kLinked ? linked_tag_[i] : std::string(); }
  std::string record_folder(int i);  // where game i's program records itself (sdl_games.h kProgramRecord); "" not known
  void find_link(int i);
  void link_picked(const std::string& path);
  void unlink_game();
  void uninstall_go(bool files);
  int page_game_ = -1;           // kGamePage's game
  bool uninstall_files_ = false;  // the running UNINSTALL takes the game files too
  bool page_confirm_ = false;    // its UNINSTALL asked: the confirm in its place
  bool remove_linked_ = false;   // ... of the linked copy, not the launcher's
  sdlfe::games::Removal removal_[upgrades::update::kMaxGames];  // how the linked copy would go (open_game_page)
  std::vector<UiPanel> rows_;    // the page's rows
  int files_page_ = -1;          // the game page whose GAME FILES opened the find screen: its BACK
  bool can_open_files(int i) const;
  void open_files(int i);
  UiPanel files_row(int i);
  void open_game_page(int i);
  void resolve_game(int i, bool refresh);
  std::string own_files(int i);  // the game files UNINSTALL may remove with the program (see its definition)
  upgrades::update::GameCheck checks_[upgrades::update::kMaxGames];
  struct GamesCheck {
    std::thread th;
    std::atomic<bool> done{false};
    upgrades::update::GameCheck result[upgrades::update::kMaxGames];
    ~GamesCheck() {
      if (th.joinable()) th.join();
    }
  };
  std::unique_ptr<GamesCheck> games_check_;
  // The launcher's own update, asked at every start (2026-11-23): check_for_updates on a thread, both homes,
  // never a prompt; a newer one shows UPDATE AVAILABLE <v> in the header (kSelfUpdate), which opens the UPDATE
  // screen. Nothing downloads until the player presses UPDATE there.
  struct SelfCheck {
    std::thread th;
    std::atomic<bool> done{false};
    upgrades::update::Verdict verdict;
    ~SelfCheck() {
      if (th.joinable()) th.join();
    }
  };
  std::unique_ptr<SelfCheck> self_check_;
  std::shared_ptr<upgrades::update::Verdict> self_update_;  // the start's verdict when it is kNewer, else null
  void start_self_check();
  void take_self_check();
  void self_verdict(const upgrades::update::Verdict& v);
  void nav_steps();
  void nav_steps(const char* steps);
  std::shared_ptr<sdlfe::games::Done> game_done_;
  std::shared_ptr<sdlfe::games::FileCheck> file_check_;
  std::string install_file_;
  bool install_file_asked_ = false;  // MW3_LAUNCHER_INSTALL_FILE (checks): pressed once  // the game started runs from this AppImage's mount (start_game)
  std::string pm_data_;                  // ... its data's version ("1.0", "unknown", "incomplete")
  std::vector<std::string> mw3_check_notes_;  // start_check's for MW3, part of notes_ (notes_refresh)
  std::string mw3_program_, pm_program_, mw1_program_;  // each game's program's version (--version), once at the start
  bool entries_logged_ = false;
  // The games running, one a game (2026-11-26: the launcher stays open beside them, docs/launcher.md, While a game
  // runs). MechWarrior 3 and Pirate's Moon say their first frame through the ready file; a kit game is
  // counted up from its start.
  struct Run {
    SDL_Process* p = nullptr;
    std::string name, ready_file;
    Uint64 click = 0;
    bool ready = false;   // its first frame seen (a kit game: started)
    bool slow = false;    // no first frame within ready_wait_ms(): the status line says so
    bool mount = false;   // it runs from this AppImage's mount (start_game)
    bool kit = false;     // a kit game or MechWarrior 1 Remake: its status read again after it exits
    std::string said;     // the game's own status line before its first frame
    double read_ms = -1e9;
  };
  Run runs_[upgrades::update::kMaxGames];
  bool running(int i) const { return i >= 0 && i < upgrades::update::kMaxGames && runs_[i].p; }
  bool any_running() const;
  bool any_starting() const;  // a game before its first frame: the launcher waits for it, as it always has
  bool update_after_[upgrades::update::kMaxGames] = {};  // an UPDATE pressed while its game ran: done when it exits
  // A game's own setup (mw3 or mw3pm started alone) is not the launcher: it goes once the game is up.
  bool own_setup() const { return o_.setup_only || o_.pm; }
  void track(int i, SDL_Process* p, const std::string& name, bool kit);
  void game_exited(int i, int code);
  void mount_wait(Run& r);
  void restore_window();
  bool mw1_logged_ = false;  // MW1: its entry's line said once
  bool mw1_remake_ = false;  // MechWarrior 1 Remake's mw1 beside ours
  // MechWarrior 1 Remake's program; its MechWarrior (1989) folder: its status's where (b5), an older one's mw1_game=
  std::string mw1_exe_, mw1_game_;
  bool quit_after_mw1_ = false;  // --launcher-mw1 (checks): ended once its pick, copy and game are done
  std::vector<UiPanel> panels_;     // the main screen's (layout)
  std::string status_;
  bool status_good_ = true;
  Vulkan vulkan_ = Vulkan::kOk;  // ours (docs/linux.md, Vulkan): what the start found
  std::unique_ptr<Work> work_;
  Pick pick_;
  bool asking_ = false;
  Uint64 ask_at_ = 0;      // when the picker was asked for (its timing in the log)
  bool ask_shown_ = false;  // the panel took the window's focus
  // The ask waits a frame: the system's call blocks (a second on the Mac, 9 s
  // seen on a cold start), so the screen says so first (docs/launcher.md).
  bool ask_pending_ = false, ask_is_folder_ = false;
  const SDL_DialogFileFilter* ask_filters_ = nullptr;
  int ask_filter_count_ = 0;
  std::string confirm_text_, pending_source_;
  std::string found_name_, found_sub_;  // confirm_go_ 1: the find's plate (its name; what it is)
  std::string move_to_, move_from_;  // the data folder's move (ask_move)
  std::vector<std::string> move_copied_;
  std::shared_ptr<std::vector<std::string>> move_copied_out_;
  int confirm_go_ = 0;
  int work_game_ = -1;  // the game a kGameInstall work installs (-1: from a file, not read yet)
  bool auto_go_ = false;  // MW3_LAUNCHER_PICK (checks): its look's INSTALL pressed too
  bool auto_ok_ = false;
  bool pick_other_ = SDL_getenv("MW3_LAUNCHER_PICK_OTHER") != nullptr;  // checks: the first find's plate clicked (other files)  // and the notes' OK after it
  bool auto_play_ = false;  // and then the game's button on the games screen (W13: an install no longer starts it)
  bool play_after_notes_ = false;  // the notes after an install: their OK starts the game
  bool pm_after_notes_ = false;    // ... Pirate's Moon (its notes, start_pm)
  bool from_files_ = false;        // the find screen opened by GAME FILES: a new pick goes back to the games
  std::function<void()> after_convert_;  // where the movies' conversion goes on to
  bool converted_ = false;               // tried once this start (a failure is not retried before the game)
  bool disc_music_ = false;        // ADD FROM MY DISC was to bring the music too
  int disc_back_ = 0;              // the screen ADD FROM MY DISC returns to (Screen)
  bool lacks_music_ = false, lacks_movies_ = false;
  LauncherChoice choice_;
  bool done_ = false;
  Sounds sounds_;
  bool sounds_on_ = false, sounds_tried_ = false;  // loaded with the first screen
};

namespace {
// mw3.cfg's KEY= value, a path as the file keeps it; "" when none.
bool is_appimage(const std::string& p) {
  std::string e = fs::path(p).extension().string();
  for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return e == ".appimage";
}
std::string cfg_path(const std::string& write_dir, const std::string& key) {
  std::ifstream f(fs::path(write_dir) / "mw3.cfg");
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind(key + "=", 0) == 0) return own::path_from_config(line.substr(key.size() + 1));
  }
  return {};
}
}  // namespace

// MechWarrior 3's copy from its status (b2): the one it reports ready, offered (not taken) when its note says its
// search found it; the summary as check_install's.
void Launcher::find_install() {
  status_refresh(0);
  if (!status_for(0)) return find_install_older();
  install_ = {};
  const rekit::Status& s = kit_status_[0];
  if (s.state != rekit::Status::kReady || s.where.empty()) {
    std::printf("mw3: launcher: no MechWarrior 3 install found\n");
    return;
  }
  const std::string v = s.data.empty() ? "unknown" : s.data;
  install_ = {true, s.where, v, own::found_words(v, s.where, false), s.note == "found by the search, not set up yet"};
  if (install_.elsewhere) std::printf("mw3: launcher: the game files at %s: offered to copy them in\n", s.where.c_str());
  std::printf("mw3: launcher: %s\n", install_.summary.c_str());
}

// The older programs' path (b2, docs/launcher-split.md): mw3 without "setup": 1, or the game's own find screen.
// b6, the launcher alone: an older MechWarrior 3 (no "setup": 1) is not checked here. Its own status's copy when it
// says one is ready, else mw3.cfg's install=; its panel offers its UPDATE (older_game).
void Launcher::find_install_older() {
  install_ = {};
  std::error_code ec;
  const rekit::Status& s = kit_status_[0];
  std::string dir = kit_known_[0] && s.state == rekit::Status::kReady ? s.where : std::string();
  if (dir.empty() && !o_.cfg_install.empty() && fs::is_directory(o_.cfg_install, ec)) dir = o_.cfg_install;
  if (dir.empty()) {
    std::printf("mw3: launcher: no MechWarrior 3 install found\n");
    return;
  }
  install_ = {true, dir, s.data, own::found_words(s.data, dir, false), false};
  std::printf("mw3: launcher: MechWarrior 3 at %s (an older program: not checked)\n", dir.c_str());
}

namespace {

#ifndef MW3_VERSION
#define MW3_VERSION ""
#endif

// The label on a game's line (2026-11-10: shown, never blocking): "official" when its data is a release's
// (MW3 1.0 or 1.2, or 1.0 with the patch's files; PM 1.0), its program a release build (its tag a bare vX.Y.Z,
// PM's pm-vX.Y.Z); else "modified (...)" naming which. Mods are a feature, not a change to the game: they are a
// note of their own (versions_words; lead, 2026-11-10). Data nobody read (an older program's copy, b6: "") is not
// judged; "unknown" (read, and no release's) is.
std::string official_words(const std::string& data, const std::string& program, bool pm) {
  const bool data_ok = data.empty() || (pm ? data == "1.0" : data == "1.0" || data == "1.2" || data == "1.0 with 1.2 files");
  const auto release = [](const std::string& v) {  // digits and dots only after the prefix: no -N-gHASH, no -dirty
    if (v.empty() || v.find_first_not_of("0123456789.") != std::string::npos || v.front() == '.' || v.back() == '.') return false;
    return std::count(v.begin(), v.end(), '.') == 2;
  };
  const std::string prefix = pm ? "pm-v" : "v";
  const bool program_ok = program.rfind(prefix, 0) == 0 && release(program.substr(prefix.size()));
  std::vector<std::string> what;
  if (!data_ok) what.push_back("data");
  if (!program_ok) what.push_back("program");
  if (what.empty()) return "official";
  std::string s = "modified (";
  for (size_t i = 0; i < what.size(); ++i) s += (i ? ", " : "") + what[i];
  return s + ")";
}

// The package's version on the main screen's footer (lead, 2026-11-10): the launcher's own tag, the package's
// (mw3 and the launcher come from main's tag, mw3pm from pm's), without its "v": "MechWarrior 3 Rebuild 1.1.0".
std::string package_words() {
  std::string v = MW3_VERSION;
  if (v.size() > 1 && v[0] == 'v' && v[1] >= '0' && v[1] <= '9') v.erase(0, 1);
  return v.empty() ? "BattleTech launcher" : "BattleTech launcher " + v;  // 2026-11-22 (was "MechWarrior 3 Rebuild")
}

// A game's line's versions: its data's, then its program's ("data 1.2, program v1.1.0, "); a program
// that did not say is left out. No word of mods: a game started here is the original, and mods are
// played from its own MODS screen, each campaign with its own (docs/mods-campaigns-plan.md).
std::string versions_words(const std::string& data, const std::string& program) {
  std::string s = data.empty() ? std::string() : data == "unknown" ? "data unknown (a mod?), " : "data " + data + ", ";
  if (!program.empty()) s += "program " + program + ", ";
  return s;
}

using sdlfe::games::program_version;

}  // namespace

// Ours, games on demand (2026-11-22; docs/launcher.md, Games on demand): a game's program beside this one
// is the package's own and is used as before; else the one the launcher installed in its games folder. The
// launcher's own find screen inside mw3 and mw3pm has none of this.

void Launcher::resolve_games() {
  games_on_ = !o_.setup_only && !o_.pm;
  for (int i = 0; i < upgrades::update::game_count() && !o_.write_dir.empty(); ++i)  // a game's own setup too: its folder as the launcher's INSTALL picked it
    games_place_[i] = cfg_path(o_.write_dir, std::string("games_") + upgrades::update::game_info(upgrades::update::game_at(i)).key);
  if (!games_on_) {
    for (int i = 0; i < upgrades::update::game_count(); ++i) kit_refresh(i);
    return;
  }
  games_root_ = sdlfe::games::root_moved(sdlfe::games::root(o_.hidden), (fs::path(o_.write_dir) / "mw3.cfg").string());
  for (int i = 0; i < upgrades::update::game_count(); ++i) {
    beside_[i] = !game_bin(i).empty();
    resolve_game(i, false);
  }
}

// Which copy of game i PLAY starts, decided again (at the start, and after a link, an unlink or an uninstall:
// `refresh` then reads its version, files and status again, and its card's DOWNLOAD/UPDATE answer).
void Launcher::resolve_game(int i, bool refresh) {
  const upgrades::update::Game g = upgrades::update::game_at(i);
  std::string prog;
  has_installed_[i] = sdlfe::games::installed(game_root(i), g, &installed_[i], &prog);
  if (!beside_[i]) find_link(i);
  // The launcher's copy or the linked one, whichever is newer (the launcher's on a tie): an UPDATE of a linked
  // copy goes into the launcher's folder and so takes over from it.
  const bool own = has_installed_[i] && (linked_[i].empty() || upgrades::update::compare_game(installed_[i].tag, linked_tag_[i]) >= 0);
  source_[i] = beside_[i] ? kBeside : own ? kOwnCopy : !linked_[i].empty() ? kLinked : kNoCopy;
  if (source_[i] != kBeside) game_bin(i) = source_[i] == kOwnCopy ? prog : source_[i] == kLinked ? linked_[i] : std::string();
  std::printf("mw3: launcher: games: %s: %s\n", upgrades::update::game_info(g).key,
              source_[i] == kBeside    ? "beside this program"
              : source_[i] == kOwnCopy ? ("installed " + installed_[i].tag + " (" + installed_[i].from + ") at " + prog).c_str()
              : source_[i] == kLinked  ? ("linked " + linked_tag_[i] + " at " + linked_[i]).c_str()
                                       : "not installed");
  kit_refresh(i);
  if (!refresh) return;
  const std::string bin = game_bin(i);
  if (i == 0) mw3_program_ = program_tag(0), find_install();
  if (i == 1) pm_program_ = program_tag(1), find_pm(), pm_refresh();
  if (i == 2) find_mw1(), mw1_program_ = mw1_remake_ ? program_tag(2) : std::string();
  using GC = upgrades::update::GameCheck;
  GC& c = checks_[i];
  if ((c.kind == GC::kNewer || c.kind == GC::kUpToDate) && !c.release.tag.empty())
    c.kind = tag_of(i).empty() || upgrades::update::compare_game(c.release.tag, tag_of(i)) > 0 ? GC::kNewer : GC::kUpToDate;
}

// Game i's program records itself here (mw3 and mw3pm since 1.3.5; MechWarrior 1 Remake since its 1.1.0, in
// BattleTech/MechWarrior 1 Remake beside MechWarrior 3's, and MechWarrior 1 Rebuild since its 1.0.0, in its own there;
// read by path: the launcher never makes or moves them).
std::string Launcher::record_folder(int i) {
  // A check run (hidden, a shot) reads only folders it was given, never this computer's own.
  const bool check = o_.hidden || !o_.shot.empty();
  if (i == 0) return o_.write_dir;
  if (i == 1) return check && !SDL_getenv("MW3_PM_WRITE_DIR") && !SDL_getenv("MW3_USUAL_FOLDER") ? std::string() : pm_wd();
  if (i == 2 || i == 3) {
    if (check && !SDL_getenv("MW3_USUAL_FOLDER")) return {};
    const std::string mw3 = own::usual_folder();
    return mw3.empty() ? std::string() : (fs::path(mw3).parent_path() / (i == 2 ? "MechWarrior 1 Remake" : "MechWarrior 1 Rebuild")).string();
  }
  return {};
}

// A copy installed without the launcher (2026-11-24, docs/launcher.md, Copies installed without the launcher): the
// one linked before, while it is still there and still this game's; else the newest found (its record, its Setup,
// Applications), linked from now on, unless it is the one the player unlinked. A check run looks only at the record
// in its own folder, never this computer's Applications or registry.
void Launcher::find_link(int i) {
  const upgrades::update::Game g = upgrades::update::game_at(i);
  const std::string key = upgrades::update::game_info(g).key;
  linked_[i].clear(), linked_tag_[i].clear();
  const std::string prog = cfg_path(o_.write_dir, "link_" + key);
  const std::string skip = cfg_path(o_.write_dir, "unlinked_" + key);
  const bool system = !o_.hidden && o_.shot.empty();
  if (!prog.empty()) {
    const std::string v = sdlfe::games::identify(prog, g, kit_game(i) || is_appimage(prog));
    if (!v.empty()) {
      linked_[i] = prog, linked_tag_[i] = v;
      // A newer release found elsewhere takes over from the link (mech1-remake-re #2: a link to an older copy kept
      // the older one asked after the player installed the new one somewhere else).
      for (const sdlfe::games::Found& f : sdlfe::games::found_elsewhere(g, record_folder(i), system)) {
        if (f.program == skip || f.program == prog || upgrades::update::compare_game(f.version, linked_tag_[i]) <= 0) continue;
        linked_[i] = f.program, linked_tag_[i] = f.version;
      }
      if (linked_[i] != prog) {
        own::save_config_value(o_.write_dir, "link_" + key, linked_[i]);
        std::printf("mw3: launcher: games: %s: found %s (%s), newer than the linked %s (%s): linked\n", key.c_str(),
                    linked_[i].c_str(), linked_tag_[i].c_str(), prog.c_str(), v.c_str());
      }
      return;
    }
    std::printf("mw3: launcher: games: %s: the linked %s is gone or not this game's now\n", key.c_str(), prog.c_str());
  }
  for (const sdlfe::games::Found& f : sdlfe::games::found_elsewhere(g, record_folder(i), system)) {
    if (f.program == skip) continue;
    linked_[i] = f.program, linked_tag_[i] = f.version;
    own::save_config_value(o_.write_dir, "link_" + key, f.program);
    catalog::keep(o_.write_dir, key);
    std::printf("mw3: launcher: games: %s: found %s (%s), linked\n", key.c_str(), f.program.c_str(), f.how.c_str());
    return;
  }
}

// A game's page, from its panel's gear (2026-11-24, docs/launcher.md, A game's page).
void Launcher::open_game_page(int i) {
  page_game_ = i, page_confirm_ = false, files_page_ = -1, status_.clear(), screen_ = kGamePage;
  layout();
}

// A game's GAME FILES (2026-11-24: on its page; before, a button for whichever game was last clicked): the player's own
// copy of the original picked or refreshed. A game whose program does that itself has none here (PLAY asks).
bool Launcher::can_open_files(int i) const {
  if (i == 0) return true;
  if (i == 1) return !o_.pm_bin.empty();
  return kit_serves(i);  // MechWarrior 1 Remake's too, once it serves its setup (b5)
}

void Launcher::open_files(int i) {
  if (!can_open_files(i)) return;
  if (i <= 1 && !status_for(i)) return older_game(i);  // b6: an older program's files come with its update
  const int page = screen_ == kGamePage ? i : -1;
  if (i > 1) {
    open_kit_files(i, false);
  } else {
    files_pm_ = i == 1;
    if (files_pm_) pm_refresh();
    from_files_ = true, status_.clear(), screen_ = kSetupScreen;
  }
  files_page_ = page;
  layout();
}

// The page's GAME FILES row: what is in use and where; none for a game not here that says nothing of its files.
UiPanel Launcher::files_row(int i) {
  UiPanel p;
  auto is = [&](const std::string& text, const std::string& path) { p.lines = {{text, ""}, {path, ""}}; };
  const std::string none = "Not found yet", asks = "PLAY finds them, or pick them here";
  auto lacks = [](std::string s, bool music, const char* other, bool it) {
    if (music && it) return s + " \xc2\xb7 no music or " + other;
    if (music) return s + " \xc2\xb7 no music";
    return it ? s + " \xc2\xb7 no " + other : s;
  };
  auto data = [](const std::string& name, const std::string& v) { return v.empty() || v == "unknown" ? name : name + " " + v; };
  auto asks_of = [&](int g) {  // b6: a program without a served setup sets its files up after its update
    if (status_for(g)) return asks;
    if (!kit_known_[g]) return std::string(checking(g) ? "Checking its program..." : "Its program did not answer: PLAY asks again");
    return std::string("Update it to set up its game files");
  };
  if (i == 0) {
    if (install_.ok) is(lacks(data("MechWarrior 3", install_.version), lacks_music_, "movies", lacks_movies_), install_.dir);
    else is(none, asks_of(0));
  } else if (i == 1 && can_open_files(1)) {
    if (!pm_dir_.empty()) is(lacks(data("Pirate's Moon", pm_data_), pm_lacks_music_, "sounds", pm_lacks_sounds_), pm_dir_);
    else is(none, asks_of(1));
  } else if (i == 2 && mw1_remake_) {  // its MechWarrior (1989): through its setup (b5); an older Remake's mw1_game=
    if (!mw1_game_.empty()) is("MechWarrior (1989)", mw1_game_);
    else if (mw1_served()) is(none, "Your MechWarrior (1989): PLAY finds it, or pick it here");
    else if (!kit_known_[2]) is(none, checking(2) ? "Checking MechWarrior 1 Remake..." : "MechWarrior 1 Remake did not answer: PLAY asks again");
    else is(none, "Update MechWarrior 1 Remake to set up your MechWarrior (1989)");
  } else if (kit_game(i) && !game_bin(i).empty()) {
    const rekit::Status& s = kit_status_[i];
    if (kit_known_[i] && kit_set_up(i)) is(s.data.empty() ? "Your copy of the game" : s.data, s.where);
    else is(none, can_open_files(i) ? asks : "PLAY finds them");
  }
  return p;
}

// LINK A COPY's pick: the game's program in it, this game's by its stamp or its --status, linked; whichever of it
// and the launcher's own copy is newer plays.
void Launcher::link_picked(const std::string& path) {
  const int i = page_game_;
  if (i < 0) return layout();
  const upgrades::update::Game g = upgrades::update::game_at(i);
  const std::string key = upgrades::update::game_info(g).key, name = upgrades::update::game_info(g).name;
  const std::string prog = sdlfe::games::program_of_pick(path, g);
  const std::string v = prog.empty() ? std::string() : sdlfe::games::identify(prog, g, true);
  std::printf("mw3: launcher: games: %s: link %s: %s\n", key.c_str(), path.c_str(), v.empty() ? "not this game" : (prog + " " + v).c_str());
  if (v.empty()) {  // not a program: perhaps the player's own copy of the game (2026-11-24: a .zip of it was picked here)
    if (kit_serves(i) && !game_bin(i).empty()) {
      open_kit_files(i, false);
      if (screen_ == kSetupScreen) kit_look(path);
      return;
    }
    status_ = "That isn't the rebuilt " + name + ". For your own copy of the original game, use GAME FILES.", status_good_ = false;
    return layout();
  }
  if (sdlfe::games::launchers_own(prog)) {
    status_ = "That is a copy the launcher installed: there is nothing to link.", status_good_ = true;
    return layout();
  }
  own::save_config_value(o_.write_dir, "link_" + key, prog), own::save_config_value(o_.write_dir, "unlinked_" + key, "");
  catalog::keep(o_.write_dir, key);  // its entry kept should a catalog drop it
  resolve_game(i, true);
  const std::string shown = upgrades::update::game_version(v);
  status_ = source_[i] == kLinked ? "Linked " + shown + ": PLAY starts it."
                                  : "Linked " + shown + ". The launcher's own copy is newer, so PLAY starts that one.";
  status_good_ = true;
  layout();
}

// UNLINK: the launcher forgets the copy (its folder is never touched) and does not link it again by itself.
void Launcher::unlink_game() {
  const int i = page_game_;
  if (i < 0 || linked_[i].empty()) return;
  const std::string key = upgrades::update::game_info(upgrades::update::game_at(i)).key, prog = linked_[i];
  own::save_config_value(o_.write_dir, "link_" + key, ""), own::save_config_value(o_.write_dir, "unlinked_" + key, prog);
  std::printf("mw3: launcher: games: %s: unlinked %s\n", key.c_str(), prog.c_str());
  resolve_game(i, true);
  status_ = "Unlinked. Nothing was deleted.", status_good_ = true;
  layout();
}

// The game files UNINSTALL may take with the program: MechWarrior 3's or Pirate's Moon's, only when the
// copy going is the last one here (the launcher's or a linked one, none unlinked but still there, nothing beside it, no MechWarrior 1 Remake, which plays them too), and
// only a folder our own copying made (its mw3-install.txt) in the places the launcher puts them (the user folder's
// install/, the game's picked folder's). Never a copy of the original the player pointed at.
std::string Launcher::own_files(int i) {
  if ((i != 0 && i != 1) || has_installed_[i] == !linked_[i].empty() || beside_[i] || !game_bin(2).empty()) return {};
  std::error_code ec;  // one the player unlinked plays them too, while it is there
  const std::string unlinked = cfg_path(o_.write_dir, std::string("unlinked_") + upgrades::update::game_info(upgrades::update::game_at(i)).key);
  if (!unlinked.empty() && fs::exists(unlinked, ec)) return {};
  const std::string dir = i == 0 ? (install_.ok ? install_.dir : std::string()) : pm_dir_;
  if (dir.empty() || !fs::is_regular_file(fs::path(dir) / "mw3-install.txt", ec)) return {};
  const fs::path at = fs::weakly_canonical(dir, ec);
  for (const std::string& base : {games_place_[i], i == 0 ? o_.write_dir : pm_wd()})
    if (!base.empty() && fs::weakly_canonical(fs::path(base) / "install", ec) == at) return at.string();
  return {};
}

// UNINSTALL: the launcher's copy (only its marked folder) or the linked one (removal_of), and, when asked and allowed
// (own_files), its game files.
void Launcher::uninstall_go(bool files) {
  const int i = page_game_;
  if (i < 0 || work_ || (remove_linked_ ? linked_[i].empty() || removal_[i].how == sdlfe::games::Removal::kNone : !has_installed_[i])) return;
  if (remove_linked_ && o_.hidden && !SDL_getenv("MW3_LAUNCHER_TRASH")) return;  // a check run touches no real trash
  const upgrades::update::Game g = upgrades::update::game_at(i);
  const std::string root = game_root(i), fdir = files ? own_files(i) : std::string(), name = upgrades::update::game_info(g).name;
  uninstall_files_ = !fdir.empty();
  std::string title = "UNINSTALLING " + name;
  for (char& ch : title) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  work_ = std::make_unique<Work>();
  work_->kind = Work::kUninstall;
  const std::string prog = remove_linked_ ? linked_[i] : std::string();
  const sdlfe::games::Removal how = removal_[i];
  work_->start(title, [root, g, fdir, name, prog, how](const own::ProgressFn& f) {
    own::Result res;
    std::string error;
    f(own::Progress{how.how == sdlfe::games::Removal::kSetup && !prog.empty() ? "Its uninstaller is running" : "Removing the game", 0.2, ""});
    if (!prog.empty() ? !sdlfe::games::remove_linked(prog, g, how, &error) : !sdlfe::games::uninstall(root, g, &error)) return res.summary = "Nothing was removed: " + error + ".", res;
    if (!fdir.empty()) {
      f(own::Progress{"Removing the game files", 0.5, ""});
      std::error_code ec;
      const fs::path old = fs::path(fdir).parent_path() / ".install.removing";
      if (!fs::is_regular_file(fs::path(fdir) / "mw3-install.txt", ec) || fs::exists(old, ec))  // asked again at the removal
        return res.summary = "The game was removed; its game files were left alone: " + fdir, res;
      fs::rename(fdir, old, ec);
      if (!ec) fs::remove_all(old, ec);
      if (ec) return res.summary = "The game was removed; its game files could not be (" + ec.message() + "): " + fdir, res;
    }
    res.ok = true;
    res.summary = name + (fdir.empty() ? " was uninstalled." : " was uninstalled, with its game files.") + " Your pilots, saves, settings and mods are kept.";
    return res;
  });
  screen_ = kWorking, layout();
}

// Each game's repo asked on a thread of its own, the cards saying "asking" meanwhile; a check run (hidden, or a shot)
// asks only a stand-in (MW3_GAMES_API), never GitHub or git.owenmorgan.com.
void Launcher::start_games_check() {
  if (!games_on_) return;
  if ((o_.hidden || !o_.shot.empty()) && !SDL_getenv("MW3_GAMES_API")) {  // a shot is a check run too
    for (auto& c : checks_) c.kind = upgrades::update::GameCheck::kFailed, c.words = "not asked in a check run";
    std::printf("mw3: launcher: games: not asked in a check run\n");
    return;
  }
  if (!o_.shot.empty()) {  // a shot is drawn once: its answers first
    for (int i = 0; i < upgrades::update::game_count(); ++i)
      if (!beside_[i] && !removed_away(i)) checks_[i] = sdlfe::games::check(upgrades::update::game_at(i), tag_of(i));
    return;
  }
  games_check_ = std::make_unique<GamesCheck>();
  GamesCheck* gc = games_check_.get();
  std::string tags[upgrades::update::kMaxGames];
  bool ask[upgrades::update::kMaxGames];
  for (int i = 0; i < upgrades::update::game_count(); ++i) tags[i] = tag_of(i), ask[i] = !beside_[i] && !removed_away(i);
  gc->th = std::thread([gc, tags = std::vector<std::string>(tags, tags + upgrades::update::game_count()), asks = std::vector<bool>(ask, ask + upgrades::update::game_count())] {
    for (int i = 0; i < upgrades::update::game_count(); ++i)
      if (asks[i]) gc->result[i] = sdlfe::games::check(upgrades::update::game_at(i), tags[i]);
    gc->done = true;
  });
}

// The launcher's own update asked at the start (2026-11-23), quietly: a thread, the main screen never waiting
// on it, nothing said on screen unless it is newer. A check run (hidden, or a shot) asks only a stand-in
// (MW3_UPDATE_API), never GitHub or git.owenmorgan.com; a shot with one asks first, so it is drawn.
void Launcher::start_self_check() {
  if ((o_.hidden || !o_.shot.empty()) && !SDL_getenv("MW3_UPDATE_API")) {
    std::printf("mw3: launcher: update check at the start: not asked in a check run\n");
    return;
  }
  if (!o_.shot.empty()) {
    self_verdict(check_for_updates(MW3_VERSION));
    return;
  }
  self_check_ = std::make_unique<SelfCheck>();
  SelfCheck* sc = self_check_.get();
  sc->th = std::thread([sc] {
    sc->verdict = check_for_updates(MW3_VERSION);
    sc->done = true;
  });
}

void Launcher::take_self_check() {
  if (!self_check_ || !self_check_->done) return;
  self_check_->th.join();
  const upgrades::update::Verdict v = self_check_->verdict;
  self_check_.reset();
  self_verdict(v);
  if (screen_ == kMain) layout();
  recheck_done();
}

// MW3_LAUNCHER_NAV's steps (checks): keys or a pad as a list ("next,next,press"), each focus logged.
void Launcher::nav_steps() {
  nav_steps(SDL_getenv("MW3_LAUNCHER_NAV"));
}
void Launcher::nav_steps(const char* steps) {
  if (!steps) return;
  const std::string all = steps;
  for (size_t at = 0; at <= all.size();) {
    const size_t comma = std::min(all.find(',', at), all.size());
    const std::string s = all.substr(at, comma - at);
    nav(s == "next" ? 1 : s == "prev" ? -1 : s == "press" ? 2 : s == "back" ? 3 : 0);
    const int f = focus_;
    if (f >= 0 && f < static_cast<int>(buttons_.size()) && s != "press" && s != "back")
      std::printf("mw3: launcher: focus %s\n", buttons_[f].log_name().c_str());
    at = comma + 1;
  }
}

void Launcher::self_verdict(const upgrades::update::Verdict& v) {
  self_kind_ = v.kind, self_words_ = v.words;
  std::printf("mw3: launcher: update%s: %s\n", rechecking_ ? "" : " check at the start", v.words.c_str());  // at the start a failure is in the log only
  if (v.kind != upgrades::update::Verdict::kNewer) return;
  self_update_ = std::make_shared<upgrades::update::Verdict>(v);
  std::printf("mw3: launcher: header UPDATE AVAILABLE %s\n", upgrades::update::plain_version(v.release.tag).c_str());
}

void Launcher::take_games_check() {
  if (!games_check_ || !games_check_->done) return;
  games_check_->th.join();
  for (int i = 0; i < upgrades::update::game_count(); ++i) {
    if (beside_[i]) continue;
    using GC = upgrades::update::GameCheck;
    const GC& now = games_check_->result[i];
    if (now.kind == GC::kFailed && (checks_[i].kind == GC::kNewer || checks_[i].kind == GC::kUpToDate)) continue;  // asked again, not answered: as known
    checks_[i] = now;
    if ((now.kind == GC::kNewer || now.kind == GC::kUpToDate) && !now.release.tag.empty())  // installed or updated while it was asked
      checks_[i].kind = tag_of(i).empty() || upgrades::update::compare_game(now.release.tag, tag_of(i)) > 0 ? GC::kNewer : GC::kUpToDate;
    std::printf("mw3: launcher: games: %s: %s\n", upgrades::update::game_info(upgrades::update::game_at(i)).key, checks_[i].words.c_str());
  }
  games_check_.reset();
  if (screen_ == kMain) layout();
  recheck_done();
  if (const char* k = SDL_getenv("MW3_LAUNCHER_GAME_INSTALL"))  // checks: a card's INSTALL or UPDATE pressed once it can be
    for (int i = 0; i < upgrades::update::game_count(); ++i)
      if (std::strcmp(k, upgrades::update::game_info(upgrades::update::game_at(i)).key) == 0) install_game(i);
}

std::vector<int> Launcher::game_updates() const {
  std::vector<int> out;
  for (int i = 0; i < upgrades::update::game_count(); ++i)
    if (games_on_ && !beside_[i] && source_[i] != kNoCopy && checks_[i].kind == upgrades::update::GameCheck::kNewer) out.push_back(i);
  return out;
}

void Launcher::recheck() {
  if (rechecking_ || work_) return;
  rechecking_ = true;
  status_ = "Checking for updates...", status_good_ = true;
  if (!games_check_) start_games_check();  // the start's own still out: its answer is this one's
  if (!self_check_) start_self_check();
  layout();
  recheck_done();
}

void Launcher::recheck_done() {
  if (!rechecking_ || games_check_ || self_check_) return;
  rechecking_ = false;
  std::string games, unasked;
  for (int i : game_updates())
    games += (games.empty() ? "" : ", ") + std::string(upgrades::update::game_info(upgrades::update::game_at(i)).name) + " " +
             upgrades::update::game_version(checks_[i].release.tag);
  for (int i = 0; i < upgrades::update::game_count(); ++i)
    if (!beside_[i] && source_[i] != kNoCopy && checks_[i].kind == upgrades::update::GameCheck::kFailed)
      unasked += (unasked.empty() ? "" : ", ") + std::string(upgrades::update::game_info(upgrades::update::game_at(i)).name);
  if (self_kind_ == upgrades::update::Verdict::kFailed) unasked = unasked.empty() ? "the launcher" : "the launcher, " + unasked;
  if (!games.empty()) status_ = "Ready to update: " + games + ".", status_good_ = true;
  else if (self_update_) status_.clear();
  else if (unasked.empty()) status_ = "Everything is up to date.", status_good_ = true;
  else status_ = "Couldn't check " + unasked + ". Check your connection and try again.", status_good_ = false;
  std::printf("mw3: launcher: updates: %s\n", status_.c_str());
  if (screen_ != kMain || work_) return;
  update_box_ = self_update_ != nullptr;
  layout();
  if (update_box_ && SDL_getenv("MW3_LAUNCHER_UPDATE_GO")) activate(kUpdateLauncher);  // checks: UPDATE LAUNCHER pressed
  else if (update_box_ && SDL_getenv("MW3_LAUNCHER_UPDATE_ALL")) activate(kUpdateAll);  // checks: UPDATE ALL pressed
}

// UPDATE ALL's next: a game, its own UPDATE; the launcher last, as it restarts. A failure or a cancel stops it there.
void Launcher::next_update() {
  while (!update_queue_.empty()) {
    const int i = update_queue_.front();
    update_queue_.erase(update_queue_.begin());
    install_game_now(i);
    if (work_) return;
  }
  for (bool w : update_after_)  // UPDATE ALL's launcher last, after a game waiting for its end (its start would lose it)
    if (w) return;
  if (std::exchange(update_queue_self_, false) && self_update_) update_ = self_update_, read_update_notes(), activate(kUpdateGo);
}

std::string Launcher::game_words(int i) const {
  using C = upgrades::update::GameCheck;
  const C& c = checks_[i];
  switch (c.kind) {
    case C::kAsking: return "Checking for the newest version...";
    case C::kFailed: return "Can't reach the server:\nthe gear can install from a file";
    case C::kNoRelease: return "No download for this computer yet";
    default: {
      const upgrades::update::Asset* a = c.asset();
      return "DOWNLOAD gets " + upgrades::update::game_version(c.release.tag) + (a && a->size ? " (" + upgrades::update::size_words(a->size) + ")" : "");
    }
  }
}

// INSTALL and UPDATE: the newest release's file, downloaded, checked and put in place on the worker.
void Launcher::install_game(int i) {
  using namespace upgrades::update;
  if (i < 0 || i >= upgrades::update::game_count() || work_ || checks_[i].kind != GameCheck::kNewer) return;
  if (game_bin(i).empty()) return ask_place(i);  // a first install: where, asked (an UPDATE goes where it is)
  install_game_now(i);
}

// A card's first INSTALL (2026-11-22): where the game goes, the folder shown with CHANGE FOLDER; its pilots,
// saves and settings stay in the user folder. MW3_LAUNCHER_INSTALL_TO (checks): that folder picked, then INSTALL.
void Launcher::ask_place(int i) {
  place_game_ = i, place_ = games_place_[i];
  confirm_go_ = 7, screen_ = kConfirm, status_.clear();
  layout();
  if (SDL_getenv("MW3_LAUNCHER_GAME_INSTALL")) {
    if (const char* to = SDL_getenv("MW3_LAUNCHER_INSTALL_TO"); to && *to) picked(kInstallTo, to);
    activate(kPlaceGo);
  }
}

void Launcher::install_game_now(int i) {
  using namespace upgrades::update;
  if (i < 0 || i >= upgrades::update::game_count() || work_ || checks_[i].kind != GameCheck::kNewer) return;
  if (running(i)) {  // never under a running game; begun when it exits (the run loop)
    update_after_[i] = true;
    status_ = runs_[i].name + " updates when it closes.", status_good_ = true;
    std::printf("mw3: launcher: %s\n", status_.c_str());
    return layout();
  }
  const Game g = upgrades::update::game_at(i);
  const bool update = !game_bin(i).empty();
  work_ = std::make_unique<Work>();
  work_->kind = Work::kGameInstall;
  auto done = game_done_ = std::make_shared<sdlfe::games::Done>();
  const Release r = checks_[i].release;
  const std::string root = place_game_ == i ? (place_.empty() ? games_root_ : place_) : game_root(i);
  work_game_ = i;
  std::string title = std::string(update ? "UPDATING " : "DOWNLOADING ") + game_info(g).name;
  for (char& ch : title) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  work_->start(title, [done, g, r, root](const own::ProgressFn& f) {
    *done = sdlfe::games::install_release(root, g, r, [&](double x, const std::string& w) { return f(own::Progress{w, x, ""}); });
    own::Result res;
    res.ok = done->ok, res.cancelled = done->cancelled, res.summary = done->words;
    return res;
  });
  screen_ = kWorking, layout();
}

void Launcher::install_file_go() {
  if (work_ || install_file_.empty()) return;
  confirm_go_ = 0, work_game_ = -1;
  work_ = std::make_unique<Work>();
  work_->kind = Work::kGameInstall;
  auto done = game_done_ = std::make_shared<sdlfe::games::Done>();
  const std::string root = games_root_, file = install_file_, sha = file_check_ ? file_check_->sha256 : std::string();
  work_->start("INSTALLING FROM A FILE", [done, root, file, sha](const own::ProgressFn& f) {
    *done = sdlfe::games::install_file(root, file, sha, [&](double x, const std::string& w) { return f(own::Progress{w, x, ""}); });
    own::Result res;
    res.ok = done->ok, res.cancelled = done->cancelled, res.summary = done->words;
    return res;
  });
  screen_ = kWorking, layout();
}

// An installed game in use at once: its program, version and card.
void Launcher::game_installed(const sdlfe::games::Done& d) {
  using namespace upgrades::update;
  const int i = static_cast<int>(d.game);
  const std::string key = std::string("games_") + game_info(d.game).key;
  if (place_game_ == i && work_game_ == i) {  // a card's INSTALL: its folder kept for UPDATE and the game files
    games_place_[i] = place_, own::save_config_value(o_.write_dir, key, place_);
  } else if (work_game_ < 0 && !games_place_[i].empty()) {  // INSTALL FROM A DOWNLOADED FILE: into the games folder, now the one in use
    games_place_[i].clear(), own::save_config_value(o_.write_dir, key, "");
  }
  place_game_ = -1;
  std::string prog;
  has_installed_[i] = sdlfe::games::installed(game_root(i), d.game, &installed_[i], &prog);
  if (has_installed_[i]) catalog::keep(o_.write_dir, game_info(d.game).key);  // its entry kept should a catalog drop it
  if (beside_[i]) {
    status_ += " This package's own copy beside the launcher is still the one PLAY starts.";
  } else if (has_installed_[i]) {
    game_bin(i) = prog, source_[i] = kOwnCopy;
    if (i == 0) mw3_program_ = program_version(prog);
    if (i == 1) pm_program_ = program_version(prog), find_pm(), pm_refresh();
    if (i == 2) find_mw1(), mw1_program_ = program_version(mw1_exe_);
    if (kit_game(i)) kit_refresh(i);
  }
  GameCheck& c = checks_[i];
  if (c.kind == GameCheck::kNewer && has_installed_[i] && compare_game(c.release.tag, installed_[i].tag) <= 0) c.kind = GameCheck::kUpToDate;
  std::printf("mw3: launcher: games: %s now %s at %s\n", game_info(d.game).key, installed_[i].tag.c_str(), prog.c_str());
}

// A game just installed whose catalog entry lists its original files (2026-11-26, docs/launcher.md, Games on demand)
// and whose status says it lacks them: they are fetched and handed to its own setup at once (the DOWNLOAD press is the
// player's say; no question), its progress and CANCEL on the work screen. False: nothing to do, or it cannot be
// (said in the log), and the launcher goes on as before.
bool Launcher::get_original(int i) {
  using namespace upgrades::update;
  if (i < 0 || i >= game_count() || game_bin(i).empty()) return false;
  const CatalogGame& e = catalog_game(game_at(i));
  if (e.original.empty()) return false;
  rekit::Status s;
  std::string why;
  if (!sdlfe::games::kit_status(game_bin(i), &s, status_args(i), 10, &why)) {
    std::printf("mw3: launcher: original: %s: not fetched: its status did not answer (%s)\n", e.key.c_str(), why.c_str());
    return false;
  }
  if (s.state != rekit::Status::kMissing) {  // ready, or some there (incomplete): its own GAME FILES, never replaced unasked
    std::printf("mw3: launcher: original: %s: not fetched: it has its game files\n", e.key.c_str());
    return false;
  }
  if (s.setup < rekit::kServedSetup) {
    std::printf("mw3: launcher: original: %s: not fetched: its setup is not one the launcher drives\n", e.key.c_str());
    return false;
  }
  std::printf("mw3: launcher: original: %s: getting its original game files (%d sources)\n", e.key.c_str(), static_cast<int>(e.original.size()));
  work_ = std::make_unique<Work>();
  work_->kind = Work::kOriginal;
  work_game_ = i;
  auto done = game_done_ = std::make_shared<sdlfe::games::Done>();
  done->game = game_at(i);
  const std::string bin = game_bin(i), folder = games_place_[i];
  const std::vector<std::string> more = status_args(i);
  const std::vector<CatalogGame::Original> sources = e.original;
  work_->start("GETTING THE ORIGINAL GAME FILES", [done, bin, sources, more, folder](const own::ProgressFn& f) {
    const upgrades::update::Game g = done->game;
    *done = sdlfe::games::get_original(bin, sources, more, folder, [&](double x, const std::string& w) { return f(own::Progress{w, x, ""}); });
    done->game = g;
    own::Result res;
    res.ok = done->ok, res.cancelled = done->cancelled, res.summary = done->words;
    return res;
  });
  screen_ = kWorking, layout();
  return true;
}

// Its original files in: in use as a PICK A FILE's would be. Not: the game installed as before, GAME FILES for them.
void Launcher::got_original(const own::Result& r) {
  using namespace upgrades::update;
  const int i = game_done_ ? static_cast<int>(game_done_->game) : work_game_;
  const std::string name = game_info(game_at(i)).name, key = game_info(game_at(i)).key;
  const std::string where = game_done_ ? game_done_->where : std::string();
  status_fresh_[i] = false;
  screen_ = kMain;
  if (!r.ok) {
    std::printf("mw3: launcher: original: %s: not set up: %s\n", key.c_str(), r.summary.c_str());
    status_ = name + " is installed. " +
              (r.cancelled ? std::string("Getting its original game files was canceled") : "Its original game files could not be fetched (" + r.summary + ")") +
              ": add them from GAME FILES on its gear.";
    status_good_ = r.cancelled;
    std::printf("mw3: launcher: %s\n", status_.c_str());
  } else {
    std::printf("mw3: launcher: original: %s: set up at %s\n", key.c_str(), where.c_str());
    status_ = r.summary.empty() ? name + "'s original game files are in." : r.summary, status_good_ = true;
    if (i == 0 && !where.empty()) {
      own::save_install(o_.write_dir, where), o_.cfg_install = where;
      find_install();
      if (install_.ok) return use_install(install_.dir, install_.version, status_);
    } else if (i == 1 && !where.empty()) {
      return use_pm(where, status_);
    } else {
      kit_refresh(i);
    }
  }
  layout();
  next_update();
}

// MW1's card (docs/launcher.md, The cards): MechWarrior 1 Remake's `mw1` beside ours. Its MechWarrior (1989) is its
// own setup's (b5: its status's where, the launcher's screens through its served setup); an older Remake's is
// mw3.cfg's mw1_game=, taken as it is (the launcher's own finder went with the raw port, b5). Its status is asked with
// MechWarrior 3's and Pirate's Moon's copies, so it is asked after they are found.
// At the start its status is asked on its own thread (its panel "checking"): the first frame does not wait for it.
void Launcher::find_mw1(bool start) {
  mw1_remake_ = !o_.mw1_bin.empty();
  if (!mw1_remake_) return;
  mw1_exe_ = o_.mw1_bin;
  std::printf("mw3: launcher: MechWarrior 1 at %s\n", mw1_exe_.c_str());
  if (start) ask_later(2);
  else kit_refresh(2);
}

std::string Launcher::files_out() {
  if (kit_files_ >= 0 && files_into_.empty()) return kit_found_ ? kit_found_->into : "";
  return (fs::path(files_into_.empty() ? files_wd() : files_into_) / "install").make_preferred().string();
}

// "Use these game files?": what was found, and where it goes (its own line, CHANGE FOLDER beside INSTALL).
void Launcher::files_confirm_text() {
  confirm_text_ = files_words_ + " Your own files are only read, and may be deleted afterwards.";
  confirm_into_ = files_out();
  std::printf("mw3: launcher: game files into %s\n", confirm_into_.c_str());
}

// Pirate's Moon: its folder as last picked, while it still holds the game;
// else the places it may be installed (sdlfe::install_candidates: its
// registry key, the usual folders, Wine, Proton and Lutris prefixes, the
// Mac's bottles), each read by map_source so an MW3 folder is never taken,
// the one found saved to mw3.cfg as pm_install= (docs/launcher.md).
// Pirate's Moon's copy from its status (b2), as find_pm_older took it: the one ready, offered when its search found it;
// mw3.cfg's pm_install= follows ours. A copy only pm_install= names (mw3pm's own mw3.cfg says nothing of it) is still
// looked at as before.
void Launcher::find_pm() {
  if (o_.pm_bin.empty() && !o_.pm) return;
  status_refresh(1);
  const rekit::Status& s = kit_status_[1];
  const bool ready = s.state == rekit::Status::kReady && !s.where.empty();
  if (!status_for(1) || (!ready && !o_.cfg_pm_install.empty())) return find_pm_older();
  pm_found_.clear();
  if (!ready) {
    std::printf("mw3: launcher: no Pirate's Moon install found\n");
    return;
  }
  if (s.note == "found by the search, not set up yet") {
    pm_found_ = s.where;
    std::printf("mw3: launcher: Pirate's Moon found at %s: offered, not taken\n", s.where.c_str());
    return;
  }
  pm_dir_ = s.where;
  std::error_code ec;
  if (o_.cfg_pm_install.empty() || !fs::equivalent(o_.cfg_pm_install, s.where, ec)) {
    if (!o_.cfg_pm_install.empty())
      std::printf("mw3: launcher: Pirate's Moon in our folder %s, not %s: ours in use\n", s.where.c_str(), o_.cfg_pm_install.c_str());
    if (!o_.pm) own::save_config_value(o_.write_dir, "pm_install", s.where);
  }
  std::printf("mw3: launcher: Pirate's Moon at %s: ready\n", pm_dir_.c_str());
}

// b6, the launcher alone: an older Pirate's Moon (no "setup": 1), or a served one that names none while mw3.cfg's
// pm_install= does: its status's copy when ready, else pm_install=, not checked here.
void Launcher::find_pm_older() {
  pm_found_.clear();
  std::error_code ec;
  const rekit::Status& s = kit_status_[1];
  std::string dir = kit_known_[1] && s.state == rekit::Status::kReady ? s.where : std::string();
  // Our own copy first (2026-11-24, issue #10's rule): one in Pirate's Moon's own folder over an older pm_install=.
  const std::string ours = (fs::path(pm_wd()) / "install").string();
  if (dir.empty() && !own::any_case(ours, "Mech3.exe").empty()) {
    dir = ours;
    if (!o_.cfg_pm_install.empty() && !fs::equivalent(o_.cfg_pm_install, ours, ec))
      std::printf("mw3: launcher: Pirate's Moon in our folder %s, not %s: ours in use\n", ours.c_str(), o_.cfg_pm_install.c_str());
    if (!o_.pm && (o_.cfg_pm_install.empty() || !fs::equivalent(o_.cfg_pm_install, ours, ec)))
      own::save_config_value(o_.write_dir, "pm_install", ours);
  }
  if (dir.empty() && !o_.cfg_pm_install.empty() && fs::is_directory(o_.cfg_pm_install, ec)) dir = o_.cfg_pm_install;
  if (dir.empty()) {
    std::printf("mw3: launcher: no Pirate's Moon install found\n");
    return;
  }
  pm_dir_ = dir;
  std::printf("mw3: launcher: Pirate's Moon at %s (not checked here)\n", pm_dir_.c_str());
}

// MechWarrior 1 Remake (mw1/src/mw1_main.cpp), this window hidden until it exits. One that serves its setup is given
// the games it needs as the kit has it (--with, b5: its MechWarrior (1989) is its own, kept in its mw1.cfg); an older
// one the three installs as before. Both take this start's choices (its status's play: 1) and --no-launcher.
void Launcher::run_mw1_remake() {
  std::vector<std::string> args = {mw1_exe_};
  if (mw1_served()) {
    for (const std::string& a : with_args(2)) args.push_back(a);
  } else {
    for (const char* a : {"--install", pm_dir_.c_str(), "--mw3-install", install_.dir.c_str(), "--mw1-game", mw1_game_.c_str()}) args.push_back(a);
  }
  args.push_back("--no-launcher");
  args.insert(args.end(), o_.pm_args.begin(), o_.pm_args.end());
  std::vector<const char*> raw;
  for (const std::string& a : args) raw.push_back(a.c_str());
  raw.push_back(nullptr);
  if (running(2)) return;
  SDL_Process* p = SDL_CreateProcess(raw.data(), false);
  if (!p) {
    status_ = std::string("MechWarrior 1 did not start: ") + SDL_GetError(), status_good_ = false;
    std::printf("mw3: launcher: %s\n", status_.c_str());
    return;
  }
  std::printf("mw3: launcher: MechWarrior 1 started\n");
  track(2, p, "MechWarrior 1", true);  // its status read again when it exits: what it did to its files (b5)
}

// An older Remake (no served setup) with no MechWarrior (1989) kept: only the launcher's finder gave it one, and that
// went in b5. Its update is the way on: UPDATE when one is known, else said (CHECK FOR UPDATES finds it).
void Launcher::mw1_too_old() {
  using GC = upgrades::update::GameCheck;
  std::printf("mw3: launcher: MechWarrior 1 Remake %s cannot be given its MechWarrior (1989): %s\n",
              mw1_program_.empty() ? "(unknown)" : mw1_program_.c_str(), checks_[2].kind == GC::kNewer ? "its update offered" : "said");
  if (games_on_ && checks_[2].kind == GC::kNewer && !work_) return install_game(2);
  status_ = "This MechWarrior 1 Remake is older than the launcher: update it to set up your MechWarrior (1989).";
  status_good_ = false;
  layout();
}

// MechWarrior 3 or Pirate's Moon whose program does not serve its setup ("setup": 1), in the launcher alone (b6):
// the launcher no longer finds or copies its game files itself; its update does. UPDATE when one is known, else said.
void Launcher::older_game(int i, int play) {
  if (!kit_known_[i]) return not_answered(i, play);  // its status unknown: not older (mech1-remake-re #2)
  using GC = upgrades::update::GameCheck;
  const std::string name = upgrades::update::game_info(upgrades::update::game_at(i)).name;
  std::printf("mw3: launcher: %s does not set up its game files here: %s\n", name.c_str(),
              checks_[i].kind == GC::kNewer ? "its update offered" : "said");
  if (games_on_ && checks_[i].kind == GC::kNewer && !work_) return install_game(i);
  status_ = "This " + name + " is older than the launcher: update it to set up its game files.";
  status_good_ = false, screen_ = kMain;
  layout();
}

// A game's --status (rekit/status.h): a kit game's for its card; MechWarrior 3's and Pirate's Moon's for their panels
// (b2), with the user folder the launcher knows for each. At the start (starting_) only asked, on its own thread:
// status_collect takes the answers.
namespace {
const char kNotOurs[] = "not asked: its program file carries no version of ours";
// An ask's limit: 10 s where the launcher waits on it, 30 s on its own thread; MW3_STATUS_SECONDS (checks) for both.
int status_seconds(int usual) {
  const char* env = SDL_getenv("MW3_STATUS_SECONDS");
  return env && std::atoi(env) > 0 ? std::atoi(env) : usual;
}
}  // namespace

void Launcher::kit_refresh(int i) {
  if (checking(i)) return;  // being asked on its thread: its answer comes in asks_collect
  kit_known_[i] = false, status_fresh_[i] = false;
  kit_why_[i].clear();
  if ((!kit_game(i) && i > 2) || game_bin(i).empty()) return;
  kit_status_[i] = {};
  if (i == 2) mw1_game_.clear();
  // MechWarrior 3's, Pirate's Moon's and MechWarrior 1 Remake's programs asked only when they are ours (may_ask). The
  // Remake's is asked with the other two's copies (--with), once they are found (find_mw1).
  const bool may = may_ask(i);
  if (starting_ && i == 2) return;
  if (starting_) {
    if (status_ask_[i].joinable()) status_ask_[i].join();
    status_ask_[i] = std::thread([this, i, may, bin = game_bin(i), args = status_args(i)] {
      status_said_[i] = {}, status_why_[i] = may ? "" : kNotOurs;
      status_ok_[i] = may && sdlfe::games::kit_status(bin, &status_said_[i], args, status_seconds(10), &status_why_[i]);
    });
    return;
  }
  std::string why = may ? "" : kNotOurs;
  kit_known_[i] = may && sdlfe::games::kit_status(game_bin(i), &kit_status_[i], status_args(i), status_seconds(10), &why);
  status_fresh_[i] = true;
  if (!kit_known_[i]) kit_why_[i] = why.empty() ? "no status" : why;
  if (i == 2) mw1_game_ = mw1_served() ? (kit_set_up(2) ? kit_status_[2].where : std::string()) : o_.cfg_mw1_game;
  log_status(i);
}

// A program file that is ours: our version in it (a file without one, a script or not ours, is never run:
// sdl_program_launcher_shows_each_games_versions), or a linked AppImage whose own --status said it is this game
// (find_link's identify): an AppImage's version is inside its packed files, unreadable from outside (mech1-remake-re
// #2). Kit games are always asked.
bool Launcher::may_ask(int i) {
  const std::string& bin = game_bin(i);
  if (i > 2 || !program_version(bin).empty()) return true;
  return is_appimage(bin) && bin == linked_[i] && !linked_tag_[i].empty();
}

// Its program's version: from its file, else what a linked AppImage's status said (unknown "").
std::string Launcher::program_tag(int i) {
  const std::string& bin = game_bin(i);
  if (bin.empty()) return {};
  const std::string v = program_version(bin);
  if (!v.empty() || bin != linked_[i] || linked_tag_[i] == "unknown") return v;
  return linked_tag_[i];
}

std::string Launcher::asked_name(int i) const {
  return i == 2 ? "MechWarrior 1 Remake" : upgrades::update::game_info(upgrades::update::game_at(i)).name;
}

void Launcher::log_status(int i) {
  std::printf("mw3: launcher: games: %s: status %s\n", upgrades::update::game_info(upgrades::update::game_at(i)).key,
              kit_known_[i] ? rekit::state_name(kit_status_[i].state).c_str() : ("no answer (" + kit_why_[i] + ")").c_str());
}

// Game i's status asked on its own thread (status_seconds): `play` is pressed again once it
// answers, and `said` says when it gives none. False when it has no program to ask.
bool Launcher::ask_later(int i, int play, bool said) {
  if (game_bin(i).empty()) return false;
  if (play) play_after_[i] = play;
  if (said) said_after_[i] = true;
  if (checking(i)) return true;
  kit_known_[i] = false, kit_status_[i] = {}, kit_why_[i].clear(), status_fresh_[i] = false;
  if (i == 2) mw1_game_ = o_.cfg_mw1_game;  // as an unknown Remake's
  const int secs = status_seconds(30);
  auto a = std::make_shared<LaterAsk>();
  a->bin = game_bin(i);
  const bool may = may_ask(i);
  later_[i] = a;
  std::printf("mw3: launcher: games: %s: status asked on its own thread (%d s)\n",
              upgrades::update::game_info(upgrades::update::game_at(i)).key, secs);
  std::thread([a, may, secs, args = status_args(i)] {
    if (!may) a->why = kNotOurs;
    else a->ok = sdlfe::games::kit_status(a->bin, &a->said, args, secs, &a->why);
    if (!a->ok && a->why.empty()) a->why = "no status";
    a->done = true;
  }).detach();
  return true;
}

// Each pass: the answers of the asks on their own threads, taken as kit_refresh's are; what was pressed meanwhile
// pressed again, or a press's ask that gave none said.
void Launcher::asks_collect(bool lay) {
  for (int i = 0; i < upgrades::update::game_count(); ++i) {
    const std::shared_ptr<LaterAsk> a = later_[i];
    if (!a || !a->done) continue;
    later_[i].reset();
    const int play = play_after_[i];
    const bool said = said_after_[i];
    play_after_[i] = 0, said_after_[i] = false;
    if (a->bin != game_bin(i)) {  // another copy since (an install): asked again as it is now
      ask_later(i, play, said);
      continue;
    }
    kit_status_[i] = a->said, kit_known_[i] = a->ok, status_fresh_[i] = true, kit_why_[i] = a->ok ? "" : a->why;
    if (i == 2) mw1_game_ = mw1_served() ? (kit_set_up(2) ? kit_status_[2].where : std::string()) : o_.cfg_mw1_game;
    log_status(i);
    if (i == 0) find_install(), notes_refresh();
    if (i == 1) find_pm(), pm_refresh();
    if (a->ok && i <= 1 && mw1_remake_) ask_later(2);  // the Remake's --with: this game's copy now
    if (!lay) continue;
    if (!a->ok && said) {
      status_ = asked_name(i) + " did not answer the launcher (see mw3launcher.log).", status_good_ = false;
      std::printf("mw3: launcher: %s\n", status_.c_str());
    } else if (said) {
      status_.clear();
    }
    if (a->ok && play && screen_ == kMain && !work_) activate(play);
    layout();
  }
}

// Checks and shots: the asks on their own threads waited for, as the start's are, so their panels are known.
void Launcher::asks_wait() {
  while (any_checking()) {
    bool all = true;
    for (const auto& a : later_) all = all && (!a || a->done);
    if (all) break;
    SDL_Delay(5);
  }
  asks_collect(false);
}

// Pressed while its status is unknown: asked again, "Checking ..." meanwhile, pressed again once it answers.
void Launcher::not_answered(int i, int play) {
  if (!ask_later(i, play, true)) {
    status_ = asked_name(i) + " is not installed here.", status_good_ = false;
    return layout();
  }
  status_ = "Checking " + asked_name(i) + "...", status_good_ = true;
  std::printf("mw3: launcher: %s\n", status_.c_str());
  layout();
}

// MechWarrior 3's status for the launcher's own user folder (they share it), Pirate's Moon's for its own (pm_wd);
// a kit game's in its own (nothing passed).
std::vector<std::string> Launcher::status_args(int i) {
  const std::string wd = i == 0 ? o_.write_dir : i == 1 ? pm_wd() : std::string();
  if (i > 1) return with_args(i);
  return wd.empty() ? std::vector<std::string>{} : std::vector<std::string>{"--write-dir", wd};
}

// The games game i needs (the catalog's needs), each as --with <its status id>=<its copy in use>, where the launcher
// has one: MechWarrior 3's and Pirate's Moon's copies, a kit game's status where once set up (the kit's Needs).
std::vector<std::string> Launcher::with_args(int i) {
  std::vector<std::string> out;
  for (const std::string& key : upgrades::update::catalog_game(upgrades::update::game_at(i)).needs) {
    upgrades::update::Game g;
    if (!upgrades::update::game_of_key(key, &g)) continue;
    const int n = static_cast<int>(g);
    const std::string where = n == 0 ? (install_.ok ? install_.dir : std::string())
                              : n == 1 ? pm_dir_
                              : kit_known_[n] && kit_set_up(n) ? kit_status_[n].where : std::string();
    const std::string id = sdlfe::games::kit_id(g);
    if (!where.empty() && !id.empty()) out.push_back("--with"), out.push_back(id + "=" + where);
  }
  return out;
}

// The start's asks, each waited for (the first frame waits as it did for the checks in this program), then logged.
void Launcher::status_collect() {
  starting_ = false;
  for (int i = 0; i < upgrades::update::game_count(); ++i) {
    if (!status_ask_[i].joinable()) continue;
    status_ask_[i].join();
    kit_status_[i] = status_said_[i], kit_known_[i] = status_ok_[i], status_fresh_[i] = true;
    kit_why_[i] = status_ok_[i] ? "" : status_why_[i].empty() ? "no status" : status_why_[i];
    log_status(i);
    // MechWarrior 3's or Pirate's Moon's that gave none (a first start's virus scan, a slow disk): asked once more on
    // its own thread, its panel "checking" meanwhile; the first frame does not wait for it.
    if (i <= 1 && !status_ok_[i] && kit_why_[i] != kNotOurs) ask_later(i);
  }
}

namespace {
std::string own_join(const std::vector<std::string>& v) {  // "music, movies"
  std::string out;
  for (const std::string& w : v) out += (out.empty() ? "" : ", ") + w;
  return out;
}
}  // namespace

bool Launcher::status_for(int i, const std::string& dir) const {
  if (i > 1 || !kit_known_[i] || kit_status_[i].setup < rekit::kServedSetup) return false;
  if (dir.empty()) return true;
  std::error_code ec;
  return !kit_status_[i].where.empty() && fs::equivalent(kit_status_[i].where, dir, ec);
}

// A game whose own search came on its files says ready, with this note (inception-re and mech1-re, word for word): a
// plain start would open its own setup window, so the launcher's screens come first (2026-11-25).
bool Launcher::kit_set_up(int i) const {
  return kit_status_[i].state == rekit::Status::kReady && kit_status_[i].note != "found by the search, not set up yet";
}

// PLAY on a game on the kit's contract: started plain (its own setup screen opens when its game files are missing),
// this window hidden until it exits, as MechWarrior 1's are; its status read again after.
// One that serves its setup has its game files found on the launcher's own screens first.
void Launcher::run_kit(int i, bool set_up) {
  const std::string bin = game_bin(i), name = upgrades::update::game_info(upgrades::update::game_at(i)).name;
  if (bin.empty() || work_) return;
  if (!set_up && kit_serves(i) && !kit_set_up(i)) return open_kit_files(i, true);
  if (running(i)) return;
  const char* args[] = {bin.c_str(), nullptr};
  SDL_Process* p = SDL_CreateProcess(args, false);
  if (!p) {
    status_ = name + " did not start: " + SDL_GetError(), status_good_ = false;
    std::printf("mw3: launcher: %s\n", status_.c_str());
    return layout();
  }
  std::printf("mw3: launcher: %s started\n", name.c_str());
  track(i, p, name, true);  // its status read again when it exits
  layout();
}

// A kit game or MechWarrior 1 Remake started (no first-frame signal: running from its start). The launcher stays
// open beside it (until 2026-11-26 it hid until the game exited) and reads its status again when it exits.
void Launcher::track(int i, SDL_Process* p, const std::string& name, bool kit) {
  Run& r = runs_[i];
  r = Run();
  r.p = p, r.name = name, r.kit = kit, r.ready = kit, r.click = SDL_GetTicksNS();
  if (!kit) return;
  allow_foreground(p);  // FIX (A), W4 as start_game's: the launcher stays up, so the game is let to the front
  status_ = name + " is running.", status_good_ = true;
}

void Launcher::open_kit_files(int i, bool play) {
  const std::string name = upgrades::update::game_info(upgrades::update::game_at(i)).name;
  rekit::SetupMessage m;
  if (!sdlfe::games::kit_setup(game_bin(i), {"--describe"}, 10, [](const rekit::SetupMessage&) { return true; }, &m) || !m.ok) {
    status_ = name + ": " + (m.summary.empty() ? "its setup did not answer." : m.summary), status_good_ = false;
    std::printf("mw3: launcher: %s\n", status_.c_str());
    screen_ = kMain;
    return layout();
  }
  kit_files_ = i, kit_play_ = play, kit_ask_ = m;
  files_pm_ = from_files_ = false, status_.clear();
  if (SDL_getenv("MW3_LAUNCHER_GO")) auto_go_ = true;  // checks: its find's INSTALL pressed
  std::printf("mw3: launcher: games: %s: its game files, on the launcher's screens\n", upgrades::update::game_info(upgrades::update::game_at(i)).key);
  screen_ = kSetupScreen, layout();
}

// A pick looked at by the game ("": its search, then what that found).
void Launcher::kit_look(const std::string& pick) {
  pending_source_ = pick;
  auto answer = kit_found_ = std::make_shared<rekit::SetupMessage>();
  work_ = std::make_unique<Work>();
  work_->kind = Work::kKitLook;
  const std::string bin = game_bin(kit_files_);
  work_->start("LOOKING AT YOUR GAME", [bin, pick, answer](const own::ProgressFn& f) {
    auto step = [&f](const rekit::SetupMessage& m) { return f(own::Progress{m.stage, m.fraction, m.message}); };
    std::string at = pick;
    own::Result r;
    bool said = true;
    if (at.empty()) {
      said = sdlfe::games::kit_setup(bin, {"--search"}, 120, step, answer.get());
      at = answer->path;
    }
    if (said && !at.empty()) said = sdlfe::games::kit_setup(bin, {"--look", at}, 0, step, answer.get());
    r.ok = said && answer->ok && !at.empty(), r.cancelled = answer->cancelled, r.summary = r.ok ? answer->words : answer->summary;
    if (r.ok && answer->path.empty()) answer->path = at;
    if (r.ok && !answer->in_place) answer->where = at;  // what the install is given again: the pick itself
    return r;
  });
  screen_ = kWorking, layout();
}

void Launcher::kit_looked(const own::Result& r) {
  if (!r.ok) {
    const bool searched = pending_source_.empty() && r.summary.empty();  // nothing on this computer: the picker
    status_ = searched ? "Not found on this computer. Pick it: " + kit_ask_.pick_words + "." : r.summary;
    status_good_ = searched || r.cancelled;
    screen_ = kSetupScreen, layout();
    if (searched) ask_file(kPoint, kGameFiles, kGameFilesCount);
    return;
  }
  if (kit_found_->in_place) return kit_ready(r.summary);  // the game's own copy, set up already
  found_name_ = fs::path(kit_found_->path).filename().string();
  if (found_name_.empty()) found_name_ = kit_found_->path;
  found_sub_ = "Click to pick other files";
  std::printf("mw3: launcher: found plate: %s (%s)\n", found_name_.c_str(), r.summary.c_str());
  files_words_ = r.summary;
  files_into_ = games_place_[kit_files_];  // where its DOWNLOAD put the program, if it was asked
  files_confirm_text();
  confirm_go_ = 1;
  if (const char* to = SDL_getenv("MW3_LAUNCHER_FILES_TO"); to && *to) picked(kInstallTo, to);  // checks: CHANGE FOLDER's pick
  screen_ = kConfirm;
  if (auto_go_) {
    auto_go_ = false;
    return activate(kGo);
  }
  layout();
}

void Launcher::kit_install() {
  std::vector<std::string> args{"--install", kit_found_->where};
  if (!files_into_.empty()) args.push_back("--folder"), args.push_back(files_into_);
  auto answer = std::make_shared<rekit::SetupMessage>();
  work_ = std::make_unique<Work>();
  work_->kind = Work::kKitInstall;
  const std::string bin = game_bin(kit_files_);
  std::string name = upgrades::update::game_info(upgrades::update::game_at(kit_files_)).name;
  for (char& ch : name) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  work_->start("INSTALLING " + name, [bin, args, answer](const own::ProgressFn& f) {
    own::Result r;
    const bool said = sdlfe::games::kit_setup(
        bin, args, 0, [&f](const rekit::SetupMessage& m) { return f(own::Progress{m.stage, m.fraction, m.message}); }, answer.get());
    r.ok = said && answer->ok, r.cancelled = answer->cancelled, r.summary = answer->summary;
    return r;
  });
  screen_ = kWorking, layout();
}

// Its game files are in: its status read again, then, from PLAY, the game.
void Launcher::kit_ready(const std::string& summary) {
  const int i = kit_files_;
  const bool play = kit_play_;
  kit_refresh(i);
  status_ = summary, status_good_ = true;
  screen_ = kMain, layout();
  if (play && kit_status_[i].state == rekit::Status::kReady) {  // once through its setup: never asked twice
    if (i == 2) return mw1_needs().empty() ? run_mw1_remake() : void();
    run_kit(i, true);
  }
}

// MW1: what MechWarrior 1 still needs before it can start, in the player's words ("" when nothing).
// Its needs as the launcher sees them, the copies it passes (--with): never the Remake's own missing, which says only
// what its MechWarrior (1989) lacks (b5).
void Launcher::mw1_others(bool* mw3, bool* pm) const {
  *mw3 = install_.ok && !install_.dir.empty();
  *pm = !pm_dir_.empty();
}

std::string Launcher::mw1_needs() const {
  bool mw3 = false, pm = false;
  mw1_others(&mw3, &pm);
  if (mw3 && pm) return "";
  return std::string("needs ") + (!mw3 && !pm ? "MechWarrior 3 and Pirate's Moon: find them" : !mw3 ? "MechWarrior 3: find it" : "Pirate's Moon: find it") +
         " in GAME FILES";
}

// Pirate's Moon checked before it starts (setup::check_install, docs/launcher.md,
// Pirate's Moon): what stops it is said instead of starting; notes not shown
// before (pm_notes=, their CRC) once on BEFORE YOU PLAY, whose OK starts it.
// Its settings folder as mw3pm finds its own; nothing is written to it here.
void Launcher::start_pm() {
  own::Result r;
  const std::vector<std::string> notes = pm_notes(&r);
  std::printf("mw3: launcher: %s\n", r.summary.c_str());
  for (const std::string& w : notes) std::printf("mw3: launcher: note: %s\n", w.c_str());
  if (!r.ok) {
    status_ = r.summary, status_good_ = false;
    screen_ = kMain, layout();
    return;
  }
  files_pm_ = true;  // its ADD FROM MY DISC on the notes
  pm_after_notes_ = true;
  if (notes_once(notes, &o_.cfg_pm_notes, "pm_notes")) return;
  pm_after_notes_ = false;
  start_game(true);
}

// A game's notes before a start, new ones (their CRC not `crc_kept`, saved as `key`=) shown once on BEFORE YOU PLAY,
// whose OK starts the game (pm_after_notes_ says which); true when shown.
bool Launcher::notes_once(const std::vector<std::string>& notes, std::string* crc_kept, const char* key) {
  std::string all;
  for (const std::string& n : notes) all += n + "\n";
  char crc[16];
  std::snprintf(crc, sizeof crc, "%08x", own::crc32(0, reinterpret_cast<const uint8_t*>(all.data()), all.size()));
  if (notes.empty() || *crc_kept == crc) return false;
  *crc_kept = crc;
  own::save_config_value(o_.write_dir, key, crc);
  notes_ = notes;
  std::printf("mw3: launcher: %s's notes shown\n", pm_after_notes_ ? "Pirate's Moon" : "MechWarrior 3");
  play_after_notes_ = true, status_.clear();
  screen_ = kNotes, layout();
  if (auto_ok_) {  // checks (MW3_LAUNCHER_PICK, --launcher-pm, --launcher-play): the notes' OK pressed
    auto_ok_ = false;
    activate(kNotesOk);
  }
  return true;
}

// MechWarrior 3 checked before PLAY as Pirate's Moon is (start_pm, setup::start_check): its new notes once on
// BEFORE YOU PLAY, whose OK plays. What stops it starting is left to mw3, which says it in its own box as the
// original does ("Cannot read mech3Msg.dll from the game folder."). False: not started now.
bool Launcher::mw3_ready() {
  if (!install_.ok) return true;  // the find screen's own path
  status_refresh(0);
  if (status_for(0, install_.dir)) {  // b2: its status's start check (notes_once of its notes); what stops it, mw3's
    const rekit::Status& s = kit_status_[0];
    const std::vector<std::string> warnings(s.notes.begin(), s.notes.begin() + s.notes_once);
    std::printf("mw3: launcher: %s\n", own::start_found_words(install_.dir, false).c_str());
    for (const std::string& w : warnings) std::printf("mw3: launcher: note: %s\n", w.c_str());
    files_pm_ = pm_after_notes_ = false;
    return !notes_once(warnings, &o_.cfg_mw3_notes, "mw3_notes");
  }
  return true;  // b6: an older program checks itself at its start
}

// Pirate's Moon's notes before a start: the check's, and no music as
// MechWarrior 3's (launcher_notes); `check` gets the check.
std::vector<std::string> Launcher::pm_notes(own::Result* check) {
  pm_refresh();
  if (status_for(1, pm_dir_)) {  // its status's notes (b2), the music's folder fitted as below
    const rekit::Status& s = kit_status_[1];
    own::Result r;
    r.ok = s.state == rekit::Status::kReady, r.install_dir = pm_dir_;
    r.summary = r.ok ? own::found_words("", pm_dir_, true) : s.missing.empty() ? std::string() : s.missing.front();
    const std::string dir = (fs::path(pm_media()) / "music").make_preferred().string();
    std::vector<std::string> out;
    for (std::string n : s.notes) {
      if (const size_t at = n.find(dir); at != std::string::npos) n.replace(at, dir.size(), text_.fit(dir, 14, 440));
      out.push_back(n);
    }
    if (check) *check = std::move(r);
    return out;
  }
  own::Result r;  // b6: an older program, not checked here, no notes
  r.ok = !pm_dir_.empty(), r.install_dir = pm_dir_, r.summary = own::found_words("", pm_dir_, true);
  if (check) *check = std::move(r);
  return {};
}

// MechWarrior 3 as its own program (game_bin): the install and the options
// passed on, the launcher kept on screen until the game's first frame
// (mw3 --ready-file) so the switch shows no empty moment; then it stays open
// beside the game (2026-11-26; from 2026-11-01 it closed there, the game going
// on alone, as a game's own setup still does).
void Launcher::start_game(bool pm) {
  std::error_code ec;
  const int gi = pm ? 1 : 0;
  if (running(gi)) return;
  Run run;
  run.name = pm ? "Pirate's Moon" : "MechWarrior 3";
  run.ready_file = (fs::temp_directory_path(ec) / ("mw3-ready-" + std::to_string(SDL_GetTicksNS()))).string();
  fs::remove(run.ready_file, ec);
  // PM has its own install, settings and saves: nothing of MW3's goes to it.
  std::vector<std::string> args;
  // b6 (decided 2026-11-26): a game that serves its setup finds the copy its status shows on its own (mw3_status_find),
  // so PLAY passes none; an older program is handed the copy, as before.
  const bool served = status_for(pm ? 1 : 0);
  if (pm) {
    args = {o_.pm_bin};
    if (!served) args.insert(args.end(), {"--install", pm_dir_});
    args.push_back("--no-launcher");
    // Its own folder as this launcher knows it (MW3_PM_WRITE_DIR, a MY DATA FOLDER move, else the usual one or its
    // pointer's): mw3pm left alone fell back to the usual folder, a named one ignored.
    if (const std::string wd = pm_wd(); !wd.empty()) args.push_back("--write-dir"), args.push_back(wd);
    args.insert(args.end(), o_.pm_args.begin(), o_.pm_args.end());
  } else {
    args = {o_.game_bin};
    if (!served) args.push_back(install_.dir);
    args.insert(args.end(), o_.game_args.begin(), o_.game_args.end());
  }
  args.push_back("--ready-file"), args.push_back(run.ready_file);
  // W15: the other game's data folder and install, where a mod imported for it is added.
  const std::string other_wd = pm ? o_.write_dir : pm_wd(), other_install = pm ? (install_.ok ? install_.dir : "") : pm_dir_;
  if (!other_wd.empty() && !other_install.empty())
    args.insert(args.end(), {"--other-write-dir", other_wd, "--other-install", other_install});
  std::vector<const char*> argv;
  std::string line;
  for (const std::string& a : args) argv.push_back(a.c_str()), line += (line.empty() ? "" : " ") + a;
  argv.push_back(nullptr);
  if (!pm && !served) own::save_install(o_.write_dir, install_.dir);
  run.click = SDL_GetTicksNS();
  // Its own stdio, none of ours: the game writes mw3.log itself, and our
  // log's pipe in its stdout would hold this program's exit until the game's.
  const SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
  // The game's box after a crash would page from a mount going away: this
  // program's instead (watch_game), which stays for an AppImage.
  // A game the launcher installed runs from its own folder: nothing to wait for (sdl_games.h).
  run.mount = sdlfe::games::in_own_mount(args.front());
  SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "MW3_LAUNCHER_WATCHES", run.mount ? "1" : "", true);
  run.p = SDL_CreateProcessWithProperties(props);
  SDL_DestroyProperties(props);
  if (!run.p) {
    status_ = run.name + " did not start: " + SDL_GetError(), status_good_ = false;
    std::printf("mw3launcher: %s (%s)\n", status_.c_str(), line.c_str());
    layout();
    return;
  }
  // FIX (A), playtest W4 (sdl_focus.h): the game's window may come to the front over ours (Windows gives the
  // foreground only to a process the foreground one allows), so it starts with the keyboard and the mouse.
  const bool front = allow_foreground(run.p);
  std::printf("mw3launcher: started %s%s\n", line.c_str(), front ? " (it may take the foreground)" : "");
  status_ = "Starting " + run.name + "...", status_good_ = true;
  runs_[gi] = std::move(run);
  layout();  // its PLAY says RUNNING
}

bool Launcher::any_running() const {
  for (const Run& r : runs_)
    if (r.p) return true;
  return false;
}

bool Launcher::any_starting() const {
  for (const Run& r : runs_)
    if (r.p && !r.ready) return true;
  return false;
}

// The launcher to the front once the last game running exits; never over a game still up.
void Launcher::restore_window() {
  if (!win_ || o_.hidden || any_running()) return;
  if (SDL_GetWindowFlags(win_) & SDL_WINDOW_MINIMIZED) SDL_RestoreWindow(win_);  // the player's own minimize
  SDL_RaiseWindow(win_);
}

// FIX (A): an AppImage unmounts when its first process (this) ends, with the game still paging its code and libraries
// from there: it would die when it reaches a page not yet read, after the intro. So the launcher waits for such a
// game before it ends, hidden, and says if the game crashes (docs/linux.md, Packaging).
void Launcher::mount_wait(Run& r) {
  int code = 0;
  if (win_) SDL_HideWindow(win_);
  std::fflush(stdout);  // a long wait: the log has this far
  SDL_WaitProcess(r.p, true, &code);
  std::printf("mw3launcher: %s exited (%d) after %.1f s\n", r.name.c_str(), code, static_cast<double>(SDL_GetTicksNS() - r.click) / 1e9);
  std::fflush(stdout);
  if (code < 0) {
    const std::string text = r.name + " crashed (signal " + std::to_string(-code) + ").\n\nThe log is " + o_.write_dir +
                             "/mw3.log\nPlease include it if you report this.";
    // Said in the log always; the box only for a player (never a hidden run's: pointer_hands_off.h).
    std::printf("mw3launcher: %s\n", text.c_str());
    if (!o_.hidden && !platform::pointer_hands_off()) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, r.name.c_str(), text.c_str(), nullptr);
  }
  SDL_DestroyProcess(r.p);
  r = Run();
}

// A game's end: said on the status line, its PLAY back, an UPDATE pressed meanwhile begun, the window back.
void Launcher::game_exited(int i, int code) {
  Run& r = runs_[i];
  std::error_code ec;
  const double secs = static_cast<double>(SDL_GetTicksNS() - r.click) / 1e9;
  const std::string name = r.name;
  const bool was_ready = r.ready, kit = r.kit, mount = r.mount;
  if (!r.ready_file.empty()) fs::remove(r.ready_file, ec), fs::remove(r.ready_file + ".status", ec);
  SDL_DestroyProcess(r.p);
  r = Run();
  // The game started itself again as a new process (Windows, into or out of a MODS campaign: sdl_restart.h). The
  // game goes on: a game's own setup ends as it does at a first frame; the launcher stays, no longer seeing it.
  if (code == kStartedAgainExit && !kit) {
    if (own_setup()) {
      std::printf("mw3launcher: %s started itself again after %.1f s (the game goes on): the launcher closes\n", name.c_str(), secs);
      done_ = true;
      return;
    }
    std::printf("mw3launcher: %s started itself again after %.1f s (the game goes on, no longer watched)\n", name.c_str(), secs);
    status_ = name + " started itself again: it runs on, but its panel can't follow it.", status_good_ = true;
    if (o_.quit_after_game && !any_running()) done_ = true;
    return layout();
  }
  if (kit) std::printf("mw3: launcher: %s exited (%d)\n", name.c_str(), code);
  else std::printf("mw3launcher: %s exited (%d) after %.1f s\n", name.c_str(), code, secs);
  if (code < 0 && mount && !o_.hidden && !platform::pointer_hands_off()) {  // the game's own box is off while we watch
    const std::string text = name + " crashed (signal " + std::to_string(-code) + ").\n\nThe log is " + o_.write_dir +
                             "/mw3.log\nPlease include it if you report this.";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, name.c_str(), text.c_str(), nullptr);
  }
  if (kit) status_ = code == 0 ? name + " closed." : name + " closed with an error (" + std::to_string(code) + "): see its log.";
  else status_ = code == 0 ? (was_ready ? name + " closed." : std::string()) : name + " closed with an error (" + std::to_string(code) + "): see mw3.log.";
  status_good_ = code == 0;
  restore_window();
  if (kit) kit_refresh(i);
  if (o_.quit_after_game && !any_running()) done_ = true;
  layout();  // an UPDATE pressed meanwhile: begun by the run loop, once nothing else is being done
}

// Each pass, each game running: an end, and MechWarrior 3's or Pirate's Moon's first frame (the launcher stays open
// beside it; a game's own setup closes); a game that exits first (an error) or shows no frame in 20 s says so on the
// status line.
void Launcher::watch_game() {
  for (int i = 0; i < upgrades::update::game_count() && !done_; ++i) {
    Run& r = runs_[i];
    if (!r.p) continue;
    std::error_code ec;
    const double ms = static_cast<double>(SDL_GetTicksNS() - r.click) / 1e6;
    int code = 0;
    if (SDL_WaitProcess(r.p, false, &code)) {
      game_exited(i, code);
      continue;
    }
    if (r.ready) continue;
    if (fs::exists(r.ready_file, ec)) {
      std::printf("mw3launcher: the game's first frame %.0f ms after %s was started: %s\n", ms, r.name.c_str(),
                  own_setup() ? "the launcher closes" : "the launcher stays open");
      fs::remove(r.ready_file, ec);
      fs::remove(r.ready_file + ".status", ec);
      r.ready = true;
      if (own_setup()) {
        if (r.mount) mount_wait(r);  // FIX (A): the AppImage's mount stays while the game pages from it
        else SDL_DestroyProcess(r.p), r = Run();  // let go: the game runs on
        done_ = true;
        return;
      }
      status_ = r.name + " is running.", status_good_ = true;
      layout();
      continue;
    }
    // What the game says it is doing before its first frame (its first start's movie import, sdl_child.h), read four
    // times a second: on the status line instead of the wait's.
    if (ms - r.read_ms >= 250.0) {
      r.read_ms = ms;
      std::ifstream in(r.ready_file + ".status");
      std::string line;
      std::getline(in, line);
      if (!line.empty() && line != r.said) {
        r.said = line;
        std::printf("mw3launcher: the game says: %s\n", line.c_str());
        status_ = line, status_good_ = true;
        layout();
      }
    }
    if (!r.slow && r.said.empty() && ms > ready_wait_ms()) {
      r.slow = true;
      std::printf("mw3launcher: no first frame from the game after %.0f s: the launcher stays\n", ready_wait_ms() / 1000.0);
      status_ = r.name + " has not shown a picture yet: still waiting (see mw3.log).", status_good_ = false;
      layout();
    }
  }
}

void Launcher::layout() {
  if (screen_ == kMain || screen_ == kGamePage) kit_files_ = -1;
  if (screen_ == kMain) files_page_ = -1;
  else update_box_ = false;
  buttons_.clear();
  ui_stale_ = true;
  auto add = [&](int id, const char* group, std::string label, std::string sub = {}, bool on = true) -> Button& {
    Button b;
    b.id = id, b.group = group, b.label = std::move(label), b.sub = std::move(sub), b.enabled = on;
    buttons_.push_back(std::move(b));
    return buttons_.back();
  };
  // A question's three: the way on in the middle, bigger and amber as a panel's PLAY, focused from the start; CHANGE
  // FOLDER and BACK alike on either side (2026-11-24).
  int first_focus = -1;
  auto add_go = [&](int go, const char* label) {
    add(kInstallTo, "go", "CHANGE FOLDER");
    add(go, "go", label).kind = "amber";
    first_focus = static_cast<int>(buttons_.size()) - 1;
    add(kBack, "go", "BACK");
  };
  if (sounds_on_ && !sounds_tried_) {  // the first screen's
    sounds_tried_ = true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) sounds_.load(assets_dir());
  }
  switch (screen_) {
    case kMain: {
      // The cards (2026-11-20, docs/launcher.md, The cards), one a family since 2026-11-22 (the mockups'
      // A): its badge, its name, its games as tabs (MECHWARRIOR 3 | PIRATE'S MOON; ORIGINAL | REMAKE), then the
      // selected game's status and its button along the bottom, PLAY split with its UPDATE when one is ready. The
      // badge follows the tab. Every game's line is logged once whichever tab shows (the checks read them).
      panels_.clear();
      using GC = upgrades::update::GameCheck;
      auto can_update = [&](int i) { return games_on_ && !beside_[i] && !game_bin(i).empty() && checks_[i].kind == GC::kNewer; };
      struct Panel {  // a game's part of its card
        std::vector<std::string> lines;
        std::vector<size_t> keep;
        int id = 0;
        std::string label = "PLAY", name;
        bool on = true, soon = false;  // soon: not out yet (a card greyed when all its games are)
        int upd = -1;                  // the game whose UPDATE splits the button
      };
      Panel pn[upgrades::update::kMaxGames];
      auto fix_keep = [](Panel& p) { p.keep.resize(p.lines.size(), 0); };
      auto button = [](int i) { return upgrades::update::catalog_game(upgrades::update::game_at(i)).button.c_str(); };
      auto update_line = [&](Panel& p, int i) {  // the split's version, on the card (the segment says UPDATE)
        if (!can_update(i)) return;
        p.lines.push_back("Update " + upgrades::update::game_version(checks_[i].release.tag) + " is ready");
        fix_keep(p);
      };
      // Games on demand: a game not installed has DOWNLOAD where PLAY goes (2026-11-23, was INSTALL). One whose
      // repo has no release yet says COMING SOON and turns to DOWNLOAD by itself once its first release is out.
      auto install = [&](Panel& p, int i, const char* name) {
        p.id = kGameInstall + i, p.name = name, p.upd = -1;
        if (checks_[i].kind == GC::kNoRelease) {
          p.lines = {"Coming soon"}, p.label = "COMING SOON", p.on = false, p.soon = true;
        } else {
          p.lines = {"Not installed"};
          const std::string w = game_words(i);  // a line, or two at its '\n'
          const size_t nl = w.find('\n');
          p.lines.push_back(w.substr(0, nl));
          if (nl != std::string::npos) p.lines.push_back(w.substr(nl + 1));
          p.label = "DOWNLOAD", p.on = checks_[i].kind == GC::kNewer && !work_;
        }
        p.keep.assign(p.lines.size(), 0);
      };
      auto later = [&](Panel& p, int id, const char* name) {  // not in this package, and no games on demand
        p.lines = {"Coming later"}, p.keep = {0}, p.id = id, p.label = "COMING LATER", p.name = name, p.on = false, p.soon = true;
      };
      // A game on the kit's contract, installed: its own --status in words.
      auto kit = [&](Panel& p, int i, const char* name) {
        const rekit::Status& s = kit_status_[i];
        const std::string ver = has_installed_[i] ? upgrades::update::game_version(installed_[i].tag) : s.version;
        const std::string dot = " \xc2\xb7 ";
        if (!kit_known_[i]) p.lines = {"Installed" + (ver.empty() ? std::string() : dot + ver), "PLAY sets it up"};
        else if (s.state == rekit::Status::kReady && !kit_set_up(i)) p.lines = {"Game files found, not set up yet", "PLAY sets them up"};
        else if (s.state == rekit::Status::kReady) p.lines = {"Ready" + (ver.empty() ? std::string() : dot + ver), s.where};
        else if (s.state == rekit::Status::kMissing) p.lines = {"Game files not found yet", "PLAY finds them"};
        else p.lines = {"Game files not ready", !s.note.empty() ? s.note : !s.missing.empty() ? "Missing: " + s.missing.front() : "PLAY sets them up"};
        if (kit_known_[i] && kit_set_up(i) && !s.note.empty()) p.lines.push_back(s.note);
        fix_keep(p);
        update_line(p, i);
        p.id = kKitPlay + i, p.name = name, p.on = !work_, p.upd = i;
      };
      const std::string ready = "Ready, " + official_words(install_.version, mw3_program_, false) + ": " +
                                versions_words(install_.version, mw3_program_);
      const std::string sub = install_.ok ? ready + install_.dir : "Not found yet: click to find your game";
      {  // MechWarrior 3
        Panel& p = pn[0];
        if (games_on_ && o_.game_bin.empty()) {
          install(p, 0, button(0));
        } else {
          if (install_.ok) p.lines = status_lines(ready, install_.dir);
          else p.lines = {"Not found yet:", "PLAY finds your game"};
          fix_keep(p);
          update_line(p, 0);
          p.id = kMw3, p.name = button(0), p.upd = 0;
        }
      }
      if (!entries_logged_)
        std::printf("mw3: launcher: entry MECHWARRIOR 3: %s\n", games_on_ && o_.game_bin.empty() ? "Not installed" : sub.c_str());
      {  // Pirate's Moon: its program beside this one (a package with it) or installed
        Panel& p = pn[1];
        if (o_.pm_bin.empty() && games_on_) {
          install(p, 1, button(1));
          if (!entries_logged_) std::printf("mw3: launcher: entry PIRATE'S MOON: Not installed\n");
        } else if (o_.pm_bin.empty()) {
          later(p, kPm, button(1));
        } else {
          const std::string pm_ready = "Ready, " + official_words(pm_data_, pm_program_, true) + ": " +
                                       versions_words(pm_data_, pm_program_);
          if (pm_dir_.empty()) p.lines = {"Not found yet:", "PLAY finds your game"};
          else p.lines = status_lines(pm_ready, pm_dir_);
          fix_keep(p);
          if (!entries_logged_)
            std::printf("mw3: launcher: entry PIRATE'S MOON: %s\n",
                        (pm_dir_.empty() ? "Not found yet: click to find your game" : pm_ready + pm_dir_).c_str());
          update_line(p, 1);
          p.id = kPm, p.name = button(1), p.upd = 1;
        }
      }
      if (!entries_logged_) std::printf("mw3: launcher: footer %s\n", package_words().c_str());
      entries_logged_ = true;  // once a run: the lines as first shown
      // MechWarrior 1 Remake: it needs MW3 and Pirate's Moon first (2026-11-16), and the 1989 game's folder.
      const std::string needs = mw1_remake_ ? mw1_needs() : std::string();
      {
        Panel& p = pn[2];
        if (games_on_ && !mw1_remake_) {
          install(p, 2, button(2));
          if (!p.soon) p.lines.push_back("Needs MW3 and Pirate's Moon to play"), fix_keep(p);
        } else if (!mw1_remake_) {
          later(p, kMw1, button(2));
        } else {
          p.lines.push_back("Remake " + (mw1_program_.empty() ? std::string("ready") : upgrades::update::game_version(mw1_program_)) +
                            " \xc2\xb7 " + (mw1_game_.empty() ? "1989 game not found yet" : "1989 game found"));
          if (!needs.empty()) {  // short on the card (it cut mid-word, MW1 dev 2's 1.2.0 pass); whole in the status line
            bool mw3 = false, pm = false;
            mw1_others(&mw3, &pm);
            p.lines.push_back(std::string("Needs ") + (!mw3 && !pm ? "MW3 and Pirate's Moon" : !mw3 ? "MechWarrior 3" : "Pirate's Moon"));
          } else if (!mw1_game_.empty()) {
            p.lines.push_back(mw1_game_);
          } else if (mw1_served()) {
            p.lines.push_back("PLAY finds your MechWarrior (1989)");
          } else if (!kit_known_[2]) {  // never "update" for a status unknown (mech1-remake-re #2)
            p.lines.push_back(checking(2) ? "Checking the Remake..." : "It did not answer: PLAY asks again");
          } else {
            p.lines.push_back("Update it to find the 1989 game");  // an older Remake: the finder was the launcher's (b5)
          }
          fix_keep(p);
          update_line(p, 2);
          p.id = kMw1, p.name = button(2), p.on = needs.empty(), p.upd = 2;
        }
      }
      {  // MW1's ORIGINAL: MechWarrior 1 Rebuild (mech1-re), a kit game (the raw port's own entry went in b5)
        Panel& p = pn[3];
        if (!game_bin(3).empty()) kit(p, 3, button(3));
        else if (games_on_) install(p, 3, button(3));
        else later(p, kKitPlay + 3, button(3));
      }
      for (int i = 4; i < upgrades::update::game_count(); ++i) {  // Inception's two (the hawk, 2026-11-24), then a fetched catalog's new ones
        const char* name = button(i);
        Panel& p = pn[i];
        if (!game_bin(i).empty()) kit(p, i, name);
        else if (games_on_) install(p, i, name);
        else later(p, kKitPlay + i, name);
      }
      if (!mw1_logged_ && !checking(2)) {  // the lines the round entries had, for the log and the checks (once known)
        if (games_on_ && !mw1_remake_) {
          std::printf("mw3: launcher: entry MECHWARRIOR 1: Not installed\n");
        } else {
          const std::string line = !mw1_remake_            ? std::string("Coming later")
                                   : !needs.empty()        ? "Not ready: " + needs
                                   : mw1_game_.empty() && mw1_served() ? "Not found yet: click to find your MechWarrior (1989)"
                                   : mw1_game_.empty() && !kit_known_[2] ? "Did not answer: PLAY asks again"
                                   : mw1_game_.empty()     ? "Not set up: update MechWarrior 1 Remake to find your MechWarrior (1989)"
                                   : mw1_program_.empty()  ? "Ready: " + mw1_game_
                                                           : "Ready, " + mw1_program_ + ": " + mw1_game_;
          std::printf("mw3: launcher: entry MECHWARRIOR 1: %s\n", line.c_str());
        }
        auto words = [&](int i) {
          std::string w;
          for (const std::string& l : pn[i].lines) w += (w.empty() ? "" : w.back() == ':' ? " " : ": ") + l;
          return w;
        };
        std::printf("mw3: launcher: entry MECHWARRIOR 1 ORIGINAL: %s\n", pn[3].soon && !games_on_ ? "coming later" : words(3).c_str());
        std::printf("mw3: launcher: entry INCEPTION ORIGINAL: %s\n", words(4).c_str());
        std::printf("mw3: launcher: entry INCEPTION REMAKE: %s\n", words(5).c_str());
        for (int i = 6; i < upgrades::update::game_count(); ++i) std::printf("mw3: launcher: entry %s: %s\n", button(i), words(i).c_str());
        mw1_logged_ = true;
      }
      // The panels (2026-11-24, the download page's look; docs/launcher.md, The panels): one a game, top to bottom
      // as the page lists them, in a list that scrolls. Inception's remake gets its own once its first release is
      // out (or it is installed); until then Inception is one panel.
      // In the catalog's order (games.json, or a fetched one over it): its title and blurb. A coming_soon one with
      // panel_before_release false (Inception's remake) only once a release is found or it is installed (never on a
      // failed check); a removed one only while it is installed, which keeps playing (b7).
      struct Row {
        int game;
        const upgrades::update::CatalogGame* entry;
      };
      std::vector<Row> rows;
      for (const upgrades::update::CatalogGame& e : upgrades::update::active_catalog().games) {
        upgrades::update::Game g;
        if (!upgrades::update::game_of_key(e.key, &g)) continue;  // past kMaxGames
        const int i = static_cast<int>(g);
        if (e.state == upgrades::update::CatalogGame::kRemoved && game_bin(i).empty()) continue;
        if (e.state == upgrades::update::CatalogGame::kComingSoon && !e.panel_before_release &&
            (pn[i].soon || !(checks_[i].kind == GC::kNewer || checks_[i].kind == GC::kUpToDate || !game_bin(i).empty())))
          continue;
        rows.push_back({i, &e});
      }
      for (size_t k = 0; k < rows.size(); ++k) {
        const Panel& p = pn[rows[k].game];
        UiPanel c;
        const std::string group = "panel" + std::to_string(k);
        c.buttons = group, c.title = rows[k].entry->title, c.on = !p.soon;
        c.badge = rows[k].entry->badge + "?76";
        // Three small lines under the name: the status's first line (with its second when both fit) in amber, the rest
        // of it, then the game's line only when it fits whole (2026-11-24: a cut one could not be read anywhere).
        size_t i = 0;
        if (!p.lines.empty()) {
          std::string first = p.lines[0];
          if (p.lines.size() > 1 && body_.width(first + " \xc2\xb7 " + p.lines[1], 12) <= kAboutWidth) first += " \xc2\xb7 " + p.lines[1], i = 1;
          c.lines.push_back({first, c.on ? "amber" : "soft", p.keep.empty() ? 0 : static_cast<int>(p.keep[0])});
          for (++i; i < p.lines.size() && c.lines.size() < 3; ++i)
            c.lines.push_back({p.lines[i], "dim", i < p.keep.size() ? static_cast<int>(p.keep[i]) : 0});
        }
        const std::vector<std::string> words = body_.wrap(rows[k].entry->description, 12, kAboutWidth);
        if (c.lines.size() + words.size() <= 3)
          for (const std::string& w : words) c.lines.push_back({w, "soft"});
        panels_.push_back(std::move(c));
        // Its button at the right; `upd`, a game with a newer release, splits it: PLAY and, at its end, that game's
        // UPDATE.
        // A game running (2026-11-26): RUNNING, grayed, until it exits; its UPDATE waits for that (pressed, it is
        // begun then).
        const int gi = rows[k].game;
        const bool split = p.upd >= 0 && can_update(p.upd), run = running(gi);
        {
          Button& b = add(p.id, group.c_str(), run ? "RUNNING" : p.label, "", p.on && !run);
          b.kind = split ? "amber split" : "amber", b.name = p.name;
        }
        if (split) {
          Button& b = add(kGameUpdate + p.upd, group.c_str(), "UPDATE TO " + upgrades::update::game_version(checks_[p.upd].release.tag), "",
                          !work_ && !update_after_[p.upd]);
          b.kind = "seg", b.name = p.name + " UPDATE";
        }
        // Its gear (2026-11-24): the game's page (where it is, link an install, uninstall), a square left of the
        // button; after it in the focus order, so a pad's first press is still PLAY.
        // Not while its repo is still being asked about a game not here (it may turn out COMING SOON).
        if (games_on_ && !p.soon && (!game_bin(gi).empty() || checks_[gi].kind != GC::kAsking)) {
          Button& b = add(kGear + gi, group.c_str(), "", "", !work_);
          b.kind = "gear", b.name = p.name + " SETTINGS";
        }
      }
      // UPDATES (docs/launcher.md, Updates); QUIT where a window has no X (Escape too). GAME FILES and INSTALL FROM A
      // DOWNLOADED FILE are on a game's page (2026-11-24).
      add(kUpdates, "row", rechecking_ ? "CHECKING..." : "CHECK FOR UPDATES", "", !work_ && !rechecking_).kind = "flat updates";
      buttons_.back().name = "CHECK FOR UPDATES";
      add(kQuit, "row", "QUIT").kind = "flat";
      // A newer launcher found by the start's check: an amber UPDATE AVAILABLE <v> in the header's top right, beside
      // the mark, as Discord's and Chrome's (2026-11-23: the footer's button was too small to see). After QUIT
      // for the keys and the pad; it opens the UPDATE screen.
      if (self_update_) {
        add(kSelfUpdate, "pill", "UPDATE AVAILABLE " + upgrades::update::plain_version(self_update_->release.tag), "", !work_).kind =
            "amber";  // logged and focused by its label, the version in it
      }
      if (update_box_) {  // over the games, which wait
        for (Button& b : buttons_) b.enabled = false;
        const bool games = !game_updates().empty();
        if (games) add(kUpdateAll, "box", "UPDATE ALL").kind = "amber", first_focus = static_cast<int>(buttons_.size()) - 1;
        add(kUpdateLauncher, "box", "UPDATE LAUNCHER").kind = games ? "flat" : "amber";
        if (!games) first_focus = static_cast<int>(buttons_.size()) - 1;
        add(kUpdateNotNow, "box", "NOT NOW").kind = "flat";
      }
      break;
    }
    case kSetupScreen: {
      // Find the game (2026-11-02: one option): the search first (an
      // installed copy or a disc in a drive used at once), else one picker:
      // an image or a download, or any file of an installed copy (its
      // folder; pick_source). The 1.2 patch is built in (patch12.h).
      // PICK A FILE and PICK A FOLDER (2026-11-24) go to a picker whatever is on this computer. A kit game whose
      // files are in: those are no answer, so its search (which would find them again) is not offered.
      if (kit_files_ >= 0 && kit_set_up(kit_files_)) {
        add(kPickFile, "find", "PICK A FILE", kit_ask_.find_sub);
        add(kPickFolder, "pick", "PICK A FOLDER");
      } else {
        add(kFind, "find", "FIND THE GAME",
            kit_files_ >= 0 ? kit_ask_.find_sub : "A disc image, a download (.zip .iso), or Mech3.exe in an installed copy");
        if (kit_files_ < 0 && from_files_ && (files_pm_ || !o_.write_dir_fixed)) {
          add(kPickFile, "pick", "PICK A FILE");
          add(kPickFolder, "pick", "PICK A FOLDER");
          add(kData, "pick", "MY DATA FOLDER");
        } else {
          add(kPickFile, "pick", "PICK A FILE");
          add(kPickFolder, "pick", "PICK A FOLDER");
        }
      }
      if (kit_files_ >= 0) {
      } else if (files_pm_ ? !pm_dir_.empty() && (pm_lacks_music_ || pm_lacks_sounds_)
                    : install_.ok && (lacks_music_ || lacks_movies_))  // what the install in use lacks, from the disc
        add(kAddDisc, "disc", "ADD FROM MY DISC", kAddDiscSub);
      add(kBack, "foot", !o_.setup_only ? "BACK" : o_.choose_again && have_files() ? "PLAY" : "QUIT");
      break;
    }
    case kCopies: {  // GAME FILES' list (2026-11-24): a row a copy, the one in use first, then PICK A FILE and BACK
      const int n = std::min<int>(static_cast<int>(copies_.size()), kCopyRows);
      for (int i = 0; i < n; ++i) {
        const Copy& c = copies_[i];
        std::string sub = (files_pm_ ? "Pirate's Moon " : "MechWarrior 3 ") + c.version;
        sub += c.missing.empty() ? " \xc2\xb7 nothing missing" : " \xc2\xb7 missing: ";
        for (size_t k = 0; k < c.missing.size(); ++k) sub += (k ? ", " : "") + c.missing[k];
        if (c.in_use) sub = "IN USE \xc2\xb7 " + sub;
        add(kCopy + i, "copies", c.path, sub, !c.in_use).name = c.in_use ? "IN USE" : "COPY " + std::to_string(i + 1);
      }
      add(kCopyPick, "pick", "PICK A FILE", "An image, a .zip, or Mech3.exe");
      add(kPickFolder, "pick", "PICK A FOLDER", "An installed copy's, or the disc's");
      add(kBack, "foot", "BACK");
      break;
    }
    case kGamePage: {  // a game's page (its gear, 2026-11-24): a row each for its game files and its copies, what each
                       // is and its buttons at its right
      const int i = page_game_;
      if (i < 0) break;
      const std::string key = upgrades::update::game_info(upgrades::update::game_at(i)).key;
      removal_[i] = linked_[i].empty() ? sdlfe::games::Removal() : sdlfe::games::removal_of(linked_[i], upgrades::update::game_at(i));
      rows_.clear();
      if (page_confirm_) {
        const bool files = !own_files(i).empty();
        add(kUninstallGo, "plates", "UNINSTALL", files || i > 1 ? "Your game files stay" : "");
        if (files) add(kUninstallAll, "plates", "UNINSTALL AND REMOVE GAME FILES", "Your pilots, saves, settings and mods still stay");
        add(kBack, "foot", "KEEP IT");
        buttons_.back().name = "KEEP IT";
        break;
      }
      auto row = [&](const char* title, const std::string& text, const std::string& path) {
        UiPanel p;
        p.title = title, p.buttons = "r" + std::to_string(rows_.size());
        p.lines = {{text, "text"}, {path, "path", 3}};
        rows_.push_back(p);
      };
      auto button = [&](int id, const char* label, const char* name = "", bool on = true) {  // none under a running game
        Button& b = add(id, ("r" + std::to_string(rows_.size() - 1)).c_str(), label, "", on && !work_ && !running(i));
        b.kind = "flat", b.name = name;
      };
      auto ver = [](const std::string& t) { return t.empty() || t == "unknown" ? std::string() : "Version " + upgrades::update::game_version(t) + ", "; };
      auto sentence = [](std::string t) { return t.empty() ? t : (t[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(t[0]))), t); };
      if (const UiPanel f = files_row(i); !f.lines.empty()) {
        row("GAME FILES", f.lines[0].text, f.lines[1].text);
        if (can_open_files(i)) button(kGameFilesPick, "CHANGE", "GAME FILES");
      }
      auto own = [&](const char* title) {
        row(title, sentence(ver(installed_[i].tag) + "installed by the launcher"), (fs::path(game_root(i)) / key).make_preferred().string());
        button(kGameUninstall, "UNINSTALL");
      };
      auto linked = [&](const char* title, const char* words) {
        std::string at = linked_[i];
        if (const size_t in = at.find(".app/Contents/MacOS/"); in != std::string::npos) at.resize(in + 4);
        row(title, sentence(ver(linked_tag_[i]) + words) + (removal_[i].why.empty() ? std::string() : " \xc2\xb7 " + removal_[i].why), at);
        button(kGameUnlink, "UNLINK");
        if (removal_[i].how != sdlfe::games::Removal::kNone) button(kGameRemove, "UNINSTALL", "UNINSTALL LINKED");
      };
      if (source_[i] == kBeside) {
        row("INSTALLED GAME", sentence(ver(MW3_VERSION) + "part of this package"), game_bin(i));
      } else if (source_[i] == kOwnCopy) {
        own("INSTALLED GAME");
        if (!linked_[i].empty()) linked("OTHER COPY", "linked, not in use");
      } else if (source_[i] == kLinked) {
        linked("INSTALLED GAME", "linked");
        if (has_installed_[i]) own("OTHER COPY");
      } else {
        using GC = upgrades::update::GameCheck;
        const GC& c = checks_[i];
        row("INSTALLED GAME", "Not installed", c.kind == GC::kNewer ? "Version " + upgrades::update::game_version(c.release.tag) + " is available" : "");
        if (c.kind == GC::kNewer) button(kGameInstall + i, "DOWNLOAD"), buttons_.back().kind = "amber";
      }
      if (source_[i] != kBeside) {
        add(kGameLink, "links", linked_[i].empty() ? "LINK A COPY ALREADY INSTALLED" : "LINK ANOTHER COPY", "", !work_).kind = "flat";
        add(kInstallFile, "links", "INSTALL FROM A DOWNLOADED FILE", "", !work_).kind = "flat";
      }
      add(kBack, "foot", "BACK");
      break;
    }
    case kDataScreen:
      add(kDataPick, "plates", "MOVE IT TO ANOTHER FOLDER", "Copied, checked, then switched. The old copy goes only if you say so");
      if (data_elsewhere()) add(kDataUsual, "plates", "MOVE IT BACK TO THE USUAL FOLDER");
      add(kBack, "foot", "BACK");
      break;
    case kConfirm:
      if (confirm_go_ == 3) {
        add(kRemoveOld, "go", "REMOVE THE OLD COPY");
        add(kKeepOld, "go", "KEEP IT");
        break;
      }
      if (confirm_go_ == 6) {  // ours: INSTALL FROM A DOWNLOADED FILE, a file nothing could check
        add(kInstallFileGo, "go", "ADD ANYWAY");
        add(kBack, "go", "BACK");
        break;
      }
      if (confirm_go_ == 4) {  // ours: Vulkan
        add(kVulkanAgain, "go", "TRY AGAIN");
        add(kBack, "go", "BACK");
        break;
      }
      if (confirm_go_ == 2) add(kMoveGo, "go", "MOVE");
      else if (confirm_go_ == 1) {  // one plate shows the find; clicking it picks other files (2026-11-21)
        add(kPickOther, "found", found_name_, found_sub_);
        if (confirm_go_ == 1) {  // ours: CHANGE FOLDER, the game files' own folder (2026-11-22)
          add_go(kGo, "INSTALL");
        } else {
          add(kGo, "go", "INSTALL");
          add(kBack, "go", "BACK");
        }
        break;
      } else if (confirm_go_ == 7) {  // a card's first DOWNLOAD: where to (2026-11-22)
        add_go(kPlaceGo, "DOWNLOAD");
        break;
      } else if (confirm_go_ == 8) {  // ours: no music came out of ADD FROM MY DISC's pick (2026-11-24)
        add(kPlayNoMusic, "plates", "PLAY WITHOUT MUSIC").kind = "amber";
        first_focus = 0;
        add(kPickOther, "pick", "PICK A FILE", "An image, a .zip, or any file of it");
        add(kPickFolder, "pick", "PICK A FOLDER", "The disc's, or its image's");
        add(kBack, "foot", "BACK");
        break;
      }
      add(kBack, "go", "BACK");
      break;
    case kWorking:
      add(kCancel, "foot", "CANCEL");
      break;
    case kUpdateScreen: {
      // Two buttons (2026-11-23: "just a back and an update button"); long notes scroll with the wheel.
      add(kUpdateGo, "foot", "UPDATE");
      add(kUpdateNotNow, "foot", "BACK");
      break;
    }
    case kNotes: {
      // Placed after the notes' measured height (notes_layout): the status line and the buttons never land on them.
      const NotesLayout n = notes_layout();
      std::printf("mw3: launcher: notes: %zu at size %d and width %d, the status line at y %d, ADD FROM MY DISC at %d, OK at %d, %s\n", notes_.size(), n.size,
                  n.width, n.status_y, n.add_y, n.ok_y, n.fits ? "above the buttons" : "into the buttons");  // for the checks
      if (n.add_y >= 0) add(kAddDisc, "disc", "ADD FROM MY DISC", kAddDiscSub);
      add(kNotesOk, "foot", "OK");
      break;
    }
  }
  {  // the screen and its plates (the checks follow them)
    static const char* const kNames[] = {"main", "find", "confirm", "work", "notes", "data", "update", "copies", "game"};
    std::string labels;
    for (const Button& b : buttons_) labels += (labels.empty() ? "" : ", ") + b.log_name();
    std::printf("mw3: launcher: screen %s: %s\n", kNames[screen_], labels.c_str());
    if (const int g = header_game(); g >= 0) std::printf("mw3: launcher: header %s\n", header_name(g));
  }
  hot_ = -1, focus_ = first_focus, follow_ = false;
}

// The game a screen sets up (0 MechWarrior 3, 1 Pirate's Moon, 2 MW1 Remake, 3 MechWarrior (1989)), its full title in
// the header (2026-11-22); -1 the launcher's own (the cards, its update, a download picked before it is read).
int Launcher::header_game() const {
  switch (screen_) {
    case kMain:
    case kUpdateScreen: return -1;
    case kGamePage: return page_game_;
    case kConfirm:
      if (confirm_go_ == 4 || confirm_go_ == 6) return -1;  // Vulkan's question; a game's download, checked
      if (confirm_go_ == 7) return place_game_;              // a card's INSTALL: where to
      if (confirm_go_ == 8) return files_pm_ ? 1 : 0;        // no music in ADD FROM MY DISC's pick
      break;
    case kWorking:
      if (!work_) break;
      switch (work_->kind) {
        case Work::kUpdateDownload:
        case Work::kGameFileCheck: return -1;
        case Work::kGameInstall: return work_game_;
        case Work::kUninstall: return page_game_;
        default: break;
      }
      break;
    default: break;
  }
  return kit_files_ >= 0 ? kit_files_ : files_pm_ ? 1 : 0;
}

// BEFORE YOU PLAY by measure (the playtest, W3: the status line "No disc in a drive ..." was at a fixed
// place and landed on the last lines of a long note; several notes at once are the worst case). The buttons
// keep their places (the frame's bottom edge is just under OK). The notes are drawn at the first of six
// forms (wider first, then smaller) at which they and the status line (a line's room kept for it whenever ADD FROM MY DISC is there, so
// the notes do not change size when it speaks) end above the first button; the status line goes under the
// notes. `fits` false: even the smallest runs into the button (said in the log; no note is cut).
Launcher::NotesLayout Launcher::notes_layout() {
  const bool add = files_pm_ ? pm_lacks_music_ || pm_lacks_sounds_ : lacks_music_ || lacks_movies_;  // what the install lacks, from the disc (lead, 2026-11-02)
  const int status_lines = std::max(add ? 1 : 0, status_.empty() ? 0 : static_cast<int>(text_.wrap(status_, 13, 480).size()));
  const int status_h = status_lines * text_.line_skip(13);
  NotesLayout l;
  l.add_y = add ? 318 : -1, l.ok_y = 384;
  const int bottom = (add ? l.add_y : l.ok_y) - 6;
  // Wider before smaller: the panel is the frame's whole width at this height.
  static constexpr int kTries[][2] = {{14, 470}, {14, 600}, {14, 700}, {13, 700}, {12, 700}, {11, 700}};
  for (const auto& t : kTries) {
    const int size = t[0];
    l.size = size, l.width = t[1], l.gap = size >= 14 ? 14 : size >= 12 ? 10 : 7;
    int h = 0;
    for (const std::string& n : notes_) h += static_cast<int>(text_.wrap(n, size, l.width).size()) * text_.line_skip(size) + l.gap;
    l.status_y = kNotesTop + h - l.gap + (status_lines ? 8 : 0);
    l.fits = l.status_y + status_h <= bottom;
    if (l.fits) break;
  }
  return l;
}

// What the screens show now (assets/launcher/ui/launcher.rml places it).
UiModel Launcher::model() {
  static const char* const kNames[] = {"main", "find", "confirm", "work", "notes", "data", "update", "copies", "game"};
  UiModel m;
  m.screen = kNames[screen_];
  for (size_t i = 0; i < buttons_.size(); ++i) {
    const Button& b = buttons_[i];
    m.buttons[b.group].push_back({static_cast<int>(i), b.label, b.sub, b.kind, b.enabled, static_cast<int>(b.sub_keep)});
  }
  m.rows = rows_;
  for (UiPanel& p : m.rows)
    for (UiLine& l : p.lines) l.text = body_.fit(l.text, 12, kRowWidth, l.keep);
  m.panels = panels_;  // kept on the other screens: the list stays as it was scrolled
  m.status = status_, m.status_kind = status_good_ ? "good" : "bad";
  // A game's own setup (mw3 or mw3pm started without the launcher): headed with that game, its badge and name (the kit
  // names no game). In the launcher every screen is headed BATTLETECH, the game a screen is for named under it
  // (2026-11-24: "launcher yes, game no").
  const int game = header_game();
  m.heading = kLauncherTitle, m.head = screen_ == kMain ? "main" : "launcher";
  if (game >= 0 && (o_.setup_only || o_.pm)) {
    const upgrades::update::CatalogGame& e = upgrades::update::catalog_game(upgrades::update::game_at(game));
    m.head = "game", m.heading = e.heading, m.badge = (e.heading_badge.empty() ? e.badge : e.heading_badge) + "?64";
    m.heading_kind = text_.width(m.heading, 36) > 520 ? "own long" : "own";  // the longer titles kept off the hazard stripes
  } else if (game >= 0) {
    m.game = header_name(game), m.heading_kind = "with";
  }
  auto upper = [](std::string n) {
    for (char& ch : n) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return n;
  };
  switch (screen_) {
    case kMain:
      m.package = package_words();  // the package's own version (the games' are on their panels)
      if (update_box_ && self_update_) {
        m.variant = "updates", m.title = "UPDATE NOW?";
        m.lines.push_back({"BattleTech launcher " + upgrades::update::plain_version(self_update_->release.tag), ""});
        for (int i : game_updates())
          m.lines.push_back({std::string(upgrades::update::game_info(upgrades::update::game_at(i)).name) + " " + upgrades::update::game_version(checks_[i].release.tag), ""});
      }
      break;
    case kSetupScreen:
      m.words = kit_files_ >= 0 ? kit_ask_.find_words : std::string(files_pm_ ? kFindWordsPm : kFindWords);
      if (!status_.empty()) break;
      m.status_kind = "use";
      if (kit_files_ >= 0) {
        if (kit_set_up(kit_files_))
          m.status = text_.fit("In use: " + (kit_status_[kit_files_].data.empty() ? std::string() : kit_status_[kit_files_].data + ", ") + kit_status_[kit_files_].where, 13, 480);
      } else if (files_pm_ ? !pm_dir_.empty() : install_.ok) {  // GAME FILES: the one in use, and what it lacks
        m.status = text_.fit(files_pm_ ? pm_in_use() : in_use(), 13, 480);
      }
      break;
    case kGamePage: {
      const int i = page_game_;
      if (i < 0) break;
      const std::string name = upper(upgrades::update::game_info(upgrades::update::game_at(i)).name);
      const std::string own_dir = (fs::path(game_root(i)) / upgrades::update::game_info(upgrades::update::game_at(i)).key).make_preferred().string();
      if (page_confirm_) {
        m.variant = "ask", m.title = "UNINSTALL " + name + "?";
        using Rm = sdlfe::games::Removal;
        if (!remove_linked_) m.lines.push_back({"Removes " + text_.fit(own_dir, 13, 400, 3), ""});
        else if (removal_[i].how == Rm::kSetup) m.lines.push_back({"Runs its own uninstaller: " + text_.fit(removal_[i].path, 13, 320, 3), ""});
        else m.lines.push_back({"Moves " + text_.fit(removal_[i].path, 13, 340, 3) + " to the trash", ""});
        const std::string files = own_files(i);
        if (!files.empty()) m.lines.push_back({"Its game files: " + text_.fit(files, 13, 400, 3), "dim"});
        else if ((i == 0 || i == 1) && !game_bin(2).empty()) m.lines.push_back({"MechWarrior 1 Remake plays these game files too, so they stay.", "dim"});
        m.lines.push_back({std::string(has_installed_[i] && !linked_[i].empty() ? "Your other copy, p" : "Your p") + "ilots, saves, settings and mods stay.", "dim"});
      }
      if (m.lines.size() > 3) m.lines.resize(3);
      for (UiLine& l : m.lines) l.text = text_.fit(l.text, 13, 480);
      m.status = text_.fit(status_, 13, 480);
      break;
    }
    case kCopies:
      m.title = files_pm_ ? "PIRATE'S MOON ON THIS COMPUTER" : "MECHWARRIOR 3 ON THIS COMPUTER";
      m.words = "Pick the copy to use: it is copied into the game's folder, with its music and movies.";
      break;
    case kNotes: {
      const NotesLayout l = notes_layout();
      m.notes = notes_;
      m.notes_size = l.size, m.notes_width = l.width, m.notes_gap = l.gap, m.notes_line = text_.line_skip(l.size);
      break;
    }
    case kConfirm:
      m.words = confirm_text_, m.variant = "ask";
      if (confirm_go_ != 7) m.status.clear();
      if (confirm_go_ == 1) {  // over and under the find's plate; where to on a line of its own, fitted
        m.variant = "found", m.title = "USE THESE GAME FILES?";
        m.path = "Into " + text_.fit(confirm_into_, 14, 440, 3);
      } else if (confirm_go_ == 7 && place_game_ >= 0) {  // a card's first INSTALL: where to
        m.variant = "place";
        m.title = "DOWNLOAD " + upper(upgrades::update::game_info(upgrades::update::game_at(place_game_)).name) + " " +
                  upgrades::update::game_version(checks_[place_game_].release.tag) + "?";
        m.words = "The game and its game files go into this folder. Your pilots, saves, settings and mods stay in your user folder.";
        m.path = text_.fit(place_.empty() ? (fs::path(games_root_) / upgrades::update::game_info(upgrades::update::game_at(place_game_)).key).make_preferred().string() : place_, 14, 440, 3);
      } else if (confirm_go_ == 8) {
        m.variant = "nomusic", m.title = "NO MUSIC ADDED";
      } else if (confirm_go_ == 4 || confirm_go_ == 6) {
        m.variant = "ask long";
      }
      break;
    case kUpdateScreen: {  // the newer release's notes (the wheel scrolls them) and the question
      const std::string version = update_ ? upgrades::update::plain_version(update_->release.tag) : std::string();
      m.title = "BATTLETECH LAUNCHER " + version;
      m.notes = update_lines_;
      m.words = !status_.empty() ? status_ : "Update the launcher to " + version + "? Your games, mods, pilots, saves and settings are kept.";
      if (status_.empty()) m.status_kind = "good";
      break;
    }
    case kDataScreen:
      m.words = "Your settings, pilots, saved games and converted movies are in:";
      m.path = text_.fit(files_wd(), 14, 480);
      break;
    case kWorking: {
      own::Progress p;
      if (work_) {
        std::lock_guard<std::mutex> g(work_->m);
        p = work_->progress, m.title = work_->title;
      }
      m.fraction = static_cast<float>(std::clamp(p.fraction, 0.0, 1.0));
      m.percent = std::to_string(std::lround(p.fraction * 100)) + "%", m.stage = p.stage, m.message = p.message;
      break;
    }
  }
  // Left on screen while the system's picker comes up.
  if (ask_pending_) m.picker = ask_is_folder_ ? "Opening the folder picker..." : "Opening the file picker...";
  return m;
}

// The screen as it is now, laid out and drawn (not yet shown).
void Launcher::draw() {
  sync_ui();
  ui_.render();
}

// What the document shows brought up to date: before it is drawn, and before the mouse asks what it is over.
void Launcher::sync_ui() {
  if (follow_ && focus_ < 0) hot_ = ui_.over();
  const int hot = focus_ >= 0 ? focus_ : hot_;
  const int lit = hot >= 0 && hot < static_cast<int>(buttons_.size()) && buttons_[hot].enabled ? hot : -1;
  ui_.show(model(), lit);
  if (ui_.update()) ui_.show(model(), lit), ui_.update();
  ui_stale_ = false;
}

// A frame at the layout's own size, saved (checks).
void Launcher::save_frame(const std::string& path) {
  SDL_Texture* t = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W, H);
  SDL_SetRenderTarget(ren_, t);
  draw();
  SDL_Surface* s = t ? SDL_RenderReadPixels(ren_, nullptr) : nullptr;
  SDL_SetRenderTarget(ren_, nullptr);
  if (SDL_Surface* c = s ? SDL_ConvertSurface(s, SDL_PIXELFORMAT_XRGB8888) : nullptr) SDL_SaveBMP(c, path.c_str()), SDL_DestroySurface(c);
  if (s) SDL_DestroySurface(s);
  if (t) SDL_DestroyTexture(t);
  sync_ui();
}

// MW3_LAUNCHER_MOUSE (checks): "move 712,130;click 712,130;wheel -2;down 800,120;up 800,300", a step a few frames, at
// the layout's 854x480, as the window's own events.
void Launcher::mouse_steps() {
  static const char* const at = SDL_getenv("MW3_LAUNCHER_MOUSE");
  static std::string left = at ? at : "";
  if (left.empty() || frames_ % 6 != 5) return;
  const size_t end = left.find(';');
  const std::string step = left.substr(0, end);
  left = end == std::string::npos ? std::string() : left.substr(end + 1);
  char what[16] = {};
  float x = 0, y = 0;
  if (std::sscanf(step.c_str(), "%15s %f,%f", what, &x, &y) < 2) return;
  int w = W, h = H;
  SDL_GetWindowSize(win_, &w, &h);
  const float k = std::min(static_cast<float>(w) / W, static_cast<float>(h) / H);
  auto push = [&](Uint32 type) {
    SDL_Event e{};
    e.type = type;
    if (type == SDL_EVENT_MOUSE_WHEEL) e.wheel.windowID = SDL_GetWindowID(win_), e.wheel.y = x;
    else if (type == SDL_EVENT_MOUSE_MOTION) e.motion.windowID = SDL_GetWindowID(win_), e.motion.x = x * k, e.motion.y = y * k;
    else e.button.windowID = SDL_GetWindowID(win_), e.button.button = SDL_BUTTON_LEFT, e.button.x = x * k, e.button.y = y * k;
    SDL_PushEvent(&e);
  };
  const std::string_view v = what;
  std::printf("mw3: launcher: mouse: %s, over %s\n", step.c_str(), hot_ >= 0 ? buttons_[hot_].log_name().c_str() : "nothing");
  if (v == "wheel") return push(SDL_EVENT_MOUSE_WHEEL);
  push(SDL_EVENT_MOUSE_MOTION);
  if (v == "down" || v == "click") push(SDL_EVENT_MOUSE_BUTTON_DOWN);
  if (v == "up" || v == "click") push(SDL_EVENT_MOUSE_BUTTON_UP);
}

void Launcher::ask_file(int what, const SDL_DialogFileFilter* filters, int n) {
  {
    std::lock_guard<std::mutex> g(pick_.m);
    pick_.what = what, pick_.path.clear(), pick_.answered = pick_.cancelled = false;
  }
  asking_ = ask_pending_ = true, ask_shown_ = ask_is_folder_ = false;
  ask_filters_ = filters, ask_filter_count_ = n;
}
void Launcher::ask_folder(int what) {
  {
    std::lock_guard<std::mutex> g(pick_.m);
    pick_.what = what, pick_.path.clear(), pick_.answered = pick_.cancelled = false;
  }
  asking_ = ask_pending_ = ask_is_folder_ = true, ask_shown_ = false;
}
// The system's picker, once the frame with its notice is on screen (run).
// MW3_LAUNCHER_PICKER=cancel (checks): no window, the ask answered as cancelled
// (=path:<file>, answered with that file);
// MW3_LAUNCHER_PICKER_SHOT: the frame on screen at the ask, saved.
void Launcher::open_picker() {
  if (const char* shot = SDL_getenv("MW3_LAUNCHER_PICKER_SHOT")) save_frame(shot);
  ask_pending_ = false, ask_at_ = SDL_GetTicksNS();
  std::printf("mw3: launcher: the %s picker asked for\n", ask_is_folder_ ? "folder" : "file");
  const char* check = SDL_getenv("MW3_LAUNCHER_PICKER");
  if (check && std::strcmp(check, "cancel") == 0) Pick::done(&pick_, nullptr, 0);
  else if (check && std::strncmp(check, "path:", 5) == 0) {  // checks: the picker's answer as given
    const char* list[] = {check + 5, nullptr};
    Pick::done(&pick_, list, 0);
  }
  else if (!sdlfe::open_picker({ask_is_folder_ ? sdlfe::PickerKind::kOpenFolder : sdlfe::PickerKind::kOpenFile, Pick::done,
                                &pick_, win_, ask_filters_, ask_filter_count_, nullptr}))
    std::printf("mw3: launcher: no picker in a run no player is at: cancelled\n");
  std::printf("mw3: launcher: the picker's call took %.0f ms\n", static_cast<double>(SDL_GetTicksNS() - ask_at_) / 1e6);
}

// A picked path: what the button that asked does with it.
void Launcher::picked(int what, const std::string& path) {
  // Before Windows 10 1903 such a name cannot be used (docs/program.md, Names on Windows).
  if (!own::narrow_paths_utf8() && !std::all_of(path.begin(), path.end(), [](char c) { return static_cast<unsigned char>(c) < 0x80; })) {
    status_ = "This version of Windows needs a folder with plain English letters in its name.", status_good_ = false;
    layout();
    return;
  }
  if (what == kGameLink) return link_picked(path);
  if (what == kInstallFile) {  // its SHA256SUMS line looked for on the worker (the file may be large)
    install_file_ = path;
    work_ = std::make_unique<Work>();
    work_->kind = Work::kGameFileCheck;
    auto check = file_check_ = std::make_shared<sdlfe::games::FileCheck>();
    work_->start("CHECKING THE FILE", [path, check](const own::ProgressFn& f) {
      f(own::Progress{"Checking " + fs::path(path).filename().string(), 0.5, ""});
      *check = sdlfe::games::check_file(path);
      own::Result r;
      r.ok = check->kind != sdlfe::games::FileCheck::kMismatch, r.summary = check->words;
      return r;
    });
    screen_ = kWorking, layout();
    return;
  }
  if (what == kInstallTo) {  // the game's own folder: the card INSTALL's (confirm 7) or the game files' (confirm 1)
    const int game = confirm_go_ == 7 ? place_game_ : kit_files_ >= 0 ? kit_files_ : files_pm_ ? 1 : 0;
    if (game < 0) return layout();
    const std::string dir = fs::path(sdlfe::games::game_folder_in(path, upgrades::update::game_at(game))).make_preferred().string();
    std::printf("mw3: launcher: %s goes into %s\n", upgrades::update::game_info(upgrades::update::game_at(game)).name, dir.c_str());
    if (confirm_go_ == 7) place_ = dir;
    else files_into_ = dir, files_confirm_text();
    status_.clear();
    return layout();
  }
  if (what == kDataPick)
    return ask_move(rekit::data_folder::destination(path, files_usual(), files_pm_ ? "Pirate's Moon Data" : "MechWarrior 3 Data"));
  if (what == kPoint && kit_files_ >= 0) return kit_look(path);  // the game knows what a pick is
  if (what == kPoint) return look_at(path);  // b6: the game's --look takes any pick, as a kit game's does
  if (what == kAddDisc) return add_disc(path);
  layout();
}

// What the install lacks (the movies left on the CD, the music), taken from
// The disc, a disc image or the disc's folder (setup::add_from_disc).
void Launcher::add_disc(const std::string& source) {
  if (screen_ != kConfirm) disc_back_ = screen_;  // PICK A FILE on the no-music dialog: where the first pick came from
  disc_music_ = files_pm_ ? pm_lacks_music_ : lacks_music_;
  if (files_served()) return served_work(Work::kAddDisc, "ADDING FROM YOUR DISC", {"--add", source});
  older_game(files_pm_ ? 1 : 0);  // b6: only a game that serves its setup adds from its disc
}

// The notes for the install now there (launcher_notes): what it lacks.
void Launcher::notes_refresh() {
  notes_.clear();
  lacks_music_ = lacks_movies_ = false;
  if (!install_.ok) return;
  status_refresh(0);
  if (!status_for(0, install_.dir)) return notes_refresh_older();
  // b2: its status's: the start check's notes_once first, then launcher_notes', shown here in the order before (the
  // music's folder shortened to fit), the check's last.
  const rekit::Status& s = kit_status_[0];
  lacks_music_ = std::find(s.lacks.begin(), s.lacks.end(), "music") != s.lacks.end();
  lacks_movies_ = std::find(s.lacks.begin(), s.lacks.end(), "movies") != s.lacks.end();
  if (!s.lacks.empty()) std::printf("mw3: launcher: games: mw3: lacks %s\n", own_join(s.lacks).c_str());
  const std::string md = media();
  const std::string dir = (md.empty() ? fs::path("music") : fs::path(md) / "music").make_preferred().string();
  const size_t once = static_cast<size_t>(s.notes_once);
  mw3_check_notes_.assign(s.notes.begin(), s.notes.begin() + static_cast<std::ptrdiff_t>(once));
  for (size_t k = once; k < s.notes.size(); ++k) {
    std::string n = s.notes[k];
    std::printf("mw3: launcher: note: %s\n", n.c_str());
    if (const size_t at = n.find(dir); at != std::string::npos) n.replace(at, dir.size(), text_.fit(dir, 14, 440));
    notes_.push_back(n);
  }
  for (const std::string& n : mw3_check_notes_) std::printf("mw3: launcher: note: %s\n", n.c_str()), notes_.push_back(n);
}

void Launcher::notes_refresh_older() { mw3_check_notes_.clear(); }  // b6: an older program has no notes here

// The start check's notes counted as shown (mw3_notes=, as notes_once keeps them): the notes screen has them.
void Launcher::mw3_notes_seen() {
  std::string all;
  for (const std::string& n : mw3_check_notes_) all += n + "\n";
  char crc[16];
  std::snprintf(crc, sizeof crc, "%08x", own::crc32(0, reinterpret_cast<const uint8_t*>(all.data()), all.size()));
  if (mw3_check_notes_.empty() || o_.cfg_mw3_notes == crc) return;
  o_.cfg_mw3_notes = crc;
  own::save_config_value(o_.write_dir, "mw3_notes", crc);
}

// Pirate's Moon's usual settings folder, as mw3pm finds it: BattleTech/Pirate's Moon, a player's earlier
// MicroProse/Pirate's Moon moved across once (usual_folder; MW3_USUAL_FOLDER's sibling in checks).
std::string Launcher::pm_usual() const {
  if (std::getenv("MW3_USUAL_FOLDER")) {
    const std::string mw3 = own::usual_folder();
    return mw3.empty() ? std::string() : (fs::path(mw3).parent_path() / "Pirate's Moon").string();
  }
  static const std::string pm = own::usual_folder("Pirate's Moon");  // moved at most once a run
  return pm;
}

// Its folder in use: MW3_PM_WRITE_DIR (checks), else the usual one or where
// its pointer leads (rekit/data_folder.h), as mw3pm resolves it.
std::string Launcher::pm_wd() {
  if (o_.pm_write_dir.empty() && !pm_usual().empty()) o_.pm_write_dir = rekit::data_folder::resolve(pm_usual(), own::data_game()).dir;
  return o_.pm_write_dir;
}

// What Pirate's Moon lacks that its disc has: the music, a soundsH.zbd with
// sounds in it (setup::check_install's note).
void Launcher::pm_refresh() {
  pm_lacks_music_ = pm_lacks_sounds_ = false;
  if (pm_dir_.empty()) return;
  status_refresh(1);
  if (!status_for(1, pm_dir_)) return pm_refresh_older();
  const rekit::Status& s = kit_status_[1];  // b2: what it lacks in its --copies words
  pm_lacks_music_ = std::find(s.lacks.begin(), s.lacks.end(), "music") != s.lacks.end();
  pm_lacks_sounds_ = std::find(s.lacks.begin(), s.lacks.end(), "full-quality sounds") != s.lacks.end();
  pm_data_ = s.data.empty() ? "unknown" : s.data;
  if (!s.lacks.empty()) std::printf("mw3: launcher: games: pm: lacks %s\n", own_join(s.lacks).c_str());
}

void Launcher::pm_refresh_older() { pm_data_.clear(); }  // b6: an older program, not checked here

std::string Launcher::pm_in_use() const {
  std::string s = "Using Pirate's Moon at " + pm_dir_;
  if (pm_lacks_music_ && pm_lacks_sounds_) s += ". No music, low-quality sounds.";
  else if (pm_lacks_music_) s += ". No music.";
  else if (pm_lacks_sounds_) s += ". Low-quality sounds.";
  return s;
}

// A Pirate's Moon found or installed in GAME FILES: saved as pm_install=,
// the launcher's own earlier copy (<its settings>/install) removed once no
// longer pointed at, then back to the games (or, from its click, started).
void Launcher::use_pm(const std::string& dir, const std::string& summary) {
  const std::string before = pm_dir_;
  pm_dir_ = dir, o_.cfg_pm_install = dir, pm_found_.clear();
  own::save_config_value(o_.write_dir, o_.pm ? "install" : "pm_install", dir);  // mw3pm's own mw3.cfg: install=
  status_ = summary, status_good_ = true;
  const fs::path own = fs::path(pm_wd()) / "install";
  std::error_code ec;
  if (!pm_wd().empty() && !before.empty() && fs::equivalent(before, own, ec) && !fs::equivalent(dir, own, ec) &&
      fs::exists(own / "mw3-install.txt", ec)) {
    std::printf("mw3: launcher: the earlier Pirate's Moon copy in %s is no longer used: removed\n", own.string().c_str());
    fs::remove_all(own, ec);
  }
  pm_refresh();
  if (o_.pm) {  // mw3pm's own start: on into the game with these files
    std::printf("mw3: launcher: Pirate's Moon at %s: on into the game\n", dir.c_str());
    choice_ = {LauncherChoice::kPlay, dir};
    done_ = true;
    return;
  }
  // W13: back to the games after an install (or GAME FILES), Pirate's Moon started by its own click.
  from_files_ = false;
  screen_ = kMain, layout();
  if (auto_play_) auto_play_ = false, activate(kPm);  // checks: PIRATE'S MOON pressed
}

// GAME FILES' line: the install in use and what it lacks.
std::string Launcher::in_use() const {
  std::string s = "Using MechWarrior 3 " + install_.version + " at " + install_.dir;
  if (lacks_movies_ && lacks_music_) s += ". No movies or music.";
  else if (lacks_movies_) s += ". No movies.";
  else if (lacks_music_) s += ". No music.";
  return s;
}

// A newer release found: its notes as plain lines wrapped to the screen, the first page, the question.
void Launcher::show_update() {
  read_update_notes();
  status_.clear();
  screen_ = kUpdateScreen, layout();
  if (SDL_getenv("MW3_LAUNCHER_UPDATE_GO")) activate(kUpdateGo);  // checks: UPDATE pressed
}

void Launcher::read_update_notes() {
  update_lines_.clear();
  for (const std::string& line : upgrades::update::notes_lines(update_->release.notes)) {
    update_lines_.push_back(line);
    for (const std::string& w : text_.wrap(line, 13, kUpdateWidth)) std::printf("mw3: launcher: update notes: %s\n", w.c_str());
  }
  if (update_lines_.empty()) update_lines_.push_back("(No notes with this release.)");
}

// Ours (docs/linux.md, Vulkan): the game cannot draw without Vulkan on Linux; this screen says what
// to install. TRY AGAIN checks again (the packages installed meanwhile), BACK to the main screen.
void Launcher::show_vulkan() {
  confirm_text_ = vulkan_help(vulkan_);
  confirm_go_ = 4;
  std::printf("mw3: launcher: %s\n", confirm_text_.c_str());
  screen_ = kConfirm, layout();
}

// MechWarrior 3 started: by mw3launcher as the game's own program, else
// (a plain mw3 start, or no game_bin) the window handed on.
void Launcher::play() {
  if (!mw3_ready()) return;
  if (vulkan_ != Vulkan::kOk && (vulkan_ = vulkan_check()) != Vulkan::kOk) return show_vulkan();  // installed meanwhile: on
  // The movies converted here first, with the bar (a first start sat
  // 44 s on "Starting MechWarrior 3..." while mw3 did it); mw3's own import
  // stays the fallback for a start without the launcher.
  if (!converted_ && movies_pending()) {
    converted_ = true;
    convert_movies([this] { play(); });
    return;
  }
  if (!o_.setup_only && !o_.game_bin.empty()) {
    screen_ = kMain, layout();
    if (!running(0)) start_game();
    return;
  }
  choice_ = {LauncherChoice::kPlay, install_.dir};
  done_ = true;
}

// The movies the install and the write dir (ADD FROM MY DISC's) have not
// converted for the player (mw3import); none on Windows, whose helper plays
// the AVIs as they are, or without the importer or with MW3_NO_IMPORT.
int Launcher::movies_to_convert() const { return 0; }  // b6: an older program converts its movies at its own start

// mw3import on the worker, the bar following its "progress" lines; CANCEL
// stops it (the movies done stay; the rest wait for the next start).
// b4: its status's pending "movies" (setup: 1), unless this launcher was told not to (--no-import, MW3_NO_IMPORT);
// else the launcher's own count (the older programs' path, ends at b6).
bool Launcher::movies_pending() {
  if (!status_for(0, install_.dir)) return movies_to_convert() > 0;
  if (o_.no_import || SDL_getenv("MW3_NO_IMPORT")) return false;
  status_refresh(0);
  const std::vector<std::string>& p = kit_status_[0].pending;
  return std::find(p.begin(), p.end(), "movies") != p.end();
}

void Launcher::convert_movies(std::function<void()> then) {
  after_convert_ = std::move(then);
  if (status_for(0, install_.dir)) return served_work(Work::kConvert, "CONVERTING THE MOVIES", {"--first-run", "movies"}, 0);  // b4
  std::function<void()> then_now = std::move(after_convert_);  // b6: nothing here converts an older program's movies
  after_convert_ = nullptr;
  if (then_now) then_now();
}

bool Launcher::data_elsewhere() {
  std::error_code ec;
  const std::string usual = files_usual(), wd = files_wd();
  return !usual.empty() && !fs::equivalent(wd, usual, ec) && wd != usual;
}

// The player's folder to `to` (docs/program.md, The data folder): asked first,
// with the sizes; a move that cannot be done is said and nothing starts.
void Launcher::ask_move(const std::string& to) {
  const rekit::data_folder::Plan p = rekit::data_folder::plan(files_wd(), to, files_usual(), own::data_game());
  if (!p.error.empty()) {
    status_ = p.error, status_good_ = false;
    screen_ = kDataScreen, layout();
    return;
  }
  char size[64];
  std::snprintf(size, sizeof size, "%.2f GB, %.1f GB free there", static_cast<double>(p.bytes) / 1e9, static_cast<double>(p.free) / 1e9);
  move_to_ = to;
  confirm_text_ = "Move your data to " + to + "?\n\n" + std::to_string(p.files) + " files, " + size +
                  ". The old copy stays until you choose.";
  confirm_go_ = 2;
  screen_ = kConfirm, layout();
}

void Launcher::start_move() {
  work_ = std::make_unique<Work>();
  work_->kind = Work::kMoveData;
  const std::string from = files_wd(), to = move_to_, usual = files_usual();
  move_from_ = from;
  auto copied = std::make_shared<std::vector<std::string>>();
  move_copied_out_ = copied;
  work_->start("MOVING YOUR DATA", [from, to, usual, copied](const own::ProgressFn& f) {
    own::Result r;
    const rekit::data_folder::Moved m = rekit::data_folder::move_data(from, to, usual, own::data_game(), [&](double at, const std::string& name) {
      return f(own::Progress{"Copying and checking", at, name});
    });
    *copied = m.copied;
    r.ok = m.ok, r.cancelled = m.cancelled;
    r.summary = m.ok ? "Your data is now in " + to + "." : m.cancelled ? "Stopped. Nothing was changed." : m.error;
    return r;
  });
  screen_ = kWorking, layout();
}

// The move done: this run goes on with the new folder (its own log stays in
// the old one until the next start), and the old copy is the player's to remove.
void Launcher::moved(const own::Result& r) {
  if (!r.ok) {
    status_ = r.summary, status_good_ = r.cancelled;
    screen_ = kDataScreen, layout();
    return;
  }
  move_copied_ = *move_copied_out_;
  using rekit::data_folder::moved_path;
  const std::string from = move_from_, to = move_to_;
  if (files_pm_) {  // Pirate's Moon's: its install inside it follows; mw3pm finds the folder by its pointer
    const std::string dir = moved_path(pm_dir_, from, to);
    if (dir != pm_dir_) pm_dir_ = o_.cfg_pm_install = dir, own::save_config_value(o_.write_dir, "pm_install", dir);
    o_.pm_write_dir = to;
    pm_refresh();
    confirm_text_ = r.summary + "\n\nThe old copy is still in " + from + ".\nRemove it? Log files stay there.";
    confirm_go_ = 3;
    screen_ = kConfirm, layout();
    if (const char* a = SDL_getenv("MW3_LAUNCHER_MOVE_OLD"))  // checks: the answer
      activate(std::strcmp(a, "remove") == 0 ? kRemoveOld : kKeepOld);
    return;
  }
  if (install_.ok) install_.dir = moved_path(install_.dir, from, to);
  o_.cfg_install = moved_path(o_.cfg_install, from, to);
  mw1_game_ = moved_path(mw1_game_, from, to);
  o_.write_dir = to;
  bool named = false;
  for (size_t i = 0; i + 1 < o_.game_args.size(); ++i)
    if (o_.game_args[i] == "--write-dir") o_.game_args[i + 1] = to, named = true;
  if (!named) o_.game_args.push_back("--write-dir"), o_.game_args.push_back(to);
  notes_refresh();
  confirm_text_ = r.summary + "\n\nThe old copy is still in " + from + ".\nRemove it? Log files stay there.";
  confirm_go_ = 3;
  screen_ = kConfirm, layout();
  if (const char* a = SDL_getenv("MW3_LAUNCHER_MOVE_OLD"))  // checks: the answer
    activate(std::strcmp(a, "remove") == 0 ? kRemoveOld : kKeepOld);
}

// A found install used: saved to mw3.cfg, then back to the games (W13
// 2026-11-12, replacing 2026-11-02's straight on into the game); when it lacks
// the movies or the music, or its data is odd, the notes first, their OK back.
void Launcher::use_install(const std::string& dir, const std::string& version, const std::string& summary) {
  const std::string before = install_.ok ? install_.dir : std::string();
  install_ = {true, dir, version, summary};
  own::save_install(o_.write_dir, dir);
  status_ = summary, status_good_ = true;
  // GAME FILES moved to a copy used where it is: the launcher's own earlier
  // copy (<write dir>/install) is no longer pointed at, so it goes (a disc
  // or archive install replaces it in place, setup::swap_in_folder).
  const fs::path own = fs::path(o_.write_dir) / "install";
  std::error_code ec;
  if (!o_.write_dir.empty() && !before.empty() && fs::equivalent(before, own, ec) && !fs::equivalent(dir, own, ec) &&
      fs::exists(own / "mw3-install.txt", ec)) {
    std::printf("mw3: launcher: the earlier copy in %s is no longer used: removed\n", own.string().c_str());
    fs::remove_all(own, ec);
  }
  notes_refresh();
  if (from_files_) {  // GAME FILES: back to the games, the new files in use
    from_files_ = false;
    screen_ = kMain, layout();
    return;
  }
  // W13 (2026-11-12): an install ends on the games, not in the game, so Pirate's Moon can be added
  // before playing either; the notes first when there are any, their OK back to the games. A plain mw3
  // start (setup_only: no games screen, only the find screen) goes on into the game as before.
  if (o_.setup_only && notes_.empty()) return play();
  if (!notes_.empty()) {
    play_after_notes_ = o_.setup_only, status_.clear();
    mw3_notes_seen();
    screen_ = kNotes, layout();
    if (auto_ok_) {  // checks (MW3_LAUNCHER_PICK): the notes' OK pressed
      auto_ok_ = false;
      activate(kNotesOk);
    }
  } else {
    screen_ = kMain, layout();
  }
  if (auto_play_ && !o_.setup_only) auto_play_ = false, activate(kMw3);  // checks (MW3_LAUNCHER_PICK): MECHWARRIOR 3 pressed, as a player would
}

// What the player pointed at, looked into on the worker (an archive's look
// can take seconds: the MW3 disc's installer header is 209 MB into its image).
void Launcher::look_at(const std::string& path) {
  pending_source_ = path;
  if (files_served()) return served_work(Work::kLook, "LOOKING AT YOUR GAME", {"--look", path});
  older_game(files_pm_ ? 1 : 0);  // b6: only a game that serves its setup looks at a pick
}

// GAME FILES' list (2026-11-24): every copy of the game this computer holds where the launcher looks (the game's own
// folder, mw3.cfg's, the user folder's, the installer's places and a drop-in), each once, the one in use first; each
// with what the game would lack with it: the music and the movies (MW3), the music and the full sounds (Pirate's
// Moon). Music or movies the user folder already holds count for every copy.
void Launcher::find_copies() {
  if (files_served()) return find_copies_served();
  copies_.clear();
}

// b3: the served asks of GAME FILES (docs/launcher-split.md): the game's --setup verb with its user folder.
std::vector<std::string> Launcher::served_args(std::vector<std::string> ask, int game) {
  for (const std::string& a : status_args(game >= 0 ? game : files_pm_ ? 1 : 0)) ask.push_back(a);
  return ask;
}

// A served ask on the worker, its stages on the work screen: a look's answer is its words, an install's or an
// add's its summary; the answer kept (served_said_) for its finish.
void Launcher::served_work(int kind, const std::string& title, std::vector<std::string> ask, int game) {
  if (game < 0) game = files_pm_ ? 1 : 0;
  auto said = served_said_ = std::make_shared<rekit::SetupMessage>();
  served_work_ = true, served_game_ = game;
  work_ = std::make_unique<Work>();
  work_->kind = static_cast<Work::Kind>(kind);
  const std::string bin = game_bin(game);
  const bool look = kind == Work::kLook;
  ask = served_args(std::move(ask), game);
  std::printf("mw3: launcher: games: %s: --setup %s\n", game == 1 ? "pm" : "mw3", ask.front().c_str());
  work_->start(title, [bin, ask, said, look](const own::ProgressFn& f) {
    own::Result r;
    // Every step on the work screen, as the launcher's own work had them (a quick one's steps come in one read).
    const bool answered = sdlfe::games::kit_setup(
        bin, ask, 0, [&f](const rekit::SetupMessage& m) { return f(own::Progress{m.stage, m.fraction, m.message}); }, said.get(),
        [&f](const rekit::SetupMessage& m) {
          if (m.copy.empty()) f(own::Progress{m.stage, m.fraction, m.message});
        });
    r.ok = answered && said->ok, r.cancelled = said->cancelled;
    r.summary = r.ok && look ? said->words : said->summary, r.install_dir = said->where;
    return r;
  });
  screen_ = kWorking, layout();
}

// GAME FILES' list from the game (--copies): each copy's folder, data, whether in use and what it lacks.
void Launcher::find_copies_served() {
  copies_.clear();
  rekit::SetupMessage done;
  const bool said = sdlfe::games::kit_setup(
      game_bin(files_pm_ ? 1 : 0), served_args({"--copies"}), 60, [](const rekit::SetupMessage&) { return true; }, &done,
      [this](const rekit::SetupMessage& m) {
        if (m.copy.empty()) return;
        Copy c;
        c.path = m.copy, c.version = m.copy_data.empty() ? "unknown" : m.copy_data, c.in_use = m.copy_in_use, c.missing = m.copy_missing;
        std::string missing;
        for (const std::string& x : c.missing) missing += (missing.empty() ? "" : " and ") + x;
        std::printf("mw3: launcher: copy %s: %s%s%s\n", c.path.c_str(), c.version.c_str(), c.in_use ? ", in use" : "",
                    c.missing.empty() ? ", nothing missing" : (", missing " + missing).c_str());
        copies_.push_back(std::move(c));
      });
  if (!said || !done.ok) std::printf("mw3: launcher: games: %s: --copies: %s\n", files_pm_ ? "pm" : "mw3", done.summary.c_str());
}

// The automatic search: an installed copy (mw3.cfg's, the installer's
// places), else a disc in a drive (looked into as if picked).
void Launcher::search() {
  if (kit_files_ >= 0) return kit_look("");
  if (files_pm_) {  // Pirate's Moon's: found again where it is, as at the start
    const std::string had = pm_dir_;
    pm_dir_.clear(), o_.cfg_pm_install = had;
    find_pm();
    if (pm_dir_.empty() && !pm_found_.empty()) return look_at(pm_found_);  // offered, to copy in or pass over
    if (!pm_dir_.empty() && !from_files_ && !o_.choose_again) return use_pm(pm_dir_, "Found Pirate's Moon at " + pm_dir_ + ".");
  } else {
    find_install();
  }
  if (!files_pm_ && install_.ok && install_.elsewhere) return look_at(install_.dir);  // offered, to copy in or pass over
  if (!files_pm_ && install_.ok && !from_files_ && !o_.choose_again) {  // SETUP's own copy (or mw3.cfg's): in use as before
    use_install(install_.dir, install_.version, install_.summary);
    return;
  }
  // GAME FILES, the player choosing again: the copy in use is no answer; the picker for another (2026-11-21).
  const std::string in_use = files_pm_ ? pm_dir_ : install_.ok ? install_.dir : "";
  if ((from_files_ || o_.choose_again) && !in_use.empty()) {  // every copy on this computer, what each lacks (2026-11-24)
    find_copies();
    status_.clear();
    screen_ = kCopies, layout();
    return;
  }
  if (files_served()) {  // the game's search (its copy, else a disc in a drive); its copy is the status's, so a disc
    rekit::SetupMessage found;
    if (sdlfe::games::kit_setup(game_bin(files_pm_ ? 1 : 0), served_args({"--search"}), 120, [](const rekit::SetupMessage&) { return true; }, &found) &&
        found.ok && !found.path.empty())
      return look_at(found.path);
  }
  status_ = std::string(files_pm_ ? "Pirate's Moon was not" : "Not") +
            " found on this computer or in a disc drive. Pick it: a disc image, a download, or Mech3.exe.";
  status_good_ = true;
  screen_ = kSetupScreen;
  layout();
  ask_file(kPoint, kGameFiles, kGameFilesCount);  // the one picker (an installed copy by any of its files)
}

void Launcher::finish_work() {
  own::Result r;
  {
    std::lock_guard<std::mutex> g(work_->m);
    r = work_->result;
  }
  if (work_->th.joinable()) work_->th.join();
  const Work::Kind kind = work_->kind;
  work_.reset();
  const bool served = std::exchange(served_work_, false);
  if (served) status_fresh_[served_game_] = false;  // what the game did: its status asked again
  std::printf("mw3: launcher: %s%s%s\n", r.summary.c_str(), r.detail.empty() ? "" : " | ", r.detail.c_str());
  if (kind == Work::kRemoveData) {
    status_ = r.summary, status_good_ = r.ok;
    from_files_ = false, screen_ = kMain, layout();
    return;
  }
  if (kind == Work::kMoveData) return moved(r);
  if (kind == Work::kKitLook) return kit_looked(r);
  if (kind == Work::kKitInstall) {
    if (r.ok) return kit_ready(r.summary);
    status_ = r.summary, status_good_ = r.cancelled;
    screen_ = kSetupScreen;
    return layout();
  }
  if (kind == Work::kUninstall) {  // the game's page again: what PLAY starts now (a linked copy, or nothing)
    const int i = page_game_;
    if (i >= 0) {
      if (uninstall_files_ && r.ok) {  // the removed game files no longer named as the ones in use
        if (i == 0) own::save_config_value(o_.write_dir, "install", "");
        if (i == 1) own::save_config_value(o_.write_dir, "pm_install", ""), own::save_config_value(pm_wd(), "install", "");
      }
      if (remove_linked_ && r.ok) own::save_config_value(o_.write_dir, std::string("link_") + upgrades::update::game_info(upgrades::update::game_at(i)).key, "");
      resolve_game(i, true);
    }
    status_ = r.summary, status_good_ = r.ok, page_confirm_ = false, screen_ = kGamePage;
    layout();
    return;
  }
  if (kind == Work::kOriginal) return got_original(r);
  if (kind == Work::kGameInstall) {  // games on demand: in use at once, or why not on the status line
    status_ = r.summary, status_good_ = r.ok || r.cancelled, screen_ = kMain;
    if (r.ok && game_done_) {
      const int g = static_cast<int>(game_done_->game);
      const bool first = !has_installed_[g] && work_game_ == g;  // a card's DOWNLOAD: not an UPDATE, not a file of the player's
      game_installed(*game_done_);
      if (first && get_original(g)) return;  // its original files first; the queue after them
    }
    layout();
    if (!r.ok) update_queue_.clear(), update_queue_self_ = false;
    next_update();
    return;
  }
  if (kind == Work::kGameFileCheck) {  // matches: installed; no SHA256SUMS: asked; does not match: refused
    using FC = sdlfe::games::FileCheck;
    const FC c = file_check_ ? *file_check_ : FC{};
    if (c.kind == FC::kMatches) return install_file_go();
    if (c.kind == FC::kMismatch) {
      status_ = c.words, status_good_ = false, screen_ = kMain;
      layout();
      return;
    }
    confirm_go_ = 6, screen_ = kConfirm, status_.clear();
    confirm_text_ = c.words + " Install it anyway? Only install a file from someone you trust.";
    layout();
    if (SDL_getenv("MW3_LAUNCHER_INSTALL_ANYWAY")) activate(kInstallFileGo);  // checks: ADD ANYWAY pressed
    return;
  }
  if (kind == Work::kUpdateDownload) {  // installed: the new launcher started and this one gone; else why, on the screen
    std::printf("mw3: launcher: update: %s\n", r.summary.c_str());
    status_ = r.summary, status_good_ = r.ok || r.cancelled;
    screen_ = kUpdateScreen, layout();
    if (r.ok && !SDL_getenv("MW3_UPDATE_NO_RESTART")) {  // checks keep this launcher
      const std::string next = update_file_ ? *update_file_ : std::string();
      if (!next.empty() && !start_detached(next)) status_ = "Installed. Start the BattleTech launcher again.";
      else done_ = true;  // Windows: the Setup replaces this program once it is gone
    }
    return;
  }
  if (kind == Work::kConvert) {  // on where it was going
    std::function<void()> then = std::move(after_convert_);
    after_convert_ = nullptr;
    if (then) then();
    return;
  }
  if (kind == Work::kAddDisc) {  // back to the notes, saying what was added
    status_ = r.summary, status_good_ = r.ok || r.cancelled;
    for (const std::string& w : r.warnings) status_ += " " + w;
    notes_refresh();
    if (files_pm_) {
      pm_refresh();
      if (disc_back_ == kNotes) notes_ = pm_notes();
    }
    Screen back = disc_back_ == kSetupScreen ? kSetupScreen : kNotes;
    // Served (b3): no count of what came; the status says whether the music is still wanting.
    const bool no_music = served ? disc_music_ && !r.cancelled && (files_pm_ ? pm_lacks_music_ : lacks_music_)
                                 : disc_music_ && r.music_tracks == 0 && !r.cancelled;
    if (no_music) {  // no error: the game plays without it (2026-11-24)
      confirm_text_ = status_ + " The game plays fine without its music.";
      std::printf("mw3: launcher: no music added: %s\n", status_.c_str());
      status_.clear(), confirm_go_ = 8, back = kConfirm;
    }
    const bool movies = served ? !files_pm_ && std::find(kit_status_[0].pending.begin(), kit_status_[0].pending.end(), "movies") !=
                                                   kit_status_[0].pending.end()
                               : r.movies > 0;  // Pirate's Moon has none
    if (r.ok && movies && (served ? movies_pending() : movies_to_convert() > 0)) {  // the movies just added, converted at once
      const std::string said = status_;
      convert_movies([this, back, said] { status_ = said, screen_ = back, layout(); });  // the dialog (kConfirm) after
      return;
    }
    screen_ = back, layout();
    if (back == kConfirm) nav_steps(SDL_getenv("MW3_LAUNCHER_NAV_DIALOG"));  // checks: the dialog's own keys
    return;
  }
  if (kind == Work::kLook && served) {  // b3: the game's look
    const rekit::SetupMessage& m = *served_said_;
    if (!r.ok) {
      status_ = r.summary, status_good_ = r.cancelled, screen_ = kSetupScreen;
      return layout();
    }
    if (m.in_place) {  // this folder's own copy: in use as it is
      const int i = files_pm_ ? 1 : 0;
      status_refresh(i);
      const std::string v = kit_status_[i].data.empty() ? "unknown" : kit_status_[i].data;
      if (files_pm_) return use_pm(m.where, own::found_words("", m.where, true));
      return use_install(m.where, v, own::found_words(v, m.where, false));
    }
    return found_plate(m.path.empty() ? pending_source_ : m.path, r.summary);
  }
  status_ = r.summary, status_good_ = r.ok;
  for (const std::string& w : r.warnings) status_ += " " + w;
  if (files_pm_ && kind == Work::kInstallIt) {  // Pirate's Moon's own folder, never MW3's install=
    if (r.ok) return use_pm(r.install_dir, status_);
    screen_ = kSetupScreen, layout();
    return;
  }
  if (r.ok && !r.install_dir.empty())  // the new copy first in the search below, not the start's mw3.cfg
    own::save_install(o_.write_dir, r.install_dir), o_.cfg_install = r.install_dir;
  find_install();
  if (kind == Work::kInstallIt && r.ok && install_.ok) return use_install(install_.dir, install_.version, status_);
  screen_ = install_.ok && r.ok ? kMain : kSetupScreen;
  layout();
}

// USE THESE GAME FILES? for a find (`path`, said in `words`): copied in by GO.
void Launcher::found_plate(const std::string& path, const std::string& words) {
  found_name_ = fs::path(path).filename().string();
  if (found_name_.empty()) found_name_ = path;  // a drive's top ("D:\\")
  found_sub_ = "Click to pick other files";
  std::printf("mw3: launcher: found plate: %s (%s)\n", found_name_.c_str(), words.c_str());
  files_words_ = words;
  files_into_ = games_place_[files_pm_ ? 1 : 0];  // where its card's INSTALL put the program, if it was asked
  files_confirm_text();
  confirm_go_ = 1;
  if (const char* to = SDL_getenv("MW3_LAUNCHER_FILES_TO"); to && *to) picked(kInstallTo, to);  // checks: CHANGE FOLDER's pick
  screen_ = kConfirm;
  if (pick_other_) {  // checks (MW3_LAUNCHER_PICK_OTHER, with MW3_LAUNCHER_PICKER=path:): passed over for the pick
    pick_other_ = false;
    layout();
    activate(kPickOther);
    return;
  }
  if (auto_go_) {
    auto_go_ = false;
    activate(kGo);
    return;
  }
  layout();
}

void Launcher::activate(int id) {
  if (id >= kGameInstall && id < kGameInstall + upgrades::update::game_count()) return install_game(id - kGameInstall);
  if (id >= kGameUpdate && id < kGameUpdate + upgrades::update::game_count()) return install_game(id - kGameUpdate);
  if (id >= kKitPlay && id < kKitPlay + upgrades::update::game_count()) {
    sounds_.play(Sounds::kSelect);
    files_pm_ = false;
    return run_kit(id - kKitPlay);
  }
  if (id >= kGear && id < kGear + upgrades::update::game_count()) return sounds_.play(Sounds::kSelect), open_game_page(id - kGear);
  if (screen_ == kGamePage) {  // its page (Escape is BACK; on the confirm, KEEP IT)
    sounds_.play(Sounds::kSelect);
    if (id == kBack && page_confirm_) id = kUninstallBack;
    switch (id) {
      case kBack: screen_ = kMain, status_.clear(); return layout();
      case kGameLink: return ask_file(kGameLink, nullptr, 0);
      case kGameUnlink: return unlink_game();
      case kGameFilesPick: return open_files(page_game_);
      case kGameUninstall:
      case kGameRemove: page_confirm_ = true, remove_linked_ = id == kGameRemove, status_.clear(); return layout();
      case kInstallFile: break;
      case kUninstallBack: page_confirm_ = false; return layout();
      case kUninstallGo:
      case kUninstallAll: return uninstall_go(id == kUninstallAll);
      default: return;
    }
  }
  if (id >= kCopy && id < kCopy + static_cast<int>(copies_.size()) && id < kCopy + kCopyRows) {  // a copy from GAME FILES' list
    sounds_.play(Sounds::kSelect);
    const Copy& c = copies_[id - kCopy];
    if (c.in_use) return;
    return look_at(c.path);  // "Use these game files?", then copied in as any find
  }
  sounds_.play(Sounds::kSelect);
  static const SDL_DialogFileFilter kZip[] = {{"The 1.2 patch (.zip, .exe)", "zip;exe"}, {"All files", "*"}};
  switch (id) {
    case kMw3:
      files_pm_ = false;
      if (games_on_ && o_.game_bin.empty()) {  // not installed: its card's INSTALL (games on demand)
        status_ = "MechWarrior 3 isn't installed yet: press INSTALL on its card.", status_good_ = false;
        layout();
        break;
      }
      if (install_.ok && install_.elsewhere) {  // copied in first
        if (SDL_getenv("MW3_LAUNCHER_GO")) auto_go_ = auto_ok_ = true;  // checks: its INSTALL (and notes' OK) pressed
        from_files_ = false, status_.clear(), look_at(install_.dir);
      } else if (install_.ok) play();
      else if (!status_for(0)) older_game(0, kMw3);  // b6: only its update finds its files
      else from_files_ = false, screen_ = kSetupScreen, status_.clear(), layout();  // its files to find first
      break;
    case kQuit: done_ = true; break;
    case kData:
      status_.clear();
      screen_ = kDataScreen, layout();
      break;
    case kDataPick: ask_folder(kDataPick); break;
    case kDataUsual: ask_move(files_usual()); break;
    case kMoveGo: start_move(); break;
    case kRemoveOld: {
      work_ = std::make_unique<Work>();
      work_->kind = Work::kRemoveData;
      const std::string old = move_from_, now = files_wd(), usual = files_usual();
      const std::vector<std::string> copied = move_copied_;
      work_->start("REMOVING THE OLD COPY", [old, now, usual, copied](const own::ProgressFn&) {
        own::Result r;
        const rekit::data_folder::Removed d = rekit::data_folder::remove_old(old, now, copied, usual, own::data_game());
        r.ok = d.error.empty() && d.failed.empty();
        r.summary = !d.error.empty()     ? d.error
                    : d.failed.empty()   ? "The old copy is removed."
                                         : std::to_string(d.failed.size()) + " files of the old copy could not be removed, in " + old + ".";
        return r;
      });
      screen_ = kWorking, layout();
      break;
    }
    case kVulkanAgain:
      vulkan_ = vulkan_check();
      std::printf("mw3: launcher: Vulkan checked again: %s\n", vulkan_ == Vulkan::kOk ? "found" : "still missing");
      if (vulkan_ != Vulkan::kOk) return show_vulkan();
      status_ = "Vulkan found: MechWarrior 3 can start.", status_good_ = true;
      screen_ = kMain, layout();
      break;
    case kKeepOld:
      status_ = "The old copy stays in " + move_from_ + ".", status_good_ = true;
      from_files_ = false, screen_ = kMain, layout();
      break;
    case kUpdateNotNow:
      update_box_ = false, status_.clear(), screen_ = kMain, layout();
      break;
    case kUpdateGo: {  // the platform's package and SHA256SUMS on the worker, the package checked against its line
      if (!update_) break;
      work_ = std::make_unique<Work>();
      work_->kind = Work::kUpdateDownload;
      const upgrades::update::Release release = update_->release;
      auto file = update_file_ = std::make_shared<std::string>();
      std::error_code ec;
      const std::string dir = (fs::temp_directory_path(ec) / "MW3-Rebuild-update").string();  // ours alone, never a settings folder
      work_->start("DOWNLOADING THE UPDATE", [release, dir, file](const own::ProgressFn& f) {
        const Downloaded d = download_update(release, dir, [&](double x) {
          return f(own::Progress{"Downloading MechWarrior 3 Rebuild " + upgrades::update::plain_version(release.tag), x * 0.9, ""});
        });
        own::Result r;
        r.ok = d.ok, r.cancelled = d.cancelled, r.summary = d.words;
        if (!d.ok) return r;
        std::printf("mw3: launcher: update: %s\n", d.words.c_str());
        f(own::Progress{"Installing", 0.95, ""});  // sitting 4: the platform's own way
        const Installed in = install_update(d.path);
        r.ok = in.ok, r.summary = in.words;
        *file = in.restart;
        return r;
      });
      screen_ = kWorking, layout();
      break;
    }
    case kSelfUpdate:  // the start's newer release: its notes and the question, as UPDATES would show them
      if (!self_update_ || work_) break;
      update_ = self_update_;
      show_update();
      break;
    case kUpdates: recheck(); break;
    case kUpdateLauncher:
      if (!self_update_ || work_) break;
      update_box_ = false, update_ = self_update_, read_update_notes(), activate(kUpdateGo);
      break;
    case kUpdateAll:
      if (!self_update_ || work_) break;
      update_box_ = false, update_queue_ = game_updates(), update_queue_self_ = true;
      next_update();
      break;
    case kPm:
      if (o_.pm_bin.empty()) break;
      if (pm_dir_.empty() && !pm_found_.empty()) {  // the search's find: offered to be copied in
        files_pm_ = true, from_files_ = false, status_.clear();
        if (SDL_getenv("MW3_LAUNCHER_GO")) auto_go_ = auto_ok_ = true;  // checks: its INSTALL pressed
        look_at(pm_found_);
      } else if (pm_dir_.empty()) {  // its files to find first, as MechWarrior 3's
        if (!status_for(1)) {  // b6: only its update finds its files
          older_game(1, kPm);
          break;
        }
        files_pm_ = true, from_files_ = false, status_.clear();
        screen_ = kSetupScreen, layout();
      } else {
        start_pm();
      }
      break;
    case kMw1:
      if (!mw1_remake_) break;  // no remake in this package: its button is greyed
      if (!mw1_needs().empty()) {  // MW1: the other two games first, said (before its own folder too)
        status_ = "MechWarrior 1 " + mw1_needs() + ".", status_good_ = false;
        layout();
      } else if (!kit_known_[2] && mw1_game_.empty()) {
        not_answered(2, kMw1);  // its status unknown: asked again, never "older" (mech1-remake-re #2)
      } else if (mw1_served() && !kit_set_up(2)) {
        open_kit_files(2, true);  // its MechWarrior (1989) through its own setup, on the launcher's screens (b5)
      } else if (!mw1_served() && mw1_game_.empty()) {
        mw1_too_old();
      } else {
        run_mw1_remake();
      }
      break;
    case kInstallFile:  // a game's own download, picked: checked against a SHA256SUMS beside it first
      status_ = "Pick a game's download: a .zip on Windows, a .dmg on the Mac, an .AppImage on Linux.", status_good_ = true;
      ask_file(kInstallFile, kPackageFiles, 1);
      break;
    case kInstallFileGo:  // ADD ANYWAY: a file nothing could check, the player's choice
      install_file_go();
      break;
    case kBack:
      if (screen_ == kConfirm && confirm_go_ == 6) {
        confirm_go_ = 0, screen_ = kMain, status_.clear(), layout();
        break;
      }
      if (screen_ == kNotes) return activate(kNotesOk);  // Escape on the notes: as OK
      if (o_.setup_only && screen_ == kSetupScreen && o_.choose_again && have_files())  // Shift's: on with these
        return files_pm_ ? use_pm(pm_dir_, pm_in_use()) : play();
      if (o_.setup_only && screen_ == kSetupScreen) done_ = true;  // a plain start's QUIT
      else if (screen_ == kConfirm && confirm_go_ == 3) return activate(kKeepOld);  // Escape: nothing removed
      else if (screen_ == kConfirm && confirm_go_ == 2) screen_ = kDataScreen, layout();
      else if (screen_ == kConfirm && confirm_go_ == 4) screen_ = kMain, layout();  // Vulkan's: the main screen
      else if (screen_ == kConfirm && confirm_go_ == 8) screen_ = disc_back_ == kSetupScreen ? kSetupScreen : kNotes, layout();
      else if (screen_ == kConfirm && confirm_go_ == 7) place_game_ = -1, screen_ = kMain, layout();  // INSTALL's where: nothing done
      else if (screen_ == kConfirm) screen_ = kSetupScreen, layout();
      else if (screen_ == kDataScreen) screen_ = kSetupScreen, status_.clear(), layout();
      else if (screen_ == kCopies) screen_ = kSetupScreen, status_.clear(), layout();
      else if (screen_ == kSetupScreen && files_page_ >= 0) from_files_ = false, open_game_page(files_page_);  // its page
      else screen_ = kMain, from_files_ = false, status_.clear(), layout();
      break;
    case kNotesOk:
      status_.clear();
      if (play_after_notes_ && pm_after_notes_) {
        play_after_notes_ = pm_after_notes_ = false;
        notes_refresh();  // MW3's again
        screen_ = kMain, layout();
        start_game(true);
      } else if (play_after_notes_) {
        play_after_notes_ = false;
        play();
      } else {
        screen_ = kMain, layout();
      }
      break;
    case kFind: search(); break;  // the usual places and the drives first, then the one picker
    case kPickFile:
    case kCopyPick:
      status_ = "Pick the copy to use: " + (kit_files_ >= 0 ? kit_ask_.pick_words : std::string("a disc image, a download, or Mech3.exe")) + ".";
      status_good_ = true;
      screen_ = kSetupScreen, layout();
      ask_file(kPoint, kGameFiles, kGameFilesCount);
      break;
    case kPickFolder:  // a folder where the picker takes files only: an installed copy's, the disc's
      ask_folder(screen_ == kConfirm ? kAddDisc : kPoint);
      break;
    case kPlayNoMusic:  // the no-music dialog: on into the game it was for
      status_.clear();
      if (play_after_notes_) return screen_ = kNotes, activate(kNotesOk);
      if (o_.setup_only && have_files()) return files_pm_ ? use_pm(pm_dir_, pm_in_use()) : play();
      screen_ = kMain, layout();
      if (files_pm_) start_pm();
      else play();
      break;
    case kPickOther:  // the found copy passed over: the one picker
      if (confirm_go_ == 8) {  // another than the one without music
        ask_file(kAddDisc, kDiscFiles, kDiscFilesCount);
        break;
      }
      status_ = "Pick the copy to use: " + (kit_files_ >= 0 ? kit_ask_.pick_words : std::string("a disc image, a download, or Mech3.exe")) + ".";
      status_good_ = true;
      screen_ = kSetupScreen, layout();
      ask_file(kPoint, kGameFiles, kGameFilesCount);
      break;
    case kAddDisc: {  // a disc in a drive first, then one picker: its image or any file of the disc
      const std::vector<std::string> discs;  // b6: the drives' look is the game's (its disc's files); the picker here
      if (!discs.empty()) {
        add_disc(discs.front());
      } else {
        status_ = "No disc in a drive. Pick its image (.cue .iso), a download (.zip) or any file of the disc.", status_good_ = true;
        layout();
        ask_file(kAddDisc, kDiscFiles, kDiscFilesCount);
      }
      break;
    }
    case kPatch: ask_file(kPatch, kZip, 2); break;  // unreachable: no plate (the patch is built in)
    case kInstallTo:  // CHANGE FOLDER: the game's folder, picked (picked: game_folder_in)
      status_ = "Pick where the game goes. A folder that holds other files gets a folder of the game's own inside it.", status_good_ = true;
      ask_folder(kInstallTo);
      break;
    case kPlaceGo: {  // the card's INSTALL, once asked where
      const int i = place_game_;
      if (i < 0 || i >= upgrades::update::game_count()) break;
      if (!place_.empty()) {
        std::string error;
        if (!sdlfe::games::claim_game_folder(place_, upgrades::update::game_at(i), &error)) {
          status_ = "Nothing was installed: " + error + ".", status_good_ = false;
          std::printf("mw3: launcher: %s\n", status_.c_str());
          layout();
          break;
        }
      }
      install_game_now(i);
      break;
    }
    case kGo: {
      if (kit_files_ >= 0) {  // the game claims its folder itself
        kit_install();
        break;
      }
      if (!files_into_.empty()) {  // the game's own folder, made the game's before anything goes in
        std::string error;
        if (!sdlfe::games::claim_game_folder(files_into_, upgrades::update::game_at(files_pm_ ? 1 : 0), &error)) {
          status_ = "Nothing was copied: " + error + ".", status_good_ = false;
          std::printf("mw3: launcher: %s\n", status_.c_str());
          layout();
          break;
        }
      }
      if (files_served()) {  // the game copies its files in (the folder claimed above, as it claims it too)
        std::vector<std::string> ask{"--install", pending_source_};
        if (!files_into_.empty()) ask.insert(ask.end(), {"--folder", files_into_});
        served_work(Work::kInstallIt, files_pm_ ? "INSTALLING PIRATE'S MOON" : "INSTALLING MECHWARRIOR 3", ask);
        break;
      }
      older_game(files_pm_ ? 1 : 0);  // b6: only a game that serves its setup copies its files in
      break;
    }
    case kCancel:
      if (work_) work_->cancel = true;
      break;
    default: break;
  }
}

void Launcher::nav(int action) {
  const int n = static_cast<int>(buttons_.size());
  const int at = focus_ >= 0 ? focus_ : hot_;
  if (action == 1 || action == -1) {
    int f = at;
    for (int i = 0; i < n; ++i) {
      f = ((f < 0 ? (action > 0 ? -1 : 0) : f) + action + n) % n;
      if (buttons_[f].enabled) break;
    }
    focus_ = f;
    if (f >= 0) sync_ui(), ui_.into_view(f);  // a panel scrolled out of the list comes into it
    if (f >= 0) sounds_.play(Sounds::kRollover);
  } else if (action == 2 && at >= 0 && at < n && buttons_[at].enabled) {
    activate(buttons_[at].id);
  } else if (action == 3) {
    activate(screen_ != kMain ? kBack : update_box_ ? kUpdateNotNow : kQuit);
  }
}

// A file or a folder dropped on the window (2026-11-24): what this screen's picker would have answered.
void Launcher::dropped(const std::string& path) {
  int what = 0;
  if (screen_ == kSetupScreen || screen_ == kCopies || (screen_ == kConfirm && confirm_go_ == 1)) what = kPoint;
  else if (screen_ == kNotes && notes_layout().add_y >= 0) what = kAddDisc;
  else if (screen_ == kConfirm && confirm_go_ == 8) what = kAddDisc;
  else if (screen_ == kGamePage && !page_confirm_) what = kGameLink;
  std::printf("mw3: launcher: dropped: %s%s\n", path.c_str(), what ? "" : " (nothing asked for here)");
  if (what) picked(what, path);
}

// The least a vsynced frame takes: the display's refresh, a tenth under so that vsync does the pacing; 60 Hz unknown.
Uint64 Launcher::display_frame() const {
  const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(win_));
  const float hz = m && m->refresh_rate >= 24.0f ? std::min(m->refresh_rate, 500.0f) : 60.0f;
  return static_cast<Uint64>(0.9e9 / hz);
}

LauncherChoice Launcher::run() {
  const bool shot = !o_.shot.empty();
  // Where the start's time goes ("pretty slow"), in the log.
  const Uint64 t0 = SDL_GetTicksNS();
  auto ms = [](Uint64 a, Uint64 b) { return static_cast<double>(b - a) / 1e6; };
  // Each game's program and its --status first (b2): the asks run beside the fonts and the rest, and are waited for
  // where the checks of MechWarrior 3's and Pirate's Moon's files were, so the first frame has every panel's lines.
  if (o_.pm) files_pm_ = true, o_.pm_write_dir = o_.write_dir, o_.cfg_pm_install = o_.cfg_install;  // mw3pm's own start
  starting_ = true;
  resolve_games();
  const std::string dir = assets_dir();
  const Uint64 t_art = SDL_GetTicksNS();
  const std::string font = font_path(dir);
  std::printf("mw3: launcher: font %s\n", font.empty() ? "none found: its text will be blank" : font.c_str());
  const std::string body_font = body_font_path(dir);
  std::printf("mw3: launcher: small lines' font %s\n", body_font.empty() ? "none found" : body_font.c_str());
  if (!ui_.init(dir, font, body_font)) std::fprintf(stderr, "mw3: launcher: its screens cannot be set up (RmlUi)\n");
  ui_.art_from(sdlfe::catalog::cache_dir(o_.write_dir));  // a fetched catalog's badges the package does not carry
  const Uint64 t_font = SDL_GetTicksNS();
  // Ours: the game draws with Vulkan on Linux, this window does not; said here, before PLAY.
  vulkan_ = vulkan_check();
  const bool vulkan_said = vulkan_ != Vulkan::kOk;
  status_collect();
  find_install();
  const Uint64 t_install = SDL_GetTicksNS();
  find_pm();
  pm_refresh();
  find_mw1(true);  // after the two it needs: its status is asked with their copies (b5), on its own thread
  if (!o_.shot.empty() || o_.play_mw1 || o_.play_pm || o_.play || quit_after_ > 0) asks_wait();  // checks, shots: known
  // Each game's program's own version, read from its file (the package's
  // programs come from different tags: mw3 and this one from main's, mw3pm
  // from pm's).
  mw3_program_ = o_.game_bin.empty() ? (games_on_ ? std::string() : std::string(MW3_VERSION)) : program_tag(0);
  if (!o_.pm_bin.empty()) pm_program_ = program_tag(1);
  if (mw1_remake_) mw1_program_ = program_tag(2);  // MechWarrior 1 Remake's, from a mw1-vX.Y.Z tag on mw1
  std::printf("mw3: launcher: programs: MechWarrior 3 %s", mw3_program_.empty() ? "unknown" : mw3_program_.c_str());
  if (!o_.pm_bin.empty()) std::printf(", Pirate's Moon %s", pm_program_.empty() ? "unknown" : pm_program_.c_str());
  if (mw1_remake_) std::printf(", MechWarrior 1 %s", mw1_program_.empty() ? "unknown" : mw1_program_.c_str());
  std::printf("\n");
  start_games_check();
  start_self_check();  // the launcher's own update, quietly (2026-11-23)
  std::printf("mw3: launcher: art %.0f ms, font %.0f ms, the statuses and the install search %.0f ms\n", ms(t0, t_art),
              ms(t_art, t_font), ms(t_font, t_install));
  sounds_on_ = o_.sound && !shot;
  // The first start's notes (no install= in mw3.cfg yet: PLAY saves one),
  // on their own screen before the main one; logged on every start.
  notes_refresh();
  const bool first_start = o_.cfg_install.empty() && !notes_.empty();
  if (shot) {
    if (vulkan_said && (o_.screen.empty() || o_.screen == "main")) show_vulkan();  // as the start below shows it
    if (o_.screen == "setup" || o_.screen == "pick" || o_.screen == "files")
      screen_ = kSetupScreen, from_files_ = o_.screen == "files";
    if (o_.screen == "pick") status_ = "Not found on this computer or in a disc drive. Pick it: a disc image, a download, or Mech3.exe.";
    if (o_.screen == "notes" || o_.screen == "disc") screen_ = kNotes;
    // Pirate's Moon's notes as its start shows them (start_pm), and with ADD FROM MY DISC's answer.
    if (o_.screen == "pmnotes" || o_.screen == "pmdisc") {
      files_pm_ = true;
      notes_ = pm_notes(nullptr);
      for (const std::string& n : notes_) std::printf("mw3: launcher: note: %s\n", n.c_str());
      screen_ = kNotes;
    }
    if (o_.screen == "pmdisc") status_ = "No disc in a drive. Pick its image (.cue .iso), a download (.zip) or any file of the disc.";
    if (o_.screen == "data") screen_ = kDataScreen;
    if (o_.screen == "found") {  // a find offered: its plate (click: other files), INSTALL, BACK
      screen_ = kConfirm, confirm_go_ = 1, found_name_ = "MechWarrior 3", found_sub_ = "Click to pick other files";
      files_words_ = "An installed copy of MechWarrior 3 version 1.2.";
      files_confirm_text();
    }
    if (o_.screen == "found")
      if (const char* to = SDL_getenv("MW3_LAUNCHER_FILES_TO"); to && *to) picked(kInstallTo, to);  // CHANGE FOLDER's pick, shown
    if (o_.screen == "place") {  // a card's first INSTALL asking where (MW3_LAUNCHER_INSTALL_TO: a pick shown)
      if (checks_[0].release.tag.empty()) checks_[0].release.tag = "v1.3.1";
      place_game_ = 0, place_ = games_place_[0], confirm_go_ = 7, screen_ = kConfirm;
      if (const char* to = SDL_getenv("MW3_LAUNCHER_INSTALL_TO"); to && *to) picked(kInstallTo, to);
    }
    if (o_.screen == "nomusic") {
      files_pm_ = true, screen_ = kConfirm, confirm_go_ = 8;
      confirm_text_ = "That disc image has no CD audio tracks, so there is no music in it. The game plays fine without its music.";
    }
    if (o_.screen == "update") {  // the check (MW3_UPDATE_API in shots) and a newer release's screen
      update_ = std::make_shared<upgrades::update::Verdict>(check_for_updates(MW3_VERSION));
      if (update_->kind == upgrades::update::Verdict::kNewer) show_update();
    }
    if (o_.screen == "updatebox") recheck();  // CHECK FOR UPDATES pressed (MW3_UPDATE_API)
    if (o_.screen == "disc") status_ = "No disc in a drive. Pick its image (.cue .iso), a download (.zip) or any file of the disc.";
    if (o_.screen.rfind("game:", 0) == 0)  // a game's page (its gear): "game:" and its key
      for (int i = 0; i < upgrades::update::game_count(); ++i)
        if (o_.screen.substr(5) == upgrades::update::game_info(upgrades::update::game_at(i)).key) open_game_page(i);
    if (o_.screen == "chifiles") open_kit_files(4, false);  // a kit game's find screen (MW3_CHI_BIN)
    if (o_.screen == "copies" || o_.screen == "pmcopies")  // GAME FILES' list, MW3's or Pirate's Moon's
      files_pm_ = o_.screen == "pmcopies", from_files_ = true, find_copies(), screen_ = kCopies;
    for (int i = 0; i < upgrades::update::game_count(); ++i)  // a game's page (its gear): game:<key>, game:<key>:uninstall
      if (const std::string k = std::string("game:") + upgrades::update::game_info(upgrades::update::game_at(i)).key;
          o_.screen == k || o_.screen == k + ":uninstall")
        open_game_page(i), page_confirm_ = o_.screen != k;
    if (o_.screen == "work") {
      screen_ = kWorking;
      work_ = std::make_unique<Work>();
      work_->title = "INSTALLING MECHWARRIOR 3";
      work_->progress = {"Copying the movies", 0.62, "VIDEO\\C1M3.AVI"};
    }
    layout();
    if (!buttons_.empty()) hot_ = 0;
    const Uint64 td = SDL_GetTicksNS();
    // Drawn without a window; MW3_LAUNCHER_SHOT_SIZE (checks): as in a window of that many pixels (1708x960).
    int sw = W, sh = H;
    if (const char* size = SDL_getenv("MW3_LAUNCHER_SHOT_SIZE")) std::sscanf(size, "%dx%d", &sw, &sh);
    SDL_Surface* canvas = SDL_CreateSurface(std::clamp(sw, 64, 8192), std::clamp(sh, 64, 8192), SDL_PIXELFORMAT_XRGB8888);
    ren_ = canvas ? SDL_CreateSoftwareRenderer(canvas) : nullptr;
    if (!ren_ || !ui_.attach(ren_)) std::fprintf(stderr, "mw3: launcher: its screens cannot be drawn\n");
    draw();
    if (const char* y = SDL_getenv("MW3_LAUNCHER_LIST_Y")) ui_.scroll_list(static_cast<float>(std::atof(y)));  // checks: a shot scrolled
    draw();
    SDL_RenderPresent(ren_);
    if (hot_ >= 0)
      std::printf("mw3: launcher: hover %s:%s\n", buttons_[hot_].log_name().c_str(), buttons_[hot_].enabled ? " edge" : "");
    std::printf("mw3: launcher: first frame drawn %.0f ms after the start (its draw %.1f ms)\n", ms(t0, SDL_GetTicksNS()),
                ms(td, SDL_GetTicksNS()));
    SDL_SaveBMP(canvas, o_.shot.c_str());
    ui_.detach();
    SDL_DestroyRenderer(ren_), ren_ = nullptr;
    SDL_DestroySurface(canvas);
    work_.reset();
    return {};
  }
  // A game without its files opens the find screen only when it is chosen;
  // a plain mw3 start that found none shows that screen alone.
  if (o_.setup_only) screen_ = kSetupScreen;
  else if (install_.ok && !install_.elsewhere && first_start) screen_ = kNotes, mw3_notes_seen();  // a find elsewhere: after its copy
  if (o_.choose_again && SDL_getenv("MW3_LAUNCHER_GO")) auto_go_ = auto_ok_ = true;  // checks: a pick's INSTALL (and notes' OK)
  if (o_.choose_again && have_files())  // Shift at a plain start: what is in use, and the way on
    status_ = "In use: " + files_dir() + ". FIND THE GAME to choose other files, or PLAY to go on.", status_good_ = true;
  if (vulkan_said) show_vulkan();  // ours: before anything else, so PLAY never reaches the game's box
  layout();
  // A game's own start is not high pixel density: the game takes that window (one window into the
  // game), and its sizes are the window's points (SdlWindow, SdlInput).
  SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
  if (!o_.setup_only && !o_.pm) flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (o_.fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
  if (o_.hidden) flags |= SDL_WINDOW_HIDDEN;
  win_ = SDL_CreateWindow(o_.setup_only || o_.pm ? (o_.pm ? "Pirate's Moon" : "MechWarrior 3") : "BattleTech", o_.win_w, o_.win_h, flags);
  if (!win_ || !(ren_ = SDL_CreateRenderer(win_, nullptr)) || !ui_.attach(ren_)) {
    std::fprintf(stderr, "mw3: launcher: no window (%s): straight to the game\n", SDL_GetError());
    return install_.ok ? LauncherChoice{LauncherChoice::kPlay, install_.dir} : LauncherChoice{};
  }
  // Drawn while its edge is dragged too: the system holds the loop below then (the Mac, Windows) and only asks for the
  // window's picture.
  SDL_AddEventWatch(&Launcher::exposed, this);
  // Paced by the display ("pretty slow"): a fixed SDL_Delay(15) was
  // 16 to 31 ms on Windows' 15.6 ms timer, and no vsync let frames tear.
  // MW3_LAUNCHER_NOVSYNC (checks): the timed sleep instead, to compare.
  vsync_ = !SDL_getenv("MW3_LAUNCHER_NOVSYNC") && SDL_SetRenderVSync(ren_, 1);
  if (vsync_ && SDL_getenv("MW3_LAUNCHER_UNPACED")) SDL_SetRenderVSync(ren_, 0);  // checks: a present that does not wait, as a window not on screen
  SDL_InitSubSystem(SDL_INIT_GAMEPAD);  // a pad steps through the plates too (nav)
  if (o_.play) auto_ok_ = true, activate(kMw3);  // its notes' OK pressed too
  // Checks: keys or a pad as a list ("next,next,press"), each step logged; with MW3_LAUNCHER_NAV_LATE, once the
  // start's own update check has answered (its footer button is there to reach).
  if (!SDL_getenv("MW3_LAUNCHER_NAV_LATE")) nav_steps();
  // Checks: GAME FILES opened on Pirate's Moon, or MechWarrior 1's, as from its page.
  if (const char* g = SDL_getenv("MW3_LAUNCHER_FILES"); g && std::strcmp(g, "pm") == 0) open_files(1);
  else if (g && std::strcmp(g, "mw1") == 0) open_files(2);
  // Checks: a pick as if made in the find screen's dialog (after any NAV
  // steps: GAME FILES), INSTALL and the notes' OK pressed.
  if (const char* to = SDL_getenv("MW3_LAUNCHER_MOVE_TO")) {  // checks: the data folder's move, its MOVE pressed
    from_files_ = true;
    picked(kDataPick, to);
    if (screen_ == kConfirm && confirm_go_ == 2) activate(kMoveGo);
  }
  if (const char* add = SDL_getenv("MW3_LAUNCHER_ADD")) add_disc(add);  // checks: ADD FROM MY DISC's pick
  if (!o_.look_first.empty()) {  // a plain start's find, offered to be copied in
    if (SDL_getenv("MW3_LAUNCHER_GO")) auto_go_ = auto_ok_ = true;  // checks: its INSTALL (and notes' OK) pressed
    look_at(o_.look_first);
  }
  const char* pick = SDL_getenv("MW3_LAUNCHER_PICK");
  // --launcher-pm with none yet and a pick: its find screen, the pick installed, then started.
  const bool pm_pick = o_.play_pm && !o_.pm_bin.empty() && pm_dir_.empty() && pick;
  if (pm_pick) auto_play_ = true, activate(kPm), o_.quit_after_game = true;
  if (pick && (screen_ == kSetupScreen || !install_.ok))  // MW3_LAUNCHER_NO_PLAY (checks): the game's button left to the player
    auto_go_ = auto_ok_ = true, auto_play_ = !SDL_getenv("MW3_LAUNCHER_NO_PLAY"), look_at(pick);
  if (o_.play_mw1) {
    // checks: MECHWARRIOR 1 pressed; its own setup's screens (b5), with MW3_LAUNCHER_PICK as the pick, then the game
    if (pick && mw1_served() && !kit_set_up(2)) auto_go_ = true;
    activate(kMw1);
    if (pick && kit_files_ == 2 && screen_ == kSetupScreen) kit_look(pick);
    done_ = !asking_ && !work_ && !any_checking();
    quit_after_mw1_ = !done_;  // a pick, an install or its status still to come: ended once done (the loop below)
  }
  if (o_.play_pm && !pm_pick) {  // never the picker: a check has nobody to answer it
    if (pm_dir_.empty() && !pm_found_.empty() && SDL_getenv("MW3_LAUNCHER_GO"))  // checks: the find offered, INSTALL
      auto_play_ = true, o_.quit_after_game = true, activate(kPm);           // pressed, copied in, then started
    else if (pm_dir_.empty()) done_ = true;
    else auto_ok_ = true, activate(kPm), o_.quit_after_game = true, done_ = !running(1);  // its notes' OK; refused: quit
  }
  while (!done_) {
    for (bool& f : status_fresh_) f = false;  // a status asked again when a panel's lines are made after now (b2)
    asks_collect();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      SDL_ConvertEventToRenderCoordinates(ren_, &e);
      if (e.type == SDL_EVENT_QUIT) done_ = true;
      if (asking_ && !ask_shown_ && e.type == SDL_EVENT_WINDOW_FOCUS_LOST) {  // the panel up (a sheet takes the focus)
        ask_shown_ = true;
        std::printf("mw3: launcher: the picker took the focus %.0f ms after it was asked for\n",
                    static_cast<double>(SDL_GetTicksNS() - ask_at_) / 1e6);
      }
      if (any_starting()) continue;  // a game is starting: the launcher waits for its first frame
      if (asking_ || screen_ == kWorking) {
        if (screen_ == kWorking && e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
          sync_ui();
          ui_.mouse_move(e.button.x, e.button.y);
          if (const int h = ui_.over(); h >= 0) activate(buttons_[h].id);
        }
        continue;
      }
      if (e.type == SDL_EVENT_DROP_FILE && e.drop.data) {  // a file or a folder dropped: as picked on this screen
        dropped(e.drop.data);
        continue;
      }
      if (e.type == SDL_EVENT_MOUSE_MOTION) {
        if (ui_stale_) sync_ui();
        ui_.mouse_move(e.motion.x, e.motion.y);
        const int h = ui_.over();
        if (h != hot_) {
          hot_ = h, focus_ = -1;
          if (h >= 0 && buttons_[h].enabled) sounds_.play(Sounds::kRollover);
        }
      } else if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
        ui_.mouse_leave();
      } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (ui_stale_) sync_ui();
        ui_.mouse_move(e.button.x, e.button.y);
        ui_.mouse_button(true);
        follow_ = ui_.over() < 0;  // the scrollbar, it may be
      } else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
        if (ui_stale_) sync_ui();
        ui_.mouse_button(false);
        // A press, never a scrollbar drag's end over a button.
        if (!follow_ && hot_ >= 0 && buttons_[hot_].enabled) activate(buttons_[hot_].id);
      } else if (e.type == SDL_EVENT_MOUSE_WHEEL) {  // the panels, the notes
        ui_.mouse_wheel(e.wheel.y);
        follow_ = screen_ == kMain;
      } else if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
        SDL_OpenGamepad(e.gdevice.which);  // closed with the subsystem
      } else if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        // Keys and a pad alike: the arrows (and Tab) or the d-pad step through
        // the plates, Enter or A presses, Escape or B goes back.
        enum { kNone, kNext, kPrev, kPress, kEscape } a = kNone;
        if (e.type == SDL_EVENT_KEY_DOWN) {
          const SDL_Keycode k = e.key.key;
          if (k == SDLK_DOWN || k == SDLK_RIGHT || k == SDLK_TAB) a = kNext;
          else if (k == SDLK_UP || k == SDLK_LEFT) a = kPrev;
          else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) a = kPress;
          else if (k == SDLK_ESCAPE) a = kEscape;
        } else {
          const int b = e.gbutton.button;
          if (b == SDL_GAMEPAD_BUTTON_DPAD_DOWN || b == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) a = kNext;
          else if (b == SDL_GAMEPAD_BUTTON_DPAD_UP || b == SDL_GAMEPAD_BUTTON_DPAD_LEFT) a = kPrev;
          else if (b == SDL_GAMEPAD_BUTTON_SOUTH || b == SDL_GAMEPAD_BUTTON_START) a = kPress;
          else if (b == SDL_GAMEPAD_BUTTON_EAST || b == SDL_GAMEPAD_BUTTON_BACK) a = kEscape;
        }
        nav(a == kNext ? 1 : a == kPrev ? -1 : a == kPress ? 2 : a == kEscape ? 3 : 0);
      }
    }
    if (asking_ && !ask_pending_) {
      int what = 0;
      std::string path;
      bool answered = false, cancelled = false;
      {
        std::lock_guard<std::mutex> g(pick_.m);
        answered = pick_.answered, cancelled = pick_.cancelled, what = pick_.what, path = pick_.path;
      }
      if (answered) {
        asking_ = false;
        std::printf("mw3: launcher: the picker answered (%s) %.0f ms after it was asked for\n", cancelled ? "cancelled" : "a pick",
                    static_cast<double>(SDL_GetTicksNS() - ask_at_) / 1e6);
        if (!cancelled) picked(what, path);
      } else {
        // While the panel is up the launcher stops drawing and waits in the
        // run loop: on the Mac the panel is another process answering through
        // the main run loop, which a vsync frame loop served once a frame
        // (the picker took a long time to open).
        SDL_WaitEventTimeout(nullptr, 50);
        continue;
      }
    }
    if (work_ && work_->done) finish_work();
    take_games_check();
    take_self_check();
    for (int i = 0; i < upgrades::update::kMaxGames && !work_; ++i)  // an UPDATE that waited for its game's end
      if (update_after_[i] && !running(i)) update_after_[i] = false, install_game(i);
    if (const char* late = SDL_getenv("MW3_LAUNCHER_NAV_LATE"); late && !games_check_ && !self_check_ && !nav_late_) {  // checks: the steps once the start's checks have answered (a path: and that file is there)
      std::error_code ec;
      std::fflush(stdout);  // the check reads it as it goes
      if (late[0] != '/' || fs::exists(late, ec)) nav_late_ = true, nav_steps();
    }
    if (games_on_ && !install_file_asked_ && SDL_getenv("MW3_LAUNCHER_INSTALL_FILE") && screen_ == kMain && !work_)
      install_file_asked_ = true, activate(kInstallFile);  // checks: INSTALL FROM A DOWNLOADED FILE pressed (the picker's MW3_LAUNCHER_PICKER)
    if (quit_after_mw1_ && !asking_ && !ask_pending_ && !work_ && !any_running() && !any_checking()) done_ = true;
    watch_game();
    const Uint64 ta = SDL_GetTicksNS();
    draw();
    const Uint64 tb = SDL_GetTicksNS();
    SDL_RenderPresent(ren_);
    const Uint64 tc = SDL_GetTicksNS();
    if (!frames_) std::printf("mw3: launcher: first frame shown %.0f ms after the start\n", ms(t0, tc));
    if (ask_pending_) {
      std::printf("mw3: launcher: the picker's notice shown\n");
      open_picker();
    }
    mouse_steps();
    ++frames_, draw_ms_ += ms(ta, tb), present_ms_ += ms(tb, tc);
    if (quit_after_ > 0 && frames_ >= quit_after_ && !any_checking()) done_ = true;  // MW3_LAUNCHER_FRAMES (checks)
    // No vsync: the rest of a 60th of a second, to the nanosecond. With it, never faster than a little over the display:
    // macOS does not hold a present back for a window not on screen (behind a fullscreen game), and at some 950 frames a
    // second its drawables piled up to tens of gigabytes (2026-11-25, docs/launcher.md, Pacing).
    const Uint64 spent = SDL_GetTicksNS() - ta, frame = vsync_ ? display_frame() : 1000000000ull / 60;
    if (spent < frame) SDL_DelayPrecise(frame - spent);
    // Covered or minimised: a few frames a second, any event at once.
    // A game running and this window not the one in use (2026-11-26: the launcher stays open beside the games): the
    // same, wherever the system does not say a full-screen game covers it.
    if (!o_.hidden && (SDL_GetWindowFlags(win_) & (SDL_WINDOW_OCCLUDED | SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN) ||
                       (any_running() && !(SDL_GetWindowFlags(win_) & SDL_WINDOW_INPUT_FOCUS))))
      SDL_WaitEventTimeout(nullptr, 100);
  }
  if (const char* path = SDL_getenv("MW3_LAUNCHER_FRAME_SHOT")) save_frame(path);  // checks: the last frame, as this renderer draws it
  if (frames_) {
    const double secs = ms(t0, SDL_GetTicksNS()) / 1000.0;
    std::printf("mw3: launcher: %d frames, %.1f a second; the draw %.2f ms, the present %.2f ms a frame (%s)\n", frames_,
                frames_ / std::max(secs, 0.001), draw_ms_ / frames_, present_ms_ / frames_, ren_ ? SDL_GetRendererName(ren_) : "-");
    std::printf("mw3: launcher: paced by %s\n", vsync_ ? "vsync" : "a timed sleep to 60 a second");
  }
  if (work_) work_->cancel = true;
  for (Run& r : runs_) {  // quit while a game runs: it goes on, we stop watching
    if (!r.p) continue;
    if (r.mount) mount_wait(r);  // FIX (A): not from under a game paging from this AppImage
    else SDL_DestroyProcess(r.p), r = Run();
  }
  if (choice_.kind == LauncherChoice::kPlay) {
    own::save_install(o_.write_dir, choice_.install);
    // The window goes on to the game, its renderer freed first (a window
    // drawn by an SDL renderer cannot be the GPU device's too).
    SDL_RemoveEventWatch(&Launcher::exposed, this);
    ui_.detach();
    if (ren_) SDL_DestroyRenderer(ren_), ren_ = nullptr;
    // Its window's events so far are the launcher's: a focus lost to another
    // app would reach the game as a deactivation and leave it waiting.
    SDL_FlushEvents(SDL_EVENT_WINDOW_FIRST, SDL_EVENT_WINDOW_LAST);
    choice_.window = win_;
    win_ = nullptr;
    if (choice_.window) std::printf("mw3: launcher: its window goes on to the game\n");
  }
  return choice_;
}

}  // namespace

// The games catalog (b7): the cached fetched one in use from the start, before any game is looked up; a newer one
// fetched meanwhile, on a thread, for the next start (a check run fetches first, from a stand-in only).
// Why game `key` is on this computer, for the catalog's kept entries: its record in its folder (the games folder's, or
// the one its INSTALL picked), or a link to a program still there.
static std::string game_here(const std::string& write_dir, const std::string& games_root, const std::string& key) {
  std::error_code ec;
  const std::string place = cfg_path(write_dir, "games_" + key);
  const std::string root = place.empty() ? games_root : place;
  if (!root.empty()) {
    std::ifstream f(fs::path(root) / key / upgrades::update::kInstalledName);
    const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    upgrades::update::InstalledGame rec;
    if (!text.empty() && upgrades::update::parse_installed(text, &rec) && rec.game == key) return "installed here";
  }
  const std::string link = cfg_path(write_dir, "link_" + key);
  return !link.empty() && fs::is_regular_file(link, ec) ? "linked here" : "";
}

LauncherChoice run_launcher(const LauncherOptions& o) {
  const std::string games_root = sdlfe::games::root(o.hidden);
  const catalog::Here here{o.write_dir, MW3_VERSION, assets_dir(),
                           [w = o.write_dir, games_root](const std::string& key) { return game_here(w, games_root, key); }};
  const bool check_run = o.hidden || !o.shot.empty();
  catalog::at_start(here, check_run);
  std::thread fetch;
  LauncherChoice c;
  {
    Launcher l(o);  // stop_asking(false): the fetch after it
    if (!check_run && !o.write_dir.empty()) fetch = std::thread([here] { catalog::fetch(here, std::getenv("MW3_CATALOG_URL") != nullptr); });  // checks: a stand-in's file://
    c = l.run();
  }  // stop_asking(): a fetch still asking ends at once
  if (fetch.joinable()) fetch.join();
  return c;
}

}  // namespace sdlfe
