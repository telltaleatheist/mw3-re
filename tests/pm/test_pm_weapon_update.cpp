// Pirate's Moon's weapon manager frame (0x004b4a90, the port's WeaponManager::update): nothing without weapons; the
// fire mode spoken a second after the key (0x004b4690 / 0x004b46c0); in chain fire a failed first trigger stays held
// when the current weapon is off (+0x78), and a release refreshes the HUD when it is (docs/pm-tests.md).
#include <vector>

#include "check.h"
#include "mech3/weapon.h"

namespace {
std::vector<int32_t> g_sounds;
int g_huds = 0;

struct Scripted : mech3::Weapon {
  explicit Scripted(mech3::WeaponManager* m) : mech3::Weapon(&def_, nullptr, m, true) {}
  static inline mech3::WeaponDef def_{};
  bool fires = false;
  int triggers = 0;
  bool is_beam() const override { return false; }
  int32_t ammo() const override { return 5; }
  bool trigger() override { return ++triggers, fires; }
  void release() override {}
  void held() override {}
  void update(float) override {}
};

// One manager, one group of two weapons, the hooks caught; everything put back after.
struct Bench {
  mech3::WeaponCalls saved_calls = mech3::weapon_calls();
  mech3::WeaponWorld saved_world = mech3::weapon_world();
  mech3::WeaponManager m{nullptr};
  Scripted a{&m}, b{&m};
  int player = 0;
  Bench() {
    g_sounds.clear(), g_huds = 0;
    mech3::weapon_calls().mech_sound = [](int32_t i) { g_sounds.push_back(i); };
    mech3::weapon_calls().hud_groups = [](mech3::Handle) { ++g_huds; };
    mech3::weapon_world().player = &player;
    m.owner = &player;
    m.weapons[0] = &a, m.weapons[1] = &b, m.count = 2;
    mech3::WeaponGroup* g = m.groups[0];
    g->weapons[0] = &a, g->weapons[1] = &b, g->count = 2, g->current = 0, g->current_weapon = &a;
    m.group = 0;
  }
  ~Bench() {
    m.count = 0;
    mech3::WeaponGroup* g = m.groups[0];
    g->count = 0, g->current_weapon = nullptr;
    a.bin = b.bin = nullptr;
    mech3::weapon_calls() = saved_calls;
    mech3::weapon_world() = saved_world;
  }
};
}  // namespace

TEST(pm_weapon_frame_does_nothing_without_weapons) {
  Bench t;
  t.m.count = 0;
  t.m.state = 7, t.m.trigger_down = true, t.m.mode_until = 1.0f;
  mech3::weapon_world().clock = 5.0f;
  t.m.update(0.03f);
  EXPECT(t.m.state == 7 && t.m.trigger_down && t.m.mode_until == 1.0f && g_sounds.empty() && t.a.triggers == 0);
}

TEST(pm_weapon_mode_spoken_a_second_on) {
  Bench t;
  for (const int mode : {0, 1, 2}) {
    g_sounds.clear();
    t.m.mode = mode, t.m.mode_until = 5.0f;
    mech3::weapon_world().clock = 4.5f;
    t.m.update(0.03f);
    EXPECT(g_sounds.empty() && t.m.mode_until == 5.0f);
    mech3::weapon_world().clock = 5.5f;
    t.m.update(0.03f);
    const int want = mode == 0 ? 0x16 : mode == 1 ? 0x14 : 0x15;
    EXPECT(g_sounds.size() == 1 && g_sounds[0] == want && t.m.mode_until == 0.0f);
    t.m.update(0.03f);
    EXPECT(g_sounds.size() == 1);
  }
}

TEST(pm_chain_fire_holds_a_failed_trigger_on_an_off_weapon) {
  for (const int mode : {0, 1}) {
    for (const bool enabled : {true, false}) {
      Bench t;
      t.m.mode = mode;
      t.a.enabled = enabled, t.a.fires = false;
      t.m.trigger_down = true, t.m.trigger_was = false;
      t.m.update(0.03f);
      const bool kept = mode == 0 && !enabled;
      EXPECT(t.m.state == 1 && t.m.trigger_down == kept);
    }
  }
}

TEST(pm_chain_release_refreshes_the_hud_for_an_off_weapon) {
  for (const bool enabled : {true, false}) {
    Bench t;
    t.m.mode = 0;
    t.a.enabled = enabled;
    t.m.trigger_down = false, t.m.trigger_was = true;
    t.m.update(0.03f);
    EXPECT(t.m.state == 3 && g_huds == (enabled ? 0 : 1));
  }
}
