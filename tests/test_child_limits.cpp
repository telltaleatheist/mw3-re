// A forked test child's own limits (tests/main.cpp child_limits): past MW3_CHILD_MEM_GB it exits 125, past
// MW3_CHILD_SECS SIGALRM ends it, so an orphan of a killed run cannot grow or run on.
#if !defined(_WIN32)
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "check.h"

namespace {
// Forks with `name` set to `value` for the child's limits, the parent's left as it was.
pid_t fork_with(const char* name, const char* value) {
  const char* had = std::getenv(name);
  const std::string old = had ? had : "";
  setenv(name, value, 1);
  const pid_t child = check::fork_child();
  if (child != 0) {
    if (had) setenv(name, old.c_str(), 1);
    else unsetenv(name);
  }
  return child;
}
}  // namespace

TEST(test_child_ends_past_its_memory_cap) {
  const auto t0 = std::chrono::steady_clock::now();
  const pid_t child = fork_with("MW3_CHILD_MEM_GB", "0.25");
  if (child == 0) {
    std::vector<char*> held;
    for (int i = 0; i < 64; ++i) {  // 16 MB a step, touched, to 1 GB
      char* p = static_cast<char*>(std::malloc(16 << 20));
      std::memset(p, 1, 16 << 20);
      held.push_back(p);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    _exit(0);
  }
  const int status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 125);
  EXPECT(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(10));
}

TEST(test_child_ends_past_its_time) {
  const pid_t child = fork_with("MW3_CHILD_SECS", "1");
  if (child == 0) {
    std::this_thread::sleep_for(std::chrono::seconds(20));
    _exit(0);
  }
  const int status = check::wait_child(child);
  EXPECT(WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM);
}
#endif
