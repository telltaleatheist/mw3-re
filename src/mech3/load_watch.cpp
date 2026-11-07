#include "mech3/load_watch.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <thread>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mech3 {
namespace {
using Clock = std::chrono::steady_clock;
double cpu_seconds() {
#if defined(_WIN32)  // clock() there is the time since start, not CPU time
  FILETIME made, ended, kernel, user;
  if (!GetProcessTimes(GetCurrentProcess(), &made, &ended, &kernel, &user)) return 0;
  const auto ticks = [](const FILETIME& t) { return (static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime; };
  return static_cast<double>(ticks(kernel) + ticks(user)) / 1e7;
#else
  return static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
#endif
}
LoadWatch::Say say_to_log() {
  return [](const std::string& s) { std::printf("%s\n", s.c_str()); };
}
}  // namespace

struct LoadWatch::Impl {
  std::mutex m;
  std::condition_variable wake;
  bool stop = false;
  std::string name = "the start";
  Clock::time_point since = Clock::now();
  int first_ms = 0;
  Say say;
  std::thread thread;

  void run() {
    std::unique_lock<std::mutex> g(m);
    auto wait = std::chrono::milliseconds(first_ms);
    Clock::time_point seen = since;
    while (!stop) {
      if (wake.wait_until(g, seen + wait, [&] { return stop || since != seen; })) {
        if (since != seen) seen = since, wait = std::chrono::milliseconds(first_ms);
        continue;
      }
      const auto quiet = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - since).count();
      char line[256];
      std::snprintf(line, sizeof line, "mw3: load: nothing new for %lld s, on \"%s\" (CPU time so far %.0f s)",
                    static_cast<long long>(quiet), name.c_str(), cpu_seconds());
      say(line);
      wait *= 4;
    }
  }
};

namespace {
std::mutex g_running_lock;
LoadWatch::Impl* g_running = nullptr;
}  // namespace

LoadWatch::LoadWatch(int first_ms, Say say) : impl_(new Impl) {
  impl_->first_ms = first_ms;
  impl_->say = say ? std::move(say) : say_to_log();
  impl_->thread = std::thread([i = impl_] { i->run(); });
  std::lock_guard<std::mutex> g(g_running_lock);
  g_running = impl_;
}

LoadWatch::~LoadWatch() {
  {
    std::lock_guard<std::mutex> g(g_running_lock);
    g_running = nullptr;
  }
  {
    std::lock_guard<std::mutex> g(impl_->m);
    impl_->stop = true;
  }
  impl_->wake.notify_all();
  impl_->thread.join();
  delete impl_;
}

void LoadWatch::step(const char* name) {
  std::lock_guard<std::mutex> g(g_running_lock);
  if (!g_running) return;
  {
    std::lock_guard<std::mutex> l(g_running->m);
    g_running->name = name ? name : "";
    g_running->since = Clock::now();
  }
  g_running->wake.notify_all();
}

}  // namespace mech3
