#include "gamez/zerror/zerr.h"

#include <cstdarg>
#include <cstdio>

namespace zerr {

namespace {
Sink g_sink = nullptr;
}  // namespace

void set_sink(Sink sink) { g_sink = sink; }

void report(int level, const char* file, int line, const char* message) {
  if (g_sink) g_sink(level, file, line, message);
}

void reportf(int level, const char* file, int line, const char* format, ...) {
  if (!g_sink) return;
  char text[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(text, sizeof text, format, args);
  va_end(args);
  g_sink(level, file, line, text);
}

}  // namespace zerr
