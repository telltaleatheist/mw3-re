#include "frontend/sdl/sdl_restart.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "frontend/sdl/win_cmdline.h"
#else
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

namespace {
std::wstring wide(const std::string& s) {
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring w(static_cast<size_t>(n > 0 ? n : 0), L'\0');
  if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
  return w;
}
std::string narrow(const std::wstring& w) {
  if (w.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(static_cast<size_t>(n > 0 ? n : 0), '\0');
  if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}
// This program's file, whatever its length (a long path: the buffer grown until the name fits).
std::wstring own_path_wide() {
  std::wstring buf(MAX_PATH, L'\0');
  for (;;) {
    const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (n == 0) return {};
    if (n < buf.size()) return buf.substr(0, n);
    if (buf.size() >= 32768) return {};  // past the longest path Windows has
    buf.assign(buf.size() * 2, L'\0');
  }
}
std::string last_error_words() {
  const DWORD code = GetLastError();
  wchar_t* text = nullptr;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
                 reinterpret_cast<wchar_t*>(&text), 0, nullptr);
  std::string s = text ? narrow(text) : std::string();
  if (text) LocalFree(text);
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '.')) s.pop_back();
  return (s.empty() ? std::string("error") : s) + " (" + std::to_string(code) + ")";
}
}  // namespace

std::string restart_own_path() { return narrow(own_path_wide()); }
// Nothing to list: the next start is given none of this one's handles (CreateProcessW's bInheritHandles FALSE).
std::vector<int> restart_open_handles() { return {}; }
std::string restart_handle_name(int) { return {}; }

// Windows has no exec: the program is started as a new process and this one
// ends. The arguments are quoted as its own splitting reads them
// (win_cmdline.h); the working directory and the environment are this
// process's; no handle is inherited. The new process is made suspended, so
// it cannot reach <write dir>/mw3.log (which it renames to mw3.old.log and
// opens anew) while this one still holds it: with no console stdout and
// stderr are that file (log_to_file), and they are closed here before the
// new process runs a single instruction. The launcher has closed by then
// (it ends at the game's first frame), so nothing waits on this PID.
bool restart_into(const std::string&, const std::vector<std::string>& args, std::string* why) {
  const std::wstring exe = own_path_wide();
  if (exe.empty()) {
    if (why) *why = "the program's own file cannot be told";
    return false;
  }
  std::wstring line = wide(win_command_line(narrow(exe), args));  // CreateProcessW may write into it
  STARTUPINFOW si{};
  si.cb = sizeof si;
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
    if (why) *why = "cannot start " + narrow(exe) + ": " + last_error_words();
    return false;
  }
  std::printf("mw3: starting again as process %lu: %s\n", static_cast<unsigned long>(pi.dwProcessId), narrow(line).c_str());
  std::fflush(nullptr);
  std::fclose(stdout), std::fclose(stderr);    // the log let go before the new start renames it
  AllowSetForegroundWindow(pi.dwProcessId);     // its window may come to the front as ours did
  ResumeThread(pi.hThread);
  CloseHandle(pi.hThread), CloseHandle(pi.hProcess);
  ExitProcess(0);  // the run is over (main has closed it); nothing more of this process is wanted
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
