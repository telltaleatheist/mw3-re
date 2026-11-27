// The games catalog fetched and cached (launcher split b7); sdl_catalog.h.
#include "frontend/sdl/sdl_catalog.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "frontend/sdl/sdl_update.h"
#include "upgrades/update/games.h"

namespace fs = std::filesystem;
namespace u = upgrades::update;

namespace sdlfe::catalog {

namespace {

// The hub's main on GitHub (docs/launcher-split.md, b7), asked after the homes.
const char* const kGithub = "https://raw.githubusercontent.com/telltaleatheist/mw3-re/main/catalog/games.json";

// Only after the start (fetch's thread): the homes are the catalog in use's. A check run's fetch, before it, has
// MW3_CATALOG_URL and asks only that.
std::vector<std::string> sources() {
  if (const char* env = std::getenv("MW3_CATALOG_URL")) {
    const std::string s = env;
    const size_t bar = s.find('|');
    if (bar == std::string::npos) return {s};
    return {s.substr(0, bar), s.substr(bar + 1)};
  }
  return addresses(u::active_homes());
}

// The catalog in use, said: its homes, and each game's original download (a slot for later, never fetched).
void say_in_use() {
  std::string homes;
  for (const u::CatalogHome& h : u::active_homes()) homes += h.url + ", ";
  std::printf("mw3: launcher: catalog: homes: %sthen GitHub\n", homes.c_str());
  for (const u::CatalogGame& g : u::active_catalog().games)
    if (!g.original.empty()) std::printf("mw3: launcher: catalog: %s names an original download (not used)\n", g.key.c_str());
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
void fetch_art(const u::Catalog& c, const std::string& url, const fs::path& dir, const std::string& carried_dir, bool allow_file) {
  const size_t slash = url.rfind('/');
  if (slash == std::string::npos) return;
  const std::string base = url.substr(0, slash + 1) + "art/";
  const fs::path carried = carried_dir;
  for (const u::CatalogGame& g : c.games)
    for (const std::string& name : {g.badge, g.heading_badge, g.card}) {
      std::error_code ec;
      if (name.empty() || fs::exists(carried / name, ec) || fs::exists(dir / name, ec)) continue;
      std::string png, error;
      if (!sdlfe::curl_get(base + name, 15, &png, &error, allow_file) || !is_png(png)) {
        std::printf("mw3: launcher: catalog: art %s not fetched: %s\n", name.c_str(), error.empty() ? "not a PNG" : error.c_str());
        continue;
      }
      const fs::path part = dir / (name + ".part");
      { std::ofstream(part, std::ios::binary) << png; }
      fs::rename(part, dir / name, ec);
      if (ec) fs::remove(part, ec);
    }
}

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
  for (const std::string& url : sources()) {  // both asked: the newest wins (one home may lag)
    std::string text, error;
    if (!sdlfe::curl_get(url, 15, &text, &error, allow_file)) {
      std::printf("mw3: launcher: catalog at %s: %s\n", url.c_str(), error.c_str());
      continue;
    }
    std::printf("mw3: launcher: catalog at %s: answered\n", url.c_str());
    u::Catalog c;
    if (!read(text, h.version, url, &c) || (got && c.revision <= best.revision)) continue;
    best = std::move(c), best_text = std::move(text), best_url = url, got = true;
  }
  if (!got) return false;
  std::error_code ec;
  fs::create_directories(dir, ec);
  u::Catalog cached;
  const std::string had = read_file(dir / "games.json");
  std::vector<std::string> ignored;
  std::string why;
  if (!had.empty() && u::read_fetched_catalog(had, h.version, &cached, &ignored, &why) && cached.revision >= best.revision) {
    std::printf("mw3: launcher: catalog: revision %d, the cache's is %d: kept\n", best.revision, cached.revision);
    fetch_art(cached, best_url, dir, h.carried, allow_file);  // art a failed fetch missed
    return false;
  }
  fetch_art(best, best_url, dir, h.carried, allow_file);
  const fs::path part = dir / "games.json.part";
  { std::ofstream(part, std::ios::binary) << best_text; }
  fs::rename(part, dir / "games.json", ec);
  if (ec) return fs::remove(part, ec), false;
  std::printf("mw3: launcher: catalog: revision %d cached\n", best.revision);
  return true;
}

void at_start(const Here& h, bool check_run) {
  if (check_run && std::getenv("MW3_CATALOG_URL")) fetch(h, true);
  else if (check_run) std::printf("mw3: launcher: catalog: not asked in a check run\n");
  const u::Catalog& builtin = u::builtin_catalog();
  const std::string text = h.write_dir.empty() ? std::string() : read_file(fs::path(cache_dir(h.write_dir)) / "games.json");
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
  say_in_use();
}

std::vector<std::string> addresses(const std::vector<u::CatalogHome>& homes) {
  std::vector<std::string> out;
  const std::string owner = u::catalog_owner();
  for (const u::CatalogHome& h : homes) out.push_back(h.url + "/" + owner + "/mw3-re/raw/branch/main/catalog/games.json");
  out.push_back(kGithub);
  return out;
}

}  // namespace sdlfe::catalog
