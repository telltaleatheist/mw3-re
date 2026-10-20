#include "gamez/zutil/zutl_path.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace zutl {

namespace {
Mounts g_mounts;

bool is_file(const std::string& p) {
  std::error_code ec;
  return std::filesystem::is_regular_file(p, ec);
}

std::string join(const std::string& dir, const std::string& rel) {
  if (dir.empty()) return rel;
  return dir.back() == '/' ? dir + rel : dir + "/" + rel;
}
std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

// FindFirstFileA's wildcards, without case.
bool match(const char* p, const char* s) {
  for (; *p; ++p, ++s) {
    if (*p == '*') {
      for (const char* t = s;; ++t) {
        if (match(p + 1, t)) return true;
        if (!*t) return false;
      }
    }
    if (!*s) return false;
    if (*p != '?' && std::toupper(static_cast<unsigned char>(*p)) != std::toupper(static_cast<unsigned char>(*s)))
      return false;
  }
  return !*s;
}
}  // namespace

Mounts& mounts() { return g_mounts; }

void mount(const std::string& dir) { g_mounts.read.push_back(dir); }
void mount_front(const std::string& dir) { g_mounts.read.insert(g_mounts.read.begin(), dir); }
void set_write_dir(const std::string& dir) { g_mounts.write = dir; }

std::string host(const char* path) {
  std::string s = path ? path : "";
  for (char& c : s)
    if (c == '\\') c = '/';
  return s;
}

std::string find(const char* path) {
  const std::string h = host(path);
  if (h.empty()) return {};
  if (h[0] == '/' || (g_mounts.read.empty() && g_mounts.write.empty())) return is_file(h) ? h : std::string{};
  if (!g_mounts.write.empty() && is_file(join(g_mounts.write, h))) return join(g_mounts.write, h);  // what the game wrote
  for (const std::string& dir : g_mounts.read) {
    const std::string p = join(dir, h);
    if (is_file(p)) return p;
  }
  return is_file(h) ? h : std::string{};  // relative to the working directory, as before
}

bool exists(const char* path) { return !find(path).empty(); }

std::string write_path(const char* path) {
  const std::string h = host(path);
  if (!h.empty() && h[0] == '/') return h;
  const std::string& dir = !g_mounts.write.empty() ? g_mounts.write : g_mounts.read.empty() ? std::string{} : g_mounts.read[0];
  return join(dir, h);
}

std::FILE* open(const char* path, const char* mode) {
  const std::string m = mode ? mode : "rb";
  if (m.find('w') != std::string::npos || m.find('a') != std::string::npos) {
    const std::string p = write_path(path);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(p).parent_path(), ec);
    return std::fopen(p.c_str(), m.c_str());
  }
  const std::string p = find(path);
  return p.empty() ? nullptr : std::fopen(p.c_str(), m.c_str());
}

std::vector<std::string> list(const char* pattern) {
  const std::string h = host(pattern);
  const size_t slash = h.rfind('/');
  const std::string dir = slash == std::string::npos ? std::string{} : h.substr(0, slash);
  const std::string pat = slash == std::string::npos ? h : h.substr(slash + 1);
  std::vector<std::string> roots;
  if (!g_mounts.write.empty()) roots.push_back(g_mounts.write);
  roots.insert(roots.end(), g_mounts.read.begin(), g_mounts.read.end());
  if (roots.empty()) roots.push_back(".");
  std::vector<std::string> out;
  for (const std::string& r : roots) {
    std::error_code ec;
    for (std::filesystem::directory_iterator it(join(r, dir), ec), end; !ec && it != end; it.increment(ec)) {
      if (!it->is_regular_file(ec)) continue;
      const std::string name = it->path().filename().string();
      if (!match(pat.c_str(), name.c_str())) continue;
      const std::string u = upper(name);
      if (std::none_of(out.begin(), out.end(), [&](const std::string& o) { return upper(o) == u; }))
        out.push_back(name);
    }
  }
  std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return upper(a) < upper(b); });
  return out;
}

}  // namespace zutl
