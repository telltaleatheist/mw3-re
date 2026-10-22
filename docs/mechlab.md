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

Left to 5b2: +0xa8 (0x00453840, whether an item may go on) and +0xac
(0x00454600, putting it on), "lab.vt+0xa8" / "lab.vt+0xac".

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

Not ported: the pages' contents (steps 3..8); the second saved-loadout
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
8. **The engine, sinks and armor** (ENGINE_TABLE, ids 14..30's engine,
   sink and armor presses) and the data panels (MECH_DATA, CHASSIS_DATA,
   ARMOR_TITLES, LOADOUT_LIST).
9. **The 3D view**: the lab world and camera (0x0044ffc0, 0x00450390,
   0x00450690), CURRENT_CHASSIS's model, picking a part (0x004507c0..);
   with the render port's pieces it needs.
10. **MECHLAB from the briefing opens the real lab, and ACCEPT comes back
    with the loadout**: the program end to end (a mech and loadout changed
    in the lab reach the launch), mw3tests' campaign loop through the lab.
