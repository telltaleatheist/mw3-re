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
