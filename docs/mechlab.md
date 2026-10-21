# The mech lab

The mech lab is where the player picks each lance member's mech, chassis
skin and loadout from the campaign's store (or from everything, in the
other games). It is a state of its own, with a 2D dialog over a 3D view of
the mech. This doc maps it and gives a porting order. The briefing's
MECHLAB button opens it; ACCEPT goes back to the briefing.

Original file unknown: the code sits between mech_data.cpp and mech_map.cpp
(func2file), 0x00448830..0x004561ff, about 262 functions. The loadout
reader and writer it uses are in the mech-definition code (0x00424xxx, the
range between mech_anim.cpp and mech_data.cpp).

## 1. The way in and out

- **0x00448830** (ECX the pilot, EDX the store, a byte flag): stores them in
  [0x00605a7c], [0x00605a80] and [0x00605a84], then pushes the Lab State
  (0x00590400 on 0x006059f8) and turns the input over (0x0051b1c0). Callers:
  the briefing's MECHLAB (0x00404d3c: `mech_lab_open(store, 0)`, the store
  from 0x00408b40, `lab_store_build`), the in-mission debug keys
  (0x0040db60, the current game's store), and the multiplayer screen
  (0x0049b500).
- **The Lab State** (0x006059f8, "Lab State", vtable 0x005c0c90, made by
  0x00448930 at start-up through 0x004487f0 / atexit 0x00448810):
  - enter 0x00448960: without a pilot, false. When [0x00608e4b] is clear it
    makes four 3D objects for the scene (0x0056f520, 0x0056f6f0 into
    +0x70..+0x7c). The game's slot count (pilot +0x6c vt+0x3c) goes to
    +0x34, the slot shown (+0x30) to -1 and +0x38 to 0x13. Then the cursor
    (0x00516f50), FastMenu off around the 3D world's load (0x0044ffc0) and
    the screen's making (0x00448e70: new(0x1b468), 0x0044b920(slot count),
    then the first slot (0x00448f80(0)) and the first page (0x0044ce30(0))).
  - update 0x00448a80: input (0x0051b360), the cursor, the scene's node
    update (0x004f7f70), the 3D view's picking (0x00450810, 0x00450800,
    0x004507c0, 0x00448e20: a part clicked on the mech shows its page),
    the frame (0x00448f10, 0x00450690 with [0x0072c52c]) and the screen's
    flip (0x00576ea0). Developer keys while [0x00605a84] is clear: one
    writes the slot's loadout out (0x00426180(1)), another renames it at
    random (0x00426300, 0x00424e90) — both reachable only with the flag
    clear, which the briefing never leaves (it passes 0).
  - leave 0x00448a20: the scene's objects freed (0x0056f530), the 3D world
    unloaded (0x00450390), the screen deleted (0x00448ef0), the input given
    back, the pilot and store cleared, 0x005394c0.
- **ACCEPT** (the screen's ICON_ACCEPT, id 6, 0x0044ce30(6)): the shown
  slot's mech is taken back out of the store (0x0044e6c0, 0x00472a30), then
  the state's 0x00448870: each slot (game vt+0x34(i)) has its +0x0c cleared
  (0x00433ee0(0)), then the state pops (0x005905a0(4)). The loadouts were edited in place in
  the slots; the briefing then fits the lance to the store and splits the
  store back (0x00408c30, 0x00408bd0: campaign.md section 9).

## 2. The dialog

dlgMechLabII.zrd's LABMENU is the one the exe loads (0x0044b920, string
0x005e45cc). dlgMechLab.zrd (an older lab with fewer, different controls)
and dlgWeapon.zrd (a weapon-group dialog) ship but nothing reads them.

LABMENU: primitives TEMP_BACKGROUND, PANEL_CENTER, PANEL_LEFT, PANEL_RIGHT,
ICON_GLOW, PILOT_GLOW, MESSAGE_AREA; buttons:

| Control | Class (vtable) | Id |
|---|---|---|
| ICON_CHASSIS, ICON_WEAPONS, ICON_EQUIPMENT, ICON_CRITICALS, ICON_WEAPONGROUP, ICON_MISSIONDATA, ICON_ACCEPT | the pages' tabs | 0..6 |
| ICON_MECH1..4 | `MechButton` 0x0044a770 (0x005c0d70, ported) | 8..11, the lance's slots |
| CHASSIS_LIST | a store list (0x005c0cc0: refill 0x00449b30) | |
| SORT_CHASSISNAME, SORT_CHASSISMASS | buttons (0x005c1630) | 31, 32 |
| CONFIG_LIST | a store list (0x005c1588: refill 0x00449c40) | |
| CONFIG_RENAME, CONFIG_SAVE, CONFIG_DELETE | buttons (0x005c1630, two ShellButtons) | 13..16 |
| CONFIG_EDIT | an edit bar (0x005c14f0, like the pilot screen's) | 17 |
| SKIN_LIST | a cycle button (0x005c1448) | |
| PART_HEAD .. PART_RLEG | part buttons (0x005c13b0) | 18.. |
| SELECT_WEAPON / AMMO / EQUIP / ARMOR | buttons | 19..22 |
| UNASSIGNED_LIST | a list (0x005c1d68, ctor 0x004535d0) | |
| ASSIGNED_LIST | a list (0x005c1fe0, ctor 0x004552e0) | |
| WEAPONS_UNGROUPED, WEAPONS_GROUPED | lists (0x005c1300, 0x005c1250) | |
| CURRENT_CHASSIS | a panel (0x005c11c0) | |
| ENGINE_TABLE | the engine panel (0x005c1130, 0x0044f040) | |
| MECH_DATA, CHASSIS_DATA, ARMOR_TITLES, LOADOUT_LIST | texts and their panels (0x005c18b8, 0x005c1ac0) | |

The ids 0..32 are the press's (0x0044d4d0, the screen's +0x14): 0..6 a page
(0x0044ce30: 6 is ACCEPT, otherwise the old page's controls hidden and the
new one's shown, 0x0044ca50), 8..11 the slot (0x00448f80: the old slot's
mech back out of the store, the new one's put in, its controls refreshed),
14..32 the pages' own (save, delete, the engine faster or slower, the
sinks, the armor, the sorts; to be detailed with their items).

The screen (vtable 0x005c1110, new(0x1b468), destructor 0x0044c2f0) is a
MenuScreen like the briefing's; its page is +0x111ec.

## 3. What the rest of the game needs from it

- **The briefing**: MECHLAB opens it on the lab store (0x00408b40); after
  ACCEPT the Load state calls 0x00408bd0 and 0x00404ae0(1) (campaign.md
  section 9). Today `mech_lab_open` is a logged hook.
- **The debriefing**: the lance cards are the lab's `MechButton` with its
  icon 0x0044a9b0 (`mech_icon`), both ported (campaign.md section 3).
- **The salvage screen** (0x0047d3b0, dlgSalvage.zrd): the store lists; not
  the lab itself.

## 4. Already ported

- The stores (`Store`, pilot module): adds, removes, merges, the configs'
  items in and out (`has_config`, `take_config`, `give_config`), the
  reallocation (campaign.md section 9).
- The loadouts (`Config`, mech_config): the reader and copy, the recount,
  weights, `config_copy_store` (0x00424010 with a store), the saved list's
  loader 0x00424b70 and writer 0x00424f60 (`test_mechsave.py`), the slots'
  `set_config` with a store (0x00433df0).
- The store list (`StoreList`): rows, names, labels (0x0048dbe0).
- `MechButton` 0x0044a770 and its icon 0x0044a9b0.
- The mech definitions and their configs (mech_reader), the engine table,
  the names and techs (mech_names).

Not ported: everything in section 1 and 2 above; the second saved-loadout
loader 0x004260c0; the 3D lab world (labworld, labwindow, Blabcam:
0x0044ffc0, 0x00450390, 0x00450690, 0x004507c0..0x00450810).

## 5. Porting order

One sitting each; each with its differential test and break tests.

1. **The Lab State and the way in and out**: 0x00448830, the state
   (0x00448930, enter 0x00448960, leave 0x00448a20, update 0x00448a80's
   frame with the 3D world, the picking and the developer keys as hooks),
   the screen's making 0x00448e70 as a hook, ACCEPT's 0x00448870. In the
   program MECHLAB pushes the Lab State and a test's ACCEPT pops it back to
   the briefing.
2. **The screen's shell**: 0x0044b920 (the dialog bound, the panels, the
   tabs and the slot buttons), its destructor 0x0044c2f0, the page switch
   0x0044ce30 / 0x0044ca50 and the slot switch 0x00448f80 / 0x00449010 /
   0x0044ce90; ACCEPT through the press. The program shows the real lab.
3. **The chassis page**: CHASSIS_LIST (0x00449b30) and its sorts (31, 32),
   SKIN_LIST, choosing a chassis for the slot.
4. **The loadouts page**: CONFIG_LIST (0x00449c40), CONFIG_EDIT, rename,
   save (0x00424f60), delete (0x00425090), choosing a loadout for the slot
   (`set_config` with the store).
5. **The item pages**: SELECT_WEAPON / AMMO / EQUIP / ARMOR, the
   UNASSIGNED_LIST (0x005c1d68): adding and removing items between the
   store and the loadout.
6. **The criticals page**: ASSIGNED_LIST (0x005c1fe0) and the part
   buttons: placing items in parts' slots.
7. **The weapon groups page**: WEAPONS_UNGROUPED / WEAPONS_GROUPED.
8. **The engine, sinks and armor** (ENGINE_TABLE, ids 14..30's engine,
   sink and armor presses) and the data panels (MECH_DATA, CHASSIS_DATA,
   ARMOR_TITLES, LOADOUT_LIST).
9. **The 3D view**: the lab world and camera (0x0044ffc0, 0x00450390,
   0x00450690), CURRENT_CHASSIS's model, picking a part (0x004507c0..);
   with the render port's pieces it needs.
10. **MECHLAB from the briefing opens the real lab, and ACCEPT comes back
    with the loadout**: the program end to end (a mech and loadout changed
    in the lab reach the launch), mw3tests' campaign loop through the lab.
