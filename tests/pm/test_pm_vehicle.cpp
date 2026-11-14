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

#include "gamez/zclass/cls_di.h"
#include "mech3/veh_move.h"

namespace {
// One crossing on the first segment, on `node`; a mover (a vehicle's or mech part's node) or another entity.
struct Bumps : mech3::VehBasicHooks {
  int node = 0, responses = 0;
  bool mover = false;
  void segments(float*, int32_t points, zcls::SegmentHits* out) override {
    for (int32_t i = 0; i < points / 2; ++i) out[i].count = 0;
    out[0].count = 1;
    out[0].hit[0] = zcls::SegmentHit{};
    out[0].hit[0].normal[2] = 1.0f;
    out[0].hit[0].node = reinterpret_cast<zcls::Node*>(&node);
  }
  void node_set_intersect(mech3::Handle, int32_t) override {}
  mech3::Handle hit_mover(mech3::Handle n) override { return mover && n == &node ? &node : nullptr; }
  mech3::Handle hit_object(mech3::Handle n) override { return n == &node ? &node : nullptr; }
  bool object_active(mech3::Handle) override { return true; }
  void wall_response(mech3::BasicVehicle&, const mech3::CollisionHit&) override { ++responses; }
  uint32_t frame() override { return 0; }
};
}  // namespace

// 0x004a3cb0 / 0x004a3200: a vehicle that runs into a mech or another vehicle is flagged as hit with no response of
// its own (and, with nothing else hit, none at all: the exe's response read an empty list); another entity is answered.
TEST(pm_vehicle_bumping_a_mover_is_flagged_not_answered) {
  for (const bool mover : {true, false}) {
    Bumps hooks;
    hooks.mover = mover;
    mech3::set_veh_basic_hooks(&hooks);
    mech3::BasicVehicle v;
    mech3::VehicleControls c;
    v.controls = &c;
    c.x70.z = 1.0f;  // moving: the sweep queries
    mech3::vehicle_collisions(v);
    EXPECT(c.hit_object == 1);
    EXPECT(hooks.responses == (mover ? 0 : 1));
    EXPECT(c.movers.empty() && c.objects.empty());
    v.controls = nullptr;
    mech3::set_veh_basic_hooks(nullptr);
  }
}
