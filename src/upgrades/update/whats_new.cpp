#include "upgrades/update/whats_new.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "upgrades/update/games.h"

namespace upgrades::update {

std::vector<Notes> collect_notes(const std::vector<Release>& releases, const std::function<bool(const Release&)>& mine) {
  std::vector<Notes> out;
  for (const Release& r : releases) {
    if (r.draft || r.prerelease || !mine(r)) continue;
    auto at = std::find_if(out.begin(), out.end(), [&](const Notes& n) { return n.tag == r.tag; });
    if (at == out.end()) out.push_back({r.tag, r.notes});
    else if (at->text.empty()) at->text = r.notes;  // a home that has its words
  }
  std::stable_sort(out.begin(), out.end(), [](const Notes& a, const Notes& b) { return compare_game(a.tag, b.tag) > 0; });
  return out;
}

std::vector<Notes> notes_to_show(const std::vector<Notes>& all, const std::string& installed, const std::string& newest) {
  std::vector<Notes> out;
  if (!installed.empty() && !newest.empty() && compare_game(newest, installed) > 0) {
    for (const Notes& n : all)
      if (compare_game(n.tag, installed) > 0 && compare_game(n.tag, newest) <= 0) out.push_back(n);
    return out;
  }
  const std::string& one = installed.empty() ? newest : installed;
  for (const Notes& n : all)
    if (!one.empty() && compare_game(n.tag, one) == 0) return {n};
  return out;
}

namespace {

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
// "## Download" -> 2; 0 when not a heading.
int heading_level(const std::string& t) {
  size_t n = 0;
  while (n < t.size() && t[n] == '#') ++n;
  return n > 0 && n <= 6 && (n == t.size() || t[n] == ' ') ? static_cast<int>(n) : 0;
}
bool item_start(const std::string& t) { return t.size() > 1 && (t[0] == '-' || t[0] == '*') && t[1] == ' '; }

}  // namespace

std::vector<NoteLine> whats_new_lines(const std::string& markdown) {
  struct Block {
    NoteLine::Kind kind;
    std::string raw;  // its Markdown, a paragraph's lines joined
    bool gap = false; // a blank line (or a heading) before it
  };
  std::vector<Block> blocks;
  int skip = 0;            // the level of a heading being left out, 0 none
  bool open = false;       // the last block still takes continuation lines
  bool gap = false;
  size_t at = 0;
  while (at <= markdown.size()) {
    const size_t nl = markdown.find('\n', at);
    std::string line = markdown.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
    at = nl == std::string::npos ? markdown.size() + 1 : nl + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t lead = line.find_first_not_of(" \t");
    if (lead == std::string::npos) {
      open = false, gap = true;
      continue;
    }
    const std::string t = line.substr(lead);
    if (const int level = heading_level(t)) {
      if (skip && level > skip) continue;
      std::string words = lower(t.substr(std::min(t.size(), t.find_first_not_of("# "))));
      skip = words.rfind("download", 0) == 0 || words.rfind("before you install", 0) == 0 ? level : 0;
      if (!skip) blocks.push_back({NoteLine::kHeading, t, true});
      open = false, gap = false;
      continue;
    }
    if (skip) continue;
    if (item_start(t)) {
      blocks.push_back({NoteLine::kItem, line, gap});
      open = true, gap = false;
    } else if (open) {
      blocks.back().raw += " " + t;  // a wrapped line of the paragraph or item above
    } else {
      blocks.push_back({NoteLine::kText, t, gap});
      open = true, gap = false;
    }
  }
  std::vector<NoteLine> out;
  for (const Block& b : blocks) {
    if (b.kind != NoteLine::kHeading &&
        (b.raw.find("SHA256SUMS") != std::string::npos || lower(b.raw).find("**reporting a bug") != std::string::npos))
      continue;
    const std::vector<std::string> clean = notes_lines(b.raw);
    if (clean.empty() || clean[0].empty()) continue;
    if ((b.gap || b.kind == NoteLine::kHeading) && !out.empty() && out.back().kind != NoteLine::kBlank && out.back().kind != NoteLine::kHeading)
      out.push_back({NoteLine::kBlank, ""});  // a paragraph's gap, none under a heading
    out.push_back({b.kind, clean[0]});
  }
  while (!out.empty() && out.back().kind == NoteLine::kBlank) out.pop_back();
  return out;
}

std::string notes_cache(const std::vector<Notes>& notes) {
  auto quote = [](const std::string& s) {
    std::string q = "\"";
    for (const char c : s) {
      const unsigned char u = static_cast<unsigned char>(c);
      if (c == '"' || c == '\\') q += '\\', q += c;
      else if (c == '\n') q += "\\n";
      else if (c == '\r') q += "\\r";
      else if (c == '\t') q += "\\t";
      else if (u < 0x20) {
        char e[8];
        std::snprintf(e, sizeof e, "\\u%04x", u);
        q += e;
      } else {
        q += c;
      }
    }
    return q + "\"";
  };
  std::string j = "[";
  for (size_t i = 0; i < notes.size(); ++i)
    j += std::string(i ? ",\n" : "\n") + "{\"tag_name\": " + quote(notes[i].tag) + ", \"body\": " + quote(notes[i].text) + "}";
  return j + "\n]\n";
}

bool read_notes_cache(const std::string& json, std::vector<Notes>* out) {
  std::vector<Release> r;
  std::string error;
  if (!parse_releases(json, &r, &error)) return false;
  out->clear();
  for (const Release& x : r) out->push_back({x.tag, x.notes});
  return true;
}

bool land_on_whats_new(const std::string& seen, const std::string& installed) {
  return !seen.empty() && !installed.empty() && compare_game(installed, seen) > 0;
}

}  // namespace upgrades::update
