// How a start takes over the log (frontend/sdl/log_start.h): the last start's log moved aside, or, when the move
// cannot be made, kept and appended to, never emptied. The decision is plain code and is checked here with moves
// that fail as told, and once with real files (a move made to fail by a folder in the way). What this cannot show
// on a Mac or Linux: Windows refusing to move a file that is held open (a held file moves fine here); that is the
// Windows check's part (tests/win/restart_check.cpp).
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "check.h"
#include "frontend/sdl/log_start.h"

namespace {
namespace fs = std::filesystem;
std::string text_of(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
}  // namespace

TEST(log_start_moves_the_old_log_or_keeps_it) {
  int moves = 0, pauses = 0;
  auto fails = [&](int n, unsigned long code) {
    moves = pauses = 0;
    return [&moves, n, code]() -> unsigned long { return ++moves <= n ? code : 0; };
  };
  auto pause = [&] { ++pauses; };
  // No old log: nothing moved, a new file.
  sdlfe::LogStart s = sdlfe::log_start(false, 20, fails(99, 32), pause);
  EXPECT(!s.had_old && !s.moved && !s.append && s.tries == 0 && moves == 0 && std::string(sdlfe::log_start_mode(s)) == "w");
  EXPECT(sdlfe::log_start_note(s, "mw3.old.log").empty());
  // Moved at the first try.
  s = sdlfe::log_start(true, 20, fails(0, 32), pause);
  EXPECT(s.moved && !s.append && s.tries == 1 && moves == 1 && pauses == 0 && std::string(sdlfe::log_start_mode(s)) == "w");
  // Held for a moment (the restart on Windows: the second try): moved, with one pause.
  s = sdlfe::log_start(true, 20, fails(1, 32), pause);
  EXPECT(s.moved && !s.append && s.tries == 2 && moves == 2 && pauses == 1);
  // Held throughout: every try made, a pause between each, and the old log kept: append, with its line.
  s = sdlfe::log_start(true, 20, fails(99, 32), pause);
  EXPECT(!s.moved && s.append && s.tries == 20 && moves == 20 && pauses == 19 && s.error == 32);
  EXPECT(std::string(sdlfe::log_start_mode(s)) == "a");
  EXPECT(sdlfe::log_start_note(s, "mw3.old.log") ==
         "mw3: the last start's log could not be moved to mw3.old.log (error 32 after 20 tries): it is kept above, and this start's lines follow it");
  // A first start: one try, and still never emptied.
  s = sdlfe::log_start(true, 1, fails(99, 5), pause);
  EXPECT(!s.moved && s.append && s.tries == 1 && moves == 1 && pauses == 0);
  EXPECT(sdlfe::log_start_note(s, "mw3.old.log").find("(error 5 after 1 try)") != std::string::npos);
  s = sdlfe::log_start(true, 0, fails(0, 5), pause);  // a count of none is one try
  EXPECT(s.moved && s.tries == 1);
}

// With real files: the old log moved and a new one begun; then a move that cannot be made (a folder with a file
// in it where the old log goes): the last start's lines are still there, this start's after them.
TEST(log_start_never_empties_the_last_log) {
  const fs::path dir = fs::path(MW3_TEST_TMP) / "log_start";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  const fs::path log = dir / "mw3.log", old = dir / "mw3.old.log";
  auto start = [&](const char* line, int tries) {
    const sdlfe::LogStart s = sdlfe::log_start(
        fs::exists(log), tries,
        [&]() -> unsigned long {
          std::error_code e;
          fs::rename(log, old, e);
          return e ? static_cast<unsigned long>(e.value()) : 0;
        },
        nullptr);
    if (std::FILE* f = std::fopen(log.string().c_str(), sdlfe::log_start_mode(s))) {
      if (s.append) std::fprintf(f, "\n%s\n", sdlfe::log_start_note(s, "mw3.old.log").c_str());
      std::fprintf(f, "%s\n", line);
      std::fclose(f);
    }
    return s;
  };
  EXPECT(!start("first start", 1).had_old && text_of(log) == "first start\n");
  const sdlfe::LogStart second = start("second start", 1);
  EXPECT(second.moved && text_of(old) == "first start\n" && text_of(log) == "second start\n");
  // The old log's place taken by a folder that is not empty: the move fails every time.
  fs::remove(old, ec);
  fs::create_directories(old, ec);
  std::ofstream(old / "in the way.txt") << "x";
  const sdlfe::LogStart third = start("third start", 3);
  const std::string now = text_of(log);
  std::printf("%s", now.c_str());
  EXPECT(!third.moved && third.append && third.tries == 3 && third.error != 0);
  EXPECT(now.rfind("second start\n", 0) == 0);  // the last start's lines, whole
  EXPECT(now.find("could not be moved to mw3.old.log") != std::string::npos && now.find("after 3 tries") != std::string::npos);
  EXPECT(now.size() > 13 && now.compare(now.size() - 12, 12, "third start\n") == 0);
  fs::remove_all(dir, ec);
}
