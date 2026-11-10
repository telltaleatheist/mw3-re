#include "frontend/sdl/sdl_restart.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace sdlfe {

namespace {
RestartTo g_to;
bool g_asked = false;

// 0 not ours, 1 a flag of ours alone, 2 one with its value.
int ours(const std::string& s) {
  if (s == "--entry" || s == "--mod") return 2;
  if (s == "--entry-play" || s == "--entry-ia" || s == "--open-mods" || s == "--restarted" || s == "--no-mods") return 1;
  return 0;
}
}  // namespace

std::vector<std::string> restart_args(const std::vector<std::string>& args, const RestartTo& to) {
  std::vector<std::string> out;
  for (size_t i = 0; i < args.size(); ++i) {
    const int n = ours(args[i]);
    if (n == 2) ++i;
    if (n == 0) out.push_back(args[i]);
  }
  if (!to.entry.empty()) {
    out.push_back("--entry"), out.push_back(to.entry);
    if (to.screen == RestartTo::kPlay) out.push_back("--entry-play");
    if (to.screen == RestartTo::kInstantAction) out.push_back("--entry-ia");
  } else if (to.open_mods) {
    out.push_back("--open-mods");
  }
  out.push_back("--restarted");
  return out;
}

void restart_ask(const RestartTo& to) { g_to = to, g_asked = true; }
bool restart_asked(RestartTo* to) {
  if (!g_asked) return false;
  if (to) *to = g_to;
  g_asked = false;
  return true;
}

#if defined(_WIN32)

// The Windows side (CreateProcessW of our own path, the arguments quoted, then
// this process's end) is not written yet: the start is refused, said.
std::string restart_own_path() { return {}; }
std::vector<int> restart_open_handles() { return {}; }
std::string restart_handle_name(int) { return {}; }
bool restart_into(const std::string&, const std::vector<std::string>&, std::string* why) {
  if (why) *why = "starting again is not there yet on Windows";
  return false;
}

#else

std::string restart_own_path() {
  char buf[4096];
#if defined(__APPLE__)
  uint32_t size = sizeof buf;
  if (_NSGetExecutablePath(buf, &size) != 0) return {};
  return buf;
#else
  const ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
  if (n <= 0) return {};
  buf[n] = 0;
  return buf;
#endif
}

std::vector<int> restart_open_handles() {
  std::vector<int> out;
  const int top = getdtablesize();
  for (int fd = 3; fd < top; ++fd)
    if (fcntl(fd, F_GETFD) != -1) out.push_back(fd);
  return out;
}

std::string restart_handle_name(int fd) {
  char buf[4096] = "";
#if defined(__APPLE__)
  if (fcntl(fd, F_GETPATH, buf) == -1) buf[0] = 0;
#else
  const std::string link = "/proc/self/fd/" + std::to_string(fd);
  const ssize_t n = readlink(link.c_str(), buf, sizeof buf - 1);
  buf[n > 0 ? n : 0] = 0;
#endif
  return buf[0] ? buf : "(no file: a pipe, socket or queue)";
}

bool restart_into(const std::string& argv0, const std::vector<std::string>& args, std::string* why) {
  const std::string exe = restart_own_path();
  if (exe.empty()) {
    if (why) *why = "the program's own file cannot be told";
    return false;
  }
  // Nothing of this run reaches the next: whatever is still open (a library's
  // own handle; ours are closed by now) closes at the exec.
  for (int fd : restart_open_handles()) fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
  std::vector<std::string> all;
  all.push_back(argv0.empty() ? exe : argv0);
  all.insert(all.end(), args.begin(), args.end());
  std::vector<char*> argv;
  for (std::string& s : all) argv.push_back(s.data());
  argv.push_back(nullptr);
  std::fflush(nullptr);  // every stream still open (an exec, unlike an exit, writes none of them out)
  execv(exe.c_str(), argv.data());
  if (why) *why = std::string("cannot start ") + exe + ": " + std::strerror(errno);
  return false;
}

#endif

}  // namespace sdlfe
