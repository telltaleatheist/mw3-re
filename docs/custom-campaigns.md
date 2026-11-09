# Custom campaigns: the cut (draft, step 1 of the in-game MODS work)

DRAFT, for the lead's review before any code (modding.md, Planned, "The
in-game MODS screen and custom campaigns", step 1). Stopped 2026-11-08 night
at the facts and a first shape of the file; the open questions at the end
are what the review should settle.

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
| +0xa8 set to 3 after Op 1 Mission 1 and Op 4 Mission 4 (else kept) | 0x00434e00 | traced first (what +0xa8 is) |
| The saved game per mission: `pilots\games\<pilot><op><n>.mw3`, its name "Op %1, Mission %2" (0x21d) | 0x00434950, 0x00434900 | its own folder and names |
| Between missions: the debriefing (Win State), salvage, the briefing's SALVAGE and mech lab, CONTINUE / ABORT; a failed mission played again | campaign.md section 1 | the same screens, unchanged |

Everything per mission that is already data (the world, its scripts, the
AI, the objectives, the briefing text and map, salvage) stays as it is: a
mod ships those files the way the game's missions have them.

## A first shape of the file

`campaign=<file>` in mod.txt names a text file in the mod, read as a
zReader text (.zrd) like the game's other lists, or as plain key=value lines
like mod.txt (to settle). One operation after another, one mission a line:

```
name=Hard Rain
start_stock=default            # or a file of mechs, loadouts and ammo
movie_start=campaign           # optional, by fmv.zrd's name
op=1 title=Operation 1
  mission=hr1 world=c1 number=1 briefing=brief_c11 movie=c1m1
  mission=hr2 world=mymod_w1 number=1
op=2
  ...
movie_end=c4win
```

- `world=` a mission folder (the game's c1..c4b, or a mod's world added by
  `world=` in mod.txt, modding.md New worlds); `number=` its `m<n>` block.
- The between-mission screens are the original's, in its order; nothing in
  the file turns one off in this first cut.
- A single mission is a campaign of one.

## Open questions for the review

1. Text format: mod.txt-style lines (simple, ours) or .zrd (the game's own
   reader, already bounded and fuzzed)?
2. Saves: `pilots\<campaign>\...` beside the original's, or the pilot file
   holding a game per campaign (the pilot screen then lists them)?
3. +0xa8 (set to 3 after Op 1 Mission 1 and Op 4 Mission 4): trace what it
   is before the cut is final.
4. The c4b folder (a second Op 4 world): does any shipped mission choose
   between c4 and c4b by progress, or is it fixed per mission?
5. Pirate's Moon: its own counts and folders (pm-map.md) checked against the
   same table before the crossing.
