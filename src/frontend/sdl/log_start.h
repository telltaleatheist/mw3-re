#pragma once
// Ours (sdl_child.h's log_to_file; docs/mods-campaigns-plan.md, the restart
// on Windows): how a start takes over the log file. The last start's log is
// moved aside (mw3.log to mw3.old.log) and a new one begun; when the move
// cannot be made (on Windows, while anything still holds the old file) the
// old log is never emptied: this start's lines are appended to it, after a
// line that says so. Plain code, the move and the pause given by the caller,
// so it is built and tested on every system.
#include <functional>
#include <string>

namespace sdlfe {

// The restarted program is told the process it comes from (its id, decimal)
// by this environment variable, set by restart_into on Windows: the new start
// waits for that process to end before it moves the log.
constexpr const char* kRestartedFromEnv = "MW3_RESTARTED_FROM";

struct LogStart {
  bool had_old = false;     // there was a log to move
  bool moved = false;       // it was moved aside
  int tries = 0;            // moves tried
  unsigned long error = 0;  // the last failed move's error (the system's code)
  bool append = false;      // the old log stays: open for append, never "w"
};

// `move` tries the move once and returns 0 or the system's error; it is tried
// up to `tries` times with `pause` between. No old log: nothing to move, a
// new file.
LogStart log_start(bool log_exists, int tries, const std::function<unsigned long()>& move, const std::function<void()>& pause);

// fopen's mode for the log after log_start: "w", or "a" when the old one stays.
inline const char* log_start_mode(const LogStart& s) { return s.append ? "a" : "w"; }

// The line to write first when the old log stayed ("" otherwise).
std::string log_start_note(const LogStart& s, const std::string& old_name);

}  // namespace sdlfe
