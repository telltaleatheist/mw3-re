// PM's objectives can fail: a group's `fails` (0x004600d0), its own sound and message (0x0045fcf0), no salvage.
#include <memory>

#include "check.h"
#include <cstdio>
#include "gamez/zweapon/zweapon.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_data.h"
#include "mech3/app_mech.h"
#include "pm/gamez/zclass/cls_clutter.h"
#include "mech3/mech_config.h"
#include "mech3/mech_salvage.h"
#include "mech3/mech_objective.h"
#include "mech3/pilot.h"

using namespace mech3;

namespace {
struct Hooks : ObjectiveHooks {
  std::vector<int32_t> messages;
  std::vector<zgame::Handle> voices;
  zgame::Handle player_vehicle() override { return 1; }
  zgame::Handle view_vehicle() override { return 0; }
  void ask_voice(zgame::Handle s, float) override { voices.push_back(s); }
  void hud_message(float, int32_t id, const char*) override { messages.push_back(id); }
};
struct Flag : Objective {
  explicit Flag(uint8_t met) : Objective(kValveChange, nullptr), met(met) {}
  uint8_t check(ObjectiveHooks&) override { return met; }
  uint8_t met;
};
std::unique_ptr<ObjectiveGroup> group() {
  auto g = std::make_unique<ObjectiveGroup>("Protect the camp");
  g->sound = 7, g->fail_sound = 8;
  g->items.push_back(std::make_unique<Flag>(0));
  g->fails.push_back(std::make_unique<Flag>(0));
  return g;
}
}  // namespace

TEST(pm_objective_group_fails) {
  Hooks h;
  auto g = group();
  EXPECT(!g->check_group(h, 0) && !g->done);
  EXPECT(h.voices.empty() && h.messages.empty());
  static_cast<Flag*>(g->fails.front().get())->met = 1;
  EXPECT(g->check_group(h, 0) && g->done && g->failed);
  EXPECT(h.voices.size() == 1 && h.voices[0] == 8);
  EXPECT(h.messages.size() == 1 && h.messages[0] == 0x265);
}

TEST(pm_objective_group_won_is_not_failed) {
  Hooks h;
  auto g = group();
  static_cast<Flag*>(g->items.front().get())->met = 1;
  EXPECT(g->check_group(h, 0) && g->done && !g->failed);
  EXPECT(h.voices.size() == 1 && h.voices[0] == 7);
  EXPECT(h.messages.size() == 1 && h.messages[0] == 0x263);
}

// A failed condition doesn't count toward its group (0x004600d0).
TEST(pm_objective_failed_condition_does_not_count) {
  Hooks h;
  auto g = group();
  auto* c = static_cast<Flag*>(g->items.front().get());
  c->met = 1, c->failed = 1;
  EXPECT(!g->check_group(h, 0) && !g->done);
}

// 0x00425970: a loadout's items draw on the store in turn; two of a kind share what there is.
TEST(pm_config_from_a_short_store) {
  Config c(0, "test");
  auto* a = new AmmoItem();
  a->weapon = 5, a->count = 15;
  auto* b = new AmmoItem();
  b->weapon = 5, b->count = 10;
  c.items.push_back(a), c.items.push_back(b);
  Store store;
  store.add_ammo(5, 12);
  std::unique_ptr<Config> got(config_copy_store("copy", c, &store, false));
  EXPECT(got != nullptr);
  int32_t total = 0;
  for (ConfigItem* i : got->items) total += i->count;
  std::printf("  store copy: total %d, items %zu\n", total, got->items.size());
  EXPECT(total == 12);
  EXPECT(!got->modified);
  c.items.clear();
}

// A clutter piece's break (0x00513c60, 0x00513cf0) finds and starts its anim: both hooks were left
// unset, so a piece walked into or shot just vanished (docs/pm-map.md, cls_clutter).
TEST(pm_clutter_break_has_its_anim) {
  mech3::wire_weapons([](const char*) -> uint32_t { return 0; });
  const pm::zcls::ClutterHooks h = pm::zcls::clutter_hooks();
  EXPECT(h.anim_find != nullptr && h.anim_start != nullptr && h.watch_hits != nullptr);
  mech3::unwire_weapons();
  EXPECT(pm::zcls::clutter_hooks().anim_find == nullptr);
}

// A weapon's light (zWeapon's hooks, Pirate's Moon 0x005a8767..0x005a8855) goes through PM's own setters: its colour
// mixed by its ambient and diffuse into pm_colors (0x0050f460), its range (0x0050edd0). MW3's colour setter left the
// mixes as the light was made (white).
TEST(pm_weapon_light_takes_pms_setters) {
  mech3::wire_weapons([](const char*) -> uint32_t { return 0; });
  const zwep::RunHooks& r = zwep::run_hooks();
  EXPECT(r.light_create && r.light_color && r.light_range && r.light_position);
  if (r.light_create && r.light_color && r.light_range && r.light_position) {
    zwep::Handle h = r.light_create();
    auto* n = static_cast<zcls::Node*>(h);
    auto* l = n ? static_cast<zcls::Light*>(n->data) : nullptr;
    EXPECT(l != nullptr);
    if (l) {
      r.light_color(h, 1.0f, 0.5f, 0.25f);
      r.light_range(h, 60.0f, 10.0f);
      r.light_position(h, 1.0f, 2.0f, 3.0f);
      const float amb = l->get<float>(0xa0), dif = l->get<float>(0x9c);  // as made: 1 and 0
      std::printf("  weapon light: ambient %.2f diffuse %.2f; mixed", amb, dif);
      for (float v : l->pm_colors) std::printf(" %.3f", v);
      std::printf("; range %.1f..%.1f\n", l->get<float>(0xb4), l->get<float>(0xb8));
      EXPECT(l->pm_colors[6] == 1.0f * amb && l->pm_colors[7] == 0.5f * amb && l->pm_colors[8] == 0.25f * amb);  // ambient mixed
      EXPECT(l->pm_colors[9] == l->pm_colors[3] + l->pm_colors[6] && l->pm_colors[11] == l->pm_colors[5] + l->pm_colors[8]);  // summed
      EXPECT(l->get<float>(0xb4) == 10.0f && l->get<float>(0xb8) == 60.0f && l->get<float>(0x10) == 2.0f);
    }
    if (n) zcls::node_delete(n);
  }
  mech3::unwire_weapons();
}

// Pirate's Moon 0x00474f80: the chassis by +0x5ae alone, so a head shot's mech comes and one the torso's
// loss or kill() cleared doesn't.
TEST(pm_salvage_takes_the_chassis_by_its_flag) {
  SalvageMech m;
  m.has_data = true, m.type = 3;
  SalvageCalls c;
  c.rand = [] { return 0; };
  Store a(1), b(1);
  add_salvage(a, m, c);
  EXPECT(a.count() == 1);
  m.chassis = false;
  add_salvage(b, m, c);
  EXPECT(b.count() == 0);
}
