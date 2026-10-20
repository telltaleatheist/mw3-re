// Probes for test_hud.py (docs/hud.md): the HUD (mech3/hud) built on the
// ported zTwoD with every call out recorded; dumped the way test_hud.py dumps
// the original's object from the emulator's memory.
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

#include "mech3/hud.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;

uint32_t g_hud_addr = 0;
mech3::Hud* g_hud = nullptr;

struct Hooks : mech3::HudHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return call(key, args); }
  float entry_flip(uint32_t entry) override { return zgame::from_bits(call("peek32", {entry + 0xd0})); }
  uint32_t id(const void* p) override { return p == g_hud ? g_hud_addr : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
};
Hooks g_hooks;

// A piece's name by its place in the HUD (as the emulator's offsets).
std::string piece_name(const ztwod::Object2D* p) {
  if (!p) return "0";
  const mech3::Hud& h = *g_hud;
  const std::pair<const void*, const char*> named[] = {
      {&h.radar, "radar"}, {&h.compass, "compass"}, {&h.reticle, "reticle"}, {&h.gauges, "gauges"},
      {&h.mfd, "mfd"}, {&h.target, "target"}, {&h.weapons, "weapons"}, {&h.objectives, "objectives"},
      {&h.timer, "timer"}, {&h.equipment, "equipment"}};
  for (auto& [q, n] : named)
    if (q == p) return n;
  return "?";
}

void dump_hud(Dump& d) {
  mech3::Hud& h = *g_hud;
  d.kv("active", h.active);
  d.o << "head=" << piece_name(h.head) << " tail=" << piece_name(h.tail) << ' ';
  d.kv("mech", 0);
  for (uint32_t v : h.x22e08) d.kv("z", v);
  d.o << "\n";
  for (const ztwod::Object2D* p : {static_cast<ztwod::Object2D*>(&h.compass), static_cast<ztwod::Object2D*>(&h.gauges),
                                   static_cast<ztwod::Object2D*>(&h.target), static_cast<ztwod::Object2D*>(&h.mfd),
                                   static_cast<ztwod::Object2D*>(&h.weapons), static_cast<ztwod::Object2D*>(&h.equipment)}) {
    d.o << "link " << piece_name(p) << " next=" << piece_name(p->next) << " owner=" << (p->owner == &h ? 1 : 0)
        << "\n";
  }
  mech3::HudCompass& c = h.compass;
  d.o << "compass ";
  d.base(&c);
  d.f("scale", c.scale);
  d.kv("half", c.half);
  d.kv("centre", c.centre);
  d.r("srcr", c.src);
  d.o << "\ntape ";
  d.image(&c.tape);
  d.o << "\nframe ";
  d.image(&c.frame);
  d.o << "\nctext ";
  d.fonttext(&c.text);
  d.o << "\ntimer ";
  d.fonttext(&h.timer);
  d.o << "\n";
  const mech3::DamageSet& s = mech3::damage_set();
  d.kv("loaded", s.loaded);
  d.kv("bgw", s.bg_w);
  d.kv("bgh", s.bg_h);
  for (auto* t : s.bg) d.t("bg", t);
  for (const mech3::DamageImages& p : s.parts) {
    for (auto* t : p.image) d.t("i", t);
    d.kv("px", p.x);
    d.kv("py", p.y);
  }
  d.o << "\n";
}

}  // namespace

extern "C" {

// Builds the HUD as the emulator did at `hud_addr` for `mech`, dumps it.
int probe_hud_build(uint32_t hud_addr, uint32_t mech, char* out, int size) {
  RecordingHost host;
  ztwod::set_host(&host);
  reset();  // the textures by address: each run's emulator starts over
  mech3::set_hud_hooks(&g_hooks);
  mech3::damage_set() = mech3::DamageSet{};
  g_hud_addr = hud_addr;
  g_hud = nullptr;  // the last one is left: its images' release would call the host
  g_hud = static_cast<mech3::Hud*>(::operator new(sizeof(mech3::Hud)));
  new (g_hud) mech3::Hud(reinterpret_cast<mech3::Mech*>(static_cast<uintptr_t>(mech)));
  Dump d;
  dump_hud(d);
  const std::string s = d.o.str();
  if (static_cast<int>(s.size()) >= size) return -1;
  std::memcpy(out, s.c_str(), s.size() + 1);
  ztwod::set_host(nullptr);
  return static_cast<int>(s.size());
}

}  // extern "C"
