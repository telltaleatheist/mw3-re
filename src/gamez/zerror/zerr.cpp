#include "gamez/zerror/zerr.h"

namespace zerr {

namespace {
Sink g_sink = nullptr;
}  // namespace

void set_sink(Sink sink) { g_sink = sink; }

void report(int level, const char* file, int line, const char* message) {
  if (g_sink) g_sink(level, file, line, message);
}

}  // namespace zerr
