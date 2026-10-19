#include "crt/msvc_string.h"

#include <cstring>

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

}  // namespace crt
