#include "crt/msvc_string.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace crt {

int msvc_stricmp(const char* a, const char* b) {
  for (;; ++a, ++b) {
    unsigned char ca = static_cast<unsigned char>(*a), cb = static_cast<unsigned char>(*b);
    if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
    if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
    if (ca != cb) return ca < cb ? -1 : 1;
    if (ca == 0) return 0;
  }
}

void msvc_splitpath_name(const char* path, char* fname, char* ext) {
  const char* name = path;
  for (const char* p = path; *p; ++p)
    if (*p == '\\' || *p == '/' || *p == ':') name = p + 1;
  const char* dot = std::strrchr(name, '.');
  size_t len = dot ? static_cast<size_t>(dot - name) : std::strlen(name);
  if (len > 255) len = 255;
  std::memcpy(fname, name, len);
  fname[len] = '\0';
  if (dot) {
    std::strncpy(ext, dot, 255);
    ext[255] = '\0';
  } else {
    ext[0] = '\0';
  }
}

namespace {
bool c_space(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
bool digit(char c) { return c >= '0' && c <= '9'; }
}  // namespace

double msvc_strtod(const char* s, const char** end) {
  const char* const start = s;
  if (end) *end = start;
  while (c_space(*s)) ++s;
  std::string t;
  if (*s == '+' || *s == '-') t += *s++;
  bool any = false;
  while (digit(*s)) t += *s++, any = true;
  if (*s == '.') {
    t += *s++;
    while (digit(*s)) t += *s++, any = true;
  }
  if (!any) return 0.0;
  if (*s == 'e' || *s == 'E' || *s == 'd' || *s == 'D') {
    const char* e = s + 1;
    std::string x = "e";
    if (*e == '+' || *e == '-') x += *e++;
    if (digit(*e)) {
      while (digit(*e)) x += *e++;
      t += x;
      s = e;
    }
  }
  if (end) *end = s;
  return std::strtod(t.c_str(), nullptr);
}

double msvc_atof(const char* s) { return msvc_strtod(s, nullptr); }

int32_t msvc_strtol(const char* s, const char** end) {
  const char* const start = s;
  while (c_space(*s)) ++s;
  const char sign = *s;
  if (sign == '+' || sign == '-') ++s;
  if (!digit(*s)) {
    if (end) *end = start;
    return 0;
  }
  uint64_t v = 0;
  bool over = false;
  while (digit(*s)) {
    v = v * 10 + static_cast<uint64_t>(*s++ - '0');
    if (v > 0x80000000ull) over = true, v = 0x80000000ull;
  }
  if (end) *end = s;
  if (sign == '-') return over || v > 0x80000000ull ? INT32_MIN : static_cast<int32_t>(0u - static_cast<uint32_t>(v));
  return over || v > 0x7fffffffull ? INT32_MAX : static_cast<int32_t>(v);
}

int msvc_scan_hex(const char* s, uint32_t* out) {
  while (c_space(*s)) ++s;
  bool any = false;
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2, any = true;
  uint32_t v = 0;
  for (;; ++s) {
    const char c = *s;
    const int d = digit(c) ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    if (d < 0) break;
    v = v << 4 | static_cast<uint32_t>(d);
    any = true;
  }
  if (!any) return 0;
  *out = v;
  return 1;
}

int msvc_scan_dec(const char* s, int32_t* out) {
  while (c_space(*s)) ++s;
  const char sign = *s;
  if (sign == '+' || sign == '-') ++s;
  if (!digit(*s)) return 0;
  uint32_t v = 0;
  while (digit(*s)) v = v * 10 + static_cast<uint32_t>(*s++ - '0');
  *out = static_cast<int32_t>(sign == '-' ? 0u - v : v);
  return 1;
}

int32_t msvc_atoi(const char* s) {
  while (c_space(*s)) ++s;
  const char sign = *s;
  if (sign == '+' || sign == '-') ++s;
  uint32_t v = 0;
  while (digit(*s)) v = v * 10 + static_cast<uint32_t>(*s++ - '0');
  return static_cast<int32_t>(sign == '-' ? 0u - v : v);
}

bool msvc_iswspace_char(char c) { return c_space(c); }

}  // namespace crt
