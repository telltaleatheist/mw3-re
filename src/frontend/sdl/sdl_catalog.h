#pragma once
// The games catalog fetched (launcher split b7, 2026-11-26; docs/launcher.md, The games catalog): a newer
// games.json from the hub (mw3-re), cached in the write dir with its art, merged over the built-in one at the next
// start. Data only: nothing about the player is sent, and no catalog change touches an installed game.

#include <functional>
#include <string>

namespace sdlfe::catalog {

// The cache: <write dir>/catalog/games.json and the art it names (badges the launcher does not carry).
std::string cache_dir(const std::string& write_dir);

// At the start, before any game is looked up: the cache read (a bad or older one left out, said in the log) and
// merged over the built-in catalog, put in use. A check run (hidden, a shot) fetches first, and only from
// MW3_CATALOG_URL; without it, nothing is asked.
// `version` this launcher's (MW3_VERSION: an entry needing a newer one is left out), `carried` its art's folder
// (assets/launcher: art there is not fetched).
// `here` says why a game key is on this computer ("installed here", "linked here"; "" when not): a game installed or
// linked keeps its entry when a later catalog drops it (kept, below).
struct Here {
  std::string write_dir, version, carried;
  std::function<std::string(const std::string& key)> here;
};
void at_start(const Here& h, bool check_run);

// A game installed or linked keeps its last-known entry (2026-11-26, after DEPLOY.md's "never delete an entry"): a
// fetched-only game (not in the built-in catalog) here has the catalog it came from copied to
// <cache>/kept/<key>.json, at each start and at its install or link (keep); a catalog in use without its key gets that
// entry back ("catalog: <key> kept: installed here"). One no longer here is forgotten at the next start.
void keep(const std::string& write_dir, const std::string& key);

// Both homes asked (MW3_CATALOG_URL in checks: one address, or "BACKUP|GITHUB"; file:// there too); the newest
// good one written to the cache with its art, unless the cache's is as new. False: nothing new cached.
bool fetch(const Here& h, bool allow_file);

}  // namespace sdlfe::catalog
