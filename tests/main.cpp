#include <csignal>
#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <execinfo.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#endif
#include <thread>
#endif
#if defined(_WIN32)
#include <process.h>
#endif
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "check.h"
#include "gamez/zgame/zgame_table.h"

namespace check {
std::vector<Test>& registry() {
  static std::vector<Test> tests;
  return tests;
}
int failures = 0;
}  // namespace check

#if !defined(_WIN32)
namespace check {
namespace {
std::vector<std::pair<pid_t, std::string>>& child_files() {
  static std::vector<std::pair<pid_t, std::string>> files;
  return files;
}

// The child's own memory in bytes: macOS's footprint (what the memory pressure counts; setrlimit is not
// enforced there), else the resident set.
uint64_t child_footprint() {
#if defined(__APPLE__)
  task_vm_info_data_t info{};
  mach_msg_type_number_t n = TASK_VM_INFO_COUNT;
  if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &n) != KERN_SUCCESS) return 0;
  return info.phys_footprint;
#else
  unsigned long pages = 0, resident = 0;
  if (std::FILE* f = std::fopen("/proc/self/statm", "r")) {
    if (std::fscanf(f, "%lu %lu", &pages, &resident) != 2) resident = 0;
    std::fclose(f);
  }
  return static_cast<uint64_t>(resident) * static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
#endif
}

double env_or(const char* name, double fallback) {
  const char* v = std::getenv(name);
  if (!v || !*v) return fallback;
  const double d = std::atof(v);
  return d > 0 ? d : fallback;
}

// A forked child's own limits, so an orphan cannot run on when a wrapper's cap kills only the parent:
// MW3_CHILD_SECS (default 1800) by alarm, MW3_CHILD_MEM_GB (default 8) and the parent's death by a
// watchdog thread twice a second. Over the memory it exits 125 (run_capped.py's code), orphaned 124.
void child_limits(pid_t parent) {
  alarm(static_cast<unsigned>(env_or("MW3_CHILD_SECS", 1800.0)));
  const uint64_t cap = static_cast<uint64_t>(env_or("MW3_CHILD_MEM_GB", 8.0) * 1024.0 * 1024.0 * 1024.0);
  std::thread([parent, cap] {
    for (;;) {
      if (getppid() != parent) _exit(124);
      const uint64_t used = child_footprint();
      if (used > cap) {
        char msg[128];
        const int len = std::snprintf(msg, sizeof msg, "  the child went past MW3_CHILD_MEM_GB (%.1f GB); ended\n",
                                      static_cast<double>(used) / (1024.0 * 1024.0 * 1024.0));
        if (len > 0) (void)!write(2, msg, static_cast<size_t>(len));
        _exit(125);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }).detach();
}
}  // namespace

pid_t fork_child() {
  static int made = 0;
  std::error_code ec;
  const std::filesystem::path dir = std::filesystem::path(MW3_TEST_TMP_ROOT) / "child-output";
  std::filesystem::create_directories(dir, ec);
  const std::string path = (dir / (std::to_string(static_cast<long>(getpid())) + "-" + std::to_string(++made) + ".txt")).string();
  std::fflush(nullptr);
  const pid_t parent = getpid();
  const pid_t child = fork();
  if (child == 0) {
    failures = 0;  // the child counts its own checks: the parent's earlier failures would fail every later child
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
      dup2(fd, 1), dup2(fd, 2);
      close(fd);
    }
    child_limits(parent);
    return 0;
  }
  if (child > 0) child_files().push_back({child, path});
  return child;
}

int wait_child(pid_t child) {
  int status = 1;
  waitpid(child, &status, 0);
  std::string path;
  for (auto it = child_files().begin(); it != child_files().end(); ++it)
    if (it->first == child) {
      path = it->second;
      child_files().erase(it);
      break;
    }
  if (path.empty()) return status;
  std::fflush(nullptr);
  if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
    char buf[1 << 14];
    for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) std::fwrite(buf, 1, n, stdout);
    std::fclose(f);
  }
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    std::remove(path.c_str());
  } else {
    if (WIFSIGNALED(status)) std::printf("  the child was ended by signal %d; its output is kept: %s\n", WTERMSIG(status), path.c_str());
    else std::printf("  the child exited %d; its output is kept: %s\n", WIFEXITED(status) ? WEXITSTATUS(status) : -1, path.c_str());
  }
  std::fflush(stdout);
  return status;
}
}  // namespace check
#endif

const char* mw3_test_tmp() {
  // Never freed: the exit's cleanup runs after the statics are gone.
  static const std::string* dir = new std::string([] {
    namespace fs = std::filesystem;
    const fs::path root = fs::path(MW3_TEST_TMP_ROOT) / "test-tmp";
    std::error_code ec;
#if !defined(_WIN32)
    // The folders of runs that died (no process of that id) cleared.
    for (const fs::directory_entry& e : fs::directory_iterator(root, ec)) {
      const long pid = std::strtol(e.path().filename().string().c_str(), nullptr, 10);
      if (pid > 0 && kill(static_cast<pid_t>(pid), 0) != 0) fs::remove_all(e.path(), ec);
    }
    const long me = static_cast<long>(getpid());
#else
    const long me = static_cast<long>(_getpid());
#endif
    const fs::path d = root / std::to_string(me);
    fs::remove_all(d, ec);
    fs::create_directories(d, ec);
    return d.generic_string();  // as the program's own paths, '/' on Windows too
  }());
  static const bool cleanup = (std::atexit([] {
    std::error_code ec;
    std::filesystem::remove_all(*dir, ec);
  }), true);
  (void)cleanup;
  return dir->c_str();
}

#if !defined(_WIN32)
namespace {
// A crash prints where it happened and leaves at once (no crash report to wait
// on, which kept dying test processes stuck at exit).
void on_crash(int sig) {
  void* frames[48];
  const int n = backtrace(frames, 48);
  const char msg[] = "mw3tests: crashed, backtrace:\n";
  write(2, msg, sizeof msg - 1);
  backtrace_symbols_fd(frames, n, 2);
  _exit(128 + sig);
}
}  // namespace
#endif

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define MW3_ASAN 1
#endif
#endif

int main() {
#if defined(_WIN32)
  // No crash dialogs: a test that crashes exits (the run is often headless, over ssh).
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#elif !defined(MW3_ASAN)  // AddressSanitizer reports crashes itself
  static char alt[1 << 16];
  stack_t ss{};
  ss.ss_sp = alt, ss.ss_size = sizeof alt;
  sigaltstack(&ss, nullptr);
  for (int sig : {SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE}) {
    struct sigaction sa{};
    sa.sa_handler = on_crash;
    sa.sa_flags = SA_ONSTACK;
    sigaction(sig, &sa, nullptr);
  }
#endif
  // A test that crashes still shows where. Windows' CRT has no line buffering
  // (and takes size 0 as an invalid argument): unbuffered there.
#if defined(_WIN32)
  std::setvbuf(stdout, nullptr, _IONBF, 0);
#else
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
#endif
  // MW3_TEST=name runs only the tests whose names contain it; a,b,c any of them.
  // MW3_SKIP=name (the same form) leaves those out: MW3_SKIP=headless skips the
  // whole-mission runs, nearly all of the suite's time.
  const char* only = std::getenv("MW3_TEST");
  const char* skip = std::getenv("MW3_SKIP");
  auto any_in = [](const char* list, const char* name) {
    for (const char* p = list; *p;) {
      const char* e = std::strchr(p, ',');
      const std::string part(p, e ? e - p : std::strlen(p));
      if (!part.empty() && std::strstr(name, part.c_str())) return true;
      if (!e) break;
      p = e + 1;
    }
    return false;
  };
  auto wanted = [&](const char* name) {
    if (skip && any_in(skip, name)) return false;
    return !only || any_in(only, name);
  };
  size_t ran = 0;
  for (const check::Test& t : check::registry()) {
    if (!wanted(t.name)) continue;
    ++ran;
    if (std::getenv("MW3_TEST_TRACE")) std::fprintf(stderr, "start %s\n", t.name);
    const int before = check::failures;
    // The game table (MW3's or PM's) each test starts with, put back after it: a PM test sets PM's, and a
    // later MW3-format writer (the new weapon form's reader.zbd) wrote PM's archives.
    const zgame::Table& game = zgame::game();
    t.fn();
    zgame::set_game(game);
    std::printf("%s %s\n", check::failures == before ? "ok  " : "FAIL", t.name);
#if !defined(_WIN32)
    // Ours: MW3_TEST_FDS=1, the open files after each test (a leak would
    // starve Windows' 512 FILEs long before a POSIX limit).
    if (std::getenv("MW3_TEST_FDS")) {
      int n = 0;
      if (DIR* d = opendir("/dev/fd")) {
        while (readdir(d)) ++n;
        closedir(d);
      }
      std::fprintf(stderr, "fds %d after %s\n", n, t.name);
#if defined(__APPLE__)
      if (std::strcmp(std::getenv("MW3_TEST_FDS"), "2") == 0)  // each one's file
        for (int fd = 3; fd < 4096; ++fd) {
          char path[1024];
          if (fcntl(fd, F_GETPATH, path) == 0) std::fprintf(stderr, "  fd %d %s\n", fd, path);
        }
#endif
    }
#endif
  }
  std::printf("%zu tests, %d failed checks\n", ran, check::failures);
  return check::failures == 0 ? 0 : 1;
}
