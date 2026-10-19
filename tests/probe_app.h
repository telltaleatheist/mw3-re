#pragma once
// Shared by the probe_app*.cpp files: the one Python callback every recorded call
// goes to (tests/differential/calltrace.py).
#include <cstdint>
#include <initializer_list>

#include "gamez/zgame/unported.h"

namespace probe_app {
uint32_t call(const char* key, std::initializer_list<zgame::Arg> args);
// Silences call() while it lives (set-up the test does not compare).
struct Mute {
  Mute();
  ~Mute();
};
}  // namespace probe_app
