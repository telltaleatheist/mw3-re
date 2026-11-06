// PM's objectives can fail: a group's `fails` (0x004600d0), its own sound and message (0x0045fcf0), no salvage.
#include <memory>

#include "check.h"
#include "mech3/mech_objective.h"

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
