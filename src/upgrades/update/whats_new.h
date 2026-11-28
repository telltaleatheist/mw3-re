#pragma once
// Ours, not the original's: the launcher's WHAT'S NEW (2.2.0; docs/launcher.md, A game's page). A game's release
// notes from the releases lists the launcher already reads, the versions between the installed one and the newest,
// the notes' Markdown as plain lines for the page, a cache for offline starts, and when the page opens on them.
#include <functional>
#include <string>
#include <vector>

#include "upgrades/update/update.h"

namespace upgrades::update {

// Every release `mine` takes (a game's tags, game_tag; the launcher's packages), no draft or prerelease, one a
// tag: the first one met with notes (the homes' lists in their download order), newest first.
std::vector<Notes> collect_notes(const std::vector<Release>& releases, const std::function<bool(const Release&)>& mine);

// What the page shows: with `newest` newer than `installed`, every release after `installed` up to `newest`, newest
// first; otherwise `installed`'s own (`newest`'s when nothing is installed). Empty when none of them is known.
std::vector<Notes> notes_to_show(const std::vector<Notes>& all, const std::string& installed, const std::string& newest);

// A release's Markdown as the page's lines, done simply: headings, "- " and "* " items (nested ones indented), a
// paragraph's wrapped lines joined, the marks dropped and a link as its text (notes_lines). Left out, for every
// repo alike: a section headed "Download..." or "Before you install" (to the next heading as high), a paragraph
// naming SHA256SUMS, and a "Reporting a bug" one.
struct NoteLine {
  enum Kind { kText, kHeading, kItem, kBlank } kind = kText;
  std::string text;
};
std::vector<NoteLine> whats_new_lines(const std::string& markdown);

// The cache of a game's notes (a releases list's JSON: tag_name and body), read back by parse_releases.
std::string notes_cache(const std::vector<Notes>& notes);
bool read_notes_cache(const std::string& json, std::vector<Notes>* out);

// The page opens on WHAT'S NEW once after an update: `seen` the version it last knew installed ("" never),
// `installed` the version now. True only for an update from a version it knew (never a first install, never the
// first open after the launcher learnt of WHAT'S NEW).
bool land_on_whats_new(const std::string& seen, const std::string& installed);

}  // namespace upgrades::update
