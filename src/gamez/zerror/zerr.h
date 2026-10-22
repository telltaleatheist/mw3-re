#pragma once

// Original: FUN_0049bfd0(level, file, line, format, ...), called by every engine
// assertion with the source path that let us map the exe (docs/module-map.md).
// The rebuild routes reports to whatever sink the front end installs.
namespace zerr {

using Sink = void (*)(int level, const char* file, int line, const char* message);

void set_sink(Sink sink);
void report(int level, const char* file, int line, const char* message);
// The original's own form: a printf format with its arguments, filled here
// (the exe's 0x0049bfd0 is empty in this build: the text is the rebuild's).
void reportf(int level, const char* file, int line, const char* format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 4, 5)))
#endif
    ;

}  // namespace zerr
