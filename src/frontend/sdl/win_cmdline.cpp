#include "frontend/sdl/win_cmdline.h"

namespace sdlfe {

std::string win_quote_arg(const std::string& arg) {
  if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos) return arg;
  std::string out = "\"";
  size_t slashes = 0;  // the run of backslashes just written
  for (char c : arg) {
    if (c == '\\') {
      ++slashes;
      out += c;
      continue;
    }
    if (c == '"') out.append(slashes + 1, '\\');  // the run doubled, and one more for the quote
    slashes = 0;
    out += c;
  }
  out.append(slashes, '\\');  // a run before the closing quote doubled, so the quote still closes
  return out + "\"";
}

std::string win_command_line(const std::string& program, const std::vector<std::string>& args) {
  std::string line = "\"" + program + "\"";
  for (const std::string& a : args) line += " " + win_quote_arg(a);
  return line;
}

}  // namespace sdlfe
