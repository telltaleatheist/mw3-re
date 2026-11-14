// The briefing script's skip and finish (Pirate's Moon 0x00405f20 / 0x00404780), for
// test_pm_briefing_skip.py: a script of recording steps, a frame (its head, then Space or S)
// or a press of MECHLAB / SALVAGE, as BriefingScreen::update and on_button run them.
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/app_hooks.h"
#include "mech3/briefing.h"

namespace {
std::vector<int32_t>* g_log = nullptr;

void put(int32_t k, int32_t a, int32_t b) {
  g_log->push_back(k);
  g_log->push_back(a);
  g_log->push_back(b);
}

int32_t bits(float f) {
  int32_t v;
  std::memcpy(&v, &f, 4);
  return v;
}

struct RecStep : mech3::BriefingStep {
  int32_t index;
  bool done;
  RecStep(int32_t i, bool d) : index(i), done(d) {}
  bool step(float dt) override {
    put(1, index, bits(dt));
    return done;
  }
};

struct SkipEngine : mech3::Engine {
  void voice_stop(mech3::Handle s) override { put(3, static_cast<int32_t>(s), 0); }
  void input_clear() override { put(2, 0, 0); }
};
}  // namespace

// mode 0: a frame (key 1 = Space or S held); 2 / 4: MECHLAB / SALVAGE pressed. Returns the
// log's length in ints: (kind, a, b) triples, the last (9, playing, at's index) then
// (8, reached, voice).
extern "C" int32_t probe_pm_briefing_skip(int32_t n, const uint8_t* dones, int32_t at, int32_t playing,
                                          int32_t key, uint32_t voice, int32_t reached, float dt, int32_t mode,
                                          int32_t* out, int32_t cap) {
  std::vector<int32_t> log;
  g_log = &log;
  SkipEngine e;
  std::vector<RecStep> steps;
  steps.reserve(static_cast<size_t>(n));
  mech3::briefing_voice() = 0;
  mech3::BriefingScript script(e);
  for (int32_t i = 0; i < n; ++i) {
    steps.emplace_back(i, dones[i] != 0);
    script.steps.push_back(&steps.back());
  }
  script.at = script.steps.begin();
  for (int32_t i = 0; i < at; ++i) ++script.at;
  script.playing = static_cast<uint8_t>(playing);
  mech3::briefing_voice() = voice;
  mech3::briefing_reached() = reached;
  if (mode == 0) {  // BriefingScreen::update's script head and its skip
    script.step(dt);
    if (key) {
      e.input_clear();
      script.finish(e);
    }
  } else {  // on_button's MECHLAB and SALVAGE
    script.finish(e);
    put(4, mode, 0);
  }
  int32_t idx = 0;
  for (auto it = script.steps.begin(); it != script.at; ++it) ++idx;
  put(9, script.playing, idx);
  put(8, mech3::briefing_reached(), static_cast<int32_t>(mech3::briefing_voice()));
  const int32_t len = static_cast<int32_t>(log.size());
  for (int32_t i = 0; i < len && i < cap; ++i) out[i] = log[static_cast<size_t>(i)];
  g_log = nullptr;
  return len;
}
