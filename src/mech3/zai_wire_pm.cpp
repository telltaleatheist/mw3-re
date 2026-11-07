#include "mech3/zai_wire_pm.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mech3::ai {

namespace {
std::map<std::string, int>& missing_counts() {
  static std::map<std::string, int> m;
  return m;
}
}  // namespace

bool pm_ai_on() {
  static const bool on = [] {
    const char* v = std::getenv("PM_AI");
    return v && std::strcmp(v, "1") == 0;
  }();
  return on;
}

void pm_hook_missing(const char* key) {
  if (++missing_counts()[key] == 1) std::printf("mw3: pm ai: no answer yet for %s\n", key);
}

// The params keep MW3's offsets (zai_params.h); +0xcc is PM's +0xec. The brain's own fields are
// the twins docs/pm-tests.md names; `home` and the times start as the Brain's ctor left them.
void pm_seed(BrainTickPm& t, const Brain& b) {
  const AiParams& p = b.params;
  t.sight = p.get<float>(0x1c), t.engage = p.get<float>(0x20), t.leash = p.get<float>(0x24);
  t.mode = p.get<int32_t>(0x3c), t.health_floor = p.get<float>(0x50);
  t.delay_on = p.get<int32_t>(0x5c), t.wait_low = p.get<float>(0x64), t.wait_high = p.get<float>(0x68);
  t.awake = p.get<int32_t>(0x88), t.by_player = p.get<int32_t>(0x8c), t.reacts = p.get<int32_t>(0x9c);
  t.wanders = p.get<int32_t>(0xa0), t.manoeuvre_kind = p.get<int32_t>(0xa4);
  t.react_gap = p.get<float>(0xb0), t.engage_flags = p.get<uint8_t>(0xcc);
  t.delay_until = b.xdc, t.busy = b.xf0, t.wait_until = b.xf4, t.home = b.xe4;
  t.attack_until = b.xe8, t.react_at = b.x100, t.give_up = b.x230, t.runs_down = b.x2a4;
  t.radio = b.radio != nullptr;
}

namespace {
std::map<const Brain*, PmBrain>& brains() {
  static std::map<const Brain*, PmBrain> m;
  return m;
}
std::map<zgame::Handle, TargetsPm>& targets() {
  static std::map<zgame::Handle, TargetsPm> m;
  return m;
}
std::map<const void*, uint32_t>& ids() {
  static std::map<const void*, uint32_t> m;
  return m;
}
std::map<uint32_t, void*>& ptrs() {
  static std::map<uint32_t, void*> m;
  return m;
}
uint32_t g_next_id = 1;
}  // namespace

PmBrain& pm_brain(Brain& b) {
  auto [it, fresh] = brains().try_emplace(&b);
  if (fresh) pm_seed(it->second.tick, b);
  return it->second;
}
TargetsPm& pm_targets(zgame::Handle vehicle) { return targets()[vehicle]; }
void pm_forget(Brain& b, zgame::Handle vehicle) {
  brains().erase(&b);
  targets().erase(vehicle);
}

uint32_t pm_id(const void* p) {
  if (!p) return 0;
  auto [it, fresh] = ids().try_emplace(p, g_next_id);
  if (fresh) ptrs()[g_next_id++] = const_cast<void*>(p);
  return it->second;
}
void* pm_ptr(uint32_t id) {
  const auto it = ptrs().find(id);
  return it == ptrs().end() ? nullptr : it->second;
}
void pm_ids_forget(const void* p) {
  const auto it = ids().find(p);
  if (it == ids().end()) return;
  ptrs().erase(it->second);
  ids().erase(it);
}
void pm_wire_clear() {
  brains().clear(), targets().clear(), ids().clear(), ptrs().clear();
  g_next_id = 1;
}

const std::map<std::string, int>& pm_hooks_missing() { return missing_counts(); }
void pm_hooks_missing_clear() { missing_counts().clear(); }

}  // namespace mech3::ai
