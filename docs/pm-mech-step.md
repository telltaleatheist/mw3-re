# PM's mech step, measured (2026-11-05)

Every function the mech step reaches on PM: from the port's mech files, the mech's vtable and Mech::Update (0x41ae00), walking
direct calls, in PM's mech ranges 0x414000..0x42a000 and 0x460000..0x47a000. Direct calls only, so a part's vtable method is
here only if the port names it or something calls it; engine and AI code is out of scope.

| | functions |
|---|---:|
| identical (bytes modulo relocations) | 64 |
| changed\* (no call, string, constant or import added; pseudocode 90% alike or more) | 122 |
| changed (one of those added or removed, or pseudocode under 90% alike) | 242 |
| new in PM | 89 |
| total | 517 |

331 to read (changed and new); 138 of them have no port at all; 9040 instructions in those.
107 of the 517 run in the frame, reached from Update, Move, jets, landing and 0x423590.
"Tested" means a `test_pm_*` names the address: not that it proves the function; 40 of the 331 are named.

**How far to trust this.** The 90% is a threshold I chose, not a measured one: changed\* can hide a changed constant under 0x100
or a field's meaning, and "changed" includes compiler rearrangements Ghidra did not normalise. The earlier sample of 30 (pm-map, The
categories) found about 1 in 15 of the "compiler only" pairs changed logic. Instruction counts come from the diff tool's sizes, except
for new functions, counted from the listing. *Depth* is calls from the frame's roots; *callers* are direct call sites.
MW3 functions the port has with no PM twin (69 or so: inlined or rewritten) are not rows; their callers are.

**"New" overstates it.** Reading Update showed some are MW3's inline code moved into a function: 0x4186f0 is the lean and 0x423590 the orientation step, both inline in MW3's Update, with PM's real change being when they run (ahead of collision, committed after it). Done so far: the loadouts (`test_pm_config`), jets 0x417f50 with the fuel moved into Update (`test_pm_jets`).

**Done, on `pm` (2026-11-05):** Update's order is PM's, with predict 0x423590, the lean 0x4186f0, the landing 0x416d70, the skis 0x477f90 and the whole of collision 0x423730 on the prediction (`test_pm_update`, `test_pm_jets`, `test_pm_skis`, `test_pm_collide`). Nothing in this group is temporary any more. Standing on something (0x4e2e30, its weight) is called through a hook left unset until that function is ported. Per-frame amounts checked: the fuel, the jet assist and predict use dt; the throttle's 0.6 under a push and the collision's amounts were per frame and go by time behind `per_second` (`fixes.md`, `tests/pm/test_pm_collide_rate.cpp`). Crushing needs an owner's +0x08: only vehicles have one in the port, so a dead non-vehicle owner still counts as standing.

**Parts and deaths (2026-11-05):** `test_pm_parts.py` and `test_pm_deaths.py`, moved from MW3's. What they changed: the arm's aim 0x4681b0 clamps what it heads for before easing; the torso's mode 0x467a90 gives anyone but the mission's player mode 0; the part aim 0x41e4d0 first eases a new vector at mech +0x100 toward the target's velocity (rate from the AI record's +0xb8 through the controls' vt+0x88, y at 0.25, landing within 0.05), reset by 0x41e7e0 when the target changes; nothing found in the exe reads it, by direct offset, `lea`/`add` of the base, or a getter, so `aim_skill` is unset in play until PM's AI and 0x41e7e0's reset is not ported. The dead mechs are rewritten: the step 0x433020 goes by game type (1 and 5 the wreck's own opacity 0x41d730; 2 instant action, fading from the first frame with +0x658; 3 network), 0x432e80 also sets flag 8 by opacity and is safe with no node, the tally moved to 0x40ce90, and 0x4332e0 commits the predicted move whole instead of MW3's 0x5a8000. The step works out an opacity for 0x432e80, which ignores it (with its 0.25 cap): dead code, left out. zEffect's 0x51f650 also checks a connector's two ends. Black Hawk and Owens: 0x468a40 checks the arm first. Mech +0x658: bit 0 is 0x41d8b0, called from the view code (0x42297a, 0x4b2be5) and no longer cleared at death; bit 1 is toggled by 0x41d8f0 through a jump at 0x40bb1e, caller not traced. So the thinning 0x41d730 looks like the player's own wreck's, not an AI's. Seen in shots (2026-11-05, debug build, c1m1, default cockpit view, `--mech-ahead 80`, the target killed through the destroy valve's Mech::kill at frame 40 at 33 ms): instant action's fade with the time shortened to 4..14 s draws, thinning to nothing and the target dropping; the thinning with +0x658 forced runs 3.0 a second at both 33 ms and 7 ms a frame, and draws. #22 0x428a00 matches the port's list, all 103 ids in order.

**The moved tests' offset maps, audited (2026-11-05).** A probe that maps the exe's offsets onto the port's fields can test a wrong mapping as correct, so each pair is checked against the exes instead of the map. The pairs are every offset changed between a test or probe on `main` and on `pm`, with the tests' `FIELDS` dicts, in heat, jets, update, skis, collide, gait, health, actions, damage and parts. *Matched* means the two exes' matched functions, lined up by instruction shape, hold an instruction with the MW3 offset opposite one with the PM offset (count, then an example MW3 / PM address, in that field's module where there is one). *Read* means the exe's own use was read directly. The table below was generated, then the unmatched and shared rows gone through by hand.

Result: 209 pairs, 168 matched, 10 read, 31 explained (no field at all, such as the test's allocation sizes and image slots, or vtable slots read in the code). None rests on the map alone. What the audit and this group's tests found wrong in the maps: the knee byte, MW3 +0x585, is PM +0x5aa, not the +0x5a9 the block rule gives (+0x5a9 is unused; the parts test's real leg setup caught it); the two water probes (`surface[1]`, `[4]`, found by reading the heat code, fixed in 138d6ffb). The limits: a matched instruction proves the offset's image, not the field's name, which rests on MW3's own tests; and a pair lined up wrongly by the extraction can appear (0xac / 0x5ae is one, marked).

<details><summary>The pairs</summary>

| MW3 | PM | where | field | proof |
|---|---|---|---|---|
| 0x28 | 0x2c | parts,skis |  | stride of a ski contact: both exes' loops, 0x5cbac0 |
| 0x2c | 0x28 | damage |  | not a field: the game object's vt slot (0x47b727) |
| 0x5c | 0x60 | collide |  | matched 2: 0x423855 / 0x424e61 |
| 0x5c | 0x64 | actions |  | vt slot remote, 0x4172be |
| 0x70 | 0x7c | parts |  | read: the controls' mech, vbase+0x20 at 0x4283ff (PM) against vbase+0x14 at 0x426792 (MW3) |
| 0x74 | 0x90 | parts |  | not a field: the test's allocation of the controls |
| 0x7c | 0x84 | damage,parts | t.half_width | matched 11: 0x46439d / 0x465b74 |
| 0x80 | 0x88 | damage,parts | t.half_depth | matched 7: 0x4643ac / 0x465b86 |
| 0x84 | 0x8c | parts | t.mode | matched 3: 0x464ea1 / 0x466741 |
| 0x88 | 0x90 | parts | t.aim_local | matched 4: 0x465add / 0x4673ae |
| 0x8c | 0x94 | parts | t.aim_dir | matched 7: 0x465aea / 0x4673bb |
| 0x94 | 0x8c | damage |  | vt slot, 0x41ba6f |
| 0x98 | 0xa0 | parts | t.facing | matched 1: 0x464037 / 0x4657c3 (other module) |
| 0x9c | 0xa4 | parts | t.aim | matched 2: 0x464eaf / 0x46674f |
| 0xa0 | 0xa8 | parts | t.absolute | matched 4: 0x4651a6 / 0x466a42 |
| 0xa1 | 0xa9 | parts | t.pitch_lock | matched 6: 0x464ad6 / 0x466366 |
| 0xa2 | 0xaa | parts | t.view_aim | matched 6: 0x464c94 / 0x466554 |
| 0xa4 | 0xac | parts | t.view_q | matched 5: 0x464cb2 / 0x466572 |
| 0xa8 | 0xa0 | damage |  | vt slot, 0x41bfac |
| 0xac | 0x5ae | heat |  | **false pair from lining up two lines**: `in_water` stays +0xac (foot), `salvage_mech` +0x5ae read at 0x467800 |
| 0xb4 | 0xbc | parts | t.look_yaw_angle | matched 4: 0x464cba / 0x46657a |
| 0xb8 | 0xc0 | parts | t.look_pitch_angle | matched 4: 0x464cc2 / 0x466582 |
| 0xbc | 0xc4 | parts | t.twist_in | matched 5: 0x4650a5 / 0x466946 |
| 0xc0 | 0xc8 | parts | t.pitch_in | matched 9: 0x46505b / 0x466900 |
| 0xc4 | 0xcc | parts | t.look_yaw | matched 4: 0x4650cb / 0x46696c |
| 0xc8 | 0xd0 | parts | t.look_pitch | matched 5: 0x4650de / 0x46697f |
| 0xcc | 0xd4 | parts |  | range of the torso's look floats; ends proved at +0xbc and +0xd8 |
| 0xd0 | 0xd8 | parts | t.kick | matched 5: 0x465660 / 0x466f1e |
| 0xd4 | 0xdc | parts | t.kick_twist | matched 4: 0x46567b / 0x466f39 |
| 0xd8 | 0xe0 | damage,parts | t.jolt_spin | matched 4: 0x464be0 / 0x4664a0 |
| 0xdc | 0xe0 | actions,heat,parts,update | m.power_down_pending | matched 8: 0x41cc53 / 0x41dad7 |
| 0xdc | 0xe4 | damage | t->jolt_spin.y | matched 1: 0x464beb / 0x4664ab |
| 0xdd | 0xe1 | heat,update |  | matched 8: 0x41cc33 / 0x41dab3 |
| 0xde | 0xe2 | actions,gait,heat,parts,update | m.powered_down | matched 11: 0x426974 / 0x4285e4 |
| 0xe0 | 0xe4 | damage,update |  | matched 1: 0x4ae5cd / 0x4b1eb3 |
| 0xe0 | 0xe8 | damage | t->jolt_spin.z | matched 1: 0x464bf4 / 0x4664b4 |
| 0xe4 | 0xe8 | damage,update |  | matched 4: 0x414e68 / 0x4157db |
| 0xe4 | 0xec | parts | t.jolt_turn | matched 3: 0x46564b / 0x466f09 |
| 0xec | 0xf0 | heat |  | matched 4: 0x41d98f / 0x41e909 |
| 0xec | 0xf8 | collide |  | matched 2: 0x42382e / 0x424e3a |
| 0xf0 | 0xf8 | parts | t.jolt_q | matched 4: 0x46572c / 0x466fe2 |
| 0xf4 | 0x100 | collide |  | matched 2: 0x4227cc / 0x423949 |
| 0xf8 | 0xfc | parts |  | matched 7: 0x414cf4 / 0x415665 |
| 0xfc | 0x10c | collide |  | matched 5: 0x41615f / 0x416bfe |
| 0x100 | 0x108 | damage,parts | t.jolt | matched 4: 0x464bc6 / 0x466486 |
| 0x100 | 0x110 | collide |  | matched 4: 0x422699 / 0x423820 |
| 0x104 | 0x10c | damage | t->jolt.y | matched 1: 0x464c19 / 0x4664d9 |
| 0x104 | 0x114 | collide |  | matched 3: 0x4226aa / 0x423831 |
| 0x108 | 0x110 | damage | t->jolt.z | matched 1: 0x464c2d / 0x4664ed |
| 0x108 | 0x118 | collide |  | matched 4: 0x4226d7 / 0x42385e |
| 0x10c | 0x114 | parts | t.jolt_pos | matched 4: 0x465787 / 0x467040 |
| 0x10c | 0x118 | parts |  | vt slot camera, 0x428416 |
| 0x10c | 0x11c | collide |  | matched 4: 0x4226ef / 0x423876 |
| 0x110 | 0x120 | collide |  | inside the collider at +0x10c, whose base is matched; no use of its own in either exe's mech code (the test clears it) |
| 0x114 | 0x124 | collide |  | matched 6: 0x4226bb / 0x423842 |
| 0x118 | 0x120 | parts | t.turn_q | matched 6: 0x464e9b / 0x46673b |
| 0x118 | 0x128 | collide |  | collider, matched 0x424941 |
| 0x11c | 0x12c | collide |  | collider, matched 0x424b55 |
| 0x120 | 0x130 | collide |  | matched 7: 0x422676 / 0x4237fa |
| 0x124 | 0x134 | collide |  | collider contacts, `lea` at 0x423781 (PM) against 0x4225fa (MW3) |
| 0x128 | 0x130 | parts | t.rot | matched 3: 0x4659d2 / 0x4672a0 |
| 0x138 | 0x140 | parts | t.pitch | matched 16: 0x464f26 / 0x4667c6 |
| 0x13c | 0x144 | parts | t.yaw | matched 15: 0x464f36 / 0x4667d6 |
| 0x140 | 0x148 | parts | t.min_pitch | matched 10: 0x465c02 / 0x4674d2 |
| 0x140 | 0x14c | collide |  | matched 16: 0x4aee75 / 0x4b2ce5 |
| 0x144 | 0x14c | parts | t.max_pitch | matched 11: 0x465be7 / 0x4674b7 |
| 0x148 | 0x150 | parts | t.max_yaw | matched 11: 0x465c1f / 0x4674f7 |
| 0x14c | 0x154 | damage,parts |  | matched 1: 0x464bfa / 0x4664ba |
| 0x158 | 0x160 | parts | t.freq | matched 1: 0x4657cb / 0x467084 |
| 0x15c | 0x164 | parts | t.damp | matched 1: 0x465825 / 0x4670de |
| 0x160 | 0x168 | parts | t.front | matched 5: 0x464f82 / 0x466824 |
| 0x160 | 0x178 | collide |  | matched 2: 0x4af58a / 0x4b34ba |
| 0x168 | 0x17c | actions,damage |  | matched 8: 0x41683a / 0x417336 |
| 0x16c | 0x174 | parts | t.flat_front | matched 3: 0x464f9d / 0x46683e |
| 0x170 | 0x184 | parts |  | vt slot, 0x466a2b |
| 0x174 | 0x17c | damage |  | matched 3: 0x4649d1 / 0x46628e |
| 0x188 | 0x190 | damage |  | matched 1: 0x464a01 / 0x4662ae |
| 0x1a0 | 0x1a8 | damage | t->side_internal[0] | matched 4: 0x46485f / 0x4660cf |
| 0x1a4 | 0x1ac | damage | t->side_internal[1] | matched 4: 0x464857 / 0x4660c7 |
| 0x1a8 | 0x1b0 | damage,parts | t->side_internal_scale[0] | matched 2: 0x464571 / 0x465d94 |
| 0x1a8 | 0x1b8 | collide |  | not a field: the end of the contact array, 0x134 + 33 x 4 |
| 0x1ac | 0x1b4 | damage | t->side_internal_scale[1] | matched 2: 0x46456b / 0x465d8e |
| 0x1b0 | 0x1b8 | gait,parts | t.centring | matched 7: 0x465100 / 0x46699c |
| 0x1b4 | 0x1c0 | actions,gait,parts | kMechNode | not a field: the test's stride. The torso is `push 0x1bc` at 0x41691a (MW3 0x1b4 at 0x415ea9), the +8 of every field past +0x7c |
| 0x1d8 | 0x1e8 | collide |  | matched 3: 0x422972 / 0x423b4b |
| 0x1f0 | 0x200 | damage |  | matched 12: 0x41acd0 / 0x41ba7b |
| 0x200 | 0x210 | damage |  | not a field: the test's allocation of the weapon def |
| 0x28c | 0x29c | collide |  | matched 1: 0x422c0e / 0x423fef |
| 0x2ac | 0x2c8 | skis |  | matched 3: 0x4763fa / 0x477d6d |
| 0x2c8 | 0x2e4 | skis |  | matched 1: 0x476d45 / 0x478687 |
| 0x2e4 | 0x300 | skis |  | matched 2: 0x476d65 / 0x4786a7 |
| 0x2ec | 0x308 | skis | s.length | matched 2: 0x47633e / 0x477c9e |
| 0x2f0 | 0x30c | parts,skis | s.half | matched 9: 0x476106 / 0x477bc4 |
| 0x2f4 | 0x310 | parts,skis | s.mid_width | matched 3: 0x475e2b / 0x4778e5 |
| 0x2f8 | 0x314 | skis | s.inv_half | matched 4: 0x4763b7 / 0x477d26 |
| 0x2fc | 0x318 | skis | s.inv_half2 | matched 3: 0x4763c1 / 0x477d30 |
| 0x300 | 0x340 | parts |  | not a field: the test's allocation of the skis |
| 0x374 | 0x384 | collide,update |  | matched 3: 0x42270e / 0x423895 |
| 0x375 | 0x385 | collide |  | matched 3: 0x42271c / 0x4238a3 |
| 0x376 | 0x386 | collide |  | matched 1: 0x4226e8 / 0x42386f |
| 0x377 | 0x387 | collide |  | matched 5: 0x4226d0 / 0x423857 |
| 0x378 | 0x388 | collide |  | matched 5: 0x414cbd / 0x41562e |
| 0x37c | 0x38c | actions,damage,parts |  | matched 73: 0x4151f9 / 0x415bd7 |
| 0x384 | 0x394 | damage |  | matched 3: 0x415198 / 0x415b8e |
| 0x38c | 0x39c | actions,damage,parts |  | matched 43: 0x414752 / 0x414fe8 |
| 0x390 | 0x3a0 | damage,parts |  | matched 37: 0x414764 / 0x414ffa |
| 0x394 | 0x3a4 | damage,parts |  | matched 24: 0x414776 / 0x41500c |
| 0x398 | 0x3a8 | damage,parts |  | matched 23: 0x414788 / 0x41501e |
| 0x39c | 0x3ac | actions,damage,parts |  | matched 78: 0x4147ac / 0x415042 |
| 0x3a0 | 0x3b0 | actions,damage,parts |  | matched 21: 0x415014 / 0x415a2c |
| 0x3a4 | 0x3b4 | actions,damage |  | matched 14: 0x41479a / 0x415030 |
| 0x3a8 | 0x3b8 | jets |  | matched 3: 0x416189 / 0x416c2c |
| 0x3ac | 0x3bc | damage |  | matched 8: 0x414488 / 0x414d40 |
| 0x3d8 | 0x3e8 | jets |  | matched 7: 0x417309 / 0x417ea9 |
| 0x3e4 | 0x3f4 | parts |  | matched 1: 0x41d816 / 0x41e764 |
| 0x3e8 | 0x3f8 | damage |  | matched 5: 0x414f4b / 0x415956 |
| 0x3ec | 0x3fc | damage |  | matched 19: 0x414758 / 0x414fee |
| 0x410 | 0x420 | damage |  | matched 7: 0x414aa3 / 0x415410 |
| 0x411 | 0x421 | damage |  | matched 4: 0x414aaa / 0x415417 |
| 0x414 | 0x424 | update |  | matched 2: 0x414d2c / 0x41569d |
| 0x418 | 0x428 | gait | m.blend_weight | matched 6: 0x418b14 / 0x4198e1 |
| 0x41c | 0x42c | parts |  | matched 17: 0x415098 / 0x415ab0 |
| 0x420 | 0x430 | gait | m.anim_index | matched 12: 0x418543 / 0x419337 |
| 0x424 | 0x434 | damage,gait |  | matched 4: 0x415109 / 0x415b21 |
| 0x458 | 0x468 | actions,damage |  | matched 7: 0x415136 / 0x415b4e |
| 0x480 | 0x490 | actions,damage |  | matched 7: 0x4167df / 0x4172d7 |
| 0x484 | 0x494 | actions |  | matched 7: 0x4167ce / 0x4172cd |
| 0x488 | 0x498 | actions,jets | m.momentum | matched 4: 0x416ebd / 0x417930 |
| 0x48c | 0x49c | jets |  | matched 3: 0x416eeb / 0x41795c |
| 0x490 | 0x4a0 | jets |  | matched 3: 0x416efd / 0x41796e |
| 0x494 | 0x4a4 | actions | m.action_root | matched 2: 0x416621 / 0x41712a |
| 0x4a0 | 0x4b0 | gait |  | read: the gait table's start, `lea` at 0x414c7c (PM) against 0x4143fb (MW3) |
| 0x4a4 | 0x4b4 | gait |  | read: inside the table, as +0x4b0 |
| 0x4bc | 0x4cc | parts | m.gait[3][1] | matched 5: 0x418238 / 0x419023 |
| 0x504 | 0x584 | parts,skis |  | not a field: size of the ground query's record; PM's stride 0x2c |
| 0x508 | 0x518 | parts | m.hip_height | matched, 0x4637d2 |
| 0x50c | 0x51c | parts | m.crouch_depth | matched 3: 0x41bdee / 0x41ca36 |
| 0x510 | 0x520 | damage |  | matched 1: 0x414adc / 0x415445 |
| 0x518 | 0x528 | damage,jets,parts | m.max_speed | matched 14: 0x4175a4 / 0x418143 |
| 0x520 | 0x530 | gait,parts | m.turn_rate | matched 5: 0x418f98 / 0x419db8 |
| 0x528 | 0x538 | gait,jets,update | m.speed_factor | matched 2: 0x414b19 / 0x415482 |
| 0x52c | 0x53c | gait | m.speed_target | matched 8: 0x414b1f / 0x415488 |
| 0x534 | 0x548 | parts | m.throttle_down | matched 3: 0x4269d1 / 0x428637 |
| 0x538 | 0x54c | gait |  | matched 1: 0x426a20 / 0x428686 |
| 0x53c | 0x550 | actions,damage,jets,update | m.knock | matched 6: 0x414ee0 / 0x415857 |
| 0x540 | 0x554 | actions,damage,jets,update | m.knock_vec | matched 6: 0x414eec / 0x415863 |
| 0x544 | 0x558 | damage,jets,update |  | matched 1: 0x41ab2c / 0x41b8b9 |
| 0x548 | 0x55c | damage,jets,update |  | matched 1: 0x41ab3a / 0x41b8c5 |
| 0x54c | 0x560 | jets,update |  | read: 0x418791 (PM) against 0x41a480 (MW3), the lean |
| 0x554 | 0x578 | gait,heat | m.heat | matched 25: 0x414dcf / 0x415742 |
| 0x558 | 0x57c | damage,heat |  | matched 14: 0x414df2 / 0x415765 |
| 0x55c | 0x580 | heat |  | matched 6: 0x4193f2 / 0x41a263 |
| 0x560 | 0x584 | jets |  | matched 1: 0x41738f / 0x417f2b |
| 0x564 | 0x588 | jets |  | matched 2: 0x415a0a / 0x4163ce |
| 0x568 | 0x58c | jets |  | matched 1: 0x414b49 / 0x4154b8 |
| 0x570 | 0x594 | damage,update |  | matched 6: 0x414b4f / 0x4154be |
| 0x574 | 0x598 | heat |  | matched 4: 0x414b5a / 0x4154c9 |
| 0x578 | 0x59c | actions,damage,gait,jets,update | m.anim_state | matched 35: 0x41b666 / 0x41c3f4 |
| 0x57c | 0x5a0 | actions,damage,gait,jets,update | m.prev_state | matched 18: 0x414b66 / 0x4154d5 |
| 0x580 | 0x5a4 | heat,jets,update |  | matched 4: 0x414e0e / 0x415781 |
| 0x584 | 0x5a8 | actions,damage,gait,jets,parts,update | m.no_power_down | matched 9: 0x41cc49 / 0x41dac9 |
| 0x585 | 0x5aa | parts | m.knee_back | matched 2: 0x415e75 / 0x4168e5 |
| 0x586 | 0x5ab | gait,parts | m.crouched | matched 5: 0x4184e4 / 0x4192ed |
| 0x588 | 0x5ad | gait | m.wading | matched 4: 0x41843c / 0x41921f |
| 0x58c | 0x5b0 | gait,jets | m.gait_frames | matched 3: 0x414b84 / 0x4154f9 |
| 0x590 | 0x5b4 | actions,update | m.lie_time | matched 4: 0x414b8a / 0x4154ff |
| 0x594 | 0x5b8 | actions,jets | m.impulse | matched 4: 0x414b96 / 0x41550b |
| 0x598 | 0x5bc | jets |  | matched 3: 0x41749f / 0x418040 |
| 0x59c | 0x5c0 | jets |  | impulse z: impulse at +0x5b8 proved, a Vec3 |
| 0x5a0 | 0x5c4 | actions,collide,jets | m.push | matched 6: 0x414bae / 0x415524 |
| 0x5a4 | 0x5c8 | jets |  | read: 0x424aca (PM) against 0x423547 (MW3), push y |
| 0x5a8 | 0x5cc | jets |  | matched 2: 0x41a6bb / 0x41b381 |
| 0x5ac | 0x5d0 | damage,gait,jets,update | m.limp | matched 7: 0x414cee / 0x41565f |
| 0x5b0 | 0x5d4 | update |  | matched 1: 0x414bd5 / 0x415549 |
| 0x5c4 | 0x5e8 | jets |  | matched 2: 0x4173cb / 0x417f64 |
| 0x5c8 | 0x5ec | parts | m.servo( | matched, 0x467b0d |
| 0x5cc | 0x5f0 | heat |  | matched 6: 0x419700 / 0x41a51a |
| 0x5d0 | 0x5f4 | heat |  | matched 4: 0x4198b3 / 0x41a705 |
| 0x5e4 | 0x608 | parts | m.sounds[11] | matched 1: 0x41be72 / 0x41cabf |
| 0x5e8 | 0x60c | parts | m.sounds[12] | matched, 0x4639e2 |
| 0x5ec | 0x610 | collide |  | matched 1: 0x422a9e / 0x423c80 |
| 0x600 | 0x624 | actions | m.sounds[0x12] | array: sounds at +0x5dc+4k, [11] proved at +0x608, [12] at +0x60c |
| 0x604 | 0x628 | actions | m.sounds[0x13] | array, as above |
| 0x608 | 0x62c | heat |  | read: 0x41abfb (PM) against 0x419d74 (MW3) |
| 0x60c | 0x630 | heat |  | matched 1: 0x419a0e / 0x41a950 |
| 0x620 | 0x644 | jets |  | matched 5: 0x4173e0 / 0x417f7d |
| 0x638 | 0x65c | actions | m.hud_fall | matched 2: 0x41ca3e / 0x41d88e |
| 0x640 | 0x650 | parts |  | not a field: the test's stack pointer below EBP |
| 0x65c | 0x680 | heat |  | matched 2: 0x414be6 / 0x41555a |
| 0x664 | 0x688 | collide | att | matched 7: 0x414ce8 / 0x415659 |
| 0x684 | 0x6a8 | actions,damage,parts |  | matched 125: 0x41446b / 0x414ce9 |
| 0x688 | 0x6ac | actions,gait,jets,parts,update | m.detail | matched 14: 0x41ccef / 0x41db67 |
| 0x68c | 0x6b0 | update |  | matched 6: 0x4151e9 / 0x415bc7 |
| 0x69c | 0x6c0 | heat |  | matched 29: 0x414e43 / 0x4157b6 |
| 0x6a0 | 0x6c4 | heat |  | matched 6: 0x414e4c / 0x4157bf |
| 0x6b0 | 0x6d4 | gait,update | m.frame_dt | matched 16: 0x414c25 / 0x415596 |
| 0x6b4 | 0x6d8 | gait |  | matched 1: 0x414c2b / 0x41559c |
| 0x6b8 | 0x728 | damage |  | matched 1: 0x417a15 / 0x4184f4 |
| 0x6c0 | 0x7e0 | collide |  | not a field: the test's own image slot past the struct |
| 0x6c4 | 0x734 | jets |  | matched 1: 0x417ae4 / 0x4188b4 |
| 0x6c8 | 0x738 | jets |  | matched 1: 0x417ade / 0x4188ae |
| 0x6d0 | 0x7f0 | collide | g->c.throttle | not a field: as above |
| 0x6d4 | 0x7f4 | collide | g->mech_node_flags | not a field: as above |
| 0x6d8 | 0x800 | actions,gait,heat |  | not a field: the test's mech image size |
| 0x6e0 | 0x800 | parts |  | as above |
| 0x700 | 0x800 | damage |  | as above |
| 0x700 | 0x900 | actions,gait |  | as above |
| 0x7d0 | 0x584 | update |  | not a field: image slots for the ground query record |
| 0x7d4 | 0x588 | update |  | as above |

</details>

**Firepoints (2026-11-05, #29 0x46ea40; #22 and #28 needed nothing, #28 is `std::vector`'s insert):** `CZMechFirePoint::CFixup` runs once for each definition. Each firepoint's aim limits gain the torso's, and on an arm's the arm's (the right mirrored), and it builds an aim cone at firepoint +0x14 (0x4a6ee0, class `CZValidateDir`): two yaw bands split at 0, a pitch band at +0x38. Ported and checked (`test_pm_firepoints.py`). Its readers, 0x4b1030..0x4b10e0, 0x4aaf40..0x4aafa0, 0x4b8f80 and 0x4a2220, are in weapons and AI code not reached yet, so nothing in play uses the cone until those are ported. The exe's clamp of a top past pi/2 compares with -pi/2, a typo: FIX (bug).

**Power (2026-11-05, #33 0x41d920):** `test_pm_power.py`, moved from MW3's. Power-down completion now turns the headlight off (0x41f8d0(0), 0x465da0) and, for a network mech, drops it as the mission player's target (vt+0xcc, 0x41e7e0, with the HUD and the +0x100 reset). The test stands in the headlight and target calls; the real ones are wired but not run in a test. The power function 0x41dab0 has no logic change, only PM's slots.

**Saved loadouts (2026-11-05, #34 0x426000):** PM's file starts with a magic (-1) and the version (0x10); a file without them is MW3's, and its mech and weapons are renumbered (0x487740, 0x487810) and its checksum made anew. The checksum's packed form moved its flags up a bit (0x4274d0). `test_pm_mechsave.py` checks saving and loading against the exe on all 113 loadouts, in both formats, with MW3's numbers across their range. It replaces only part of MW3's `test_mechsave.py`, which stays unmoved: whole archives, byte damage and the copy are not checked on PM. The archive's version (0x10, read at 0x426bd0 and 0x4267f0) rests on reading the exe. An MW3 archive, version 15, is refused at that level, so the conversion is reached only through the second loader 0x427e80, which is not ported.

**Byte damage on saved loadouts (2026-11-05):** `test_pm_mechsave` now flips bits in every loadout in both formats (1,356 cases, 559 still accepted), cuts each short, and splices a kind 4 item and a count of -1. It found what the plain loads couldn't: PM's item list 0x473930 has no kind 4 (MW3's engine item), so one makes no item and reads nothing, and a count below 1 ends the list where MW3 stopped only at 0. The port follows. PM's salvage block 0x473690 reads no `engines` either (no such string in the exe, none in PM's readers), so the port's reading of them is gone: it would have saved an item PM can't load back. Only the port runs: a name length or item count of 0x7fffffff, which crashes or hangs the exe. 42 damaged engine numbers fault in the exe at 0x433a45, past the engine table; for those the test only sees the port survive (it takes 0, FIX (A)). 18 of 18 breaks caught: the 14 owed plus the kind 4, the count, the short read's id and the order of two bytes. **Left:** 0x473690's other changes (it starts from `mechs`) are not compared; `EngineItem` stays in the port, unreachable from PM's data. #35 0x424dd0 is covered by `test_pm_collide`.

**Animation (2026-11-05, #36 0x420aa0):** `test_pm_mechanim.py`, moved from MW3's, on PM's 360 motions of 25 mechs: the diff was the compiler's inlining and calling conventions, and the port needed no change.

**The build (2026-11-05, #37 0x414cb0, the launch 0x494500):** `test_pm_mech_build.py`, moved from MW3's: launch and AI mech down the whole constructor chain, every block byte for byte, on PM's 25 mechs and their nodes. The launch: no cockpit cycles at ObjectLOD 2, game type 5's own line-up, the `dbase` under `tur_ppc02` shown in types 2 and 3 on world 0, the list's +0x10 cleared. The build: 0x4148c0 takes 0x748 bytes, the weapon manager 0x98 with a new field at +0x1c, the torso 0x1bc with the headlight at +0x7c and a field at +0x80, `PlayerControls` 0x18c with its IControls at +0x16c and a timed list (0x4b2580) only made so far, the skis 0x31c with no debug quads, the low-detail node asked for first. 0x414cb0 looks up the engine's limits and drops the answer. The test also found `mid_width` unset: PM's maker writes it at 0x477d1a, half x 0.5 (+0x310, read at 0x477851, 0x4778e5, 0x47797e). MW3 has no such write (0x476320 leaves +0x2f4 alone), so this is PM's own fix: `Skis::mid`'s FIX is gone on `pm`, and `main` stays as it is.

Not covered: the weapons and equipment loop in 0x414cb0 (0x414dbe..0x414fc8) is one stubbed call, as in MW3's test; read by hand it has MW3's structure with PM's TAG id and firepoint name offset, and it is tested when `test_weapons_run` moves. What the manager's +0x1c and the torso's +0x80 are is not traced, and the timed list's readers are in PM's AI code. Stubs and offsets for a few calls were corrected against the exe where the address map mis-paired them (0x430d10 mapped to 0x4703c0).

**Literal ids (2026-11-05):** a search of the source by pattern, plus every small constant that differs between matched instructions of the two exes, kept where it is a renumbered mech or weapon id or a count (56 places, one by one). Wrong on `pm` and fixed, each checked against the exe's own bytes in `test_pm_names` where the code can be reached: TAG 0x20 to 0x2c (a mech's TAG was added as a heavy machine gun; salvage kept TAG and skipped heavy machine guns), the chase camera's height table 0x4312b0, the lab's missile rule 0x4559e0, a mech's shown name past roster 18 in the lab and Instant Action, the lab's and the debriefing card's "no mech" 0x13 to 0x19 (a Shadow Cat never appeared in the lab's view). Not reached by a test, read only: the two shown names (now `kMechCount`).

Open from it: MW3's LRM hold is gone from PM's weapon type 0x4b34d0 (no +0xc0 written, 0x4b4c90 unmatched): `lrm_flight` stays 0, so `hold_for_flight` adds no delay; settle it when PM's AI firing is ported. `tests/test_program.cpp` (26 places), `test_pilot_names.cpp` and `test_mech_reader.cpp` hold MW3's numbers (`l.mech = 17`, the Thor); they are in `mw3tests`, not built on `pm`, and change when moved. The lancemate's hit on the player at frame 129 of the c1m1 walk waits for PM's AI.

**Health and the limp (2026-11-05):** `test_pm_health`. PM's health 0x41c740 keeps MW3's weights but, each time it is worked out, sets how bad a limp is (+0x540, 0x418540) and a limping mech's top speed from it (0x418640). The speed setter 0x414c00 now holds speed to 5..60 and can leave the gait bounds alone. A limp's start 0x41e3b0 stops MASC, and MASC 0x41eec0 will not run on a bad leg. **The script path:** PM's destroy valve 0x4f5210 does not go through 0x41de30 as MW3's 0x4e34b0 did 0x41cfa0: it calls the entity's vt+0x78, for a mech the kill 0x41de00. 0x41de30 is called from heat alone. `WiredValveHooks` had no `mech_destroy`, so a destroy valve on a mech did nothing here but log; it is wired to `Mech::kill` now. The kill is tested through the exe's valve; the wiring itself and a mission using it are not. `origin/main` lacks that override too.

**The player's controls (2026-11-05, #47 0x4299f0):** `test_pm_player.py` and `test_pm_controls.py`, moved from MW3's. PM's update leaves the inputs alone in the animation states 12 and 13 as well as when powered down; with the torso absolute (+0xa8) the twist keys go to 1, not 1.5, and the twist eases at 1/s, not 6/s (0x604734); from rest the jets' first press puts the thrust straight at gravity / 38 (vt+0x4c, 0x4287d0), what holds the mech up; the look's return multiplies in another order. The jet keys' gate is the game's vt+0x44: the option in single player, Instant Action and standalone (0x435070), a multiplayer game's own +0xa7 (0x4365f0), which the port does not have (multiplayer's item, the option stands in). The smoothing's zeroing goes through vt+0x18 (0x428910, the player's also clears the look). The tests read the game's vt+0x44 through the real 0x435070 and the torso flag as scenario data; a gravity other than 30 is not tried.

**The lance orders wait for PM's AI (2026-11-05, #48 0x46a750, #50 0x46b1b0, #96 0x46b4d0, #132 0x46af20, #141 0x46a9c0, #152 0x46a8e0):** 0x46a750 is new: for an order on the object at [0x629a2c] it sets the order's handler to 0x4fb790 and marks an AI object at +0x2cc of the lancemate's controls (bit 30 in its masks +0xc0..+0xc8, active), and clears that for any other target. That object is made and written by the AI (0x4dd34c, 0x4e60b8), and `test_lance_dispatch` is already "to move with PM AI". Read, not ported: `lance_orders.cpp` is MW3's until then, so PM's orders do not do this.

**The hit path (2026-11-05, #49 0x461bb0, #64 0x461d70, with 0x4617a0): no change from MW3.** Read against MW3's 0x460780, 0x460970 and 0x460350, and PM's constants against MW3's: the section table, the fallback to the centre, 0.0062, 0.15, the five rates and 10 are the same; what differs is arguments on the stack and field offsets. +0x1a8 and +0x1ac are the torso's side internals (MW3's +0x1a0, +0x1a4), set by the build and the damage, not by the AI. `test_pm_damage` runs these three but its random hits missed one edge test and the 0.008 boundary, so `test_pm_hit.py` aims shots across each edge from both sides and forces the roll on each side of each rate. One break in 20 is not caught, the exe's `s1 >= 0` test in `crosses`: `s1 < 0` passing the guard above means `s2 > 0`, and the next line then answers the same. Forced on both sides, marked in the test: armor, rolls, the structure values.

**Collision's rates (2026-11-05):** per hit, as the exe: a2105b11's scaling is out, by dev 2's measure on `main` (`movement-collision.md`). PM-only and not covered by that measure: the throttle's 0.6 under a push in Update, still per second behind `per_second` (a short c1m1 walk at 33 ms has it fire on a third of the frames while pinned under a roof; whether that holds at higher rates was not reached).

**The order after this table's frame rows (the lead, 2026-11-05):** the campaign items; the frame's 85 inferred rows, logic class first, with a break in the function itself naming the PM difference it covers; #20 0x424030 and #311 0x41a020; then the second campaign (type 5, `pm-map.md` 7b, read and sized first, the lead's go before porting); then the logic and new rows outside the frame.

**Open, not to lose (2026-11-05):**
- `test_pilot`'s 1,136 lines: move with the pilot and campaign screens, at the latest with the Pirate Missions, whose screens read the fifth game.
- The starting store 0x4351b0's mech and store calls are stubbed on both sides: not compared. The restored world (0x435d30) runs, not compared with the exe's.
- 0x404a20's lancemate warnings: type 5's half with the Pirate Missions, type 2's with the outside-frame rows.
- c2m3's stop at (2636, -1528): needs its own run on the walker's route. This walk stops first at a wall at (1365, -6.8, -824) by another route, which settles nothing about it.
- PM's salvage block 0x473690 against the exe (above, Byte damage).
- `test_pilot` isn't moved. It holds the store and game calls on the pilot (0x478560, 0x4788b0, the stores' add and take, saved campaigns, rename, damage). It needs a sitting: 40 addresses, 7 with no PM counterpart (0x424010, 0x435240, 0x484840 and four inside functions), and PM's campaign differs. Pointed at PM as it stands, it stops at 0x484840.
- The campaign's flag 0x10 on a record (0x4a19c0): a vehicle with it is made only if its name is in the campaign's string list (0x4356c0, filled at 0x45fcf0). The port makes it regardless. Not changed: which shipped records have it isn't looked at yet.
- PM's new 0x4095e0: its own node out of a line query from the camera, not ported or looked at further.
- 0x41f8d0's `+0xe2` on the way to the lod's light: read only.
- **Outside the frame, with the mech lab's tests:** the saved archives: done, see The mech lab. Player files: `test_pm_pilot`. `test_lab_list` and `test_lab_screen`: not moved, see The mech lab.

**The campaign items (2026-11-05):** done but type 5; `pm-map.md` step 6. Breaks on the pilot file, per function, caught: 0x479d80, 0x47af50, 0x47b2f0 (3), 0x47b610 (2), 0x435d80, 0x435e40, 0x435820 (2), 0x4356f0, 0x40e250 (2), 0x40e2b0 (2), 0x4361c0, 0x4363b0, 0x40c460 (2), 0x434780, 0x40cbb0.

## The stop in c1m1 at 33 ms (2026-11-05, was "the roof pin")

It is not the roof. Walking with `--tap 0x0b@69`, the mech ends at (4529.2, 28, -4517.0) with the throttle at 0.05..0.09 for as long as forward stays on. Measured with temporary counters, none committed.

- **The roof does nothing.** The top probe (y 38.87) runs under a roof at y 35.38, normal (0, -1, 0), from about (4518, -4385). That goes ceiling, can't crush, wall response, which returns at once on a normal with no x or z: the call counts, nothing changes. What dropped the throttle first was a real wall, normal (-0.85, 0.41, 0.32).
- **A ledge holds it.** At the stop no wall responds. The ground under the two front probes is 3.50..3.55 above the middle's, over `kStepUp` 3.5, so each ski kicks the push back 0.5 along forward (0x607a20; MW3's is 1), 1.0 a frame. Moving against that push fires Update's 0.6 (0x41b35f), and the throttle regrows about 0.011 a frame between. Slopes (ny 0.64) slow the last metres.
- **Exe or port.** All 1,043 frames of the walk through exe 0x423730 and the port on the captured segment answers: identical. `test_pm_skis` now has ledge walks, the same numbers scripted, through exe 0x477f90: 3.52 kicks, 3.4 doesn't, port equal; five breaks caught (the kick's size, the height 3.6 and 3.3, the direction, the slid flag). The 0.6's condition was read in the exe: velocity dot push below zero. Not run: the exe's own ground and segment queries at this place, so the geometry is the port's; test_pm_di compares those queries elsewhere.
- **What frees it.** Key 1 (stop): stands still. Reverse held 90 frames: back 25 m. Backspace: reverses away at -0.98. Left held 60 frames: walks off at speed, until the next wall. Jump not tested: J did nothing in this mission even on open ground, and why wasn't checked.
- **Rerun after the skis' query fix (2026-11-05):** the walks stop in the same places to the printed decimal: c1m1 33 ms (4529.3, -4517.0), 8 ms (4528.4, -4517.5), c2m3 33 ms (1368.6, -818.8). c3m6 reads (2515.5, -1636.0) before and (2515.0, -1636.2) after, 24 frames apart, so not compared exactly. So the verdict stands, and no walk shows the mech's own node changing an answer. Correction: with the tap at the same game time (frame 276) 8 ms stops here too; only 16 ms (tap 138) walks on.
- **Not tied to 33 ms.** With the heading forced at frame 60, 33, 16, 8 and 5 ms all stop at the same ledge line, z about -4517. The rate only decides who gets there: in the plain walk one hit from another mech gives -32.79 at 33 ms, -33.72 at 16, -34.13 at 8, with the same kick and hit normals of 0.071, 0.103, 0.112, and about a degree decides it. It looks inherent to a one-hit kick decayed at different steps; no same-state step-by-step comparison from t 0.84 was done. MW3's first wall was not checked.

The 0.6, frames fired in the stop (per_second on, so each firing costs 0.6^(dt/33 ms)):

| ms a frame | fired / frames | throttle held |
|---|---|---|
| 33 | 23/61, 11-12/31 | 0.05..0.09 |
| 16 | 43-44/125 | 0.06 |
| 8 | 70/250, 85/250 | 0.063..0.069 |
| 5 | 98/400, 142/400 | 0.066..0.128 (few samples) |

A real wall at fast rates (heading forced to 50, one heading only), walls a 2 s window at its worst: 33 ms 31/61, still there after 23 s; 16 ms 83/125 and 8 ms 209/251, both away after about 8 s; 5 ms 357/400, then 36..97/400, throttle 0.002..0.05, still there after 25 s. Close to MW3's fast-frame pin (contact on most frames, the throttle low), but not on every frame and with no steady 0.0198.

Reports at a plain start: c1m1's `player.cpp:727`, `:742` (0x800) and `:974` (0x200) are for the `stealth` object, which has no support or collision points. They are at a plain start in any mission, not a pirate-mission problem; left alone.

**c4's four corrupt towers: the data's own.** Every c4 mission reports `Corrupt animation loaded: radiotower1..4` (seen in c4sa1, c4sa2, c4m9). The 5 is in the shipped `zbd/c4/anim.zbd`, PM's status byte at +0xa0 of defs 1406..1409 (nodes rtwr1..4). Exe reader 0x523140 and port, all four worlds: identical, 5 on those four only, none in c1, c2, c3, where the towers of the same names have 0. PM's post-load 0x524480 reports them as the port does (read in the exe; its text and line number differ from the port's, which doesn't matter in play). MW3's four anim.zbd have no such names. So in the exe as well as here, c4's radio towers get no binding and their animations never play; c1's and c3's do. A candidate for Owen, not changed: clearing the byte would be a guess at what the tools meant.

## The true size, normalized (2026-11-05, an estimate)

The table's *changed* came from pseudocode alike-ness, which counts argument passing, field offsets and compiler rearrangement as change. Here each row's instructions are compared after mapping registers, stack slots, field offsets, moved data and calls through the address map, and reading constants by value (`norm.py`, not in the repo):

| | functions | instructions |
|---|---:|---:|
| identical (bytes) | 64 | 1219 |
| same (nothing left after mapping) | 84 | 2536 |
| reordered (the same instructions in another order) | 8 | 249 |
| integers only, 95% alike (ids, sizes, an assert's line) | 6 | 896 |
| shape (the same calls and constants, other instructions) | 131 | 8939 |
| logic at most (calls or constants differ) | 135 | 22571 |
| new | 89 | 5393 |

So of the 364 "changed" and "changed\*", at most 135 show a difference in calls or constants; 98 are layout or numbering; 131 are probably layout, unproven. Both edges are soft. By hand, of 15 read: the 3 known same (#49, #64, 0x4617a0) class as shape, not same, so *same* is small; 0x4184f0 and 0x464610 in shape are the same maths; `logic` holds real changes (0x460f00's new clamp, 0x467800) and false ones (0x474e10 and 0x473e70's differ in renumbered ids, 0x416560's in an assert's line, 0x4285d0's in one direct call made virtual). Shape can hide logic: field offsets and vtable slots are wildcards, and a new use of a constant the function already has is not seen, so a swapped field or #47's twist key 1.0 would not show by themselves.

## The frame's 107: executed is not compared (2026-11-05, an estimate)

From a hook on each row's address under `test_pm_*`, a stub's address not counted as run. **Compared** is only a test comparing that function against the exe with a break in it caught and recorded (from 2026-11-05 on; older breaks are not counted unless redone). *Ran; compared inferred*: the exe's function ran, a port of it exists, the test compares the state after; nothing shows this function's own result is seen. *Through its caller*: it ran, no port names it. *Read*: compared by reading only, as written, which can miss what a test would not. The hook sweep leaves out `test_pm_anim` (over its time cap under the hook) and `test_pm_hud_lock`, `test_pm_hud_refresh` (over 20 GB under it); *Port* means the source names the address, PM's or MW3's twin, which does not say the port has PM's logic. *Class* is the normalized comparison above.

| # | PM | Class | Instr | State | Ran under, or stubbed in |
|---:|---|---|---:|---|---|
| 8 | 0x41ae00 | logic | 805 | compared: 23 of 24 breaks caught; the jet fuel moved in from the jets, the narc timer, frame_dt | update |
| 9 | 0x477f90 | logic | 1347 | compared: 11 of 13 breaks caught; the kick of 0.5, the one probe for a remote mech in low detail | skis |
| 10 | 0x41a100 | logic | 803 | compared: 7 of 9 breaks caught; the water from the skis' surface, the attacker cleared | heat |
| 11 | 0x418850 | logic | 635 | compared: 5 of 8 breaks caught; the knock and fall moved in; the root motion only in z (x stays 0 in the test) | jets |
| 12 | 0x417f50 | logic | 410 | compared: 7 of 8 breaks caught; the thrust at once, the assist for the mission's player | jets |
| 13 | 0x418fc0 | logic | 817 | compared: 10 of 12 breaks caught; the wade line, the limp's speed, the reverse's heat slow | gait |
| 14 | 0x423730 | logic | 708 | compared: 14 of 15 breaks caught; the prediction's velocity and points, the Elemental rule | collide |
| 15 | 0x417350 | logic | 796 | compared: 13 of 13 breaks caught; the kinds zeroing the controls, the fall camera, the lie time, the momentum | actions |
| 16 | 0x425030 | logic | 280 | compared: 6 of 10 breaks caught; a low hit to the leg on its side; the no-mech branch not separated, as Collide clears the flag first | collide |
| 17 | 0x423590 | new | 153 | compared: 5 of 5 breaks caught; new: the prediction | actions,jets |
| 18 | 0x416d70 | logic | 311 | compared: 8 of 8 breaks caught; the landing on the prediction, only the player falling | actions,jets |
| 19 | 0x4253f0 | new | 197 | compared: 8 of 8 breaks caught; new: the crash by two speeds | collide |
| 20 | 0x424030 | new | 160 | compared: 5 of 5 breaks caught; new: the port empties its five lists where the exe returns them to a pool (no pool in the port); the clears | collide |
| 24 | 0x424730 | logic | 360 | compared: 9 of 10 breaks caught; the reflection against the prediction's velocity | collide |
| 25 | 0x4186f0 | new | 115 | compared: 8 of 10 breaks caught; new: the lean, the half lean in the view's mode | actions,jets |
| 33 | 0x41d920 | logic | 109 | compared: 3 of 4 breaks caught; a network mech drops the target, the state 10 pending; the headlight is a hook here | power |
| 35 | 0x424dd0 | logic | 204 | compared: 7 of 9 breaks caught; the shove and spin through a tilted matrix, the dead unit's probes | collide |
| 41 | 0x479a40 | new | 93 | compared: 4 of 4 breaks caught; new: the beam and its light | headlight |
| 42 | 0x464060 | shape | 276 | compared: 4 of 6 breaks caught; fields moved, no other change | jets,parts,pose |
| 44 | 0x424ba0 | logic | 142 | compared: 7 of 7 breaks caught; can_crush's wall, the ceiling's height | collide |
| 46 | 0x465360 | shape | 117 | compared: 4 of 4 breaks caught; fields moved, no other change | pose |
| 51 | 0x467a90 | new | 51 | compared: 2 of 2 breaks caught; new: only the mission's player takes the mode | parts |
| 53 | 0x425370 | new | 59 | compared: 6 of 6 breaks caught; new: the crash's parts | collide |
| 56 | 0x415e90 | new | 50 | compared: 1 of 1 breaks caught; new: every part's structure | health,heat |
| 62 | 0x41dab0 | shape | 67 | compared: 2 of 3 breaks caught; fields moved, no other change | power |
| 65 | 0x477ef0 | new | 32 | compared: 2 of 2 breaks caught; new: the two soils | skis |
| 69 | 0x4777f0 | shape | 60 | read: only under [0x629992], which nothing writes (no store in the exe); the port leaves the branch out, as `mech_part_update.cpp` says. Dead |  |
| 76 | 0x41c9f0 | logic | 59 | compared: 3 of 3 breaks caught; an Elemental doesn't crouch | parts |
| 78 | 0x464450 | logic | 141 | compared: 4 of 4 breaks caught; the foot on the normal, the prediction's orientation | parts |
| 82 | 0x479190 | shape | 98 | compared: 2 of 4 breaks caught; fields moved, no other change | skis |
| 86 | 0x419de0 | logic | 143 | compared: 3 of 5 breaks caught; the creep, the gait's thresholds | gait |
| 98 | 0x41c3f0 | shape | 65 | compared: 3 of 3 breaks caught; fields moved, no other change | actions,damage,hit,jets |
| 99 | 0x424d80 | new | 36 | compared: 3 of 3 breaks caught; new: the expiry and owner | collide |
| 103 | 0x4790c0 | new | 23 | read: three products, inline at its caller | skis |
| 106 | 0x41ade0 | new | 16 | compared: 3 of 3 breaks caught; new: active and not dead, inline | damage,heat,hit,jets |
| 109 | 0x4792b0 | shape | 48 | compared: 2 of 3 breaks caught; fields moved, no other change | skis |
| 112 | 0x4243f0 | logic | 101 | compared: 3 of 4 breaks caught; the segments and probes; the zone FIX is behind `bugs`, off here | collide |
| 115 | 0x41de30 | new | 24 | compared: 1 of 1 breaks caught; new: structure and armor to nothing | damage,health,heat |
| 116 | 0x4799b0 | new | 27 | compared: 2 of 2 breaks caught; new: the headlight's turn | headlight |
| 119 | 0x41f740 | new | 19 | read: the definition by +0x6a8's +0xc (the roster) through the cache 0x4867f0; the port's `mech_dfn(pilot->roster)`, by a hook in the tests | stub: actions,jets,parts,update |
| 124 | 0x424520 | logic | 148 | compared: 6 of 6 breaks caught; the first query in the air, the top probe, the touchable node | collide |
| 128 | 0x41fb70 | logic | 30 | compared: 1 of 1 breaks caught; the phase's rate | gait,mechanim |
| 133 | 0x4790f0 | logic | 58 | compared: 2 of 3 breaks caught; the slide along the normal | skis |
| 135 | 0x4615b0 | new | 22 | read: a wrapper of 0x510540 | parts,pose |
| 136 | 0x472420 | reordered | 26 | read: message 0x18, size 0x10, three floats, to vt+0x60's +0x76c; `net_send_shove` the same, `t->owner` standing for +0x76c, not checked further | stub: collide |
| 137 | 0x4778a0 | shape | 65 | compared: 3 of 3 breaks caught; fields moved, no other change | parts |
| 144 | 0x465da0 | new | 18 | **gap:** no test sees the headlight go out at a death (`mech_damage.cpp`); the turn itself is test_pm_headlight's | damage,hit |
| 145 | 0x4638f0 | shape | 75 | compared: 2 of 2 breaks caught; fields moved, no other change | parts |
| 146 | 0x4255f0 | logic | 34 | compared: 1 of 2 breaks caught; the touch's stop; its damage scale is per_second, off here | collide |
| 153 | 0x41f8d0 | new | 14 | **gap:** a hook in test_pm_power, `app_mech.cpp` not run; same by reading | lod |
| 159 | 0x41c570 | shape | 34 | compared: 2 of 2 breaks caught; fields moved, no other change | actions |
| 181 | 0x467770 | shape | 31 | compared: 2 of 2 breaks caught; fields moved, no other change | parts |
| 187 | 0x41dcd0 | shape | 40 | read: look vt+0x14; viewed; HUD 0x430e30; place 0x445fe0(x, y); zoom vt+0x108(0x14) when +0x152c bit 4 is clear and +0x1694 is 0. `player_frame` the same if `kHidden` is that bit and `x174` that byte, which the HUD tests compare | stub: update |
| 190 | 0x477760 | reordered | 38 | compared: 2 of 2 breaks caught; fields moved, no other change | skis |
| 191 | 0x414370 | shape | 46 | read: viewed, then torso 0x4672d0(1); [0x62939c], never written, picks the mode switch; else mode vt+0x20(view, 1). `mech_falls` the same; the mode's vt+0x20 is `on_switch` | stub: actions |
| 204 | 0x460eb0 | logic | 17 | compared, a break caught | stub: damage,deaths,hit,parts |
| 206 | 0x477e40 | same | 68 | compared: 1 of 3 breaks caught; no PM logic here | mech_build,skis |
| 215 | 0x460f00 | logic | 16 | compared: 1 of 1 breaks caught; the sinks' rate | heat |
| 235 | 0x466550 | same | 25 | compared: 2 of 2 breaks caught; no PM logic here | parts |
| 238 | 0x41f340 | shape | 22 | compared, a break caught | stub: update |
| 239 | 0x477960 | shape | 44 | compared: 2 of 3 breaks caught; fields moved, no other change | parts |
| 247 | 0x467b70 | shape | 11 | compared: 2 of 2 breaks caught; fields moved, no other change | update |
| 251 | 0x4283f0 | shape | 158 | compared: 4 of 5 breaks caught; fields moved, no other change | parts |
| 263 | 0x41f310 | shape | 14 | compared, a break caught | stub: command,damage,health,heat |
| 271 | 0x419d90 | same | 14 | compared: 1 of 1 breaks caught; no PM logic here | collide |
| 272 | 0x41f770 | shape | 12 | compared: 1 of 1 breaks caught; fields moved, no other change | jets |
| 282 | 0x417e00 | logic | 14 | read: a wrapper of 0x4ff8d0 | parts |
| 284 | 0x424220 | same | 156 | compared: 2 of 3 breaks caught; no PM logic here | collide |
| 290 | 0x477f30 | same | 23 | compared: 1 of 2 breaks caught; no PM logic here | skis |
| 293 | 0x41f390 | shape | 13 | compared, a break caught |  |
| 298 | 0x417290 | shape | 67 | compared: 3 of 3 breaks caught; fields moved, no other change | actions |
| 303 | 0x4675e0 | shape | 150 | compared: 4 of 4 breaks caught; fields moved, no other change | parts |
| 304 | 0x417e30 | same | 10 | compared: 1 of 1 breaks caught; no PM logic here | actions |
| 310 | 0x467b30 | shape | 11 | compared: 2 of 2 breaks caught; fields moved, no other change | update |
| 311 | 0x41a020 | shape | 67 | compared: 6 of 6 breaks caught (test_pm_ams, with the real find 0x4b7c90, 4 of 4, and fire 0x4b7d40, 3 of 4: `rounds > 0` for `!= 0` can't differ after find) | stub: update |
| 312 | 0x4654e0 | same | 64 | compared: 1 of 1 breaks caught; no PM logic here | pose |
| 321 | 0x4672d0 | shape | 63 | compared: 3 of 3 breaks caught; fields moved, no other change | parts |
| 323 | 0x4616c0 | same | 71 | compared: 1 of 1 breaks caught; no PM logic here | parts,pose |
| 324 | 0x419bb0 | shape | 64 | compared: 2 of 2 breaks caught; fields moved, no other change | gait |
| 325 | 0x4779e0 | same | 50 | read: its switches are never set, the port leaves it out | pose |
| 329 | 0x463840 | shape | 51 | compared: 2 of 2 breaks caught; fields moved, no other change | parts |
| 332 | 0x41e2d0 | same | 39 | read: the part by id; the port uses its parts directly | collide,damage,health,heat |
| 334 | 0x417170 | shape | 47 | compared: 1 of 1 breaks caught; fields moved, no other change | actions |
| 338 | 0x461650 | same | 34 | compared: 2 of 2 breaks caught; no PM logic here | pose |
| 341 | 0x467490 | reordered | 64 | compared: 1 of 2 breaks caught; fields moved, no other change | parts |
| 342 | 0x461430 | shape | 29 | read: the part's mech, found once | damage,deaths,hit,mech_build |
| 364 | 0x41d880 | logic | 7 | **gap:** the exe's ran; the port's `mech_set_hud_fall` is not run by a test; same by reading | actions |
| 371 | 0x41f940 | shape | 23 | compared: 1 of 1 breaks caught; fields moved, no other change | actions,skis |
| 384 | 0x4143f0 | shape | 20 | read: viewed; [0x62939c] never written; else mode vt+0x20(view, 0). `mech_stands` the same | stub: actions |
| 392 | 0x4643f0 | same | 17 | read: the foot's leg | damage,hit |
| 397 | 0x41e2b0 | same | 8 | compared: 1 of 1 breaks caught; no PM logic here | collide,damage,gait,heat |
| 405 | 0x417220 | shape | 11 | compared: 1 of 1 breaks caught; fields moved, no other change | actions |
| 432 | 0x41e4c0 | same | 3 | read: an accessor | damage,gait,health,hit |
| 433 | 0x424710 | same | 4 | read: calls 0x424730 | collide |
| 434 | 0x424720 | same | 4 | read: calls 0x424ba0 | collide |
| 436 | 0x41f760 | same | 3 | read: an accessor | jets |
| 452 | 0x417e50 | identical | 7 | read: the attached ones by side | parts |
| 454 | 0x41c520 | identical | 29 | compared: 1 of 1 breaks caught; no PM logic here | actions,jets |
| 474 | 0x422040 | identical | 15 | compared: 1 of 1 breaks caught; no PM logic here | actions,mechanim |
| 475 | 0x422140 | identical | 26 | read; a generated break landed elsewhere | actions,mechanim |
| 477 | 0x4221c0 | identical | 25 | bytes identical to MW3's 0x421080 (table); `calf` not compared on PM | stub: pose |
| 478 | 0x422210 | identical | 25 | bytes identical to MW3's 0x4210d0 (table); `thigh` not compared on PM | stub: pose |
| 489 | 0x460ef0 | identical | 5 | compared, a break caught | stub: parts |
| 494 | 0x463fb0 | identical | 2 | read: an accessor | gait |
| 495 | 0x464430 | identical | 4 | compared: 1 of 1 breaks caught; no PM logic here | gait |
| 500 | 0x4677e0 | identical | 10 | compared: 1 of 1 breaks caught; no PM logic here | parts |
| 517 | 0x477f70 | identical | 7 | compared: 1 of 1 breaks caught; no PM logic here | collide |

**Breaks, per function, from 2026-11-05:** 0x4149b0: 1 of 1 (test_pm_cockpit_sound); 0x41f310: 1 of 1 (test_pm_cockpit_sound); 0x41f340: 3 of 3 (test_pm_cockpit_sound); 0x41f390: 2 of 2 (test_pm_cockpit_sound); 0x41f3c0: 1 of 1 (test_pm_cockpit_sound); 0x460d90: 2 of 2 (test_pm_wake); 0x460e50: 2 of 2 (test_pm_wake); 0x460eb0: 3 of 4 (test_pm_wake); 0x460ef0: 1 of 1 (test_pm_wake); the wake's stop has one uncaught, swapping two writes nothing reads between. Also 0x461bb0, 0x461d70, 0x4617a0 (`test_pm_hit`), 0x4299f0, 0x429560 (`test_pm_player`).

**The frame's rows after the breaks (2026-11-05):** 82 compared with breaks in the function itself, 20 read (accessors and wrappers, or code the port leaves out), 3 gaps, 2 identical in bytes and not run on PM. 292 of 337 breaks caught over 81 functions, each test clean first. Run: 57 recorded ones again, 238 generated in the row's own body, 44 by hand; 2 generated ones fell outside their row and aren't counted. A generated break lands anywhere in the body, so it shows the function is checked, not that its PM change is: a row names a PM difference only where a break was on it. The rows of class shape, same and identical were checked as fields moved, not read for other changes here.

The 45 not caught, by reading:
- **Can't differ** (25): `<` for `<=` where both give the same value or only an exact tie differs; a sign on a value that is 0, or a null check on what is never null here; `flat_normalize`'s result where only the sign is read; the wade's `(3f + 1)^2`, capped at 8 whatever it is (the quirk in `heat`).
- **Cleared first** (3): `blocked[1] = false` with no mech in 0x425030, the same break three times: Collide clears the flags before it.
- **Off in the tests by design** (4): the zone FIX behind `bugs` in 0x4243f0, `frame_steps` in 0x4255f0, the no-hook fallbacks of `mouse_set` and `body_mass`.
- **Closed** (2): 0x424dd0's tilted vehicle matrix and 0x4186f0's half lean, in the tests now and caught.
- **Not settled** (6): the root motion's x in 0x418850, always 0 in the test; the step's `stop` in 0x418fc0; `c.clear` in 0x424220; the ski's `m[9]`, `m[11]` in 0x477e40; `water_surface` in 0x464060. Each could be a small gap.
- **Gaps** (5): the headlight at a death 0x465da0 (three breaks), 0x41d880 in the port's `app_mech`, and `zcls::ground_query`'s arguments in 0x477f90, which go through test_pm_di's path and not test_pm_skis, whose query is canned. Also 0x41f8d0, with no break. Not in the table: `heat_override` 0x419fb0's check on the view's mech, not caught by the four tests tried (a generated break that fell outside its row, one of two left out of the count).

**The second campaign's rows (2026-11-05, breaks in the function itself):** 0x40def0 3 of 3, 0x40d920 1 of 1, 0x40e340 3 of 3, 0x40e3f0 1 of 2 (the missing block, no shipped mission lacks one), 0x40e4d0 1 of 1, 0x40e510 2 of 2, 0x473690 4 of 4, 0x40e6c0 2 of 3 (the kept loadout's take is stood in), 0x40e760 2 of 2, 0x40e830 1 of 1. Read and run only through the chain, the exe not run: the Win State's branch 0x47cf20, 0x47ee40, the menu 0x4912f0, 0x4916c0 and its state 0x489b40, 0x489b70, the pause's REPLAY 0x495ec0, the end 0x494fa0, the player's setup 0x4a19c0 and the lance 0x407c20 (`keep` for types 5 and 2 now, for all slots; MW3's Op 4 Mission 6 rule isn't in PM), the warnings 0x404a20. The last is the campaign's missions 2 and 5, not game types: vt+0x48 is the mission's number there.

**The second campaign's rows: what the compare says differs, and where the port has it (2026-11-05).** Read from both sides' code unless it says run; the vtables used are PM's (Game -4 from MW3's, Store's engine slot gone).

| Row | Differs from MW3, or new | Where the port has it |
|---|---|---|
| 0x47cf20 Win State | type 5: 0x47b770, 0x40e510, 0x47ee40 after the screen is made, not networked and no cheat | `WinState::enter`, `win_standalone`. Read, and run by the chain |
| 0x47ee40 | new: writes the Win State's flag; help 0xfb8, else 0x173 + no cheat | `standalone_won`. Help read back in the chain |
| 0x4912f0 | new: type 1 chooses a saved game, type 5 0x40e340 | `GameList::select`. Run by the chain's replay |
| 0x4916c0 pilot list (0x4882b0) | 5: 0x1027, the reached rows, 0x48bce0, else mission 0. 3: **"Pilot Statistics", kills, shots, kills per 1k: the port had MW3's line, fixed**. 4: **one training mission, not MW3's four: not changed, PM's menu doesn't reach it** | `fill_games` |
| 0x489b40, 0x489b70 state | new: mode 5, then Load | `GameMenuState`. Run by the chain |
| 0x495ec0 pause (0x492750) | **starts the MENU sounds, 0x496120 releases them: the port lacked it, fixed**; REPLAY for types 1, 2, 5; four buttons | `PauseScreen`. Read; REPLAY for 5 not run |
| 0x494fa0 end | case 5 with 0 and 1 | `end_screen`: **was missing, found by the chain**. Run by the chain |
| 0x4a19c0 setup (0x49e810) | 0x18 clamp: **the port had MW3's 0x12: lancemates past 18 became mech 18, fixed**. 0x5bfdd0 on training: **added**. 0x40e6c0, 0x40e6a0 with a bad index report 0x24a: ported. The record flag 0x10 and 0x4356c0: not ported, 0 of 335 shipped records have it | `player_setup`. Read; the clamp shown by the chain |
| 0x407c20, 0x4079b0 lance | new: `keep` for 5 and 2, all slots; **the search over 25: the port had 19, fixed** | `lance_keeps`, `lance_slot`. Run: `test_pm_lance` |
| 0x4cdb30 inout | 0x435db0 | 10 and "sa", already |
| 0x4486e0 objectives box | case 5 with 1, 2; 0x448520 | already; the box's call not read further |
| 0x47d890 debriefing | types 5 and 2 | ported earlier (`test_pm_debrief_chain`); 5 is the chain's |
| 0x40e3f0, 0x40e6c0 | **the store's vt+0x10 is remove: the port added, fixed** | `test_pm_standalone` on real lineups |

The mech and weapon count sweep (2026-11-05, grep and read): lance, lab, salvage, pilot, Instant Action, player, names. Wrong: the two above. Everything else already uses `kMechCount` 25, `kWeaponCount` 56; `kTable[19]` in mech_names is by tonnage, not mechs. Vtables in the account: Game and Store checked here. The 2D objects come with the lab; controls and parts get a line in the table when a read-only row reaches them.

**A logic row isn't finished without its account (the lead, 2026-11-05).** Every constant and call the normalized compare says differs gets a line: where the port has it, or why it doesn't apply. A break that hits a line is not that account: two real differences were found by doing it, in rows already marked compared, and the compare had listed both all along. Rows outside the frame the same, when reached.

| Row | The compare says | Where the port has it |
|---|---|---|
| 0x41ae00 Update | PM adds 0.6, calls 0x447da0, 0x592a50, 0x433500; MW3's math calls go | 0.6: the throttle under a push, `mech_update.cpp`. 0x447da0: `hud_ammo`, `app_hud.cpp`. 0x433500: `fade_debris`, `mech_dead.cpp`. 0x592a50: a sound's query, run through `test_pm_jets`'s log. The math moved to 0x423590 and 0x4186f0. |
| 0x477f90 skis | PM adds 0.7, 1024, 0x41f940 twice, **0x4fcf70, 0x4fcf20 twice**; MW3's -0.7, -50 go | 1024: `kQueryTop`. 0.7: `kStep`, subtracted. 0x41f940: the clamp, inline. **0x4fcf70, 0x4fcf20: the mech's own node out of the query, missing until this pass, fixed.** -50 is MW3's only. |
| 0x41a100 heat | PM adds 0.2, 0.5, 0.75, 2.9, 4, 25, 0x5cb260, 0x58fe70 x3; MW3's negatives go | The constants: `mech.cpp` (drowning at 25, 0.75 left a step, killed under 0.2; the rest are MW3's with the sign moved). 0x5cb260: the place. 0x58fe70: stop sound. |
| 0x418850 Move | PM adds 16; MW3 loses two calls | 16: the sound's slot and the mech's detail bit, `detail & 0x10`. The calls are inlined accessors. |
| 0x417f50 jets | PM adds 0.9, 2, 255, 0x434340; MW3's 0.5, 0.7, 0x40afd0 go | 0.9: the thrust at once. 0x434340: the canned effect's stop, logged in `test_pm_jets`. The rest are the jets' move in PM. |
| 0x418fc0 gait | PM adds 16; MW3's 7, 0.5 go | 16: the detail bit. 7: MW3's Elemental id, now 11. 0.5: read back from a global, same value in `mech_move.cpp`. |
| 0x423730 Collide | PM adds 0.25, 0.5, 0.7, 0.9, 11, 2, 0x4f8d80, 0x41e2d0, 0x472420 | 0.25, 0.5: the crash's shares, `crash_part`. 0.7, 0.9: the bump. 11: Elemental. 0x4f8d80: the node's object. 0x41e2d0: the torso, direct in the port. 0x472420: `net_send_shove`. |
| 0x417350 actions | PM adds 65, 0x5cc3a0; MW3's 5, -50, 0x41d880 go | 65: the state check. 0x5cc3a0: an iterator. 5: `lie_time`, in a global in PM, a break on it caught. 0x41d880: set elsewhere in PM (test_pm_power tests it). |
| 0x425030 mech hit | PM adds 1, 4, 0x41e2d0, 0x472420; MW3's 0.02 goes | MW3's small head damage is gone; the crash's parts. 0x472420: `net_send_shove`. |
| 0x416d70 landing | PM adds 8, 11, 12, 13, 0x4638f0 | Ids and states. 0x4638f0: `crouch`. |
| 0x424730 wall | MW3's 0x5cb260 x2 go | The prediction replaces the place. |
| 0x41d920 power | PM adds 0x4f8d80, a virtual | The target's object, and clearing the target. |
| 0x424dd0 vehicle | MW3's 10 is 11; a virtual inlined | Elemental's id. |
| 0x424ba0 ceiling | PM adds 0x424730, a virtual | `wall_response`, `can_crush`. |
| 0x41c9f0 crouch | MW3's 7 | Elemental's id: 11 in `kElemental`. |
| 0x464450 foot | PM adds 16, 4 | The detail bits. |
| 0x419de0 turn | MW3's 0.85 is 1.0 | `kAirTurn`. |
| 0x4243f0 query | stack sizes | The size of the exe's own stack buffer only. |
| 0x424520 sort | PM adds 11, 76, a virtual; MW3's 65, 72 go | The pool's hit list, which the port doesn't have; offsets; `airborne`. |
| 0x41fb70 phase | MW3's two calls | Accessors inlined. No logic change. |
| 0x4790f0 slide | a call; MW3's 10 goes | 0x477ef0, the two soils, `step_kicks`. |
| 0x4255f0 touch | MW3's 20 goes | The 20 is in a global, [0x60463c], in PM; the call is now 0x5a50a0 with the hit's point. |
| 0x460eb0 wake | MW3's 0x4fca50 is a virtual | `test_pm_wake`. |
| 0x460f00 sinks | **PM adds 1.0** | **The floor of 1, missing until this pass, fixed.** |
| 0x417e00 ground | PM adds 16 | The argument's offset. A wrapper. |
| 0x41d880 hud fall | MW3's 0.0 goes | Not a difference in the logic; compared in test_pm_power. |

Before this pass, 9 of the 26 notes named what a break hit and not a PM difference (0x460f00, 0x41fb70, 0x4255f0, 0x4790f0, 0x41c9f0, 0x464450, 0x4243f0, 0x419de0, 0x424730). This table is read from the compare and the two sides' decompiled code, not each line run; the constants were also searched for in the port by value.

## The mech lab (2026-11-05)

The account compares each row's assembly with MW3's (immediates and mapped calls), since the decompiler's view of MW3 hides register arguments. 219 lab rows; the differences that are more than offsets are read below as they are reached.

- 0x455930 firepoint picker, new, called from the put-on 0x455a80 in place of MW3's last-and-first turn (`lab_pick_firepoint`). `test_pm_lab_pick.py`, 6000 picks against the exe; six breaks caught (the part's filter, missile or not, the random pick, first of equals, widest, the table). The put-on around it, 0x455a80: `test_pm_puton.py`, the exe's function on the first 9 loadouts of each mech with real firepoints, store and item code, 1,447 puts, events, armour, loadout and store after each. 11 breaks caught (16 weapons, a full store's rounds, half a ton, as much of the new as the old, the old armour back, one kind of sink, the jets' limit, no firepoint, the picker wired in, the flag, out of the store). Stood in: the list's rows and rule, the part, the refresh, the sound, the message text, the ammo rounds' weapon lookup (both sides the no-definition answer).
- 0x44a660 chassis rows: the empty row for a lancemate is in both campaigns (0x47b6c0 or 0x47b770). The port had the first only, so the second campaign could not take a lancemate's mech away. Fixed; `test_pm_puton.py` runs 0x44a660 itself by game type 0..5 and slot 0..3 on an empty store; 4 breaks caught (each campaign, lancemates only, the row). The port's hook `pilot_standalone` in GameEngine and the rows' sort over a filled store are read only.
- 0x44a010 the lab's update: the two developer keys no longer want the flag clear, an imported loadout goes on its own mech's list, not the shown one's (0x426d10 refuses any other). Ported; read only. PM's in-game file boxes 0x48fef0 and 0x490120 replace the Windows dialogs; the port keeps its own `export\\` and `import\\` stand-in (FIX (A)), not ported.
- 0x458380 MECH_DATA, 0x458a50 CHASSIS_DATA: **a bug in both campaigns.** PM's weight function moved the most tons from mode 8 to 9, and 8 became the shielding, but the panels still asked for 8: the lab showed a few tons as the mech's maximum, far too high speeds, and every loadout as over. Now 9; also PM's floors (18 and 13.5 kph) and row 3, structure with shielding. `test_pm_lab_data.py`, all 113 loadouts and random engines, 1,356 states, against the numbers the exe hands to sprintf; four breaks caught (mode 9, each floor, row 3). The rows' text and placing are not compared.
- The weight function's callers, every one (the store's sibling check): in the exe 0x427130 has 7 callers: its own sum, 0x427360 validate, 0x44f740 and 0x454bc0 (-1), 0x4581b0 (-1, 3), and the two panels. None in salvage, the AI's build, the HUD or multiplayer; those take the item's weight or the engine's, other functions. The port's callers match in mode: only the two panels had the old 8. Read, from the call sites; the two panels and validate are also run (`test_pm_lab_data`, `test_pm_config`).
- 0x44cec0 the screen's constructor: PM no longer sets MW3's opening message 0xdcc. Removed; read only.
- 0x44b390, 0x44c620, 0x44c930, 0x44f220, 0x44ed20, 0x451c20, 0x44f740's copy (case 0x10): read against the port, no logic differs; the new calls are inlined or moved helpers.

- 0x426bd0 and 0x4267f0, the saved archives, `configs\\<mech>.mw3`: `test_pm_archives.py`, 25 archives written alike by the exe and the port (entries' flags, comment and time masked, the exe fills them from its stack), 100 reads of the exe's file as written, with another version, cut, and with no file; 5 breaks caught (the version check, from entry 1, the player's own only, version 16, each under its name). 0x427e50 and 0x427e80, the developer keys' file bodies: read only; they match the stand-in's order (name, then the loadout), and the checksum trap stays out.
- Compared with MW3's by assembly and not run here: the rest of the 219 rows. 92 differ at all, most in offsets alone or by inlining; those are constants and calls compared, not results. The port's own 2D classes call no exe vtable, so the moved slots don't reach them; a 2D row's text, placing and mouse are unchecked on PM.

- 0x4509f0, 0x450950, 0x450990, 0x4509c0 MESSAGE_AREA, with the flashing text base 0x5c2230, 0x5c2320, 0x5c2430, 0x5c24e0: **a bug in both campaigns (Inner Sphere and Pirate Missions).** Each message plays its sound, shows for 3 seconds and swaps white and red every 0.175 s, then hides; the port kept MW3's permanent white line. The dialog's new FLASH (235,0,0 at 0.35) is the only text FLASH in any dialog of either game, so the port's `LabMessage` carries the flash and the base's blink mode isn't ported. `test_pm_lab_message.py`, 7,200 steps; nine breaks caught. Reading FLASH from the dialog is read only, and seen working in the shots.
- 0x407cc0 the briefing's buttons, which the address map doesn't pair with MW3's 0x408ef0, so the row sweep missed it: **a bug.** MW3 locks MECHLAB and SALVAGE at Op 1 Mission 1; PM has no such test, and Owen found MECHLAB open at the first Inner Sphere mission in the original. `test_pm_briefing_buttons.py`, game types 0..5, loaded or not, against stand-in buttons; five breaks caught. A second campaign's SALVAGE stays dim, as the exe's default.
- The version (0x4b31c0, 0x4b30b0): the exe formats its file version in full. Three uses. The main menu 0x489041 and the briefing 0x40513b draw "%s %s", message 5 and the version, `MechWarrior 3 Pirate's Moon 1.0.11.5` in debug_font, bottom right at (640 - width - 5, 480 - height - 3), no condition; Owen saw 1.0.11.5 in that corner of the original. The load's progress line 0x492a55 has `MechWarrior3 (PRODUCTION) (1.0.11.5)`. The window's title 0x485952 is message 5 alone. MW3 had a fourth use there, with its version. The port had MW3's 1.2, and MW3's name, message 7, which PM's table doesn't have, so `mw3pm` drew an empty name, an empty title, an empty caption on the "cannot read" box 0x48292f, and an empty product for the settings. Those are message 5 now, the settings' product PM's literal `MechWarrior 3 EP1` (0x4828d2), and the version 1.0.11.5. Message 5's other use, 0x4854eb, is the disc check on `data.tag`, which the port removes. Seen in shots on the menu and the briefing; the placing, read in the exe, is the port's. `test_pm_messages` now also checks a plain `message(N)` against the ids PM lost, which is how 7 got by: a break putting it back is caught. The title, the box's caption and the settings' product are read only; so is the short version string 0x4b31b0, the port's 1.0, which goes to the settings' key. Another plain id with text PM changed, 0x320 and the lobby's, belongs to step 8.
- 0x41c900 the aim (vt+0x164), which the map doesn't pair with MW3's 0x41bc20: **a difference for PM's AI.** PM has no facing test and no cockpit view: the torso always gets the direction, and a point behind the torso no longer clears the aim. Its callers are 0x41c9d0 and 0x4bbcc3 in the exe, and the port's AI hook, so no player sees it yet. `Mech::aim_at`; `test_pm_mech_vt.py`, 3,000 cases; seven breaks caught.
- 0x419c40 the placement, MW3's 0x418e90: PM fills predict's block (+0x6dc..+0x724) as the mech stands. `pending_from_state`, compared on 200 cases, two breaks caught; its place in `reset_at` is read only. Its last call, through the mech's +0x14c (the controls' base 0x428880), then +0x58 and +0xc, isn't ported: not read further, and it may wait on PM's AI.
- Mech vtable rows paired by their code: 0x41ed00 (+0xa0) `clamp_aim`, MW3's 0x41dca0, the same; 0x41df80 (+0x158), MW3's 0x41d190, the same (+1 where MW3 subtracts -1, the field now +0xe4); 0x41e110 (+0x140), MW3's 0x41d320, the same shape; 0x419d50 (+0xe8), MW3's 0x418f30, the same with the placement above. 0x41e180 (+0x148) has no twin found and no direct virtual call: not settled.

- 0x454bd0 the item rule, which the map doesn't pair with MW3's 0x453840: **a bug, new in PM.** A part takes one CASE; the port let a second on. `lab_item_fits`; `test_pm_puton` now runs the exe's rule itself, about 3,000 calls on real loadouts, both tabs; 12 breaks caught (the CASE to a part, the weight, 16 weapons, jump jets' parts, the slots, a firepoint, the armour's most, up to five, one only, C3 each way). It takes out MW3's rule's stand-in only for this compare; the puts themselves are still told yes or no, so they reach their own refusals.
- MW3 rows with no pair in the map, found by position: 0x44e450 (0x4506a0), 0x4559e0 (0x456cb0) with the same calls and constants; 0x453770 and 0x454580, the armour's and other tabs' filters, inlined in 0x4548d0 and 0x4552b0, and 0x448f10 (0x44a320), its draw; read only. **A row sweep by the map can't see a function it doesn't pair**: the two bugs above were such rows. Outside the lab the same holds, so each later batch lists its unpaired rows first.

**The walk through the real screens (2026-11-05):** `mw3pm` and `mw3` from `main`, hidden and silent, a scratch write dir, clicks through `--click-at`, one run for each shot; `--briefing-wait N` is new, since PM's movies outlast `--briefing`'s clicks. Compared by eye, side by side: chassis, criticals, ammunition, equipment, armour, weapon groups and salvage. Layout, fonts, rows and panel placing match, apart from the message line above and PM's data. Also seen on PM: the armour's refusal message, a take-off and a put-on with the mouse, the load-out list scrolled, a first-campaign and a pirate mission's lab, and a pirate lancemate's "No 'Mech" row. The two games' `dlgSalvage.zrd` are byte-identical. `dlgMechLabII.zrd` differs only in the message's FLASH, the rollover flash rate (0.05 to 0.08) and the weapon lists' SIZE (15 and 12 to 18). The 2D code is the same on both branches.

**What the walk does not do:** it checks that the screens work and look right, with `main` and the dialog files as its reference. It doesn't compare with PM's exe, and it isn't behaviour coverage. Owen reports the original's lab and salvage look identical to MW3's, a reference from memory of the look only. Not reached through the screens: the alternate font for an over-weight total (the rules refuse every way to get there; the comparison itself is in `test_pm_lab_data`), and the chassis list's scrolling (no store here fills it). The 2D world on PM stays on the list as needed before PM can be called finished, with `test_pilot`.

**The table's "not ported" marks (2026-11-05):** the Port column below was matched by name and is stale; 45 rows say `no`. Read one by one, not run:
- Ported inside another row: 0x41f7a0, the LOD switch in 0x41f4d0 (`mech_lod`); 0x426600 (`part_armor_max`); 0x425c60, 0x427310 (`validate`).
- Standard library or the compiler's: 0x468da0 (`std::vector` insert), 0x472e60, 0x46b4b0.
- Accessors and one-line wrappers: `skim` in the column below.
- Settled since: the rows above and the Port column below. `inside` for 0x475b20, 0x414ba0, 0x41c4c0, 0x477dd0 and 0x415160 goes by their callers (the store's, the build, the action step, the mech's end), which are ported but not all compared, and by no compare of their own. #20 0x424030 is compared in the frame's table.
- **Not ported: PM's network rounds.** 0x472850 takes a round's state from the host: overtime ("No winner yet. We are into Overtime, folks!"), a winner, messages 0x9aa and 0x9ab, no more enemies, base destroyed, then the round's end at +0x94 and +0x88. MW3 has none of these strings. Nothing in the port reads them. It belongs to `pm-map.md` step 8, PM's multiplayer, not to these rows.

**Not moved: `test_lab_list`, `test_lab_screen`.** Both need the 2D world (`twod_world`, `test_twod`) on PM, and no test has it. A try through `MW3_EXE=pm` got as far as the screen: PM inlines MW3's menu screen constructor 0x40a110 (no twin to call), its node reader takes other arguments, and its 2D objects are larger (texts and buttons by 8, `0x4b0`→`0x4b8`, panels by more), so every dump's offsets change. What they alone would check on PM: the screen's construction, the pages, the list's rows, scrolling and mouse.

**Shots (+0xcc, 0x471ad4):** the exe's network sender counts each shot on the network game; the port did not. `shot_fired` in `NetCalls`, `test_pm_net_shot.py` against the exe's sender kind 0x16, 40 shots, record and count; a break removing the call is caught. The hook's wiring to the game (`mission_multiplayer_count_shot`) is read only: no multiplayer run reads it back.

## The order

The lead's rule (2026-11-05): what the frame runs for every mech ranks above what it runs for the player only, unless the player-only one changes how the game feels. The score below cannot see that, so it is applied by reading each function as the order reaches it.

The loadouts first: they decide what exists. Then by a score: size, how unlike MW3, nearness to the frame, callers. It ranks
the work; it does not measure play, so jets, landing and 0x423590 are near the top by size and depth, not because of a play test.

**The Port column (2026-11-05, from the source):** `yes`, the source cites the row by PM's address or MW3's twin, or the row was paired by its code and read; `inside`, the logic is ported inside another row or its compared caller, not cited on its own; `lib`, the compiler's or the library's; `skim`, a small accessor or wrapper with its first lines read, no sign of logic to port, nothing compared; `ai`, waits on PM's AI; `step8`, PM's multiplayer, `pm-map.md` step 8; `unread`, marked unported by name and not read: those are small, under about 12 instructions or with no decompile, and could still hold something. The Tested column isn't kept up; the frame's table above and the breaks say what's compared.

| # | PM | MW3 | Class | Instr MW3 | Instr PM | Alike | Depth | Callers | Port | Tested | Note |
|---:|---|---|---|---:|---:|---:|---:|---:|---|---|---|
| 1 | 0x425c60 | - | new | 0 | 39 | 0.00 |  | 1 | inside |  | an unplaced item |
| 2 | 0x427020 | 0x4251d0 | changed | 108 | 97 | 0.90 |  | 1 | yes |  | weight of a category |
| 3 | 0x427130 | 0x425300 | changed | 137 | 140 | 0.62 |  | 21 | yes |  | weight: case 8 is the cockpit and shielding, none for the Elemental (ported), the port numbers 8 as max mass |
| 4 | 0x427310 | - | new | 0 | 35 | 0.00 |  | 1 | yes |  | free slots not negative, not in the port |
| 5 | 0x427360 | 0x4254d0 | changed | 73 | 70 | 0.75 |  | 5 | yes | yes | validate: no Elemental exemption |
| 6 | 0x427db0 | 0x425fb0 | changed | 16 | 18 | 0.67 |  | 4 | yes |  | jump jet limit |
| 7 | 0x427df0 | 0x425ff0 | changed | 56 | 43 | 0.87 |  | 4 | yes |  | jump jets |
| 8 | 0x41ae00 | 0x419fc0 | changed | 890 | 805 | 0.55 | 0 | 1 | yes |  | Mech::Update |
| 9 | 0x477f90 | 0x4765b0 | changed | 1297 | 1347 | 0.66 | 1 | 1 | yes |  | calls 0->17 |
| 10 | 0x41a100 | 0x4192f0 | changed | 732 | 803 | 0.44 | 1 | 1 | yes |  | calls 3->6, consts 8->11 |
| 11 | 0x418850 | 0x417a80 | changed | 608 | 635 | 0.57 | 0 | 5 | yes |  | Move |
| 12 | 0x417f50 | 0x4173b0 | changed | 456 | 410 | 0.36 | 0 | 1 | yes |  | jets: thrust from [0x629314], fuel moved out, assist asks the game (vt+0x44) |
| 13 | 0x418fc0 | 0x4181c0 | changed | 861 | 817 | 0.62 | 1 | 1 | yes |  |  |
| 14 | 0x423730 | 0x4225c0 | changed | 525 | 708 | 0.54 | 1 | 1 | yes |  | calls 2->5, consts 1->4 |
| 15 | 0x417350 | 0x416860 | changed | 796 | 796 | 0.71 | 1 | 1 | yes |  | consts 2->1 |
| 16 | 0x425030 | 0x423a20 | changed | 196 | 280 | 0.15 | 2 | 1 | yes |  |  |
| 17 | 0x423590 | - | new | 0 | 153 | 0.00 | 0 | 1 | yes |  | new: quaternion and matrix step at +0x6dc..+0x724 |
| 18 | 0x416d70 | 0x4162c0 | changed | 274 | 311 | 0.63 | 0 | 3 | yes |  | landing: only the player falls, no set_throttle, hip duck |
| 19 | 0x4253f0 | - | new | 0 | 197 | 0.00 | 3 | 1 | yes |  |  |
| 20 | 0x424030 | - | new | 0 | 160 | 0.00 | 2 | 2 | yes |  |  |
| 21 | 0x462ba0 | - | new | 0 | 287 | 0.00 |  | 6 | yes |  |  |
| 22 | 0x428a00 | - | new | 0 | 328 | 0.00 |  | 1 | yes |  |  |
| 23 | 0x462fd0 | - | new | 0 | 297 | 0.00 |  | 2 | yes |  |  |
| 24 | 0x424730 | 0x4231c0 | changed | 350 | 360 | 0.69 | 2 | 6 | yes |  | calls 2->0 |
| 25 | 0x4186f0 | - | new | 0 | 115 | 0.00 | 1 | 1 | yes |  | new: lean into turns, +0x560, +0x568 |
| 26 | 0x41be50 | - | new | 0 | 255 | 0.00 |  | 3 | yes |  |  |
| 27 | 0x462090 | 0x460d50 | changed | 435 | 483 | 0.46 |  | 2 | yes |  | calls 6->1, consts 0->1 |
| 28 | 0x468da0 | - | new | 0 | 247 | 0.00 |  | 2 | lib |  |  |
| 29 | 0x46ea40 | - | new | 0 | 211 | 0.00 |  | 1 | yes |  |  |
| 30 | 0x41f4d0 | - | new | 0 | 211 | 0.00 |  | 0 | yes | yes |  |
| 31 | 0x41ba10 | 0x41ac60 | changed | 391 | 367 | 0.48 |  | 0 | yes |  |  |
| 32 | 0x466720 | 0x464e80 | changed | 765 | 786 | 0.75 |  | 0 | yes |  |  |
| 33 | 0x41d920 | 0x41cac0 | changed | 96 | 109 | 0.32 | 1 | 1 | yes |  | calls 0->2 |
| 34 | 0x426000 | 0x424600 | changed | 151 | 270 | 0.36 |  | 2 | yes |  | calls 0->4, imports 0->2 |
| 35 | 0x424dd0 | 0x4237c0 | changed | 208 | 204 | 0.61 | 2 | 1 | yes |  | calls 1->0 |
| 36 | 0x420aa0 | 0x41fd30 | changed | 129 | 261 | 0.41 |  | 3 | yes |  | calls 2->1, consts 0->1, imports 0->5 |
| 37 | 0x414cb0 | 0x414430 | changed | 366 | 372 | 0.62 |  | 4 | yes | yes | calls 0->4 |
| 38 | 0x464e70 | 0x4637b0 | changed | 243 | 245 | 0.31 |  | 0 | yes |  |  |
| 39 | 0x41e4d0 | 0x41d6f0 | changed | 94 | 192 | 0.25 |  | 3 | yes |  | calls 0->1, consts 0->1 |
| 40 | 0x467800 | 0x465f40 | changed | 180 | 209 | 0.22 |  | 0 | yes |  | calls 17->6 |
| 41 | 0x479a40 | - | new | 0 | 93 | 0.00 | 5 | 3 | yes | yes |  |
| 42 | 0x464060 | 0x462a10 | changed | 258 | 276 | 0.80 | 1 | 3 | yes |  |  |
| 43 | 0x425e40 | 0x424480 | changed | 165 | 185 | 0.29 |  | 3 | yes |  | calls 0->1 |
| 44 | 0x424ba0 | 0x423600 | changed | 133 | 142 | 0.47 | 3 | 1 | yes |  | calls 1->2 |
| 45 | 0x41cad0 | 0x41be80 | changed | 683 | 719 | 0.80 |  | 0 | yes | yes | calls 3->6 |
| 46 | 0x465360 | 0x463c90 | changed | 137 | 117 | 0.53 | 2 | 1 | yes |  |  |
| 47 | 0x4299f0 | 0x427d60 | changed | 464 | 489 | 0.71 |  | 0 | yes |  |  |
| 48 | 0x46a750 | - | new | 0 | 116 | 0.00 |  | 4 | ai |  |  |
| 49 | 0x461bb0 | 0x460780 | changed | 192 | 165 | 0.32 |  | 1 | yes |  |  |
| 50 | 0x46b1b0 | 0x469650 | changed | 221 | 228 | 0.45 |  | 2 | yes |  | calls 0->2 |
| 51 | 0x467a90 | - | new | 0 | 51 | 0.00 | 2 | 5 | yes |  |  |
| 52 | 0x477b50 | 0x476090 | changed | 187 | 89 | 0.35 |  | 2 | yes |  | calls 6->0, consts 2->0 |
| 53 | 0x425370 | - | new | 0 | 59 | 0.00 | 3 | 3 | yes |  |  |
| 54 | 0x479780 | - | new | 0 | 119 | 0.00 |  | 1 | yes | yes |  |
| 55 | 0x46f150 | 0x46d100 | changed | 180 | 231 | 0.53 |  | 3 | yes |  | calls 1->6 |
| 56 | 0x415e90 | - | new | 0 | 50 | 0.00 | 2 | 3 | yes |  |  |
| 57 | 0x4681b0 | 0x466840 | changed | 271 | 344 | 0.66 |  | 1 | yes |  |  |
| 58 | 0x41f7a0 | - | new | 0 | 111 | 0.00 |  | 2 | inside | yes |  |
| 59 | 0x41c740 | - | new | 0 | 122 | 0.00 |  | 0 | yes |  |  |
| 60 | 0x419c40 | - | new | 0 | 94 | 0.00 |  | 5 | yes |  |  |
| 61 | 0x477210 | 0x475520 | changed | 248 | 158 | 0.66 |  | 5 | yes |  |  |
| 62 | 0x41dab0 | 0x41cc30 | changed | 71 | 67 | 0.54 | 2 | 15 | yes | yes |  |
| 63 | 0x414430 | 0x413e60 | changed | 99 | 241 | 0.59 |  | 1 | yes | yes | calls 0->9, consts 0->4 |
| 64 | 0x461d70 | 0x460970 | changed | 226 | 235 | 0.64 |  | 4 | yes |  |  |
| 65 | 0x477ef0 | - | new | 0 | 32 | 0.00 | 2 | 9 | yes |  |  |
| 66 | 0x470b80 | 0x46eba0 | changed | 139 | 117 | 0.36 |  | 2 | yes |  | calls 2->3, imports 0->3 |
| 67 | 0x418540 | - | new | 0 | 88 | 0.00 |  | 1 | yes |  | new: speed factor from the worst leg, limp |
| 68 | 0x472da0 | - | new | 0 | 84 | 0.00 |  | 2 | yes |  |  |
| 69 | 0x4777f0 | 0x475d10 | changed | 75 | 60 | 0.46 | 2 | 1 | yes |  |  |
| 70 | 0x477470 | 0x475870 | changed | 215 | 181 | 0.64 |  | 3 | yes |  | calls 0->2 |
| 71 | 0x4149b0 | 0x4141a0 | changed | 91 | 93 | 0.09 |  | 1 | yes |  |  |
| 72 | 0x4795c0 | - | new | 0 | 82 | 0.00 |  | 1 | yes | yes |  |
| 73 | 0x476750 | 0x4749e0 | changed | 152 | 141 | 0.55 |  | 5 | yes |  | calls 0->1 |
| 74 | 0x4731b0 | 0x470d20 | changed | 107 | 47 | 0.48 |  | 25 | yes |  | calls 2->1 |
| 75 | 0x463380 | 0x461d80 | changed | 215 | 218 | 0.66 |  | 2 | yes |  |  |
| 76 | 0x41c9f0 | 0x41bda0 | changed | 61 | 59 | 0.46 | 2 | 3 | yes | yes |  |
| 77 | 0x4291a0 | 0x4274d0 | changed | 279 | 278 | 0.72 |  | 1 | yes |  |  |
| 78 | 0x464450 | 0x462de0 | changed | 131 | 141 | 0.75 | 2 | 1 | yes |  |  |
| 79 | 0x4703c0 | 0x430d10 | changed | 93 | 86 | 0.23 |  | 2 | yes |  | consts 3->0 |
| 80 | 0x415f10 | - | new | 0 | 71 | 0.00 |  | 2 | yes |  |  |
| 81 | 0x41c900 | - | new | 0 | 78 | 0.00 |  | 0 | yes |  |  |
| 82 | 0x479190 | 0x477820 | changed | 98 | 98 | 0.65 | 2 | 1 | yes |  |  |
| 83 | 0x479b50 | - | new | 0 | 72 | 0.00 |  | 1 | yes | yes |  |
| 84 | 0x4626d0 | 0x461300 | changed | 164 | 155 | 0.54 |  | 0 | yes |  |  |
| 85 | 0x472850 | - | new | 0 | 68 | 0.00 |  | 2 | step8 |  |  |
| 86 | 0x419de0 | 0x418fc0 | changed | 144 | 143 | 0.80 | 1 | 1 | yes |  |  |
| 87 | 0x479440 | - | new | 0 | 70 | 0.00 |  | 1 | yes | yes |  |
| 88 | 0x464780 | 0x463110 | changed | 361 | 396 | 0.81 |  | 0 | yes |  |  |
| 89 | 0x476ab0 | 0x474d90 | changed | 118 | 104 | 0.41 |  | 1 | yes |  |  |
| 90 | 0x462f50 | - | new | 0 | 56 | 0.00 |  | 6 | yes |  |  |
| 91 | 0x464610 | 0x462fa0 | changed | 126 | 129 | 0.47 |  | 1 | yes |  |  |
| 92 | 0x4274d0 | 0x425650 | changed | 291 | 282 | 0.78 |  | 2 | yes |  |  |
| 93 | 0x475fe0 | 0x474210 | changed | 198 | 198 | 0.69 |  | 2 | yes |  |  |
| 94 | 0x475b20 | - | new | 0 | 57 | 0.00 |  | 3 | inside |  |  |
| 95 | 0x426600 | - | new | 0 | 52 | 0.00 |  | 5 | inside |  |  |
| 96 | 0x46b4d0 | 0x469940 | changed | 257 | 268 | 0.77 |  | 1 | yes |  | calls 0->3 |
| 97 | 0x429560 | 0x427870 | changed | 366 | 294 | 0.83 |  | 1 | yes |  | calls 4->0 |
| 98 | 0x41c3f0 | 0x41b660 | changed | 83 | 65 | 0.76 | 1 | 5 | yes |  |  |
| 99 | 0x424d80 | - | new | 0 | 36 | 0.00 | 4 | 1 | yes |  |  |
| 100 | 0x418640 | - | new | 0 | 54 | 0.00 |  | 3 | yes |  |  |
| 101 | 0x4262f0 | 0x4247a0 | changed | 262 | 285 | 0.82 |  | 4 | yes |  |  |
| 102 | 0x414ba0 | - | new | 0 | 45 | 0.00 |  | 7 | inside |  |  |
| 103 | 0x4790c0 | - | new | 0 | 23 | 0.00 | 2 | 4 | skim |  |  |
| 104 | 0x4721e0 | 0x470170 | changed | 144 | 180 | 0.70 |  | 2 | yes |  | calls 0->6 |
| 105 | 0x4611f0 | 0x45fe00 | changed | 119 | 112 | 0.53 |  | 1 | yes |  | calls 4->0 |
| 106 | 0x41ade0 | - | new | 0 | 16 | 0.00 | 1 | 9 | yes | yes |  |
| 107 | 0x471040 | 0x46ef70 | changed | 129 | 116 | 0.55 |  | 0 | yes |  | calls 0->1, consts 3->2 |
| 108 | 0x474f80 | 0x472f00 | changed | 173 | 139 | 0.70 |  | 2 | yes |  |  |
| 109 | 0x4792b0 | 0x477940 | changed | 59 | 48 | 0.57 | 2 | 1 | yes |  |  |
| 110 | 0x41f460 | - | new | 0 | 50 | 0.00 |  | 3 | yes | yes |  |
| 111 | 0x41e8b0 | 0x41d940 | changed | 216 | 213 | 0.76 |  | 2 | yes |  |  |
| 112 | 0x4243f0 | 0x422ef0 | changed | 94 | 101 | 0.72 | 3 | 1 | yes |  |  |
| 113 | 0x465a60 | 0x464290 | changed | 82 | 105 | 0.48 |  | 0 | yes |  |  |
| 114 | 0x462020 | 0x460c10 | changed | 79 | 50 | 0.38 |  | 2 | yes |  |  |
| 115 | 0x41de30 | - | new | 0 | 24 | 0.00 | 2 | 1 | yes |  |  |
| 116 | 0x4799b0 | - | new | 0 | 27 | 0.00 | 4 | 4 | yes | yes |  |
| 117 | 0x4267f0 | 0x424b70 | changed | 258 | 247 | 0.81 |  | 1 | yes | yes | strings 0->1 |
| 118 | 0x4764d0 | 0x474740 | changed | 149 | 125 | 0.69 |  | 2 | yes |  |  |
| 119 | 0x41f740 | - | new | 0 | 19 | 0.00 | 2 | 5 | skim |  |  |
| 120 | 0x4776b0 | 0x475b00 | changed | 108 | 78 | 0.57 |  | 2 | yes |  |  |
| 121 | 0x41c4c0 | - | new | 0 | 44 | 0.00 |  | 3 | inside |  |  |
| 122 | 0x477dd0 | - | new | 0 | 48 | 0.00 |  | 1 | inside |  |  |
| 123 | 0x479510 | - | new | 0 | 48 | 0.00 |  | 1 | yes | yes |  |
| 124 | 0x424520 | 0x423010 | changed | 123 | 148 | 0.81 | 4 | 1 | yes |  | imports 0->1 |
| 125 | 0x4793c0 | - | new | 0 | 47 | 0.00 |  | 1 | yes |  |  |
| 126 | 0x4798d0 | - | new | 0 | 44 | 0.00 |  | 2 | yes |  |  |
| 127 | 0x416c70 | 0x4161c0 | changed | 66 | 72 | 0.45 |  | 4 | yes |  |  |
| 128 | 0x41fb70 | 0x41ef00 | changed | 26 | 30 | 0.50 | 2 | 9 | yes |  | calls 2->0 |
| 129 | 0x4796f0 | - | new | 0 | 44 | 0.00 |  | 1 | yes |  |  |
| 130 | 0x41e110 | - | new | 0 | 44 | 0.00 |  | 0 | skim |  |  |
| 131 | 0x463bd0 | 0x462590 | changed | 191 | 194 | 0.79 |  | 1 | yes |  |  |
| 132 | 0x46af20 | 0x469400 | changed | 177 | 189 | 0.78 |  | 1 | yes |  | calls 0->4 |
| 133 | 0x4790f0 | 0x477620 | changed | 63 | 58 | 0.70 | 2 | 1 | yes |  | calls 0->1 |
| 134 | 0x41eec0 | 0x41df50 | changed | 76 | 91 | 0.63 |  | 5 | yes | yes | calls 0->1 |
| 135 | 0x4615b0 | - | new | 0 | 22 | 0.00 | 4 | 3 | skim |  |  |
| 136 | 0x472420 | 0x470320 | changed | 24 | 26 | 0.38 | 2 | 4 | yes |  |  |
| 137 | 0x4778a0 | 0x475de0 | changed | 65 | 65 | 0.73 | 2 | 2 | yes |  |  |
| 138 | 0x472940 | 0x4707c0 | changed | 105 | 98 | 0.65 |  | 2 | yes |  |  |
| 139 | 0x472e60 | - | new | 0 | 35 | 0.00 |  | 3 | lib |  |  |
| 140 | 0x479950 | - | new | 0 | 40 | 0.00 |  | 0 | yes | yes |  |
| 141 | 0x46a9c0 | 0x468e20 | changed | 84 | 66 | 0.55 |  | 1 | yes |  |  |
| 142 | 0x4628c0 | 0x461510 | changed | 98 | 90 | 0.60 |  | 0 | yes |  | calls 4->0, consts 1->0 |
| 143 | 0x472cb0 | - | new | 0 | 37 | 0.00 |  | 1 | yes |  |  |
| 144 | 0x465da0 | - | new | 0 | 18 | 0.00 | 3 | 3 | yes | yes |  |
| 145 | 0x4638f0 | 0x4622a0 | changed | 76 | 75 | 0.82 | 1 | 3 | yes | yes |  |
| 146 | 0x4255f0 | 0x423c90 | changed | 30 | 34 | 0.50 | 2 | 1 | yes |  | consts 1->0 |
| 147 | 0x415940 | 0x414f40 | changed | 292 | 295 | 0.88 |  | 1 | yes |  | calls 1->4 |
| 148 | 0x4210d0 | 0x4200c0 | changed | 63 | 65 | 0.47 |  | 1 | yes |  | calls 4->0 |
| 149 | 0x427a30 | 0x425bd0 | changed | 114 | 105 | 0.79 |  | 18 | yes |  |  |
| 150 | 0x474010 | 0x471de0 | changed | 183 | 152 | 0.80 |  | 0 | yes |  |  |
| 151 | 0x4617a0 | 0x460350 | changed | 284 | 272 | 0.88 |  | 1 | yes |  | consts 0->1 |
| 152 | 0x46a8e0 | 0x468c00 | changed | 62 | 66 | 0.50 |  | 1 | yes |  | calls 0->2 |
| 153 | 0x41f8d0 | - | new | 0 | 14 | 0.00 | 2 | 3 | yes | yes |  |
| 154 | 0x476a20 | 0x474cd0 | changed | 81 | 62 | 0.60 |  | 1 | yes |  |  |
| 155 | 0x415190 | 0x4148c0 | changed | 102 | 133 | 0.76 |  | 1 | yes |  | calls 1->2, consts 0->1 |
| 156 | 0x466380 | 0x464af0 | changed | 139 | 152 | 0.78 |  | 0 | yes |  |  |
| 157 | 0x467d10 | 0x4663e0 | changed | 87 | 118 | 0.72 |  | 0 | yes |  | calls 0->2 |
| 158 | 0x4211b0 | 0x4201b0 | changed | 50 | 54 | 0.42 |  | 1 | yes |  | calls 7->0 |
| 159 | 0x41c570 | 0x41b840 | changed | 54 | 34 | 0.79 | 1 | 2 | yes | yes |  |
| 160 | 0x475be0 | 0x473e50 | changed | 86 | 106 | 0.79 |  | 7 | yes |  |  |
| 161 | 0x460c20 | 0x45f980 | changed | 46 | 64 | 0.55 |  | 1 | yes |  |  |
| 162 | 0x465150 | 0x463aa0 | changed | 49 | 62 | 0.51 |  | 0 | yes |  |  |
| 163 | 0x467200 | 0x465940 | changed | 31 | 41 | 0.33 |  | 2 | yes |  | consts 0->1 |
| 164 | 0x41b930 | - | new | 0 | 30 | 0.00 |  | 0 | yes |  |  |
| 165 | 0x479c60 | - | new | 0 | 27 | 0.00 |  | 2 | yes | yes |  |
| 166 | 0x4799f0 | - | new | 0 | 28 | 0.00 |  | 1 | yes | yes |  |
| 167 | 0x476260 | 0x4744a0 | changed | 43 | 34 | 0.37 |  | 1 | yes |  |  |
| 168 | 0x4266d0 | 0x424a90 | changed | 82 | 102 | 0.73 |  | 1 | yes |  |  |
| 169 | 0x41e3b0 | 0x41d5f0 | changed | 72 | 78 | 0.68 |  | 3 | yes |  | calls 0->2 |
| 170 | 0x419d50 | - | new | 0 | 28 | 0.00 |  | 0 | yes |  |  |
| 171 | 0x426af0 | 0x424e90 | changed | 71 | 63 | 0.70 |  | 6 | yes |  |  |
| 172 | 0x468660 | 0x466c50 | changed | 119 | 84 | 0.77 |  | 0 | yes |  |  |
| 173 | 0x460f60 | 0x45fbe0 | changed | 97 | 105 | 0.82 |  | 8 | yes |  |  |
| 174 | 0x4738b0 | - | new | 0 | 22 | 0.00 |  | 4 | yes |  |  |
| 175 | 0x427430 | 0x4255b0 | changed | 49 | 61 | 0.65 |  | 4 | yes |  |  |
| 176 | 0x468050 | 0x4666f0 | changed | 117 | 128 | 0.81 |  | 1 | yes |  |  |
| 177 | 0x473240 | 0x470bc0 | changed | 40 | 45 | 0.63 |  | 21 | yes |  |  |
| 178 | 0x4285d0 | 0x426960 | changed | 131 | 128 | 0.84 |  | 4 | yes |  | calls 1->0 |
| 179 | 0x465bd0 | 0x4643d0 | changed | 71 | 83 | 0.70 |  | 0 | yes |  |  |
| 180 | 0x479c30 | - | new | 0 | 22 | 0.00 |  | 2 | yes | yes |  |
| 181 | 0x467770 | 0x465ea0 | changed | 42 | 31 | 0.67 | 5 | 3 | yes |  |  |
| 182 | 0x468ff0 | - | new | 0 | 21 | 0.00 |  | 3 | skim |  |  |
| 183 | 0x41df80 | - | new | 0 | 24 | 0.00 |  | 0 | yes |  |  |
| 184 | 0x465ea0 | 0x464600 | changed | 76 | 79 | 0.70 |  | 0 | yes |  |  |
| 185 | 0x4768e0 | 0x474b70 | changed | 42 | 36 | 0.47 |  | 1 | yes |  |  |
| 186 | 0x4741a0 | 0x471fb0 | changed | 201 | 194 | 0.88 |  | 0 | yes |  |  |
| 187 | 0x41dcd0 | 0x41ce60 | changed | 42 | 40 | 0.79 | 1 | 1 | yes |  |  |
| 188 | 0x417270 | - | new | 0 | 21 | 0.00 |  | 2 | skim |  |  |
| 189 | 0x414c00 | 0x4143c0 | changed | 38 | 56 | 0.68 |  | 6 | yes |  |  |
| 190 | 0x477760 | 0x475c80 | changed | 38 | 38 | 0.69 | 3 | 1 | yes |  |  |
| 191 | 0x414370 | 0x413af0 | changed | 50 | 46 | 0.81 | 2 | 2 | yes |  |  |
| 192 | 0x428f10 | 0x427280 | changed | 36 | 41 | 0.48 |  | 1 | yes |  |  |
| 193 | 0x416560 | 0x415b00 | changed | 101 | 103 | 0.79 |  | 1 | yes |  |  |
| 194 | 0x476200 | 0x474430 | changed | 48 | 38 | 0.56 |  | 1 | yes |  |  |
| 195 | 0x476940 | 0x474bd0 | changed | 110 | 93 | 0.83 |  | 3 | yes |  |  |
| 196 | 0x427d50 | 0x425f30 | changed | 61 | 45 | 0.71 |  | 5 | yes |  |  |
| 197 | 0x423330 | 0x422360 | changed | 138 | 133 | 0.85 |  | 1 | yes |  | consts 0->1 |
| 198 | 0x426ee0 | 0x425090 | changed | 55 | 56 | 0.67 |  | 3 | yes |  |  |
| 199 | 0x41b900 | - | new | 0 | 21 | 0.00 |  | 0 | yes |  |  |
| 200 | 0x465e00 | - | new | 0 | 19 | 0.00 |  | 2 | yes | yes |  |
| 201 | 0x4665a0 | 0x464ce0 | changed | 148 | 130 | 0.88 |  | 2 | yes |  |  |
| 202 | 0x415160 | - | new | 0 | 19 | 0.00 |  | 1 | inside |  |  |
| 203 | 0x46b4b0 | - | new | 0 | 19 | 0.00 |  | 1 | lib | yes |  |
| 204 | 0x460eb0 | 0x45fb50 | changed | 14 | 17 | 0.60 | 2 | 7 | yes |  | calls 1->0 |
| 205 | 0x41de60 | 0x41d070 | changed | 55 | 51 | 0.71 |  | 5 | yes | yes | calls 1->0 |
| 206 | 0x477e40 | 0x4764a0 | changed | 65 | 68 | 0.89 | 2 | 5 | yes |  |  |
| 207 | 0x417ea0 | 0x417300 | changed | 48 | 45 | 0.61 |  | 1 | yes |  | calls 1->0 |
| 208 | 0x427830 | 0x4259c0 | changed | 151 | 151 | 0.88 |  | 1 | yes |  | consts 1->2 |
| 209 | 0x4660a0 | 0x464830 | changed | 87 | 113 | 0.83 |  | 0 | yes |  | calls 0->2 |
| 210 | 0x420390 | 0x41f680 | changed | 105 | 117 | 0.85 |  | 1 | yes |  |  |
| 211 | 0x465e20 | 0x464580 | changed | 47 | 49 | 0.65 |  | 2 | yes |  |  |
| 212 | 0x41faf0 | 0x41ee80 | changed | 28 | 31 | 0.42 |  | 1 | yes |  | calls 3->0 |
| 213 | 0x468cd0 | 0x467290 | changed | 36 | 41 | 0.59 |  | 2 | yes |  |  |
| 214 | 0x4158f0 | - | new | 0 | 18 | 0.00 |  | 0 | skim |  |  |
| 215 | 0x460f00 | 0x45fb90 | changed | 13 | 16 | 0.50 | 2 | 1 | yes |  | consts 0->2 |
| 216 | 0x475390 | 0x473480 | changed | 63 | 56 | 0.72 |  | 0 | yes |  |  |
| 217 | 0x465210 | 0x463b40 | changed | 115 | 111 | 0.85 |  | 0 | yes |  | consts 3->2 |
| 218 | 0x4287f0 | 0x426b60 | changed | 37 | 41 | 0.59 |  | 0 | yes |  |  |
| 219 | 0x41f900 | - | new | 0 | 16 | 0.00 |  | 1 | yes | yes |  |
| 220 | 0x41c380 | 0x41b5f0 | changed | 25 | 24 | 0.56 |  | 10 | yes |  |  |
| 221 | 0x463650 | 0x462040 | changed | 28 | 38 | 0.57 |  | 0 | yes |  |  |
| 222 | 0x41ee80 | - | new | 0 | 14 | 0.00 |  | 3 | yes |  |  |
| 223 | 0x477c80 | 0x476320 | changed | 97 | 103 | 0.85 |  | 1 | yes |  | calls 2->0 |
| 224 | 0x426bd0 | 0x424f60 | changed | 78 | 82 | 0.84 |  | 4 | yes |  | strings 0->1 |
| 225 | 0x41fc00 | 0x41ef90 | changed | 32 | 33 | 0.58 |  | 3 | yes |  | calls 4->0 |
| 226 | 0x473930 | 0x471540 | changed | 171 | 149 | 0.92 |  | 2 | yes |  | calls 1->0 |
| 227 | 0x416710 | 0x415ca0 | changed | 107 | 113 | 0.87 |  | 1 | yes |  |  |
| 228 | 0x461120 | 0x45fd40 | changed | 45 | 54 | 0.78 |  | 5 | yes |  |  |
| 229 | 0x468850 | 0x466e40 | changed | 23 | 17 | 0.50 |  | 6 | yes |  |  |
| 230 | 0x472d10 | 0x470b40 | changed | 27 | 29 | 0.51 |  | 1 | yes |  | consts 1->0 |
| 231 | 0x460e50 | 0x45faf0 | changed | 31 | 34 | 0.58 |  | 1 | yes |  | calls 1->0 |
| 232 | 0x479940 | - | new | 0 | 13 | 0.00 |  | 2 | yes |  |  |
| 233 | 0x41f0c0 | 0x41e5a0 | changed | 36 | 37 | 0.63 |  | 1 | yes |  |  |
| 234 | 0x468a40 | 0x466fe0 | changed | 71 | 74 | 0.81 |  | 0 | yes |  |  |
| 235 | 0x466550 | 0x464c90 | changed | 25 | 25 | 0.75 | 2 | 1 | yes |  |  |
| 236 | 0x462f30 | - | new | 0 | 13 | 0.00 |  | 1 | skim |  |  |
| 237 | 0x4184f0 | 0x417a10 | changed | 27 | 16 | 0.55 |  | 2 | yes |  |  |
| 238 | 0x41f340 | 0x41e7a0 | changed | 17 | 22 | 0.82 | 1 | 7 | yes | yes |  |
| 239 | 0x477960 | 0x475ea0 | changed | 45 | 44 | 0.87 | 2 | 1 | yes |  |  |
| 240 | 0x4743f0 | 0x472210 | changed | 93 | 96 | 0.86 |  | 0 | yes |  |  |
| 241 | 0x41ffa0 | 0x41f340 | changed | 153 | 155 | 0.92 |  | 2 | yes | yes | calls 1->0 |
| 242 | 0x41ec10 | 0x41dca0 | changed | 97 | 94 | 0.86 |  | 0 | yes |  |  |
| 243 | 0x41ed00 | 0x41dd90 | changed | 97 | 94 | 0.86 |  | 0 | yes |  |  |
| 244 | 0x463330 | 0x461d30 | changed | 28 | 29 | 0.57 |  | 1 | yes |  |  |
| 245 | 0x41e180 | 0x41d380 | changed | 33 | 31 | 0.62 |  | 0 | skim |  |  |
| 246 | 0x460cd0 | 0x45fa00 | changed | 26 | 38 | 0.69 |  | 1 | yes |  |  |
| 247 | 0x467b70 | 0x466240 | changed | 11 | 11 | 0.57 | 1 | 1 | yes |  |  |
| 248 | 0x467f80 | 0x466610 | changed | 63 | 58 | 0.81 |  | 0 | yes |  |  |
| 249 | 0x465cd0 | 0x4644b0 | changed | 63 | 64 | 0.81 |  | 0 | yes |  |  |
| 250 | 0x4727b0 | - | new | 0 | 12 | 0.00 |  | 0 | skim |  |  |
| 251 | 0x4283f0 | 0x426780 | changed* | 163 | 158 | 0.93 | 1 | 9 | yes | yes |  |
| 252 | 0x41dbb0 | 0x41cd40 | changed | 29 | 28 | 0.59 |  | 0 | yes |  | calls 1->0 |
| 253 | 0x463f10 | 0x4628c0 | changed | 46 | 44 | 0.76 |  | 1 | yes |  | calls 1->0, imports 1->0 |
| 254 | 0x41e1f0 | 0x41d400 | changed | 46 | 35 | 0.75 |  | 0 | yes |  |  |
| 255 | 0x463770 | 0x462130 | changed | 61 | 67 | 0.83 |  | 0 | yes |  |  |
| 256 | 0x475460 | 0x473570 | changed | 55 | 48 | 0.81 |  | 2 | yes |  |  |
| 257 | 0x41f930 | - | new | 0 | 9 | 0.00 |  | 5 | yes |  |  |
| 258 | 0x4158d0 | - | new | 0 | 11 | 0.00 |  | 0 | skim |  |  |
| 259 | 0x4158e0 | - | new | 0 | 11 | 0.00 |  | 0 | skim |  |  |
| 260 | 0x41c9d0 | - | new | 0 | 11 | 0.00 |  | 0 | skim |  |  |
| 261 | 0x41e7e0 | 0x41d890 | changed | 56 | 63 | 0.83 |  | 0 | yes | yes |  |
| 262 | 0x4262a0 | 0x424760 | changed | 17 | 17 | 0.57 |  | 13 | yes |  | consts 3->2 |
| 263 | 0x41f310 | 0x41e770 | changed | 12 | 14 | 0.80 | 1 | 14 | yes | yes |  |
| 264 | 0x463e90 | 0x462860 | changed | 14 | 24 | 0.57 |  | 0 | yes |  |  |
| 265 | 0x467290 | 0x4659c0 | changed | 17 | 21 | 0.53 |  | 0 | yes |  |  |
| 266 | 0x425d50 | 0x424350 | changed | 26 | 14 | 0.75 |  | 12 | yes |  |  |
| 267 | 0x461480 | 0x460050 | changed | 39 | 36 | 0.80 |  | 5 | yes |  |  |
| 268 | 0x465fc0 | 0x464720 | changed | 50 | 48 | 0.81 |  | 0 | yes |  |  |
| 269 | 0x41ad50 | 0x419e60 | changed | 39 | 38 | 0.79 |  | 2 | yes | yes |  |
| 270 | 0x465590 | 0x463ef0 | changed | 46 | 43 | 0.81 |  | 0 | yes |  |  |
| 271 | 0x419d90 | 0x418f70 | changed | 14 | 14 | 0.73 | 2 | 1 | yes |  |  |
| 272 | 0x41f770 | 0x41e9c0 | changed | 11 | 12 | 0.73 | 1 | 1 | yes |  |  |
| 273 | 0x41e070 | 0x41d280 | changed | 17 | 17 | 0.50 |  | 0 | skim |  |  |
| 274 | 0x4773e0 | 0x4757f0 | changed | 52 | 53 | 0.85 |  | 1 | yes |  |  |
| 275 | 0x4166d0 | 0x415c70 | changed | 21 | 24 | 0.69 |  | 2 | yes |  |  |
| 276 | 0x4639f0 | 0x4623a0 | changed | 72 | 70 | 0.89 |  | 0 | yes |  | consts 1->0 |
| 277 | 0x428fd0 | 0x427320 | changed | 61 | 61 | 0.88 |  | 2 | yes |  |  |
| 278 | 0x41f000 | 0x41e4e0 | changed | 46 | 47 | 0.85 |  | 2 | yes | yes |  |
| 279 | 0x463af0 | 0x4624b0 | changed | 61 | 62 | 0.87 |  | 0 | yes |  |  |
| 280 | 0x41ee40 | 0x41ded0 | changed | 19 | 17 | 0.58 |  | 0 | yes |  |  |
| 281 | 0x426f60 | 0x425110 | changed | 65 | 63 | 0.88 |  | 1 | yes |  |  |
| 282 | 0x417e00 | 0x41eb90 | changed | 10 | 14 | 0.75 | 2 | 1 | yes |  |  |
| 283 | 0x41f920 | - | new | 0 | 7 | 0.00 |  | 2 | skim |  |  |
| 284 | 0x424220 | 0x422d20 | changed* | 156 | 156 | 0.94 | 2 | 2 | yes |  |  |
| 285 | 0x415910 | - | new | 0 | 7 | 0.00 |  | 0 | skim |  |  |
| 286 | 0x41e280 | - | new | 0 | 6 | 0.00 |  | 3 | skim |  |  |
| 287 | 0x462ae0 | 0x461730 | changed | 57 | 58 | 0.89 |  | 2 | yes |  |  |
| 288 | 0x414950 | 0x414150 | changed | 35 | 39 | 0.83 |  | 1 | yes |  |  |
| 289 | 0x415340 | 0x4149f0 | changed* | 275 | 284 | 0.95 |  | 3 | yes |  |  |
| 290 | 0x477f30 | 0x476550 | changed | 21 | 23 | 0.88 | 2 | 1 | yes |  |  |
| 291 | 0x41dc00 | 0x41cd90 | changed | 14 | 13 | 0.57 |  | 0 | yes |  | calls 1->0 |
| 292 | 0x4662c0 | 0x464a20 | changed | 22 | 19 | 0.73 |  | 0 | yes |  |  |
| 293 | 0x41f390 | 0x41e7e0 | changed | 9 | 13 | 0.80 | 2 | 1 | yes |  |  |
| 294 | 0x466210 | 0x464950 | changed | 36 | 36 | 0.84 |  | 0 | yes |  |  |
| 295 | 0x4614e0 | 0x4600b0 | changed | 19 | 20 | 0.75 |  | 3 | yes |  |  |
| 296 | 0x4168b0 | 0x415e40 | changed* | 247 | 272 | 0.91 |  | 1 | yes |  |  |
| 297 | 0x463600 | 0x462000 | changed | 21 | 21 | 0.77 |  | 2 | yes |  |  |
| 298 | 0x417290 | 0x416780 | changed* | 78 | 67 | 0.96 | 2 | 11 | yes |  |  |
| 299 | 0x4158b0 | - | new | 0 | 5 | 0.00 |  | 0 | skim |  |  |
| 300 | 0x41de00 | 0x4092c0 | changed | 11 | 11 | 0.55 |  | 0 | yes |  | calls 1->2 |
| 301 | 0x41fc70 | 0x41f000 | changed | 74 | 74 | 0.95 |  | 4 | yes | yes | calls 1->0 |
| 302 | 0x421260 | 0x420260 | changed | 28 | 28 | 0.83 |  | 0 | yes |  |  |
| 303 | 0x4675e0 | 0x465d10 | changed* | 151 | 150 | 0.95 | 6 | 2 | yes |  |  |
| 304 | 0x417e30 | 0x41eca0 | changed | 7 | 10 | 0.83 | 2 | 5 | yes |  |  |
| 305 | 0x4745a0 | 0x4723c0 | changed* | 209 | 202 | 0.99 |  | 1 | yes |  |  |
| 306 | 0x41c680 | 0x41b980 | changed | 19 | 17 | 0.77 |  | 0 | skim |  |  |
| 307 | 0x421e00 | 0x420d00 | changed | 71 | 71 | 0.95 |  | 2 | yes |  | calls 1->0 |
| 308 | 0x41dd40 | 0x41cee0 | changed | 26 | 24 | 0.84 |  | 0 | skim |  |  |
| 309 | 0x415ce0 | 0x4152d0 | changed* | 190 | 140 | 1.00 |  | 2 | yes |  |  |
| 310 | 0x467b30 | 0x466200 | changed | 11 | 11 | 0.86 | 1 | 1 | yes |  |  |
| 311 | 0x41a020 | 0x419200 | changed* | 76 | 67 | 0.96 | 1 | 1 | yes |  |  |
| 312 | 0x4654e0 | 0x463e40 | changed* | 64 | 64 | 1.00 | 1 | 4 | yes |  |  |
| 313 | 0x425770 | 0x423e00 | changed* | 179 | 168 | 0.91 |  | 1 | yes |  |  |
| 314 | 0x474cd0 | 0x472bc0 | changed | 22 | 22 | 0.83 |  | 0 | yes |  |  |
| 315 | 0x474d50 | 0x472c40 | changed | 22 | 22 | 0.83 |  | 0 | yes |  |  |
| 316 | 0x426b80 | 0x424f30 | changed | 13 | 20 | 0.83 |  | 1 | yes |  |  |
| 317 | 0x421fc0 | 0x420ec0 | changed | 36 | 35 | 0.92 |  | 2 | yes |  | calls 1->0 |
| 318 | 0x466280 | 0x4649c0 | changed | 9 | 7 | 0.67 |  | 0 | yes |  |  |
| 319 | 0x4662a0 | 0x4649f0 | changed | 9 | 7 | 0.67 |  | 0 | yes |  |  |
| 320 | 0x41fbd0 | 0x41ef60 | changed | 12 | 12 | 0.75 |  | 0 | yes |  |  |
| 321 | 0x4672d0 | 0x465a00 | changed* | 63 | 63 | 1.00 | 3 | 5 | yes | yes |  |
| 322 | 0x41b9d0 | 0x41ac10 | changed | 16 | 13 | 0.83 |  | 2 | yes |  |  |
| 323 | 0x4616c0 | 0x460280 | changed* | 65 | 71 | 0.94 | 3 | 2 | yes |  |  |
| 324 | 0x419bb0 | 0x418e00 | changed* | 60 | 64 | 0.95 | 2 | 1 | yes |  |  |
| 325 | 0x4779e0 | 0x475f20 | changed* | 50 | 50 | 1.00 | 1 | 2 | yes |  |  |
| 326 | 0x41c1a0 | 0x41b420 | changed* | 137 | 136 | 0.90 |  | 0 | yes |  |  |
| 327 | 0x46ac20 | 0x4690d0 | changed* | 128 | 129 | 0.92 |  | 1 | yes |  |  |
| 328 | 0x41fde0 | 0x41f180 | changed* | 123 | 123 | 1.00 |  | 2 | yes |  |  |
| 329 | 0x463840 | 0x4621f0 | changed* | 52 | 51 | 0.96 | 2 | 3 | yes |  |  |
| 330 | 0x427ca0 | 0x425e60 | changed | 9 | 11 | 0.80 |  | 3 | yes |  |  |
| 331 | 0x473e70 | 0x471c10 | changed* | 90 | 88 | 1.00 |  | 8 | yes |  |  |
| 332 | 0x41e2d0 | 0x41d510 | changed* | 39 | 39 | 1.00 | 2 | 32 | yes |  |  |
| 333 | 0x427cc0 | 0x425e80 | changed | 13 | 15 | 0.86 |  | 3 | yes |  |  |
| 334 | 0x417170 | 0x416660 | changed* | 54 | 47 | 0.92 | 2 | 1 | yes |  |  |
| 335 | 0x422260 | 0x421120 | changed | 12 | 11 | 0.80 |  | 0 | yes |  | calls 1->0 |
| 336 | 0x474e10 | 0x472da0 | changed* | 106 | 108 | 0.94 |  | 2 | yes |  |  |
| 337 | 0x465780 | 0x464000 | changed* | 107 | 109 | 0.98 |  | 1 | yes |  |  |
| 338 | 0x461650 | 0x460210 | changed* | 34 | 34 | 1.00 | 2 | 12 | yes |  |  |
| 339 | 0x41dca0 | 0x41ce30 | changed | 15 | 15 | 0.86 |  | 0 | yes |  |  |
| 340 | 0x414b80 | 0x414370 | changed | 5 | 7 | 0.75 |  | 3 | yes |  |  |
| 341 | 0x467490 | 0x465bc0 | changed* | 66 | 64 | 1.00 | 7 | 4 | yes |  |  |
| 342 | 0x461430 | 0x460000 | changed* | 30 | 29 | 1.00 | 2 | 102 | yes |  |  |
| 343 | 0x475150 | 0x4730e0 | changed | 10 | 13 | 0.86 |  | 0 | yes |  |  |
| 344 | 0x475180 | 0x473110 | changed | 10 | 13 | 0.86 |  | 0 | yes |  |  |
| 345 | 0x475200 | 0x4731c0 | changed | 9 | 11 | 0.83 |  | 0 | yes |  |  |
| 346 | 0x464ca0 | 0x4635e0 | changed* | 82 | 82 | 0.99 |  | 2 | yes |  |  |
| 347 | 0x4673a0 | 0x465ad0 | changed* | 80 | 79 | 0.90 |  | 2 | yes |  |  |
| 348 | 0x41e010 | 0x41d220 | changed | 7 | 7 | 0.75 |  | 0 | skim |  |  |
| 349 | 0x475d10 | 0x473f40 | changed* | 65 | 67 | 0.96 |  | 6 | yes |  |  |
| 350 | 0x420e40 | 0x432a80 | changed* | 65 | 58 | 0.92 |  | 5 | yes |  |  |
| 351 | 0x467e90 | 0x466510 | changed* | 81 | 76 | 0.96 |  | 0 | yes |  |  |
| 352 | 0x425670 | 0x423d00 | changed* | 66 | 67 | 0.97 |  | 4 | yes |  |  |
| 353 | 0x461510 | 0x4600e0 | changed* | 72 | 69 | 0.93 |  | 2 | yes |  |  |
| 354 | 0x419dc0 | 0x418fa0 | changed | 6 | 6 | 0.75 |  | 1 | yes |  |  |
| 355 | 0x4751b0 | 0x473140 | changed | 9 | 11 | 0.86 |  | 0 | yes |  |  |
| 356 | 0x4738e0 | 0x4714c0 | changed* | 52 | 33 | 1.00 |  | 20 | yes |  |  |
| 357 | 0x4234d0 | 0x422500 | changed* | 72 | 72 | 1.00 |  | 1 | yes |  |  |
| 358 | 0x422280 | 0x421140 | changed* | 67 | 73 | 0.91 |  | 0 | yes |  |  |
| 359 | 0x427cf0 | 0x425ea0 | changed* | 65 | 47 | 1.00 |  | 2 | yes |  |  |
| 360 | 0x467bb0 | 0x466280 | changed* | 65 | 65 | 0.98 |  | 2 | yes |  |  |
| 361 | 0x46ab20 | 0x468fa0 | changed* | 62 | 67 | 0.95 |  | 1 | yes |  |  |
| 362 | 0x468910 | 0x466ee0 | changed | 12 | 12 | 0.89 |  | 1 | yes |  |  |
| 363 | 0x468c10 | 0x4671a0 | changed | 12 | 12 | 0.89 |  | 1 | yes |  |  |
| 364 | 0x41d880 | 0x41ca30 | changed | 9 | 7 | 1.00 | 2 | 9 | yes |  | consts 1->0 |
| 365 | 0x473b30 | 0x4717a0 | changed* | 63 | 44 | 0.94 |  | 2 | yes |  |  |
| 366 | 0x475aa0 | 0x473ca0 | changed* | 65 | 52 | 1.00 |  | 1 | yes |  |  |
| 367 | 0x420fd0 | 0x41ffc0 | changed* | 64 | 64 | 1.00 |  | 1 | yes |  |  |
| 368 | 0x46aa70 | 0x468f00 | changed* | 59 | 63 | 0.94 |  | 1 | yes |  |  |
| 369 | 0x416d60 | 0x41ec60 | changed | 3 | 3 | 0.57 |  | 0 | yes |  |  |
| 370 | 0x428200 | 0x4265a0 | changed* | 54 | 53 | 0.90 |  | 3 | yes |  |  |
| 371 | 0x41f940 | 0x409980 | changed* | 25 | 23 | 1.00 | 2 | 3 | yes |  |  |
| 372 | 0x425ca0 | 0x4242a0 | changed* | 56 | 55 | 1.00 |  | 2 | yes |  |  |
| 373 | 0x41e760 | 0x41d810 | changed* | 42 | 39 | 1.00 |  | 9 | yes | yes |  |
| 374 | 0x4759b0 | 0x473bb0 | changed* | 57 | 57 | 0.99 |  | 1 | yes |  |  |
| 375 | 0x477a70 | 0x475fb0 | changed* | 56 | 56 | 1.00 |  | 1 | yes |  |  |
| 376 | 0x474510 | 0x472330 | changed* | 53 | 53 | 1.00 |  | 0 | yes |  |  |
| 377 | 0x4615d0 | 0x460190 | changed* | 49 | 49 | 1.00 |  | 1 | yes |  |  |
| 378 | 0x420f40 | 0x41ff30 | changed* | 41 | 39 | 1.00 |  | 5 | yes |  |  |
| 379 | 0x474d90 | 0x472cf0 | changed* | 41 | 29 | 1.00 |  | 5 | yes |  |  |
| 380 | 0x4148c0 | 0x4140c0 | changed* | 46 | 46 | 1.00 |  | 2 | yes |  |  |
| 381 | 0x425d80 | 0x4243c0 | changed* | 44 | 43 | 1.00 |  | 1 | yes |  |  |
| 382 | 0x425de0 | 0x424420 | changed* | 44 | 43 | 1.00 |  | 1 | yes |  |  |
| 383 | 0x4277f0 | 0x425960 | changed* | 43 | 27 | 0.93 |  | 1 | yes |  |  |
| 384 | 0x4143f0 | 0x413b70 | changed* | 20 | 20 | 1.00 | 2 | 1 | yes |  |  |
| 385 | 0x468b70 | 0x467100 | changed* | 44 | 43 | 1.00 |  | 0 | yes |  |  |
| 386 | 0x41dd90 | 0x41cf30 | changed* | 37 | 35 | 0.95 |  | 2 | yes |  |  |
| 387 | 0x41c6d0 | 0x41b9d0 | changed* | 40 | 40 | 1.00 |  | 0 | yes |  |  |
| 388 | 0x4232a0 | 0x4222e0 | changed* | 33 | 35 | 0.94 |  | 1 | yes |  |  |
| 389 | 0x4636c0 | 0x4620a0 | changed* | 35 | 35 | 0.97 |  | 1 | yes |  |  |
| 390 | 0x41c5d0 | 0x41b8d0 | changed* | 34 | 34 | 0.95 |  | 1 | yes |  |  |
| 391 | 0x41fa40 | 0x41edd0 | changed* | 33 | 33 | 1.00 |  | 1 | yes |  |  |
| 392 | 0x4643f0 | 0x462d80 | changed* | 17 | 17 | 1.00 | 4 | 4 | yes |  |  |
| 393 | 0x421ef0 | 0x420df0 | changed* | 31 | 31 | 1.00 |  | 1 | yes |  |  |
| 394 | 0x464e00 | 0x463740 | changed* | 31 | 31 | 1.00 |  | 1 | yes |  |  |
| 395 | 0x419fb0 | 0x419190 | changed* | 27 | 30 | 1.00 |  | 1 | yes | yes |  |
| 396 | 0x473170 | 0x470ce0 | changed* | 21 | 21 | 0.96 |  | 26 | yes |  |  |
| 397 | 0x41e2b0 | 0x41d4f0 | changed* | 8 | 8 | 1.00 | 1 | 25 | yes |  |  |
| 398 | 0x41edf0 | 0x41de80 | changed* | 25 | 25 | 1.00 |  | 2 | yes |  |  |
| 399 | 0x427bc0 | 0x425d80 | changed* | 19 | 19 | 1.00 |  | 8 | yes |  |  |
| 400 | 0x41f9e0 | 0x41ed70 | changed* | 25 | 25 | 1.00 |  | 1 | unread |  |  |
| 401 | 0x467cb0 | 0x466380 | changed* | 25 | 25 | 1.00 |  | 1 | yes |  |  |
| 402 | 0x474f40 | 0x472ec0 | changed* | 25 | 25 | 1.00 |  | 1 | yes |  |  |
| 403 | 0x463ff0 | 0x4629b0 | changed* | 23 | 26 | 0.93 |  | 0 | yes |  |  |
| 404 | 0x475a60 | 0x473c60 | changed* | 24 | 23 | 1.00 |  | 1 | yes |  |  |
| 405 | 0x417220 | 0x416720 | changed* | 11 | 11 | 1.00 | 2 | 1 | yes |  |  |
| 406 | 0x475ba0 | 0x473e10 | changed* | 19 | 22 | 0.92 |  | 2 | yes |  |  |
| 407 | 0x41f980 | 0x41ed10 | changed* | 23 | 23 | 1.00 |  | 1 | yes |  |  |
| 408 | 0x41fd60 | 0x41f100 | changed* | 23 | 23 | 1.00 |  | 1 | yes |  |  |
| 409 | 0x4632f0 | 0x461cf0 | changed* | 18 | 21 | 0.90 |  | 2 | yes |  |  |
| 410 | 0x41eb40 | 0x41dbd0 | changed* | 23 | 23 | 1.00 |  | 0 | yes |  |  |
| 411 | 0x468750 | 0x466da0 | changed* | 21 | 21 | 0.95 |  | 1 | yes |  |  |
| 412 | 0x428190 | 0x426440 | changed* | 20 | 18 | 1.00 |  | 2 | unread |  |  |
| 413 | 0x4689e0 | 0x466f80 | changed* | 17 | 17 | 1.00 |  | 5 | yes |  |  |
| 414 | 0x41df20 | 0x41d130 | changed* | 20 | 20 | 1.00 |  | 1 | yes |  |  |
| 415 | 0x466300 | 0x464a70 | changed* | 21 | 21 | 1.00 |  | 0 | yes |  |  |
| 416 | 0x41c630 | 0x41b930 | changed* | 20 | 18 | 0.92 |  | 0 | yes |  |  |
| 417 | 0x420f10 | 0x41fef0 | changed* | 18 | 18 | 1.00 |  | 1 | yes |  |  |
| 418 | 0x427bf0 | 0x425db0 | changed* | 11 | 12 | 1.00 |  | 13 | yes |  |  |
| 419 | 0x41e030 | 0x41d240 | changed* | 17 | 17 | 1.00 |  | 0 | yes |  |  |
| 420 | 0x41e0b0 | 0x41d2c0 | changed* | 16 | 16 | 1.00 |  | 0 | unread |  |  |
| 421 | 0x41ebb0 | 0x41dc40 | changed* | 15 | 15 | 1.00 |  | 0 | unread |  |  |
| 422 | 0x41ebe0 | 0x41dc70 | changed* | 15 | 15 | 1.00 |  | 0 | yes |  |  |
| 423 | 0x466350 | 0x464ac0 | changed* | 12 | 12 | 1.00 |  | 2 | yes |  |  |
| 424 | 0x479ca0 | 0x4779f0 | changed* | 11 | 10 | 1.00 |  | 1 | yes |  |  |
| 425 | 0x41f0a0 | 0x41e580 | changed* | 10 | 10 | 1.00 |  | 3 | yes |  |  |
| 426 | 0x41dfc0 | 0x41d1d0 | changed* | 11 | 11 | 1.00 |  | 0 | yes |  |  |
| 427 | 0x41e290 | 0x41d4d0 | changed* | 8 | 8 | 1.00 |  | 6 | yes |  |  |
| 428 | 0x426680 | 0x426540 | changed* | 9 | 8 | 1.00 |  | 3 | yes |  |  |
| 429 | 0x41da90 | 0x41cc10 | changed* | 10 | 10 | 1.00 |  | 0 | yes | yes |  |
| 430 | 0x41e0f0 | 0x41d300 | changed* | 10 | 10 | 1.00 |  | 0 | unread |  |  |
| 431 | 0x41eb10 | 0x41dba0 | changed* | 10 | 10 | 1.00 |  | 0 | yes |  |  |
| 432 | 0x41e4c0 | 0x41d6e0 | changed* | 3 | 3 | 1.00 | 2 | 39 | unread |  |  |
| 433 | 0x424710 | 0x4231a0 | changed* | 4 | 4 | 1.00 | 2 | 1 | unread |  |  |
| 434 | 0x424720 | 0x4231b0 | changed* | 4 | 4 | 1.00 | 2 | 1 | unread |  |  |
| 435 | 0x428880 | 0x426bf0 | changed* | 8 | 8 | 1.00 |  | 0 | unread |  |  |
| 436 | 0x41f760 | 0x41e9b0 | changed* | 3 | 3 | 1.00 | 2 | 1 | unread |  |  |
| 437 | 0x41dc80 | 0x41ce10 | changed* | 5 | 5 | 1.00 |  | 5 | yes |  |  |
| 438 | 0x41df60 | 0x41d170 | changed* | 5 | 5 | 1.00 |  | 1 | yes |  |  |
| 439 | 0x41ad30 | 0x419e40 | changed* | 4 | 4 | 1.00 |  | 5 | yes |  |  |
| 440 | 0x41e260 | 0x41d480 | changed* | 5 | 5 | 1.00 |  | 0 | yes |  |  |
| 441 | 0x41f790 | 0x41e9e0 | changed* | 3 | 3 | 1.00 |  | 5 | yes |  |  |
| 442 | 0x41dfb0 | 0x41d1c0 | changed* | 3 | 3 | 1.00 |  | 0 | yes |  |  |
| 443 | 0x41dff0 | 0x41d200 | changed* | 3 | 3 | 1.00 |  | 0 | unread |  |  |
| 444 | 0x41e000 | 0x41d210 | changed* | 3 | 3 | 1.00 |  | 0 | yes |  |  |
| 445 | 0x425660 | 0x423cf0 | changed* | 2 | 2 | 1.00 |  | 5 | yes |  |  |
| 446 | 0x467480 | 0x409e80 | changed* | 2 | 2 | 1.00 |  | 1 | yes |  |  |
| 447 | 0x4158c0 | 0x40a830 | changed* | 2 | 2 | 1.00 |  | 0 | yes |  |  |
| 448 | 0x4158a0 | - | new | 0 | 0 | 0.00 |  | 0 | unread |  |  |
| 449 | 0x415920 | 0x414f20 | identical | 11 | 11 |  |  | 0 | yes |  |  |
| 450 | 0x415cd0 | - | new | 0 | 0 | 0.00 |  | 8 | unread |  |  |
| 451 | 0x416540 | 0x415ae0 | identical | 6 | 6 |  |  | 1 | yes |  |  |
| 452 | 0x417e50 | 0x41ecf0 | identical | 7 | 7 |  | 2 | 1 | yes |  |  |
| 453 | 0x41c3d0 | 0x41b640 | identical | 11 | 11 |  |  | 2 | yes |  |  |
| 454 | 0x41c520 | 0x41b7f0 | identical | 29 | 29 |  | 1 | 1 | yes |  |  |
| 455 | 0x41eea0 | 0x41df30 | identical | 10 | 10 |  |  | 0 | yes |  |  |
| 456 | 0x41f3d0 | - | new | 0 | 0 | 0.00 |  | 0 | unread | yes |  |
| 457 | 0x41f3d5 | - | new | 0 | 0 | 0.00 |  | 0 | yes | yes |  |
| 458 | 0x41f450 | - | new | 0 | 0 | 0.00 |  | 0 | unread |  |  |
| 459 | 0x41fac0 | 0x41ee50 | identical | 9 | 9 |  |  | 3 | unread |  |  |
| 460 | 0x420530 | 0x41f800 | identical | 35 | 35 |  |  | 3 | yes |  |  |
| 461 | 0x420600 | 0x41f8d0 | identical | 27 | 27 |  |  | 0 | yes | yes |  |
| 462 | 0x420660 | 0x41f930 | identical | 25 | 25 |  |  | 0 | yes |  |  |
| 463 | 0x4206b0 | 0x41f980 | identical | 23 | 23 |  |  | 0 | yes |  |  |
| 464 | 0x420700 | 0x41f9d0 | identical | 36 | 36 |  |  | 0 | yes |  |  |
| 465 | 0x420770 | 0x41fa40 | identical | 36 | 36 |  |  | 0 | yes |  |  |
| 466 | 0x4207e0 | 0x41fab0 | identical | 15 | 15 |  |  | 0 | yes |  |  |
| 467 | 0x420810 | 0x41fae0 | identical | 23 | 23 |  |  | 0 | yes |  |  |
| 468 | 0x420860 | 0x41fb70 | identical | 33 | 33 |  |  | 0 | yes |  |  |
| 469 | 0x4208d0 | 0x41fbe0 | identical | 14 | 14 |  |  | 0 | yes |  |  |
| 470 | 0x420900 | 0x41fc10 | identical | 13 | 13 |  |  | 0 | yes |  |  |
| 471 | 0x421330 | 0x420320 | identical | 27 | 27 |  |  | 0 | yes |  |  |
| 472 | 0x421380 | 0x420370 | identical | 23 | 23 |  |  | 0 | yes |  |  |
| 473 | 0x421db0 | 0x420cb0 | identical | 27 | 27 |  |  | 0 | yes |  |  |
| 474 | 0x422040 | 0x420f40 | identical | 15 | 15 |  | 1 | 5 | yes |  |  |
| 475 | 0x422140 | 0x421000 | identical | 26 | 26 |  | 2 | 1 | yes |  |  |
| 476 | 0x4221a0 | 0x421060 | identical | 11 | 11 |  |  | 1 | yes |  |  |
| 477 | 0x4221c0 | 0x421080 | identical | 25 | 25 |  | 2 | 2 | yes |  |  |
| 478 | 0x422210 | 0x4210d0 | identical | 25 | 25 |  | 2 | 2 | yes |  |  |
| 479 | 0x4266a0 | 0x426560 | identical | 16 | 16 |  |  | 3 | yes |  |  |
| 480 | 0x427420 | 0x4255a0 | identical | 6 | 6 |  |  | 1 | yes |  |  |
| 481 | 0x427c60 | 0x425e20 | identical | 7 | 7 |  |  | 6 | yes |  |  |
| 482 | 0x427c80 | 0x425e40 | identical | 12 | 12 |  |  | 5 | yes |  |  |
| 483 | 0x4282c0 | 0x426650 | identical | 44 | 44 |  |  | 8 | yes | yes |  |
| 484 | 0x4288a0 | 0x426c10 | identical | 36 | 36 |  |  | 0 | yes |  |  |
| 485 | 0x428910 | 0x426c80 | identical | 14 | 14 |  |  | 1 | yes |  |  |
| 486 | 0x428fb0 | 0x427300 | identical | 8 | 8 |  |  | 1 | yes |  |  |
| 487 | 0x460bc0 | 0x45b800 | identical | 38 | 38 |  |  | 24 | yes |  |  |
| 488 | 0x460d90 | 0x45fa70 | identical | 17 | 17 |  |  | 1 | yes |  |  |
| 489 | 0x460ef0 | 0x45fb80 | identical | 5 | 5 |  | 2 | 5 | yes |  |  |
| 490 | 0x460f40 | 0x45fbc0 | identical | 8 | 8 |  |  | 2 | yes |  |  |
| 491 | 0x4611c0 | 0x45fdd0 | identical | 20 | 20 |  |  | 6 | yes |  |  |
| 492 | 0x4629d0 | 0x461640 | identical | 13 | 13 |  |  | 0 | yes |  |  |
| 493 | 0x463ee0 | 0x462890 | identical | 16 | 16 |  |  | 0 | yes |  |  |
| 494 | 0x463fb0 | 0x462970 | identical | 2 | 2 |  | 3 | 4 | unread |  |  |
| 495 | 0x464430 | 0x462dc0 | identical | 4 | 4 |  | 2 | 1 | yes |  |  |
| 496 | 0x464770 | 0x463100 | identical | 5 | 5 |  |  | 2 | yes |  |  |
| 497 | 0x4654d0 | 0x463e30 | identical | 5 | 5 |  |  | 2 | unread |  |  |
| 498 | 0x465580 | 0x463ee0 | identical | 6 | 6 |  |  | 0 | yes |  |  |
| 499 | 0x467540 | 0x465c70 | identical | 63 | 63 |  |  | 2 | yes |  |  |
| 500 | 0x4677e0 | 0x465f20 | identical | 10 | 10 |  | 4 | 2 | yes |  |  |
| 501 | 0x468820 | 0x466e10 | identical | 19 | 19 |  |  | 0 | yes |  |  |
| 502 | 0x468890 | 0x466e80 | identical | 15 | 15 |  |  | 0 | yes |  |  |
| 503 | 0x4688e0 | 0x466eb0 | identical | 16 | 16 |  |  | 0 | yes |  |  |
| 504 | 0x468960 | 0x466f00 | identical | 16 | 16 |  |  | 0 | yes |  |  |
| 505 | 0x468d30 | 0x4672f0 | identical | 11 | 11 |  |  | 0 | yes |  |  |
| 506 | 0x468d80 | 0x4c4c60 | identical | 9 | 9 |  |  | 2 | unread |  |  |
| 507 | 0x46a8b0 | 0x468bd0 | identical | 11 | 11 |  |  | 1 | yes |  |  |
| 508 | 0x46a990 | 0x468df0 | identical | 13 | 13 |  |  | 1 | yes |  |  |
| 509 | 0x471210 | - | new | 0 | 0 | 0.00 |  | 0 | yes |  |  |
| 510 | 0x474b90 | 0x472a80 | identical | 70 | 70 |  |  | 2 | yes |  |  |
| 511 | 0x474c30 | 0x472b20 | identical | 49 | 49 |  |  | 4 | yes |  |  |
| 512 | 0x474ca0 | 0x472b90 | identical | 17 | 17 |  |  | 1 | yes |  |  |
| 513 | 0x474d10 | 0x472c00 | identical | 22 | 22 |  |  | 1 | yes |  |  |
| 514 | 0x475310 | 0x473400 | identical | 20 | 20 |  |  | 4 | yes |  |  |
| 515 | 0x4755b0 | 0x473700 | identical | 18 | 18 |  |  | 0 | yes |  |  |
| 516 | 0x476bf0 | 0x474ef0 | identical | 9 | 9 |  |  | 1 | unread |  |  |
| 517 | 0x477f70 | 0x476590 | identical | 7 | 7 |  | 4 | 1 | yes |  |  |
