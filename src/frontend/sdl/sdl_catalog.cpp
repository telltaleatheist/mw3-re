// The games catalog fetched and cached (launcher split b7); sdl_catalog.h.
#include "frontend/sdl/sdl_catalog.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "frontend/sdl/sdl_update.h"
#include "rekit/hash.h"
#include "rekit/sign.h"
#include "upgrades/update/games.h"
#include "upgrades/update/sources.h"

namespace fs = std::filesystem;
namespace u = upgrades::update;

namespace sdlfe::catalog {

namespace {

std::string read_file(const fs::path& p);

// The cached catalog's text when its stored games.json.sig checks against the keys in use (shipped and learned, less
// the revoked), else empty: a cache an older launcher fetched unsigned (1.7.1 and before), or one signed by a key
// since revoked, counts as no cache.
std::string signed_cache(const std::string& write_dir, bool* unsigned_there = nullptr) {
  const fs::path dir = fs::path(write_dir) / "catalog";
  std::string text = read_file(dir / "games.json");
  if (unsigned_there) *unsigned_there = false;
  if (text.empty() || !rekit::signed_by(text, read_file(dir / "games.json.sig"), u::signing_keys()).empty()) return text;
  if (unsigned_there) *unsigned_there = true;
  return std::string();
}

// The catalog's addresses in the order asked: MW3_CATALOG_URL's in checks ("A|B"), else the cached catalog's own
// (catalog_sources: read from the cache, which a check run's fetch asks before the cache is in use) and then
// sources.json's, each once.
std::vector<std::string> sources(const std::string& write_dir) {
  if (const char* env = std::getenv("MW3_CATALOG_URL")) {
    const std::string s = env;
    const size_t bar = s.find('|');
    if (bar == std::string::npos) return {s};
    return {s.substr(0, bar), s.substr(bar + 1)};
  }
  u::Catalog cached;
  std::vector<std::string> ignored;
  std::string why;
  const std::string text = write_dir.empty() ? std::string() : signed_cache(write_dir);
  if (text.empty() || !u::read_fetched_catalog(text, "unknown", &cached, &ignored, &why)) cached.catalog_sources.clear();
  return addresses(cached.catalog_sources, u::active_sources().catalog);
}

// The catalog in use, said: its homes and addresses, and each game's original download.
void say_in_use(const std::string& write_dir) {
  std::string homes, where;
  for (const u::CatalogHome& h : u::active_homes()) homes += (homes.empty() ? "" : ", ") + h.key;
  for (const std::string& s : sources(write_dir)) where += (where.empty() ? "" : ", ") + s;
  std::printf("mw3: launcher: catalog: homes: %s\n", homes.empty() ? "(none)" : homes.c_str());
  std::printf("mw3: launcher: catalog: asked at %s\n", where.c_str());
  for (const u::CatalogGame& g : u::active_catalog().games) {
    if (!g.original.empty()) std::printf("mw3: launcher: catalog: %s names an original download\n", g.key.c_str());
    if (!g.compact.empty()) std::printf("mw3: launcher: catalog: %s names a compact download\n", g.key.c_str());
  }
}

// The keys signed catalogs changed (<cache>/keys.txt): a key a line learned from next_keys, "revoked <key>" one
// revoked by revoked_keys (shipped or learned, kept revoked across starts). Read at the start, before the cache is
// checked.
fs::path keys_file(const std::string& write_dir) { return fs::path(cache_dir(write_dir)) / "keys.txt"; }
void read_learned(const std::string& write_dir) {
  std::vector<std::string> keys, revoked;
  std::ifstream f(keys_file(write_dir));
  for (std::string line; std::getline(f, line);) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (line.rfind("revoked ", 0) == 0) revoked.push_back(line.substr(8));
    else if (!line.empty()) keys.push_back(line);
  }
  u::revoke_keys(revoked);
  u::learn_keys(keys);
}
// A taken catalog's revoked_keys honored, then its next_keys learned (a revoked one is not), and keys.txt rewritten.
void write_learned(const std::string& write_dir, const std::vector<std::string>& next, const std::vector<std::string>& revoked) {
  if (next.empty() && revoked.empty()) return;
  u::revoke_keys(revoked);
  u::learn_keys(next);
  std::string text;
  for (const std::string& k : u::learned_keys()) text += k + "\n";
  for (const std::string& k : u::revoked_keys()) text += "revoked " + k + "\n";
  std::error_code ec;
  const fs::path file = keys_file(write_dir), part = file.string() + ".part";
  { std::ofstream(part, std::ios::binary) << text; }
  fs::rename(part, file, ec);
  if (ec) fs::remove(part, ec);
  else
    std::printf("mw3: launcher: catalog: its keys taken (%d next, %d revoked)\n", static_cast<int>(next.size()), static_cast<int>(revoked.size()));
}

std::string read_file(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// A catalog's text read as a fetched one; its ignored entries said in the log under `log`.
bool read(const std::string& text, const std::string& version, const std::string& log, u::Catalog* out) {
  std::vector<std::string> ignored;
  std::string why;
  if (!u::read_fetched_catalog(text, version, out, &ignored, &why)) {
    std::printf("mw3: launcher: catalog: %s: not used: %s\n", log.c_str(), why.c_str());
    return false;
  }
  for (const std::string& i : ignored) std::printf("mw3: launcher: catalog: %s: left out: %s\n", log.c_str(), i.c_str());
  return true;
}

// A PNG's first bytes.
bool is_png(const std::string& s) { return s.size() > 8 && s.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0; }

// The art a catalog names that the launcher does not carry, fetched beside it (the catalog's address
// with games.json replaced by art/<name>), kept only when it is a PNG; none already carried or cached.
// Each art a game names with its sum (the badge, the heading badge, the card).
std::vector<std::pair<std::string, std::string>> art_of(const u::CatalogGame& g) {
  return {{g.badge, g.badge_sha256}, {g.heading_badge, g.heading_badge_sha256}, {g.card, g.card_sha256}};
}

void fetch_art(const u::Catalog& c, const std::string& url, const fs::path& dir, const std::string& carried_dir, bool allow_file) {
  const size_t slash = url.rfind('/');
  if (slash == std::string::npos) return;
  const std::string base = url.substr(0, slash + 1) + "art/";
  const fs::path carried = carried_dir;
  for (const u::CatalogGame& g : c.games)
    for (const auto& [name, sum] : art_of(g)) {
      std::error_code ec;
      if (name.empty() || fs::exists(carried / name, ec)) continue;
      if (sum.empty()) {  // not vouched for by the signed catalog: never fetched
        std::printf("mw3: launcher: catalog: art %s not fetched: the catalog gives no sha256 for it\n", name.c_str());
        continue;
      }
      if (fs::exists(dir / name, ec) && rekit::sha256_file_hex((dir / name).string()) == sum) continue;
      std::string png, error;
      if (!sdlfe::curl_get(base + name, 15, &png, &error, allow_file) || !is_png(png)) {
        std::printf("mw3: launcher: catalog: art %s not fetched: %s\n", name.c_str(), error.empty() ? "not a PNG" : error.c_str());
        continue;
      }
      if (rekit::sha256_hex(reinterpret_cast<const uint8_t*>(png.data()), png.size()) != sum) {
        std::printf("mw3: launcher: catalog: art %s not kept: not the catalog's sha256\n", name.c_str());
        continue;
      }
      const fs::path part = dir / (name + ".part");
      { std::ofstream(part, std::ios::binary) << png; }
      fs::rename(part, dir / name, ec);
      if (ec) fs::remove(part, ec);
    }
}

std::atomic<int> g_in_use{0};  // the revision in use (the built-in one's, or the cache's put in use)
std::mutex g_used_mutex;
std::string g_used_text;  // the fetched catalog put in use at the start, as cached: what keep() copies

fs::path kept_dir(const std::string& write_dir) { return fs::path(cache_dir(write_dir)) / "kept"; }

bool holds(const u::Catalog& c, const std::string& key) {
  return std::any_of(c.games.begin(), c.games.end(), [&](const u::CatalogGame& g) { return g.key == key; });
}

// A plain key only: it names a file.
bool plain_key(const std::string& key) {
  return !key.empty() && key.size() < 64 && std::all_of(key.begin(), key.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
  });
}

// `text` (a fetched catalog that holds `key`) as <kept>/<key>.json, when not so already.
void write_kept(const std::string& write_dir, const std::string& key, const std::string& text) {
  if (!plain_key(key)) return;
  const fs::path dir = kept_dir(write_dir), file = dir / (key + ".json");
  if (read_file(file) == text) return;
  std::error_code ec;
  fs::create_directories(dir, ec);
  const fs::path part = dir / (key + ".json.part");
  { std::ofstream(part, std::ios::binary) << text; }
  fs::rename(part, file, ec);
  if (ec) fs::remove(part, ec);
  else std::printf("mw3: launcher: catalog: %s's entry kept beside the cache\n", key.c_str());
}

// The kept entries of games still here put back in `c` where it lacks their key (their family too, when missing);
// those of games gone forgotten. True when one was put back.
bool put_back_kept(const Here& h, u::Catalog* c) {
  bool added = false;
  std::error_code ec;
  const fs::path dir = kept_dir(h.write_dir);
  if (h.write_dir.empty() || !fs::is_directory(dir, ec)) return false;
  std::vector<fs::path> files;
  for (const auto& e : fs::directory_iterator(dir, ec))
    if (e.path().extension() == ".json") files.push_back(e.path());
  std::sort(files.begin(), files.end());
  for (const fs::path& f : files) {
    const std::string key = f.stem().string();
    if (holds(*c, key)) continue;
    const std::string why = h.here ? h.here(key) : std::string();
    if (why.empty()) {
      fs::remove(f, ec);
      std::printf("mw3: launcher: catalog: %s forgotten: not in the catalog, and not installed or linked here\n", key.c_str());
      continue;
    }
    u::Catalog k;
    if (!read(read_file(f), h.version, "kept " + key, &k)) continue;
    const auto g = std::find_if(k.games.begin(), k.games.end(), [&](const u::CatalogGame& x) { return x.key == key; });
    if (g == k.games.end()) continue;
    const bool family = std::any_of(c->families.begin(), c->families.end(), [&](const u::CatalogFamily& x) { return x.key == g->family; });
    if (!family) {
      const auto fam = std::find_if(k.families.begin(), k.families.end(), [&](const u::CatalogFamily& x) { return x.key == g->family; });
      if (fam == k.families.end()) continue;
      c->families.push_back(*fam);
    }
    c->games.push_back(*g);
    added = true;
    std::printf("mw3: launcher: catalog: %s kept: %s\n", key.c_str(), why.c_str());
  }
  if (added) {
    std::stable_sort(c->games.begin(), c->games.end(), [](const u::CatalogGame& a, const u::CatalogGame& b) { return a.order < b.order; });
    std::stable_sort(c->families.begin(), c->families.end(), [](const u::CatalogFamily& a, const u::CatalogFamily& b) { return a.order < b.order; });
  }
  return added;
}

// The fetched catalog in use copied for each of its own games (not the built-in's) installed or linked here.
void refresh_kept(const Here& h, const u::Catalog& fetched, const std::string& text) {
  if (h.write_dir.empty() || !h.here) return;
  for (const u::CatalogGame& g : fetched.games)
    if (!holds(u::builtin_catalog(), g.key) && !h.here(g.key).empty()) write_kept(h.write_dir, g.key, text);
}

}  // namespace

void keep(const std::string& write_dir, const std::string& key) {
  if (write_dir.empty() || holds(u::builtin_catalog(), key)) return;
  std::string text;
  {
    std::lock_guard<std::mutex> lock(g_used_mutex);
    text = g_used_text;
  }
  u::Catalog c;
  std::vector<std::string> ignored;
  std::string why;
  if (text.empty() || !u::read_fetched_catalog(text, "unknown", &c, &ignored, &why) || !holds(c, key)) return;  // kept already
  write_kept(write_dir, key, text);
}

std::string cache_dir(const std::string& write_dir) {
  return write_dir.empty() ? std::string() : (fs::path(write_dir) / "catalog").string();
}

bool fetch(const Here& h, bool allow_file) {
  if (h.write_dir.empty()) return false;
  const fs::path dir = cache_dir(h.write_dir);
  u::Catalog best;
  std::string best_text, best_url;
  bool got = false;
  std::string best_sig;
  const std::vector<std::string> keys = u::signing_keys();
  for (const std::string& url : sources(h.write_dir)) {  // each asked: the newest signed one wins (one may lag), the first on a tie
    std::string text, error, sig;
    if (allow_file && url.rfind("file://", 0) != 0) {  // a check run never asks the real addresses
      std::printf("mw3: launcher: catalog at %s: not asked in a check run\n", url.c_str());
      continue;
    }
    if (!sdlfe::curl_get(url, 15, &text, &error, allow_file, true)) {
      std::printf("mw3: launcher: catalog at %s: %s\n", url.c_str(), error.c_str());
      continue;
    }
    if (!sdlfe::curl_get(url + ".sig", 15, &sig, &error, allow_file, true)) {
      std::printf("mw3: launcher: catalog at %s: not used: no signature (%s)\n", url.c_str(), error.c_str());
      continue;
    }
    if (rekit::signed_by(text, sig, keys).empty()) {
      std::printf("mw3: launcher: catalog at %s: not used: its signature does not check against the launcher's keys\n", url.c_str());
      continue;
    }
    std::printf("mw3: launcher: catalog at %s: answered, signed\n", url.c_str());
    u::Catalog c;
    if (!read(text, h.version, url, &c) || (got && c.revision <= best.revision)) continue;
    best = std::move(c), best_text = std::move(text), best_sig = std::move(sig), best_url = url, got = true;
  }
  if (!got) return false;
  std::error_code ec;
  fs::create_directories(dir, ec);
  u::Catalog cached;
  const std::string had = signed_cache(h.write_dir);  // an unsigned cache is replaced by a signed catalog of any revision
  std::vector<std::string> ignored;
  std::string why;
  if (!had.empty() && u::read_fetched_catalog(had, h.version, &cached, &ignored, &why) && cached.revision >= best.revision) {
    std::printf("mw3: launcher: catalog: revision %d, the cache's is %d: kept\n", best.revision, cached.revision);
    fetch_art(cached, best_url, dir, h.carried, allow_file);  // art a failed fetch missed
    return false;
  }
  fetch_art(best, best_url, dir, h.carried, allow_file);
  for (const auto& [name, body] : {std::pair<const char*, const std::string*>{"games.json.sig", &best_sig}, {"games.json", &best_text}}) {
    const fs::path part = dir / (std::string(name) + ".part");
    { std::ofstream(part, std::ios::binary) << *body; }
    fs::rename(part, dir / name, ec);
    if (ec) return fs::remove(part, ec), false;
  }
  write_learned(h.write_dir, best.next_keys, best.revoked_keys);
  std::printf("mw3: launcher: catalog: revision %d cached\n", best.revision);
  return true;
}

// The cached art the catalog in use names (not carried) kept only while it is that catalog's sha256: art an older
// launcher fetched unchecked into the same write dir, or one with no sum, removed (drawn as none).
void check_cached_art(const Here& h) {
  const fs::path dir = cache_dir(h.write_dir), carried = h.carried;
  for (const u::CatalogGame& g : u::active_catalog().games)
    for (const auto& [name, sum] : art_of(g)) {
      std::error_code ec;
      if (name.empty() || fs::exists(carried / name, ec) || !fs::exists(dir / name, ec)) continue;
      if (!sum.empty() && rekit::sha256_file_hex((dir / name).string()) == sum) continue;
      fs::remove(dir / name, ec);
      std::printf("mw3: launcher: catalog: art %s in the cache: %s: removed\n", name.c_str(), sum.empty() ? "no sha256 for it" : "not its sha256");
    }
}

void at_start(const Here& h, bool check_run) {
  {  // where to look: the shipped sources.json (a check run's MW3_SOURCES_FILE), else the built-in copy
    const char* file = check_run ? std::getenv("MW3_SOURCES_FILE") : nullptr;
    std::string said;
    u::use_sources(u::load_sources(file ? std::string(file) : (fs::path(h.carried) / "sources.json").string(), file != nullptr, &said));
    std::printf("mw3: launcher: %s\n", said.c_str());
    if (u::active_sources().keys.empty()) std::printf("mw3: launcher: catalog: sources.json lists no key: no fetched catalog is taken\n");
    if (!h.write_dir.empty()) read_learned(h.write_dir);
  }
  const char* start = check_run ? std::getenv("MW3_CATALOG_START") : nullptr;  // checks: "late" or "none" (header)
  if (start && (std::strcmp(start, "late") == 0 || std::strcmp(start, "none") == 0)) {
    std::printf("mw3: launcher: catalog: the start's fetch %s (MW3_CATALOG_START)\n", start[0] == 'l' ? "left to the launcher's own" : "not made");
  } else if (check_run && sdlfe::live_check()) {  // a release checked without a window: the real addresses, https only
    std::printf("mw3: launcher: catalog: a live check: the real addresses asked\n");
    fetch(h, false);
  } else if (check_run && (std::getenv("MW3_CATALOG_URL") || std::getenv("MW3_SOURCES_FILE"))) {
    fetch(h, true);
  } else if (check_run) {
    std::printf("mw3: launcher: catalog: not asked in a check run\n");
  }
  const u::Catalog& builtin = u::builtin_catalog();
  bool not_signed = false;
  const std::string text = h.write_dir.empty() ? std::string() : signed_cache(h.write_dir, &not_signed);
  if (not_signed) std::printf("mw3: launcher: catalog: the cache is not signed: not used\n");
  u::Catalog fetched, c = builtin;
  bool from_cache = false;
  if (text.empty() || !read(text, h.version, "the cache", &fetched)) {
    std::printf("mw3: launcher: catalog: the built-in one (revision %d)\n", builtin.revision);
  } else if (fetched.revision <= builtin.revision) {
    std::printf("mw3: launcher: catalog: the cache's revision %d is not newer than the built-in %d: the built-in one\n", fetched.revision, builtin.revision);
  } else {
    std::vector<std::string> ignored;
    c = u::merged_catalog(builtin, fetched, &ignored);
    for (const std::string& i : ignored) std::printf("mw3: launcher: catalog: the cache: left out: %s\n", i.c_str());
    from_cache = true;
  }
  if (from_cache) refresh_kept(h, fetched, text);
  g_in_use = from_cache ? fetched.revision : builtin.revision;
  const bool put_back = put_back_kept(h, &c);
  if (!from_cache && !put_back) {
  } else if (!u::use_catalog(c)) {
    std::printf("mw3: launcher: catalog: the built-in one (the games were looked up already)\n");
  } else {
    if (from_cache) {
      std::lock_guard<std::mutex> lock(g_used_mutex);
      g_used_text = text;
    }
    if (from_cache) std::printf("mw3: launcher: catalog: revision %d, fetched (%d games)\n", fetched.revision, u::game_count());
    else std::printf("mw3: launcher: catalog: the built-in one and the kept entries (%d games)\n", u::game_count());
  }
  if (!h.write_dir.empty()) check_cached_art(h);
  say_in_use(h.write_dir);
}

bool may_fetch(bool check_run, bool* allow_file) {
  const bool stand_in = std::getenv("MW3_CATALOG_URL") || std::getenv("MW3_SOURCES_FILE");
  *allow_file = check_run ? stand_in && !sdlfe::live_check() : std::getenv("MW3_CATALOG_URL") != nullptr;
  return !check_run || sdlfe::live_check() || stand_in;
}

int in_use_revision() { return g_in_use; }

bool apply_newer(const Here& h) {
  if (h.write_dir.empty()) return false;
  const std::string text = signed_cache(h.write_dir);
  u::Catalog fetched;
  if (text.empty() || !read(text, h.version, "the cache", &fetched)) return false;
  const u::Catalog& builtin = u::builtin_catalog();
  if (fetched.revision <= g_in_use || fetched.revision <= builtin.revision) return false;
  std::vector<std::string> ignored;
  u::Catalog c = u::merged_catalog(builtin, fetched, &ignored);
  for (const std::string& i : ignored) std::printf("mw3: launcher: catalog: the cache: left out: %s\n", i.c_str());
  refresh_kept(h, fetched, text);
  put_back_kept(h, &c);
  if (!u::replace_catalog(c)) {
    std::printf("mw3: launcher: catalog: revision %d not put in use: it lacks a game the launcher knows\n", fetched.revision);
    g_in_use = fetched.revision;  // not tried again
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(g_used_mutex);
    g_used_text = text;
  }
  g_in_use = fetched.revision;
  std::printf("mw3: launcher: catalog: revision %d in use (fetched while open, %d games)\n", fetched.revision, u::game_count());
  check_cached_art(h);
  say_in_use(h.write_dir);
  return true;
}

std::vector<std::string> addresses(const std::vector<std::string>& catalogs, const std::vector<std::string>& shipped) {
  std::vector<std::string> out;
  for (const std::vector<std::string>* l : {&catalogs, &shipped})
    for (const std::string& s : *l)
      if (std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
  return out;
}

}  // namespace sdlfe::catalog
