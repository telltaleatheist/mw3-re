#include "frontend/sdl/font_search.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace sdlfe {
namespace fs = std::filesystem;
namespace {

std::string lower(std::string s) {
  for (char& c : s) c = c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c;
  return s;
}
bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }
bool text_face(const std::string& name) {
  for (const char* no : {"emoji", "symbol", "dingbat", "icon", "wingding", "webding", "italic", "oblique"})
    if (has(name, no)) return false;
  return true;
}

}  // namespace

std::vector<std::string> font_folders() {
  std::vector<std::string> out;
  const char* home = std::getenv("HOME");
  if (const char* d = std::getenv("XDG_DATA_HOME"); d && *d) out.push_back(std::string(d) + "/fonts");
  if (home && *home) {
    out.push_back(std::string(home) + "/.local/share/fonts");
    out.push_back(std::string(home) + "/.fonts");
  }
  if (const char* d = std::getenv("XDG_DATA_DIRS"); d && *d) {
    std::stringstream dirs(d);
    for (std::string one; std::getline(dirs, one, ':');)
      if (!one.empty()) out.push_back(one + "/fonts");
  }
  out.push_back("/usr/local/share/fonts");
  out.push_back("/usr/share/fonts");
  return out;
}

std::vector<std::string> scan_fonts(const std::vector<std::string>& roots) {
  constexpr size_t kMost = 20000;  // a tree of links can't hold the start up
  std::vector<std::string> out;
  for (const std::string& root : roots) {
    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end && out.size() < kMost; it.increment(ec)) {
      const std::string ext = lower(it->path().extension().string());
      std::error_code file;
      if ((ext == ".ttf" || ext == ".otf" || ext == ".ttc") && it->is_regular_file(file)) out.push_back(it->path().string());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

bool has_latin(const std::string& file) {
  if (!TTF_Init()) return false;
  TTF_Font* f = TTF_OpenFont(file.c_str(), 12.0f);
  const bool ok = f && TTF_FontHasGlyph(f, 'A') && TTF_FontHasGlyph(f, 'a') && TTF_FontHasGlyph(f, '0');
  if (f) TTF_CloseFont(f);
  TTF_Quit();
  return ok;
}

std::string pick_font(const std::vector<std::string>& files, const std::vector<std::string>& wanted,
                      const std::function<bool(const std::string&)>& usable) {
  for (const std::string& w : wanted)
    for (const std::string& f : files)
      if (lower(fs::path(f).filename().string()) == lower(w)) return f;
  std::string upright;
  for (const std::string& f : files) {
    const std::string name = lower(fs::path(f).filename().string());
    const bool bold = has(name, "bold");
    if (!text_face(name) || (!bold && !upright.empty()) || (usable && !usable(f))) continue;
    if (bold) return f;
    upright = f;
  }
  return upright;
}

}  // namespace sdlfe
