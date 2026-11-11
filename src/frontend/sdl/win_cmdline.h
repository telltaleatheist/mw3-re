#pragma once
// Ours (sdl_restart.h, the Windows side): a Windows command line from a
// program and its arguments, quoted so that the started program's own
// splitting (CommandLineToArgvW, and the C runtime's, which agree on these
// rules) gives back exactly these arguments. Plain string code, so it is
// built and tested on every system.
#include <string>
#include <vector>

namespace sdlfe {

// One argument as it stands in a command line: as it is when it has no
// space, tab, newline or quote and is not empty; else in quotes, a quote
// inside written \", the backslashes just before a quote (or before the
// closing one) doubled, every other backslash as it is.
std::string win_quote_arg(const std::string& arg);

// The whole line: the program's path in plain quotes (the first word is
// split by other rules: no escapes, so a path, which holds no quote, is only
// wrapped), then each argument by win_quote_arg, one space between.
std::string win_command_line(const std::string& program, const std::vector<std::string>& args);

}  // namespace sdlfe
