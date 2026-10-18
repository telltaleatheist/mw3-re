#pragma once

// Original: FUN_0049bfd0(level, file, line, format, ...), called by every engine
// assertion with the source path that let us map the exe (docs/module-map.md).
// The rebuild routes reports to whatever sink the front end installs.
namespace zerr {

using Sink = void (*)(int level, const char* file, int line, const char* message);

void set_sink(Sink sink);
void report(int level, const char* file, int line, const char* message);

}  // namespace zerr
