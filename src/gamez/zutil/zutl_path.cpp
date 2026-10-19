#include "gamez/zutil/zutl_path.h"

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

}  // namespace zutl
