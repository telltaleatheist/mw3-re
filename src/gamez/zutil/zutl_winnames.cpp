// Ours, Windows only: names as UTF-8 in the narrow calls (docs/program.md, Names
// on Windows). tools/windows/utf8.manifest makes UTF-8 the programs' ANSI code
// page, which also moves what earlier builds wrote in the real one.
#include "gamez/zutil/zutl_path.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace zutl {

namespace {

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

// What an earlier build meant by this name: it passed UTF-8 to calls that read
// the system's code page, so "é" reached the disk as "Ã©". Empty when the name
// is not such a one.
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

}  // namespace

bool narrow_paths_utf8() { return GetACP() == CP_UTF8 && !std::getenv("MW3_OLD_WINDOWS"); }

std::string path_from_config(const std::string& s) {
  if (!narrow_paths_utf8() || valid_utf8(s)) return s;
  return to_utf8(to_wide(system_code_page(), s));
}

// Renames only, and never over a file; a name that cannot be mapped stays.
void repair_old_names(const std::string& write_dir) {
  if (!narrow_paths_utf8() || write_dir.empty()) return;
  const UINT cp = system_code_page();
  if (cp == CP_UTF8) return;  // nothing to undo: earlier builds wrote UTF-8 there too
  std::error_code ec;
  for (const char* folder : {"pilots", "configs", "keys", "export", "import"}) {
    const fs::path root = fs::path(write_dir) / folder;
    if (!fs::is_directory(root, ec)) continue;
    std::vector<fs::path> paths;
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) paths.push_back(it->path());
    for (auto p = paths.rbegin(); p != paths.rend(); ++p) {  // files before their folders
      const std::wstring meant = meant_by(p->filename().native(), cp);
      if (meant.empty()) continue;
      const fs::path to = p->parent_path() / meant;
      if (fs::exists(to, ec)) continue;
      fs::rename(*p, to, ec);
      std::printf("mw3: an old name put right: %s\n", to_utf8(to.native()).c_str());
    }
  }
}

}  // namespace zutl

#endif
