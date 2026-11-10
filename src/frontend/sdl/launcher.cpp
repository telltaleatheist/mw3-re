#include "frontend/sdl/launcher.h"
#include "frontend/sdl/data_folder_start.h"
#include "frontend/sdl/font_search.h"
#include "frontend/sdl/launcher_notes.h"
#include "frontend/sdl/movie_import.h"
#include "frontend/sdl/text_fit.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

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
#include <thread>

#include "frontend/sdl/sdl_install.h"
#include "gamez/zutil/zutl_zar.h"
#include "gamez/zrender/zrndr_ripple.h"
#include "mech3/menu_ripple.h"
#include "gamez/zrender/zrndr_tint.h"
#include "mech3/game_setup.h"
#include "mech3/mods.h"
#include "mech3/mod_install.h"
#include "upgrades/mods/mod_export.h"
#include "frontend/sdl/sdl_mods.h"
#include "mech3/source_map.h"
#include "gamez/zutil/zutl_path.h"

namespace sdlfe {

namespace {
// How long the launcher waits for the game's first frame before it says so
// on its status line: 20 s (MW3_LAUNCHER_READY_MS for the checks).
double ready_wait_ms() {
  const char* e = SDL_getenv("MW3_LAUNCHER_READY_MS");
  return e ? std::atof(e) : 20000.0;
}
namespace fs = std::filesystem;
// 16:9 (Owen, 2026-11-08): the art across the width, the plates and boxes a
// 640-wide block in the middle (kX in), as laid out for 640x480 before.
constexpr int W = 854, H = 480, kX = (W - 640) / 2;
constexpr int kBadge = 76;  // a game's badge across, on the 640x480 canvas

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

SDL_Surface* load_png(const std::string& dir, const char* name) {
  if (dir.empty()) return nullptr;
  SDL_Surface* s = SDL_LoadPNG((fs::path(dir) / name).string().c_str());
  if (!s) return nullptr;
  SDL_Surface* c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888);
  SDL_DestroySurface(s);
  return c;
}
// The picture's lit part (brighter than `thresh`), cropped; with `alpha`, its
// near-black made see-through (the generated art is on black).
SDL_Surface* cut_out(SDL_Surface* s, int thresh, bool alpha) {
  if (!s) return nullptr;
  int x0 = s->w, y0 = s->h, x1 = -1, y1 = -1;
  for (int y = 0; y < s->h; ++y) {
    const uint32_t* row = reinterpret_cast<const uint32_t*>(static_cast<uint8_t*>(s->pixels) + y * s->pitch);
    for (int x = 0; x < s->w; ++x) {
      const uint32_t p = row[x];
      const int lum = std::max({int(p >> 16 & 0xff), int(p >> 8 & 0xff), int(p & 0xff)});
      if (lum > thresh) x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
    }
  }
  if (x1 < x0) return s;
  SDL_Surface* c = SDL_CreateSurface(x1 - x0 + 1, y1 - y0 + 1, SDL_PIXELFORMAT_ARGB8888);
  const SDL_Rect r{x0, y0, c->w, c->h};
  SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);
  SDL_BlitSurface(s, &r, c, nullptr);
  SDL_DestroySurface(s);
  if (alpha) {
    for (int y = 0; y < c->h; ++y) {
      uint32_t* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(c->pixels) + y * c->pitch);
      for (int x = 0; x < c->w; ++x) {
        const uint32_t p = row[x];
        const int lum = std::max({int(p >> 16 & 0xff), int(p >> 8 & 0xff), int(p & 0xff)});
        const int a = std::clamp((lum - 18) * 255 / 42, 0, 255);
        row[x] = (p & 0xffffffu) | (static_cast<uint32_t>(a) << 24);
      }
    }
    SDL_SetSurfaceBlendMode(c, SDL_BLENDMODE_BLEND);
  }
  return c;
}
SDL_Surface* scaled(SDL_Surface* s, int w, int h) {
  if (!s) return nullptr;
  SDL_Surface* r = SDL_ScaleSurface(s, w, h, SDL_SCALEMODE_LINEAR);
  if (r) SDL_SetSurfaceBlendMode(r, SDL_BLENDMODE_BLEND);
  return r;
}

struct Art {
  SDL_Surface *stars = nullptr, *metal = nullptr, *plate = nullptr, *title = nullptr, *emblem = nullptr;
  SDL_Surface* badge[3] = {};  // the games' round badges: MechWarrior 3, Pirate's Moon, MechWarrior (1989)
  ~Art() {
    for (SDL_Surface* s : {stars, metal, plate, title, emblem, badge[0], badge[1], badge[2]})
      if (s) SDL_DestroySurface(s);
  }
  void load(const std::string& dir) {
    if (SDL_Surface* s = load_png(dir, "bg_stars.png")) stars = scaled(s, s->w * H / s->h, H), SDL_DestroySurface(s);
    if (SDL_Surface* s = load_png(dir, "bg_metal.png")) metal = scaled(s, s->w * 240 / s->h, 240), SDL_DestroySurface(s);
    if (stars) SDL_SetSurfaceBlendMode(stars, SDL_BLENDMODE_NONE);
    if (metal) SDL_SetSurfaceBlendMode(metal, SDL_BLENDMODE_NONE), SDL_SetSurfaceColorMod(metal, 190, 190, 190);
    plate = cut_out(load_png(dir, "plate.png"), 24, false);
    if (SDL_Surface* t = cut_out(load_png(dir, "title.png"), 24, true)) title = scaled(t, 420, t->h * 420 / t->w), SDL_DestroySurface(t);
    if (SDL_Surface* e = cut_out(load_png(dir, "emblem.png"), 24, true)) emblem = scaled(e, 72, 72), SDL_DestroySurface(e);
    const char* const badges[3] = {"badge_mw3.png", "badge_pm.png", "badge_mw1.png"};
    for (int i = 0; i < 3; ++i)
      if (SDL_Surface* b = cut_out(load_png(dir, badges[i]), 24, true)) badge[i] = scaled(b, kBadge, kBadge), SDL_DestroySurface(b);
  }
};

// ---- text ----

struct Text {
  std::string path;
  std::map<int, TTF_Font*> fonts;
  bool ok = false;
  ~Text() {
    for (auto& [k, f] : fonts) TTF_CloseFont(f);
    if (ok) TTF_Quit();
  }
  void open(const std::string& p) {
    path = p;
    ok = !p.empty() && TTF_Init();
  }
  TTF_Font* font(int size) {
    if (!ok) return nullptr;
    auto it = fonts.find(size);
    if (it != fonts.end()) return it->second;
    return fonts[size] = TTF_OpenFont(path.c_str(), static_cast<float>(size));
  }
  // `s` as drawn and measured: a narrow space doubled (widen_spaces).
  std::string shown(TTF_Font* f, int size, const std::string& s) {
    int adv = 0;
    if (!TTF_GetGlyphMetrics(f, ' ', nullptr, nullptr, nullptr, nullptr, &adv)) return s;
    return widen_spaces(s, adv, size);
  }
  // `s` at (x, y): align -1 left, 0 centred, 1 right of x; y its middle.
  void draw(SDL_Surface* dst, const std::string& s, int size, SDL_Color c, int x, int y, int align = 0) {
    TTF_Font* f = font(size);
    if (!f || s.empty()) return;
    const std::string text = shown(f, size, s);
    SDL_Surface* t = TTF_RenderText_Blended(f, text.c_str(), 0, c);
    if (!t) return;
    SDL_Rect r{x, y - t->h / 2, t->w, t->h};
    if (align == 0) r.x -= t->w / 2;
    if (align > 0) r.x -= t->w;
    SDL_Surface* shadow = TTF_RenderText_Blended(f, text.c_str(), 0, SDL_Color{0, 0, 0, 255});
    if (shadow) {
      SDL_Rect sr{r.x + 1, r.y + 2, r.w, r.h};
      SDL_BlitSurface(shadow, nullptr, dst, &sr), SDL_DestroySurface(shadow);
    }
    SDL_BlitSurface(t, nullptr, dst, &r);
    SDL_DestroySurface(t);
  }
  // `s` shortened from the middle with "..." until it is `width` across; its
  // first `keep` characters stay whole (a folder's start and end both show).
  std::string fit(const std::string& s, int size, int width, size_t keep = 0) {
    TTF_Font* f = font(size);
    if (!f) return s;
    auto w_of = [&](const std::string& t) {
      int w = 0, h = 0;
      TTF_GetStringSize(f, shown(f, size, t).c_str(), 0, &w, &h);
      return w;
    };
    return fit_middle(s, keep, width, w_of);
  }
  // Wrapped to `width`, lines centred on x from y; the height drawn.
  int draw_wrapped(SDL_Surface* dst, const std::string& s, int size, SDL_Color c, int x, int y, int width) {
    TTF_Font* f = font(size);
    if (!f) return 0;
    std::vector<std::string> lines;
    std::string line, word;
    auto fits = [&](const std::string& t) {
      int w = 0, h = 0;
      TTF_GetStringSize(f, shown(f, size, t).c_str(), 0, &w, &h);
      return w <= width;
    };
    auto flush_word = [&] {
      if (word.empty()) return;
      const std::string tryl = line.empty() ? word : line + " " + word;
      if (!fits(tryl) && !line.empty()) lines.push_back(line), line = word;
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
    const int lh = TTF_GetFontLineSkip(f);
    for (size_t i = 0; i < lines.size(); ++i) draw(dst, lines[i], size, c, x, y + static_cast<int>(i) * lh);
    return static_cast<int>(lines.size()) * lh;
  }
};

const SDL_Color kWhite{228, 230, 232, 255}, kAmber{255, 176, 64, 255}, kGrey{120, 124, 130, 255}, kDim{190, 190, 180, 255};

// The one picker's filters (Owen: one option): the sources first, any file
// as well (an installed copy or the disc's folder by one of its files).
const SDL_DialogFileFilter kGameFiles[] = {
    {"Your game: a disc image, a download, or Mech3.exe", "iso;cue;bin;img;zip;7z;rar;tar;gz;tgz;xz;exe"},
    {"All files", "*"}};
const SDL_DialogFileFilter kDiscFiles[] = {{"Your disc: its image (.cue, .iso) or any file of it", "cue;iso;bin;img"},
                                           {"All files", "*"}};

// The find screen's words (Owen, 2026-11-02): what the game needs, and that
// nothing of the player's is changed.
const char* const kFindWords =
    "MechWarrior 3 needs your own copy of the game. Show us your disc, a disc image or download (.iso, .cue, .zip), "
    "or a folder where it's installed. We copy what the game needs into your user folder; your disc and any "
    "installed copy are left untouched.";
const char* const kFindWordsPm =
    "Pirate's Moon needs your own copy of the game. Show us its disc, a disc image or download (.iso, .cue, .zip), "
    "or a folder where it's installed. We copy what the game needs into its own user folder; your disc and any "
    "installed copy are left untouched.";

// The main menu's own sounds (sounds.zrd's MAINMENU set: menu_rollover,
// sfx_button2.wav at 0.7; menu_select, sfx_button6.wav), read from the
// player's sound archive as zSnd reads it; none without an install (we ship
// no sounds of our own).
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
  bool load(const std::string& install) {
    static const char* const kFiles[2] = {"sfx_button2.wav", "sfx_button6.wav"};
    static const float kGain[2] = {0.7f, 1.0f};
    for (const char* arc : {"soundsL.zbd", "soundsH.zbd"}) {
      zutl_zar::Archive a;
      zutl_zar::init(a);
      const std::string path = zutl::any_case(install, std::string("zbd/") + arc);
      if (path.empty() || !fs::is_regular_file(path) || !zutl_zar::open(a, path.c_str())) {
        zutl_zar::destroy(a);
        continue;
      }
      int got = 0;
      for (int i = 0; i < 2; ++i) {
        const int e = zutl_zar::find(a, kFiles[i]);
        uint32_t size = 0;
        if (e < 0) continue;
        zutl_zar::read(a, e, nullptr, &size);
        std::vector<Uint8> bytes(size);
        if (!size || zutl_zar::read(a, e, bytes.data(), &size) != 0) continue;
        SDL_AudioSpec spec{};
        if (!SDL_LoadWAV_IO(SDL_IOFromConstMem(bytes.data(), bytes.size()), true, &spec, &audio[i], &len[i])) continue;
        stream[i] = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        // MW3_LAUNCHER_MUTE (checks): loaded and played at no volume.
        if (stream[i]) SDL_SetAudioStreamGain(stream[i], SDL_getenv("MW3_LAUNCHER_MUTE") ? 0.0f : kGain[i]), ++got;
      }
      zutl_zar::close(a);
      zutl_zar::destroy(a);
      if (got) {
        std::printf("mw3: launcher: the menu's sounds from %s (%d of 2)\n", path.c_str(), got);
        return true;
      }
    }
    return false;
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
  SDL_Rect r{};
  std::string label, sub;
  bool enabled = true;
  size_t sub_keep = 0;  // the sub's start kept whole when it is shortened to fit
  int badge = -1;       // a game's entry: its round badge (Art::badge), the name beside it
};

enum Id {
  kMw3 = 1, kPm, kQuit, kMw1, kFiles,
  kFind = 10, kPoint, kBack,
  kPatch,  // the 1.2 patch's zip: unreachable since the patch is built in (patch12.h), its code kept
  kAddDisc = 20,
  kGo = 30, kCancel, kNotesOk,
  kData = 40, kDataPick, kDataUsual, kMoveGo, kRemoveOld, kKeepOld,
  kMods = 50, kModUp, kModDown, kModCheck, kModPrev, kModNext, kModAdd, kModAddFolder, kModFolder,
  kModExport,
  kModRow = 60,  // kModRow + i: the page's i-th mod (kModsPerPage)
};
constexpr int kModsPerPage = 5;

struct Install {
  bool ok = false;
  std::string dir, version, summary;
};

// An operation on a worker thread (mech3::setup's calls run on the caller's).
struct Work {
  std::thread th;
  std::mutex m;
  mech3::setup::Progress progress;
  mech3::setup::Result result;
  std::atomic<bool> cancel{false}, done{false};
  std::string title;
  enum Kind { kOther, kLook, kInstallIt, kAddDisc, kConvert, kMoveData, kRemoveData } kind = kOther;  // a look, an install, ADD FROM MY DISC, the movies
  ~Work() {
    cancel = true;
    if (th.joinable()) th.join();
  }
  void start(std::string t, std::function<mech3::setup::Result(const mech3::setup::ProgressFn&)> op) {
    title = std::move(t);
    std::printf("mw3: launcher: work: %s\n", title.c_str());  // the checks follow it
    th = std::thread([this, op] {
      mech3::setup::Result r = op([this](const mech3::setup::Progress& p) {
        std::lock_guard<std::mutex> g(m);
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
  explicit Launcher(const LauncherOptions& o) : o_(o) {}
  ~Launcher() {
    if (canvas_) SDL_DestroySurface(canvas_);
    if (tex_) SDL_DestroyTexture(tex_);
    if (ren_) SDL_DestroyRenderer(ren_);
    if (win_) SDL_DestroyWindow(win_);
  }
  LauncherChoice run();

 private:
  void find_install();
  void find_mw1();
  void run_mw1();
  void start_game(bool pm = false);
  void start_pm();
  bool mw3_ready();
  void mw3_notes_seen();
  bool notes_once(const std::vector<std::string>& notes, std::string* crc_kept, const char* key);
  void find_pm();
  // GAME FILES on Pirate's Moon (files_pm_): its settings folder as mw3pm
  // finds it, its usual one (MW3's sibling, "MicroProse/Pirate's Moon", made
  // by mw3pm, not here), what it lacks, and the folder in use.
  std::string pm_usual() const;
  std::string pm_wd();
  std::string files_wd() { return files_pm_ ? pm_wd() : o_.write_dir; }
  std::string files_usual() const { return files_pm_ ? pm_usual() : usual_folder(); }
  void pm_refresh();
  std::string pm_in_use() const;
  void use_pm(const std::string& dir, const std::string& summary);
  std::vector<std::string> pm_notes(mech3::setup::Result* check = nullptr);
  void watch_game();
  void layout();
  void draw(float dt);
  void background(float dt);
  void plate(const Button& b, bool hot);
  void activate(int id);
  void ask_file(int what, const SDL_DialogFileFilter* filters, int n);
  void ask_folder(int what);
  void ask_save(int what, const std::string& file_name);
  void picked(int what, const std::string& path);
  void finish_work();
  void look_at(const std::string& path);
  void search();
  void use_install(const std::string& dir, const std::string& version, const std::string& summary);
  bool data_elsewhere();
  void ask_move(const std::string& to);
  void start_move();
  void moved(const mech3::setup::Result& r);
  void play();
  int movies_to_convert() const;
  void convert_movies(std::function<void()> then);
  void notes_refresh();
  std::string in_use() const;
  void add_disc(const std::string& source);
  void present();
  void nav(int action);  // 1 next, -1 previous, 2 press, 3 back (keys and a pad)

  LauncherOptions o_;
  SDL_Window* win_ = nullptr;
  SDL_Renderer* ren_ = nullptr;
  SDL_Texture* tex_ = nullptr;
  SDL_Surface* canvas_ = nullptr;
  Art art_;
  Text text_;
  enum Screen { kMain, kSetupScreen, kConfirm, kWorking, kNotes, kDataScreen, kModsScreen } screen_ = kMain;
  // The MODS screen (docs/launcher.md, Mods): the list as mech3::mods_list
  // gives it, the one selected (UP, DOWN), the page, whether CHECK ran.
  std::vector<mech3::ModsListEntry> mods_;
  int mod_sel_ = -1, mod_page_ = 0;
  bool mods_checked_ = false;
  void mods_refresh();
  void mods_save(std::vector<std::string> enabled, const std::string& said);
  void mods_add_pending();
  std::string pending_add_;  // IMPORT's pick, added once "Adding ..." is drawn
  bool add_drawn_ = false;
  std::string export_folder_;   // EXPORT's mod, while the save box is up
  std::string pending_export_;  // its zip, written once "Exporting ..." is drawn
  bool export_drawn_ = false;
  void mods_export_pending();
  std::string mod_line(const mech3::ModsListEntry& e) const;
  std::vector<std::string> notes_;  // the first start's (launcher_notes)
  std::vector<Button> buttons_;
  int hot_ = -1, focus_ = -1;
  float hot_t_ = 0, phase_ = 0, scroll_ = 0;
  // The ripple (mech3::menu_ripple_step's state) and the button it is for.
  int frames_ = 0;                    // frames shown, and their draw and present times (the log)
  bool vsync_ = false;
  int quit_after_ = SDL_getenv("MW3_LAUNCHER_FRAMES") ? std::atoi(SDL_getenv("MW3_LAUNCHER_FRAMES")) : 0;
  double draw_ms_ = 0, present_ms_ = 0;
  int ring_for_ = -1, ring_radius_ = 0;
  bool drew_ripple_ = false, drew_edge_ = false;  // the last frame's hover (the shot's log)
  float ring_carry_ = 0, ring_amp_ = 0, ring_phase_ = 0;
  // The ripple's centre: a game's badge, else the plate's middle.
  SDL_Point ring_centre(const Button& b) const {
    if (b.badge >= 0) return {b.r.x + kBadge / 2 + 4, b.r.y + b.r.h / 2};
    return {b.r.x + b.r.w / 2, b.r.y + b.r.h / 2};
  }
  Install install_;
  std::string pm_dir_;  // Pirate's Moon's folder, kept as pm_install= in mw3.cfg
  bool pm_last_ = false;  // the game last clicked: GAME FILES acts on it
  bool files_pm_ = false;  // GAME FILES' screens are Pirate's Moon's (pm_last_ when they opened)
  bool pm_lacks_music_ = false, pm_lacks_sounds_ = false;  // pm_refresh
  std::string pm_data_;                  // ... its data's version ("1.0", "unknown", "incomplete")
  std::vector<std::string> mw3_check_notes_;  // start_check's for MW3, part of notes_ (notes_refresh)
  std::string mw3_program_, pm_program_;  // each game's program's version (--version), once at the start
  bool entries_logged_ = false;
  std::string game_name_ = "MechWarrior 3";  // the one started, for the status lines
  SDL_Process* game_ = nullptr;  // mw3, started by MECHWARRIOR 3 (game_bin)
  Uint64 game_click_ = 0;
  bool game_slow_ = false;
  std::string game_status_;     // the game's own status line before its first frame
  double status_read_ms_ = -1e9;  // no first frame within ready_wait_ms(): the status line says so
  std::string ready_file_;
  std::string mw1_exe_, mw1_game_;  // MechWarrior (1989): Elthial's port beside ours, the player's folder
  std::string status_;
  bool status_good_ = true;
  std::unique_ptr<Work> work_;
  Pick pick_;
  bool asking_ = false;
  Uint64 ask_at_ = 0;      // when the picker was asked for (its timing in the log)
  bool ask_shown_ = false;  // the panel took the window's focus
  std::string confirm_text_, pending_source_;
  std::string move_to_, move_from_;  // the data folder's move (ask_move)
  std::vector<std::string> move_copied_;
  std::shared_ptr<std::vector<std::string>> move_copied_out_;
  int confirm_go_ = 0;
  std::shared_ptr<mech3::source::Found> found_;  // the last look's (look_at)
  bool auto_go_ = false;  // MW3_LAUNCHER_PICK (checks): its look's INSTALL pressed too
  bool auto_ok_ = false;  // and the notes' OK after it
  bool play_after_notes_ = false;  // the notes after an install: their OK starts the game
  bool pm_after_notes_ = false;    // ... Pirate's Moon (its notes, start_pm)
  bool from_files_ = false;        // the find screen opened by GAME FILES: a new pick goes back to the games
  std::function<void()> after_convert_;  // where the movies' conversion goes on to
  bool converted_ = false;               // tried once this start (a failure is not retried before the game)
  int disc_back_ = 0;              // the screen ADD FROM MY DISC returns to (Screen)
  bool lacks_music_ = false, lacks_movies_ = false;
  LauncherChoice choice_;
  bool done_ = false;
  Sounds sounds_;
  bool sounds_on_ = false, sounds_tried_ = false;  // loaded once there is an install (at the start or once found)
};

void Launcher::find_install() {
  install_ = {};
  std::vector<std::string> tries;
  if (!o_.cfg_install.empty()) tries.push_back(o_.cfg_install);
  if (!o_.write_dir.empty()) tries.push_back((fs::path(o_.write_dir) / "install").string());
  for (const std::string& c : install_candidates()) tries.push_back(c);
  for (const std::string& c : o_.more_installs) tries.push_back(c);
  for (const std::string& d : tries) {
    if (!install_valid(d)) continue;
    const mech3::setup::Result r = mech3::setup::check_install(d, o_.write_dir);
    if (!r.ok) continue;
    install_ = {true, r.install_dir.empty() ? d : r.install_dir, r.version, r.summary};
    std::printf("mw3: launcher: %s\n", r.summary.c_str());
    return;
  }
  std::printf("mw3: launcher: no MechWarrior 3 install found\n");
}

namespace {
bool is_pm_install(const mech3::source::Found& f) {
  return f.kind == mech3::source::Found::kInstall && f.game == mech3::source::Found::kPiratesMoon && f.on_disk;
}

#ifndef MW3_VERSION
#define MW3_VERSION ""
#endif

// The label on a game's line (Owen, 2026-11-10: shown, never blocking): "official" when its data is a release's
// (MW3 1.0 or 1.2, or 1.0 with the patch's files; PM 1.0), its program a release build (its tag a bare vX.Y.Z,
// PM's pm-vX.Y.Z); else "modified (...)" naming which. Mods are a feature, not a change to the game: they are a
// note of their own (versions_words; lead, 2026-11-10).
std::string official_words(const std::string& data, const std::string& program, bool pm) {
  const bool data_ok = pm ? data == "1.0" : data == "1.0" || data == "1.2" || data == "1.0 with 1.2 files";
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
  return v.empty() ? "MechWarrior 3 Rebuild" : "MechWarrior 3 Rebuild " + v;
}

// A game's line's versions: its data's, then its program's, then the mods on ("data 1.2, program v1.1.0,
// 2 mods on, "); a program that did not say is left out.
std::string versions_words(const std::string& data, const std::string& program, size_t mods) {
  std::string s = data.empty() ? std::string() : data == "unknown" ? "data unknown (a mod?), " : "data " + data + ", ";
  if (!program.empty()) s += "program " + program + ", ";
  if (mods) s += (mods == 1 ? std::string("a mod") : std::to_string(mods) + " mods") + " on, ";
  return s;
}

// A program's version as its file holds it ("MW3-PROGRAM-VERSION:v1.1.0",
// mw3_main.cpp's mw3_version_tag), read, never run: a game started only to
// say its version could show Windows' box for a missing DLL. Empty when the
// file has none (an older build, a script).
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

// A MechWarrior (1989) folder: BTECH.EXE in it (any case).
bool mw1_game_valid(const std::string& dir) {
  std::error_code ec;
  if (dir.empty() || !fs::is_directory(dir, ec)) return false;
  for (const fs::directory_entry& e : fs::directory_iterator(dir, ec)) {
    std::string n = e.path().filename().string();
    for (char& c : n) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
    if (n == "BTECH.EXE") return true;
  }
  return false;
}
}  // namespace

// Elthial's MechWarrior (1989) SDL port (docs/launcher.md): its program
// beside ours or in a MechWarrior1989 folder beside it (MW3_MW1_PORT for
// checks); the entry shows only when it is there.
void Launcher::find_mw1() {
#if defined(_WIN32)
  const char* name = "mechwarrior.exe";
#else
  const char* name = "mechwarrior";
#endif
  std::vector<fs::path> tries;
  if (const char* env = SDL_getenv("MW3_MW1_PORT")) tries.emplace_back(env);
  tries.push_back(fs::path(o_.program_dir) / name), tries.push_back(fs::path(o_.program_dir) / "MechWarrior1989" / name);
  std::error_code ec;
  for (const fs::path& t : tries)
    if (fs::is_regular_file(t, ec)) {
      mw1_exe_ = t.string();
      std::printf("mw3: launcher: MechWarrior (1989) port at %s\n", mw1_exe_.c_str());
      break;
    }
  if (mw1_game_valid(o_.cfg_mw1_game)) mw1_game_ = o_.cfg_mw1_game;
}

// The port started as its own script starts it (`mechwarrior <game dir>
// <capture dir>`, the saves in MECHWARRIOR_SAVE_DIRECTORY), both under the
// write dir's mw1; this window hidden until it exits.
// Pirate's Moon: its folder as last picked, while it still holds the game;
// else the places it may be installed (sdlfe::install_candidates: its
// registry key, the usual folders, Wine, Proton and Lutris prefixes, the
// Mac's bottles), each read by map_source so an MW3 folder is never taken,
// the one found saved to mw3.cfg as pm_install= (docs/launcher.md).
void Launcher::find_pm() {
  if (o_.pm_bin.empty()) return;
  if (!o_.cfg_pm_install.empty() && is_pm_install(mech3::source::map_source(o_.cfg_pm_install))) {
    pm_dir_ = o_.cfg_pm_install;
    std::printf("mw3: launcher: Pirate's Moon at %s: ready\n", pm_dir_.c_str());
    return;
  }
  if (!o_.cfg_pm_install.empty()) std::printf("mw3: launcher: Pirate's Moon at %s: not there\n", o_.cfg_pm_install.c_str());
  std::vector<std::string> tries;
  if (!pm_wd().empty()) tries.push_back((fs::path(pm_wd()) / "install").string());  // GAME FILES' own install
  for (const std::string& c : sdlfe::install_candidates(true)) tries.push_back(c);
  for (const std::string& d : tries) {
    std::error_code ec;
    if (!fs::is_directory(d, ec) || !is_pm_install(mech3::source::map_source(d))) continue;
    pm_dir_ = d;
    sdlfe::save_config_value(o_.write_dir, "pm_install", d);
    std::printf("mw3: launcher: Pirate's Moon found at %s\n", d.c_str());
    return;
  }
  std::printf("mw3: launcher: no Pirate's Moon install found\n");
}

void Launcher::run_mw1() {
  const fs::path base = o_.write_dir.empty() ? fs::path("mw1") : fs::path(o_.write_dir) / "mw1";
  std::error_code ec;
  fs::create_directories(base / "captures", ec);
  fs::create_directories(base / "saves", ec);
  SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "MECHWARRIOR_SAVE_DIRECTORY", (base / "saves").string().c_str(), true);
  const std::string game = mw1_game_, captures = (base / "captures").string();
  const char* args[] = {mw1_exe_.c_str(), game.c_str(), captures.c_str(), nullptr};
  SDL_Process* p = SDL_CreateProcess(args, false);
  if (!p) {
    status_ = std::string("MechWarrior (1989) did not start: ") + SDL_GetError(), status_good_ = false;
    std::printf("mw3: launcher: %s\n", status_.c_str());
    return;
  }
  std::printf("mw3: launcher: MechWarrior (1989) started\n");
  if (win_) SDL_HideWindow(win_);
  int code = 0;
  while (!SDL_WaitProcess(p, false, &code)) {
    SDL_PumpEvents();
    SDL_Delay(50);
  }
  SDL_DestroyProcess(p);
  std::printf("mw3: launcher: MechWarrior (1989) exited (%d)\n", code);
  status_ = code == 0 ? "MechWarrior (1989) closed." : "MechWarrior (1989) closed with an error (" + std::to_string(code) + "): see its log.";
  status_good_ = code == 0;
  if (win_ && !o_.hidden) SDL_ShowWindow(win_), SDL_RaiseWindow(win_);
}

// Pirate's Moon checked before it starts (setup::check_install, docs/launcher.md,
// Pirate's Moon): what stops it is said instead of starting; notes not shown
// before (pm_notes=, their CRC) once on BEFORE YOU PLAY, whose OK starts it.
// Its settings folder as mw3pm finds its own; nothing is written to it here.
void Launcher::start_pm() {
  mech3::setup::Result r;
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
  std::snprintf(crc, sizeof crc, "%08x", zutl_zar::crc32(0, reinterpret_cast<const uint8_t*>(all.data()), all.size()));
  if (notes.empty() || *crc_kept == crc) return false;
  *crc_kept = crc;
  save_config_value(o_.write_dir, key, crc);
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
  const mech3::setup::Result r = mech3::setup::start_check(install_.dir, o_.write_dir, false);
  std::printf("mw3: launcher: %s\n", r.summary.c_str());
  for (const std::string& w : r.warnings) std::printf("mw3: launcher: note: %s\n", w.c_str());
  if (!r.ok) return true;
  files_pm_ = pm_after_notes_ = false;
  return !notes_once(r.warnings, &o_.cfg_mw3_notes, "mw3_notes");
}

// Pirate's Moon's notes before a start: the check's, and no music as
// MechWarrior 3's (launcher_notes); `check` gets the check.
std::vector<std::string> Launcher::pm_notes(mech3::setup::Result* check) {
  const std::string wd = pm_wd();
  mech3::setup::Result r = mech3::setup::check_install(pm_dir_, wd, true);
  std::vector<std::string> out = r.warnings;
  pm_refresh();
  if (r.ok && pm_lacks_music_) {
    const std::string dir = (fs::path(wd) / "music").make_preferred().string();
    for (const std::string& n : launcher_notes("", false, true, text_.fit(dir, 14, 440))) out.push_back(n);
  }
  if (check) *check = std::move(r);
  return out;
}

// MechWarrior 3 as its own program (game_bin): the install and the options
// passed on, the launcher kept on screen until the game's first frame
// (mw3 --ready-file) so the switch shows no empty moment, then closed: the
// game goes on alone (Owen, 2026-11-01).
void Launcher::start_game(bool pm) {
  std::error_code ec;
  game_name_ = pm ? "Pirate's Moon" : "MechWarrior 3";
  ready_file_ = (fs::temp_directory_path(ec) / ("mw3-ready-" + std::to_string(SDL_GetTicksNS()))).string();
  fs::remove(ready_file_, ec);
  // PM has its own install, settings and saves: nothing of MW3's goes to it.
  std::vector<std::string> args;
  if (pm) {
    args = {o_.pm_bin, "--install", pm_dir_, "--no-launcher"};
    args.insert(args.end(), o_.pm_args.begin(), o_.pm_args.end());
  } else {
    args = {o_.game_bin, install_.dir};
    args.insert(args.end(), o_.game_args.begin(), o_.game_args.end());
  }
  args.push_back("--ready-file"), args.push_back(ready_file_);
  std::vector<const char*> argv;
  std::string line;
  for (const std::string& a : args) argv.push_back(a.c_str()), line += (line.empty() ? "" : " ") + a;
  argv.push_back(nullptr);
  if (!pm) save_install(o_.write_dir, install_.dir);
  game_click_ = SDL_GetTicksNS(), game_slow_ = false, game_status_.clear(), status_read_ms_ = -1e9;
  // Its own stdio, none of ours: the game writes mw3.log itself, and our
  // log's pipe in its stdout would hold this program's exit until the game's.
  const SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
  // The game's box after a crash would page from a mount going away: this
  // program's instead (watch_game), which stays for an AppImage.
  if (std::getenv("APPIMAGE")) SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "MW3_LAUNCHER_WATCHES", "1", true);
  game_ = SDL_CreateProcessWithProperties(props);
  SDL_DestroyProperties(props);
  if (!game_) {
    status_ = game_name_ + " did not start: " + SDL_GetError(), status_good_ = false;
    std::printf("mw3launcher: %s (%s)\n", status_.c_str(), line.c_str());
    layout();
    return;
  }
  std::printf("mw3launcher: started %s\n", line.c_str());
  status_ = "Starting " + game_name_ + "...", status_good_ = true;
}

// Each pass while the game starts: at its first frame the launcher closes,
// the game left running; a game that exits first (an error) or shows no
// frame in 20 s leaves the launcher up with a status line, still watching.
void Launcher::watch_game() {
  if (!game_) return;
  std::error_code ec;
  const double ms = static_cast<double>(SDL_GetTicksNS() - game_click_) / 1e6;
  int code = 0;
  if (SDL_WaitProcess(game_, false, &code)) {
    SDL_DestroyProcess(game_);
    game_ = nullptr;
    fs::remove(ready_file_, ec);
    fs::remove(ready_file_ + ".status", ec);
    std::printf("mw3launcher: %s exited (%d) after %.1f s\n", game_name_.c_str(), code, ms / 1000.0);
    status_ = code == 0 ? "" : game_name_ + " closed with an error (" + std::to_string(code) + "): see mw3.log.";
    status_good_ = code == 0;
    if (win_ && !o_.hidden) SDL_ShowWindow(win_), SDL_RaiseWindow(win_);
    if (o_.quit_after_game) done_ = true;
    layout();
    return;
  }
  if (fs::exists(ready_file_, ec)) {
    std::printf("mw3launcher: the game's first frame %.0f ms after MECHWARRIOR 3 was pressed: the launcher closes\n", ms);
    fs::remove(ready_file_, ec);
    fs::remove(ready_file_ + ".status", ec);
    if (win_) SDL_HideWindow(win_);
    // FIX (A): an AppImage unmounts when its first process (this) ends, with
    // the game still paging its code and libraries from there: it would die
    // when it reaches a page not yet read, after the intro. So here this
    // waits, hidden, and says if the game crashes (docs/linux.md, Packaging).
    if (std::getenv("APPIMAGE")) {
      int code = 0;
      std::fflush(stdout);  // a long wait: the log has this far
      SDL_WaitProcess(game_, true, &code);
      std::printf("mw3launcher: MechWarrior 3 exited (%d) after %.1f s\n", code,
                  static_cast<double>(SDL_GetTicksNS() - game_click_) / 1e9);
      std::fflush(stdout);
      if (code < 0) {
        const std::string text = "MechWarrior 3 crashed (signal " + std::to_string(-code) + ").\n\nThe log is " +
                                 o_.write_dir + "/mw3.log\nPlease include it if you report this.";
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "MechWarrior 3", text.c_str(), nullptr);
      }
    }
    SDL_DestroyProcess(game_);  // let go: the game runs on, where it has not ended above
    game_ = nullptr;
    done_ = true;
    return;
  }
  // What the game says it is doing before its first frame (its first start's
  // movie import, sdl_child.h), read four times a second: on the status line
  // instead of the wait's.
  if (ms - status_read_ms_ >= 250.0) {
    status_read_ms_ = ms;
    std::ifstream in(ready_file_ + ".status");
    std::string line;
    std::getline(in, line);
    if (!line.empty() && line != game_status_) {
      game_status_ = line;
      std::printf("mw3launcher: the game says: %s\n", line.c_str());
      status_ = line, status_good_ = true;
      layout();
    }
  }
  if (!game_slow_ && game_status_.empty() && ms > ready_wait_ms()) {
    game_slow_ = true;
    std::printf("mw3launcher: no first frame from the game after %.0f s: the launcher stays\n", ready_wait_ms() / 1000.0);
    status_ = "MechWarrior 3 has not shown a picture yet: still waiting (see mw3.log).", status_good_ = false;
    layout();
  }
}

void Launcher::layout() {
  buttons_.clear();
  auto add = [&](int id, SDL_Rect r, std::string label, std::string sub = {}, bool on = true) {
    r.x += kX;
    buttons_.push_back({id, r, std::move(label), std::move(sub), on});
  };
  if (sounds_on_ && install_.ok && !sounds_tried_) {  // any screen, once there is an install
    sounds_tried_ = true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) sounds_.load(install_.dir);
  }
  switch (screen_) {
    case kMain: {
      // A long folder is shortened to the plate when drawn (Text::fit), the
      // words before it kept.
      const std::string ready = "Ready, " + official_words(install_.version, mw3_program_, false) + ": " +
                                versions_words(install_.version, mw3_program_, mech3::mods_enabled(o_.write_dir).size());
      std::string sub = install_.ok ? ready + install_.dir : "Not found yet: click to find your game";
      // The games as round badges (Owen: the ripple belongs to a circle),
      // each name and line beside its badge, the ripple from its centre.
      auto game = [&](int id, int row, int badge, const char* name, const std::string& line, bool on) {
        add(id, {70, 118 + row * 84, 500, 80}, name, line, on);  // 500 wide at 16:9 (380 at 4:3): the longer lines
        buttons_.back().badge = badge;
      };
      game(kMw3, 0, 0, "MECHWARRIOR 3", sub, true);
      if (!entries_logged_) std::printf("mw3: launcher: entry MECHWARRIOR 3: %s\n", sub.c_str());
      if (sub.rfind(ready, 0) == 0) buttons_.back().sub_keep = ready.size();
      // Pirate's Moon when its program is beside this one (a package with it).
      if (o_.pm_bin.empty()) {
        game(kPm, 1, 1, "PIRATE'S MOON", "Coming later", false);
      } else {
        const std::string pm_ready = "Ready, " + official_words(pm_data_, pm_program_, true) + ": " +
                                     versions_words(pm_data_, pm_program_, mech3::mods_enabled(pm_wd()).size());
        game(kPm, 1, 1, "PIRATE'S MOON", pm_dir_.empty() ? "Not found yet: click to find your game" : pm_ready + pm_dir_, true);
        if (!pm_dir_.empty()) buttons_.back().sub_keep = pm_ready.size();
        if (!entries_logged_) std::printf("mw3: launcher: entry PIRATE'S MOON: %s\n", buttons_.back().sub.c_str());
      }
      if (!entries_logged_) std::printf("mw3: launcher: footer %s\n", package_words().c_str());
      entries_logged_ = true;  // once a run: the lines as first shown
      if (!mw1_exe_.empty()) {  // Elthial's port, credited on its line (MIT; THIRD-PARTY.txt)
        const std::string by = "Elthial's SDL port: ";
        game(kMw1, 2, 2, "MECHWARRIOR (1989)", by + (mw1_game_.empty() ? "click to pick your MechWarrior folder" : mw1_game_), true);
        buttons_.back().sub_keep = by.size();
      }
      // GAME FILES (Owen, 2026-11-02): the find screen at any time, to pick
      // or refresh the game's files; QUIT for fullscreen (Escape too).
      add(kFiles, {132, 384, 120, 34}, "GAME FILES");
      add(kMods, {260, 384, 120, 34}, "MODS");
      add(kQuit, {388, 384, 120, 34}, "QUIT");
      break;
    }
    case kSetupScreen: {
      // Find the game (Owen, 2026-11-02: one option): the search first (an
      // installed copy or a disc in a drive used at once), else one picker:
      // an image or a download, or any file of an installed copy (its
      // folder; pick_source). The 1.2 patch is built in (patch12.h).
      add(kFind, {130, 222, 380, 52}, "FIND THE GAME", "A disc image, a download (.zip .iso), or Mech3.exe in an installed copy");
      if (from_files_ && (files_pm_ || !o_.write_dir_fixed)) add(kData, {130, 278, 380, 36}, "MY DATA FOLDER");
      if (files_pm_ ? !pm_dir_.empty() && (pm_lacks_music_ || pm_lacks_sounds_)
                    : install_.ok && (lacks_music_ || lacks_movies_))  // what the install in use lacks, from the disc
        add(kAddDisc, {130, 318, 380, 46}, "ADD FROM MY DISC", "A disc in a drive first, else its image or any file of the disc");
      add(kBack, {250, 384, 140, 34}, o_.setup_only ? "QUIT" : "BACK");
      break;
    }
    case kDataScreen:
      add(kDataPick, {130, 250, 380, 46}, "MOVE IT TO ANOTHER FOLDER", "Copied, checked, then switched. The old copy goes only if you say so");
      if (data_elsewhere()) add(kDataUsual, {130, 304, 380, 40}, "MOVE IT BACK TO THE USUAL FOLDER");
      add(kBack, {250, 384, 140, 34}, "BACK");
      break;
    case kConfirm:
      if (confirm_go_ == 3) {
        add(kRemoveOld, {115, 300, 260, 40}, "REMOVE THE OLD COPY");
        add(kKeepOld, {385, 300, 140, 40}, "KEEP IT");
        break;
      }
      if (confirm_go_ == 2) add(kMoveGo, {170, 300, 140, 40}, "MOVE");
      else if (confirm_go_) add(kGo, {170, 300, 140, 40}, "INSTALL");
      add(kBack, {330, 300, 140, 40}, "BACK");
      break;
    case kWorking:
      add(kCancel, {250, 372, 140, 34}, "CANCEL");
      break;
    case kNotes:
      if (files_pm_ ? pm_lacks_music_ || pm_lacks_sounds_ : lacks_music_ || lacks_movies_)  // what the install lacks, from the disc (lead, 2026-11-02)
        add(kAddDisc, {130, 318, 380, 46}, "ADD FROM MY DISC", "A disc in a drive first, else its image or any file of the disc");
      add(kNotesOk, {250, 384, 140, 34}, "OK");
      break;
    case kModsScreen: {
      // A row a mod (click: on or off), UP and DOWN for the selected one,
      // CHECK (what the game would refuse in each), the pages, BACK.
      const int first = mod_page_ * kModsPerPage;
      for (int i = 0; i < kModsPerPage && first + i < static_cast<int>(mods_.size()); ++i) {
        const mech3::ModsListEntry& e = mods_[first + i];
        const std::string name = e.info.name.empty() ? e.info.folder : e.info.name + (e.info.version.empty() ? "" : " " + e.info.version);
        add(kModRow + i, {70, 104 + i * 42, 500, 38}, (first + i == mod_sel_ ? "> " : "") + name, mod_line(e), e.info.error.empty());
      }
      const bool sel = mod_sel_ >= 0 && mod_sel_ < static_cast<int>(mods_.size()) && mods_[mod_sel_].on;
      // ADD MOD (an archive), ADD FOLDER and MODS FOLDER (docs/modding.md, ADD MOD).
      // IMPORT (an archive; ADD MOD before), EXPORT (the selected mod as a .zip),
      // ADD FOLDER and MODS FOLDER (docs/modding.md, IMPORT and EXPORT).
      const bool idle = !asking_ && pending_add_.empty() && pending_export_.empty();
      const bool chosen = mod_sel_ >= 0 && mod_sel_ < static_cast<int>(mods_.size());
      add(kModAdd, {70, 318, 112, 34}, "IMPORT", "A .zip, .7z or .rar", idle);
      add(kModExport, {188, 318, 112, 34}, "EXPORT", "The mod as a .zip", idle && chosen);
      add(kModAddFolder, {306, 318, 128, 34}, "ADD FOLDER", "A mod's folder", idle);
      add(kModFolder, {440, 318, 130, 34}, "MODS FOLDER", "In the file browser");
      add(kModUp, {70, 384, 70, 34}, "UP", {}, sel && mod_sel_ > 0);
      add(kModDown, {146, 384, 70, 34}, "DOWN", {}, sel && mod_sel_ + 1 < static_cast<int>(mods_.size()) && mods_[mod_sel_ + 1].on);
      add(kModCheck, {222, 384, 100, 34}, "CHECK", {}, !mods_.empty());
      add(kModPrev, {328, 384, 50, 34}, "<", {}, mod_page_ > 0);
      add(kModNext, {384, 384, 50, 34}, ">", {}, (mod_page_ + 1) * kModsPerPage < static_cast<int>(mods_.size()));
      add(kBack, {440, 384, 130, 34}, "BACK");
      break;
    }
  }
  {  // the screen and its plates (the checks follow them)
    static const char* const kNames[] = {"main", "find", "confirm", "work", "notes", "data", "mods"};
    std::string labels;
    for (const Button& b : buttons_) labels += (labels.empty() ? "" : ", ") + b.label;
    std::printf("mw3: launcher: screen %s: %s\n", kNames[screen_], labels.c_str());
  }
  hot_ = focus_ = -1;
}

void Launcher::background(float dt) {
  scroll_ += dt * 6.0f;  // a few pixels a second
  SDL_FillSurfaceRect(canvas_, nullptr, SDL_MapSurfaceRGB(canvas_, 8, 8, 12));
  if (art_.stars) {
    const int sw = art_.stars->w, x = static_cast<int>(scroll_) % sw;
    SDL_Rect src{x, 0, std::min(W, sw - x), H}, dst{0, 0, src.w, H};
    SDL_BlitSurface(art_.stars, &src, canvas_, &dst);
    if (src.w < W) {
      SDL_Rect s2{0, 0, W - src.w, H}, d2{src.w, 0, W - src.w, H};
      SDL_BlitSurface(art_.stars, &s2, canvas_, &d2);
    }
  }
  if (art_.metal) {
    const int span = art_.metal->w - W - 200, o = span > 0 ? static_cast<int>(scroll_ / 3) % span : 0;
    SDL_Rect top{o, 40, W, 96}, bot{o + 100, 150, W, 48};
    SDL_Rect dt0{0, 0, W, 96}, db{0, H - 48, W, 48};
    SDL_BlitSurface(art_.metal, &top, canvas_, &dt0);
    SDL_BlitSurface(art_.metal, &bot, canvas_, &db);
  }
  const uint32_t amber = SDL_MapSurfaceRGB(canvas_, 255, 176, 64);
  SDL_Rect l1{0, 96, W, 1}, l2{0, H - 49, W, 1};
  SDL_FillSurfaceRect(canvas_, &l1, amber), SDL_FillSurfaceRect(canvas_, &l2, amber);
  if (art_.title) {
    const int x = (W - art_.title->w - 84) / 2;
    if (art_.emblem) {
      SDL_Rect e{x, 12, art_.emblem->w, art_.emblem->h};
      SDL_BlitSurface(art_.emblem, nullptr, canvas_, &e);
    }
    SDL_Rect t{x + 84, 48 - art_.title->h / 2, art_.title->w, art_.title->h};
    SDL_BlitSurface(art_.title, nullptr, canvas_, &t);
  } else {
    text_.draw(canvas_, "MW3 REBUILD", 40, kWhite, W / 2, 48);
  }
}

void Launcher::plate(const Button& b, bool hot) {
  SDL_Rect r = b.r;
  if (b.badge >= 0) {  // a game: its badge, the name and its line beside it
    const SDL_Point c = ring_centre(b);
    SDL_Surface* art = art_.badge[b.badge];
    const uint8_t m = !b.enabled ? 80 : hot ? 255 : 205;
    if (art) {
      SDL_SetSurfaceColorMod(art, m, m, m);
      SDL_Rect d{c.x - kBadge / 2, c.y - kBadge / 2, kBadge, kBadge};
      SDL_BlitSurface(art, nullptr, canvas_, &d);
    } else {  // no art: a plain disc
      for (int y = -kBadge / 2; y <= kBadge / 2; ++y) {
        const int half = static_cast<int>(std::sqrt(static_cast<float>(kBadge * kBadge / 4 - y * y)));
        SDL_Rect row{c.x - half, c.y + y, half * 2, 1};
        SDL_FillSurfaceRect(canvas_, &row, SDL_MapSurfaceRGB(canvas_, m / 3, m / 3, m / 3 + 8));
      }
    }
    const SDL_Color col = !b.enabled ? kGrey : hot ? kAmber : kWhite;
    const int tx = c.x + kBadge / 2 + 18, tw = r.x + r.w - tx;
    text_.draw(canvas_, b.label, 22, col, tx, c.y - 9, -1);
    text_.draw(canvas_, text_.fit(b.sub, 12, tw, b.sub_keep), 12, b.enabled ? kDim : kGrey, tx, c.y + 14, -1);
    return;
  }
  if (art_.plate) {
    const uint8_t m = !b.enabled ? 90 : hot ? 255 : 215;
    SDL_SetSurfaceColorMod(art_.plate, m, m, m);
    SDL_BlitSurfaceScaled(art_.plate, nullptr, canvas_, &r, SDL_SCALEMODE_LINEAR);
  } else {
    SDL_FillSurfaceRect(canvas_, &r, SDL_MapSurfaceRGB(canvas_, b.enabled ? 70 : 40, b.enabled ? 72 : 40, b.enabled ? 78 : 44));
  }
  if (hot) {  // a square plate lights its edge in the frame's amber (the ripple is the round badges'; Owen)
    const uint32_t amber = SDL_MapSurfaceRGB(canvas_, 255, 176, 64), soft = SDL_MapSurfaceRGB(canvas_, 150, 104, 40);
    const SDL_Rect outer[4] = {{r.x, r.y, r.w, 1}, {r.x, r.y + r.h - 1, r.w, 1}, {r.x, r.y, 1, r.h}, {r.x + r.w - 1, r.y, 1, r.h}};
    const SDL_Rect inner[4] = {{r.x + 1, r.y + 1, r.w - 2, 1}, {r.x + 1, r.y + r.h - 2, r.w - 2, 1},
                               {r.x + 1, r.y + 1, 1, r.h - 2}, {r.x + r.w - 2, r.y + 1, 1, r.h - 2}};
    SDL_FillSurfaceRects(canvas_, outer, 4, amber);
    SDL_FillSurfaceRects(canvas_, inner, 4, soft);
    drew_edge_ = true;
  }
  const SDL_Color c = !b.enabled ? kGrey : hot ? kAmber : kWhite;
  const int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
  const int big = r.h >= 50 ? 22 : r.h >= 40 ? 20 : 18;
  if (b.sub.empty()) {
    text_.draw(canvas_, b.label, big, c, cx, cy + 1);
  } else {
    text_.draw(canvas_, b.label, big, c, cx, cy - 8);
    text_.draw(canvas_, text_.fit(b.sub, 12, r.w - 20, b.sub_keep), 12, b.enabled ? kDim : kGrey, cx, cy + 13);
  }
}

void Launcher::draw(float dt) {
  background(dt);
  switch (screen_) {
    case kMain:
      if (!status_.empty()) {
        SDL_Rect box{kX + 110, 104, 420, 24};
        SDL_FillSurfaceRect(canvas_, &box, SDL_MapSurfaceRGB(canvas_, status_good_ ? 16 : 30, status_good_ ? 26 : 14, 16));
        text_.draw(canvas_, status_, 12, status_good_ ? SDL_Color{200, 230, 200, 255} : SDL_Color{240, 190, 180, 255}, W / 2, 116);
      }
      text_.draw(canvas_, "Unofficial fan rebuild. MechWarrior is a trademark of Microsoft.", 12, kDim, 16, H - 24, -1);
      text_.draw(canvas_, package_words(), 12, kDim, W - 16, H - 24, 1);  // the package's own version (the games' are on their lines)
      break;
    case kSetupScreen: {
      SDL_Rect box{kX + 70, 104, 500, 64};
      SDL_FillSurfaceRect(canvas_, &box, SDL_MapSurfaceRGB(canvas_, 16, 18, 22));
      text_.draw_wrapped(canvas_, files_pm_ ? kFindWordsPm : kFindWords, 13, kWhite, W / 2, 114, 480);
      if (!status_.empty())
        text_.draw_wrapped(canvas_, status_, 13, status_good_ ? kAmber : SDL_Color{240, 190, 180, 255}, W / 2, 184, 480);
      else if (files_pm_ ? !pm_dir_.empty() : install_.ok)  // GAME FILES: the one in use, and what it lacks
        text_.draw(canvas_, text_.fit(files_pm_ ? pm_in_use() : in_use(), 13, 480), 13, kDim, W / 2, 190);
      break;
    }
    case kNotes: {
      text_.draw(canvas_, "BEFORE YOU PLAY", 24, kAmber, W / 2, 128);
      int y = 162;
      for (const std::string& n : notes_) {
        y += text_.draw_wrapped(canvas_, n, 14, kWhite, W / 2, y, 470) + 14;
      }
      if (!status_.empty())  // what ADD FROM MY DISC did, or where to look
        text_.draw_wrapped(canvas_, status_, 13, status_good_ ? kAmber : SDL_Color{240, 190, 180, 255}, W / 2, 296, 480);
      break;
    }
    case kConfirm:
      text_.draw_wrapped(canvas_, confirm_text_, 16, kWhite, W / 2, 150, 460);
      break;
    case kModsScreen: {
      // Below the banner, as the other screens: the rows, the status under
      // them, the folder in the footer.
      const std::string where = mods_target(o_, false).write_dir + "/mods";
      text_.draw(canvas_, text_.fit("Mods in " + where + ", in the order they win", 12, W - 32), 12, kDim, 16, H - 24, -1);
      if (mods_.empty())
        text_.draw_wrapped(canvas_, "No mods yet. A mod is a folder in the mods folder below with a mod.txt (docs/modding.md).", 14,
                           kWhite, W / 2, 180, 460);
      if (!status_.empty())
        text_.draw(canvas_, text_.fit(status_, 12, 560), 12, status_good_ ? kAmber : SDL_Color{240, 190, 180, 255}, W / 2, 367);
      break;
    }
    case kDataScreen: {
      text_.draw_wrapped(canvas_, "Your settings, pilots, saved games and converted movies are in:", 16, kWhite, W / 2, 130, 480);
      text_.draw_wrapped(canvas_, text_.fit(files_wd(), 14, 480), 14, kAmber, W / 2, 176, 480);
      if (!status_.empty())
        text_.draw_wrapped(canvas_, status_, 13, status_good_ ? kAmber : SDL_Color{240, 190, 180, 255}, W / 2, 350, 480);
      break;
    }
    case kWorking: {
      mech3::setup::Progress p;
      std::string title;
      if (work_) {
        std::lock_guard<std::mutex> g(work_->m);
        p = work_->progress, title = work_->title;
      }
      text_.draw(canvas_, title, 26, kAmber, W / 2, 140);
      SDL_Rect bar{kX + 120, 200, 400, 26}, in{kX + 123, 203, static_cast<int>(394 * std::clamp(p.fraction, 0.0, 1.0)), 20};
      SDL_FillSurfaceRect(canvas_, &bar, SDL_MapSurfaceRGB(canvas_, 140, 100, 50));
      SDL_Rect inner{kX + 122, 202, 396, 22};
      SDL_FillSurfaceRect(canvas_, &inner, SDL_MapSurfaceRGB(canvas_, 10, 12, 14));
      SDL_FillSurfaceRect(canvas_, &in, SDL_MapSurfaceRGB(canvas_, 214, 140, 40));
      char pct[16];
      std::snprintf(pct, sizeof pct, "%d%%", static_cast<int>(std::lround(p.fraction * 100)));
      text_.draw(canvas_, pct, 14, kWhite, W / 2, 214);
      text_.draw(canvas_, p.stage, 13, kWhite, W / 2, 246);
      text_.draw(canvas_, p.message, 10, kGrey, W / 2, 266);
      text_.draw(canvas_, "Cancel stops within a file and changes nothing.", 12, kDim, 16, H - 24, -1);
      break;
    }
  }
  const int hot = focus_ >= 0 ? focus_ : hot_;
  drew_ripple_ = drew_edge_ = false;
  for (size_t i = 0; i < buttons_.size(); ++i) plate(buttons_[i], static_cast<int>(i) == hot && buttons_[i].enabled);
  // The main menu's own ripple (its step, mech3::menu_ripple_step, and its
  // draw 0x00558dd0) round the round button (a game's badge) under the
  // mouse, started afresh on each new one as SlideButton's on_enter does;
  // a square plate lights its edge instead (plate).
  hot_t_ += dt;
  if (hot != ring_for_) {
    ring_for_ = hot, ring_radius_ = mech3::MenuRipple::kStartRadius, ring_carry_ = 0.0f;
    ring_amp_ = mech3::MenuRipple::kStartAmplitude, ring_phase_ = 0.0f;
  }
  if (hot >= 0 && hot < static_cast<int>(buttons_.size()) && buttons_[hot].enabled && buttons_[hot].badge >= 0 &&
      mech3::menu_ripple_step(ring_radius_, ring_carry_, ring_amp_, ring_phase_, mech3::MenuRipple::kSpeed,
                              mech3::MenuRipple::kWavelength, mech3::MenuRipple::kMaxRadius, dt)) {
    const SDL_Point c = ring_centre(buttons_[hot]);
    const float wl = mech3::MenuRipple::kWavelength;
    zrndr::set_finish_target(static_cast<uint16_t*>(canvas_->pixels), W, H, canvas_->pitch);
    zrndr::ripple(c.x, c.y, static_cast<int32_t>(static_cast<float>(ring_radius_) - wl * 3.0f), ring_radius_,
                  static_cast<int32_t>(ring_amp_), wl, ring_phase_, nullptr, zrndr::X87::k53);
    drew_ripple_ = true;
  }
}

void Launcher::present() {
  SDL_UpdateTexture(tex_, nullptr, canvas_->pixels, canvas_->pitch);
  SDL_SetRenderDrawColor(ren_, 0, 0, 0, 255);
  SDL_RenderClear(ren_);
  SDL_RenderTexture(ren_, tex_, nullptr, nullptr);
  SDL_RenderPresent(ren_);
}

void Launcher::ask_file(int what, const SDL_DialogFileFilter* filters, int n) {
  {
    std::lock_guard<std::mutex> g(pick_.m);
    pick_.what = what, pick_.path.clear(), pick_.answered = pick_.cancelled = false;
  }
  asking_ = true, ask_shown_ = false, ask_at_ = SDL_GetTicksNS();
  std::printf("mw3: launcher: the file picker asked for\n");
  SDL_ShowOpenFileDialog(Pick::done, &pick_, win_, filters, n, nullptr, false);
}
void Launcher::ask_folder(int what) {
  {
    std::lock_guard<std::mutex> g(pick_.m);
    pick_.what = what, pick_.path.clear(), pick_.answered = pick_.cancelled = false;
  }
  asking_ = true, ask_shown_ = false, ask_at_ = SDL_GetTicksNS();
  std::printf("mw3: launcher: the folder picker asked for\n");
  SDL_ShowOpenFolderDialog(Pick::done, &pick_, win_, nullptr, false);
}

void Launcher::ask_save(int what, const std::string& file_name) {
  {
    std::lock_guard<std::mutex> g(pick_.m);
    pick_.what = what, pick_.path.clear(), pick_.answered = pick_.cancelled = false;
  }
  asking_ = true, ask_shown_ = false, ask_at_ = SDL_GetTicksNS();
  std::printf("mw3: launcher: the save box asked for\n");
  static const SDL_DialogFileFilter filters[] = {{"Zip archive (.zip)", "zip"}};
  const char* docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
  static std::string start;
  start = docs ? (fs::path(docs) / file_name).string() : file_name;
  SDL_ShowSaveFileDialog(Pick::done, &pick_, win_, filters, 1, start.c_str());
}

// A picked path: what the button that asked does with it.
void Launcher::picked(int what, const std::string& path) {
  using namespace mech3::setup;
  // Before Windows 10 1903 such a name cannot be used (docs/program.md, Names on Windows).
  if (!zutl::narrow_paths_utf8() && !std::all_of(path.begin(), path.end(), [](char c) { return static_cast<unsigned char>(c) < 0x80; })) {
    status_ = "This version of Windows needs a folder with plain English letters in its name.", status_good_ = false;
    layout();
    return;
  }
  if (what == kModExport) {  // said, drawn, then written (mods_export_pending)
    std::string zip = path;
    if (fs::path(zip).extension().empty()) zip += ".zip";  // a save box that did not add it
    status_ = "Exporting to " + fs::path(zip).filename().string() + "...", status_good_ = true;
    pending_export_ = zip, export_drawn_ = false;
    layout();
    return;
  }
  if (what == kModAdd || what == kModAddFolder) {  // said, drawn, then added (mods_add_pending)
    fs::path shown(path);
    while (!shown.empty() && shown.filename().empty()) shown = shown.parent_path();
    status_ = "Adding " + shown.filename().string() + "...", status_good_ = true;
    pending_add_ = path, add_drawn_ = false;
    layout();
    return;
  }
  if (what == kPm) {
    if (!is_pm_install(mech3::source::map_source(path))) {
      status_ = "That is not an installed Pirate's Moon: pick its folder, the one with Mech3.exe.", status_good_ = false;
      layout();
      return;
    }
    pm_dir_ = fs::is_regular_file(path) ? fs::path(path).parent_path().string() : path;
    save_config_value(o_.write_dir, "pm_install", pm_dir_);
    status_.clear();
    layout();
    return;
  }
  if (what == kMw1) {
    const std::string dir = fs::is_regular_file(path) ? fs::path(path).parent_path().string() : path;
    if (!mw1_game_valid(dir)) {
      status_ = "That folder has no BTECH.EXE: it is not MechWarrior (1989).", status_good_ = false;
      return;
    }
    mw1_game_ = dir;
    save_config_value(o_.write_dir, "mw1_game", dir);
    layout();
    run_mw1();
    return;
  }
  if (what == kDataPick)
    return ask_move(mech3::data_folder::destination(path, files_usual(), files_pm_ ? "Pirate's Moon Data" : "MechWarrior 3 Data"));
  if (what == kPoint) {  // an image or archive as it is, any other file for its folder
    look_at(pick_source(path, {"iso", "cue", "img", "zip", "7z", "rar", "tar", "gz", "tgz", "xz"}));
    return;
  } else if (what == kAddDisc) {
    add_disc(pick_source(path, {"iso", "cue", "img"}));
    return;
  } else if (what == kPatch) {  // unreachable (kPatch has no plate since the patch is built in); kept
    work_ = std::make_unique<Work>();
    const std::string wd = o_.write_dir;
    work_->start("APPLYING THE 1.2 PATCH", [path, wd](const ProgressFn& f) { return apply_patch(path, wd, f); });
    screen_ = kWorking;
  }
  layout();
}

// What the install lacks (the movies left on the CD, the music), taken from
// the disc, a disc image or the disc's folder (setup::add_from_disc).
void Launcher::add_disc(const std::string& source) {
  disc_back_ = screen_;
  work_ = std::make_unique<Work>();
  work_->kind = Work::kAddDisc;
  if (files_pm_) {  // its music and its sounds (no movies), into its own folder
    const std::string wd = pm_wd();
    const std::string music = pm_lacks_music_ ? (fs::path(wd) / "music").string() : std::string();
    const std::string sounds = pm_lacks_sounds_ ? wd : std::string();
    work_->start("ADDING FROM YOUR DISC", [source, music, sounds](const mech3::setup::ProgressFn& f) {
      return mech3::setup::add_pm_from_disc(source, music, sounds, f);
    });
    screen_ = kWorking, layout();
    return;
  }
  const std::string video = lacks_movies_ ? (fs::path(o_.write_dir) / "video").string() : std::string();
  const std::string music = lacks_music_ ? (fs::path(o_.write_dir) / "music").string() : std::string();
  work_->start("ADDING FROM YOUR DISC", [source, video, music](const mech3::setup::ProgressFn& f) {
    return mech3::setup::add_from_disc(source, video, music, f);
  });
  screen_ = kWorking;
  layout();
}

// The notes for the install now there (launcher_notes): what it lacks.
void Launcher::notes_refresh() {
  notes_.clear();
  lacks_music_ = lacks_movies_ = false;
  if (!install_.ok) return;
  const fs::path music = o_.write_dir.empty() ? fs::path("music") : fs::path(o_.write_dir) / "music";
  lacks_music_ = !have_music({music.string(), (zutl::any_case(install_.dir, "music").empty() ? fs::path(install_.dir) / "music" : fs::path(zutl::any_case(install_.dir, "music"))).string()});
  lacks_movies_ = !have_movies(install_.dir, o_.write_dir);
  const std::string dir = fs::path(music).make_preferred().string();
  for (const std::string& n : launcher_notes(install_.version, !lacks_music_, !lacks_movies_, dir))
    std::printf("mw3: launcher: note: %s\n", n.c_str());
  notes_ = launcher_notes(install_.version, !lacks_music_, !lacks_movies_, text_.fit(dir, 14, 440));  // a long folder shortened
  // The start check's notes on the same screen, so PLAY does not show a second one (mw3_ready).
  const mech3::setup::Result c = mech3::setup::start_check(install_.dir, o_.write_dir, false);
  mw3_check_notes_ = c.ok ? c.warnings : std::vector<std::string>{};
  for (const std::string& n : mw3_check_notes_) std::printf("mw3: launcher: note: %s\n", n.c_str()), notes_.push_back(n);
}

// The start check's notes counted as shown (mw3_notes=, as notes_once keeps them): the notes screen has them.
void Launcher::mw3_notes_seen() {
  std::string all;
  for (const std::string& n : mw3_check_notes_) all += n + "\n";
  char crc[16];
  std::snprintf(crc, sizeof crc, "%08x", zutl_zar::crc32(0, reinterpret_cast<const uint8_t*>(all.data()), all.size()));
  if (mw3_check_notes_.empty() || o_.cfg_mw3_notes == crc) return;
  o_.cfg_mw3_notes = crc;
  save_config_value(o_.write_dir, "mw3_notes", crc);
}

// Pirate's Moon's usual settings folder: beside MechWarrior 3's under
// MicroProse, as SDL_GetPrefPath makes it for mw3pm (not made here).
std::string Launcher::pm_usual() const {
  const std::string mw3 = usual_folder();
  return mw3.empty() ? std::string() : (fs::path(mw3).parent_path() / "Pirate's Moon").string();
}

// Its folder in use: MW3_PM_WRITE_DIR (checks), else the usual one or where
// its pointer leads (mech3/data_folder.h), as mw3pm resolves it.
std::string Launcher::pm_wd() {
  if (o_.pm_write_dir.empty() && !pm_usual().empty()) o_.pm_write_dir = mech3::data_folder::resolve(pm_usual()).dir;
  return o_.pm_write_dir;
}

// What Pirate's Moon lacks that its disc has: the music, a soundsH.zbd with
// sounds in it (setup::check_install's note).
void Launcher::pm_refresh() {
  pm_lacks_music_ = pm_lacks_sounds_ = false;
  if (pm_dir_.empty()) return;
  const std::string wd = pm_wd();
  const std::string own = zutl::any_case(pm_dir_, "music");
  pm_lacks_music_ = !have_music({(fs::path(wd) / "music").string(), own.empty() ? (fs::path(pm_dir_) / "music").string() : own});
  const mech3::setup::Result c = mech3::setup::check_install(pm_dir_, wd, true);
  pm_data_ = c.version;
  for (const std::string& w : c.warnings)
    if (w.find("soundsH.zbd") != std::string::npos) pm_lacks_sounds_ = true;
}

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
  pm_dir_ = dir, o_.cfg_pm_install = dir;
  save_config_value(o_.write_dir, "pm_install", dir);
  status_ = summary, status_good_ = true;
  const fs::path own = fs::path(pm_wd()) / "install";
  std::error_code ec;
  if (!pm_wd().empty() && !before.empty() && fs::equivalent(before, own, ec) && !fs::equivalent(dir, own, ec) &&
      fs::exists(own / "mw3-install.txt", ec)) {
    std::printf("mw3: launcher: the earlier Pirate's Moon copy in %s is no longer used: removed\n", own.string().c_str());
    fs::remove_all(own, ec);
  }
  pm_refresh();
  if (from_files_) {
    from_files_ = false;
    screen_ = kMain, layout();
    return;
  }
  start_pm();
}

// GAME FILES' line: the install in use and what it lacks.
std::string Launcher::in_use() const {
  std::string s = "Using MechWarrior 3 " + install_.version + " at " + install_.dir;
  if (lacks_movies_ && lacks_music_) s += ". No movies or music.";
  else if (lacks_movies_) s += ". No movies.";
  else if (lacks_music_) s += ". No music.";
  return s;
}

// MechWarrior 3 started: by mw3launcher as the game's own program, else
// (a plain mw3 start, or no game_bin) the window handed on.
void Launcher::play() {
  if (!mw3_ready()) return;
  // The movies converted here first, with the bar (Owen: a first start sat
  // 44 s on "Starting MechWarrior 3..." while mw3 did it); mw3's own import
  // stays the fallback for a start without the launcher.
  if (!converted_ && movies_to_convert() > 0) {
    converted_ = true;
    convert_movies([this] { play(); });
    return;
  }
  if (!o_.setup_only && !o_.game_bin.empty()) {
    screen_ = kMain, layout();
    if (!game_) start_game();
    return;
  }
  choice_ = {LauncherChoice::kPlay, install_.dir};
  done_ = true;
}

// The movies the install and the write dir (ADD FROM MY DISC's) have not
// converted for the player (mw3import); none on Windows, whose helper plays
// the AVIs as they are, or without the importer or with MW3_NO_IMPORT.
int Launcher::movies_to_convert() const {
#if defined(_WIN32)
  return 0;
#else
  if (!install_.ok || o_.no_import || SDL_getenv("MW3_NO_IMPORT") || importer_path().empty()) return 0;
  std::error_code ec;
  const bool same = fs::equivalent(install_.dir, o_.write_dir, ec);
  return movies_missing(install_.dir, o_.write_dir) + (same ? 0 : movies_missing(o_.write_dir, o_.write_dir));
#endif
}

// mw3import on the worker, the bar following its "progress" lines; CANCEL
// stops it (the movies done stay; the rest wait for the next start).
void Launcher::convert_movies(std::function<void()> then) {
  after_convert_ = std::move(then);
  work_ = std::make_unique<Work>();
  work_->kind = Work::kConvert;
  const std::string install = install_.dir, wd = o_.write_dir;
  std::error_code ec;
  const bool same = fs::equivalent(install, wd, ec);
  work_->start("CONVERTING THE MOVIES", [install, wd, same](const mech3::setup::ProgressFn& f) {
    mech3::setup::Result r;
    const int a = movies_missing(install, wd), b = same ? 0 : movies_missing(wd, wd), total = a + b;
    int base = 0, code = 0;
    for (const auto& [src, n] : {std::pair<std::string, int>{install, a}, std::pair<std::string, int>{wd, b}}) {
      if (n == 0) continue;
      code = run_importer(src, wd, [&](int done, int, const std::string& name) {
        return f(mech3::setup::Progress{"Converting the movies (" + std::to_string(base + done) + " of " + std::to_string(total) + ")",
                                        total ? static_cast<double>(base + done) / total : 0, name});
      });
      if (code != 0) break;
      base += n;
    }
    r.ok = code == 0, r.cancelled = code == 1, r.movies = total;
    r.summary = r.ok          ? std::to_string(total) + " movies converted for this computer."
                : r.cancelled ? "Converting the movies was stopped; the rest are done when the game starts."
                              : "Some movies could not be converted; the game plays without them.";
    return r;
  });
  screen_ = kWorking, layout();
}

bool Launcher::data_elsewhere() {
  std::error_code ec;
  const std::string usual = files_usual(), wd = files_wd();
  return !usual.empty() && !fs::equivalent(wd, usual, ec) && wd != usual;
}

// The player's folder to `to` (docs/program.md, The data folder): asked first,
// with the sizes; a move that cannot be done is said and nothing starts.
void Launcher::ask_move(const std::string& to) {
  const mech3::data_folder::Plan p = mech3::data_folder::plan(files_wd(), to, files_usual());
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
  work_->start("MOVING YOUR DATA", [from, to, usual, copied](const mech3::setup::ProgressFn& f) {
    mech3::setup::Result r;
    const mech3::data_folder::Moved m = mech3::data_folder::move_data(from, to, usual, [&](double at, const std::string& name) {
      return f(mech3::setup::Progress{"Copying and checking", at, name});
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
void Launcher::moved(const mech3::setup::Result& r) {
  if (!r.ok) {
    status_ = r.summary, status_good_ = r.cancelled;
    screen_ = kDataScreen, layout();
    return;
  }
  move_copied_ = *move_copied_out_;
  using mech3::data_folder::moved_path;
  const std::string from = move_from_, to = move_to_;
  if (files_pm_) {  // Pirate's Moon's: its install inside it follows; mw3pm finds the folder by its pointer
    const std::string dir = moved_path(pm_dir_, from, to);
    if (dir != pm_dir_) pm_dir_ = o_.cfg_pm_install = dir, save_config_value(o_.write_dir, "pm_install", dir);
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

// A found install used: saved to mw3.cfg, then straight on into the game
// (Owen, 2026-11-02); when it lacks the movies or the music, or its data is
// odd, the notes first, their OK the start (lead).
void Launcher::use_install(const std::string& dir, const std::string& version, const std::string& summary) {
  const std::string before = install_.ok ? install_.dir : std::string();
  install_ = {true, dir, version, summary};
  save_install(o_.write_dir, dir);
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
  if (!notes_.empty()) {
    play_after_notes_ = true, status_.clear();
    mw3_notes_seen();
    screen_ = kNotes, layout();
    if (auto_ok_) {  // checks (MW3_LAUNCHER_PICK): the notes' OK pressed
      auto_ok_ = false;
      activate(kNotesOk);
    }
    return;
  }
  play();
}

// What the player pointed at, looked into on the worker (an archive's look
// can take seconds: the MW3 disc's installer header is 209 MB into its image).
void Launcher::look_at(const std::string& path) {
  pending_source_ = path;
  found_ = std::make_shared<mech3::source::Found>();
  work_ = std::make_unique<Work>();
  work_->kind = Work::kLook;
  auto found = found_;
  const std::string name = fs::path(path).filename().string();
  const bool pm = files_pm_;  // the game GAME FILES is for; the other one is said, not taken
  work_->start("LOOKING AT YOUR GAME", [path, found, name, pm](const mech3::setup::ProgressFn& f) {
    using mech3::source::Found;
    *found = mech3::source::map_source(path, [&](double at, const std::string& entry) {
      return f(mech3::setup::Progress{"Looking into " + (name.empty() ? path : name), at, entry});
    });
    mech3::setup::Result r;
    r.ok = found->game == (pm ? Found::kPiratesMoon : Found::kMw3), r.cancelled = found->cancelled;
    r.summary = found->summary, r.detail = found->detail;
    if (pm && found->game == Found::kMw3)
      r.summary = "This is MechWarrior 3, not Pirate's Moon: give it under MECHWARRIOR 3's GAME FILES. Pick Pirate's "
                  "Moon's own disc, image or folder here.";
    else if (pm && r.ok)
      r.summary = found->kind == Found::kInstall ? "An installed copy of Pirate's Moon." : "The Pirate's Moon disc.";
    return r;
  });
  screen_ = kWorking;
  layout();
}

// The automatic search: an installed copy (mw3.cfg's, the installer's
// places), else a disc in a drive (looked into as if picked).
void Launcher::search() {
  if (files_pm_) {  // Pirate's Moon's: found again where it is, as at the start
    const std::string had = pm_dir_;
    pm_dir_.clear(), o_.cfg_pm_install = had;
    find_pm();
    if (!pm_dir_.empty()) return use_pm(pm_dir_, "Found Pirate's Moon at " + pm_dir_ + ".");
  } else {
    find_install();
  }
  if (!files_pm_ && install_.ok) {
    use_install(install_.dir, install_.version, install_.summary);
    return;
  }
  const std::vector<mech3::setup::DiscSource> discs = mech3::setup::find_discs();
  if (!discs.empty()) {
    look_at(discs.front().path);
    return;
  }
  status_ = std::string(files_pm_ ? "Pirate's Moon was not" : "Not") +
            " found on this computer or in a disc drive. Pick it: a disc image, a download, or Mech3.exe.";
  status_good_ = true;
  screen_ = kSetupScreen;
  layout();
  ask_file(kPoint, kGameFiles, 2);  // the one picker (an installed copy by any of its files)
}

void Launcher::finish_work() {
  mech3::setup::Result r;
  {
    std::lock_guard<std::mutex> g(work_->m);
    r = work_->result;
  }
  if (work_->th.joinable()) work_->th.join();
  const Work::Kind kind = work_->kind;
  work_.reset();
  std::printf("mw3: launcher: %s%s%s\n", r.summary.c_str(), r.detail.empty() ? "" : " | ", r.detail.c_str());
  if (kind == Work::kRemoveData) {
    status_ = r.summary, status_good_ = r.ok;
    from_files_ = false, screen_ = kMain, layout();
    return;
  }
  if (kind == Work::kMoveData) return moved(r);
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
    const Screen back = disc_back_ == kSetupScreen ? kSetupScreen : kNotes;
    if (r.ok && r.movies > 0 && movies_to_convert() > 0) {  // the movies just added, converted at once
      const std::string said = status_;
      convert_movies([this, back, said] { status_ = said, screen_ = back, layout(); });
      return;
    }
    screen_ = back, layout();
    return;
  }
  if (kind == Work::kLook) {
    const mech3::source::Found& f = *found_;
    if (f.cancelled || !r.ok) {
      status_ = r.summary, status_good_ = f.cancelled;
      screen_ = kSetupScreen;
    } else if (f.kind == mech3::source::Found::kInstall && f.on_disk) {  // used where it is
      const mech3::setup::Result c = mech3::setup::check_install(f.path, files_wd(), files_pm_);
      const std::string dir = c.install_dir.empty() ? f.path : c.install_dir;
      if (c.ok && files_pm_) return use_pm(fs::is_regular_file(dir) ? fs::path(dir).parent_path().string() : dir, c.summary);
      if (c.ok) return use_install(dir, c.version, c.summary);
      status_ = c.summary, status_good_ = false, screen_ = kSetupScreen;
    } else {
      const std::string out = (fs::path(files_wd()) / "install").make_preferred().string();
      confirm_text_ = (files_pm_ ? r.summary : f.summary) + "\n\nIt will be installed into " + out + ". Your own files are only read.";
      confirm_go_ = 1;
      screen_ = kConfirm;
      if (auto_go_) {
        auto_go_ = false;
        activate(kGo);
        return;
      }
    }
    layout();
    return;
  }
  status_ = r.summary, status_good_ = r.ok;
  for (const std::string& w : r.warnings) status_ += " " + w;
  if (files_pm_ && kind == Work::kInstallIt) {  // Pirate's Moon's own folder, never MW3's install=
    if (r.ok) return use_pm(r.install_dir, status_);
    screen_ = kSetupScreen, layout();
    return;
  }
  if (r.ok && !r.install_dir.empty()) save_install(o_.write_dir, r.install_dir);
  find_install();
  if (kind == Work::kInstallIt && r.ok && install_.ok) return use_install(install_.dir, install_.version, status_);
  screen_ = install_.ok && r.ok ? kMain : kSetupScreen;
  layout();
}

void Launcher::mods_refresh() {
  const ModsTarget t = mods_target(o_, false);
  mods_ = mech3::mods_list(t.write_dir, mech3::mods_enabled(t.write_dir), mods_checked_, install_.dir);
  if (mod_sel_ >= static_cast<int>(mods_.size())) mod_sel_ = static_cast<int>(mods_.size()) - 1;
  if (mod_page_ * kModsPerPage >= static_cast<int>(mods_.size())) mod_page_ = 0;
}

// The new order (the enabled, as they win) written to mw3.cfg; the list read again.
void Launcher::mods_save(std::vector<std::string> enabled, const std::string& said) {
  const bool ok = mech3::mods_save_enabled(mods_target(o_, false).write_dir, enabled);
  std::printf("mw3: launcher: mods: %s\n", ok ? said.c_str() : "mw3.cfg could not be written");
  status_ = ok ? said + ". It counts from the next start of the game." : "Could not write mw3.cfg: nothing changed.";
  status_good_ = ok;
  mods_refresh();
}

// A row's second line: on or off, then why it cannot be used, what an
// earlier mod takes from it, or CHECK's verdict.
// ADD MOD's pick, once "Adding ..." has been drawn: the game's own add
// (mech3::mod_add_run), the mod shown selected and the result said.
void Launcher::mods_add_pending() {
  const std::string path = pending_add_;
  pending_add_.clear();
  const ModsTarget t = mods_target(o_, false);
  std::vector<std::string> on = mech3::mods_enabled(t.write_dir);
  const mech3::ModAddDone d =
      mech3::mod_add_run(t.write_dir, path, install_.dir,
                         [this](const std::string& n) { return o_.shot.empty() ? ask_mod_game(win_, n) : 1; },  // a scripted run: no box
                         &on);
  std::printf("mw3: launcher: mods: %s\n", d.message.c_str());
  status_ = d.message, status_good_ = d.ok;
  mods_refresh();
  for (size_t k = 0; k < mods_.size(); ++k)
    if (mods_[k].info.folder == d.folder) mod_sel_ = static_cast<int>(k), mod_page_ = mod_sel_ / kModsPerPage;
  layout();
}

// EXPORT's zip, once "Exporting ..." has been drawn: the game's own export
// (mech3::mod_export) of the mod chosen when the save box opened.
void Launcher::mods_export_pending() {
  const std::string zip = pending_export_;
  pending_export_.clear();
  const ModsTarget t = mods_target(o_, false);
  const mech3::ModExportResult r = mech3::mod_export((fs::path(t.write_dir) / "mods" / export_folder_).string(), zip);
  export_folder_.clear();
  std::printf("mw3: launcher: mods: %s\n", r.message.c_str());
  status_ = r.message, status_good_ = r.ok;
  layout();
}

std::string Launcher::mod_line(const mech3::ModsListEntry& e) const {
  if (!e.info.error.empty()) return "Not usable: " + e.info.error;
  std::string s = e.on ? "ON" : "OFF";
  s += " - " + e.info.folder;
  if (!e.conflicts_with.empty()) {
    std::string with;
    for (const std::string& c : e.conflicts_with) with += (with.empty() ? "" : ", ") + c;
    s += " - shares " + std::to_string(e.shadowed.size() + e.rows_taken.size()) + " with " + with + " (above wins)";
  }
  if (e.check_errors == 0) s += " - check: fine";
  else if (e.check_errors > 0) s += " - check: " + std::to_string(e.check_errors) + " the game would refuse (mw3.log)";
  return s;
}

void Launcher::activate(int id) {
  sounds_.play(Sounds::kSelect);
  if (id >= kModRow && id < kModRow + kModsPerPage) {  // a mod's row: selected, and turned on or off
    const int i = mod_page_ * kModsPerPage + (id - kModRow);
    if (i >= static_cast<int>(mods_.size())) return;
    mod_sel_ = i;
    std::vector<std::string> on;
    for (const mech3::ModsListEntry& e : mods_)
      if (e.on && &e != &mods_[i]) on.push_back(e.info.folder);
    const bool now = !mods_[i].on;
    const std::string folder = mods_[i].info.folder;
    if (now) on.push_back(folder);  // turned on: last, below the others
    mods_save(on, folder + (now ? " on" : " off"));
    for (size_t k = 0; k < mods_.size(); ++k)
      if (mods_[k].info.folder == folder) mod_sel_ = static_cast<int>(k);
    layout();
    return;
  }
  static const SDL_DialogFileFilter kZip[] = {{"The 1.2 patch (.zip, .exe)", "zip;exe"}, {"All files", "*"}};
  switch (id) {
    case kMw3:
      pm_last_ = files_pm_ = false;
      if (install_.ok) play();
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
      work_->start("REMOVING THE OLD COPY", [old, now, usual, copied](const mech3::setup::ProgressFn&) {
        mech3::setup::Result r;
        const mech3::data_folder::Removed d = mech3::data_folder::remove_old(old, now, copied, usual);
        r.ok = d.error.empty() && d.failed.empty();
        r.summary = !d.error.empty()     ? d.error
                    : d.failed.empty()   ? "The old copy is removed."
                                         : std::to_string(d.failed.size()) + " files of the old copy could not be removed, in " + old + ".";
        return r;
      });
      screen_ = kWorking, layout();
      break;
    }
    case kKeepOld:
      status_ = "The old copy stays in " + move_from_ + ".", status_good_ = true;
      from_files_ = false, screen_ = kMain, layout();
      break;
    case kMods:
      screen_ = kModsScreen, status_.clear(), mods_checked_ = false, mod_sel_ = -1, mod_page_ = 0;
      mods_refresh();
      layout();
      break;
    case kModUp:
    case kModDown: {  // the selected (enabled) mod a place earlier or later in mw3.cfg
      std::vector<std::string> on;
      for (const mech3::ModsListEntry& e : mods_)
        if (e.on) on.push_back(e.info.folder);
      if (mod_sel_ < 0 || mod_sel_ >= static_cast<int>(on.size())) break;
      const int to = mod_sel_ + (id == kModUp ? -1 : 1);
      if (to < 0 || to >= static_cast<int>(on.size())) break;
      std::swap(on[mod_sel_], on[to]);
      mod_sel_ = to;
      mods_save(on, on[to] + (id == kModUp ? " moved up" : " moved down"));
      mod_page_ = mod_sel_ / kModsPerPage;
      layout();
      break;
    }
    case kModAdd: {
      static const SDL_DialogFileFilter filters[] = {{"Mods (.zip, .7z, .rar)", "zip;7z;rar"}};
      ask_file(kModAdd, filters, 1);
      break;
    }
    case kModAddFolder:
      ask_folder(kModAddFolder);
      break;
    case kModExport:
      if (mod_sel_ < 0 || mod_sel_ >= static_cast<int>(mods_.size())) break;
      export_folder_ = mods_[mod_sel_].info.folder;
      if (!o_.save_pick.empty()) {  // --launcher-save-pick: no box
        picked(kModExport, o_.save_pick);
        break;
      }
      ask_save(kModExport, mech3::mod_export_file_name(mods_[mod_sel_].info));
      break;
    case kModFolder: {
      const std::string where = mods_target(o_, false).write_dir + "/mods";
      std::error_code ec;
      fs::create_directories(where, ec);
      if (!open_folder(where)) status_ = "The mods folder: " + where, status_good_ = true, layout();
      break;
    }
    case kModCheck:
      mods_checked_ = true;
      mods_refresh();
      status_ = "Checked: each mod read as the game would read it", status_good_ = true;
      layout();
      break;
    case kModPrev:
    case kModNext:
      mod_page_ = std::max(0, mod_page_ + (id == kModNext ? 1 : -1));
      layout();
      break;
    case kFiles:  // the find screen at any time (Owen): repick or refresh the game's files
      files_pm_ = pm_last_ && !o_.pm_bin.empty();  // the game last clicked
      if (files_pm_) pm_refresh();
      from_files_ = true, status_.clear();
      screen_ = kSetupScreen, layout();
      break;
    case kPm:
      if (o_.pm_bin.empty()) break;
      pm_last_ = true;
      if (pm_dir_.empty()) {  // its files to find first, as MechWarrior 3's
        files_pm_ = true, from_files_ = false, status_.clear();
        screen_ = kSetupScreen, layout();
      } else {
        start_pm();
      }
      break;
    case kMw1:
      if (mw1_game_.empty()) {
        status_ = "Pick your MechWarrior (1989) folder: the one with BTECH.EXE.", status_good_ = true;
        ask_folder(kMw1);
      } else {
        run_mw1();
      }
      break;
    case kBack:
      if (screen_ == kNotes) return activate(kNotesOk);  // Escape on the notes: as OK
      if (o_.setup_only && screen_ == kSetupScreen) done_ = true;  // a plain start's QUIT
      else if (screen_ == kConfirm && confirm_go_ == 3) return activate(kKeepOld);  // Escape: nothing removed
      else if (screen_ == kConfirm && confirm_go_ == 2) screen_ = kDataScreen, layout();
      else if (screen_ == kConfirm) screen_ = kSetupScreen, layout();
      else if (screen_ == kDataScreen) screen_ = kSetupScreen, status_.clear(), layout();
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
    case kAddDisc: {  // a disc in a drive first, then one picker: its image or any file of the disc
      const std::vector<mech3::setup::DiscSource> discs = mech3::setup::find_discs();
      if (!discs.empty()) {
        add_disc(discs.front().path);
      } else {
        status_ = "No disc in a drive. Pick its image (.cue .iso) or any file of the disc.", status_good_ = true;
        layout();
        ask_file(kAddDisc, kDiscFiles, 2);
      }
      break;
    }
    case kPatch: ask_file(kPatch, kZip, 2); break;  // unreachable: no plate (the patch is built in)
    case kGo: {
      work_ = std::make_unique<Work>();
      work_->kind = Work::kInstallIt;
      const auto found = found_;
      const std::string wd = files_wd(), out = (fs::path(wd) / "install").string(), music = (fs::path(wd) / "music").string();
      work_->start(files_pm_ ? "INSTALLING PIRATE'S MOON" : "INSTALLING MECHWARRIOR 3", [found, out, music, wd](const mech3::setup::ProgressFn& f) {
        return mech3::setup::install_from_source(*found, out, "", music, wd, f);
      });
      screen_ = kWorking, layout();
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
    focus_ = f, hot_t_ = 0;
    if (f >= 0) sounds_.play(Sounds::kRollover);
  } else if (action == 2 && at >= 0 && at < n && buttons_[at].enabled) {
    activate(buttons_[at].id);
  } else if (action == 3) {
    activate(screen_ == kMain ? kQuit : kBack);
  }
}

LauncherChoice Launcher::run() {
  const bool shot = !o_.shot.empty();
  // Where the start's time goes (Owen: "pretty slow"), in the log.
  const Uint64 t0 = SDL_GetTicksNS();
  auto ms = [](Uint64 a, Uint64 b) { return static_cast<double>(b - a) / 1e6; };
  const std::string dir = assets_dir();
  art_.load(dir);
  const Uint64 t_art = SDL_GetTicksNS();
  const std::string font = font_path(dir);
  text_.open(font);
  std::printf("mw3: launcher: font %s\n", font.empty() ? "none found: its text will be blank" : font.c_str());
  canvas_ = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_RGB565);
  const Uint64 t_font = SDL_GetTicksNS();
  find_install();
  const Uint64 t_install = SDL_GetTicksNS();
  find_mw1();
  find_pm();
  pm_refresh();
  // Each game's program's own version, read from its file (the package's
  // programs come from different tags: mw3 and this one from main's, mw3pm
  // from pm's).
  mw3_program_ = o_.game_bin.empty() ? std::string(MW3_VERSION) : program_version(o_.game_bin);
  if (!o_.pm_bin.empty()) pm_program_ = program_version(o_.pm_bin);
  std::printf("mw3: launcher: programs: MechWarrior 3 %s", mw3_program_.empty() ? "unknown" : mw3_program_.c_str());
  if (!o_.pm_bin.empty()) std::printf(", Pirate's Moon %s", pm_program_.empty() ? "unknown" : pm_program_.c_str());
  std::printf("\n");
  std::printf("mw3: launcher: art %.0f ms, font %.0f ms, the install search %.0f ms\n", ms(t0, t_art), ms(t_art, t_font),
              ms(t_font, t_install));
  sounds_on_ = o_.sound && !shot;
  // The first start's notes (no install= in mw3.cfg yet: PLAY saves one),
  // on their own screen before the main one; logged on every start.
  notes_refresh();
  const bool first_start = o_.cfg_install.empty() && !notes_.empty();
  if (shot) {
    if (o_.screen == "setup" || o_.screen == "pick" || o_.screen == "files")
      screen_ = kSetupScreen, from_files_ = o_.screen == "files";
    if (o_.screen == "pick") status_ = "Not found on this computer or in a disc drive. Pick it: a disc image, a download, or Mech3.exe.";
    if (o_.screen == "notes" || o_.screen == "disc") screen_ = kNotes;
    if (o_.screen == "data") screen_ = kDataScreen;
    if (o_.screen == "mods") screen_ = kModsScreen, mods_refresh();
    if (o_.screen == "mods" && !o_.pick.empty()) pending_add_ = o_.pick, mods_add_pending();  // --launcher-pick
    if (o_.screen == "disc") status_ = "No disc in a drive. Pick its image (.cue .iso) or any file of the disc.";
    if (o_.screen == "work") {
      screen_ = kWorking;
      work_ = std::make_unique<Work>();
      work_->title = "INSTALLING MECHWARRIOR 3";
      work_->progress = {"Copying the movies", 0.62, "VIDEO\\C1M3.AVI"};
    }
    layout();
    if (!buttons_.empty()) hot_ = 0, hot_t_ = 0.4f;
    scroll_ = 120.0f;
    const Uint64 td = SDL_GetTicksNS();
    draw(0.0f);
    for (int i = 0; i < 6; ++i) draw(1.0f / 30.0f);  // 0.2 s on: the hovered entry's ripple spreading
    if (hot_ >= 0)
      std::printf("mw3: launcher: hover %s:%s%s\n", buttons_[hot_].label.c_str(), drew_ripple_ ? " ripple" : "", drew_edge_ ? " edge" : "");
    std::printf("mw3: launcher: first frame drawn %.0f ms after the start (its draw %.1f ms)\n", ms(t0, SDL_GetTicksNS()),
                ms(td, SDL_GetTicksNS()));
    SDL_SaveBMP(canvas_, o_.shot.c_str());
    work_.reset();
    return {};
  }
  // A game without its files opens the find screen only when it is chosen;
  // a plain mw3 start that found none shows that screen alone.
  if (o_.setup_only) screen_ = kSetupScreen;
  else if (install_.ok && first_start) screen_ = kNotes, mw3_notes_seen();
  layout();
  // Not high pixel density: the game takes this window (one window into the
  // game), and its sizes are the window's points (SdlWindow, SdlInput).
  SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
  if (o_.fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
  if (o_.hidden) flags |= SDL_WINDOW_HIDDEN;
  win_ = SDL_CreateWindow("MechWarrior 3", o_.win_w, o_.win_h, flags);
  if (!win_ || !(ren_ = SDL_CreateRenderer(win_, nullptr))) {
    std::fprintf(stderr, "mw3: launcher: no window (%s): straight to the game\n", SDL_GetError());
    return install_.ok ? LauncherChoice{LauncherChoice::kPlay, install_.dir} : LauncherChoice{};
  }
  SDL_SetRenderLogicalPresentation(ren_, W, H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  // Paced by the display (Owen: "pretty slow"): a fixed SDL_Delay(15) was
  // 16 to 31 ms on Windows' 15.6 ms timer, and no vsync let frames tear.
  // MW3_LAUNCHER_NOVSYNC (checks): the timed sleep instead, to compare.
  vsync_ = !SDL_getenv("MW3_LAUNCHER_NOVSYNC") && SDL_SetRenderVSync(ren_, 1);
  tex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, W, H);
  SDL_SetTextureScaleMode(tex_, SDL_SCALEMODE_LINEAR);
  Uint64 last = SDL_GetTicks();
  SDL_InitSubSystem(SDL_INIT_GAMEPAD);  // a pad steps through the plates too (nav)
  if (o_.play) auto_ok_ = true, activate(kMw3);  // its notes' OK pressed too
  // Checks: keys or a pad as a list ("next,next,press"), each step logged.
  if (const char* steps = SDL_getenv("MW3_LAUNCHER_NAV")) {
    std::string all = steps;
    for (size_t at = 0; at <= all.size();) {
      const size_t comma = std::min(all.find(',', at), all.size());
      const std::string s = all.substr(at, comma - at);
      nav(s == "next" ? 1 : s == "prev" ? -1 : s == "press" ? 2 : s == "back" ? 3 : 0);
      const int f = focus_;
      if (f >= 0 && f < static_cast<int>(buttons_.size()) && s != "press" && s != "back")
        std::printf("mw3: launcher: focus %s\n", buttons_[f].label.c_str());
      at = comma + 1;
    }
  }
  // Checks: GAME FILES opened on Pirate's Moon, as after a click on it.
  if (const char* g = SDL_getenv("MW3_LAUNCHER_FILES"); g && std::strcmp(g, "pm") == 0) pm_last_ = true, activate(kFiles);
  // Checks: a pick as if made in the find screen's dialog (after any NAV
  // steps: GAME FILES), INSTALL and the notes' OK pressed.
  if (const char* to = SDL_getenv("MW3_LAUNCHER_MOVE_TO")) {  // checks: the data folder's move, its MOVE pressed
    from_files_ = true;
    picked(kDataPick, to);
    if (screen_ == kConfirm && confirm_go_ == 2) activate(kMoveGo);
  }
  if (const char* add = SDL_getenv("MW3_LAUNCHER_ADD")) add_disc(add);  // checks: ADD FROM MY DISC's pick
  const char* pick = SDL_getenv("MW3_LAUNCHER_PICK");
  // --launcher-pm with none yet and a pick: its find screen, the pick installed, then started.
  const bool pm_pick = o_.play_pm && !o_.pm_bin.empty() && pm_dir_.empty() && pick;
  if (pm_pick) activate(kPm), o_.quit_after_game = true;
  if (pick && (screen_ == kSetupScreen || !install_.ok)) auto_go_ = auto_ok_ = true, look_at(pick);
  if (o_.play_mw1) activate(kMw1), done_ = !asking_;
  if (o_.play_pm && !pm_pick) {  // never the picker: a check has nobody to answer it
    if (pm_dir_.empty()) done_ = true;
    else auto_ok_ = true, activate(kPm), o_.quit_after_game = true, done_ = !game_;  // its notes' OK; refused: quit
  }
  while (!done_) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      SDL_ConvertEventToRenderCoordinates(ren_, &e);
      if (e.type == SDL_EVENT_QUIT) done_ = true;
      if (asking_ && !ask_shown_ && e.type == SDL_EVENT_WINDOW_FOCUS_LOST) {  // the panel up (a sheet takes the focus)
        ask_shown_ = true;
        std::printf("mw3: launcher: the picker took the focus %.0f ms after it was asked for\n",
                    static_cast<double>(SDL_GetTicksNS() - ask_at_) / 1e6);
      }
      if (game_) continue;  // the game is starting or running
      if (asking_ || screen_ == kWorking) {
        if (screen_ == kWorking && e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT)
          for (const Button& b : buttons_)
            if (e.button.x >= b.r.x && e.button.x < b.r.x + b.r.w && e.button.y >= b.r.y && e.button.y < b.r.y + b.r.h) activate(b.id);
        continue;
      }
      if (e.type == SDL_EVENT_MOUSE_MOTION) {
        int h = -1;
        for (size_t i = 0; i < buttons_.size(); ++i) {
          const SDL_Rect& r = buttons_[i].r;
          if (e.motion.x >= r.x && e.motion.x < r.x + r.w && e.motion.y >= r.y && e.motion.y < r.y + r.h) h = static_cast<int>(i);
        }
        if (h != hot_) {
          hot_ = h, hot_t_ = 0, focus_ = -1;
          if (h >= 0 && buttons_[h].enabled) sounds_.play(Sounds::kRollover);
        }
      } else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
        if (hot_ >= 0 && buttons_[hot_].enabled) activate(buttons_[hot_].id);
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
    if (asking_) {
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
        // (Owen: the picker took a long time to open).
        SDL_WaitEventTimeout(nullptr, 50);
        continue;
      }
    }
    if (work_ && work_->done) finish_work();
    watch_game();
    const Uint64 now = SDL_GetTicks();
    const float dt = std::min(0.1f, static_cast<float>(now - last) / 1000.0f);
    last = now;
    const Uint64 ta = SDL_GetTicksNS();
    draw(dt);
    const Uint64 tb = SDL_GetTicksNS();
    present();
    if (!pending_add_.empty()) {  // IMPORT: a frame with "Adding ..." shown first
      if (add_drawn_) mods_add_pending();
      else add_drawn_ = true;
    }
    if (!pending_export_.empty()) {  // EXPORT: a frame with "Exporting ..." shown first
      if (export_drawn_) mods_export_pending();
      else export_drawn_ = true;
    }
    const Uint64 tc = SDL_GetTicksNS();
    if (!frames_) std::printf("mw3: launcher: first frame shown %.0f ms after the start\n", ms(t0, tc));
    ++frames_, draw_ms_ += ms(ta, tb), present_ms_ += ms(tb, tc);
    if (quit_after_ > 0 && frames_ >= quit_after_) done_ = true;  // MW3_LAUNCHER_FRAMES (checks)
    if (!vsync_) {  // no vsync: the rest of a 60th of a second, to the nanosecond
      const Uint64 spent = SDL_GetTicksNS() - ta, frame = 1000000000ull / 60;
      if (spent < frame) SDL_DelayPrecise(frame - spent);
    }
  }
  if (frames_) {
    const double secs = ms(t0, SDL_GetTicksNS()) / 1000.0;
    std::printf("mw3: launcher: %d frames, %.1f a second; the draw %.2f ms, the present %.2f ms a frame (%s)\n", frames_,
                frames_ / std::max(secs, 0.001), draw_ms_ / frames_, present_ms_ / frames_, ren_ ? SDL_GetRendererName(ren_) : "-");
    std::printf("mw3: launcher: paced by %s\n", vsync_ ? "vsync" : "a timed sleep to 60 a second");
  }
  if (work_) work_->cancel = true;
  if (game_) {  // quit while the game runs: it goes on, we stop watching
    SDL_DestroyProcess(game_);
    game_ = nullptr;
  }
  if (choice_.kind == LauncherChoice::kPlay) {
    save_install(o_.write_dir, choice_.install);
    // The window goes on to the game, its renderer freed first (a window
    // drawn by an SDL renderer cannot be the GPU device's too).
    if (tex_) SDL_DestroyTexture(tex_), tex_ = nullptr;
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

LauncherChoice run_launcher(const LauncherOptions& o) {
  Launcher l(o);
  return l.run();
}

ModsTarget mods_target(const LauncherOptions& o, bool pm) {
  if (pm) return {};  // Pirate's Moon's own folder and for=pm: with its branch
  return {o.write_dir, "mw3"};
}

}  // namespace sdlfe
