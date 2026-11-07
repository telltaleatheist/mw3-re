#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "mech3/load_watch.h"
#include "check.h"

// A step that goes quiet is named; a step that moves on, or a load that ends, says nothing.
TEST(load_watch_names_a_quiet_step) {
  std::mutex m;
  std::vector<std::string> lines;
  auto count = [&] {
    std::lock_guard<std::mutex> g(m);
    return lines.size();
  };
  {
    mech3::LoadWatch watch(40, [&](const std::string& s) {
      std::lock_guard<std::mutex> g(m);
      lines.push_back(s);
    });
    for (int i = 0; i < 20; ++i) {  // steps inside the first wait: silence
      mech3::LoadWatch::step("Loading Textures");
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT(count() == 0);
    mech3::LoadWatch::step("Loading HUD");
    for (int i = 0; i < 400 && count() < 2; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));  // 40 ms, then 160
  }
  const size_t ended = count();
  EXPECT(ended >= 2);
  if (ended) {
    EXPECT(lines[0].find("nothing new for ") != std::string::npos);
    EXPECT(lines[0].find("\"Loading HUD\"") != std::string::npos);
    EXPECT(lines[0].find("CPU time") != std::string::npos);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  EXPECT(count() == ended);
  mech3::LoadWatch::step("after the end");  // no watch running: nothing to touch
}
