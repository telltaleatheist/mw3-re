// The launcher's own small copies (launcher split b1); launcher_own.h.
#include "frontend/sdl/launcher_own.h"

#include "rekit/game_folder.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace sdlfe::own {

namespace {

// mw3.cfg's lines, each with its own ending, byte for byte (a CRLF file stays CRLF, a mixed one mixed), and the
// ending a new line takes: the file's first, else the system's.
struct CfgLines {
  std::vector<std::string> lines;  // endings included; the last may have none
#if defined(_WIN32)
  std::string eol = "\r\n";
#else
  std::string eol = "\n";
#endif
};
CfgLines read_cfg_lines(const fs::path& cfg) {
  CfgLines c;
  std::ifstream f(cfg, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  const std::string text = ss.str();
  bool seen = false;
  for (size_t at = 0; at < text.size();) {
    const size_t nl = text.find('\n', at);
    const size_t end = nl == std::string::npos ? text.size() : nl + 1;
    c.lines.push_back(text.substr(at, end - at));
    if (!seen && nl != std::string::npos) seen = true, c.eol = nl > at && text[nl - 1] == '\r' ? "\r\n" : "\n";
    at = end;
  }
  return c;
}
bool write_cfg_lines(const fs::path& cfg, const CfgLines& c) {
  std::ofstream o(cfg, std::ios::binary | std::ios::trunc);
  for (const std::string& l : c.lines) o << l;
  return static_cast<bool>(o);
}
bool is_key(const std::string& line, const std::string& key) { return line.rfind(key + "=", 0) == 0; }

std::string join(const std::string& dir, const std::string& rel) {
  if (dir.empty()) return rel;
  return dir.back() == '/' ? dir + rel : dir + "/" + rel;
}
bool same_letters(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
  return true;
}

#if defined(_WIN32)
// The system's own code page: GetACP says 65001 under the manifest.
UINT system_code_page() {
  wchar_t buf[16] = {};
  DWORD size = sizeof buf;
  if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Nls\\CodePage", L"ACP", RRF_RT_REG_SZ,
                   nullptr, buf, &size) == ERROR_SUCCESS) {
    const unsigned long cp = std::wcstoul(buf, nullptr, 10);
    if (cp != 0) return static_cast<UINT>(cp);
  }
  return 1252;
}
bool valid_utf8(const std::string& s) {
  return s.empty() ||
         MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0) > 0;
}
std::wstring to_wide(UINT cp, const std::string& s) {
  std::wstring w(static_cast<size_t>(MultiByteToWideChar(cp, 0, s.data(), static_cast<int>(s.size()), nullptr, 0)), L'\0');
  if (!w.empty()) MultiByteToWideChar(cp, 0, s.data(), static_cast<int>(s.size()), w.data(), static_cast<int>(w.size()));
  return w;
}
std::string to_utf8(const std::wstring& w) {
  std::string s(static_cast<size_t>(WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                                                        nullptr, nullptr)), '\0');
  if (!s.empty())
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), static_cast<int>(s.size()), nullptr,
                        nullptr);
  return s;
}
// What an earlier build meant by this name: it passed UTF-8 to calls that read the system's code page, so "é" reached
// the disk as "Ã©". Empty when the name is not such a one.
std::wstring meant_by(const std::wstring& name, UINT cp) {
  BOOL lost = FALSE;
  const int n = WideCharToMultiByte(cp, WC_NO_BEST_FIT_CHARS, name.data(), static_cast<int>(name.size()), nullptr, 0,
                                    nullptr, &lost);
  if (n <= 0 || lost) return {};
  std::string bytes(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(cp, WC_NO_BEST_FIT_CHARS, name.data(), static_cast<int>(name.size()), bytes.data(), n, nullptr,
                      &lost);
  bool plain = true;
  for (const char c : bytes) plain = plain && static_cast<unsigned char>(c) < 0x80;
  if (plain || lost || !valid_utf8(bytes)) return {};
  return to_wide(CP_UTF8, bytes);
}
#endif

}  // namespace

std::string media_dir(const std::string& install, const std::string& write_dir) {
  if (install.empty()) return write_dir;
  std::error_code ec;
  const fs::path up = fs::path(install).parent_path();
  return fs::is_regular_file(up / rekit::kGameFolderMark, ec) ? up.string() : write_dir;
}

bool save_config_value(const std::string& write_dir, const std::string& key, const std::string& value) {
  if (write_dir.empty()) return false;
  const fs::path cfg = fs::path(write_dir) / "mw3.cfg";
  CfgLines c = read_cfg_lines(cfg);
  c.lines.erase(std::remove_if(c.lines.begin(), c.lines.end(), [&](const std::string& l) { return is_key(l, key); }), c.lines.end());
  // A last line with no ending gets the file's, so the next one starts its own.
  if (!c.lines.empty() && c.lines.back().back() != '\n') c.lines.back() += c.eol;
  c.lines.insert(c.lines.begin(), key + "=" + value + c.eol);
  return write_cfg_lines(cfg, c);
}

bool save_install(const std::string& write_dir, const std::string& install) {
  // Absolute: a relative folder means nothing to the app, which starts in another directory.
  std::error_code ec;
  const fs::path abs = install.empty() ? fs::path() : fs::absolute(install, ec);
  return save_config_value(write_dir, "install", ec || abs.empty() ? install : abs.lexically_normal().string());
}

uint32_t crc32(uint32_t crc, const void* data, size_t n) {
  static const auto table = [] {
    struct T {
      uint32_t v[256];
    } t{};
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t v = i << 24;
      for (int b = 0; b < 8; ++b) v = (v & 0x80000000u) ? (v << 1) ^ 0x04c11db7u : v << 1;
      t.v[i] = v;
    }
    return t;
  }();
  const auto* p = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < n; ++i) crc = table.v[(crc >> 24) ^ p[i]] ^ (crc << 8);
  return crc;
}

std::string any_case(const std::string& root, const std::string& rel) {
  std::string at = root;
  std::error_code ec;
  // The root by its letters too ("<install>/zbd").
  if (!at.empty() && !fs::exists(at, ec)) {
    const size_t slash = at.rfind('/');
    if (slash == std::string::npos || slash == 0) return {};
    at = any_case(at.substr(0, slash), at.substr(slash + 1));
    if (at.empty()) return {};
  }
  for (size_t i = 0; i < rel.size();) {
    const size_t end = std::min(rel.find('/', i), rel.size());
    const std::string part = rel.substr(i, end - i);
    i = end + 1;
    if (part.empty()) continue;
    std::string next = join(at, part);
    if (!fs::exists(next, ec)) {
      next.clear();
      for (fs::directory_iterator it(at.empty() ? "." : at, ec), last; !ec && it != last; it.increment(ec))
        if (same_letters(it->path().filename().string(), part)) {
          next = join(at, it->path().filename().string());
          break;
        }
      if (next.empty()) return {};
    }
    at = next;
  }
  return fs::exists(at, ec) ? at : std::string{};
}

#if defined(_WIN32)
bool narrow_paths_utf8() { return GetACP() == CP_UTF8 && !std::getenv("MW3_OLD_WINDOWS"); }
std::string path_from_config(const std::string& s) {
  if (!narrow_paths_utf8() || valid_utf8(s)) return s;
  return to_utf8(to_wide(system_code_page(), s));
}
// Once per settings folder (its mark stays in it). Only our own folders inside it, not following a link or junction
// out; renames only, each logged, never over a file; a name that cannot be mapped stays.
void repair_old_names(const std::string& write_dir) {
  if (!narrow_paths_utf8() || write_dir.empty()) return;
  const UINT cp = system_code_page();
  if (cp == CP_UTF8) return;  // nothing to undo: earlier builds wrote UTF-8 there too
  std::error_code ec;
  const fs::path mark = fs::path(write_dir) / L"names-utf8.done";
  if (fs::exists(mark, ec)) return;
  for (const char* folder : {"pilots", "configs", "keys", "export", "import"}) {
    const fs::path root = fs::path(write_dir) / folder;
    if (fs::symlink_status(root, ec).type() != fs::file_type::directory) continue;
    std::vector<fs::path> paths;
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) paths.push_back(it->path());
    ec.clear();
    for (auto p = paths.rbegin(); p != paths.rend(); ++p) {  // files before their folders
      const std::wstring meant = meant_by(p->filename().native(), cp);
      if (meant.empty()) continue;
      const fs::path to = p->parent_path() / meant;
      if (fs::exists(to, ec)) {
        std::printf("mw3: an old name left alone, the right one is taken: %s\n", to_utf8(p->native()).c_str());
        continue;
      }
      fs::rename(*p, to, ec);
      std::printf(ec ? "mw3: an old name could not be put right: %s\n" : "mw3: an old name put right: %s\n",
                  to_utf8(ec ? p->native() : to.native()).c_str());
    }
  }
  if (std::FILE* f = _wfopen(mark.c_str(), L"wb")) std::fclose(f);
}
#else
bool narrow_paths_utf8() { return true; }
std::string path_from_config(const std::string& s) { return s; }
void repair_old_names(const std::string&) {}
#endif

std::string game_words(const std::string& v, bool pirates_moon) {
  if (pirates_moon) return v.empty() ? "MechWarrior 3: Pirate's Moon" : "MechWarrior 3: Pirate's Moon " + v;
  if (v == "1.2" || v == "1.0") return "MechWarrior 3 " + v;
  if (v == "1.0 with 1.2 files") return "MechWarrior 3 (partly patched to 1.2)";
  return "MechWarrior 3 (a version we don't know, perhaps modded)";
}

std::string found_words(const std::string& version, const std::string& dir, bool pirates_moon) {
  return "Found " + game_words(pirates_moon ? std::string() : version, pirates_moon) + " at " + dir + ".";
}

std::string start_found_words(const std::string& dir, bool pirates_moon) {
  return "Found " + (pirates_moon ? game_words("", true) : std::string("MechWarrior 3")) + " at " + dir + ".";
}

}  // namespace sdlfe::own
