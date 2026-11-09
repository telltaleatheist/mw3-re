// Ours (docs/modding.md, Readable files read directly): a reader entry as
// mech3ax's JSON (unzbd's `<entry>.zrd.json`), the tree one to one, and back.
// An array is a list, a string a string, a number with '.' or an exponent a
// float, one without an int; null is an empty list (unzbd writes one for a
// list of no items). unzbd writes each float widened to a double exactly, so
// the double's nearest float is the entry's own bits.
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_path.h"

namespace zrdr {
namespace {

constexpr int kMaxDepth = 64;  // as the binary reader's bound (zreader.cpp)

struct Parser {
  const std::string& t;
  size_t at = 0;
  int line = 1;
  std::vector<int> path;  // the index at each depth, for the error
  std::string why;

  bool fail(const std::string& what) {
    if (!why.empty()) return false;
    std::string p = "$";
    for (int i : path) p += "[" + std::to_string(i) + "]";
    why = "line " + std::to_string(line) + " (at " + p + "): " + what;
    return false;
  }
  void space() {
    while (at < t.size() && (t[at] == ' ' || t[at] == '\t' || t[at] == '\r' || t[at] == '\n')) {
      if (t[at] == '\n') ++line;
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
  bool string(Node* n) {
    ++at;  // the quote
    std::string utf8;
    for (;;) {
      if (at >= t.size()) return fail("a string never closed");
      const char c = t[at++];
      if (c == '"') break;
      if (c == '\n') return fail("a string never closed on its line");
      if (static_cast<unsigned char>(c) < 0x20) return fail("a control character in a string (write it as \\u00XX)");
      if (c != '\\') {
        utf8 += c;
        continue;
      }
      if (at >= t.size()) return fail("a string never closed");
      const char e = t[at++];
      switch (e) {
        case '"': utf8 += '"'; break;
        case '\\': utf8 += '\\'; break;
        case '/': utf8 += '/'; break;
        case 'b': utf8 += '\b'; break;
        case 'f': utf8 += '\f'; break;
        case 'n': utf8 += '\n'; break;
        case 'r': utf8 += '\r'; break;
        case 't': utf8 += '\t'; break;
        case 'u': {
          uint32_t u;
          if (!hex4(&u)) return false;
          if (u >= 0xd800 && u < 0xdc00) {  // a surrogate pair
            uint32_t lo;
            if (!(word("\\u") && hex4(&lo)) || lo < 0xdc00 || lo >= 0xe000) return fail("half a surrogate pair");
            u = 0x10000 + ((u - 0xd800) << 10) + (lo - 0xdc00);
          }
          put_utf8(utf8, u);
          break;
        }
        default:
          return fail(std::string("an unknown escape \\") + e);
      }
    }
    std::string game;
    if (!zutl::game_text(utf8, &game)) return fail("a character the game's text (Windows-1252) cannot hold, or bad UTF-8");
    n->type = String;
    n->s = static_cast<char*>(std::malloc(game.size() + 1));
    std::memcpy(n->s, game.c_str(), game.size() + 1);
    return true;
  }
  bool number(Node* n) {
    const size_t start = at;
    bool real = false;
    if (t[at] == '-') ++at;
    if (at >= t.size() || t[at] < '0' || t[at] > '9') return fail("a '-' with no digits");
    while (at < t.size() && t[at] >= '0' && t[at] <= '9') ++at;
    if (at < t.size() && t[at] == '.') {
      real = true, ++at;
      if (at >= t.size() || t[at] < '0' || t[at] > '9') return fail("a '.' with no digits after it");
      while (at < t.size() && t[at] >= '0' && t[at] <= '9') ++at;
    }
    if (at < t.size() && (t[at] == 'e' || t[at] == 'E')) {
      real = true, ++at;
      if (at < t.size() && (t[at] == '+' || t[at] == '-')) ++at;
      if (at >= t.size() || t[at] < '0' || t[at] > '9') return fail("an exponent with no digits");
      while (at < t.size() && t[at] >= '0' && t[at] <= '9') ++at;
    }
    const std::string text = t.substr(start, at - start);
    if (real) {
      const double d = std::strtod(text.c_str(), nullptr);
      if (!std::isfinite(d) || std::fabs(d) > FLT_MAX) return fail(text + ": past a float's range");
      n->type = Float;
      n->f = static_cast<float>(d);
      return true;
    }
    const long long v = std::strtoll(text.c_str(), nullptr, 10);
    if (text.size() > 11 || v < INT32_MIN || v > INT32_MAX) return fail(text + ": past the game's 32-bit integers");
    n->type = Int;
    n->i = static_cast<int32_t>(v);
    return true;
  }
  static void set_list(Node* n, std::vector<Node>& items) {
    const int32_t count = static_cast<int32_t>(items.size()) + 1;
    n->type = List;
    n->list = alloc_list(count);
    n->list[0].type = Int;
    n->list[0].i = count;
    for (size_t i = 0; i < items.size(); ++i) n->list[i + 1] = items[i];
  }
  bool value(Node* n) {
    space();
    if (at >= t.size()) return fail("the file ends where a value should be");
    const char c = t[at];
    if (c == '[') return array(n);
    if (c == '"') return string(n);
    if (c == '-' || (c >= '0' && c <= '9')) return number(n);
    if (word("null")) {
      std::vector<Node> none;
      set_list(n, none);
      return true;
    }
    if (word("true") || word("false")) return fail("true or false: the reader has none (write 1 or 0)");
    if (c == '{') return fail("an object: the reader's JSON has only lists [..], strings, numbers and null");
    return fail(std::string("'") + c + "' where a value should be");
  }
  bool array(Node* n) {
    if (static_cast<int>(path.size()) >= kMaxDepth) return fail("lists nested past 64 deep");
    ++at;  // '['
    std::vector<Node> items;
    auto drop = [&] {
      for (Node& i : items) free_contents(&i);
      return false;
    };
    space();
    if (at < t.size() && t[at] == ']') {
      ++at;
      set_list(n, items);
      return true;
    }
    for (;;) {
      path.push_back(static_cast<int>(items.size()));
      Node item{};
      item.type = Int, item.i = 0;
      const bool ok = value(&item);
      path.pop_back();
      if (!ok) {
        free_contents(&item);
        return drop();
      }
      items.push_back(item);
      space();
      if (at >= t.size()) return fail("a '[' never closed"), drop();
      if (t[at] == ',') {
        ++at;
        continue;
      }
      if (t[at] == ']') {
        ++at;
        break;
      }
      return fail(std::string("'") + t[at] + "' where a ',' or ']' should be"), drop();
    }
    set_list(n, items);
    return true;
  }
};

void indent(std::string& o, int depth) { o.append(static_cast<size_t>(depth) * 2, ' '); }

bool write(const Node& n, std::string& o, int depth) {
  char b[48];
  switch (n.type) {
    case Int:
      o += std::to_string(n.i);
      return true;
    case Float: {
      if (!std::isfinite(n.f)) return false;
      // The shortest text that reads back as this float, always with a '.'
      // or an exponent so it reads as a float.
      for (int p = 1; p <= 9; ++p) {
        std::snprintf(b, sizeof b, "%.*g", p, static_cast<double>(n.f));
        if (static_cast<float>(std::strtod(b, nullptr)) == n.f) break;
      }
      if (!std::strpbrk(b, ".eE")) std::strcat(b, ".0");
      o += b;
      return true;
    }
    case String: {
      o += '"';
      for (unsigned char c : zutl::utf8_text(n.s ? n.s : "")) {
        if (c == '"' || c == '\\') o += '\\', o += static_cast<char>(c);
        else if (c < 0x20) std::snprintf(b, sizeof b, "\\u%04x", c), o += b;
        else o += static_cast<char>(c);
      }
      o += '"';
      return true;
    }
    case List: {
      const int32_t count = n.list ? n.list[0].i : 1;
      if (count <= 1) {
        o += "null";
        return true;
      }
      o += "[\n";
      for (int32_t i = 1; i < count; ++i) {
        indent(o, depth + 1);
        if (!write(n.list[i], o, depth + 1)) return false;
        o += i + 1 < count ? ",\n" : "\n";
      }
      indent(o, depth);
      o += ']';
      return true;
    }
    default:
      return false;
  }
}

}  // namespace

Node* parse_json(const std::string& text, std::string* why) {
  Parser p{text, 0, 1, {}, {}};
  Node* root = new_nodes(Int, 1);
  root->i = 0;
  p.space();
  bool ok = p.at < text.size() && (text[p.at] == '[' || text.compare(p.at, 4, "null") == 0)
                ? p.value(root)
                : p.fail("the entry is not a list ('[' or null)");
  if (ok) {
    p.space();
    if (p.at < text.size()) ok = p.fail("more after the entry's list");
  }
  if (!ok) {
    free_node(root);
    if (why) *why = p.why;
    return nullptr;
  }
  if (why) why->clear();
  return root;
}

bool to_json(const Node& root, std::string* out) {
  out->clear();
  if (!write(root, *out, 0)) return false;
  *out += '\n';
  return true;
}

}  // namespace zrdr
