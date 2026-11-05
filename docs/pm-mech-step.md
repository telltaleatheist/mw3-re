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

**Owed on `test_pm_mechsave` (the lead, 2026-11-05):** byte damage on a saved file against both sides, and rerun its 14 breaks, on the next pass through this file or when 0x427e80 arrives, whichever is first. #35 0x424dd0 is covered by `test_pm_collide`.

**Animation (2026-11-05, #36 0x420aa0):** `test_pm_mechanim.py`, moved from MW3's, on PM's 360 motions of 25 mechs: the diff was the compiler's inlining and calling conventions, and the port needed no change.

**The build (2026-11-05, #37 0x414cb0, the launch 0x494500):** `test_pm_mech_build.py`, moved from MW3's: launch and AI mech down the whole constructor chain, every block byte for byte, on PM's 25 mechs and their nodes. The launch: no cockpit cycles at ObjectLOD 2, game type 5's own line-up, the `dbase` under `tur_ppc02` shown in types 2 and 3 on world 0, the list's +0x10 cleared. The build: 0x4148c0 takes 0x748 bytes, the weapon manager 0x98 with a new field at +0x1c, the torso 0x1bc with the headlight at +0x7c and a field at +0x80, `PlayerControls` 0x18c with its IControls at +0x16c and a timed list (0x4b2580) only made so far, the skis 0x31c with no debug quads, the low-detail node asked for first. 0x414cb0 looks up the engine's limits and drops the answer. The test also found `mid_width` unset: PM's maker writes it at 0x477d1a, half x 0.5 (+0x310, read at 0x477851, 0x4778e5, 0x47797e). MW3 has no such write (0x476320 leaves +0x2f4 alone), so this is PM's own fix: `Skis::mid`'s FIX is gone on `pm`, and `main` stays as it is.

Not covered: the weapons and equipment loop in 0x414cb0 (0x414dbe..0x414fc8) is one stubbed call, as in MW3's test; read by hand it has MW3's structure with PM's TAG id and firepoint name offset, and it is tested when `test_weapons_run` moves. What the manager's +0x1c and the torso's +0x80 are is not traced, and the timed list's readers are in PM's AI code. Stubs and offsets for a few calls were corrected against the exe where the address map mis-paired them (0x430d10 mapped to 0x4703c0).

**Literal ids (2026-11-05):** a search of the source by pattern, plus every small constant that differs between matched instructions of the two exes, kept where it is a renumbered mech or weapon id or a count (56 places, one by one). Wrong on `pm` and fixed, each checked against the exe's own bytes in `test_pm_names` where the code can be reached: TAG 0x20 to 0x2c (a mech's TAG was added as a heavy machine gun; salvage kept TAG and skipped heavy machine guns), the chase camera's height table 0x4312b0, the lab's missile rule 0x4559e0, a mech's shown name past roster 18 in the lab and Instant Action, the lab's and the debriefing card's "no mech" 0x13 to 0x19 (a Shadow Cat never appeared in the lab's view). Not reached by a test, read only: the two shown names (now `kMechCount`).

Open from it: MW3's LRM hold is gone from PM's weapon type 0x4b34d0 (no +0xc0 written, 0x4b4c90 unmatched): `lrm_flight` stays 0, so `hold_for_flight` adds no delay; settle it when PM's AI firing is ported. `tests/test_program.cpp` (26 places), `test_pilot_names.cpp` and `test_mech_reader.cpp` hold MW3's numbers (`l.mech = 17`, the Thor); they are in `mw3tests`, not built on `pm`, and change when moved. The lancemate's hit on the player at frame 129 of the c1m1 walk waits for PM's AI.

**Health and the limp (2026-11-05):** `test_pm_health`. PM's health 0x41c740 keeps MW3's weights but, each time it is worked out, sets how bad a limp is (+0x540, 0x418540) and a limping mech's top speed from it (0x418640). The speed setter 0x414c00 now holds speed to 5..60 and can leave the gait bounds alone. A limp's start 0x41e3b0 stops MASC, and MASC 0x41eec0 will not run on a bad leg. **The script path:** PM's destroy valve 0x4f5210 does not go through 0x41de30 as MW3's 0x4e34b0 did 0x41cfa0: it calls the entity's vt+0x78, for a mech the kill 0x41de00. 0x41de30 is called from heat alone. `WiredValveHooks` had no `mech_destroy`, so a destroy valve on a mech did nothing here but log; it is wired to `Mech::kill` now. The kill is tested through the exe's valve; the wiring itself and a mission using it are not. `origin/main` lacks that override too.

**The player's controls (2026-11-05, #47 0x4299f0):** `test_pm_player.py` and `test_pm_controls.py`, moved from MW3's. PM's update leaves the inputs alone in the animation states 12 and 13 as well as when powered down; with the torso absolute (+0xa8) the twist keys go to 1, not 1.5, and the twist eases at 1/s, not 6/s (0x604734); from rest the jets' first press puts the thrust straight at gravity / 38 (vt+0x4c, 0x4287d0), what holds the mech up; the look's return multiplies in another order. The jet keys' gate is the game's vt+0x44: the option in single player, Instant Action and standalone (0x435070), a multiplayer game's own +0xa7 (0x4365f0), which the port does not have (multiplayer's item, the option stands in). The smoothing's zeroing goes through vt+0x18 (0x428910, the player's also clears the look). The tests read the game's vt+0x44 through the real 0x435070 and the torso flag as scenario data; a gravity other than 30 is not tried.

**The lance orders wait for PM's AI (2026-11-05, #48 0x46a750, #50 0x46b1b0, #96 0x46b4d0, #132 0x46af20, #141 0x46a9c0, #152 0x46a8e0):** 0x46a750 is new: for an order on the object at [0x629a2c] it sets the order's handler to 0x4fb790 and marks an AI object at +0x2cc of the lancemate's controls (bit 30 in its masks +0xc0..+0xc8, active), and clears that for any other target. That object is made and written by the AI (0x4dd34c, 0x4e60b8), and `test_lance_dispatch` is already "to move with PM AI". Read, not ported: `lance_orders.cpp` is MW3's until then, so PM's orders do not do this.

**The hit path (2026-11-05, #49 0x461bb0, #64 0x461d70, with 0x4617a0): no change from MW3.** Read against MW3's 0x460780, 0x460970 and 0x460350, and PM's constants against MW3's: the section table, the fallback to the centre, 0.0062, 0.15, the five rates and 10 are the same; what differs is arguments on the stack and field offsets. +0x1a8 and +0x1ac are the torso's side internals (MW3's +0x1a0, +0x1a4), set by the build and the damage, not by the AI. `test_pm_damage` runs these three but its random hits missed one edge test and the 0.008 boundary, so `test_pm_hit.py` aims shots across each edge from both sides and forces the roll on each side of each rate. One break in 20 is not caught, the exe's `s1 >= 0` test in `crosses`: `s1 < 0` passing the guard above means `s2 > 0`, and the next line then answers the same. Forced on both sides, marked in the test: armor, rolls, the structure values.

**Collision's rates are suspect on `pm` (2026-11-05):** a2105b11 scales the wall's kick and throttle, the bump's shove and throttle, the crash damage, the ceiling's and a rammed vehicle's amounts by `frame_steps`, assuming a contact on every frame, and `test_pm_collide_rate.cpp` pins that same assumption. A wall hit is often one frame (the mech moves out and bounces): on `main` the same scaling made a single bounce weaker as the frame rate rose, changed a route on c1m4 and was reverted. Not redone yet: dev 2 is measuring real contact frames per second at 8, 16 and 33 ms and gives a verdict per amount, and `pm` then follows it, checked against PM's own values. Until then `per_second` on `pm` keeps the scaling (the remote shove alone is already out, ed3e54f9); `bugs` and the exe's path are not affected, as the tests run per_second off.

**Waiting for a break in this table (2026-11-05):** a player can't keep a PM campaign without the pilot file's format, the starting store 0x4351b0, `lance_keeps`' twin, restore of a saved world, and type 5 unrun (`pm-map.md`, step 6). They take the first natural break here.

## The order

The lead's rule (2026-11-05): what the frame runs for every mech ranks above what it runs for the player only, unless the player-only one changes how the game feels. The score below cannot see that, so it is applied by reading each function as the order reaches it.

The loadouts first: they decide what exists. Then by a score: size, how unlike MW3, nearness to the frame, callers. It ranks
the work; it does not measure play, so jets, landing and 0x423590 are near the top by size and depth, not because of a play test.

| # | PM | MW3 | Class | Instr MW3 | Instr PM | Alike | Depth | Callers | Port | Tested | Note |
|---:|---|---|---|---:|---:|---:|---:|---:|---|---|---|
| 1 | 0x425c60 | - | new | 0 | 39 | 0.00 |  | 1 | no |  | an unplaced item |
| 2 | 0x427020 | 0x4251d0 | changed | 108 | 97 | 0.90 |  | 1 | no |  | weight of a category |
| 3 | 0x427130 | 0x425300 | changed | 137 | 140 | 0.62 |  | 21 | yes |  | weight: case 8 is the cockpit and shielding, none for the Elemental (ported), the port numbers 8 as max mass |
| 4 | 0x427310 | - | new | 0 | 35 | 0.00 |  | 1 | no |  | free slots not negative, not in the port |
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
| 17 | 0x423590 | - | new | 0 | 153 | 0.00 | 0 | 1 | no |  | new: quaternion and matrix step at +0x6dc..+0x724 |
| 18 | 0x416d70 | 0x4162c0 | changed | 274 | 311 | 0.63 | 0 | 3 | yes |  | landing: only the player falls, no set_throttle, hip duck |
| 19 | 0x4253f0 | - | new | 0 | 197 | 0.00 | 3 | 1 | no |  |  |
| 20 | 0x424030 | - | new | 0 | 160 | 0.00 | 2 | 2 | no |  |  |
| 21 | 0x462ba0 | - | new | 0 | 287 | 0.00 |  | 6 | no |  |  |
| 22 | 0x428a00 | - | new | 0 | 328 | 0.00 |  | 1 | no |  |  |
| 23 | 0x462fd0 | - | new | 0 | 297 | 0.00 |  | 2 | no |  |  |
| 24 | 0x424730 | 0x4231c0 | changed | 350 | 360 | 0.69 | 2 | 6 | yes |  | calls 2->0 |
| 25 | 0x4186f0 | - | new | 0 | 115 | 0.00 | 1 | 1 | no |  | new: lean into turns, +0x560, +0x568 |
| 26 | 0x41be50 | - | new | 0 | 255 | 0.00 |  | 3 | no |  |  |
| 27 | 0x462090 | 0x460d50 | changed | 435 | 483 | 0.46 |  | 2 | yes |  | calls 6->1, consts 0->1 |
| 28 | 0x468da0 | - | new | 0 | 247 | 0.00 |  | 2 | no |  |  |
| 29 | 0x46ea40 | - | new | 0 | 211 | 0.00 |  | 1 | no |  |  |
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
| 48 | 0x46a750 | - | new | 0 | 116 | 0.00 |  | 4 | no |  |  |
| 49 | 0x461bb0 | 0x460780 | changed | 192 | 165 | 0.32 |  | 1 | yes |  |  |
| 50 | 0x46b1b0 | 0x469650 | changed | 221 | 228 | 0.45 |  | 2 | no |  | calls 0->2 |
| 51 | 0x467a90 | - | new | 0 | 51 | 0.00 | 2 | 5 | no |  |  |
| 52 | 0x477b50 | 0x476090 | changed | 187 | 89 | 0.35 |  | 2 | yes |  | calls 6->0, consts 2->0 |
| 53 | 0x425370 | - | new | 0 | 59 | 0.00 | 3 | 3 | no |  |  |
| 54 | 0x479780 | - | new | 0 | 119 | 0.00 |  | 1 | no | yes |  |
| 55 | 0x46f150 | 0x46d100 | changed | 180 | 231 | 0.53 |  | 3 | no |  | calls 1->6 |
| 56 | 0x415e90 | - | new | 0 | 50 | 0.00 | 2 | 3 | no |  |  |
| 57 | 0x4681b0 | 0x466840 | changed | 271 | 344 | 0.66 |  | 1 | yes |  |  |
| 58 | 0x41f7a0 | - | new | 0 | 111 | 0.00 |  | 2 | no | yes |  |
| 59 | 0x41c740 | - | new | 0 | 122 | 0.00 |  | 0 | no |  |  |
| 60 | 0x419c40 | - | new | 0 | 94 | 0.00 |  | 5 | no |  |  |
| 61 | 0x477210 | 0x475520 | changed | 248 | 158 | 0.66 |  | 5 | no |  |  |
| 62 | 0x41dab0 | 0x41cc30 | changed | 71 | 67 | 0.54 | 2 | 15 | yes | yes |  |
| 63 | 0x414430 | 0x413e60 | changed | 99 | 241 | 0.59 |  | 1 | yes | yes | calls 0->9, consts 0->4 |
| 64 | 0x461d70 | 0x460970 | changed | 226 | 235 | 0.64 |  | 4 | yes |  |  |
| 65 | 0x477ef0 | - | new | 0 | 32 | 0.00 | 2 | 9 | no |  |  |
| 66 | 0x470b80 | 0x46eba0 | changed | 139 | 117 | 0.36 |  | 2 | no |  | calls 2->3, imports 0->3 |
| 67 | 0x418540 | - | new | 0 | 88 | 0.00 |  | 1 | no |  | new: speed factor from the worst leg, limp |
| 68 | 0x472da0 | - | new | 0 | 84 | 0.00 |  | 2 | no |  |  |
| 69 | 0x4777f0 | 0x475d10 | changed | 75 | 60 | 0.46 | 2 | 1 | no |  |  |
| 70 | 0x477470 | 0x475870 | changed | 215 | 181 | 0.64 |  | 3 | no |  | calls 0->2 |
| 71 | 0x4149b0 | 0x4141a0 | changed | 91 | 93 | 0.09 |  | 1 | no |  |  |
| 72 | 0x4795c0 | - | new | 0 | 82 | 0.00 |  | 1 | yes | yes |  |
| 73 | 0x476750 | 0x4749e0 | changed | 152 | 141 | 0.55 |  | 5 | no |  | calls 0->1 |
| 74 | 0x4731b0 | 0x470d20 | changed | 107 | 47 | 0.48 |  | 25 | no |  | calls 2->1 |
| 75 | 0x463380 | 0x461d80 | changed | 215 | 218 | 0.66 |  | 2 | yes |  |  |
| 76 | 0x41c9f0 | 0x41bda0 | changed | 61 | 59 | 0.46 | 2 | 3 | yes | yes |  |
| 77 | 0x4291a0 | 0x4274d0 | changed | 279 | 278 | 0.72 |  | 1 | yes |  |  |
| 78 | 0x464450 | 0x462de0 | changed | 131 | 141 | 0.75 | 2 | 1 | yes |  |  |
| 79 | 0x4703c0 | 0x430d10 | changed | 93 | 86 | 0.23 |  | 2 | yes |  | consts 3->0 |
| 80 | 0x415f10 | - | new | 0 | 71 | 0.00 |  | 2 | no |  |  |
| 81 | 0x41c900 | - | new | 0 | 78 | 0.00 |  | 0 | no |  |  |
| 82 | 0x479190 | 0x477820 | changed | 98 | 98 | 0.65 | 2 | 1 | yes |  |  |
| 83 | 0x479b50 | - | new | 0 | 72 | 0.00 |  | 1 | yes | yes |  |
| 84 | 0x4626d0 | 0x461300 | changed | 164 | 155 | 0.54 |  | 0 | yes |  |  |
| 85 | 0x472850 | - | new | 0 | 68 | 0.00 |  | 2 | no |  |  |
| 86 | 0x419de0 | 0x418fc0 | changed | 144 | 143 | 0.80 | 1 | 1 | yes |  |  |
| 87 | 0x479440 | - | new | 0 | 70 | 0.00 |  | 1 | yes | yes |  |
| 88 | 0x464780 | 0x463110 | changed | 361 | 396 | 0.81 |  | 0 | yes |  |  |
| 89 | 0x476ab0 | 0x474d90 | changed | 118 | 104 | 0.41 |  | 1 | no |  |  |
| 90 | 0x462f50 | - | new | 0 | 56 | 0.00 |  | 6 | no |  |  |
| 91 | 0x464610 | 0x462fa0 | changed | 126 | 129 | 0.47 |  | 1 | yes |  |  |
| 92 | 0x4274d0 | 0x425650 | changed | 291 | 282 | 0.78 |  | 2 | yes |  |  |
| 93 | 0x475fe0 | 0x474210 | changed | 198 | 198 | 0.69 |  | 2 | no |  |  |
| 94 | 0x475b20 | - | new | 0 | 57 | 0.00 |  | 3 | no |  |  |
| 95 | 0x426600 | - | new | 0 | 52 | 0.00 |  | 5 | no |  |  |
| 96 | 0x46b4d0 | 0x469940 | changed | 257 | 268 | 0.77 |  | 1 | no |  | calls 0->3 |
| 97 | 0x429560 | 0x427870 | changed | 366 | 294 | 0.83 |  | 1 | yes |  | calls 4->0 |
| 98 | 0x41c3f0 | 0x41b660 | changed | 83 | 65 | 0.76 | 1 | 5 | yes |  |  |
| 99 | 0x424d80 | - | new | 0 | 36 | 0.00 | 4 | 1 | no |  |  |
| 100 | 0x418640 | - | new | 0 | 54 | 0.00 |  | 3 | no |  |  |
| 101 | 0x4262f0 | 0x4247a0 | changed | 262 | 285 | 0.82 |  | 4 | yes |  |  |
| 102 | 0x414ba0 | - | new | 0 | 45 | 0.00 |  | 7 | no |  |  |
| 103 | 0x4790c0 | - | new | 0 | 23 | 0.00 | 2 | 4 | no |  |  |
| 104 | 0x4721e0 | 0x470170 | changed | 144 | 180 | 0.70 |  | 2 | no |  | calls 0->6 |
| 105 | 0x4611f0 | 0x45fe00 | changed | 119 | 112 | 0.53 |  | 1 | yes |  | calls 4->0 |
| 106 | 0x41ade0 | - | new | 0 | 16 | 0.00 | 1 | 9 | no | yes |  |
| 107 | 0x471040 | 0x46ef70 | changed | 129 | 116 | 0.55 |  | 0 | no |  | calls 0->1, consts 3->2 |
| 108 | 0x474f80 | 0x472f00 | changed | 173 | 139 | 0.70 |  | 2 | yes |  |  |
| 109 | 0x4792b0 | 0x477940 | changed | 59 | 48 | 0.57 | 2 | 1 | yes |  |  |
| 110 | 0x41f460 | - | new | 0 | 50 | 0.00 |  | 3 | no | yes |  |
| 111 | 0x41e8b0 | 0x41d940 | changed | 216 | 213 | 0.76 |  | 2 | yes |  |  |
| 112 | 0x4243f0 | 0x422ef0 | changed | 94 | 101 | 0.72 | 3 | 1 | yes |  |  |
| 113 | 0x465a60 | 0x464290 | changed | 82 | 105 | 0.48 |  | 0 | yes |  |  |
| 114 | 0x462020 | 0x460c10 | changed | 79 | 50 | 0.38 |  | 2 | no |  |  |
| 115 | 0x41de30 | - | new | 0 | 24 | 0.00 | 2 | 1 | no |  |  |
| 116 | 0x4799b0 | - | new | 0 | 27 | 0.00 | 4 | 4 | yes | yes |  |
| 117 | 0x4267f0 | 0x424b70 | changed | 258 | 247 | 0.81 |  | 1 | yes | yes | strings 0->1 |
| 118 | 0x4764d0 | 0x474740 | changed | 149 | 125 | 0.69 |  | 2 | no |  |  |
| 119 | 0x41f740 | - | new | 0 | 19 | 0.00 | 2 | 5 | no |  |  |
| 120 | 0x4776b0 | 0x475b00 | changed | 108 | 78 | 0.57 |  | 2 | no |  |  |
| 121 | 0x41c4c0 | - | new | 0 | 44 | 0.00 |  | 3 | no |  |  |
| 122 | 0x477dd0 | - | new | 0 | 48 | 0.00 |  | 1 | no |  |  |
| 123 | 0x479510 | - | new | 0 | 48 | 0.00 |  | 1 | yes | yes |  |
| 124 | 0x424520 | 0x423010 | changed | 123 | 148 | 0.81 | 4 | 1 | yes |  | imports 0->1 |
| 125 | 0x4793c0 | - | new | 0 | 47 | 0.00 |  | 1 | no |  |  |
| 126 | 0x4798d0 | - | new | 0 | 44 | 0.00 |  | 2 | no |  |  |
| 127 | 0x416c70 | 0x4161c0 | changed | 66 | 72 | 0.45 |  | 4 | yes |  |  |
| 128 | 0x41fb70 | 0x41ef00 | changed | 26 | 30 | 0.50 | 2 | 9 | yes |  | calls 2->0 |
| 129 | 0x4796f0 | - | new | 0 | 44 | 0.00 |  | 1 | yes |  |  |
| 130 | 0x41e110 | - | new | 0 | 44 | 0.00 |  | 0 | no |  |  |
| 131 | 0x463bd0 | 0x462590 | changed | 191 | 194 | 0.79 |  | 1 | yes |  |  |
| 132 | 0x46af20 | 0x469400 | changed | 177 | 189 | 0.78 |  | 1 | no |  | calls 0->4 |
| 133 | 0x4790f0 | 0x477620 | changed | 63 | 58 | 0.70 | 2 | 1 | yes |  | calls 0->1 |
| 134 | 0x41eec0 | 0x41df50 | changed | 76 | 91 | 0.63 |  | 5 | yes | yes | calls 0->1 |
| 135 | 0x4615b0 | - | new | 0 | 22 | 0.00 | 4 | 3 | no |  |  |
| 136 | 0x472420 | 0x470320 | changed | 24 | 26 | 0.38 | 2 | 4 | no |  |  |
| 137 | 0x4778a0 | 0x475de0 | changed | 65 | 65 | 0.73 | 2 | 2 | yes |  |  |
| 138 | 0x472940 | 0x4707c0 | changed | 105 | 98 | 0.65 |  | 2 | no |  |  |
| 139 | 0x472e60 | - | new | 0 | 35 | 0.00 |  | 3 | no |  |  |
| 140 | 0x479950 | - | new | 0 | 40 | 0.00 |  | 0 | yes | yes |  |
| 141 | 0x46a9c0 | 0x468e20 | changed | 84 | 66 | 0.55 |  | 1 | no |  |  |
| 142 | 0x4628c0 | 0x461510 | changed | 98 | 90 | 0.60 |  | 0 | yes |  | calls 4->0, consts 1->0 |
| 143 | 0x472cb0 | - | new | 0 | 37 | 0.00 |  | 1 | no |  |  |
| 144 | 0x465da0 | - | new | 0 | 18 | 0.00 | 3 | 3 | no | yes |  |
| 145 | 0x4638f0 | 0x4622a0 | changed | 76 | 75 | 0.82 | 1 | 3 | yes | yes |  |
| 146 | 0x4255f0 | 0x423c90 | changed | 30 | 34 | 0.50 | 2 | 1 | yes |  | consts 1->0 |
| 147 | 0x415940 | 0x414f40 | changed | 292 | 295 | 0.88 |  | 1 | no |  | calls 1->4 |
| 148 | 0x4210d0 | 0x4200c0 | changed | 63 | 65 | 0.47 |  | 1 | yes |  | calls 4->0 |
| 149 | 0x427a30 | 0x425bd0 | changed | 114 | 105 | 0.79 |  | 18 | yes |  |  |
| 150 | 0x474010 | 0x471de0 | changed | 183 | 152 | 0.80 |  | 0 | yes |  |  |
| 151 | 0x4617a0 | 0x460350 | changed | 284 | 272 | 0.88 |  | 1 | yes |  | consts 0->1 |
| 152 | 0x46a8e0 | 0x468c00 | changed | 62 | 66 | 0.50 |  | 1 | no |  | calls 0->2 |
| 153 | 0x41f8d0 | - | new | 0 | 14 | 0.00 | 2 | 3 | no | yes |  |
| 154 | 0x476a20 | 0x474cd0 | changed | 81 | 62 | 0.60 |  | 1 | no |  |  |
| 155 | 0x415190 | 0x4148c0 | changed | 102 | 133 | 0.76 |  | 1 | yes |  | calls 1->2, consts 0->1 |
| 156 | 0x466380 | 0x464af0 | changed | 139 | 152 | 0.78 |  | 0 | yes |  |  |
| 157 | 0x467d10 | 0x4663e0 | changed | 87 | 118 | 0.72 |  | 0 | yes |  | calls 0->2 |
| 158 | 0x4211b0 | 0x4201b0 | changed | 50 | 54 | 0.42 |  | 1 | yes |  | calls 7->0 |
| 159 | 0x41c570 | 0x41b840 | changed | 54 | 34 | 0.79 | 1 | 2 | yes | yes |  |
| 160 | 0x475be0 | 0x473e50 | changed | 86 | 106 | 0.79 |  | 7 | no |  |  |
| 161 | 0x460c20 | 0x45f980 | changed | 46 | 64 | 0.55 |  | 1 | yes |  |  |
| 162 | 0x465150 | 0x463aa0 | changed | 49 | 62 | 0.51 |  | 0 | yes |  |  |
| 163 | 0x467200 | 0x465940 | changed | 31 | 41 | 0.33 |  | 2 | yes |  | consts 0->1 |
| 164 | 0x41b930 | - | new | 0 | 30 | 0.00 |  | 0 | no |  |  |
| 165 | 0x479c60 | - | new | 0 | 27 | 0.00 |  | 2 | yes | yes |  |
| 166 | 0x4799f0 | - | new | 0 | 28 | 0.00 |  | 1 | yes | yes |  |
| 167 | 0x476260 | 0x4744a0 | changed | 43 | 34 | 0.37 |  | 1 | no |  |  |
| 168 | 0x4266d0 | 0x424a90 | changed | 82 | 102 | 0.73 |  | 1 | yes |  |  |
| 169 | 0x41e3b0 | 0x41d5f0 | changed | 72 | 78 | 0.68 |  | 3 | yes |  | calls 0->2 |
| 170 | 0x419d50 | - | new | 0 | 28 | 0.00 |  | 0 | no |  |  |
| 171 | 0x426af0 | 0x424e90 | changed | 71 | 63 | 0.70 |  | 6 | yes |  |  |
| 172 | 0x468660 | 0x466c50 | changed | 119 | 84 | 0.77 |  | 0 | yes |  |  |
| 173 | 0x460f60 | 0x45fbe0 | changed | 97 | 105 | 0.82 |  | 8 | yes |  |  |
| 174 | 0x4738b0 | - | new | 0 | 22 | 0.00 |  | 4 | no |  |  |
| 175 | 0x427430 | 0x4255b0 | changed | 49 | 61 | 0.65 |  | 4 | yes |  |  |
| 176 | 0x468050 | 0x4666f0 | changed | 117 | 128 | 0.81 |  | 1 | yes |  |  |
| 177 | 0x473240 | 0x470bc0 | changed | 40 | 45 | 0.63 |  | 21 | no |  |  |
| 178 | 0x4285d0 | 0x426960 | changed | 131 | 128 | 0.84 |  | 4 | yes |  | calls 1->0 |
| 179 | 0x465bd0 | 0x4643d0 | changed | 71 | 83 | 0.70 |  | 0 | yes |  |  |
| 180 | 0x479c30 | - | new | 0 | 22 | 0.00 |  | 2 | yes | yes |  |
| 181 | 0x467770 | 0x465ea0 | changed | 42 | 31 | 0.67 | 5 | 3 | yes |  |  |
| 182 | 0x468ff0 | - | new | 0 | 21 | 0.00 |  | 3 | no |  |  |
| 183 | 0x41df80 | - | new | 0 | 24 | 0.00 |  | 0 | no |  |  |
| 184 | 0x465ea0 | 0x464600 | changed | 76 | 79 | 0.70 |  | 0 | yes |  |  |
| 185 | 0x4768e0 | 0x474b70 | changed | 42 | 36 | 0.47 |  | 1 | no |  |  |
| 186 | 0x4741a0 | 0x471fb0 | changed | 201 | 194 | 0.88 |  | 0 | yes |  |  |
| 187 | 0x41dcd0 | 0x41ce60 | changed | 42 | 40 | 0.79 | 1 | 1 | yes |  |  |
| 188 | 0x417270 | - | new | 0 | 21 | 0.00 |  | 2 | no |  |  |
| 189 | 0x414c00 | 0x4143c0 | changed | 38 | 56 | 0.68 |  | 6 | yes |  |  |
| 190 | 0x477760 | 0x475c80 | changed | 38 | 38 | 0.69 | 3 | 1 | yes |  |  |
| 191 | 0x414370 | 0x413af0 | changed | 50 | 46 | 0.81 | 2 | 2 | no |  |  |
| 192 | 0x428f10 | 0x427280 | changed | 36 | 41 | 0.48 |  | 1 | yes |  |  |
| 193 | 0x416560 | 0x415b00 | changed | 101 | 103 | 0.79 |  | 1 | yes |  |  |
| 194 | 0x476200 | 0x474430 | changed | 48 | 38 | 0.56 |  | 1 | no |  |  |
| 195 | 0x476940 | 0x474bd0 | changed | 110 | 93 | 0.83 |  | 3 | no |  |  |
| 196 | 0x427d50 | 0x425f30 | changed | 61 | 45 | 0.71 |  | 5 | yes |  |  |
| 197 | 0x423330 | 0x422360 | changed | 138 | 133 | 0.85 |  | 1 | yes |  | consts 0->1 |
| 198 | 0x426ee0 | 0x425090 | changed | 55 | 56 | 0.67 |  | 3 | yes |  |  |
| 199 | 0x41b900 | - | new | 0 | 21 | 0.00 |  | 0 | no |  |  |
| 200 | 0x465e00 | - | new | 0 | 19 | 0.00 |  | 2 | no | yes |  |
| 201 | 0x4665a0 | 0x464ce0 | changed | 148 | 130 | 0.88 |  | 2 | yes |  |  |
| 202 | 0x415160 | - | new | 0 | 19 | 0.00 |  | 1 | no |  |  |
| 203 | 0x46b4b0 | - | new | 0 | 19 | 0.00 |  | 1 | no | yes |  |
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
| 214 | 0x4158f0 | - | new | 0 | 18 | 0.00 |  | 0 | no |  |  |
| 215 | 0x460f00 | 0x45fb90 | changed | 13 | 16 | 0.50 | 2 | 1 | yes |  | consts 0->2 |
| 216 | 0x475390 | 0x473480 | changed | 63 | 56 | 0.72 |  | 0 | yes |  |  |
| 217 | 0x465210 | 0x463b40 | changed | 115 | 111 | 0.85 |  | 0 | yes |  | consts 3->2 |
| 218 | 0x4287f0 | 0x426b60 | changed | 37 | 41 | 0.59 |  | 0 | yes |  |  |
| 219 | 0x41f900 | - | new | 0 | 16 | 0.00 |  | 1 | no | yes |  |
| 220 | 0x41c380 | 0x41b5f0 | changed | 25 | 24 | 0.56 |  | 10 | yes |  |  |
| 221 | 0x463650 | 0x462040 | changed | 28 | 38 | 0.57 |  | 0 | yes |  |  |
| 222 | 0x41ee80 | - | new | 0 | 14 | 0.00 |  | 3 | no |  |  |
| 223 | 0x477c80 | 0x476320 | changed | 97 | 103 | 0.85 |  | 1 | yes |  | calls 2->0 |
| 224 | 0x426bd0 | 0x424f60 | changed | 78 | 82 | 0.84 |  | 4 | yes |  | strings 0->1 |
| 225 | 0x41fc00 | 0x41ef90 | changed | 32 | 33 | 0.58 |  | 3 | yes |  | calls 4->0 |
| 226 | 0x473930 | 0x471540 | changed | 171 | 149 | 0.92 |  | 2 | yes |  | calls 1->0 |
| 227 | 0x416710 | 0x415ca0 | changed | 107 | 113 | 0.87 |  | 1 | yes |  |  |
| 228 | 0x461120 | 0x45fd40 | changed | 45 | 54 | 0.78 |  | 5 | yes |  |  |
| 229 | 0x468850 | 0x466e40 | changed | 23 | 17 | 0.50 |  | 6 | yes |  |  |
| 230 | 0x472d10 | 0x470b40 | changed | 27 | 29 | 0.51 |  | 1 | no |  | consts 1->0 |
| 231 | 0x460e50 | 0x45faf0 | changed | 31 | 34 | 0.58 |  | 1 | yes |  | calls 1->0 |
| 232 | 0x479940 | - | new | 0 | 13 | 0.00 |  | 2 | no |  |  |
| 233 | 0x41f0c0 | 0x41e5a0 | changed | 36 | 37 | 0.63 |  | 1 | no |  |  |
| 234 | 0x468a40 | 0x466fe0 | changed | 71 | 74 | 0.81 |  | 0 | yes |  |  |
| 235 | 0x466550 | 0x464c90 | changed | 25 | 25 | 0.75 | 2 | 1 | yes |  |  |
| 236 | 0x462f30 | - | new | 0 | 13 | 0.00 |  | 1 | no |  |  |
| 237 | 0x4184f0 | 0x417a10 | changed | 27 | 16 | 0.55 |  | 2 | yes |  |  |
| 238 | 0x41f340 | 0x41e7a0 | changed | 17 | 22 | 0.82 | 1 | 7 | no | yes |  |
| 239 | 0x477960 | 0x475ea0 | changed | 45 | 44 | 0.87 | 2 | 1 | yes |  |  |
| 240 | 0x4743f0 | 0x472210 | changed | 93 | 96 | 0.86 |  | 0 | yes |  |  |
| 241 | 0x41ffa0 | 0x41f340 | changed | 153 | 155 | 0.92 |  | 2 | yes | yes | calls 1->0 |
| 242 | 0x41ec10 | 0x41dca0 | changed | 97 | 94 | 0.86 |  | 0 | yes |  |  |
| 243 | 0x41ed00 | 0x41dd90 | changed | 97 | 94 | 0.86 |  | 0 | no |  |  |
| 244 | 0x463330 | 0x461d30 | changed | 28 | 29 | 0.57 |  | 1 | no |  |  |
| 245 | 0x41e180 | 0x41d380 | changed | 33 | 31 | 0.62 |  | 0 | no |  |  |
| 246 | 0x460cd0 | 0x45fa00 | changed | 26 | 38 | 0.69 |  | 1 | no |  |  |
| 247 | 0x467b70 | 0x466240 | changed | 11 | 11 | 0.57 | 1 | 1 | yes |  |  |
| 248 | 0x467f80 | 0x466610 | changed | 63 | 58 | 0.81 |  | 0 | yes |  |  |
| 249 | 0x465cd0 | 0x4644b0 | changed | 63 | 64 | 0.81 |  | 0 | yes |  |  |
| 250 | 0x4727b0 | - | new | 0 | 12 | 0.00 |  | 0 | no |  |  |
| 251 | 0x4283f0 | 0x426780 | changed* | 163 | 158 | 0.93 | 1 | 9 | yes | yes |  |
| 252 | 0x41dbb0 | 0x41cd40 | changed | 29 | 28 | 0.59 |  | 0 | yes |  | calls 1->0 |
| 253 | 0x463f10 | 0x4628c0 | changed | 46 | 44 | 0.76 |  | 1 | yes |  | calls 1->0, imports 1->0 |
| 254 | 0x41e1f0 | 0x41d400 | changed | 46 | 35 | 0.75 |  | 0 | yes |  |  |
| 255 | 0x463770 | 0x462130 | changed | 61 | 67 | 0.83 |  | 0 | yes |  |  |
| 256 | 0x475460 | 0x473570 | changed | 55 | 48 | 0.81 |  | 2 | yes |  |  |
| 257 | 0x41f930 | - | new | 0 | 9 | 0.00 |  | 5 | no |  |  |
| 258 | 0x4158d0 | - | new | 0 | 11 | 0.00 |  | 0 | no |  |  |
| 259 | 0x4158e0 | - | new | 0 | 11 | 0.00 |  | 0 | no |  |  |
| 260 | 0x41c9d0 | - | new | 0 | 11 | 0.00 |  | 0 | no |  |  |
| 261 | 0x41e7e0 | 0x41d890 | changed | 56 | 63 | 0.83 |  | 0 | no | yes |  |
| 262 | 0x4262a0 | 0x424760 | changed | 17 | 17 | 0.57 |  | 13 | no |  | consts 3->2 |
| 263 | 0x41f310 | 0x41e770 | changed | 12 | 14 | 0.80 | 1 | 14 | no | yes |  |
| 264 | 0x463e90 | 0x462860 | changed | 14 | 24 | 0.57 |  | 0 | yes |  |  |
| 265 | 0x467290 | 0x4659c0 | changed | 17 | 21 | 0.53 |  | 0 | yes |  |  |
| 266 | 0x425d50 | 0x424350 | changed | 26 | 14 | 0.75 |  | 12 | yes |  |  |
| 267 | 0x461480 | 0x460050 | changed | 39 | 36 | 0.80 |  | 5 | yes |  |  |
| 268 | 0x465fc0 | 0x464720 | changed | 50 | 48 | 0.81 |  | 0 | yes |  |  |
| 269 | 0x41ad50 | 0x419e60 | changed | 39 | 38 | 0.79 |  | 2 | yes | yes |  |
| 270 | 0x465590 | 0x463ef0 | changed | 46 | 43 | 0.81 |  | 0 | yes |  |  |
| 271 | 0x419d90 | 0x418f70 | changed | 14 | 14 | 0.73 | 2 | 1 | yes |  |  |
| 272 | 0x41f770 | 0x41e9c0 | changed | 11 | 12 | 0.73 | 1 | 1 | yes |  |  |
| 273 | 0x41e070 | 0x41d280 | changed | 17 | 17 | 0.50 |  | 0 | no |  |  |
| 274 | 0x4773e0 | 0x4757f0 | changed | 52 | 53 | 0.85 |  | 1 | no |  |  |
| 275 | 0x4166d0 | 0x415c70 | changed | 21 | 24 | 0.69 |  | 2 | no |  |  |
| 276 | 0x4639f0 | 0x4623a0 | changed | 72 | 70 | 0.89 |  | 0 | yes |  | consts 1->0 |
| 277 | 0x428fd0 | 0x427320 | changed | 61 | 61 | 0.88 |  | 2 | yes |  |  |
| 278 | 0x41f000 | 0x41e4e0 | changed | 46 | 47 | 0.85 |  | 2 | no | yes |  |
| 279 | 0x463af0 | 0x4624b0 | changed | 61 | 62 | 0.87 |  | 0 | yes |  |  |
| 280 | 0x41ee40 | 0x41ded0 | changed | 19 | 17 | 0.58 |  | 0 | no |  |  |
| 281 | 0x426f60 | 0x425110 | changed | 65 | 63 | 0.88 |  | 1 | yes |  |  |
| 282 | 0x417e00 | 0x41eb90 | changed | 10 | 14 | 0.75 | 2 | 1 | no |  |  |
| 283 | 0x41f920 | - | new | 0 | 7 | 0.00 |  | 2 | no |  |  |
| 284 | 0x424220 | 0x422d20 | changed* | 156 | 156 | 0.94 | 2 | 2 | yes |  |  |
| 285 | 0x415910 | - | new | 0 | 7 | 0.00 |  | 0 | no |  |  |
| 286 | 0x41e280 | - | new | 0 | 6 | 0.00 |  | 3 | no |  |  |
| 287 | 0x462ae0 | 0x461730 | changed | 57 | 58 | 0.89 |  | 2 | yes |  |  |
| 288 | 0x414950 | 0x414150 | changed | 35 | 39 | 0.83 |  | 1 | yes |  |  |
| 289 | 0x415340 | 0x4149f0 | changed* | 275 | 284 | 0.95 |  | 3 | yes |  |  |
| 290 | 0x477f30 | 0x476550 | changed | 21 | 23 | 0.88 | 2 | 1 | yes |  |  |
| 291 | 0x41dc00 | 0x41cd90 | changed | 14 | 13 | 0.57 |  | 0 | no |  | calls 1->0 |
| 292 | 0x4662c0 | 0x464a20 | changed | 22 | 19 | 0.73 |  | 0 | yes |  |  |
| 293 | 0x41f390 | 0x41e7e0 | changed | 9 | 13 | 0.80 | 2 | 1 | no |  |  |
| 294 | 0x466210 | 0x464950 | changed | 36 | 36 | 0.84 |  | 0 | yes |  |  |
| 295 | 0x4614e0 | 0x4600b0 | changed | 19 | 20 | 0.75 |  | 3 | no |  |  |
| 296 | 0x4168b0 | 0x415e40 | changed* | 247 | 272 | 0.91 |  | 1 | yes |  |  |
| 297 | 0x463600 | 0x462000 | changed | 21 | 21 | 0.77 |  | 2 | no |  |  |
| 298 | 0x417290 | 0x416780 | changed* | 78 | 67 | 0.96 | 2 | 11 | yes |  |  |
| 299 | 0x4158b0 | - | new | 0 | 5 | 0.00 |  | 0 | no |  |  |
| 300 | 0x41de00 | 0x4092c0 | changed | 11 | 11 | 0.55 |  | 0 | no |  | calls 1->2 |
| 301 | 0x41fc70 | 0x41f000 | changed | 74 | 74 | 0.95 |  | 4 | yes | yes | calls 1->0 |
| 302 | 0x421260 | 0x420260 | changed | 28 | 28 | 0.83 |  | 0 | yes |  |  |
| 303 | 0x4675e0 | 0x465d10 | changed* | 151 | 150 | 0.95 | 6 | 2 | yes |  |  |
| 304 | 0x417e30 | 0x41eca0 | changed | 7 | 10 | 0.83 | 2 | 5 | yes |  |  |
| 305 | 0x4745a0 | 0x4723c0 | changed* | 209 | 202 | 0.99 |  | 1 | yes |  |  |
| 306 | 0x41c680 | 0x41b980 | changed | 19 | 17 | 0.77 |  | 0 | no |  |  |
| 307 | 0x421e00 | 0x420d00 | changed | 71 | 71 | 0.95 |  | 2 | yes |  | calls 1->0 |
| 308 | 0x41dd40 | 0x41cee0 | changed | 26 | 24 | 0.84 |  | 0 | no |  |  |
| 309 | 0x415ce0 | 0x4152d0 | changed* | 190 | 140 | 1.00 |  | 2 | yes |  |  |
| 310 | 0x467b30 | 0x466200 | changed | 11 | 11 | 0.86 | 1 | 1 | yes |  |  |
| 311 | 0x41a020 | 0x419200 | changed* | 76 | 67 | 0.96 | 1 | 1 | yes |  |  |
| 312 | 0x4654e0 | 0x463e40 | changed* | 64 | 64 | 1.00 | 1 | 4 | yes |  |  |
| 313 | 0x425770 | 0x423e00 | changed* | 179 | 168 | 0.91 |  | 1 | yes |  |  |
| 314 | 0x474cd0 | 0x472bc0 | changed | 22 | 22 | 0.83 |  | 0 | yes |  |  |
| 315 | 0x474d50 | 0x472c40 | changed | 22 | 22 | 0.83 |  | 0 | yes |  |  |
| 316 | 0x426b80 | 0x424f30 | changed | 13 | 20 | 0.83 |  | 1 | no |  |  |
| 317 | 0x421fc0 | 0x420ec0 | changed | 36 | 35 | 0.92 |  | 2 | yes |  | calls 1->0 |
| 318 | 0x466280 | 0x4649c0 | changed | 9 | 7 | 0.67 |  | 0 | yes |  |  |
| 319 | 0x4662a0 | 0x4649f0 | changed | 9 | 7 | 0.67 |  | 0 | yes |  |  |
| 320 | 0x41fbd0 | 0x41ef60 | changed | 12 | 12 | 0.75 |  | 0 | yes |  |  |
| 321 | 0x4672d0 | 0x465a00 | changed* | 63 | 63 | 1.00 | 3 | 5 | yes | yes |  |
| 322 | 0x41b9d0 | 0x41ac10 | changed | 16 | 13 | 0.83 |  | 2 | yes |  |  |
| 323 | 0x4616c0 | 0x460280 | changed* | 65 | 71 | 0.94 | 3 | 2 | yes |  |  |
| 324 | 0x419bb0 | 0x418e00 | changed* | 60 | 64 | 0.95 | 2 | 1 | yes |  |  |
| 325 | 0x4779e0 | 0x475f20 | changed* | 50 | 50 | 1.00 | 1 | 2 | no |  |  |
| 326 | 0x41c1a0 | 0x41b420 | changed* | 137 | 136 | 0.90 |  | 0 | yes |  |  |
| 327 | 0x46ac20 | 0x4690d0 | changed* | 128 | 129 | 0.92 |  | 1 | no |  |  |
| 328 | 0x41fde0 | 0x41f180 | changed* | 123 | 123 | 1.00 |  | 2 | yes |  |  |
| 329 | 0x463840 | 0x4621f0 | changed* | 52 | 51 | 0.96 | 2 | 3 | yes |  |  |
| 330 | 0x427ca0 | 0x425e60 | changed | 9 | 11 | 0.80 |  | 3 | yes |  |  |
| 331 | 0x473e70 | 0x471c10 | changed* | 90 | 88 | 1.00 |  | 8 | no |  |  |
| 332 | 0x41e2d0 | 0x41d510 | changed* | 39 | 39 | 1.00 | 2 | 32 | yes |  |  |
| 333 | 0x427cc0 | 0x425e80 | changed | 13 | 15 | 0.86 |  | 3 | yes |  |  |
| 334 | 0x417170 | 0x416660 | changed* | 54 | 47 | 0.92 | 2 | 1 | yes |  |  |
| 335 | 0x422260 | 0x421120 | changed | 12 | 11 | 0.80 |  | 0 | yes |  | calls 1->0 |
| 336 | 0x474e10 | 0x472da0 | changed* | 106 | 108 | 0.94 |  | 2 | yes |  |  |
| 337 | 0x465780 | 0x464000 | changed* | 107 | 109 | 0.98 |  | 1 | yes |  |  |
| 338 | 0x461650 | 0x460210 | changed* | 34 | 34 | 1.00 | 2 | 12 | yes |  |  |
| 339 | 0x41dca0 | 0x41ce30 | changed | 15 | 15 | 0.86 |  | 0 | no |  |  |
| 340 | 0x414b80 | 0x414370 | changed | 5 | 7 | 0.75 |  | 3 | no |  |  |
| 341 | 0x467490 | 0x465bc0 | changed* | 66 | 64 | 1.00 | 7 | 4 | yes |  |  |
| 342 | 0x461430 | 0x460000 | changed* | 30 | 29 | 1.00 | 2 | 102 | no |  |  |
| 343 | 0x475150 | 0x4730e0 | changed | 10 | 13 | 0.86 |  | 0 | yes |  |  |
| 344 | 0x475180 | 0x473110 | changed | 10 | 13 | 0.86 |  | 0 | yes |  |  |
| 345 | 0x475200 | 0x4731c0 | changed | 9 | 11 | 0.83 |  | 0 | yes |  |  |
| 346 | 0x464ca0 | 0x4635e0 | changed* | 82 | 82 | 0.99 |  | 2 | yes |  |  |
| 347 | 0x4673a0 | 0x465ad0 | changed* | 80 | 79 | 0.90 |  | 2 | yes |  |  |
| 348 | 0x41e010 | 0x41d220 | changed | 7 | 7 | 0.75 |  | 0 | no |  |  |
| 349 | 0x475d10 | 0x473f40 | changed* | 65 | 67 | 0.96 |  | 6 | no |  |  |
| 350 | 0x420e40 | 0x432a80 | changed* | 65 | 58 | 0.92 |  | 5 | no |  |  |
| 351 | 0x467e90 | 0x466510 | changed* | 81 | 76 | 0.96 |  | 0 | yes |  |  |
| 352 | 0x425670 | 0x423d00 | changed* | 66 | 67 | 0.97 |  | 4 | yes |  |  |
| 353 | 0x461510 | 0x4600e0 | changed* | 72 | 69 | 0.93 |  | 2 | no |  |  |
| 354 | 0x419dc0 | 0x418fa0 | changed | 6 | 6 | 0.75 |  | 1 | no |  |  |
| 355 | 0x4751b0 | 0x473140 | changed | 9 | 11 | 0.86 |  | 0 | yes |  |  |
| 356 | 0x4738e0 | 0x4714c0 | changed* | 52 | 33 | 1.00 |  | 20 | yes |  |  |
| 357 | 0x4234d0 | 0x422500 | changed* | 72 | 72 | 1.00 |  | 1 | yes |  |  |
| 358 | 0x422280 | 0x421140 | changed* | 67 | 73 | 0.91 |  | 0 | yes |  |  |
| 359 | 0x427cf0 | 0x425ea0 | changed* | 65 | 47 | 1.00 |  | 2 | yes |  |  |
| 360 | 0x467bb0 | 0x466280 | changed* | 65 | 65 | 0.98 |  | 2 | yes |  |  |
| 361 | 0x46ab20 | 0x468fa0 | changed* | 62 | 67 | 0.95 |  | 1 | no |  |  |
| 362 | 0x468910 | 0x466ee0 | changed | 12 | 12 | 0.89 |  | 1 | no |  |  |
| 363 | 0x468c10 | 0x4671a0 | changed | 12 | 12 | 0.89 |  | 1 | yes |  |  |
| 364 | 0x41d880 | 0x41ca30 | changed | 9 | 7 | 1.00 | 2 | 9 | no |  | consts 1->0 |
| 365 | 0x473b30 | 0x4717a0 | changed* | 63 | 44 | 0.94 |  | 2 | yes |  |  |
| 366 | 0x475aa0 | 0x473ca0 | changed* | 65 | 52 | 1.00 |  | 1 | no |  |  |
| 367 | 0x420fd0 | 0x41ffc0 | changed* | 64 | 64 | 1.00 |  | 1 | yes |  |  |
| 368 | 0x46aa70 | 0x468f00 | changed* | 59 | 63 | 0.94 |  | 1 | no |  |  |
| 369 | 0x416d60 | 0x41ec60 | changed | 3 | 3 | 0.57 |  | 0 | yes |  |  |
| 370 | 0x428200 | 0x4265a0 | changed* | 54 | 53 | 0.90 |  | 3 | yes |  |  |
| 371 | 0x41f940 | 0x409980 | changed* | 25 | 23 | 1.00 | 2 | 3 | no |  |  |
| 372 | 0x425ca0 | 0x4242a0 | changed* | 56 | 55 | 1.00 |  | 2 | yes |  |  |
| 373 | 0x41e760 | 0x41d810 | changed* | 42 | 39 | 1.00 |  | 9 | yes | yes |  |
| 374 | 0x4759b0 | 0x473bb0 | changed* | 57 | 57 | 0.99 |  | 1 | no |  |  |
| 375 | 0x477a70 | 0x475fb0 | changed* | 56 | 56 | 1.00 |  | 1 | yes |  |  |
| 376 | 0x474510 | 0x472330 | changed* | 53 | 53 | 1.00 |  | 0 | yes |  |  |
| 377 | 0x4615d0 | 0x460190 | changed* | 49 | 49 | 1.00 |  | 1 | yes |  |  |
| 378 | 0x420f40 | 0x41ff30 | changed* | 41 | 39 | 1.00 |  | 5 | yes |  |  |
| 379 | 0x474d90 | 0x472cf0 | changed* | 41 | 29 | 1.00 |  | 5 | no |  |  |
| 380 | 0x4148c0 | 0x4140c0 | changed* | 46 | 46 | 1.00 |  | 2 | yes |  |  |
| 381 | 0x425d80 | 0x4243c0 | changed* | 44 | 43 | 1.00 |  | 1 | no |  |  |
| 382 | 0x425de0 | 0x424420 | changed* | 44 | 43 | 1.00 |  | 1 | no |  |  |
| 383 | 0x4277f0 | 0x425960 | changed* | 43 | 27 | 0.93 |  | 1 | yes |  |  |
| 384 | 0x4143f0 | 0x413b70 | changed* | 20 | 20 | 1.00 | 2 | 1 | no |  |  |
| 385 | 0x468b70 | 0x467100 | changed* | 44 | 43 | 1.00 |  | 0 | yes |  |  |
| 386 | 0x41dd90 | 0x41cf30 | changed* | 37 | 35 | 0.95 |  | 2 | yes |  |  |
| 387 | 0x41c6d0 | 0x41b9d0 | changed* | 40 | 40 | 1.00 |  | 0 | yes |  |  |
| 388 | 0x4232a0 | 0x4222e0 | changed* | 33 | 35 | 0.94 |  | 1 | yes |  |  |
| 389 | 0x4636c0 | 0x4620a0 | changed* | 35 | 35 | 0.97 |  | 1 | yes |  |  |
| 390 | 0x41c5d0 | 0x41b8d0 | changed* | 34 | 34 | 0.95 |  | 1 | no |  |  |
| 391 | 0x41fa40 | 0x41edd0 | changed* | 33 | 33 | 1.00 |  | 1 | no |  |  |
| 392 | 0x4643f0 | 0x462d80 | changed* | 17 | 17 | 1.00 | 4 | 4 | yes |  |  |
| 393 | 0x421ef0 | 0x420df0 | changed* | 31 | 31 | 1.00 |  | 1 | no |  |  |
| 394 | 0x464e00 | 0x463740 | changed* | 31 | 31 | 1.00 |  | 1 | yes |  |  |
| 395 | 0x419fb0 | 0x419190 | changed* | 27 | 30 | 1.00 |  | 1 | yes | yes |  |
| 396 | 0x473170 | 0x470ce0 | changed* | 21 | 21 | 0.96 |  | 26 | no |  |  |
| 397 | 0x41e2b0 | 0x41d4f0 | changed* | 8 | 8 | 1.00 | 1 | 25 | yes |  |  |
| 398 | 0x41edf0 | 0x41de80 | changed* | 25 | 25 | 1.00 |  | 2 | no |  |  |
| 399 | 0x427bc0 | 0x425d80 | changed* | 19 | 19 | 1.00 |  | 8 | yes |  |  |
| 400 | 0x41f9e0 | 0x41ed70 | changed* | 25 | 25 | 1.00 |  | 1 | no |  |  |
| 401 | 0x467cb0 | 0x466380 | changed* | 25 | 25 | 1.00 |  | 1 | yes |  |  |
| 402 | 0x474f40 | 0x472ec0 | changed* | 25 | 25 | 1.00 |  | 1 | yes |  |  |
| 403 | 0x463ff0 | 0x4629b0 | changed* | 23 | 26 | 0.93 |  | 0 | yes |  |  |
| 404 | 0x475a60 | 0x473c60 | changed* | 24 | 23 | 1.00 |  | 1 | no |  |  |
| 405 | 0x417220 | 0x416720 | changed* | 11 | 11 | 1.00 | 2 | 1 | yes |  |  |
| 406 | 0x475ba0 | 0x473e10 | changed* | 19 | 22 | 0.92 |  | 2 | no |  |  |
| 407 | 0x41f980 | 0x41ed10 | changed* | 23 | 23 | 1.00 |  | 1 | no |  |  |
| 408 | 0x41fd60 | 0x41f100 | changed* | 23 | 23 | 1.00 |  | 1 | yes |  |  |
| 409 | 0x4632f0 | 0x461cf0 | changed* | 18 | 21 | 0.90 |  | 2 | no |  |  |
| 410 | 0x41eb40 | 0x41dbd0 | changed* | 23 | 23 | 1.00 |  | 0 | yes |  |  |
| 411 | 0x468750 | 0x466da0 | changed* | 21 | 21 | 0.95 |  | 1 | no |  |  |
| 412 | 0x428190 | 0x426440 | changed* | 20 | 18 | 1.00 |  | 2 | no |  |  |
| 413 | 0x4689e0 | 0x466f80 | changed* | 17 | 17 | 1.00 |  | 5 | yes |  |  |
| 414 | 0x41df20 | 0x41d130 | changed* | 20 | 20 | 1.00 |  | 1 | yes |  |  |
| 415 | 0x466300 | 0x464a70 | changed* | 21 | 21 | 1.00 |  | 0 | yes |  |  |
| 416 | 0x41c630 | 0x41b930 | changed* | 20 | 18 | 0.92 |  | 0 | no |  |  |
| 417 | 0x420f10 | 0x41fef0 | changed* | 18 | 18 | 1.00 |  | 1 | no |  |  |
| 418 | 0x427bf0 | 0x425db0 | changed* | 11 | 12 | 1.00 |  | 13 | no |  |  |
| 419 | 0x41e030 | 0x41d240 | changed* | 17 | 17 | 1.00 |  | 0 | no |  |  |
| 420 | 0x41e0b0 | 0x41d2c0 | changed* | 16 | 16 | 1.00 |  | 0 | no |  |  |
| 421 | 0x41ebb0 | 0x41dc40 | changed* | 15 | 15 | 1.00 |  | 0 | no |  |  |
| 422 | 0x41ebe0 | 0x41dc70 | changed* | 15 | 15 | 1.00 |  | 0 | yes |  |  |
| 423 | 0x466350 | 0x464ac0 | changed* | 12 | 12 | 1.00 |  | 2 | yes |  |  |
| 424 | 0x479ca0 | 0x4779f0 | changed* | 11 | 10 | 1.00 |  | 1 | no |  |  |
| 425 | 0x41f0a0 | 0x41e580 | changed* | 10 | 10 | 1.00 |  | 3 | no |  |  |
| 426 | 0x41dfc0 | 0x41d1d0 | changed* | 11 | 11 | 1.00 |  | 0 | yes |  |  |
| 427 | 0x41e290 | 0x41d4d0 | changed* | 8 | 8 | 1.00 |  | 6 | yes |  |  |
| 428 | 0x426680 | 0x426540 | changed* | 9 | 8 | 1.00 |  | 3 | no |  |  |
| 429 | 0x41da90 | 0x41cc10 | changed* | 10 | 10 | 1.00 |  | 0 | yes | yes |  |
| 430 | 0x41e0f0 | 0x41d300 | changed* | 10 | 10 | 1.00 |  | 0 | no |  |  |
| 431 | 0x41eb10 | 0x41dba0 | changed* | 10 | 10 | 1.00 |  | 0 | yes |  |  |
| 432 | 0x41e4c0 | 0x41d6e0 | changed* | 3 | 3 | 1.00 | 2 | 39 | no |  |  |
| 433 | 0x424710 | 0x4231a0 | changed* | 4 | 4 | 1.00 | 2 | 1 | no |  |  |
| 434 | 0x424720 | 0x4231b0 | changed* | 4 | 4 | 1.00 | 2 | 1 | no |  |  |
| 435 | 0x428880 | 0x426bf0 | changed* | 8 | 8 | 1.00 |  | 0 | no |  |  |
| 436 | 0x41f760 | 0x41e9b0 | changed* | 3 | 3 | 1.00 | 2 | 1 | no |  |  |
| 437 | 0x41dc80 | 0x41ce10 | changed* | 5 | 5 | 1.00 |  | 5 | no |  |  |
| 438 | 0x41df60 | 0x41d170 | changed* | 5 | 5 | 1.00 |  | 1 | yes |  |  |
| 439 | 0x41ad30 | 0x419e40 | changed* | 4 | 4 | 1.00 |  | 5 | no |  |  |
| 440 | 0x41e260 | 0x41d480 | changed* | 5 | 5 | 1.00 |  | 0 | no |  |  |
| 441 | 0x41f790 | 0x41e9e0 | changed* | 3 | 3 | 1.00 |  | 5 | no |  |  |
| 442 | 0x41dfb0 | 0x41d1c0 | changed* | 3 | 3 | 1.00 |  | 0 | no |  |  |
| 443 | 0x41dff0 | 0x41d200 | changed* | 3 | 3 | 1.00 |  | 0 | no |  |  |
| 444 | 0x41e000 | 0x41d210 | changed* | 3 | 3 | 1.00 |  | 0 | no |  |  |
| 445 | 0x425660 | 0x423cf0 | changed* | 2 | 2 | 1.00 |  | 5 | no |  |  |
| 446 | 0x467480 | 0x409e80 | changed* | 2 | 2 | 1.00 |  | 1 | no |  |  |
| 447 | 0x4158c0 | 0x40a830 | changed* | 2 | 2 | 1.00 |  | 0 | no |  |  |
| 448 | 0x4158a0 | - | new | 0 | 0 | 0.00 |  | 0 | no |  |  |
| 449 | 0x415920 | 0x414f20 | identical | 11 | 11 |  |  | 0 | yes |  |  |
| 450 | 0x415cd0 | - | new | 0 | 0 | 0.00 |  | 8 | no |  |  |
| 451 | 0x416540 | 0x415ae0 | identical | 6 | 6 |  |  | 1 | no |  |  |
| 452 | 0x417e50 | 0x41ecf0 | identical | 7 | 7 |  | 2 | 1 | no |  |  |
| 453 | 0x41c3d0 | 0x41b640 | identical | 11 | 11 |  |  | 2 | yes |  |  |
| 454 | 0x41c520 | 0x41b7f0 | identical | 29 | 29 |  | 1 | 1 | yes |  |  |
| 455 | 0x41eea0 | 0x41df30 | identical | 10 | 10 |  |  | 0 | no |  |  |
| 456 | 0x41f3d0 | - | new | 0 | 0 | 0.00 |  | 0 | no | yes |  |
| 457 | 0x41f3d5 | - | new | 0 | 0 | 0.00 |  | 0 | yes | yes |  |
| 458 | 0x41f450 | - | new | 0 | 0 | 0.00 |  | 0 | no |  |  |
| 459 | 0x41fac0 | 0x41ee50 | identical | 9 | 9 |  |  | 3 | no |  |  |
| 460 | 0x420530 | 0x41f800 | identical | 35 | 35 |  |  | 3 | no |  |  |
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
| 479 | 0x4266a0 | 0x426560 | identical | 16 | 16 |  |  | 3 | no |  |  |
| 480 | 0x427420 | 0x4255a0 | identical | 6 | 6 |  |  | 1 | no |  |  |
| 481 | 0x427c60 | 0x425e20 | identical | 7 | 7 |  |  | 6 | no |  |  |
| 482 | 0x427c80 | 0x425e40 | identical | 12 | 12 |  |  | 5 | yes |  |  |
| 483 | 0x4282c0 | 0x426650 | identical | 44 | 44 |  |  | 8 | yes | yes |  |
| 484 | 0x4288a0 | 0x426c10 | identical | 36 | 36 |  |  | 0 | yes |  |  |
| 485 | 0x428910 | 0x426c80 | identical | 14 | 14 |  |  | 1 | yes |  |  |
| 486 | 0x428fb0 | 0x427300 | identical | 8 | 8 |  |  | 1 | yes |  |  |
| 487 | 0x460bc0 | 0x45b800 | identical | 38 | 38 |  |  | 24 | no |  |  |
| 488 | 0x460d90 | 0x45fa70 | identical | 17 | 17 |  |  | 1 | yes |  |  |
| 489 | 0x460ef0 | 0x45fb80 | identical | 5 | 5 |  | 2 | 5 | yes |  |  |
| 490 | 0x460f40 | 0x45fbc0 | identical | 8 | 8 |  |  | 2 | yes |  |  |
| 491 | 0x4611c0 | 0x45fdd0 | identical | 20 | 20 |  |  | 6 | yes |  |  |
| 492 | 0x4629d0 | 0x461640 | identical | 13 | 13 |  |  | 0 | yes |  |  |
| 493 | 0x463ee0 | 0x462890 | identical | 16 | 16 |  |  | 0 | yes |  |  |
| 494 | 0x463fb0 | 0x462970 | identical | 2 | 2 |  | 3 | 4 | no |  |  |
| 495 | 0x464430 | 0x462dc0 | identical | 4 | 4 |  | 2 | 1 | yes |  |  |
| 496 | 0x464770 | 0x463100 | identical | 5 | 5 |  |  | 2 | yes |  |  |
| 497 | 0x4654d0 | 0x463e30 | identical | 5 | 5 |  |  | 2 | no |  |  |
| 498 | 0x465580 | 0x463ee0 | identical | 6 | 6 |  |  | 0 | yes |  |  |
| 499 | 0x467540 | 0x465c70 | identical | 63 | 63 |  |  | 2 | yes |  |  |
| 500 | 0x4677e0 | 0x465f20 | identical | 10 | 10 |  | 4 | 2 | yes |  |  |
| 501 | 0x468820 | 0x466e10 | identical | 19 | 19 |  |  | 0 | yes |  |  |
| 502 | 0x468890 | 0x466e80 | identical | 15 | 15 |  |  | 0 | yes |  |  |
| 503 | 0x4688e0 | 0x466eb0 | identical | 16 | 16 |  |  | 0 | yes |  |  |
| 504 | 0x468960 | 0x466f00 | identical | 16 | 16 |  |  | 0 | yes |  |  |
| 505 | 0x468d30 | 0x4672f0 | identical | 11 | 11 |  |  | 0 | yes |  |  |
| 506 | 0x468d80 | 0x4c4c60 | identical | 9 | 9 |  |  | 2 | no |  |  |
| 507 | 0x46a8b0 | 0x468bd0 | identical | 11 | 11 |  |  | 1 | no |  |  |
| 508 | 0x46a990 | 0x468df0 | identical | 13 | 13 |  |  | 1 | no |  |  |
| 509 | 0x471210 | - | new | 0 | 0 | 0.00 |  | 0 | no |  |  |
| 510 | 0x474b90 | 0x472a80 | identical | 70 | 70 |  |  | 2 | yes |  |  |
| 511 | 0x474c30 | 0x472b20 | identical | 49 | 49 |  |  | 4 | yes |  |  |
| 512 | 0x474ca0 | 0x472b90 | identical | 17 | 17 |  |  | 1 | yes |  |  |
| 513 | 0x474d10 | 0x472c00 | identical | 22 | 22 |  |  | 1 | yes |  |  |
| 514 | 0x475310 | 0x473400 | identical | 20 | 20 |  |  | 4 | no |  |  |
| 515 | 0x4755b0 | 0x473700 | identical | 18 | 18 |  |  | 0 | yes |  |  |
| 516 | 0x476bf0 | 0x474ef0 | identical | 9 | 9 |  |  | 1 | no |  |  |
| 517 | 0x477f70 | 0x476590 | identical | 7 | 7 |  | 4 | 1 | yes |  |  |
