#include "frontend/sdl/sdl_mods.h"

#include <SDL3/SDL.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace sdlfe {

bool open_folder(const std::string& dir) {
  if (SDL_getenv("MW3_NO_OPEN_FOLDER")) {  // checks: said, not opened (no window on the machine running them)
    std::printf("mw3: open folder %s\n", dir.c_str());
    return true;
  }
  std::string url = "file://";
  for (const char ch : std::filesystem::absolute(std::filesystem::path(dir)).generic_string()) {
    const unsigned char c = static_cast<unsigned char>(ch);
    if (std::isalnum(c) || std::strchr("/-_.~:", c)) {
      url += ch;
    } else {
      char hex[4];
      std::snprintf(hex, sizeof hex, "%%%02X", c);
      url += hex;
    }
  }
#if defined(_WIN32)
  url.insert(7, "/");  // file:///C:/...
#endif
  return SDL_OpenURL(url.c_str());
}

}  // namespace sdlfe
