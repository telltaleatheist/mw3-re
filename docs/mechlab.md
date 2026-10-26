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
    update (0x004f7f70), the 3D view's frame (0x00450810, 0x00450800). When
    +0x38 holds a mech to show (the slot switch 0x00449010 puts the slot's
    mech there; 0x13, the 19 mechs, means none) its entry is looked up in
    the +0x20 vector (0x004507c0: each entry's +0x08 is its mech; 0x00448e20
    checks the index), kept in +0x2c and handed to the page (0x0044cf40,
    which ignores it and redraws the page), then +0x38 goes back to 0x13.
    (Item 1 first read +0x38 as a node pointed at in the 3D view; the slot
    switch shows it is the mech.)
    The frame (0x00448f10: the screen's vt+0 under the 3D lock or the 2D
    one, by +0x6c), 0x00450690 with [0x0072c52c], and the flip (0x00576ea0).
    Its result is [0x00605a88], which nothing writes: the lab never leaves
    by its update, only by ACCEPT.
  - **Two hidden keys** while [0x00605a84] is clear (the briefing passes 0)
    and the shown slot has a loadout: **Ctrl+X** (0x22d) writes the loadout
    to a file through a Windows save dialog, "Mech3 Configurations"
    (0x00426180(1)); **Ctrl+I** (0x217) reads one through an open dialog
    (0x00426300), and when its mech's definition has a loadout list
    (0x004846a0, +0x20) adds it there: renamed to its name plus a random
    number when the list already has that name (0x00424e90, "%s%d" with
    rand()), pushed on the list's end, the page redrawn (0x0044cf50). The
    dialogs make this FIX (A) when it is ported (the screen's item 4).
  - leave 0x00448a20: the MECHLAB sounds released (0x0056f530) and their
    handles cleared, the 3D world unloaded (0x00450390), the screen deleted
    (0x00448ef0), the frame clear's flag ([0x008026f0], 0x0057a3c0) set back
    from +0x64 (which the 3D world's load keeps), the pilot and store
    cleared (the flag stays), the input reset, the model cache purged
    (0x005394c0(1, 1)).
  - The state's message handler returns 1 (0x0042a130); suspend and resume
    are empty (0x004bf620); Escape does nothing (0x0049bfd0).
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
| PART_HEAD .. PART_RLEG | part buttons (0x005c13b0) | 18, all of them |
| SELECT_WEAPON / AMMO / EQUIP / ARMOR | UNASSIGNED_LIST's tabs | 25, 23, 24, 26 |
| UNASSIGNED_LIST | a list (0x005c1d68, ctor 0x004535d0) | |
| ASSIGNED_LIST | a list (0x005c1fe0, ctor 0x004552e0) | |
| WEAPONS_UNGROUPED, WEAPONS_GROUPED | lists (0x005c1300, 0x005c1250) | |
| CURRENT_CHASSIS | a panel (0x005c11c0) | |
| ENGINE_TABLE | the engine panel (0x005c1130; faster, slower, the two type buttons) | 19..22 |
| MECH_DATA, CHASSIS_DATA, ARMOR_TITLES, LOADOUT_LIST | texts and their panels (0x005c18b8, 0x005c1ac0) | |

The ids 0..32 are the press's (0x0044d4d0, the screen's +0x14): 0..6 a page
(0x0044ce30: 6 is ACCEPT, otherwise the old page's controls hidden and the
new one's shown, 0x0044ca50), 8..11 the slot (0x00448f80: the old slot's
mech back out of the store, the new one's put in, its controls refreshed),
14..32 the pages' own (the loadouts' buttons, 18 a part, 19..22 the
engine, the sorts; there is no sink press).

The screen (vtable 0x005c1110, new(0x1b468), destructor 0x0044c2f0) is a
MenuScreen like the briefing's; its page is +0x111ec.

### The screen's making (0x0044b920, step 2)

Ported in `mech3/lab_screen.{h,cpp}`, checked by `test_lab_screen.py`.

- It is a menu screen without a state (+0x10fec stays 0); its page
  (+0x111ec) starts at -1.
- **The members, in the order they are made.** TEMP_BACKGROUND, then the
  three panels, then LOADOUT_LIST and ICON_GLOW. Then the seven tabs
  (help buttons, ids 0..6 set at binding) and PILOT_GLOW. Then **twelve**
  mech buttons, of which only the first four are bound (ICON_MECH1..4,
  ids 8..11). Then CHASSIS_LIST, CONFIG_LIST (with its four buttons
  inside it) and CONFIG_EDIT (17 characters). Then SKIN_LIST, the two
  sorts (ids 31, 32) and the eight parts. Then UNASSIGNED_LIST (four
  SELECT_* check buttons, ids 0x19, 0x17, 0x18, 0x1a, four sorts, ids
  0x1b..0x1e, and ALIST_TITLES). Then ASSIGNED_LIST, the two weapon
  lists, CURRENT_CHASSIS, ENGINE_TABLE (seven rows and four buttons, ids
  0x13..0x16) and MESSAGE_AREA. Last come three panels made with new:
  CHASSIS_DATA (0x00457600), MECH_DATA (0x00456cd0) and ARMOR_TITLES
  (inline: a button, eleven toggles 0x0044fab0, a button 0x0044fad0).
- **The panels** (PANEL_LEFT/RIGHT/CENTER, 0x0044b010) have three looks
  read from "single", "double" and "double2". The destructor gives back
  only the first two images: "double2"'s is never released.
- **The part buttons.** PART_HEAD is made by 0x0044ed40 and the others by
  0x0044f290, but the constructor overwrites the head's vtable, so all
  eight are one class (0x005c13b0). They read as plain check buttons, with
  id 0x12 and their part's number at +0x16c. The numbers are: head 6,
  torso 1, left torso 7, right torso 8, left leg 4, right leg 5, left arm
  2, right arm 3. The order of binding is head, torso, left and right
  torso, legs, arms.
- **MESSAGE_AREA** (0x0044e6d0) reads a SOUND and plays it with every
  text it is given (0x0044e740). At the end of making it shows message
  0xdcc.
- **The end of the constructor.** All 7 pages are hidden (0x0044ca50(i,
  0)). ICON_MISSIONDATA is disabled for good. The mech buttons past the
  lance's slot count are hidden.
- **The page switch 0x0044ca50(page, show).**
  - Every call does three things. It asks for the shown slot's mech's
    tech (0x00484720) and drops the answer. It shows MECH_DATA with its
    arrows off (+0x188 vt+0x98(0, 0)). It shows CURRENT_CHASSIS.
  - Page 0 (chassis and loadouts, panels "double", text 0x18c) shows or
    hides CHASSIS_LIST, CONFIG_LIST, CHASSIS_DATA, LOADOUT_LIST, the two
    sorts and SKIN_LIST. SKIN_LIST is never shown in multiplayer (game
    type 3).
  - Page 3 (criticals, "single", text 0x1aa) turns MECH_DATA's arrows on
    while the flag [0x00605a84] is clear. It always hides ARMOR_TITLES.
    It shows or hides ASSIGNED_LIST, the eight parts and UNASSIGNED_LIST.
    Hidden, it drops the part pick (0x004504d0(-1)).
  - Page 4 (weapon groups, "single", text 0x18d) refreshes the groups
    (0x0044dcf0) and shows or hides the two weapon lists.
  - The text goes to CURRENT_CHASSIS's first label (taken unchecked).
  - The glow goes to the page's tab (0x0044af90). The tab shows its
    ACTIVATE image and stops blinking, and ICON_GLOW moves to the tab's
    GLOW_OFFSET.
  - Pages 1, 2, 5 and 6 have no controls of their own here. Their
    contents come from the page refresh (0x0044cf50, later steps).

### The press, the pages and the slots (step 2)

- **The press 0x0044d4d0.**
  - While a rename is under way (CONFIG_EDIT's +0x38c), any press but
    CONFIG_SAVE (16) first asks the bar to finish it (0x0044a3a0, step 4).
    If that fails, the press is dropped.
  - Ids 0..6 go to 0x0044ce30 and 8..11 to the slot switch (id - 8).
  - Ids 14..32 are the pages' own (a jump table at 0x0044dc34; steps 3..8).
  - Anything else, CONFIG_ADD's 13 among them, does nothing.
- **0x0044ce30(page).**
  - ACCEPT (6) takes the shown slot's mech back out of the lab's store
    (0x00472a30) and calls the state's 0x00448870, which pops it with 4.
  - Any other page hides the old one's controls (0x0044ca50(old, 0)),
    shows the new one's and redraws it (0x0044cf50, the pages' contents,
    later steps).
- **The slot switch 0x00448f80(slot), on the state.**
  - A slot the game lacks is refused.
  - Another slot takes the old slot's mech out of the lab's store
    (0x00472a30) and puts the new slot's in (0x00471c10). Then it sets
    the chassis (0x00449010) and shows the lance's buttons (0x0044ce90).
- **0x00449010(mech).** A chassis different from the slot's gives the
  slot's loadout back to the store (0x00433df0(0, store, 1)). It sets the
  slot's mech (0x005a2ba0, a +0x04 setter the linker shares with zTwoD's
  window), shows it on the slot's button and selects it in CHASSIS_LIST
  (0x00449b00, step 3). Either way +0x38 asks the 3D view for it.
- **0x0044ce90(slot).**
  - Every lance button gets its slot's mech (0x0044a890; -1 for an empty
    slot): icons green, grey and red, by 0x0044a9b0.
  - The given button is chosen, with PILOT_GLOW at its GLOW_OFFSET.
  - CHASSIS_LIST gets the lab's store and refills (0x00449220), selects
    the mech, and the page redraws.
- **0x0044a890** shows the mech's name under the button only when the name
  is already showing (the name's own hidden flag, +0x18c).
- **In the program.**
  - MECHLAB opens the real screen, and ACCEPT on it comes back to the
    briefing (the campaign test).
  - The pages' contents are still empty: their controls are made, and
    their reading and showing wait for their steps (unported
    "lab.vt+<slot>").

### The chassis page and the page refresh (step 3)

- **CHASSIS_LIST** (vtable 0x005c0cc0).
  - It takes every row (0x0045b6e0).
  - The refill 0x00449250 lists the store's mechs by name (0x00484670).
    For a lancemate in a campaign, an empty entry (message 0x23d) comes
    first.
  - It sorts with the exe's inlined std::sort (VC98's insertion sort up to
    16 items, median-of-three introsort above). The comparator 0x004496c0
    uses the mech's number for SORT_CHASSISNAME (0): it looks both names
    up and ignores them. SORT_CHASSISMASS (1) uses the tonnage, then the
    number. The sorts' presses 31 and 32 call 0x00449160.
  - The row of the shown slot's mech is chosen by its place *before* the
    sort.
  - The layout 0x0044f4c0 / 0x00449760 puts each mech's maximum tonnage
    ("%.1f") 175 pixels right of its row: lab_large_greenR for the rows,
    and the chosen row's in lab_large_redR (font set after showing, its
    text used as the format). The list's selected-row object shares that
    text.
  - A row pressed (0x00449b30) asks to finish a rename first. The same
    chassis does nothing. A modified loadout asks 0x137 / 0x1ed with the
    mech's and the loadout's names. Then it plays the select sound,
    selects the row and sets the slot's chassis (0x00449010).
- **SKIN_LIST.**
  - Its select 0x0044b1c0 skins the 3D model when one is up (0x0044fd90)
    and sets lance slot 0's skin (+0x10), whichever slot is shown.
  - The refresh fills it from the mech definition's skin set (+0x1c). It
    chooses slot 0's skin, clamped to 0..count (count itself is one past
    the end). The arrows go on unless a rename is under way.
- **The page refresh 0x0044cf70 (0x0044cf50 with the shown slot's
  loadout).**
  - It empties CONFIG_LIST and refills SKIN_LIST.
  - With a loadout, it recounts it (0x00425bd0) and writes CURRENT_CHASSIS's
    second label, "mech: loadout" (or message 0xdcd when it is modified).
    Then it refreshes the data panels (0x00457940, 0x00457330,
    0x0044b230), the item lists (+0xb8), ARMOR_TITLES (0x00450e00),
    ENGINE_TABLE (0x0044b4b0) and the groups (0x0044dcf0).
  - Without one, the label gets message 0x249.
  - CRITICALS and WEAPON GROUPS are enabled only with a loadout.
  - WEAPONS, EQUIPMENT and MISSION DATA are hidden.
  - ACCEPT is enabled only while every slot's loadout passes 0x004254d0.
- **The campaign's starting mech** (0x004733b0, ported here as the pilot
  hook's default, with ConfigList::find 0x00424e90). A new campaign's
  store gets mech 3 with its loadout named by message 0xf20 (added twice,
  one taken out). Before this, the program's new campaigns started with
  an empty store, so the briefing's lance fit cleared slot 0.

### The loadouts page (step 4)

- **CONFIG_LIST** (vtable 0x005c1588).
  - It binds CONFIG_ADD, CONFIG_DELETE, CONFIG_SAVE and CONFIG_RENAME
    inside itself (0x0044f390), and shows and hides them with itself.
  - The fill 0x00449fb0 lists the shown slot's mech's loadouts
    (definition +0x20). It leaves out training's (+0x1e), and the player's
    own in the multiplayer lab (flag set).
  - Each row is the loadout's name, plus message 0x23e when the lab
    store can't fill it (0x004735f0 with the slot's loadout counted in;
    `Store::has_config` with a loadout as `other`).
  - A slot without a loadout first gets one (0x00449e10): the first the
    store can fill, else the list's first.
  - The row of the slot's loadout is chosen by name (0x00449d40). There is
    no row when it is modified.
  - The buttons (0x0044a1f0): ADD is never enabled. RENAME and DELETE work
    on the player's own (+0x1c). SAVE works with a loadout, outside the
    multiplayer lab.
  - A row pressed (0x00449c40) asks to finish a rename first. A modified
    loadout asks 0x137 / 0x1ed. Then the slot takes that loadout from the
    store (0x00433df0) and the page redraws.
- **The presses.**
  - 14 RENAME types over the chosen row (0x0044dca0 -> 0x0044a2f0).
  - 15 DELETE takes the player's own loadout out of its list (0x00425090),
    deletes it and writes the list (0x00424f60).
  - 16 SAVE finishes a rename under way; otherwise it copies the slot's
    loadout (0x00424010, no store) to the list's end. When the name was
    taken, the name is typed over it.
  - 17 (CONFIG_EDIT's own) and 13 (ADD) do nothing.
- **CONFIG_EDIT's finish 0x0044a3a0.** Enter is 0x0044a6e0, Escape is
  0x0044f4b0 (cancel).
  - A name taken by another loadout:
    - if that one is the game's, message 0xfb2 and the bar stays;
    - if it is the player's, the question 0x137 / 0xdcb; yes renames the
      slot's loadout and the edited one, removes and deletes the old one,
      and writes the list.
  - A free name renames both and writes the list.
  - Escape or an empty name on SAVE's copy throws the copy away.
  - The bar hides and the page redraws.
- **Ctrl+X / Ctrl+I.**
  - The exe exports with 0x00426180 (the name, then the loadout, 0x00426090,
    in a .m3c file) and imports with 0x00426300 / 0x004260c0.
  - An import goes to the end of the *shown* mech's list, renamed with
    rand() when its name is taken (0x00448c1d, ported in mech_lab.cpp).
  - FIX (A): the export\ and import\ folders instead of the dialogs, and
    no checksum check (FIX (trap)).

### The item lists' base (step 5a)

UNASSIGNED_LIST and ASSIGNED_LIST share a base (0x00451b40, vtable
0x005c1b70, 0x22c bytes; `LabItemList` in `mech3/lab_items.{h,cpp}`), a near
copy of the pilot screen's FileList (menus.md) with its own row object.

- **Items** (0x10 bytes): a name (strdup'd; null gives an empty row), the
  store item, a count and a font. +0x90 adds by text, +0x94 by store item
  (ammo, kind 2, is message 0xf59 with the item's name (vt+0x20); a count
  of -1 takes the item's critical slots, vt+0x18), +0x98 by part (0x00484d70's
  name, the part number as the "item"). Adding does not lay out; clearing
  (0x00453420) does.
- **Reading** (0x00451e60): SELECTED_FONT (else NORMAL_FONT), NORMAL_FONT,
  LARGE_FONT, STATIC_FONT, MASS_FONT, COUNT_FONT, GREY_FONT, ROLLOVER_SOUND,
  SELECT_SOUND, SPACING, OFFSET, SIZE (the rows' width and height), HEIGHT
  (12: SIZE's height in rows), BAR (an image at its OFFSET from the selected
  row), ITEMROLLOVER (an image beside the row under the mouse) and FONT, the
  rollover colour, read from the list's own entry: the shipped lists keep it
  inside ITEMROLLOVER, so it is never found and hovering does not recolour.
  UP and DOWN (vtables 0x005c1cc0, 0x005c1c30) are placed from the list and
  taken back out of the dialog; the list updates them. The list sits at its
  position with its image's rectangle (0x00452bc0).
- **Binding** (0x004522d0): the rows made (0x00453020) and a selected row
  (never shown here), then the layout.
- **The rows** (0x00451410, vtable 0x005c1ac0, 0x1490 bytes): a Text with the
  name over a panel (`HudPanel`, 0xac by 13, (25, 25, 25)), the count
  ("%02d", right-aligned, 0xac right), two texts at 0x93 and 0x7a (the
  mass and the store's count, set by the subclasses) and a green line from
  the name's end to 0x89 (the subclasses'). A row hits on its text
  rectangle 0xac wide and draws only for an item with a name.
- **Layout** (0x00452780): rows from `top` down from OFFSET, SPACING.y + 13
  apart while they fit SIZE's height (3 pixels over allowed); the selected
  item's row in SELECTED_FONT with the BAR on it; the shown range's end
  kept at +0x15c; the rectangle round the rows (as wide as the widest
  panel) when the list has no image.
- **The frame** (0x004523f0), hover (0x00452cb0), enter and leave, the
  press (0x00452f40) and the hit (0x00452af0) are FileList's with small
  differences: the arrows light by their hit alone (not their enabled
  state), the hover does not tell the rows, a scroll (0x00452a90) refills
  rather than lays out, select (0x00452a40) refuses -1 and does not scroll
  into view, DOWN is enabled while the end is below the item count.
- The base's own refill (0x00453540, the store's items through the filter
  at +0x228) is reached by neither list: both have their own (steps 5b, 6).
- `HudPanel`'s destructor (0x0043a890) was missing: the panel's image (its
  colours and alpha) leaked with every HUD; now freed, as the original.

Tested by `test_lab_list.py` (`tests/probe_lab_list.cpp`): the base list
bound from LABMENU's UNASSIGNED_LIST or ASSIGNED_LIST entry (shipped or
mutated) in a menu screen whose on_button is logged, with items by text,
store item and part, a store refill, selects, scrolls, clears, show and hide
and frames of the mouse over the rows and arrows; the dialog, list, items,
rows, arrows and images dumped, then the destructor. 100 seeds match.
Breaking it: 34 of 37 caught; the three left change nothing the original
shows (a grey that packs to the same 565 pixel, the fill's font set twice
as the exe does, the panel's alpha set just before its image is freed).

### The unassigned list (step 5b1)

UNASSIGNED_LIST (0x004535d0, vtable 0x005c1d68, 0xeac bytes;
`LabUnassignedList` in `mech3/lab_screen.{h,cpp}`): the lab store's items
of one tab, on the criticals page.

- **Its controls**: SELECT_WEAPON, SELECT_AMMO, SELECT_EQUIP, SELECT_ARMOR
  (vtable 0x005c1f48, ids 25, 23, 24, 26), SORT_ITEM, SORT_QTY, SORT_MASS,
  SORT_CRIT (ids 27..30) and ALIST_TITLES (+0xd60), bound by its reading
  (0x00454fc0) after the base's, which also reads SHORT_SIZE (+0x22c) and
  keeps SIZE (+0x234) and DOWN's distance below the rows (+0x23c). The
  selects and the part buttons share a class (`LabCheck`): CheckButton's
  reading (0x00456980), and their labels shown only while unchecked
  (0x0044f2c0); the selects press as InvertCheck, reporting their id.
- **The tab** (0x004550d0, kind +0xd58, armour +0xd54): a new kind or the
  armour tab goes back to the top; the tab's button checked; weapons and
  ammo take SIZE, equipment and armour SHORT_SIZE, DOWN moved under the
  rows; ENGINE_TABLE shown on the equipment tab only; PANEL_RIGHT's single
  look. The screen's presses 23..26 set the kind (2, 3, 1, 3 with armour),
  the top to 0 and refill; 27..30 sort (0x00453f00: 0 the item's id, 2 the
  store's count, 1 the mass, 3 the slots) and refill.
- **Refill** (0x00453f20): ALIST_TITLES's labels with the shown loadout's
  critical slots left (message 0xf5b, 0x00425d80), then endo steel's
  (0x24c, 0x00425e80) and ferro-fibrous's (0x24d, 0x00425e40) when it has
  them, the unused labels hidden; the tab; the store's items through the
  filter (0x00453770 the tab's kind without armour, 0x00454580 armour:
  equipment 1 and 2), each in NORMAL_FONT (0x00454f60) with its slots as
  its count; a sink rule that never fires (below); VC98's std::sort with
  0x004564a0 (mass: ammo by id, the weights compared twice; count:
  unlimited items by id; ties by id); the layout; each shown row greyed
  (GREY_FONT) unless +0xa4 (= +0xa8, step 5b2's "may it go on") allows it,
  its mass ("1.0" for ammo, else "%.1f" of one's weight) and, unless
  unlimited, the store's count, in COUNT_FONT when greyed and MASS_FONT
  otherwise. Then the screen: off the armour tab ASSIGNED_LIST is shown
  and refilled if hidden, the eight part buttons shown and ARMOR_TITLES
  hidden; on it the part pick is dropped (0x004504d0(-1)), ASSIGNED_LIST
  and the parts hidden and ARMOR_TITLES shown.
- **The sink rule**: with fewer sink items (0x00425f30) than the engine's
  sinks (0x00426070, always 10) less 10, on the equipment tab, sinks would
  list with no slots. 10 - 10 > n never holds (discoveries.md).
- Show (0x00454db0) and hide (0x00454e70) carry the controls; hide unchecks
  the selects and hides ENGINE_TABLE too.
- The loadout's slot helpers are `Config`'s now: 0x00425d80 (slots left:
  the free count less ferro-fibrous's, endo steel's and the engine's),
  0x00425e40 (14, 7 with Clan armour), 0x00425e80 (14, 7 on a Clan mech),
  0x00425e60 / 0x00432f70 (an XL engine 12, 10 on a Clan mech, else 6),
  0x00426070 (10), 0x00425e20 (a part's free slots).

### The item rules (step 5b2)

**May it go on** (+0xa8, 0x00453840; +0xa4 0x00453830 calls it, so a row
it refuses is greyed). The row's item, the shown loadout (ASSIGNED_LIST's
+0x224: the screen's, through [0x00605a58] + 0x1a13c) and ASSIGNED_LIST's
chosen part (0x00455eb0, its +0x160 through 0x00455ec0; -1 none; step 6).
The shown slot (the game's vt+0x34 with [0x00605a28]) gives the mech's
definition (0x004846a0). In order:

- Never while the lab is read-only ([0x00605a84], the Lab State's flag).
- Off the armour tab, the weight: the loadout's (0x00425300(-1)) plus one
  of the item (ammo a ton) must not pass the mech's most (0x004846e0).
- On the armour tab only armour (equipment 1, standard, or 2,
  ferro-fibrous): its points (0x00424760 by its tech and kind, the
  loadout's tons plus half a ton when it is the kind the mech has) within
  the most the mech's parts take (0x00456750 over sections 3..10: twice the
  internal structure, the head 9), the weight with that half ton, and the
  slots left plus ferro-fibrous's at least ferro-fibrous's own (7 Clan, 14
  Inner Sphere; standard 0). Nothing else goes on there.
- Weapons and TAG (equipment 13) only while the loadout has fewer than 16
  of them (0x00425ea0).
- Equipment by kind, counting the loadout's entries of it (0x00473570, any
  tech): AMS (0) and FCS (3) up to five; BAP (4), ECM (8), MASC (12), TAG
  (13) and the targeting computer (14) one; C3 (6) and its slave (7) one,
  and not both. The rest (sinks, CASE, jump jets) any number.
- No part chosen: yes. With one: jump jets (11) never in the arms or the
  head (parts 2, 3, 6); armour by the slots left less ferro-fibrous's
  (standard any, ferro-fibrous its 7 or 14); sinks (9, 10) by the part's
  free slots (0x00425e20) and the loadout's, after a test that never holds
  (the engine's 10 sinks less 10, as the refill's); everything else by its
  slots (vt+0x18) against both, and a weapon only in a part the mech's
  definition has a firepoint on (+0x24, each firepoint's +0x0c).

The refresh (0x0044cf50) refills UNASSIGNED_LIST before it gives
ASSIGNED_LIST the new loadout, so a slot changed while the criticals page
is up greys the rows by the previous slot's loadout until the next refill
(the exe's order, kept). The exe reads the loadout and the definition
unchecked: the criticals page is grey without a loadout and the lab's
entry refreshes first, so in play both are there.

Ported: `LabUnassignedList::can_xa8`, `part_armor_max` (0x00456750),
`Config::weapon_count` (0x00425ea0) and `equipment_count` (0x00473570, a
Store method the exe runs on the loadout's list); ASSIGNED_LIST's part is
step 6's (`LabLaterList::part`, "0x00455eb0"). Engine hooks: the loadout's
weight, weapons, equipment, a part's free slots, its armour fields, an
item's tech, a definition's firepoint parts. Test: test_lab_screen.py
runs the exe's rule (the loadout's helpers stubbed with answers, the
loadout's weight often just under its mech's most; the loadouts' armour,
the definitions' firepoints and C3, BAP, ECM and TAG in the store in the
setup; the criticals page and its tabs only after a refresh with a
loadout, as in play): 80/80, breaks 14 of 15. The one left cannot run:
armour's slot test by a chosen part, since armour lists only on the
armour tab, whose rule returns before the part.

**Putting it on** (+0xac, 0x00454600; a row's left-up 0x00456820 calls
it). The list left and entered; the row's item; the rule (+0xa8) again;
0x0044dc80: the shown slot (0x0044e680, the game's vt+0x34 with
[0x00605a28]) has a loadout and the lab is not read-only. Into that
loadout (the slot's +0x08, not ASSIGNED_LIST's) from the lab's store
(+0x220), `n` 1:

- Ammo: `n` the lab's rounds (0x004b4900) of the store's count, or of
  100000 when the store is the full one (+0x10).
- Weapons: none past 16 (0x00425ea0), "Only 16 weapons are allowed" in
  MESSAGE_AREA (its vt+0x7c, the text as the format).
- Armour, dealt with here: the same kind and tech on the mech (0x004714c0
  with a stand-in 0x0041ec00): one more half ton (the item's count and
  +0x2c both), one out of the store; else the mech's own kind (its +0x34
  ferro and +0x30 tech) found: all of it back to the store, out of the
  loadout (0x00471de0), and as much of the new as there was of the old
  (all of it when the store's item is unlimited, else its count at most)
  onto the loadout and out of the store, the sections spread anew
  (0x004247a0) when the points no longer cover them; none of the mech's:
  one half ton on.
- Sinks: only of the kind already on (or onto a mech with none),
  else message 0xf02 ("Incompatable Heat Sink Type").
- Jump jets: up to the loadout's limit (0x00425fb0, 0x00425ff0), else
  0xfc1 "%1 can only carry %2!d! jump jets" with the mech's name
  (0x00484670) into 0x46 bytes, then as CString::Format's format.
- A weapon takes a firepoint of the chosen part (ASSIGNED_LIST's,
  0x00455eb0) from the definition (+0x24): none there, it does not go on;
  missiles (ids 0x11..0x14 and 0x1a..0x1f) only those whose +0x04 is over
  0. The static [0x005e47d4] (1 at start) alternates: set, the last that
  fits (and cleared); clear, the first (and set). Its place in the
  definition's list is the item's +0x0c. When none fits, the weapon still
  goes on and its +0x0c is the part's number (an exe bug, kept: the firepoint
  of that index is used in play; a fixes.md candidate).
- Then the item on (the loadout's vt+0x00, 0x004723c0), the select sound
  (0x00452c80(1, 1.0)), its place (vt+0x04), out of the store, the
  loadout modified and recounted; the list left and entered, the screen
  refreshed (0x0044cf50), left and entered again.

The armour spread (0x004247a0, on the loadout's +0x2c with the mech): no
change when the tons' points pass what the mech's sections take (2 x the
structure, 0x004264e0, the head 9) or already equal the sections' sum;
none at all for 0 points. Else each section's share of the points kept,
the head first (up to 9), the rest by share (rounded, 0x005bd400's -0.5)
within each section's most; the rear torsos (3..5) cut to what their front
(0..2) leaves; then a point at a time off round from the first section
(never the head) while over, and on while under wherever the room left
is not 0 (front and rear sharing one most). The room is never negative
after the caps, so "not 0" is "over 0".

Ported: `lab_put_on` (an Engine hook: the lab test keeps its stand-in),
`lab_firepoint_toggle`, `Config::remove` (0x00471de0), `find_like`
(0x004714c0 on the loadout), `spread_armor` (0x004247a0 with 0x004264e0,
0x00426560, 0x00426540). Tests: test_salvage.py's puton part, the exe's
0x00454600 over real loadouts and stores (0x00423d00, 0x004723c0, the
recount native) with the list's +0x3c, +0x40, +0xa8, MESSAGE_AREA, the
part, the refresh and the sound logged: 200/200, breaks 12 of 14 (the two
left are the remove's: the put-on removes only old armour, then adds the
new); test_mechsave.py adds the spread at seven tonnages and the remove of
one of each item to every loaded loadout (breaks 6 of 7 and 3 of 4; the
last of each cannot differ: the room after the caps, and standard
armour's tech already 3 in the game's loadouts).

Tested in `test_lab_screen.py` (`tests/probe_lab.cpp`): the store now has
ammo and equipment (armour, sinks, unlimited items) and the weapons'
tons from weapons.zrd (test_mechsave.py's table) on both sides; the plans
press the tabs and sorts on a shown criticals page; the list, its items,
rows (name, slots, mass, count with flags and fonts) and ALIST_TITLES
dumped, with its arrows' places; a scroll op; ALIST_TITLES's messages
carry their count; ASSIGNED_LIST's stand-in keeps its hidden flag as its
own show and hide would; the slot helpers stubbed with random answers (so
the sink rule's other side runs too) and checked themselves in
`test_mechsave.py` on every saved loadout. 60 seeds match. Breaking it:
43 of 43 caught (the list is in the frontend worklog).

### The criticals page (step 6)

**ASSIGNED_LIST** (0x004552e0: the item lists' base, vtable 0x005c1fe0;
+0x22c the part being listed, not set by the constructor). Its rows are
the shown loadout (+0x224) by part: a part row's object is the part's
number (1..8: 0x00455d70 tells them, 0x00455ec0 reads it, 0x004560d0
finds a part's row), the other rows the loadout's items (0x00455df0,
0x00455e50). Its own slots: +0x90 a text in STATIC_FONT (0x00455bf0);
+0x94 an item in NORMAL_FONT when the player can take it off (the
loadout the player's, +0x1c, and the lab not read-only), else STATIC
(0x00455b60); +0x98 a part in SELECTED_FONT on the selected row, else
LARGE, and listed from then on (0x00455af0); +0xa4 part rows, and item
rows when they can come off, light (0x00455f30); +0xa8 true; +0xb0 no
rollover on part rows (0x00455f90); +0xb4, +0x60, +0x64, +0x84 the
base's.

- **Refill** (+0xb8, 0x004554d0, shown only). The chosen part kept (the
  head when none); cleared; the shown slot's mech's tech (0x00484720);
  an XL engine's side-torso slots (3, 2 Clan); recounted; ferro-fibrous
  (+0x34: 7 Clan armour, else 14) and endo steel (+0x20: 7 on a Clan mech,
  else 14) to place. Then in the exe's order the head (cockpit 0x24e,
  life support 0x24f twice, sensors 0x250), the centre torso (the engine,
  0x251 with its name 0x00432f00, 6 slots; the gyro 0x252, 4), the left
  and right torsos (an XL engine's 0x253), the arms (shoulder 0x255, upper
  arm 0x256), the legs (hip 0x2d5, upper leg 0x257, lower leg 0x258, foot
  0x259): each a part row, its fixed rows (their slots as their count),
  then 0x004553f0: only when the loadout has no slots left (0x00425d80
  0), the part's free slots take ferro-fibrous rows and then "Endo-Steel
  Internals" from what is left to place; then the loadout's items whose
  place on the mech (vt+0x08: a weapon's firepoint's part) is this part,
  their slots as the count (a sink of no group, -1, none). Then the
  chosen part's row selected (0x00456150, with the 3D view's part
  0x004504d0, the part buttons 0x0044d3e0 and UNASSIGNED_LIST's refill),
  the layout, each part row's slot bar (0x00455c50: "used/total", total
  0x00425db0's 12 or 6, used all of them when the loadout has none left,
  else total less the part's free slots; 0x00451870 the count, the line
  from after the name, in the row's colour) and each item row's tons
  ("%.1f" of vt+0x00(-1)) and count, in COUNT_FONT when read-only, else
  MASS.
- **Press** (+0xac, 0x00455fb0): a part row becomes the top and selected
  row and refills, checks its button and the 3D view's part; an item row
  (an editable lab) goes back to the store ([0x00605a80]) whole, off the
  loadout (0x00471de0), recounted, the select sound, then the screen's
  refresh (0x0044cf50).
- **Arrows** (+0x9c, 0x004559e0): the chosen part steps in the list's
  order, head, centre, left and right torso, left and right arm, left and
  right leg; up from the head and down from the right leg stay.
- **The part buttons** (PART_HEAD..PART_RLEG): a press (0x0044f340) of a
  checked one unchecks it and reports none first (0), then its part,
  to the screen's 0x12, which chooses that part's row (0x00456130);
  0x0044d3e0 checks only the chosen part's button.

Ported: `LabAssignedList` (all of the above), `LabPart::on_left_up`,
`LabScreen::check_part`, `LabItemRow::show_slots`, `lab_take_off`
(the press's item row, an Engine hook like the put-on), `engine_name`
(0x00432f00, shared with the salvage's item names). Tests:
test_lab_screen.py runs the exe's ASSIGNED_LIST (the loadouts with real
items, mirrored in the probe; part buttons and arrows in the plan; the
list's rows, slot bars and the buttons dumped): 80/80, the criticals page
filled in 29 of them; test_salvage.py's puton part takes items off with the
exe's 0x00455fb0 on real loadouts and stores (200/200, 249 take-offs).
Breaks 16 of 17 (the one left, which item rows light, shows only under the
mouse, and the plans move none over the list).

### The weapon groups page (step 7)

**WEAPONS_UNGROUPED** (vtable 0x005c1300) and **WEAPONS_GROUPED**
(0x005c1250): file lists sharing a reading (0x0044df70: STATIC_FONT at
+0x200) and a layout (0x0044f240: the list's, then 0x0044e020), the rows'
attached objects deleted with the list (0x0044dee0). A weapon's group is
its +0x08 (-1 none, 0..4).

- **Refresh** (0x0044dcf0, on page 4 with a loadout shown; the shown slot
  read through 0x0044e680 three times over): the chosen group kept
  (WEAPONS_GROUPED's selected row's, 0x0044e630); both lists emptied;
  WEAPONS_UNGROUPED gets the weapons of no group, WEAPONS_GROUPED each
  group's row (message 0x24b with its number, its value 0..4, 0x0044e580)
  and its weapons, each by its label (vt+0x1c, 0x00473740: "name (n)");
  then the chosen group's row selected again (0x0044e4e0, the first when
  none).
- **Layout** (0x0044e020): down from OFFSET, each shown row (and the
  selected one's place) 12 + 5 apart: a weapon's row in NORMAL_FONT with
  its tag (vtable 0x005c16c0: a dark panel 172 x 12, 3 to the left, the
  part the weapon is on, 0x00484d70 of vt+0x08, right-aligned 5 inside its
  edge); a group's row in STATIC_FONT, 5 lower after the first, with the
  selected row's text and BAR on it.
- **Presses** (+0x98): a weapon in WEAPONS_UNGROUPED joins the chosen
  group (0x0044e3f0; -1 when no group row is chosen), one in
  WEAPONS_GROUPED leaves its group (0x0044e490), each with the select
  sound and the refresh; a group's row is chosen (FileList's select).
  WEAPONS_GROUPED chooses and rolls over by row (+0x94 0x0044e560: a group's
  row; +0x9c 0x0044e540: not one); +0xa8 is the row's weapon (0x0044e450,
  0x0044e5e0: none for a group's row).

Ported: `LabWeaponList`, `LabLocationTag`, `LabScreen::refresh_groups`.
Tests: test_lab_screen.py runs the exe's lists (the loadouts' weapons of no
group and in groups 0..4; a weapon groups session: the page's tab, rows of
both lists pressed; both lists, their rows and tags dumped): 80/80, the
groups in 22 runs and the tags in 12. Breaks 13 of 15: which group rows can
be chosen shows only under the mouse. The first group chosen when none is
(0x0044e4e0's out-of-range case) cannot happen in play (only a group's row
can be chosen, and a row past the list counts as group 0); the test sets a
weapon's row chosen and refreshes (step 8a), and its break shows there.

### The engine and the data tables (step 8a)

**The data tables' base** (vtable 0x005c20a0, ctor 0x004569c0, dtor
0x00456a40): a Control holding rows (vtable 0x005c18b8, 0x960 bytes: a
label with its value at +0x4b0) at +0x178. Its reading (0x00456a60) clears
every flag but hidden and takes LABEL_FONT, VALUE_FONT, ALTERNATE_FONT
(+0x14c..+0x154), WIDTH (+0x158, 130), OFFSET (+0x15c, 5 12), SPACING
(+0x164, 0 14) and POSITION (+0x16c); its place (0x00456c30) puts each
row's label at OFFSET (plus SPACING's x), its value WIDTH to the right,
each next row SPACING lower. A row's value is set as "%d" or "%d %s"
(0x0044ef50, with its label 0x0044ef00) or as a float (0x0044ee80, "%.1f";
with a unit its format is " %s", so the unit never shows and the value is
the number with a space before it; without one it is blank).

**ENGINE_TABLE** (vtable 0x005c1130; seven rows, four buttons: faster
0x13 and the right type button 0x15 read from TOGGLE_RIGHT, slower 0x14
and the left type 0x16 from TOGGLE_LEFT, 0x0044b830; shown and hidden with
its buttons, 0x0044b770 / 0x0044b7e0). Its refresh (0x0044b4b0, from the
page refresh with a loadout shown) fills:

| Row | Label | Value |
|---|---|---|
| 0 | 0x23f | the rating, 10 + 5 x the engine's index (0x00432fa0) |
| 1 | 0x240 | the maker, message 0x210 + its table's (0x00432ec0) |
| 2 | 0x242 | Extra Light 0x241 or Standard 0x2d2 |
| 3 | 0x243 | the tons (0x00432f40: an XL's are ceil(t) / 2), the unit 0x244 dropped |
| 4 | 0x248 | the engine's critical slots (0x00425e60) |

("kph", 0x245, is fetched and not used; rows 5 and 6 stay empty.) Faster
and slower sit 50 right and 10 left of the rating's value, 2 lower, the
type buttons the same beside the type's. The type buttons are enabled on
the player's own loadouts (+0x1c); faster while the rating is under the
definition's most (+0x14), slower while over its least (+0x10).

**The presses** (0x0044d4d0's cases 18..22; 0x0044dc80 first: a loadout
on the shown slot and the flag 0x00605a84 clear):

- 18: ASSIGNED_LIST's part chosen (0x00456130); no sink or armour press
  goes through here.
- 19 faster: the index one up (0x00432e70, to 78 at most), the loadout's
  tons asked, back down; over the mech's most it stays and the message
  box says "Need to free up %1 tons" (0xdce, "%.1f" over); at or under,
  up again.
- 20 slower: one down (0x00432e90, to 0).
- 21 and 22 (the same) the type: to XL it needs n free slots in each side
  torso (n = 3, 2 on a Clan mech; else 0xdcf with n) and 2n free in all
  (else 0xdd0); back to standard the loadout's tons with a standard
  engine must be under the mech's most (at it is refused, unlike faster;
  0xdce).
- After a change: the loadout marked changed (+0x1f), its slots counted
  (0x00425bd0) and the page refreshed.

Ported: `LabTable`, `LabRow`'s setters, `LabEngineTable`,
`LabScreen::engine_editable`, `engine_press`; `engine_weight_of`,
`engine_rating_of`, `engine_maker_message` (mech_names, mech_salvage).
Tests: test_lab_screen.py runs the exe's table and presses (an engine
session on the equipment tab: faster, slower and both type buttons, now
and then another slot; ENGINE_TABLE's rows and buttons dumped, the
loadout's engine and type after each step; the loadout's tons at, under
and over the limits): 80/80, the XL type shown in 46 runs, the refusals
0xdce x18, 0xdcf x72, 0xdd0 x7. Breaks 20 of 21, the groups' out-of-range
case among them: the reading's flag clear shows nowhere (no flag but
hidden is set before a table reads).

### The data panels: MECH_DATA and CHASSIS_DATA (step 8b1)

Both are data tables (above), made with new before the dialog is read and
deleted first in the screen's destructor; the page refresh fills them with
the shown slot's loadout (or zeros without one). Every value goes through
" %s" with its unit, so neither "kph" nor "tons" ever shows.

**MECH_DATA** (0x00456cd0, vtable 0x005c2130, new(0x5ac); shown on every
page): four rows, run speed (0x247), walk speed (0x246), the loadout's
tons (0x25a) and the mech's most (0x25b). The refresh (0x00457330) takes
the most from 0x00425300(8), the speeds from 0x00433050 (rating / most x
10.8, x 1.5 running) and the tons from 0x00425300(-1), in ALTERNATE_FONT
(lab_small_redR) when over the most (at it, the value font). Its reading
(0x004570a0) makes one control besides the table's:

- **ENDOSTEEL** (+0x188, a CycleButton, vtable 0x005c21c0; choices
  Standard 0x2d2 = 0 and 0x25c = 1; the refresh selects the loadout's,
  0x00457130). Its step (+0xa4, 0x00457160): to endo steel needs 14 free
  critical slots (0x00425d80; 7 on a Clan mech), else the message box's
  0xdd1 with both numbers; back to standard the loadout's tons without
  its structure (0x00425300(-1) - 0x00425300(3)) plus a tenth of the
  mech's most must not pass the most, else 0xdd2 with the excess ("%.1f").
  Then the choice steps, the loadout's +0x20 follows it, the loadout is
  marked changed, counted (0x00425bd0) and the page refreshed. Its arrows
  are enabled only on the criticals page while the lab can change things
  (0x0044ca50).
- The dialog's MECH_DATA also has **ARMORTYPE** and **ARMORMASS**
  toggles, laid out like ENDOSTEEL; neither name is in the exe, so they are
  never read or shown. The armour's type and tons change through the item
  list's armour tab (step 5b2).

**CHASSIS_DATA** (0x00457600, vtable 0x005c2270, new(0x188); the chassis
page's): ten rows, the loadout's tons by kind (0x00425300(0..7): engine
0x253, heat sinks 0x25d, jump jets 0x2d6, structure 0x25e, armour 0x25f,
weapons 0x295, ammunition 0x296, equipment 0x297), the total (0x25a, -1)
and the mech's most (0x260, 8). Its place (0x00457e80) leaves half a
SPACING before the total. Its refresh (0x00457940) places it anew first;
it also computes both speeds and drops them.

Ported: `LabMechData`, `LabEndoCycle`, `LabChassisData`, `LabRow::set_float`
(0x00457f90), `engine_kph` (mech_data). Tests: test_lab_screen.py runs
the exe's panels (dumped with their rows and ENDOSTEEL's state; the
engine session steps ENDOSTEEL either way; 0x00425300 answered by kind,
the structure's often at endo's limit): 80/80, 0xdd1 x59, 0xdd2 x6.
Breaks 22 of 22 (the free slots drawn include 6, 7, 12 and 13, so the
need of 14 or 7 shows).

### LOADOUT_LIST and the armour panel (step 8b2)

**LOADOUT_LIST** (0x0044b210, vtable 0x005c0f18; the chassis page's) is a
FileList whose rows are never lit or chosen (+0x90 and +0x94 are
0x0044f4e0, false). The page refresh fills it with the shown loadout
(0x0044b230; empty without one):

- Each weapon kind, 0..36, Clan before Inner Sphere: a probe item
  (0x0044ec90) looked for in the loadout (0x004714c0, the first one like it)
  and counted over all (0x00473510). A weapon that takes ammunition
  (0x00484960) shows message 0x1c5 "%1!2d! %2  (%3!d! rounds)" with its
  ammunition's rounds, counted the same way (0 when it has none aboard);
  the others "%2d %s". The name is the first one's label (vt+0x1c).
- A blank row, then the heat sinks: 0xf4b "%1!d! %2" with the loadout's
  count (+0x68) and "Double Heat Sink" (0x1d0) or "Heat Sink" (0x1cf) by
  +0x6c.
- Each equipment kind, 0..14, Clan before Inner Sphere, "%2d %s" the same
  way, but not armour (1, 2) or heat sinks (9, 10), which have their own
  lines.

**ARMOR_TITLES** (vtable 0x005c0ff0, new(0x31e0), made inline in the
screen's constructor at 0x0044bd7e; the armour tab's) is a Control holding
three kinds of control, each bound from the dialog by name by its read
(0x004510a0) before the panel's own reading:

- **ARMOR_REMOVE** (+0x14c, a push button, vtable 0x005c1080): its second
  text in each state is "<tons> tons" (0x00450bd0: "%.1f", 0xf38; taken
  unchecked). Its press (0x00450cc0; enabled only) clicks, then looks
  for the mech's armour on the loadout (an equipment probe of kind 2 for
  ferro-fibrous, else 1, at the armour's tech +0x30; the first like it,
  0x004714c0). With none there the click is all. Else half a ton goes
  back to the store (its vt+0x0c with 1) and off the loadout (its vt+0x14
  with 1, which sets the tons, 0.5 an item), the sections are spread anew
  (0x004247a0) when more points are on them than the tons now give, and
  the loadout is marked changed (+0x1f), recounted (0x00425bd0) and the
  page redrawn (0x0044cf50).
- **ARMOR_TOGGLE** x 11 (+0x2a4, 0x42c each, 0x0044fab0, CycleButtons,
  vtable 0x005c1968): toggle k is section k (+0x424) with its most's text
  (+0x428). All eleven read the same dialog entry (0x00450840) and are then
  placed: the front column at the panel's x + 0x70 (the head, centre torso,
  left and right torsos, arms, legs: sections 6, 0, 1, 2, 7, 8, 9, 10, the
  panel's texts 19..26 their most), the rear column (3, 4, 5, no text)
  at x + 0xac beside the torsos, rows 15 apart ([0x005e479c]) from y +
  0x91. The three places are computed once and kept for the run
  ([0x00605aac..0x00605ab8], guard bits [0x00605ab4]), so a panel read
  later at another place keeps the first one's toggles. Each toggle's one
  choice is its section's points (0x00450880, "%d"), its text twice the
  internal structure (the head 9). The step (0x004509b0) runs the
  CycleButton's first; from no choice (the first one added) nothing more.
  Down takes a point off (none below 0, 0x00426560). Up, with the most
  (0x004264e0: twice the structure, the head 9) and the other side of a
  torso (front 0, 1, 2 with rear 3, 4, 5; none for the rest): at the most
  with nothing on the other side it stops there, no redraw; under the most
  with points left, a point on; else (at or over the most, or none left)
  one moved over from the other side when it has some, or nothing. Every
  case but the stop marks the loadout changed, recounts and redraws.
- **ARMOR_DISTRIBUTE** (+0x3088, 0x0044fad0, vtable 0x005c1a10): enabled
  while points are left unassigned. Its press (0x00450c70; not checked
  for enabled, as the exe) spreads the sections anew (0x004247a0), marks
  the loadout changed, recounts and redraws.

Its refresh (0x00450e00, with a loadout): REMOVE's tons, its texts 4 (the
points the tons give, 0x00424760) and 6 (those on no section), both
0xf37 "%1!d! pts", and 3 the type ("Clan F.Fibrous" 0xfb1 for armour of
Clan tech, which only ferro-fibrous has, standard's being 3; else "Ferro
Fibrous" 0x1b5 or "Standard" 0x2d2); DISTRIBUTE's enabling; the toggles. Texts 3, 4
and 6 are taken unchecked (the exe crashes without them; the port skips
a missing one). Showing it (0x00450fb0) turns the toggles' arrows and
REMOVE on only while the lab may change things ([0x00605a84] clear);
hiding it (0x00451050) hides all three kinds.

Ported: `LabLoadoutList`, `LabArmorTitles`, `LabArmorRemove`,
`LabArmorToggle`, `LabArmorDistribute` (the presses in step 8b3, below),
`LabState`'s armour places. `zutl::format_message_v` reads a
width in a message's type ("%1!2d!", 0x1c5's; it took the number as a
string before), and `format_message_f` formats one directly. Tests:
test_lab_screen.py runs the exe's LOADOUT_LIST and ARMOR_TITLES (their
fakes and stubs gone; the loadouts given heat sinks and section points,
some bare, and weapons with and without ammunition; an armour session
shows the panel on other slots; the synthetic dialogs move the panel and
cut its texts past 6): lab_screen 80/80 identical (43 seeds with rounds lines, 42 with plain weapon or equipment lines; the armour panel shown and hidden, moved, DISTRIBUTE on and off). Breaks 31 of 31 caught (12 on the loadout list, 19 on the armour panel; armour kinds in a loadout were added to catch the last).

Step 8b3 ported the three presses (above) through new hooks: a section
set (`lab_config_set_section`), the spread (`lab_config_spread_armor`,
`Config::spread_armor`), the loadout's remove (`lab_config_remove`) and
the store's equipment add (`lab_store_add_equipment`). The test stubs
the spread, the remove and the store's add (each recorded, none changing
anything, so the tons stay as they were), gives the loadouts a vtable for
the remove and half of them their armour as an item, and its armour
session sets sections (plan op A: to 0, to the most, or so none are left; the most read from the front toggle's
text) and steps toggles (I) and presses REMOVE and DISTRIBUTE (F). It
covers both ends the lead asked for: a front at 0 stepped down, a rear
and a front at the most with the other side empty and with some, and
REMOVE with no armour on the loadout. lab_screen 80/80 identical. Breaks 28 of 28 caught (13 on REMOVE, 3 on DISTRIBUTE, 12 on the step; a section set so none are left (A mode 3) was added to catch the last two).

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

Ported in item 1 (`mech3/mech_lab.cpp`, `LabState` in app_states.h):
0x00448830, the state's enter, update, leave, ACCEPT's 0x00448870, the
screen's making and deleting (0x00448e70, 0x00448ef0) and the frame
(0x00448f10), the part lookup (0x004507c0, 0x00448e20); emulator-checked
in `test_app_states.py` (the `lab` cases). The calls they make into code
not ported yet are hooks (`Engine::lab_*`). The Ctrl+I read's add to the
list (0x00448c1d..0x00448dfa) is a hook too: the test keeps its read or
its list empty.

Step 2 (`mech3/lab_screen.{h,cpp}`, `test_lab_screen.py`): the screen's
making, destructor, page and slot switches and the press's shell (above).

Not ported: the second saved-loadout
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
5. **The item pages**, in three: 5a the item lists' base (0x00451b40, the
   rows, the arrows; done, above), 5b1 the UNASSIGNED_LIST's shell
   (0x005c1d68: the tabs, sorts, refill; done, above), 5b2 its rules:
   whether an item may go on (0x00453840) and putting it on (0x00454600),
   with the Config helpers they ask.
6. **The criticals page**: ASSIGNED_LIST (0x005c1fe0) and the part
   buttons: placing items in parts' slots.
7. **The weapon groups page**: WEAPONS_UNGROUPED / WEAPONS_GROUPED.
8. **The engine and the data panels**, in three: 8a the tables' base and
   ENGINE_TABLE with its presses (19..22; done, above; the exe has no sink
   presses), 8b1 MECH_DATA with ENDOSTEEL and CHASSIS_DATA (done, above;
   the dialog's armour type and mass toggles are never read), 8b2
   LOADOUT_LIST and ARMOR_TITLES' making, reading, showing and refresh
   (done, above), 8b3 ARMOR_TITLES' presses: REMOVE (0x00450cc0),
   DISTRIBUTE (0x00450c70) and the section step (0x004509b0) (done,
   above).
9. **The 3D view**: the lab world and camera (0x0044ffc0, 0x00450390,
   0x00450690), CURRENT_CHASSIS's model, picking a part (0x004507c0..);
   with the render port's pieces it needs.
10. **MECHLAB from the briefing opens the real lab, and ACCEPT comes back
    with the loadout** (done): the program end to end (a mech and loadout changed
    in the lab reach the launch), mw3tests' campaign loop through the lab.
    The launch reads the slot as it is then (mission-start.md section 1);
    the campaign test picks c1m2's loadout from LOADOUT_LIST, takes a
    weapon off on the criticals page, slows the engine a step and takes a
    point off the centre torso, ACCEPTs and LAUNCHes: the launched mech
    has 5 weapons (6), engine 52 (53), 22.09 m/s (22.50) and 22 points on
    the centre torso (23). Seen live with `--briefing c1m2 --verbose` and
    the same clicks (mw3 dev's work log, Lab 9).
