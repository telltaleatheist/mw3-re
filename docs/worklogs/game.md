# Game systems log (mw3 mac 2)

Newest last. One short entry per item: what, commits, what to remember.

## 2026-10-20: mech placement 0x004161c0 (branch mac2/placement)

- Commits 68c7be9 (merge main), 27e10bd (the work); reported to mw3 mac 1.
- Ported the placement, SetPlace, the point query family (cls_di
  `ground_point`, gmod_hit point tests), the pick (veh_basic.cpp), the
  spawn 0x004782f0; docs in mission-start.md section 11.
- Remember: `test_place.py` (about 33 s) builds a mech image in the emulator
  with a copied vtable and stubs; `scratch/breaks.py` holds the 30 break
  tests (run at 100 queries). In one mw3tests process the mission world is
  kept from the first test and later engines have no camera: give the
  engine the kept camera (`set_mission_view`). A crashed mw3tests sits in
  state UE for a long time (crash report); look at the log, not the exit.
- Open: the pick's 2y + 10 step (fixes.md candidate), the player's power-up,
  zEntity's marker nodes 0x005a8390.

## 2026-10-20: the player's mech drives (branch mac2/drive)

- The power-up is Core's resume(7) (0x0041ca30(0), 0x0041cc30(0)), wired
  as GameEngine hooks; the frame: player tick -> vehicle_update ->
  Mech::update, the spawn's step too; `wire_mech_frame` in app_mech.cpp
  sets the pose/skis/collide/moved hooks; the soil table 0x0047f400 ported.
- mw3tests `mission_c1m1_player_walks_and_turns_headless` feeds a fake
  DirectInput keyboard. Tests start "as a new process" (missions() and
  mech types reset, main's pattern); the kept-world camera hack is gone.
- c1m1's player starts on the seabed under 4.86 m of water.
- Open: HUD, weapons (0x0041aba0, +0xd4), foot effects and sounds, mechs
  and vehicles as collision targets, the AI mechs' own frames.

## 2026-10-20: the shot queries (branch mac2/shot-queries)

- Ported cls_di.c's third query family: segment_list 0x4eed90 (cells
  0x4eefe0, nodes 0x4ef570 / 0x4ef980 / 0x4efaa0 / 0x4efba0, box 0x4f1470
  with 0x4ec6b0), segment_nearest 0x4eece0, sphere_query 0x4f0ac0
  (0x4f0f90, 0x4f0f00); gmod_hit model_segment 0x54cad0, polygon_segment
  0x54d760, polygon_segment_uv 0x54dd30, box_segment 0x54d2f0;
  zmth::invert_single (0x523c50 at 24 bits).
- test_shotq.py; scratch/x87.py traces an asm range's x87 sums (Ghidra's
  printed orders are wrong for these). Ground probes in a test must run on
  both sides (the point query moves cached polygons).
- Next: the fire wiring on mac2/fire (RunHooks::segment_list/segment,
  HitHooks::query to these).

## 2026-10-20: the player's weapons fire (branch mac2/fire)

- Commits f1a9e50 (invert comments, tools/analysis/x87.py), a59570b (the
  wiring and the c1m1 fire test). app_weapons.cpp wires the manager's,
  zWeapon's RunHooks/HitHooks, DamageCalls' node_part/node_owner, the Heat
  option, the frame step copies, Mech::aim_at 0x41bc20, 0x41aba0; the load
  needed InitHooks::pool_objects and the light hooks; zwep::shutdown at exit.
- The aim comes from the HUD's reticle (0x41cd40, not ported): the test
  aims for it. Group selection needs the mech's command handler (not
  ported): the test selects a hot group on the manager. The Mad Cat's first
  group is machine guns (heat 0).
- test_weapons_fly takes ~25 min (normal).
