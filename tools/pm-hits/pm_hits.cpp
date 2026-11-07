// The first-run's first-hit log (docs/pm-tests.md, The switch's plan, 2): with MW3_PM_HITS=ON the PM AI
// sources are built with -finstrument-functions, and this records each function entered once. At exit,
// with PM_AI_HITS=<file>, it writes their offsets in the image; tools/pm-hits/score.py names them.
#include <dlfcn.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <set>

namespace {
std::set<void*>* g_seen = nullptr;
std::mutex* g_lock = nullptr;
const void* g_base = nullptr;

__attribute__((no_instrument_function)) void write_out() {
  const char* path = std::getenv("PM_AI_HITS");
  if (!path || !g_seen) return;
  std::FILE* f = std::fopen(path, "w");
  if (!f) return;
  for (void* p : *g_seen)
    std::fprintf(f, "%llx\n", static_cast<unsigned long long>(static_cast<const char*>(p) - static_cast<const char*>(g_base)));
  std::fclose(f);
}
}  // namespace

extern "C" {
__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void* fn, void*) {
  if (!g_seen) {
    g_seen = new std::set<void*>;
    g_lock = new std::mutex;
    Dl_info i;
    if (dladdr(fn, &i)) g_base = i.dli_fbase;
    std::atexit(write_out);
  }
  std::lock_guard<std::mutex> l(*g_lock);
  g_seen->insert(fn);
}
__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void*, void*) {}
}
