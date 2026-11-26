// The games catalog fetched and cached (launcher split b7); sdl_catalog.h.
#include "frontend/sdl/sdl_catalog.h"

#include <cstdio>
#include <cstdlib>
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

// The hub's main (docs/launcher-split.md, b7): the mirror's first, then GitHub's, as the update check asks.
const char* const kMirror = "https://git.owenmorgan.com/telltaleatheist/mw3-re/raw/branch/main/catalog/games.json";
const char* const kGithub = "https://raw.githubusercontent.com/telltaleatheist/mw3-re/main/catalog/games.json";

std::vector<std::string> sources() {
  if (const char* env = std::getenv("MW3_CATALOG_URL")) {
    const std::string s = env;
    const size_t bar = s.find('|');
    if (bar == std::string::npos) return {s};
    return {s.substr(0, bar), s.substr(bar + 1)};
  }
  return {kMirror, kGithub};
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

}  // namespace

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
  u::Catalog fetched;
  if (text.empty() || !read(text, h.version, "the cache", &fetched)) {
    std::printf("mw3: launcher: catalog: the built-in one (revision %d)\n", builtin.revision);
    return;
  }
  if (fetched.revision <= builtin.revision) {
    std::printf("mw3: launcher: catalog: the cache's revision %d is not newer than the built-in %d: the built-in one\n", fetched.revision, builtin.revision);
    return;
  }
  std::vector<std::string> ignored;
  const u::Catalog merged = u::merged_catalog(builtin, fetched, &ignored);
  for (const std::string& i : ignored) std::printf("mw3: launcher: catalog: the cache: left out: %s\n", i.c_str());
  if (!u::use_catalog(merged)) {
    std::printf("mw3: launcher: catalog: the built-in one (the games were looked up already)\n");
    return;
  }
  std::printf("mw3: launcher: catalog: revision %d, fetched (%d games)\n", fetched.revision, u::game_count());
}

}  // namespace sdlfe::catalog
