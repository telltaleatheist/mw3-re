// Pirate's Moon's basic vehicle damage (0x004b0d90; docs/pm-tests.md): every hit takes its damage. MW3's (0x004ad3f0)
// warmed the glow by a heat weapon's share, only chilled for a freeze weapon, and added x08 x health once frozen.
#include "check.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/veh_basic_object.h"

namespace {
struct Hooks : mech3::VehBasicHooks {
  uint32_t flags = 0;
  int glows = 0, healths = 0;
  uint32_t weapon_flags(mech3::Handle) override { return flags; }
  int32_t glow_hit(mech3::Block398&, mech3::Handle, mech3::Handle, float) override { return ++glows, 0; }
  void hit_health(float) override { ++healths; }
  void die(mech3::BasicVehicle&) override {}
  bool controls_destroyed(mech3::VehicleControls&) override { return false; }
};
}  // namespace

TEST(pm_vehicle_takes_every_hit_as_damage) {
  Hooks hooks;
  mech3::set_veh_basic_hooks(&hooks);
  int weapon = 0;
  const struct {
    uint32_t flags;
    uint32_t frozen;
  } cases[] = {{zwep::kTargetHeat, 0}, {zwep::kTargetHeat | zwep::kFreeze, 0}, {0, 1}};
  for (const auto& k : cases) {
    mech3::BasicVehicle v;
    mech3::VehicleControls c;
    v.controls = &c;
    v.active = true;
    c.health_max = 100.0f, c.health = 100.0f, c.frozen = k.frozen;
    c.x398.x08 = -0.5f;
    hooks.flags = k.flags, hooks.glows = 0, hooks.healths = 0;
    EXPECT(mech3::vehicle_damage(v, &weapon, 0, 3.0f) == 0);
    EXPECT(c.health == 97.0f);   // the damage, no more, no less
    EXPECT(hooks.glows == 0);    // no glow warmed or chilled
    EXPECT(hooks.healths == 1);  // the health's glow step (0x005a5150)
    EXPECT(c.frozen == k.frozen);
    v.controls = nullptr;
  }
  mech3::set_veh_basic_hooks(nullptr);
}
