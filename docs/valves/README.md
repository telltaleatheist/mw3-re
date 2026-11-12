# Valve maps

A **valve** is one of a mission's named numbers (most are 0 or 1). The AI's
triggers, the mission's scripts of reactions, the objectives, the AI's paths
and some animations set and read them, which is how a mission hangs together:
a pilot reaches an area, a valve goes to 1, a line of dialogue plays 10 s
later, a path opens, an objective is done. The logic is spread over half a
dozen files. A valve map puts it on one page per mission: for every valve the
mission touches, its starting value, who sets it and who reads it.

Pages so far: [c1m1](c1m1.md), [c1m2](c1m2.md). They are made from the
game's own files by the rebuild (below), never by hand; anyone with the game
can make the rest.

## Reading a page

The top says which files were read and counts each kind of link. The
**Index** lists every valve with its start and how many setters, readers and
actions it has; each name links to its section. A section has:

- **Start**: the value the mission begins with: valves.zrd's `value` for the
  valve, else 0. When it was already different at the mission's first frame
  (a trigger that holds at once, such as the player starting inside a
  region), that value too.
- **Set by**: every place that changes it (below).
- **Read by**: every place that looks at it.
- **When it changes**: what valves.zrd attaches to it: "to 1: plays sound
  ..." fires when the valve becomes 1; "any change" fires on every change.

Names in backticks are the files' own names: records, entities, regions,
sounds, animations, nodes. Objectives are given by their message id
(`MSG_OBJ_0xA00`), never their text.

## Who sets a valve

- **aiv.zrd vlist entries.** Each AI record (and the `player` record, the
  player's own list) can carry a `vlist`: triggers checked every frame. An
  entry whose valve is not yet at its value (item 1, 0 or 1) sets it when
  the first of its checks holds, in this order:

  | Check | Items | Holds when |
  |---|---|---|
  | hit | 4 | the pilot was hit within the last second |
  | heat | 7, 8 | heat is at least item 8 (only mechs have heat) |
  | health | 5, 6 | health is at most item 6 (0 is destroyed) |
  | ammo | 2, 3 | the ammunition left is at most item 3 |
  | near | 13, 14 | the nearest enemy is closer than item 14 |
  | sight | 11, 12 | the nearest enemy is closer than item 12 and in a clear line of sight |
  | region | 9, 10, 17 | inside region item 10 (item 9), outside it (item 17), or anywhere (both) |

  The file gives items 12 and 14 in metres and the game keeps them squared;
  the pages show metres. Item 15 is not read by the trigger. An entry with
  no check on and no region never fires (the pages say "never").
  When an AI pilot on the player's side dies, its region checks pass to the
  player's own list, so "a lancemate reaches the area" also fires when the
  player gets there.
- **targets.zrd `health_below`.** A target (a building, a dish) sets a valve
  to a value when its health falls below a threshold (0: destroyed).
- **valves.zrd.** `delayupdate` sets another valve to this valve's value some
  seconds after this one changes; `update` fires another valve's reactions
  without changing it. A valve with a condition (`any_nonzero`,
  `all_nonzero`, `any_zero`, `all_zero` over a `namelist`) has its value
  computed from the members: it cannot be set, and a member's change fires
  its reactions.
- **AI nets (net_NN.zrd).** `valve_assign` on a node sets the valve when a
  pilot takes that node's links. A `deploy` node sets its first valve while a
  pilot holds there (only while `mcbdeploy` is 1).
- **Objectives (objectives.zrd).** `set_valve` sets a valve when its
  objective is done.
- **Animations (anim.zbd).** A def whose callback events carry the values
  0x2a to 0x31 sets the valve named after its node (0x2a, 0x2b), or after a
  node one, two or three parents up (0x2c to 0x31), to 0 (even values) or 1
  (odd values); in c1m1 the buildings' and bridges' destruction animations
  set their own valves this way.
- **The game's code.** `mcbdeploy` and each mobile field base's own valve
  (named after its node) are set by the field base code. When the field
  base is called to a place, the valve named after that place (`MFB Baker`,
  `MFB 1`) is set to 1 and the last one to 0 (mech_fieldbase_dock.cpp); the
  pages cannot list those, since the place is known only in play, so such a
  valve shows "nothing in the mission's files" under Set by.

## Who reads a valve

- **valves.zrd** conditions (above): the valve is a member of another's
  `namelist`.
- **AI nets**: a `valve` step lets a pilot along a link only while the valve
  holds its value (with no value given: 1 on the node's last link and 0 on
  the others, so the valve switches between them); `mcbwait` holds a pilot
  until a time or the valve; `valveunion` counts valves for the way finder;
  `deploy` holds a pilot until its second valve is at its value.
- **Objectives**: `valve_change` is done when the valve holds its value.

## Making the pages

The pages are made by the rebuild from your own install, as the mission
loads (the same code that plays it), with the original game's 1.2 data:

    mw3 --install <your MechWarrior 3> --hidden --nosound --write-dir <a scratch folder> \
        --mission c1m1 --valve-map docs/valves

`--mission` takes a campaign mission as `c1m1` (c1 to c4 and c4b, then
`m` and the number). The run loads the mission, writes
`docs/valves/c1m1.md` at its first frame and ends. Use a scratch
`--write-dir` so your own settings and logs are left alone. The tool is
`src/upgrades/valve_map/`; `mw3sdltests`'s
`sdl_program_valve_map_c1m1_and_c1m3` checks it and that `c1m1.md` here is
what it makes.
