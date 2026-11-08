// PM's allies turning (0x004e6240) for test_pm_allies_turn.py: a world given as tables, the masks, the pilots' byte
// and the sorted flag back.
#include <cstdint>
#include <map>
#include <memory>
#include <utility>

#include "mech3/sensors.h"
#include "mech3/zai_target.h"

using mech3::ai::AiTarget;
using zgame::Handle;

namespace {
std::map<std::pair<Handle, uint32_t>, Handle> g_vt;  // (object, slot) -> answer
std::map<Handle, std::unique_ptr<AiTarget>> g_targets;
std::map<Handle, Handle> g_pilot;      // brain -> its pilot (the brain's +0x100 base, vt+0x88)
std::map<Handle, uint8_t> g_byte;      // pilot -> +0xed
Handle g_player = 0;
uint8_t g_sorted = 1;

// PM's slots where the port names MW3's: the mech vt+0x48, the controls vt+0x14c, the brain vt+0x58.
uint32_t pm_slot(uint32_t mw3) {
  switch (mw3) {
    case 0x40: return 0x48;
    case 0x140: return 0x14c;
    case 0x54: return 0x58;
    default: return mw3;
  }
}

struct Hooks : mech3::ai::SensorHooks {
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
  uint32_t vcall(Handle o, uint32_t slot, std::initializer_list<zgame::Arg>) override {
    const auto it = g_vt.find({o, pm_slot(slot)});
    return it == g_vt.end() ? 0 : it->second;
  }
  AiTarget* fields(Handle x) override {
    const auto it = g_targets.find(x);
    return it == g_targets.end() ? nullptr : it->second.get();
  }
  Handle player() override { return g_player; }
  void unsort() override { g_sorted = 0; }
  void brain_flags_or(Handle b, uint8_t bits) override {
    const auto it = g_pilot.find(b);
    if (it != g_pilot.end() && it->second) g_byte[it->second] |= bits;
  }
};
Hooks g_hooks;
}  // namespace

extern "C" {

// vt: (object, slot, answer) triples; targets: (object, team, allies, enemies); pilots: (brain, pilot, byte).
// out: per target its team, allies, enemies; per pilot its byte; then the sorted flag.
int32_t probe_pm_allies_turn(const uint32_t* ents, int32_t n, int32_t first, int32_t end, uint32_t player,
                             const uint32_t* vt, int32_t nvt, const uint32_t* targets, int32_t nt, const uint32_t* pilots,
                             int32_t np, uint32_t* out) {
  g_vt.clear(), g_targets.clear(), g_pilot.clear(), g_byte.clear();
  for (int32_t i = 0; i < nvt; ++i) g_vt[{vt[3 * i], vt[3 * i + 1]}] = vt[3 * i + 2];
  for (int32_t i = 0; i < nt; ++i) {
    auto t = std::make_unique<AiTarget>();
    t->team = targets[4 * i + 1], t->allies = targets[4 * i + 2], t->enemies = targets[4 * i + 3];
    g_targets[targets[4 * i]] = std::move(t);
  }
  for (int32_t i = 0; i < np; ++i) g_pilot[pilots[3 * i]] = pilots[3 * i + 1], g_byte[pilots[3 * i + 1]] = uint8_t(pilots[3 * i + 2]);
  g_player = player, g_sorted = 1;
  mech3::ai::entity_list().assign(ents, ents + n);
  mech3::ai::sorted_allies() = {size_t(first), size_t(end)};
  mech3::ai::SensorHooks& old = mech3::ai::sensor_hooks();
  mech3::ai::set_sensor_hooks(&g_hooks);
  mech3::ai::allies_turn_pm();
  mech3::ai::set_sensor_hooks(&old);
  int32_t k = 0;
  for (int32_t i = 0; i < nt; ++i) {
    const AiTarget& t = *g_targets[targets[4 * i]];
    out[k++] = t.team, out[k++] = t.allies, out[k++] = t.enemies;
  }
  for (int32_t i = 0; i < np; ++i) out[k++] = g_byte[pilots[3 * i + 1]];
  out[k++] = g_sorted;
  return k;
}
}
