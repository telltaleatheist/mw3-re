#include "upgrades/update/update.h"

#include <cctype>
#include <filesystem>
#include <system_error>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace upgrades::update {
namespace {

// A JSON value, as much as GitHub's answer needs.
struct Json {
  enum Type { kNull, kBool, kNumber, kString, kArray, kObject } type = kNull;
  bool b = false;
  std::string s;  // a string, or a number's text
  std::vector<Json> items;
  std::vector<std::pair<std::string, Json>> fields;
  const Json* at(const char* key) const {
    for (const auto& [k, v] : fields)
      if (k == key) return &v;
    return nullptr;
  }
};

class Reader {
 public:
  explicit Reader(const std::string& t) : t_(t) {}
  bool value(Json* out, int depth = 0) {
    if (depth > 64) return fail("nested too deep");
    space();
    if (i_ >= t_.size()) return fail("ends early");
    const char c = t_[i_];
    if (c == '{') return object(out, depth);
    if (c == '[') return array(out, depth);
    if (c == '"') return out->type = Json::kString, string(&out->s);
    if (word("true")) return out->type = Json::kBool, out->b = true, true;
    if (word("false")) return out->type = Json::kBool, out->b = false, true;
    if (word("null")) return out->type = Json::kNull, true;
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
      const size_t from = i_;
      while (i_ < t_.size() && (std::isdigit(static_cast<unsigned char>(t_[i_])) || std::strchr("+-.eE", t_[i_]))) ++i_;
      out->type = Json::kNumber, out->s = t_.substr(from, i_ - from);
      return true;
    }
    return fail("not JSON");
  }
  bool end() {
    space();
    return i_ == t_.size() || fail("text after the value");
  }
  std::string error;

 private:
  bool fail(const char* why) {
    if (error.empty()) error = std::string(why) + " at byte " + std::to_string(i_);
    return false;
  }
  void space() {
    while (i_ < t_.size() && std::isspace(static_cast<unsigned char>(t_[i_]))) ++i_;
  }
  bool word(const char* w) {
    const size_t n = std::strlen(w);
    if (t_.compare(i_, n, w) != 0) return false;
    i_ += n;
    return true;
  }
  bool object(Json* out, int depth) {
    out->type = Json::kObject;
    ++i_;
    space();
    if (i_ < t_.size() && t_[i_] == '}') return ++i_, true;
    for (;;) {
      space();
      std::string key;
      if (i_ >= t_.size() || t_[i_] != '"' || !string(&key)) return fail("a key expected");
      space();
      if (i_ >= t_.size() || t_[i_] != ':') return fail("':' expected");
      ++i_;
      Json v;
      if (!value(&v, depth + 1)) return false;
      out->fields.emplace_back(std::move(key), std::move(v));
      space();
      if (i_ < t_.size() && t_[i_] == ',') { ++i_; continue; }
      if (i_ < t_.size() && t_[i_] == '}') return ++i_, true;
      return fail("',' or '}' expected");
    }
  }
  bool array(Json* out, int depth) {
    out->type = Json::kArray;
    ++i_;
    space();
    if (i_ < t_.size() && t_[i_] == ']') return ++i_, true;
    for (;;) {
      Json v;
      if (!value(&v, depth + 1)) return false;
      out->items.push_back(std::move(v));
      space();
      if (i_ < t_.size() && t_[i_] == ',') { ++i_; continue; }
      if (i_ < t_.size() && t_[i_] == ']') return ++i_, true;
      return fail("',' or ']' expected");
    }
  }
  static void utf8(uint32_t c, std::string* out) {
    if (c < 0x80) {
      *out += static_cast<char>(c);
    } else if (c < 0x800) {
      *out += static_cast<char>(0xc0 | (c >> 6)), *out += static_cast<char>(0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
      *out += static_cast<char>(0xe0 | (c >> 12)), *out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
      *out += static_cast<char>(0x80 | (c & 0x3f));
    } else {
      *out += static_cast<char>(0xf0 | (c >> 18)), *out += static_cast<char>(0x80 | ((c >> 12) & 0x3f));
      *out += static_cast<char>(0x80 | ((c >> 6) & 0x3f)), *out += static_cast<char>(0x80 | (c & 0x3f));
    }
  }
  bool hex4(uint32_t* out) {
    if (i_ + 4 > t_.size()) return fail("a short \\u");
    *out = 0;
    for (int k = 0; k < 4; ++k) {
      const char h = t_[i_++];
      *out <<= 4;
      if (h >= '0' && h <= '9') *out |= h - '0';
      else if (h >= 'a' && h <= 'f') *out |= h - 'a' + 10;
      else if (h >= 'A' && h <= 'F') *out |= h - 'A' + 10;
      else return fail("a bad \\u");
    }
    return true;
  }
  bool string(std::string* out) {
    ++i_;
    out->clear();
    while (i_ < t_.size()) {
      const char c = t_[i_++];
      if (c == '"') return true;
      if (c != '\\') { *out += c; continue; }
      if (i_ >= t_.size()) break;
      const char e = t_[i_++];
      switch (e) {
        case 'n': *out += '\n'; break;
        case 't': *out += '\t'; break;
        case 'r': *out += '\r'; break;
        case 'b': *out += '\b'; break;
        case 'f': *out += '\f'; break;
        case 'u': {
          uint32_t c1 = 0;
          if (!hex4(&c1)) return false;
          if (c1 >= 0xd800 && c1 < 0xdc00 && t_.compare(i_, 2, "\\u") == 0) {  // a surrogate pair
            i_ += 2;
            uint32_t c2 = 0;
            if (!hex4(&c2)) return false;
            c1 = 0x10000 + ((c1 - 0xd800) << 10) + (c2 - 0xdc00);
          }
          utf8(c1, out);
          break;
        }
        default: *out += e; break;  // \" \\ \/
      }
    }
    return fail("a string not closed");
  }
  const std::string& t_;
  size_t i_ = 0;
};

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

const Release* newest_package(const std::vector<Release>& releases) {
  const Release* best = nullptr;
  for (const Release& r : releases)
    if (!r.draft && !r.prerelease && package_tag(r.tag) && (!best || compare_versions(r.tag, best->tag) > 0)) best = &r;
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
  Verdict v;
  if (json.empty()) {
    v.words = "Could not check for updates" + (fetch_error.empty() ? std::string(".") : ": " + fetch_error + ".") +
              " Is this computer online?";
    return v;
  }
  std::vector<Release> all;
  std::string error;
  if (!parse_releases(json, &all, &error)) {
    v.words = "Could not check for updates: GitHub's answer could not be read (" + error + ").";
    return v;
  }
  const Release* newest = newest_package(all);
  if (!newest) {
    v.words = "Could not check for updates: no release of MechWarrior 3 Rebuild was found.";
    return v;
  }
  v.release = *newest;
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
