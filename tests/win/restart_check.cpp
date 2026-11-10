// A Windows check of the restart (frontend/sdl/sdl_restart.cpp's _WIN32 side) without the game:
//
//   restart_check.exe DIR --watch      (DIR must exist)
//
// --watch starts the check and stays until it has ended, then prints both logs and returns 0 when it passed
// (so a remote session that ends with its command sees the whole run). Without --watch the first run returns
// at once, as the game does.
//
// The first run takes DIR\check.log as its stdout and stderr (as the game's log_to_file does), then starts
// itself again through sdlfe::restart_into with arguments that are hard to quote. The second run renames
// DIR\check.log to check.old.log before it opens any file (as the game rotates its log; the rename is refused
// while anything still holds the first run's log, the first run or this one's own inherited standard handles),
// counting its tries, then opens a new check.log and writes whether the rename went through at the first try
// and whether its arguments and working directory are the first run's. DIR\check.log then ends with
// "restart check: OK", or says what differs.
// Built by the mingw cross compiler on the Mac as a compile and link check; run on Windows.
#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>

#include "frontend/sdl/sdl_restart.h"
#include "frontend/sdl/win_cmdline.h"

namespace {
const char* const kHard[] = {"--write-dir", "C:\\my games\\mw 3\\", "--entry", "my mod~night", "say \"hi\"", "", "a\\\"b", "tab\there", "-nofmv"};

std::string cwd() {
  wchar_t buf[32768];
  const DWORD n = GetCurrentDirectoryW(32768, buf);
  std::string s;
  for (DWORD i = 0; i < n; ++i) s += buf[i] < 128 ? static_cast<char>(buf[i]) : '?';
  return s;
}

std::string text_of(const std::string& path) {
  std::string s;
  if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
    char buf[4096];
    for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) s.append(buf, n);
    std::fclose(f);
  }
  return s;
}

// --watch: the check as a child, waited for to its last line (30 s at most), both logs printed.
int watch(const std::string& dir) {
  const std::string log = dir + "\\check.log", old = dir + "\\check.old.log";
  DeleteFileA(log.c_str()), DeleteFileA(old.c_str());
  std::string line = sdlfe::win_command_line(sdlfe::restart_own_path(), {dir});
  STARTUPINFOA si{};
  si.cb = sizeof si;
  PROCESS_INFORMATION pi{};
  if (!CreateProcessA(nullptr, line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
    std::printf("restart check: could not start the first run (error %lu)\n", static_cast<unsigned long>(GetLastError()));
    return 1;
  }
  WaitForSingleObject(pi.hProcess, 30000);
  CloseHandle(pi.hThread), CloseHandle(pi.hProcess);
  std::string now;
  for (int i = 0; i < 300; ++i) {
    now = text_of(log);
    if (now.find("restart check: OK") != std::string::npos || now.find("restart check: NOT OK") != std::string::npos ||
        now.find("restart check: FAILED") != std::string::npos)
      break;
    Sleep(100);
  }
  std::printf("---- check.old.log (the first run)\n%s---- check.log (the second run)\n%s", text_of(old).c_str(), now.c_str());
  const bool ok = now.find("restart check: OK") != std::string::npos;
  std::printf("---- %s\n", ok ? "passed" : "did not pass");
  return ok ? 0 : 1;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: restart_check DIR [--watch]\n");
    return 2;
  }
  const std::string dir = argv[1], log = dir + "\\check.log", old = dir + "\\check.old.log";
  bool second = false;
  for (int i = 2; i < argc; ++i) second = second || std::string(argv[i]) == "--restarted";
  if (argc == 3 && std::string(argv[2]) == "--watch") return watch(dir);
  // The second run rotates the log first, before it opens anything: at most 40 tries, 50 ms apart.
  int tries = 0;
  DWORD rotate_error = 0;
  bool rotated = !second;
  if (second) {
    DeleteFileA(old.c_str());
    for (tries = 1; tries <= 40; ++tries) {
      if (MoveFileA(log.c_str(), old.c_str())) {
        rotated = true;
        break;
      }
      rotate_error = GetLastError();
      Sleep(50);
    }
  }
  if (!std::freopen(log.c_str(), "w", stdout) || !std::freopen(log.c_str(), "a", stderr)) return 3;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::printf("restart check: process %lu, %s run\n own path %s\n working directory %s\n", static_cast<unsigned long>(GetCurrentProcessId()),
              second ? "second" : "first", sdlfe::restart_own_path().c_str(), cwd().c_str());
  for (int i = 0; i < argc; ++i) std::printf(" argv[%d] = [%s]\n", i, argv[i]);
  if (!second) {
    std::vector<std::string> args = {dir};
    for (const char* h : kHard) args.emplace_back(h);
    args.push_back("--cwd"), args.push_back(cwd());
    args.push_back("--restarted");
    std::string why;
    sdlfe::restart_into(argv[0], args, &why);
    std::printf("restart check: FAILED to start again: %s\n", why.c_str());
    return 1;
  }
  // The second run: argv[1] DIR, the hard ones, --cwd <the first run's>, --restarted.
  const int hard = static_cast<int>(sizeof kHard / sizeof kHard[0]);
  if (rotated) std::printf("restart check: the first run's log renamed at try %d\n", tries);
  else std::printf("restart check: the first run's log was still held after %d tries (rename error %lu)\n", tries - 1, static_cast<unsigned long>(rotate_error));
  bool ok = rotated && tries == 1 && argc == 2 + hard + 3;
  if (rotated && tries != 1) std::printf("restart check: the log was held for a while: it must be free at the first try\n");
  if (argc != 2 + hard + 3) std::printf("restart check: %d arguments, expected %d\n", argc, 2 + hard + 3);
  for (int i = 0; ok && i < hard; ++i)
    if (std::string(argv[2 + i]) != kHard[i]) ok = false, std::printf("restart check: argument %d is [%s], sent [%s]\n", 2 + i, argv[2 + i], kHard[i]);
  if (ok && cwd() != argv[2 + hard + 1]) ok = false, std::printf("restart check: working directory %s, the first run's was %s\n", cwd().c_str(), argv[2 + hard + 1]);
  std::printf(ok ? "restart check: OK\n" : "restart check: NOT OK\n");
  return ok ? 0 : 1;
}
