#include "frontend/sdl/log_start.h"

namespace sdlfe {

LogStart log_start(bool log_exists, int tries, const std::function<unsigned long()>& move, const std::function<void()>& pause) {
  LogStart s;
  s.had_old = log_exists;
  if (!log_exists) return s;
  for (int t = 1; t <= (tries < 1 ? 1 : tries); ++t) {
    s.tries = t;
    s.error = move();
    if (s.error == 0) {
      s.moved = true;
      return s;
    }
    if (t < tries && pause) pause();
  }
  s.append = true;  // never emptied: this start's lines go after the last one's
  return s;
}

std::string log_start_note(const LogStart& s, const std::string& old_name) {
  if (!s.append) return {};
  return "mw3: the last start's log could not be moved to " + old_name + " (error " + std::to_string(s.error) + " after " +
         std::to_string(s.tries) + (s.tries == 1 ? " try" : " tries") + "): it is kept above, and this start's lines follow it";
}

}  // namespace sdlfe
