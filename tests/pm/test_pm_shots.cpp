// Pirate's Moon's shot frame stamp (test_pm_retire.py checks the retire against the exe): a shot moves by 1e-6 s on
// the frame it was made (0x005a79a0's [0x005f11bc]) and by the frame's time after; a dead shot is held one frame in
// its weapon's list by the commit (0x005a7570) and goes on the next.
#include <cmath>

#include "check.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zweapon/zweapon.h"

TEST(pm_shot_first_frame_moves_a_millionth) {
  zwep::WeaponDef def{};
  zwep::Projectile p{};
  p.speed = p.max_speed = 100.0f;
  p.velocity = {0, 0, 100.0f};
  zwep::hit_state().dt = 0.033f;
  zcls::frames().frame = 40;
  p.frame_stamp = 40;  // made this frame
  zwep::Vec3f3 move{};
  zwep::step(&def, &p, &move);
  EXPECT(std::fabs(move.z - 100.0f * 1e-6f) < 1e-9f);
  zcls::frames().frame = 41;  // the next
  zwep::step(&def, &p, &move);
  EXPECT(std::fabs(move.z - 3.3f) < 1e-4f);
  zcls::frames() = zcls::Frames{};
}

TEST(pm_shot_dead_is_held_a_frame) {
  zwep::RunHooks& h = zwep::run_hooks();
  const zwep::RunHooks kept = h;
  h = zwep::RunHooks{};
  zwep::State& s = zwep::state();
  zwep::Projectile* free_before = s.free_list;
  zwep::WeaponDef def{};
  zwep::Projectile p{};
  p.state = 0;
  p.frame_stamp = 7;
  def.active = &p;
  zcls::frames().frame = 10;
  zwep::commit(&def);  // stamped with frame 11, kept
  EXPECT(def.active == &p && p.frame_stamp == 11 && s.free_list != &p);
  zwep::update(&def);  // the frame's update skips it (0x005a73c0): it stays dead where it ended
  EXPECT(p.state == 0);
  zcls::frames().frame = 11;
  zwep::commit(&def);  // its frame: gone
  EXPECT(def.active == nullptr && s.free_list == &p);
  s.free_list = free_before;
  h = kept;
  zcls::frames() = zcls::Frames{};
}
