// Pirate's Moon's FMV script tick (0x005ba960) for test_pm_fmv_skip.py: a script of recording
// actions, each skippable or not, each step answering from a plan; polls with keys down or not.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zfmv/fmv_script.h"

namespace {
std::vector<int32_t>* g_log = nullptr;
const uint8_t* g_keys = nullptr;  // this poll's key down

void put(int32_t a, int32_t b, int32_t c) { g_log->push_back(a), g_log->push_back(b), g_log->push_back(c); }
int32_t tbits(double t) {
  const float f = static_cast<float>(t);
  int32_t v;
  std::memcpy(&v, &f, 4);
  return v;
}

struct Rec : zfmv::Action {
  int32_t index;
  bool skip;
  const uint8_t* plan;  // step's answers, one per call
  int32_t calls = 0;
  Rec(int32_t i, bool s, const uint8_t* p) : index(i), skip(s), plan(p) {}
  bool step(double t) override {
    put(1, index, tbits(t));
    return plan[calls++ & 7] != 0;
  }
  void start(double t) override { put(2, index, tbits(t)); }
  void end() override { put(3, index, 0); }
  bool skippable() override { return skip; }
};

struct Host : zfmv::Host {
  uint32_t key_state(int32_t dik) override { return dik == 0x39 && *g_keys ? 0x80 : 0; }
  void input_clear() override { put(4, 0, 0); }
  zgame::Handle sound_snapshot() override { return 0x77; }
  void sound_stop_snapshot(zgame::Handle s) override { put(5, static_cast<int32_t>(s), 0); }
};
}  // namespace

// n actions (skips[i], plans[8 i..]), the one current at `at` (n: none); polls at times ts with keys.
// Out: (kind, a, b) triples; after each poll (6, its answer, the current's index or -1).
extern "C" int32_t probe_pm_fmv_skip(int32_t n, const uint8_t* skips, const uint8_t* plans, int32_t at, int32_t polls,
                                     const double* ts, const uint8_t* keys, int32_t* out, int32_t cap) {
  std::vector<int32_t> log;
  g_log = &log;
  Host h;
  zfmv::set_host(&h);
  std::vector<Rec*> acts;
  {
    zfmv::Script s(nullptr, nullptr, 0);
    for (int32_t i = 0; i < n; ++i) {
      acts.push_back(new Rec(i, skips[i] != 0, plans + 8 * i));
      s.append(acts.back());
    }
    s.current = at < n ? acts[static_cast<size_t>(at)] : nullptr;
    s.started = 0.0;
    for (int32_t p = 0; p < polls; ++p) {
      g_keys = keys + p;
      const bool r = s.playing_at(ts[p]);
      int32_t cur = -1;
      for (int32_t i = 0; i < n; ++i)
        if (s.current == acts[static_cast<size_t>(i)]) cur = i;
      put(6, r, cur);
    }
  }
  zfmv::set_host(nullptr);
  const int32_t len = static_cast<int32_t>(log.size());
  for (int32_t i = 0; i < len && i < cap; ++i) out[i] = log[static_cast<size_t>(i)];
  g_log = nullptr;
  return len;
}
