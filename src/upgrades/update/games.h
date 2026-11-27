#pragma once
// Ours, not the original's: the launcher downloading each game from its own repo's Releases (2026-11-22,
// docs/launcher.md, Games on demand). Each card asks its game's repo for the newest release; nothing is installed
// until the player presses INSTALL or UPDATE. This part is the logic only (which repo, tag and file, the record of
// an installed game, a card's verdict); the fetching and unpacking are the front end's (frontend/sdl/sdl_games.h).
#include <string>
#include <vector>

#include "upgrades/update/update.h"

namespace upgrades::update {

// kMw1r: MechWarrior 1 Rebuild (mech1-re), the 1989 game rebuilt, MW1's card's ORIGINAL tab (2026-11-22).
// kChi, kChirm: Inception Rebuild (inception-re) and Inception Remake (inception-remake-re), the INCEPTION card's
// tabs: COMING SOON until their repos have a release, then INSTALL, with no launcher change (2026-11-22).
enum class Game { kMw3, kPm, kMw1, kMw1r, kChi, kChirm };
constexpr Game kGames[] = {Game::kMw3, Game::kPm, Game::kMw1, Game::kMw1r, Game::kChi, Game::kChirm};
constexpr int kGameCount = 6;

// The games catalog (docs/launcher-split.md, b0; 2026-11-26): the families and the games as data, games.json, built
// into the launcher. A game's options and notes are not in it: they are its own --status's.
struct CatalogFamily {
  std::string key, name;  // "mw3", "MechWarrior 3"
  int order = 0;
};
struct CatalogGame {
  std::string key;          // "pm": its folder under the games folder, its record's game=, its page (game:<key>)
  std::string family;       // a CatalogFamily's key
  int order = 0;            // its panel's place, top to bottom
  enum State { kComingSoon, kReleased, kRemoved } state = kReleased;  // read only, until the fetched catalog (b7)
  std::string name;         // "Pirate's Moon", for the words
  std::string title;        // its panel's name: "PIRATE'S MOON"
  std::string heading;      // its screens' header: "MECHWARRIOR 3: PIRATE'S MOON"
  std::string button;       // its panel button's name (the log's, a pad's): "PIRATE'S MOON"
  std::string description;  // its panel's blurb
  std::string repo;         // its GitHub repo (telltaleatheist/<repo>)
  std::string tag_prefix;   // "pm-v": its tags are this and X.Y.Z
  std::string program;      // its program's file name without .exe: "mw3pm"
  std::string asset[3];     // each Platform's file in a release, "{tag}" standing for the tag
  std::string badge;        // its panel's art (assets/launcher/ui)
  std::string heading_badge;  // its own setup's header art; "" the badge
  std::string card;         // a card's art, none yet
  std::vector<std::string> needs;  // other games' keys it needs to play
  std::string min_launcher;  // the oldest launcher version that reads the entry ("1.5.2")
  bool panel_before_release = true;  // false: no panel until a release is found or it is installed
  // "original" (2026-11-26): where the original game's files can be fetched from, the same file at each address,
  // tried in order. Read and checked (an https address, a 64-hex sha256, a size, a plain file name); absent, nothing
  // is ever fetched. No live catalog names one.
  struct Original {
    std::string url, sha256, name, note;  // the sha256 lower case; `name` the file's ("MechWarrior 3 (USA).zip")
    unsigned long long size = 0;
  };
  std::vector<Original> original;
};
constexpr int kMaxOriginals = 4;
// A home (2026-11-26, docs/launcher.md, Where the launcher looks): where a repo's releases list is asked, as an
// address with {repo} in it (and {count}, the releases wanted), e.g.
// "https://api.github.com/repos/<owner>/{repo}/releases?per_page={count}". GitHub is one like any other. Asked in
// `order`; the first is where a download comes from first.
struct CatalogHome {
  std::string key, releases;
  int order = 0;
};
constexpr int kMaxHomes = 4, kMaxCatalogSources = 6, kMaxNextKeys = 4, kMaxRevokedKeys = 8;
struct Catalog {
  int version = 0;   // "catalog": 1, the format
  int revision = 0;  // "revision": which wins, the built-in or a fetched one (the higher)
  std::vector<CatalogFamily> families;
  std::vector<CatalogGame> games;  // by order
  // "homes": where every repo's releases are asked, by order. A fetched catalog without the list, or with a bad one
  // (left out, said in `ignored`), leaves the built-in's in force.
  bool has_homes = false;
  std::vector<CatalogHome> homes;
  // "catalog_sources": the catalog's own further addresses, asked first at the next start (before sources.json's);
  // "next_keys": public keys a signed catalog vouches for, learned for the next catalogs (key rotation). A fetched
  // catalog's only: the built-in one's are not used. "revoked_keys": keys (shipped or learned) that never verify
  // again once a signed catalog naming them is taken (a leaked key cannot mint itself successors).
  std::vector<std::string> catalog_sources, next_keys, revoked_keys;
};
// A catalog's JSON read and checked: every key once, each game's family listed, a known state, the fields the
// launcher needs present. False and `why` (the entry and what is wrong) when not.
bool parse_catalog(const std::string& json, Catalog* out, std::string* why);
const std::string& builtin_catalog_text();  // games.json as built in
const Catalog& builtin_catalog();
// The catalog in use this run (b7, 2026-11-26): the built-in one unless the launcher's start set another (the fetched
// one cached, merged over it) before any game was looked up. A later use_catalog is refused (false): the catalog's
// strings stay put for the run (GameInfo points into them).
bool use_catalog(const Catalog& c);
const Catalog& active_catalog();
// The homes in use this run (the catalog in use's), by order: what the releases lists and the catalog fetch ask
// before GitHub, and so what a download falls back on (other_homes).
const std::vector<CatalogHome>& active_homes();
// kCatalogOwner ("telltaleatheist"), read so the program holds the name only at its GitHub address (check_paths.py).
std::string catalog_owner();
// Its games this run, kMaxGames at most: the six the code knows by their Game (kGames, in that order), then the
// catalog's others (a fetched catalog's new games, on the setup kit's contract: their Game is their place, 6 and up).
constexpr int kMaxGames = 10;  // the launcher's per-game button ids leave room for ten
int game_count();
Game game_at(int i);
// The catalog's entry of a game this run; every one of game_count()'s has one.
const CatalogGame& catalog_game(Game g);
// The Game an entry's key names this run; false for a key the catalog in use does not hold.
bool game_of_key(const std::string& key, Game* g);

// A fetched catalog (b7): read as parse_catalog does, but a bad entry is left out (said in `ignored`) and the rest
// kept, and so is one the launcher must not trust: a repo under another GitHub owner than kCatalogOwner, a
// min_launcher newer than `launcher_version` (a release's; a build's "unknown" checks none), art that is not a
// plain .png name. Its families are checked at the merge. False with `why` only when the whole is not a catalog (not
// JSON, no lists, a newer format).
// Kept as its address so the program holds the name only there (the packages' privacy check allows it there).
constexpr const char kCatalogOwnerAddress[] = "github.com/telltaleatheist";
constexpr const char* kCatalogOwner = kCatalogOwnerAddress + 11;
bool read_fetched_catalog(const std::string& json, const std::string& launcher_version, Catalog* out,
                          std::vector<std::string>* ignored, std::string* why);
// `fetched` over `builtin` when its revision is higher, else `builtin` as it is: a family or game by its key
// replaced, a new one added, the rest the built-in's. A game whose family neither lists is left out (in `ignored`).
// (A fetched one need list only what it changes; the games past kMaxGames are left out when it is put in use.)
Catalog merged_catalog(const Catalog& builtin, const Catalog& fetched, std::vector<std::string>* ignored = nullptr);

// The catalog's entry as the code's older shape (its strings the catalog in use's, alive for the program's run).
struct GameInfo {
  const char* key;      // "mw3", "pm", "mw1": its folder under the games folder, its record's game=
  const char* name;     // "Pirate's Moon", for the words
  const char* repo;     // its GitHub repo (telltaleatheist/<repo>)
  const char* prefix;   // its tags: "v", "pm-v", "mw1-v" followed by X.Y.Z
  const char* program;  // its program's file name without .exe: "mw3", "mw3pm", "mw1"
};
const GameInfo& game_info(Game g);

// A release tag of this game: its prefix and X.Y.Z, nothing after ("pm-v1.2.3"; not "pm-v1.2.3-rc1").
bool game_tag(Game g, const std::string& tag);

// The file a game's release has for a platform, as its packaging names it (docs/release.md; the catalog's `assets`):
//   MechWarrior 3   MW3-alone-<tag>-win64.zip    MW3-alone-<tag>-mac.dmg    MW3-alone-<tag>-x86_64.AppImage
//   Pirate's Moon   mw3pm-<tag>-win64.zip        PM-Rebuild-<tag>-mac.dmg   PM-Rebuild-<tag>-x86_64.AppImage
//   MW1 Remake      MW1-Remake-<tag>-win64.zip   MW1-Remake-<tag>-mac.dmg   MW1-Remake-<tag>-x86_64.AppImage
//   MW1 Rebuild     MW1-Rebuild-<tag>-win64.zip  MW1-Rebuild-<tag>-mac.dmg  MW1-Rebuild-<tag>-x86_64.AppImage
//   Inception Reb.  CHI-Rebuild-<tag>-win64.zip  CHI-Rebuild-<tag>-mac.dmg  CHI-Rebuild-<tag>-x86_64.AppImage
//   Inception Rem.  CHI-Remake-<tag>-win64.zip   CHI-Remake-<tag>-mac.dmg   CHI-Remake-<tag>-x86_64.AppImage
// Windows takes the zip, never the Setup: the launcher unpacks into a folder of its own.
std::string game_asset_name(Game g, const std::string& tag, Platform p);
const Asset* game_asset(Game g, const Release& r, Platform p);

// The newest release of the game that has this platform's file: not a draft or a prerelease. Null when none.
const Release* newest_game(Game g, const std::vector<Release>& releases, Platform p);

// What the launcher keeps beside an installed game (its folder's rebuild-game.txt): a few key=value lines. The
// file also marks the folder as the launcher's own, the only kind it ever replaces or removes.
struct InstalledGame {
  std::string game;    // its key
  std::string tag;     // "pm-v1.2.3"; for a file of unknown origin, its program's own version
  std::string file;    // the package it came from
  std::string sha256;  // that file's, lower case
  std::string from;    // "github" (INSTALL, UPDATE) or "file" (INSTALL FROM FILE)
};
constexpr const char* kInstalledName = "rebuild-game.txt";
std::string installed_text(const InstalledGame& in);
bool parse_installed(const std::string& text, InstalledGame* out);

// A card's answer from its repo's releases: `installed` is the installed game's tag (empty when none).
struct GameCheck {
  enum Kind { kAsking, kFailed, kNoRelease, kNewer, kUpToDate } kind = kAsking;
  Release release;    // kNewer, kUpToDate: the newest one
  const Asset* asset() const;  // its file for this platform (from `release`)
  Platform platform = this_platform();
  std::string words;  // for the log and the status line
};
// Over every home's answer for its repo (2026-11-22: git.owenmorgan.com and GitHub): the newest release on
// either; failed only when no answer reads, saying the first one's reason. `release` carries the other home's copy
// (other_homes) for the download to fall back on.
GameCheck judge_game(Game g, const std::string& installed, const std::vector<Answer>& answers, Platform p);

// A tag as a player reads it: "1.2.3" from "pm-v1.2.3", "1.0.7" from "mw1-v1.0.7".
std::string game_version(const std::string& tag);
// Two of a game's tags (or versions) by their numbers: <0, 0, >0.
int compare_game(const std::string& a, const std::string& b);

// The size of a download for the words: "48 MB".
std::string size_words(uint64_t bytes);

}  // namespace upgrades::update
