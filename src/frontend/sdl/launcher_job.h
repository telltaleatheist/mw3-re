#pragma once
// One progress line and one bar for everything a press starts (2026-11-27, docs/launcher.md, One bar for a press):
// a game's DOWNLOAD (the game, its original files, their setup), an UPDATE, UPDATE ALL, the launcher's own update. Its
// parts' bytes add up to one total; the line says how much is in, how fast and how long is left, and a smaller line
// names the part. Time is passed in (seconds), so the checks run it on a fake clock.

#include <cstdint>
#include <string>
#include <vector>

namespace sdlfe::job {

struct Part {
  std::string key;    // what the launcher calls it ("game:mw3", "original:mw3", "setup:mw3", "launcher")
  std::string words;  // the smaller line: "the game", "the original game files from the Internet Archive"
  uint64_t bytes = 0; // its size (the release's asset, the catalog's "size"); 0: not known
  bool setup = false; // a setup's share of the bar, after the bytes (no bytes of its own)
};

struct View {
  double fraction = 0;  // the bar, 0..1
  std::string line;     // "MechWarrior 3: 287 of 668 MB · 4.1 MB/s · about 2 min left"
  std::string part;     // "the game (43 MB)"
};

class Job {
 public:
  Job(std::string name, std::vector<Part> parts);

  // Part `key`'s download at `got` bytes (its .part), at `now`. A part not yet begun begins (those before it are in).
  void bytes(const std::string& key, uint64_t got, double now);
  // Part `key` past its bytes (a check, an unpack) or a setup's step: its own words and, for a setup, its fraction.
  void stage(const std::string& key, const std::string& words, double fraction, double now);
  // Part `key` left out (its game has its files already, ...): the total no longer counts it; `why` said on the
  // smaller line until the next part begins.
  void drop(const std::string& key, const std::string& why);
  // Part `key` done (its work ended well, perhaps between two looks): all its bytes in, a setup whole.
  void done(const std::string& key);
  bool has(const std::string& key) const;

  View view(double now) const;
  uint64_t total() const;  // the known sizes of the parts left in
  uint64_t got() const;    // their bytes in

  // The launcher's settings (launcher_settings.h; defaults 0.15, 5 s, 2 s, 10 s):
  static double setup_share();  // a setup's share of the bar, when the job has one
  static double window();       // seconds the speed is taken over
  static double settle();       // "starting..." until the bytes have come this long
  static double stall();        // "waiting for the server..." after this long without a byte

 private:
  struct State {
    Part part;
    uint64_t got = 0;
    bool begun = false, in = false, dropped = false;
    double setup_fraction = 0;
  };
  int find(const std::string& key) const;
  void begin(int k, double now);  // the speed taken afresh from here (a part's own)
  void sample(double now);
  std::string name_;
  std::vector<State> parts_;
  int at_ = -1;              // the part under way
  std::string words_;        // its stage's words past its bytes ("" while the bytes come)
  std::string note_;         // a dropped part's why, until the next part begins
  struct Sample {
    double t;
    uint64_t got;
  };
  std::vector<Sample> samples_;  // the last window() seconds of the total in
  double first_byte_ = -1, last_change_ = -1;
};

// "668 MB", "4.2 MB", "0.3 MB": megabytes of 1,000,000 bytes, one decimal below 10.
std::string mb(uint64_t bytes);
// "4.1 MB/s", "850 KB/s".
std::string speed_words(double bytes_per_s);
// "less than a minute left", "about 2 min left", "about 1 h 20 min left".
std::string left_words(double seconds);

}  // namespace sdlfe::job
