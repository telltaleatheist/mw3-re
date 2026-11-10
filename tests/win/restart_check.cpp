// A Windows check of the restart (frontend/sdl/sdl_restart.cpp's _WIN32 side) without the game:
//
//   restart_check.exe DIR
//
// The first run takes DIR\check.log as its stdout and stderr (as the game's log_to_file does), then starts
// itself again through sdlfe::restart_into with arguments that are hard to quote. The second run renames
// DIR\check.log to check.old.log (as the game rotates its log: this fails while the first run still holds
// the file), opens a new one, and writes whether its arguments, its working directory and its own path are
// the first run's. DIR\check.log then ends with "restart check: OK", or says what differs.
// Built by the mingw cross compiler on the Mac as a compile and link check; run on Windows.
#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>

#include "frontend/sdl/sdl_restart.h"

namespace {
const char* const kHard[] = {"--write-dir", "C:\\my games\\mw 3\\", "--entry", "my mod~night", "say \"hi\"", "", "a\\\"b", "tab\there", "-nofmv"};

std::string cwd() {
  wchar_t buf[32768];
  const DWORD n = GetCurrentDirectoryW(32768, buf);
  std::string s;
  for (DWORD i = 0; i < n; ++i) s += buf[i] < 128 ? static_cast<char>(buf[i]) : '?';
  return s;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: restart_check DIR\n");
    return 2;
  }
  const std::string dir = argv[1], log = dir + "\\check.log", old = dir + "\\check.old.log";
  bool second = false;
  for (int i = 2; i < argc; ++i) second = second || std::string(argv[i]) == "--restarted";
  bool rotated = true;
  if (second) {
    DeleteFileA(old.c_str());
    rotated = MoveFileA(log.c_str(), old.c_str()) != 0;  // fails while the first run holds its log
  }
  const DWORD rotate_error = rotated ? 0 : GetLastError();
  if (!std::freopen(log.c_str(), "w", stdout) || !std::freopen(log.c_str(), "a", stderr)) return 3;
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
  bool ok = rotated && argc == 2 + hard + 3;
  if (!rotated) std::printf("restart check: the first run still held check.log (rename error %lu)\n", static_cast<unsigned long>(rotate_error));
  if (argc != 2 + hard + 3) std::printf("restart check: %d arguments, expected %d\n", argc, 2 + hard + 3);
  for (int i = 0; ok && i < hard; ++i)
    if (std::string(argv[2 + i]) != kHard[i]) ok = false, std::printf("restart check: argument %d is [%s], sent [%s]\n", 2 + i, argv[2 + i], kHard[i]);
  if (ok && cwd() != argv[2 + hard + 1]) ok = false, std::printf("restart check: working directory %s, the first run's was %s\n", cwd().c_str(), argv[2 + hard + 1]);
  std::printf(ok ? "restart check: OK\n" : "restart check: NOT OK\n");
  return ok ? 0 : 1;
}
