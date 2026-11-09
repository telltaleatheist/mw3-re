// Ours (docs/modding.md, mech3ax's JSON): see zutl_json.h.
#include "gamez/zutil/zutl_json.h"

#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "gamez/zutil/zutl_path.h"

namespace zutl_json {
namespace {

constexpr int kMaxDepth = 64;

struct Parser {
  const std::string& t;
  size_t at = 0;
  int line = 1;
  int depth = 0;
  std::string why;
  std::vector<std::string> path;  // ".name" or "[i]" at each depth, for the error

  bool fail(const std::string& what) {
    if (!why.empty()) return false;
    std::string p = "$";
    for (const std::string& c : path) p += c;
    why = "line " + std::to_string(line) + " (at " + p + "): " + what;
    return false;
  }
  void space() {
    while (at < t.size()) {
      const char c = t[at];
      if (c == '\n') ++line;
      else if (c != ' ' && c != '\t' && c != '\r') return;
      ++at;
    }
  }
  bool word(const char* w) {
    const size_t n = std::strlen(w);
    if (t.compare(at, n, w) != 0) return false;
    at += n;
    return true;
  }
  static int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }
  bool hex4(uint32_t* u) {
    if (at + 4 > t.size()) return fail("a \\u escape cut short");
    *u = 0;
    for (int i = 0; i < 4; ++i) {
      const int h = hex(t[at + i]);
      if (h < 0) return fail("a \\u escape that is not four hex digits");
      *u = *u << 4 | static_cast<uint32_t>(h);
    }
    at += 4;
    return true;
  }
  static void put_utf8(std::string& o, uint32_t u) {
    if (u < 0x80) {
      o += static_cast<char>(u);
    } else if (u < 0x800) {
      o += static_cast<char>(0xc0 | u >> 6), o += static_cast<char>(0x80 | (u & 0x3f));
    } else if (u < 0x10000) {
      o += static_cast<char>(0xe0 | u >> 12), o += static_cast<char>(0x80 | ((u >> 6) & 0x3f));
      o += static_cast<char>(0x80 | (u & 0x3f));
    } else {
      o += static_cast<char>(0xf0 | u >> 18), o += static_cast<char>(0x80 | ((u >> 12) & 0x3f));
      o += static_cast<char>(0x80 | ((u >> 6) & 0x3f)), o += static_cast<char>(0x80 | (u & 0x3f));
    }
  }
  bool string(std::string* out) {
    ++at;  // the quote
    for (;;) {
      if (at >= t.size()) return fail("a string never closed");
      const char c = t[at++];
      if (c == '"') return true;
      if (c == '\n') return fail("a string never closed on its line");
      if (static_cast<unsigned char>(c) < 0x20) return fail("a control character in a string (write it as \\u00XX)");
      if (c != '\\') {
        *out += c;
        continue;
      }
      if (at >= t.size()) return fail("a string never closed");
      const char e = t[at++];
      switch (e) {
        case '"': *out += '"'; break;
        case '\\': *out += '\\'; break;
        case '/': *out += '/'; break;
        case 'b': *out += '\b'; break;
        case 'f': *out += '\f'; break;
        case 'n': *out += '\n'; break;
        case 'r': *out += '\r'; break;
        case 't': *out += '\t'; break;
        case 'u': {
          uint32_t u;
          if (!hex4(&u)) return false;
          if (u >= 0xd800 && u < 0xdc00) {
            uint32_t lo;
            if (!(word("\\u") && hex4(&lo)) || lo < 0xdc00 || lo >= 0xe000) return fail("half a surrogate pair");
            u = 0x10000 + ((u - 0xd800) << 10) + (lo - 0xdc00);
          }
          put_utf8(*out, u);
          break;
        }
        default:
          return fail(std::string("an unknown escape \\") + e);
      }
    }
  }
  bool number(Value* v) {
    const size_t start = at;
    if (t[at] == '-') ++at;
    if (at >= t.size() || t[at] < '0' || t[at] > '9') return fail("a '-' with no digits");
    while (at < t.size() && t[at] >= '0' && t[at] <= '9') ++at;
    if (at < t.size() && t[at] == '.') {
      ++at;
      if (at >= t.size() || t[at] < '0' || t[at] > '9') return fail("a '.' with no digits after it");
      while (at < t.size() && t[at] >= '0' && t[at] <= '9') ++at;
    }
    if (at < t.size() && (t[at] == 'e' || t[at] == 'E')) {
      ++at;
      if (at < t.size() && (t[at] == '+' || t[at] == '-')) ++at;
      if (at >= t.size() || t[at] < '0' || t[at] > '9') return fail("an exponent with no digits");
      while (at < t.size() && t[at] >= '0' && t[at] <= '9') ++at;
    }
    v->kind = Value::Number;
    v->text = t.substr(start, at - start);
    return true;
  }
  bool value(Value* v) {
    space();
    v->line = line;
    if (at >= t.size()) return fail("the file ends where a value should be");
    const char c = t[at];
    if (c == '"') return v->kind = Value::String, string(&v->text);
    if (c == '-' || (c >= '0' && c <= '9')) return number(v);
    if (word("null")) return v->kind = Value::Null, true;
    if (word("true")) return v->kind = Value::Bool, v->b = true, true;
    if (word("false")) return v->kind = Value::Bool, v->b = false, true;
    if (c != '[' && c != '{') return fail(std::string("'") + c + "' where a value should be");
    if (++depth > kMaxDepth) return fail("nested past 64 deep");
    const bool obj = c == '{';
    const char close = obj ? '}' : ']';
    v->kind = obj ? Value::Object : Value::Array;
    ++at;
    space();
    if (at < t.size() && t[at] == close) return ++at, --depth, true;
    for (;;) {
      space();
      if (obj) {
        if (at >= t.size()) return fail("a '{' never closed");
        if (t[at] != '"') return fail(std::string("'") + t[at] + "' where a member's name should be");
        std::string key;
        if (!string(&key)) return false;
        space();
        if (at >= t.size() || t[at] != ':') return fail("no ':' after the name \"" + key + "\"");
        ++at;
        path.push_back("." + key);
        v->members.emplace_back(std::move(key), Value{});
        if (!value(&v->members.back().second)) return false;
        path.pop_back();
      } else {
        path.push_back("[" + std::to_string(v->items.size()) + "]");
        v->items.emplace_back();
        if (!value(&v->items.back())) return false;
        path.pop_back();
      }
      space();
      if (at >= t.size()) return fail(obj ? "a '{' never closed" : "a '[' never closed");
      if (t[at] == ',') {
        ++at;
        continue;
      }
      if (t[at] == close) return ++at, --depth, true;
      return fail(std::string("'") + t[at] + "' where a ',' or '" + close + "' should be");
    }
  }
};

}  // namespace

const Value* Value::get(const char* key) const {
  for (const auto& [k, v] : members)
    if (k == key) return &v;
  return nullptr;
}

bool parse(const std::string& text, Value* out, std::string* why) {
  Parser p{text, 0, 1, 0, {}, {}};
  *out = Value{};
  bool ok = p.value(out);
  if (ok) {
    p.space();
    if (p.at < text.size()) ok = p.fail("more after the document's value");
  }
  if (!ok && why) *why = p.why;
  return ok;
}

struct Pull::State {
  Parser p;
  std::vector<bool> fresh;  // per open object or list: no member or item taken yet
  std::vector<int> index;   // per open list: the item's index
};

Pull::Pull(const std::string& text) : s_(new State{Parser{text, 0, 1, 0, {}, {}}, {}, {}}) {}
Pull::~Pull() { delete s_; }

bool Pull::begin_object() {
  Parser& p = s_->p;
  p.space();
  if (p.at >= p.t.size() || p.t[p.at] != '{') return p.fail("not an object ({..})");
  ++p.at;
  s_->fresh.push_back(true), p.path.push_back("");
  return true;
}

bool Pull::member(std::string* key) {
  Parser& p = s_->p;
  p.space();
  if (p.at >= p.t.size()) return p.fail("a '{' never closed");
  if (p.t[p.at] == '}') {
    ++p.at;
    s_->fresh.pop_back(), p.path.pop_back();
    return false;
  }
  if (!s_->fresh.back()) {
    if (p.t[p.at] != ',') return p.fail(std::string("'") + p.t[p.at] + "' where a ',' or '}' should be");
    ++p.at;
    p.space();
  }
  s_->fresh.back() = false;
  if (p.at >= p.t.size() || p.t[p.at] != '"') return p.fail("where a member's name should be");
  key->clear();
  if (!p.string(key)) return false;
  p.space();
  if (p.at >= p.t.size() || p.t[p.at] != ':') return p.fail("no ':' after the name \"" + *key + "\"");
  ++p.at;
  p.path.back() = "." + *key;
  return true;
}

bool Pull::begin_list() {
  Parser& p = s_->p;
  p.space();
  if (p.at >= p.t.size() || p.t[p.at] != '[') return p.fail("not a list ([..])");
  ++p.at;
  s_->fresh.push_back(true), s_->index.push_back(-1), p.path.push_back("");
  return true;
}

bool Pull::item() {
  Parser& p = s_->p;
  p.space();
  if (p.at >= p.t.size()) return p.fail("a '[' never closed");
  if (p.t[p.at] == ']') {
    ++p.at;
    s_->fresh.pop_back(), s_->index.pop_back(), p.path.pop_back();
    return false;
  }
  if (!s_->fresh.back()) {
    if (p.t[p.at] != ',') return p.fail(std::string("'") + p.t[p.at] + "' where a ',' or ']' should be");
    ++p.at;
  }
  s_->fresh.back() = false;
  p.path.back() = "[" + std::to_string(++s_->index.back()) + "]";
  return true;
}

bool Pull::value(Value* out) {
  *out = Value{};
  return s_->p.value(out);
}

bool Pull::skip() {
  Value v;
  return s_->p.value(&v);
}

bool Pull::end() {
  Parser& p = s_->p;
  p.space();
  return p.at >= p.t.size() || p.fail("more after the document's value");
}

std::string Pull::path() const {
  std::string r = "$";
  for (const std::string& c : s_->p.path) r += c;
  return r;
}

bool Pull::fail(const std::string& what) { return s_->p.fail(what); }
const std::string& Pull::why() const { return s_->p.why; }

bool Reader::fail(const Value& v, const std::string& path, const std::string& what) {
  if (why.empty()) why = "line " + std::to_string(v.line) + " (at " + path + "): " + what;
  return false;
}

const Value* Reader::member(const Value& obj, const std::string& path, const char* key) {
  if (const Value* m = obj.get(key)) return m;
  fail(obj, path, std::string("no \"") + key + "\"");
  return nullptr;
}

bool Reader::object(const Value& v, const std::string& path) {
  return v.kind == Value::Object || fail(v, path, "not an object ({..})");
}

bool Reader::array(const Value& v, const std::string& path) {
  return v.kind == Value::Array || fail(v, path, "not a list ([..])");
}

bool Reader::f32(const Value& v, const std::string& path, float* out) {
  if (v.kind != Value::Number) return fail(v, path, "not a number");
  const double d = std::strtod(v.text.c_str(), nullptr);
  if (!std::isfinite(d) || std::fabs(d) > FLT_MAX) return fail(v, path, v.text + ": past a float's range");
  *out = std::strtof(v.text.c_str(), nullptr);
  return true;
}

bool Reader::u32(const Value& v, const std::string& path, uint32_t* out, uint32_t most) {
  if (v.kind != Value::Number || v.text.find_first_of(".eE-") != std::string::npos)
    return fail(v, path, "not a whole number");
  const unsigned long long n = v.text.size() > 10 ? ~0ull : std::strtoull(v.text.c_str(), nullptr, 10);
  if (n > most) return fail(v, path, v.text + ": past " + std::to_string(most));
  *out = static_cast<uint32_t>(n);
  return true;
}

bool Reader::game_string(const Value& v, const std::string& path, std::string* out, size_t most) {
  if (v.kind != Value::String) return fail(v, path, "not a string");
  if (!zutl::game_text(v.text, out)) return fail(v, path, "a character the game's text (Windows-1252) cannot hold, or bad UTF-8");
  if (out->size() > most) return fail(v, path, "longer than " + std::to_string(most) + " characters");
  return true;
}

std::string f32_text(float f) {
  char b[48];
  for (int p = 1; p <= 9; ++p) {
    std::snprintf(b, sizeof b, "%.*g", p, static_cast<double>(f));
    if (std::strtof(b, nullptr) == f) break;
  }
  if (!std::strpbrk(b, ".eEn")) std::strcat(b, ".0");
  return b;
}

}  // namespace zutl_json
