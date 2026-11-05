// The lab's PM pieces, for test_pm_lab_pick.py.
#include <cstdint>
#include <cstring>
#include <vector>

#include "crt/msvc_rand.h"
#include "mech3/app_hooks.h"
#include "mech3/lab_screen.h"
#include "gamez/ztwod/twod_host.h"
#include "mech3/briefing.h"
#include "mech3/mech_config.h"

extern "C" int32_t probe_pm_lab_pick(const int32_t* parts, const float* radii, int32_t n, int32_t weapon, int32_t part,
                                     uint32_t seed) {
  std::vector<mech3::Engine::LabFirepoint> fps;
  for (int32_t i = 0; i < n; ++i) fps.push_back({parts[i], radii[i]});
  crt::msvc_srand(seed);
  return mech3::lab_pick_firepoint(fps, weapon, part);
}

namespace {
struct PanelEngine : mech3::Engine {
  mech3::Config* c = nullptr;
  float lab_config_weight_of(mech3::Handle, int32_t what) override { return static_cast<float>(c->weight(what)); }
  int32_t lab_config_engine(mech3::Handle) override { return c->engine; }
};
}  // namespace

// The two data panels' numbers for a loadout: run, walk, the total, the most; then the
// chassis panel's rows 0..7, the total and the most (test_pm_lab_data.py).
extern "C" void probe_pm_lab_numbers(void* config, int32_t engine, int32_t xl, float tons, int32_t endo, float* out) {
  auto& c = *static_cast<mech3::Config*>(config);
  const int32_t keep_engine = c.engine;
  const uint8_t keep_xl = c.xl, keep_endo = c.endo_steel;
  const float keep_tons = c.armor_tons;
  c.engine = engine, c.xl = static_cast<uint8_t>(xl), c.armor_tons = tons, c.endo_steel = static_cast<uint8_t>(endo);
  PanelEngine e;
  e.c = &c;
  const mech3::LabSpeeds s = mech3::lab_mech_speeds(e, 1);
  out[0] = s.run, out[1] = s.walk, out[2] = static_cast<float>(c.weight(-1)), out[3] = s.most;
  for (int32_t r = 0; r < 8; ++r) out[4 + r] = mech3::lab_chassis_tons(e, 1, r);
  out[12] = static_cast<float>(c.weight(-1)), out[13] = s.most;
  c.engine = keep_engine, c.xl = keep_xl, c.armor_tons = keep_tons, c.endo_steel = keep_endo;
}

// MESSAGE_AREA's flash and timing (test_pm_lab_message.py): op 0 flash_start(a, rate b), 1 the
// message's end (0x004509f0), 2 a tick of dt b. out: flags & 0x11, timer, colours, flash, draws.
namespace {
struct CountedMessage : mech3::LabMessage {
  int32_t draws = 0;
  void draw() override { ++draws; }
  void dirty() override {}
};
struct QuietHost : ztwod::Host {
  int32_t sound_play(int32_t, float) override { return 0; }
};
CountedMessage* g_msg = nullptr;
QuietHost g_quiet;
}  // namespace

extern "C" void probe_pm_message_new(uint32_t top, uint32_t bottom) {
  ztwod::set_host(&g_quiet);
  delete g_msg;
  g_msg = new CountedMessage;
  g_msg->color_top = top, g_msg->color_bottom = bottom;
  g_msg->needs_render = 0;
  g_msg->x4b0 = 1;
}
extern "C" void probe_pm_message_op(int32_t op, uint32_t a, float b, uint32_t* out) {
  CountedMessage& m = *g_msg;
  if (op == 0) m.flash_start(a, b);
  else if (op == 1) m.after_text();
  else m.update(b);
  const auto f = [](float v) { uint32_t u; std::memcpy(&u, &v, 4); return u; };
  const uint32_t o[] = {m.flags & 0x11, f(m.timer), m.color_top, m.color_bottom, f(m.flash_timer), f(m.flash_rate),
                        m.flash_top, m.flash_bottom, m.flashing, static_cast<uint32_t>(m.flash_mode),
                        static_cast<uint32_t>(m.flash_dir), static_cast<uint32_t>(m.needs_render),
                        static_cast<uint32_t>(m.draws)};
  std::memcpy(out, o, sizeof o);
}

// The briefing's buttons by game (test_pm_briefing_buttons.py).
extern "C" void probe_pm_briefing_buttons(int32_t kind, int32_t on, int32_t* out) {
  const mech3::BriefingButtons b = mech3::briefing_buttons(kind, on != 0);
  const int32_t o[] = {b.abort_on, on, b.lab, b.salvage, b.replay, b.hide_abort, b.hide_lab, b.hide_salvage, b.hide_replay, b.auto_launch};
  std::memcpy(out, o, sizeof o);
}
