#include "upgrades/update/update.h"

#include "upgrades/update/json_reader.h"
#include "upgrades/update/whats_new.h"

#include <cctype>
#include <filesystem>
#include <system_error>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

#if defined(__APPLE__)
#include <stdio.h>     // renamex_np
#include <sys/time.h>  // utimes
#endif

namespace upgrades::update {
namespace {

using detail::Json;
using detail::Reader;

std::string text_of(const Json* j) { return j && j->type == Json::kString ? j->s : std::string(); }
bool bool_of(const Json* j) { return j && j->type == Json::kBool && j->b; }

// The numbers of a version: "v1.0.5-264-gHASH" gives 1, 0, 5 (what follows the first '-' is not a part).
std::vector<long> numbers(const std::string& v) {
  std::vector<long> out;
  size_t i = 0;
  while (i < v.size() && !std::isdigit(static_cast<unsigned char>(v[i]))) ++i;  // "v", "pm-v"
  const size_t end = v.find('-', i);
  const std::string core = v.substr(i, end == std::string::npos ? std::string::npos : end - i);
  size_t at = 0;
  while (at < core.size()) {
    const size_t dot = core.find('.', at);
    const std::string part = core.substr(at, dot == std::string::npos ? std::string::npos : dot - at);
    if (part.empty() || part.find_first_not_of("0123456789") != std::string::npos) break;
    out.push_back(std::strtol(part.c_str(), nullptr, 10));
    if (dot == std::string::npos) break;
    at = dot + 1;
  }
  return out;
}

}  // namespace

bool parse_releases(const std::string& json, std::vector<Release>* out, std::string* error) {
  out->clear();
  Reader r(json);
  Json root;
  if (!r.value(&root) || !r.end()) return *error = r.error, false;
  if (root.type != Json::kArray) return *error = "not a list of releases", false;
  for (const Json& j : root.items) {
    if (j.type != Json::kObject) continue;
    Release rel;
    rel.tag = text_of(j.at("tag_name"));
    rel.notes = text_of(j.at("body"));
    rel.draft = bool_of(j.at("draft"));
    rel.prerelease = bool_of(j.at("prerelease"));
    if (const Json* assets = j.at("assets"); assets && assets->type == Json::kArray)
      for (const Json& a : assets->items)
        if (a.type == Json::kObject) {
          const Json* size = a.at("size");
          rel.assets.push_back({text_of(a.at("name")), text_of(a.at("browser_download_url")),
                                size && size->type == Json::kNumber ? std::strtoull(size->s.c_str(), nullptr, 10) : 0});
        }
    if (!rel.tag.empty()) out->push_back(std::move(rel));
  }
  return true;
}

bool package_tag(const std::string& tag) {
  if (tag.size() < 6 || tag[0] != 'v') return false;
  const std::string rest = tag.substr(1);
  if (rest.find_first_not_of("0123456789.") != std::string::npos) return false;
  return numbers(tag).size() == 3 && rest.front() != '.' && rest.back() != '.' && rest.find("..") == std::string::npos;
}

const Release* newest_package(const std::vector<Release>& releases, Platform p) {
  const Release* best = nullptr;
  for (const Release& r : releases)
    if (!r.draft && !r.prerelease && package_tag(r.tag) && platform_asset(r, p) && (!best || compare_versions(r.tag, best->tag) > 0))
      best = &r;
  return best;
}

int compare_versions(const std::string& a, const std::string& b) {
  const std::vector<long> x = numbers(a), y = numbers(b);
  if (x.empty() != y.empty()) return x.empty() ? -1 : 1;
  for (size_t i = 0; i < x.size() || i < y.size(); ++i) {
    const long p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
    if (p != q) return p < q ? -1 : 1;
  }
  return 0;
}

std::vector<std::string> notes_lines(const std::string& markdown) {
  std::vector<std::string> out;
  const auto inline_marks = [](std::string s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
      if (s[i] == '[') {  // [text](url) -> text
        const size_t close = s.find("](", i);
        const size_t end = close == std::string::npos ? close : s.find(')', close);
        if (end != std::string::npos) {
          r += s.substr(i + 1, close - i - 1);
          i = end;
          continue;
        }
      }
      if (s[i] == '`' || (s[i] == '*' && (i + 1 < s.size() && s[i + 1] == '*'))) {
        if (s[i] == '*') ++i;
        continue;
      }
      if (s[i] == '*' || s[i] == '_') {  // *italic*, _italic_: the mark between letters kept (file_name)
        const bool word_before = i > 0 && std::isalnum(static_cast<unsigned char>(s[i - 1]));
        const bool word_after = i + 1 < s.size() && std::isalnum(static_cast<unsigned char>(s[i + 1]));
        if (!(word_before && word_after)) continue;
      }
      r += s[i];
    }
    return r;
  };
  size_t at = 0;
  bool blank = true;  // no blank first line, none doubled
  while (at <= markdown.size()) {
    const size_t nl = markdown.find('\n', at);
    std::string line = markdown.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
    at = nl == std::string::npos ? markdown.size() + 1 : nl + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    size_t lead = line.find_first_not_of(" \t");
    if (lead == std::string::npos) {
      if (!blank) out.emplace_back(), blank = true;
      continue;
    }
    std::string text = line.substr(lead);
    if (text[0] == '#') {
      text = text.substr(text.find_first_not_of('#'));
      text = text.substr(std::min(text.size(), text.find_first_not_of(' ')));
    } else if ((text[0] == '-' || text[0] == '*') && text.size() > 1 && text[1] == ' ') {
      text = std::string(lead > 1 ? "    " : "") + "\xe2\x80\xa2 " + text.substr(2);  // a nested item indented
    }
    out.push_back(inline_marks(text));
    blank = false;
  }
  while (!out.empty() && out.back().empty()) out.pop_back();
  return out;
}

Platform this_platform() {
#if defined(_WIN32)
  return Platform::kWindows;
#elif defined(__APPLE__)
  return Platform::kMac;
#else
  return Platform::kLinux;
#endif
}

const Asset* platform_asset(const Release& r, Platform p) {
  const auto ends = [](const std::string& s, const char* e) {
    const size_t n = std::strlen(e);
    return s.size() >= n && s.compare(s.size() - n, n, e) == 0;
  };
  for (const Asset& a : r.assets) {
    const bool ours = p == Platform::kWindows ? a.name.rfind("MW3-Rebuild-Setup-", 0) == 0 && ends(a.name, ".exe")
                      : p == Platform::kMac   ? a.name.rfind("MW3-Rebuild-", 0) == 0 && ends(a.name, "-mac.dmg")
                                              : a.name.rfind("MW3-Rebuild-", 0) == 0 && ends(a.name, "-x86_64.AppImage");
    if (ours && !a.url.empty()) return &a;
  }
  return nullptr;
}

const char* platform_package_words(Platform p) {
  return p == Platform::kWindows ? "the Windows Setup" : p == Platform::kMac ? "the Mac app (.dmg)" : "the Linux AppImage";
}

std::string sums_line(const std::string& sums, const std::string& name) {
  size_t at = 0;
  while (at < sums.size()) {
    const size_t nl = sums.find('\n', at);
    std::string line = sums.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
    at = nl == std::string::npos ? sums.size() : nl + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.size() < 66) continue;
    std::string hash = line.substr(0, 64);
    for (char& ch : hash) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (hash.find_first_not_of("0123456789abcdef") != std::string::npos || line[64] != ' ') continue;
    std::string file = line.substr(65);
    if (!file.empty() && (file[0] == ' ' || file[0] == '*')) file.erase(0, 1);  // "  name" (text) or " *name" (binary)
    if (file == name) return hash;
  }
  return {};
}

bool swap_in(const std::string& fresh, const std::string& target, std::string* error) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path to(target), staged(target + ".new"), old(target + ".old");
  if (!fs::exists(fresh, ec)) return *error = "the new copy is not there: " + fresh, false;
  if (!fs::exists(to, ec)) return *error = "the program to replace is not there: " + target, false;
  // Leftovers of an earlier try cleared first, only when they are ours: a file, or a folder holding our launcher as
  // the Mac app does; anything else in the way is the player's and stops the update.
  for (const fs::path& left : {staged, old}) {
    if (!fs::exists(fs::symlink_status(left, ec))) continue;
    const bool ours = fs::is_regular_file(fs::symlink_status(left, ec)) ||
                      (fs::is_directory(fs::symlink_status(left, ec)) && fs::is_regular_file(left / "Contents" / "MacOS" / "mw3launcher", ec));
    if (!ours) return *error = left.string() + " is in the way and is not ours: move it and try again", false;
    fs::remove_all(left, ec);
  }
  fs::copy(fresh, staged, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
  if (ec) {
    const std::string why = ec.message();
    fs::remove_all(staged, ec);
    return *error = "the new copy could not be written beside the old one (" + why + ")", false;
  }
  fs::permissions(staged, fs::status(fresh, ec).permissions(), ec);  // an AppImage's execute bit
#if defined(__APPLE__)
  // A Mac app keeps its own folder and takes the new one's Contents, the two exchanged in one step: the Dock follows
  // the folder it was shown, so one renamed to X.app.old became a plain folder there (docs/launcher.md). Where the
  // disk can't exchange (or it may not), the whole folder is moved as before.
  if (fs::is_directory(staged, ec) && fs::is_directory(staged / "Contents", ec) && fs::is_directory(to / "Contents", ec) &&
      renamex_np((staged / "Contents").c_str(), (to / "Contents").c_str(), RENAME_SWAP) == 0) {
    utimes(to.c_str(), nullptr);  // the Finder and the Dock read the new icon and name
    fs::remove_all(staged, ec);   // the old Contents; a failure leaves a stale .new, cleared by the next update
    return true;
  }
#endif
  fs::rename(to, old, ec);
  if (ec) {
    const std::string why = ec.message();
    fs::remove_all(staged, ec);
    return *error = "the old copy could not be moved aside (" + why + ")", false;
  }
  fs::rename(staged, to, ec);
  if (ec) {
    const std::string why = ec.message();
    std::error_code back;
    fs::rename(old, to, back);  // the old one back in its place
    fs::remove_all(staged, back);
    return *error = "the new copy could not be put in place (" + why + "); the old one is kept", false;
  }
  fs::remove_all(old, ec);  // a failure here leaves only a stale .old, cleared by the next update
  return true;
}

std::string plain_version(const std::string& tag) {
  return tag.size() > 1 && tag[0] == 'v' && std::isdigit(static_cast<unsigned char>(tag[1])) ? tag.substr(1) : tag;
}

Verdict judge(const std::string& current, const std::string& json, const std::string& fetch_error) {
  return judge(current, std::vector<Answer>{{json, fetch_error}}, this_platform());
}

Verdict judge(const std::string& current, const std::vector<Answer>& answers) { return judge(current, answers, this_platform()); }

Homes read_homes(const std::vector<Answer>& answers) {
  Homes h;
  for (const Answer& a : answers) {
    std::vector<Release> some;
    std::string error;
    if (a.json.empty()) {
      if (!h.failed) h.failed = &a;
    } else if (!parse_releases(a.json, &some, &error)) {
      if (!h.failed) h.failed = &a, h.parse_error = error;
    } else {
      ++h.read;
      for (Release& r : some) r.unsigned_sums = a.unsigned_sums, h.releases.push_back(std::move(r)), h.first.push_back(a.download_first);
    }
  }
  return h;
}

Release with_homes(const Homes& h, const std::string& tag, const std::function<bool(const Release&)>& usable) {
  std::vector<size_t> order;
  for (bool first : {true, false})
    for (size_t i = 0; i < h.releases.size(); ++i)
      if (h.first[i] == first && h.releases[i].tag == tag && !h.releases[i].draft && !h.releases[i].prerelease) order.push_back(i);
  Release out;
  size_t base = h.releases.size();
  for (size_t i : order)
    if (usable(h.releases[i])) {
      base = i, out = h.releases[i];
      break;
    }
  if (base == h.releases.size()) return {};
  out.first_home = h.first[base];
  out.other_homes.clear(), out.other_unsigned_sums.clear();
  for (size_t i : order)
    if (i != base && !h.releases[i].assets.empty())
      out.other_homes.push_back(h.releases[i].assets), out.other_unsigned_sums.push_back(h.releases[i].unsigned_sums);
  return out;
}

Verdict judge(const std::string& current, const std::vector<Answer>& answers, Platform p) {
  Verdict v;
  const Homes h = read_homes(answers);
  if (h.read == 0) {
    if (!h.failed) v.words = "Could not check for updates.";
    else if (!h.parse_error.empty()) v.words = "Could not check for updates: the releases list could not be read (" + h.parse_error + ").";
    else
      v.words = "Could not check for updates" + (h.failed->fetch_error.empty() ? std::string(".") : ": " + h.failed->fetch_error + ".") +
                " Is this computer online?";
    return v;
  }
  const Release* newest = newest_package(h.releases, p);
  if (!newest) {
    v.words = "Could not check for updates: no release of MechWarrior 3 Rebuild was found.";
    return v;
  }
  v.release = with_homes(h, newest->tag, [p](const Release& r) { return platform_asset(r, p) != nullptr; });
  v.notes = collect_notes(h.releases, [p](const Release& r) { return package_tag(r.tag) && platform_asset(r, p) != nullptr; });
  if (numbers(current).empty() || compare_versions(newest->tag, current) > 0) {
    v.kind = Verdict::kNewer;
    v.words = "MechWarrior 3 Rebuild " + plain_version(newest->tag) + " is out (this is " + plain_version(current) + ").";
  } else {
    v.kind = Verdict::kUpToDate;
    v.words = "This is the newest MechWarrior 3 Rebuild (" + plain_version(current) + ").";
  }
  return v;
}

}  // namespace upgrades::update
