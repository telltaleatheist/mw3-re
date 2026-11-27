#pragma once
// The games catalog fetched (launcher split b7, 2026-11-26; docs/launcher.md, The games catalog): a newer
// games.json from the hub (mw3-re), cached in the write dir with its art, merged over the built-in one at once
// (apply_newer, while open; at_start, the next start). Data only: nothing about the player is sent, and no catalog change touches an installed game.

#include <functional>
#include <string>
#include <vector>

#include "upgrades/update/games.h"

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
// MW3_CATALOG_START (checks): "late" leaves a check run's start fetch to the launcher's own (on a thread, applied when
// it lands, as a player's start does); "none" makes none at the start (the fetches CHECK FOR UPDATES and the rest make).
void at_start(const Here& h, bool check_run);

// Whether a run may fetch the catalog, and whether file:// addresses are taken (`allow_file`): a player's run always
// (file:// with MW3_CATALOG_URL, checks), a check run only from a stand-in (MW3_CATALOG_URL, MW3_SOURCES_FILE) or in a
// live check (MW3_LIVE_CHECK, https only), as at the start.
bool may_fetch(bool check_run, bool* allow_file);

// While open (2026-11-27): the cache's signed catalog put in use when it is newer than the one in use, merged over
// the built-in one as at the start (u::replace_catalog: every game keeps its place, new ones follow). Said as
// "catalog: revision N in use (fetched while open, G games)". True when it was.
bool apply_newer(const Here& h);
int in_use_revision();

// A game installed or linked keeps its last-known entry (2026-11-26, after DEPLOY.md's "never delete an entry"): a
// fetched-only game (not in the built-in catalog) here has the catalog it came from copied to
// <cache>/kept/<key>.json, at each start and at its install or link (keep); a catalog in use without its key gets that
// entry back ("catalog: <key> kept: installed here"). One no longer here is forgotten at the next start.
void keep(const std::string& write_dir, const std::string& key);

// Every address asked (sources(): MW3_CATALOG_URL in checks, one address or "A|B"; file:// there too), each with its
// <address>.sig: one not signed by a key the launcher knows (sources.json's, or learned from a signed catalog's
// next_keys) is passed over; the newest signed one written to the cache with its .sig and art, unless the cache's is
// as new, and its next_keys learned (<cache>/keys.txt). False: nothing new cached.
bool fetch(const Here& h, bool allow_file);

// The catalog's addresses in the order asked (2026-11-26, docs/launcher.md, Where the launcher looks): the catalog in
// use's own (`catalogs`, its catalog_sources) first, then sources.json's (`shipped`), each once.
std::vector<std::string> addresses(const std::vector<std::string>& catalogs, const std::vector<std::string>& shipped);

}  // namespace sdlfe::catalog
