#include "upgrades/update/games.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <iterator>
#include <mutex>
#include <tuple>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "upgrades/update/json_reader.h"
#include "upgrades/update/sources.h"

namespace upgrades::update {

// games.json as built in (games_catalog.cpp, written from it by CMake).
extern const char kBuiltinCatalog[];

namespace {

using detail::Json;

// The catalog's key of each Game, in the enum's order: how the code names an entry.
constexpr const char* kKeys[] = {"mw3", "pm", "mw1", "mw1r", "chi", "chirm"};
static_assert(sizeof kKeys / sizeof *kKeys == kGameCount);

// One entry's fields read against what the launcher needs; the first failure kept in `why`.
struct Fields {
  const Json& j;
  std::string where;
  std::string* why;
  bool fail(const std::string& what) {
    if (why->empty()) *why = where + ": " + what;
    return false;
  }
  bool text(const char* key, std::string* out, bool needed = true) {
    const Json* v = j.at(key);
    if (!v) return !needed || fail(std::string("no ") + key);
    if (v->type != Json::kString || (needed && v->s.empty())) return fail(std::string(key) + " is not a text");
    return *out = v->s, true;
  }
  bool number(const char* key, int* out) {
    const Json* v = j.at(key);
    if (!v || v->type != Json::kNumber) return fail(std::string("no ") + key + " number");
    return *out = std::atoi(v->s.c_str()), true;
  }
};

// Checks may name file:/// addresses (a stand-in catalog or release): MW3_GAMES_API or MW3_SOURCES_FILE set.
bool check_address(std::string* url) {
  if ((std::getenv("MW3_GAMES_API") || std::getenv("MW3_SOURCES_FILE")) && url->rfind("file:///", 0) == 0) return url->find("..") == std::string::npos;
  return https_address(url, true);
}

// The catalog's "homes" into `out`; the first thing wrong in `why` (and `out` left as it was).
void read_homes(const Json& homes, Catalog* out, std::string* why) {
  if (homes.type != Json::kArray) return void(*why = "homes is not a list");
  if (homes.items.size() > static_cast<size_t>(kMaxHomes)) return void(*why = "homes lists more than " + std::to_string(kMaxHomes));
  if (homes.items.empty()) return void(*why = "homes lists none (nothing could be asked)");
  std::vector<CatalogHome> list;
  for (size_t i = 0; i < homes.items.size(); ++i) {
    const Json& j = homes.items[i];
    Fields f{j, "homes[" + std::to_string(i) + "]", why};
    CatalogHome h;
    if (j.type != Json::kObject) return void(f.fail("not an entry"));
    if (!f.text("key", &h.key) || !f.text("releases", &h.releases) || !f.number("order", &h.order)) return;
    const size_t repo = h.releases.find("{repo}");
    if (repo == std::string::npos || h.releases.find("{repo}", repo + 1) != std::string::npos || !check_address(&h.releases))
      return void(f.fail(h.releases + " is not https:// and a plain host with {repo} once"));
    for (const CatalogHome& o : list)
      if (o.key == h.key || o.releases == h.releases) return void(f.fail("the home " + h.releases + " twice"));
    list.push_back(std::move(h));
  }
  std::stable_sort(list.begin(), list.end(), [](const CatalogHome& a, const CatalogHome& b) { return a.order < b.order; });
  out->has_homes = true, out->homes = std::move(list);
}

// A file name a download is saved as: letters, digits, spaces and "-_.()',", not starting with a dot or a space.
bool plain_file_name(const std::string& n) {
  if (n.empty() || n.size() > 120 || n.front() == '.' || n.front() == ' ' || n.find("..") != std::string::npos) return false;
  for (char c : n)
    if (!std::isalnum(static_cast<unsigned char>(c)) && !std::strchr(" -_.()',", c)) return false;
  return true;
}

// A game's "original" (a list of sources) into `g`; the first thing wrong in `why` (and `g` left without one).
void read_original(const Json& o, CatalogGame* g, std::string* why) {
  if (o.type != Json::kArray || o.items.empty()) return void(*why = "its original is not a list of sources");
  if (o.items.size() > static_cast<size_t>(kMaxOriginals)) return void(*why = "its original lists more than " + std::to_string(kMaxOriginals));
  std::vector<CatalogGame::Original> list;
  for (size_t i = 0; i < o.items.size(); ++i) {
    const Json& j = o.items[i];
    Fields f{j, "its original[" + std::to_string(i) + "]", why};
    CatalogGame::Original s;
    if (j.type != Json::kObject) return void(f.fail("not an entry"));
    if (!f.text("url", &s.url) || !f.text("sha256", &s.sha256) || !f.text("name", &s.name) || !f.text("note", &s.note, false)) return;
    const Json* size = j.at("size");
    if (!size || size->type != Json::kNumber || size->s.empty() || size->s.find_first_not_of("0123456789") != std::string::npos || size->s.size() > 12)
      return void(f.fail("no size in bytes"));
    s.size = std::strtoull(size->s.c_str(), nullptr, 10);
    if (s.size == 0) return void(f.fail("no size in bytes"));
    // file:// only in checks (MW3_GAMES_API set: stand-in releases and downloads), as the games' downloads take it.
    const bool check_file = std::getenv("MW3_GAMES_API") && s.url.rfind("file:///", 0) == 0 && s.url.find("..") == std::string::npos;
    if (!check_file && !https_address(&s.url, true)) return void(f.fail(s.url + " is not an https:// address"));
    bool hex = s.sha256.size() == 64;
    for (char& c : s.sha256) hex = hex && std::isxdigit(static_cast<unsigned char>(c)), c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (!hex) return void(f.fail("its sha256 is not 64 hex digits"));
    if (!plain_file_name(s.name)) return void(f.fail("its name " + s.name + " is not a plain file name"));
    list.push_back(std::move(s));
  }
  g->original = std::move(list);
}

}  // namespace

namespace {

// The catalog read; `ignored` null: the built-in's strict reading (any bad entry fails it all), else a fetched one's
// (a bad entry left out, said there).
bool read_catalog(const std::string& json, Catalog* out, std::string* why, std::vector<std::string>* ignored) {
  *out = {}, why->clear();
  detail::Reader r(json);
  Json root;
  if (!r.value(&root) || !r.end()) return *why = r.error, false;
  if (root.type != Json::kObject) return *why = "not a catalog (an object)", false;
  Fields top{root, "the catalog", why};
  if (!top.number("catalog", &out->version)) return false;
  if (out->version < 1) return top.fail("catalog is not a version");
  if (out->version > 1) return top.fail("a newer catalog format (" + std::to_string(out->version) + ") than this launcher reads (1)");
  if (const Json* rev = root.at("revision")) {
    if (rev->type != Json::kNumber) return top.fail("revision is not a number");
    out->revision = std::atoi(rev->s.c_str());
  }
  const Json* families = root.at("families");
  const Json* games = root.at("games");
  if (!families || families->type != Json::kArray) return top.fail("no families list");
  if (!games || games->type != Json::kArray) return top.fail("no games list");
  if (const Json* homes = root.at("homes")) {  // bad: the whole refused (strict) or the built-in's homes kept
    std::string bad_homes;
    read_homes(*homes, out, &bad_homes);
    if (!bad_homes.empty()) {
      if (!ignored) return *why = bad_homes, false;
      ignored->push_back(bad_homes + " (the built-in homes kept)");
    }
  }
  // The catalog's own addresses and the keys it vouches for: a bad list refused whole (strict) or left out.
  for (const auto& [key, list, most, address] : {std::tuple<const char*, std::vector<std::string>*, int, bool>{"catalog_sources", &out->catalog_sources, kMaxCatalogSources, true},
                                                 {"next_keys", &out->next_keys, kMaxNextKeys, false},
                                                 {"revoked_keys", &out->revoked_keys, kMaxRevokedKeys, false}}) {
    const Json* v = root.at(key);
    if (!v) continue;
    std::string bad;
    std::vector<std::string> got;
    if (v->type != Json::kArray || v->items.size() > static_cast<size_t>(most)) bad = std::string(key) + " is not a list of at most " + std::to_string(most);
    for (size_t i = 0; bad.empty() && i < v->items.size(); ++i) {
      std::string s = v->items[i].s;
      if (v->items[i].type != Json::kString || !(address ? check_address(&s) : is_key_hex(&s)) || std::find(got.begin(), got.end(), s) != got.end())
        bad = std::string(key) + "[" + std::to_string(i) + "]: " + v->items[i].s + (address ? " is not an https:// address" : " is not a key (64 hex digits)") + " or is there twice";
      got.push_back(s);
    }
    if (bad.empty()) *list = std::move(got);
    else if (!ignored) return *why = bad, false;
    else ignored->push_back(bad + " (left out)");
  }
  // A bad entry: the whole refused (strict) or the entry left out.
  auto bad = [&]() {
    if (!ignored) return false;
    ignored->push_back(*why), why->clear();
    return true;
  };
  for (size_t i = 0; i < families->items.size(); ++i) {
    const Json& j = families->items[i];
    Fields f{j, "families[" + std::to_string(i) + "]", why};
    CatalogFamily fam;
    bool ok = j.type == Json::kObject ? f.text("key", &fam.key) && f.text("name", &fam.name) && f.number("order", &fam.order) : f.fail("not an entry");
    for (const CatalogFamily& o : out->families)
      if (ok && o.key == fam.key) ok = f.fail("the family " + fam.key + " twice");
    if (ok) out->families.push_back(std::move(fam));
    else if (!bad()) return false;
  }
  for (size_t i = 0; i < games->items.size(); ++i) {
    const Json& j = games->items[i];
    Fields f{j, "games[" + std::to_string(i) + "]", why};
    CatalogGame g;
    const bool ok = [&] {
      if (j.type != Json::kObject) return f.fail("not an entry");
      if (!f.text("key", &g.key)) return false;
      f.where += " (" + g.key + ")";
      std::string state;
      if (!f.text("family", &g.family) || !f.number("order", &g.order) || !f.text("state", &state) || !f.text("name", &g.name) ||
          !f.text("title", &g.title) || !f.text("heading", &g.heading) || !f.text("button", &g.button) ||
          !f.text("description", &g.description) || !f.text("repo", &g.repo) || !f.text("tag_prefix", &g.tag_prefix) ||
          !f.text("program", &g.program) || !f.text("badge", &g.badge) || !f.text("heading_badge", &g.heading_badge, false) ||
          !f.text("card", &g.card, false) || !f.text("min_launcher", &g.min_launcher) || !f.text("badge_sha256", &g.badge_sha256, false) ||
          !f.text("heading_badge_sha256", &g.heading_badge_sha256, false) || !f.text("card_sha256", &g.card_sha256, false))
        return false;
      if (state == "coming_soon") g.state = CatalogGame::kComingSoon;
      else if (state == "released") g.state = CatalogGame::kReleased;
      else if (state == "removed") g.state = CatalogGame::kRemoved;
      else return f.fail("state " + state + " is none of coming_soon, released, removed");
      bool known = false;
      for (const CatalogFamily& fam : out->families) known = known || fam.key == g.family;
      if (!known && !ignored) return f.fail("its family " + g.family + " is not listed");  // a fetched one's: at the merge (the built-in's families count)
      for (const CatalogGame& o : out->games)
        if (o.key == g.key) return f.fail("the key twice");
      const Json* assets = j.at("assets");
      if (!assets || assets->type != Json::kObject) return f.fail("no assets");
      Fields a{*assets, f.where + " assets", why};
      const char* const kPlatforms[] = {"windows", "mac", "linux"};  // Platform's order
      for (int p = 0; p < 3; ++p) {
        if (!a.text(kPlatforms[p], &g.asset[p])) return false;
        if (g.asset[p].find("{tag}") == std::string::npos) return a.fail(std::string(kPlatforms[p]) + " has no {tag}");
      }
      if (const Json* needs = j.at("needs")) {
        if (needs->type != Json::kArray) return f.fail("needs is not a list");
        for (const Json& n : needs->items) {
          if (n.type != Json::kString || n.s.empty()) return f.fail("needs holds a non-key");
          g.needs.push_back(n.s);
        }
      }
      if (const Json* b = j.at("panel_before_release")) {
        if (b->type != Json::kBool) return f.fail("panel_before_release is not true or false");
        g.panel_before_release = b->b;
      }
      if (const Json* o = j.at("original")) {  // bad: the whole refused (strict) or the slot left out, the game kept
        std::string bad_original;
        read_original(*o, &g, &bad_original);
        if (!bad_original.empty()) {
          if (!ignored) return f.fail(bad_original);
          ignored->push_back(f.where + ": " + bad_original + " (left out, the game kept)");
        }
      }
      return true;
    }();
    if (ok) out->games.push_back(std::move(g));
    else if (!bad()) return false;
  }
  if (!ignored)  // a fetched one's needs may name the built-in's games: checked on the merged catalog's use
    for (const CatalogGame& g : out->games)
      for (const std::string& n : g.needs) {
        bool listed = false;
        for (const CatalogGame& o : out->games) listed = listed || o.key == n;
        if (!listed) return *why = "games (" + g.key + "): it needs " + n + ", which is not listed", false;
      }
  std::stable_sort(out->games.begin(), out->games.end(), [](const CatalogGame& a, const CatalogGame& b) { return a.order < b.order; });
  std::stable_sort(out->families.begin(), out->families.end(), [](const CatalogFamily& a, const CatalogFamily& b) { return a.order < b.order; });
  return true;
}

// A file name the launcher may fetch and keep beside the catalog: letters, digits, '_', '-', '.', ending .png.
bool plain_png(const std::string& n) {
  if (n.size() < 5 || n.size() > 64 || n.compare(n.size() - 4, 4, ".png") != 0 || n.find("..") != std::string::npos) return false;
  for (char c : n)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-' && c != '.') return false;
  return true;
}

// A name a fetched entry puts in a path (its key, its program, its assets): letters, digits and `extra`, no "..".
bool plain_name(const std::string& n, const char* extra) {
  if (n.empty() || n.size() > 96 || n.find("..") != std::string::npos) return false;
  for (char c : n)
    if (!std::isalnum(static_cast<unsigned char>(c)) && !std::strchr(extra, c)) return false;
  return true;
}

// A release's version ("v1.5.2", "v1.5.2-16-gHASH-dirty", "1.5.2"); a build's "unknown" or bare commit is not one,
// and takes every entry (a development build).
bool release_version(const std::string& v) {
  const size_t d = !v.empty() && v[0] == 'v' ? 1 : 0;
  const size_t dot = v.find('.', d);
  if (dot == std::string::npos || dot == d) return false;
  for (size_t i = d; i < dot; ++i)
    if (!std::isdigit(static_cast<unsigned char>(v[i]))) return false;
  return dot + 1 < v.size() && std::isdigit(static_cast<unsigned char>(v[dot + 1]));
}

// The catalog in use this run, made once: its games by Game (the six known, then its others), their GameInfo.
struct Active {
  Catalog cat;
  std::vector<const CatalogGame*> by_game;
  std::vector<GameInfo> info;
};
std::mutex g_active_mutex;
std::atomic<const Active*> g_active{nullptr};

// Null when `c` lacks one of the six the code knows.
const Active* make_active(const Catalog& c, std::string* missing) {
  auto* a = new Active{c, {}, {}};  // kept for the run: GameInfo points into it
  for (const char* k : kKeys) {
    const CatalogGame* e = nullptr;
    for (const CatalogGame& g : a->cat.games)
      if (g.key == k) e = &g;
    if (!e) return *missing = k, delete a, nullptr;
    a->by_game.push_back(e);
  }
  for (const CatalogGame& g : a->cat.games) {
    if (a->by_game.size() >= static_cast<size_t>(kMaxGames)) break;
    if (std::find(std::begin(kKeys), std::end(kKeys), g.key) == std::end(kKeys)) a->by_game.push_back(&g);
  }
  for (const CatalogGame* e : a->by_game) a->info.push_back({e->key.c_str(), e->name.c_str(), e->repo.c_str(), e->tag_prefix.c_str(), e->program.c_str()});
  return a;
}

const Active& active() {
  if (const Active* a = g_active.load(std::memory_order_acquire)) return *a;
  std::lock_guard<std::mutex> lock(g_active_mutex);
  if (!g_active.load(std::memory_order_relaxed)) {
    std::string missing;
    const Active* a = make_active(builtin_catalog(), &missing);
    if (!a) {  // a build's own data: the catalog test stops this first
      std::fprintf(stderr, "the built-in games catalog (games.json) has no %s\n", missing.c_str());
      std::abort();
    }
    g_active.store(a, std::memory_order_release);
  }
  return *g_active.load(std::memory_order_relaxed);
}

}  // namespace

bool parse_catalog(const std::string& json, Catalog* out, std::string* why) { return read_catalog(json, out, why, nullptr); }

bool read_fetched_catalog(const std::string& json, const std::string& launcher_version, Catalog* out,
                          std::vector<std::string>* ignored, std::string* why) {
  ignored->clear();
  if (!read_catalog(json, out, why, ignored)) return false;
  std::vector<CatalogGame> kept;
  const std::string owner = catalog_owner();
  for (CatalogGame& g : out->games) {
    const std::string where = "games (" + g.key + "): ";
    const size_t slash = g.repo.find('/');
    if (slash != std::string::npos && g.repo.substr(0, slash) != owner) {
      ignored->push_back(where + "its repo " + g.repo + " is not under " + owner);
      continue;
    }
    if (slash != std::string::npos) g.repo = g.repo.substr(slash + 1);
    bool plain_repo = !g.repo.empty();
    for (char c : g.repo) plain_repo = plain_repo && (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.');
    if (!plain_repo || g.repo.find("..") != std::string::npos) {
      ignored->push_back(where + "its repo " + g.repo + " is not a repo's name");
      continue;
    }
    bool plain = plain_name(g.key, "_-") && plain_name(g.program, "_-") && plain_name(g.tag_prefix, "_-.");
    for (const std::string& asset : g.asset) plain = plain && plain_name(asset, "_-.{}");
    if (!plain) {
      ignored->push_back(where + "its key, program, tag prefix or an asset is not a plain name");
      continue;
    }
    if (release_version(launcher_version) && compare_versions(g.min_launcher, launcher_version) > 0) {
      ignored->push_back(where + "it needs launcher " + g.min_launcher + " or newer (this is " + launcher_version + ")");
      continue;
    }
    if (!plain_png(g.badge) || (!g.heading_badge.empty() && !plain_png(g.heading_badge)) || (!g.card.empty() && !plain_png(g.card))) {
      ignored->push_back(where + "its art is not a plain .png name");
      continue;
    }
    bool sums = true;
    for (const auto& [art, sum] : {std::pair<const std::string*, std::string*>{&g.badge, &g.badge_sha256}, {&g.heading_badge, &g.heading_badge_sha256},
                                   {&g.card, &g.card_sha256}})
      if (!sum->empty() && (art->empty() || !is_key_hex(sum))) sums = false;  // 64 hex digits, lower-cased
    if (!sums) {
      ignored->push_back(where + "an art's sha256 is not 64 hex digits, or names no art");
      continue;
    }
    kept.push_back(std::move(g));
  }
  out->games = std::move(kept);
  return true;
}

std::string catalog_owner() {
  // Read through a volatile pointer: clang folded the compare with the constant into the name's bytes as immediates
  // in the code, outside its address (mw3pm pm-v1.2.11, check_paths.py).
  static const char* const volatile owner_at = kCatalogOwner;
  return owner_at;
}

Catalog merged_catalog(const Catalog& builtin, const Catalog& fetched, std::vector<std::string>* ignored) {
  if (fetched.revision <= builtin.revision) return builtin;
  Catalog out = builtin;
  out.revision = fetched.revision;
  if (fetched.has_homes) out.has_homes = true, out.homes = fetched.homes;
  out.catalog_sources = fetched.catalog_sources, out.next_keys = fetched.next_keys, out.revoked_keys = fetched.revoked_keys;
  for (const CatalogFamily& f : fetched.families) {
    auto at = std::find_if(out.families.begin(), out.families.end(), [&](const CatalogFamily& o) { return o.key == f.key; });
    if (at != out.families.end()) *at = f;
    else out.families.push_back(f);
  }
  for (const CatalogGame& g : fetched.games) {
    const bool family = std::any_of(out.families.begin(), out.families.end(), [&](const CatalogFamily& o) { return o.key == g.family; });
    if (!family) {
      if (ignored) ignored->push_back("games (" + g.key + "): its family " + g.family + " is not listed");
      continue;
    }
    auto at = std::find_if(out.games.begin(), out.games.end(), [&](const CatalogGame& o) { return o.key == g.key; });
    if (at != out.games.end()) *at = g;
    else out.games.push_back(g);
  }
  std::stable_sort(out.games.begin(), out.games.end(), [](const CatalogGame& a, const CatalogGame& b) { return a.order < b.order; });
  std::stable_sort(out.families.begin(), out.families.end(), [](const CatalogFamily& a, const CatalogFamily& b) { return a.order < b.order; });
  return out;
}

const std::string& builtin_catalog_text() {
  static const std::string text = kBuiltinCatalog;
  return text;
}

const Catalog& builtin_catalog() {
  static const Catalog c = [] {
    Catalog out;
    std::string why;
    if (!parse_catalog(builtin_catalog_text(), &out, &why)) {  // a build's own data: the catalog test stops this first
      std::fprintf(stderr, "the built-in games catalog (games.json) cannot be read: %s\n", why.c_str());
      std::abort();
    }
    return out;
  }();
  return c;
}

bool use_catalog(const Catalog& c) {
  std::lock_guard<std::mutex> lock(g_active_mutex);
  if (g_active.load(std::memory_order_relaxed)) return false;
  std::string missing;
  const Active* a = make_active(c, &missing);
  if (!a) return false;
  g_active.store(a, std::memory_order_release);
  return true;
}

const Catalog& active_catalog() { return active().cat; }
const std::vector<CatalogHome>& active_homes() { return active().cat.homes; }
int game_count() { return static_cast<int>(active().by_game.size()); }
Game game_at(int i) { return static_cast<Game>(i); }

bool game_of_key(const std::string& key, Game* g) {
  const Active& a = active();
  for (size_t i = 0; i < a.by_game.size(); ++i)
    if (a.by_game[i]->key == key) return *g = static_cast<Game>(i), true;
  return false;
}

const CatalogGame& catalog_game(Game g) {
  const Active& a = active();
  const size_t i = static_cast<size_t>(g);
  if (i >= a.by_game.size()) {
    std::fprintf(stderr, "games catalog: no game %zu this run (%zu)\n", i, a.by_game.size());
    std::abort();
  }
  return *a.by_game[i];
}

const GameInfo& game_info(Game g) {
  (void)catalog_game(g);  // the same check
  return active().info[static_cast<size_t>(g)];
}

bool game_tag(Game g, const std::string& tag) {
  const std::string prefix = game_info(g).prefix;
  if (tag.rfind(prefix, 0) != 0) return false;
  // The rest as package_tag reads a bare vX.Y.Z.
  return package_tag("v" + tag.substr(prefix.size()));
}

std::string game_asset_name(Game g, const std::string& tag, Platform p) {
  std::string name = catalog_game(g).asset[static_cast<int>(p)];
  return name.replace(name.find("{tag}"), 5, tag);
}

const Asset* game_asset(Game g, const Release& r, Platform p) {
  const std::string want = game_asset_name(g, r.tag, p);
  for (const Asset& a : r.assets)
    if (a.name == want && !a.url.empty()) return &a;
  return nullptr;
}

// compare_versions reads from the first digit, which in "mw1-v" is mw1's own "1".
int compare_game(const std::string& a, const std::string& b) { return compare_versions(game_version(a), game_version(b)); }

const Release* newest_game(Game g, const std::vector<Release>& releases, Platform p) {
  const Release* best = nullptr;
  for (const Release& r : releases)
    if (!r.draft && !r.prerelease && game_tag(g, r.tag) && game_asset(g, r, p) && (!best || compare_game(r.tag, best->tag) > 0))
      best = &r;
  return best;
}

std::string installed_text(const InstalledGame& in) {
  return "# Installed by the MechWarrior 3 Rebuild launcher. This folder is the launcher's: UPDATE replaces it.\n"
         "game=" + in.game + "\ntag=" + in.tag + "\nfile=" + in.file + "\nsha256=" + in.sha256 + "\nfrom=" + in.from + "\n";
}

bool parse_installed(const std::string& text, InstalledGame* out) {
  *out = {};
  size_t at = 0;
  while (at < text.size()) {
    const size_t nl = text.find('\n', at);
    std::string line = text.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
    at = nl == std::string::npos ? text.size() : nl + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t eq = line.find('=');
    if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
    const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
    if (k == "game") out->game = v;
    else if (k == "tag") out->tag = v;
    else if (k == "file") out->file = v;
    else if (k == "sha256") out->sha256 = v;
    else if (k == "from") out->from = v;
  }
  return !out->game.empty();
}

const Asset* GameCheck::asset() const {
  for (Game g : kGames)
    if (game_tag(g, release.tag))
      if (const Asset* a = game_asset(g, release, platform)) return a;
  return nullptr;
}

GameCheck judge_game(Game g, const std::string& installed, const std::vector<Answer>& answers, Platform p) {
  GameCheck c;
  c.platform = p;
  const std::string name = game_info(g).name;
  const Homes h = read_homes(answers);
  if (h.read == 0) {  // every home failed: the first one's reason
    c.kind = GameCheck::kFailed;
    if (!h.failed) c.words = "Could not check for " + name + ".";
    else if (!h.parse_error.empty()) c.words = "The releases list for " + name + " could not be read (" + h.parse_error + ").";
    else c.words = "Could not reach the releases of " + name + (h.failed->fetch_error.empty() ? std::string(".") : ": " + h.failed->fetch_error + ".");
    return c;
  }
  const Release* newest = newest_game(g, h.releases, p);
  if (!newest) {
    c.kind = GameCheck::kNoRelease;
    c.words = "No release of " + name + " for this computer yet.";
    return c;
  }
  c.release = with_homes(h, newest->tag, [g, p](const Release& r) { return game_asset(g, r, p) != nullptr; });
  if (installed.empty() || compare_game(newest->tag, installed) > 0) {
    c.kind = GameCheck::kNewer;
    c.words = installed.empty() ? name + " " + game_version(newest->tag) + " can be installed."
                                : name + " " + game_version(newest->tag) + " is out (installed: " + game_version(installed) + ").";
  } else {
    c.kind = GameCheck::kUpToDate;
    c.words = name + " " + game_version(installed) + " is the newest.";
  }
  return c;
}

std::string game_version(const std::string& tag) {
  // After the "v" that starts the numbers: "mw1-v1.0.7" has a digit in its prefix.
  for (size_t i = 0; i + 1 < tag.size(); ++i)
    if (tag[i] == 'v' && tag[i + 1] >= '0' && tag[i + 1] <= '9' && (i == 0 || tag[i - 1] == '-')) return tag.substr(i + 1);
  return tag;
}

std::string size_words(uint64_t bytes) {
  const uint64_t mb = (bytes + 512 * 1024) / (1024 * 1024);
  return mb == 0 ? std::string("under 1 MB") : std::to_string(mb) + " MB";
}

}  // namespace upgrades::update
