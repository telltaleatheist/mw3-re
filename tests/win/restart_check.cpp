// A Windows check of the restart (frontend/sdl/sdl_restart.cpp's _WIN32 side and log_start.h) without the game:
//
//   restart_check.exe DIR --watch      (DIR must exist)
//
// --watch runs the check twice, in DIR\plain and DIR\wait, waits for each to end, prints the logs and returns 0
// when both passed (it stays in the foreground, so a remote session that ends with its command sees it all).
//
// One check: the first run takes <dir>\check.log as its stdout and stderr (as the game's log_to_file does), then
// starts itself again through sdlfe::restart_into with arguments that are hard to quote. The second run moves
// check.log to check.old.log before it opens anything (as the game rotates its log; Windows refuses while
// anything holds the file), trying up to 20 times 50 ms apart (log_start), then opens a new check.log and says
// at which try the move went through, and whether its arguments and working directory are the first run's.
//   plain: the move is tried at once. It must go through within the tries.
//   wait:  the second run first waits for the first run's process to end (its id is in MW3_RESTARTED_FROM,
//          as the game's log_to_file reads it; 2 s at most), and says how long. The move must then go
//          through at the first try: if it does where the plain one needs two, the holder was the first process.
// OK also needs the first run's log whole in check.old.log and the new check.log begun fresh.
// Built by the mingw cross compiler on the Mac as a compile and link check; run on Windows.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <windows.h>

#include "frontend/sdl/log_start.h"
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

// One check as a child, waited for to its last line (30 s at most), both logs printed.
bool watch_one(const std::string& dir, bool wait) {
  CreateDirectoryA(dir.c_str(), nullptr);
  const std::string log = dir + "\\check.log", old = dir + "\\check.old.log";
  DeleteFileA(log.c_str()), DeleteFileA(old.c_str());
  std::vector<std::string> args = {dir};
  if (wait) args.push_back("--wait");
  std::string line = sdlfe::win_command_line(sdlfe::restart_own_path(), args);
  STARTUPINFOA si{};
  si.cb = sizeof si;
  PROCESS_INFORMATION pi{};
  if (!CreateProcessA(nullptr, line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
    std::printf("restart check: could not start the first run (error %lu)\n", static_cast<unsigned long>(GetLastError()));
    return false;
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
  const bool ok = now.find("restart check: OK") != std::string::npos;
  std::printf("==== %s: %s\n---- check.old.log (the first run)\n%s---- check.log (the second run)\n%s", wait ? "wait" : "plain",
              ok ? "passed" : "did not pass", text_of(old).c_str(), now.c_str());
  return ok;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: restart_check DIR --watch\n");
    return 2;
  }
  const std::string dir = argv[1], log = dir + "\\check.log", old = dir + "\\check.old.log";
  bool second = false, wait = false;
  for (int i = 2; i < argc; ++i) second = second || std::string(argv[i]) == "--restarted", wait = wait || std::string(argv[i]) == "--wait";
  if (argc == 3 && std::string(argv[2]) == "--watch") {
    const bool plain = watch_one(dir + "\\plain", false), waited = watch_one(dir + "\\wait", true);
    std::printf("==== plain %s, wait %s\n", plain ? "passed" : "did not pass", waited ? "passed" : "did not pass");
    return plain && waited ? 0 : 1;
  }
  // The second run takes the log over before it opens anything.
  sdlfe::LogStart s;
  DWORD waited_ms = 0;
  const char* from = std::getenv(sdlfe::kRestartedFromEnv);
  const char* found = "not asked";
  if (second) {
    if (wait) {
      found = "no id given";
      if (from) {
        found = "already gone";
        if (HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(std::strtoul(from, nullptr, 10)))) {
          const DWORD t0 = GetTickCount();
          found = WaitForSingleObject(p, 2000) == WAIT_OBJECT_0 ? "waited for its end" : "still running after 2 s";
          waited_ms = GetTickCount() - t0;
          CloseHandle(p);
        }
      }
    }
    DeleteFileA(old.c_str());
    s = sdlfe::log_start(
        GetFileAttributesA(log.c_str()) != INVALID_FILE_ATTRIBUTES, 20,
        [&]() -> unsigned long { return MoveFileA(log.c_str(), old.c_str()) ? 0 : GetLastError(); }, [] { Sleep(50); });
  }
  if (!std::freopen(log.c_str(), sdlfe::log_start_mode(s), stdout) || !std::freopen(log.c_str(), "a", stderr)) return 3;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (s.append) std::printf("\n%s\n", sdlfe::log_start_note(s, "check.old.log").c_str());
  std::printf("restart check: process %lu, %s run%s\n own path %s\n working directory %s\n", static_cast<unsigned long>(GetCurrentProcessId()),
              second ? "second" : "first", wait ? ", wait mode" : ", plain mode", sdlfe::restart_own_path().c_str(), cwd().c_str());
  for (int i = 0; i < argc; ++i) std::printf(" argv[%d] = [%s]\n", i, argv[i]);
  if (!second) {
    std::vector<std::string> args = {dir};
    if (wait) args.push_back("--wait");
    for (const char* h : kHard) args.emplace_back(h);
    args.push_back("--cwd"), args.push_back(cwd());
    args.push_back("--restarted");
    std::string why;
    sdlfe::restart_into(argv[0], args, &why);
    std::printf("restart check: FAILED to start again: %s\n", why.c_str());
    return 1;
  }
  // The second run: argv[1] DIR, [--wait], the hard ones, --cwd <the first run's>, --restarted.
  const int hard = static_cast<int>(sizeof kHard / sizeof kHard[0]), at = wait ? 3 : 2, want = at + hard + 3;
  std::printf("restart check: the first process (%s): %s, %lu ms\n", from ? from : "no id", found, static_cast<unsigned long>(waited_ms));
  if (s.moved) std::printf("restart check: the first run's log moved at try %d\n", s.tries);
  else std::printf("restart check: the first run's log was still held after %d tries (error %lu): kept, this run appended\n", s.tries, s.error);
  bool ok = s.moved && (!wait || s.tries == 1) && argc == want;
  if (s.moved && wait && s.tries != 1) std::printf("restart check: with the first process gone the log must move at the first try\n");
  if (argc != want) std::printf("restart check: %d arguments, expected %d\n", argc, want);
  for (int i = 0; ok && i < hard; ++i)
    if (std::string(argv[at + i]) != kHard[i]) ok = false, std::printf("restart check: argument %d is [%s], sent [%s]\n", at + i, argv[at + i], kHard[i]);
  if (ok && cwd() != argv[at + hard + 1]) ok = false, std::printf("restart check: working directory %s, the first run's was %s\n", cwd().c_str(), argv[at + hard + 1]);
  // The first run's log whole in check.old.log; this one begun fresh.
  const std::string was = text_of(old);
  const bool whole = was.find("restart check: process") == 0 && was.find("first run") != std::string::npos && was.find("mw3: starting again as process") != std::string::npos;
  if (ok && !whole) ok = false, std::printf("restart check: check.old.log is not the first run's whole log\n");
  if (ok && s.append) ok = false;
  std::printf(ok ? "restart check: OK\n" : "restart check: NOT OK\n");
  return ok ? 0 : 1;
}
