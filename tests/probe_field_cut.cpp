// Probes for test_field_cut.py (docs/vehicles.md, A mobile field base lost):
// the store cut of 0x004590b0 (mech3/mech_fieldbase_cut). The draws, the
// mechs' names, the lab's rounds and the messages go to the shared callback;
// the stores are real.
#include <cstdint>
#include <string>

#include "mech3/mech_config.h"
#include "mech3/mech_fieldbase_cut.h"
#include "mech3/pilot.h"
#include "probe_app.h"

namespace {
namespace m3 = mech3;

m3::CampaignGame* g_game = nullptr;
std::string g_name;

struct Hooks : m3::FieldCutHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return probe_app::call(key, args);
  }
  int32_t rand() override { return static_cast<int32_t>(unported("rand", {})); }
  const char* mech_name(int32_t mech) override {
    unported("0x00484650", {mech});
    g_name = "mech" + std::to_string(mech);  // as the test's stub
    return g_name.c_str();
  }
  int32_t lab_rounds(int32_t weapon, int32_t n) override {
    return static_cast<int32_t>(unported("0x004b4900", {weapon, n}));
  }
  void message(float seconds, int32_t id, const char* text) override {
    if (text) unported("0x0040fe70", {zgame::bits(seconds), id, text});
    else unported("0x0040fe70", {zgame::bits(seconds), id, 0});
  }
};
Hooks g_hooks;

int dump(const m3::Store& s, uint32_t* out) {
  int k = 0;
  out[k++] = static_cast<uint32_t>(s.items.size());
  for (m3::ConfigItem* it : s.items) {
    out[k++] = static_cast<uint32_t>(it->kind());
    out[k++] = static_cast<uint32_t>(it->id());
    const auto* e = dynamic_cast<m3::EquipmentItem*>(it);
    const auto* w = dynamic_cast<m3::WeaponItem*>(it);
    const auto* g = dynamic_cast<m3::EngineItem*>(it);
    out[k++] = e ? static_cast<uint32_t>(e->tech) : w ? static_cast<uint32_t>(w->tech) : g ? g->xl : 0xffffffffu;
    out[k++] = static_cast<uint32_t>(it->count);
    out[k++] = it->b10;
  }
  return k;
}
}  // namespace

extern "C" {
void probe_fc_hooks(int on) { m3::set_field_cut_hooks(on ? &g_hooks : nullptr); }

// The bases (+0xa8) and the store's items (n): kind (0 a mech, 1 a weapon,
// 2 ammo, 3 equipment, 4 an engine), id, tech (xl), count, b10.
void probe_fc_setup(int32_t bases, int32_t n, const int32_t* items) {
  delete g_game;
  g_game = new m3::CampaignGame(nullptr, 2, 1);
  g_game->a8 = bases;
  m3::Store& s = g_game->store;
  for (int32_t i = 0; i < n; ++i) {
    const int32_t* x = items + 5 * i;
    m3::ConfigItem* it = nullptr;
    switch (x[0]) {
      case 0: it = s.add_mech(x[1]); break;
      case 1: it = s.add_weapon(x[1], x[2]); break;
      case 2: it = s.add_ammo(x[1], 1); break;
      case 3: it = s.add_equipment(x[1], x[2], 1); break;
      default: it = s.add_engine(x[1], static_cast<uint8_t>(x[2])); break;
    }
    if (!it) continue;
    it->count = x[3];
    it->b10 = static_cast<uint8_t>(x[4]);
  }
}

void probe_fc_run() { m3::field_base_cut(*g_game); }

// The bases, then the store and the +0x78 store dumped.
int probe_fc_image(uint32_t* out) {
  int k = 0;
  out[k++] = static_cast<uint32_t>(g_game->a8);
  k += dump(g_game->store, out + k);
  k += dump(g_game->store78, out + k);
  return k;
}
}  // extern "C"
