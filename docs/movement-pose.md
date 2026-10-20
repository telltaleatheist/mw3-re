# The pose on the parts

Mech3.exe 1.2, 2026-10-20 (part of `movement.md`). How a sampled animation
becomes the parts' node matrices. Ported in `mech3/mech_pose.cpp`, checked
by `test_pose.py`.

## set_pose (part vtable +0x2c)

- Base 0x460280: the node's translation (+0x18) and rotation (+0x14, a quaternion
  turned into the 3x3 by 0x525740), then the node set (vt +0x14, 0x460180 =
  0x4fc300). A pending pose (+0x4a, +0x4c position, +0x58 quaternion) wins once
  over the one given; the arguments only say which halves to set.
- `place` 0x460210: makes the pose pending and runs the base set_pose, so the
  subclasses' overrides are skipped (the one-shots use it for every part).
- Hip 0x4624b0: adds its offset +0x98 (y is the crouch), keeps the rotation
  (+0x74) and its inverse (+0x84), and runs both feet's ground update 0x462a10.
- Foot 0x4629b0: the translation at once, the rotation into +0x58 for the
  foot's own update.
- Torso 0x464600: only the rotation, into +0x128 for the torso's update. In the
  cockpit view the player's torso keeps a tenth of the animation's sway
  (0x525be0 scales the quaternion's angle by 0.1), taken out of the hip's turn.

## The leg IK (0x463e40, solve 0x463c90)

The ankle target, less the hip offset, in the hip's frame. Two bones in the
leg's plane: `c = (d^2 - a^2 - b^2) / 2ab` (+0xcc, +0xd0, set up from the
rest pose by 0x4637b0, not ported), clamped to 1 with the target pulled in
along the line to it when out of reach; knee `atan2(+-sqrt(1 - c^2), c)` (the
sign by +0x7c, the knee's direction), thigh pitch `atan2(z, y) - atan2(b s,
a + b c)`, and a roll about z toward the foot's side `atan2(x - x0, y)`
([0x5e4f1c] = 1, never changed). The thigh and calf get their quaternions,
the foot the target and the rotation.

## The three poses

- Gait (Move 0x417a80 from 0x417bbc): the feet sampled in their skis' frames
  (anim vt +0x1c with the ski nodes' matrices), hip (z dropped), feet, torso
  rotation; during the blend-in (walk or faster, no jets, the first 20 frames)
  each foot, the hip, the torso and the root motion go from the last pose to
  the animation's by `clamp(frames / 20, 0.15, 1)` (with a crouch, the feet and
  hip keep their height). The left foot's rotation is blended twice (from each
  foot's last) and the right's never: a bug, fixed behind `zfix bugs` (each
  foot from its own; fixes.md). Then
  0x475f20 (a foot lift to the skis, whose switches [0x6084f0] / [0x6084f4]
  nothing sets: it does nothing), the IK per leg, the arms and hands (not
  when the arm is destroyed), the toes (detail bit 1), the hip, the torso.
- Jump (JumpJets from 0x4177e7, while firing): the jump pose or, without one
  (the mech data never loads slot 12), the stand, not advanced: IK, toes, hip,
  torso.
- Actions (ActionUpdate from 0x416fb8): every part placed straight from the
  one-shot's keys, no IK: the hip at (0, root y, 0), feet, torso, thighs
  (0x4210d0), calves (0x421080), arms, hands, toes.

The toe loops use the left foot's toe count for both feet and let the right
foot's index reach its own count (`ja`), past its arrays when it has fewer
toes: FIX (bug), stopped at the count (the probes keep the exe's check).

## Left

The part updates the poses feed (torso 0x464e80 / 0x464ce0, hip 0x4623a0,
arms 0x466510, feet 0x462a10 / 0x463110), the leg setup 0x4637b0 and the hip
setup 0x462130.
