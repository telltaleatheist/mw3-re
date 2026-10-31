# The 1.2 patch's data, file by file

What the official 1.2 patch changes in the game's data, against a 1.0
install, and whether the rebuild can redo each change itself when it loads
1.0 data. The rule (Owen): small values, record edits and our own message
wording are ours to write; the patch's own bytes and whole new files are not
copied. Plan stage (2026-10-31): nothing is built yet.

The patch (`data1.cab`, InstallShield) holds 31 files: `Mech3.exe`, two
read-mes, and the 28 data files below. The exe needs nothing: the rebuild is
ported from 1.2's.

Scripts used (scratch, to go into `tools/analysis/` with the layer):
reader archives decoded entry by entry and compared by path; interp.zbd's
scripts compared one by one; the message tables; force_eff.ifr's records;
c3's world compared by node name and model content with the file's saved
pointers masked; c3's anim with 1.0's 0xcd fill, pointers and float noise
masked.

## Summary

| File | 1.0 -> 1.2 bytes | What changed | Kind | Redo at load? |
|---|---|---|---|---|
| `Mech3Msg.dll` | 120,320 -> 123,392 | 37 new messages: 22 target names (the c2/c3/c4b targets moved from fixed names to `MSG_` ids), 3 resolution labels, the jump-jet limit, "rounds", `DefaultCtlConfig`, 3 multiplayer version/world texts, Alt/Shift/Button 1/9/10; 4 edited (a `\n` fix, two chat labels, "%1 Team"); message 4 dropped | text | yes: our own wording in a small table |
| `force_eff.ifr` | 2,936 -> 4,113 | jets2 phase 90° -> 0; water damper softer; "impact fall" reworked (longer, stronger, an attack and fade); new: "impact land" (1.0's old landing bump under its new name), "dead spring", "dead dance" (two shakes) | gameplay feel | yes: 8 records of small values |
| `zbd/reader.zbd` | 2,143,369 -> 2,146,707 | below | | |
| -- weapons.zrd | 77,078 -> 77,006 | 66 values: laser heat up; PPC/ER PPC speed 650 -> 900; LB-X burst damage down, impact up, fire rate 1 -> 0.5, wider burst; autocannons retuned; LRM/SRM/Streak damage x1.25 with more impact; LRM15/20 heat down | balance | yes: a value table |
| -- 17 `dfn_*.zrd` | small | torso twist limits (mostly 80 -> 100/110, Strider 90 -> 75, the lights 5 -> 10/15); single -> double heat sinks in the Cauldron, Vulture, Puma and Supernova loadouts; Puma's loadouts (ammo moved, a medium pulse for a large); training2 Bushwacker armour cut to near nothing; a Vulture skin/damage texture fix | balance, bug fixes | yes: record edits |
| -- hud.zrd | 2,648 -> 5,284 | blocks for 1152x864, 1280x1024, 1600x1200 | layout | yes: every value is 1.0's 1024x768 block moved with its edge (right, bottom or centre), checked on all 34 |
| -- dialog.zrd | 54,819 -> 55,555 | multiplayer host screen: chat up/down arrows added, the chat list moved 14 px | layout (multiplayer) | yes: record edits (the arrows' images are 1.0's) |
| -- briefing.zrd | 1 value | the salvage label's offset (10,10) -> (10,2) | layout | yes |
| -- sounds.zrd | 133,406 -> 133,600 | footsteps and soil sounds point at the new 16-bit wavs, the engine at its trimmed one, room 4 at `env_room04_p2.wav` (which neither version has); a playback-format key | sound | yes, with the wavs below |
| -- mechsoilfx.zrd | 7,319 -> 7,143 | the soil puffs lose GRAVITY (4 motions) | effect | yes |
| `zbd/interp.zbd` | 99,662 both | no script changed: the archive rebuilt (new dates, new order) | none | nothing to do |
| 9 `zbd/*_2s.wav` | new, 110-124 KB each | the footstep and soil sounds as 16-bit; 1.0's are the same sounds at 8 bits (within the rounding; water exactly) | sound quality | the sounds: yes (1.0's own); the extra bits: no |
| `zbd/mech_engine_np.wav` | new, 107,850 | the engine loop, 16-bit, with its last 176 samples cut: 1.0's loop ends on a ramp to near full scale, a click every time it loops ("np", no pop) | bug fix | yes: trim 1.0's own sound |
| `zbd/c1/reader.zbd` | 771,601 -> 774,352 | mission 1's secondary objective (destroy the Owens and two APCs) removed; new `envlights.zrd`, `intro.zrd` (animation sources; c1's compiled anim.zbd is unchanged, so likely unused) | bug fix; unused | objective: yes; the two sources: not needed |
| `zbd/c2/reader.zbd` | 758,993 -> 759,298 | mission 2's jump-jet ceiling -15 -> -90; the multiplayer start calls two more animations (lights, the minepit turret); new 8-byte `mobilerepair.zrd` | bug fixes | yes |
| `zbd/c2/readerm3.zbd` | 68,026 -> 73,391 | the boundary valves use new region pairs (`reg_inside2/outside2`, already in 1.0's archive); two target names to `MSG_`; new `net_14.zrd` (292 lines, a new path net) | bug fix; new content | edits: yes; net_14: no |
| `zbd/c3/readerm2..m6` | +-30 bytes each | target names to `MSG_` ids; m3's two boundary regions' points moved (12 values) | text, bug fix | yes |
| `zbd/c3/readermp3.zbd`, `readermp4.zbd` | new, 7,262 and 6,662 | two new multiplayer maps on the c3 world (paths, boundaries, map info) | new content | no (multiplayer is not ported yet) |
| `zbd/c3/gamez.zbd` | 12,999,820 -> 12,994,852 | the world re-exported: 153 of the 187 models that differ have the same faces stored in another order; 34 really change (277 faces out, 239 in): the dome complex's walls re-cut by about 1 m (gaps closed), a 6.3 m wall along one edge of all 7 helipads removed, three terrain corners raised 4.5 m and a map-edge tile given its missing point (seams you could see through), 6 small objects closed with 4 faces each, a damaged part's floating top removed, a feature on `base04` moved 11 m; also one texture (`cpad07`) dropped, a node renamed, a soil type fixed (`dome12` 9 -> 0) | level fixes | removals, moves, names, soil: yes; the 239 new faces (a few KB): Owen's call |
| `zbd/c3/anim.zbd` | 3,879,480 both | re-saved only: saved pointers, uninitialised padding, floats off by an ulp or -0 -> 1e-8 | none | nothing to do |
| `zbd/c4b/readerm4.zbd` | 34,205 -> 41,618 | five path nets the mission's orders use were missing in 1.0 (net_02, 05-08, 6.7 KB); one patrol point moved; a target name | bug fix (AI paths) | point and name: yes; the nets: no (see below) |
| `zbd/c4b/readerm5.zbd` | 26,025 -> 50,984 | 1.0 shipped mission 5 with only six nets and c4's boundary; 1.2 adds 22 files: 11 are byte for byte 1.0's own c4b m6/m4 files, 5 more are those with 1-4 lines changed; 6 are new (net_20, reg_p2, the c4bm5 boundary pair); targets get `MSG_` names | bug fix (AI paths) | 16 of 22: yes, from the player's own archives (+ small edits); 6: no |

## What cannot be redone, by the rule

- **c3's new faces**: 239 faces in 34 models (the dome walls re-cut, terrain
  seams closed, six small objects closed). What a 1.0 player sees: cracks in
  the dome's walls and at three terrain corners, a 6.3 m wall along one side
  of each helipad, a part floating over a wreck. The removals and moves can
  be done as edits; the new faces are a few KB of their geometry.
- **New path nets and regions** (c2 m3 net_14; c4b m4's five nets; c4b m5's
  six): authored node lists. The small ones are 4-10 points each (400-800
  bytes); whether such a list counts as "small values" is Owen's call. Without
  them those missions' AI on 1.0 data behaves as in 1.0 (what an order
  naming a missing net does there is still to check).
- **The two c3 multiplayer maps**: new content, and multiplayer is not ported.
- **16-bit sound**: the rebuild plays 1.0's 8-bit footsteps instead.

## How the layer would work (step 2)

`mech3/patch12_*`: applied while the data loads when the install's version is
1.0 or a mix (the launcher's version check), not behind `bugs`. Reader edits
as (archive, entry, path, old, new) records checked against the old value, so
a changed install is left alone; entries copied between the player's own
archives by name and checksum; the HUD blocks made by the anchoring rule; the
message table extended in code; the force-feedback records added; the engine
loop trimmed. The test: each patched file on 1.0 data parses equal to 1.2's,
and the list above is what does not.

## Found on the way (for discoveries.md)

- The engine loop clicks in 1.0: its last 176 samples ramp to near full
  scale; 1.2 cut them.
- Room 4's sound (`env_room04.wav`) is empty in both versions, and 1.2 points
  it at a file that does not exist.
- 1.0's c4b mission 5 shipped with c4's boundary and six of its nets; 1.2
  copied in mission 6's.
- 1.2 dropped c1 mission 1's secondary objective (destroy the Owens and the
  two APCs).
