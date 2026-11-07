#pragma once
#include <functional>
#include <string>

// Ours, a line in the log and nothing else (docs/mission-load.md section 4): the
// port's load runs on the main thread with the screen on its last frame, so a
// load that stops looks the same as a slow one and the log shows only the last
// step begun. A thread says when a step has gone quiet, at growing waits, with
// the CPU time, which tells a spinning load from a blocked one.
namespace mech3 {

class LoadWatch {
 public:
  using Say = std::function<void(const std::string&)>;
  // `first_ms`: the first line after this long on one step; each next at 4 times the wait.
  explicit LoadWatch(int first_ms = 20000, Say say = {});
  ~LoadWatch();
  LoadWatch(const LoadWatch&) = delete;
  LoadWatch& operator=(const LoadWatch&) = delete;
  static void step(const char* name);  // a new step: on the watch running, if any
  struct Impl;

 private:
  Impl* impl_;
};

}  // namespace mech3
