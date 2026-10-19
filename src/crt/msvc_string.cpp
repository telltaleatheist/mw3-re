#include "crt/msvc_string.h"

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

double msvc_atof(const char* s) {
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
    }
  }
  return std::strtod(t.c_str(), nullptr);
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
