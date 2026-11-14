// Pirate's Moon's AI volley pick (0x004b56d0 with 0x004b5230, 0x004b5330): a weapon is ready for it when it has a
// definition (0x004b8ce0); MW3 also waited for the weapon type's AI wait (0x004b4c70, the type's +0xc4).
#include <cstdio>

#include "check.h"
#include "mech3/weapon.h"
#include "mech3/weapon_planner.h"
#include "mech3/zai_wire_pm.h"

namespace {
struct Unlimited : mech3::Weapon {
  using mech3::Weapon::Weapon;
  int32_t ammo() const override { return -1; }
};
int32_t high() { return 32767; }
}  // namespace

// A weapon whose type is still waiting (ready_at past the game time) is taken: its group is picked (2). The same
// weapon with the type ready is taken too, and one that can't fire (its own reload) is not. Under PM_AI=0 (MW3's
// AI) the waiting type's is not, as MW3's.
TEST(pm_volley_counts_a_waiting_type) {
  for (const int k : {0, 1, 2}) {
    mech3::WeaponType type{};
    type.heat = 0.1f, type.range = 1000.0f, type.range_sq = 1e6f;
    type.ready_at = k == 0 ? 1e9f : 0.0f;  // k 0: the type waits
    zwep::WeaponDef def{};
    def.type = &type;
    def.flags = zwep::kBeam;
    mech3::WeaponManager m(nullptr);
    Unlimited w(&def, nullptr, &m, true);
    w.rating = 5.0f;
    w.reload = k == 2 ? 1.0f : 0.0f;  // k 2: the weapon itself reloads
    m.weapons[0] = &w, m.count = 1;
    m.groups[0]->add(&w);
    w.group = 0;
    const mech3::VolleyPilot pilot{1.0f, 0.0f};
    const float d2 = 100.0f, heat = 0.0f;
    const int32_t r = mech3::volley_pick(m, &pilot, &d2, &heat, high);
    std::printf("  case %d: %d, mode %d\n", k, r, m.mode);
    EXPECT(r == (k == 2 || (k == 0 && !mech3::ai::pm_ai_on()) ? 0 : 2));  // PM_AI=0: MW3's wait
    m.groups[0]->remove(&w);
    m.weapons[0] = nullptr, m.count = 0;
  }
}
