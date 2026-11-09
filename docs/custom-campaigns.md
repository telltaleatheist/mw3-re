# Custom campaigns: the cut (step 1 of the in-game MODS work)

For the lead's review before any code (modding.md, Planned, "The in-game
MODS screen and custom campaigns", step 1): what the original campaign takes
from code, the file that replaces it, and the five questions answered from
the exe (2026-11-09) or, for the two design choices, a recommendation.

## What the original campaign takes from code, not data

From campaign.md, mission-load.md and pilot.md (Mech3.exe 1.2):

| What | Where | A custom campaign needs |
|---|---|---|
| The operations' sizes: Ops 1 and 2 four missions, 3 and 4 six | 0x00484840 | its own list of missions |
| The advance: next mission, past the op's count the next op, Op 5 the end | 0x00484870 | the same walk over its list |
| The mission's folder: c1 c2 c3 c4 c4b t1 by index | 0x00484c50 | a folder per mission (the game's or the mod's) |
| The mission in its world: world.zrd's block `m<n>`, the start animation `start_single_player_m<n>` | 0x00490590, 0x0050b450 | the mission number inside the folder |
| The briefing: briefing.zrd's `brief_<dir><n>` (else "default") | 0x00405230 | the same name, or one given |
| The movies: fmv.zrd "campaign" before Op 1 Mission 1, "c<op>m<n>" before a briefing, "c4win" after the last | 0x004099c0, Load state, Win Movie State | movies by name, each optional |
| The new campaign's stock: two Bushwackers with "Primary", ammo for LRM-5, MG, AC/10 | 0x004345a0, 0x004733b0 | a starting stock |
| The field bases (+0xa8): 3 at each operation's first mission and at Op 4 Mission 4, else carried; one lost each time an MFB dies | 0x00434e00, 0x004590b0 | a count, and where it resets |
| The mission folder by position: Op 4 Missions 4..6 in c4b | 0x00435350 | `world=` per mission |
| A failed mission replayed from a new world's start (Op N Mission 1, Op 4 Mission 4) unloads the world whole | 0x0047d030 | follows from the world changing |
| The saved game per mission: `pilots\games\<pilot><op><n>.mw3`, its name "Op %1, Mission %2" (0x21d) | 0x00434950, 0x00434900 | its own folder and names |
| Between missions: the debriefing (Win State), salvage, the briefing's SALVAGE and mech lab, CONTINUE / ABORT; a failed mission played again | campaign.md section 1 | the same screens, unchanged |

Everything per mission that is already data (the world, its scripts, the
AI, the objectives, the briefing text and map, salvage) stays as it is: a
mod ships those files the way the game's missions have them.

## A first shape of the file

`campaign=<file>` in mod.txt names a text file in the mod, plain
`key=value` lines as mod.txt (question 1), one mission a line:

```
name=Hard Rain
stock=default                  # the original's new-campaign stock, or a file: mechs, loadouts, ammo
movie_start=campaign           # optional, fmv.zrd's name
op=Operation 1 bases=3         # a heading (shown as the save's name), field bases reset to 3
mission=c1 1 brief_c11 c1m1    # world, its m<n>, briefing (else brief_<world><n>), movie (optional)
mission=mymod_w1 1
op=Operation 2 bases=3
mission=c4 4
mission=c4b 4 bases=3          # a reset inside an operation (the original's Op 4 Mission 4)
movie_end=c4win
```

- A flat list of missions in play order; `op=` lines are headings and the
  points where the field bases reset, not a count the exe's way (PM has no
  operations at all: one list of 10, its world from the mission's number,
  0x00435de0; its Pirate Missions another list, `sa<n>`). The advance is the
  next line; past the last, `movie_end` and the end as the original's c4win.
- `world=` a mission folder (the game's c1..c4b, or a mod's world added by
  `world=` in mod.txt, modding.md New worlds); the number is its `m<n>`
  block and start animation, as now.
- A mission whose world differs from the one before is a new world's start:
  replayed after a failure, it reloads the world whole (as the original at
  Op N Mission 1 and Op 4 Mission 4).
- The between-mission screens are the original's, unchanged: the Win State
  and debriefing, salvage, the briefing's SALVAGE and mech lab, a failure
  replaying the mission. Nothing in the file turns one off in this cut.
- A single mission is a campaign of one line.

## The five questions

1. **Format (design): mod.txt-style lines, recommended.** One `key=value` a
   line, `#` comments, as mod.txt and its `mech=`/`weapon=` rows: a modder
   already writes those, our parser names the line it refuses (the
   campaign's whole list checked at mount and by `--check-mod`, as rows
   are), and the list is flat, so nothing needs .zrd's nesting. The game
   never read campaign order from a .zrd (it is code, above), so .zrd buys
   no original reader, only its bounds, which a line reader has by nature.
2. **Saves (design): the campaign's own pilots folder, recommended:**
   `pilots\mods\<mod>\` with the original's layout inside (pilot files,
   `games\<pilot><op><n>.mw3`, their `.rows` sidecars). The pilot file's
   format stays the exe's, the original's pilots never hold a mod's
   numbers, entering the campaign shows its own pilots (Owen: "its own,
   beside the original's"), and removing a mod's pilots is one folder. The
   save's name comes from the `op=` heading and the mission's place, not
   message 0x21d's "Op %1, Mission %2" (the place can still be its number).
3. **+0xa8 is the field bases left** (the Mobile Field Bases): 3 at a new
   campaign, set to 3 again by the next mission's game at each operation's
   first mission and at Op 4 Mission 4 (0x00434e00), otherwise carried; one
   taken by the field-base cut when an MFB dies (0x004590b0: what the player
   keeps); read by the briefing and the MFB screens (0x00479440). Hence
   `bases=` on `op=` and on a mission line.
4. **c4b is fixed by position:** 0x00435350 (the campaign's vt+0x54) gives
   c1, c2, c3 by operation and, in Op 4, c4 for Missions 1..3 and c4b for
   4..6. Op 4 Mission 4 is where the world changes, the field bases reset
   and a replay unloads whole (0x0047d030): one rule, the world's start.
   Nothing chooses by progress. c4b's missions keep the operation's
   numbers (its anim.zbd starts `start_single_player_m5` and `_m6`; Mission
   4 takes the start animation's fallback, mission-load.md), so a mission
   line's number is the world's own block, whatever its place in the list.
5. **Pirate's Moon** drives its campaign differently (above): no
   operations, one numbered list, the world from the number, and a second
   campaign. The flat list fits it as is; the crossing maps `bases=` and
   the save names to PM's (its message 0x21d is "Mission %1", fixed on pm,
   pirates-moon.md).

## Decided (the lead's review, 2026-11-09)

The format and the pilots' folder stand, with three additions:

1. A mod may hold more than one campaign (one `campaign=` line each); each
   one's pilots go in `pilots\mods\<mod>\<campaign>\`, its id the file's
   name, so a mod's campaigns never share pilots.
2. Our code never deletes that folder on its own: removing it is the
   player's choice, behind a confirm, and only for a folder with our marker.
3. A mission line naming a world no enabled mod or the install has is
   refused at mount, naming the line, and the whole campaign is left out.

The file as built is in modding.md, Custom campaigns (the shape above with
`-` for a default briefing, and no comments after a value).

## Next, after the review

The parser and its checks (mod.txt's `campaign=`, `--check-mod`), the
campaign game type behind the pilot screen with its own folder, the advance
over the list, then the MODS screen's CUSTOM CAMPAIGNS table (step 2).
