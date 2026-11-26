#pragma once
// Ours: the launcher's small JSON reader (from update.cpp, 2026-11-26), for GitHub's release lists and the games
// catalog (games.json). Its own, not gamez's zutl_json: the launcher's code stays free of the game's (launcher-split.md).
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace upgrades::update::detail {

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

}  // namespace upgrades::update::detail
