# Game types (the "mission mode" in heat.md)

Mech3.exe 1.2, 2026-10-19 (researched by a subagent, spot-checked: 0x48a7b0, 0x48b170, 0x49cc3f). Answers the heat.md question "mission mode 3: which mode?".

## Summary

`[0x60861c]` is not a mission. It is the **pilot** (player profile) object.
Its field +0x6c points to the pilot's **current game**, one of five objects the
pilot owns. Vtable +0x2c of that game returns its type:

| value | game type | class ctor | size | vtable | +0x2c fn | held in pilot at |
|---|---|---|---|---|---|---|
| -1 | base class, never selected | (base) | | 0x5beca0 | 0x4a5260 | |
| 0 | Mech Lab, and the "Dev 1/2" test games | 0x4373e0 | 0x84 | 0x5bee08 | 0x40bae0 | +0x7c |
| 1 | Campaign | 0x4345a0 (this, 1, 1) | 200 | 0x5bec40 | 0x499270 | vector +0x60..+0x68 (last element) |
| 2 | Instant Action | 0x436650 | 0x17c | 0x5bed50 | 0x4993e0 | +0x70 |
| 3 | **Multiplayer** | 0x4353a0 | 0xb0 | 0x5becf8 | 0x41ec30 | +0x74 |
| 4 | Training | 0x436e30 | 0x8c | 0x5beda8 | 0x473a20 | +0x78 |

So mission mode 3 in the heat code (0x419bbc, 0x419dc9) is **multiplayer**:
heat is always on in multiplayer and critical heat does not destroy the torso there.

## Evidence

### The pilot object

- 0x478a40 (fastcall: ECX = campaign, EDX = mission) looks up a pilot named
  `"DefaultPlayer"` (0x5e596c; "Default Player" is the startup's, 0x5e5efc) through 0x478be0. If none exists it allocates
  0x84 bytes and runs the constructor 0x477b20, writes the pilot file
  (`WriteFile` of +0x4, +0x8, +0xc), and adds it to the pilot list at
  `[0x60860c]` (count at `[0x608610]`). Then it calls 0x478250(1) and writes the
  campaign number to `[+0x6c]+0xa4` and the mission number to `[+0x6c]+0xa0`.
- Pilot constructor 0x477b20: vtable 0x5c37a0, name copied to +0x18, then
  - `new(200)` 0x4345a0(this, 1, 1), pushed onto the vector at +0x60/+0x64/+0x68
    and also stored in +0x6c;
  - `new(0x84)` 0x4373e0 -> +0x7c;
  - `new(0x17c)` 0x436650 -> +0x70;
  - `new(0xb0)` 0x4353a0 -> +0x74;
  - `new(0x8c)` 0x436e30 -> +0x78.
- 0x478250(n) (thiscall on the pilot) selects the current game:
  0 -> +0x7c, 1 -> the last element of the vector at +0x60 (the current
  campaign), 2 -> +0x70, 3 -> +0x74, 4 -> +0x78, each into +0x6c.
  The selector number equals the value vtable +0x2c returns for the class it
  selects.
- The ctors set the base vtable 0x5beca0 first (whose +0x2c, 0x4a5260, is
  `or eax,-1`), then their own: 0x4345a0 -> 0x5bec40 (0x43468c),
  0x4373e0 -> 0x5bee08 (0x437468), 0x436650 -> 0x5bed50 (0x4366e4),
  0x4353a0 -> 0x5becf8 (0x4354b3), 0x436e30 -> 0x5beda8 (0x436ec1).

### Naming each type: who selects what

The front-end screens (constructor 0x48eb10 takes the state name) call
0x478250 when the player confirms:

- **Action State** (`"Action State"` 0x5e7214, ctor 0x48a6c0, vtable 0x5c5b10):
  0x48a750 and the handler 0x48a7b0 call 0x478250(2). -> 2 = Instant Action.
- **Training State** (`"Training State"` 0x5e7240, ctor 0x48b120, vtable
  0x5c5b70): handler 0x48b170 calls 0x478250(4). -> 4 = Training.
- **Multiplayer State** (`"Multiplayer State"` 0x5e7cc4, ctor 0x49c900,
  vtable 0x5c7fe8): handler 0x49cbb0 calls 0x47fc40(1), creates the pilot via
  0x4788b0 after network calls 0x586d10 / 0x588a60 if none exists, then
  0x478250(3). -> 3 = Multiplayer.
- **Campaign State** (`"Campaign State"` 0x5e7224, ctor 0x48aef0, vtable
  0x5c5b40): its handler 0x48b000 does not call 0x478250; it uses whatever the
  pilot already has. 1 is selected when a pilot is created or loaded
  (0x478ba7 at the end of 0x478a40) and by the debug campaign entries. ->
  1 = Campaign (the pilot's current campaign save).
- Startup 0x480b20 selects 0 at 0x480b43 right after getting the pilot
  (0x4788b0), so 0 is also the default before any game is chosen.

The debug window menu confirms all of this. The exe's MENU resources 147 and
149 have a `&Game` menu; its MFC message map (WM_COMMAND 0x111 entries at
0x5c4630..0x5c4960) sends each item to a small handler in 0x483720..0x4841b0:

| menu item (id) | handler | what it does |
|---|---|---|
| Campaign 1, Mission 1..4 (0x9c95..0x9c98) | 0x483720, 0x483770, 0x4837c0, 0x483810 | 0x478a40(ECX=1, EDX=1..4): campaign mode 1 |
| Campaign 2, Mission 1..4 (0x9c99..0x9c9c) | 0x483860..0x483960 | 0x478a40(2, 1..4) (0x4838b0 also calls 0x478250(1) itself) |
| Campaign 3, Mission 1..6 (0x9c9d..0x9ca2) | 0x4839b0..0x483b90 | 0x478a40(3, 1..6) |
| Campaign 4, Mission 1..6 (0x9ca3..0x9ca8) | 0x483bf0..0x483dc0 | 0x478a40(4, 1..6) |
| Training 1, Mission 1..4 (0x9ca9..0x9cac) | 0x483e80, 0x483ed0, 0x483f20, 0x483f70 | 0x478250(4), then `[0x4794a0()+0x40]` = 1..4 (training mission number) |
| Development, Dev 1 / Dev 2 (0x9c89, 0x9c8a) | 0x483fc0, 0x484040 | 0x478250(0), then game vtable +0x50(7 / 8) |
| Mech Lab (0x9cb0) | 0x4840c0 | 0x478250(0), game vtable +0x50(6), then jumps to 0x47ff60 |
| Multi-Player (0x9cb1) | 0x484190 | 0x483100, then 0x47fc40(1) (same call the Multiplayer State makes) |
| Instant Action (0x9cb3) | 0x4841b0 | 0x483100, then 0x47fc20(1, 0) |

- The mode-0 class's vtable +0x50 is 0x4379e0, which stores its argument at
  +0x38: likely the world/level to load (6 for the Mech Lab, 7 and 8 for the
  two dev test levels). Not checked further.
- The campaign game stores campaign at +0xa4 and mission at +0xa0 (0x478a40).
  The campaign numbers match MW3's four operations (4, 4, 6, 6 missions).
- Each debug campaign entry also calls 0x4836e0(pilot, id, id, ..., -1) with a
  list that grows with progress (C1M1: {3}; C1M2/3: {3,0xb}; C1M4:
  {3,8,0xe,0xb}; later up to {3,0xd,0xe,0xa,0xb,2,0xc}). For each id it
  allocates via 0x576040(0xf20) and passes it to pilot 0x479430(id, ...) then
  0x4733b0. Likely it gives the pilot the lancemates or mechs they would have by
  that mission; not confirmed.

### The other "constant" vtables in heat.md

0x5bed04, 0x5bee14 and 0x5bedd0 are not vtable starts. They are slots inside the
vtables above: 0x5bed04 = 0x5becf8 + 0xc, 0x5bee14 = 0x5bee08 + 0xc, 0x5bedd0 =
0x5beda8 + 0x28. Their "+0x2c" entries are really other slots (0x5becf8 +0x38,
0x5bee08 +0x38, 0x5beda8 +0x54) whose functions (0x4a5270 -> 1, 0x58a7d0 -> 3,
0x437960 -> 5) happen to return constants. They are not game types. (0x58a7d0
lies in znet_dplay.cpp's range and is likely one tiny folded function shared by
several classes.)

## Where the game type is read

Every `[[0x60861c]+0x6c]` vtable +0x2c call, with the compare after it (found by
a scan of mech3.asm):

| call site | function | test |
|---|---|---|
| 0x419bbc, 0x419dc9 | 0x4192f0 (heat) | == 3 (multiplayer) |
| 0x41b44d | 0x41b420 | == 1 (campaign) |
| 0x446b58, 0x446d55 | 0x446ad0 | switch |
| 0x458f74 | 0x458f50 | == 3 |
| 0x45a04c | 0x45a030 | == 1 |
| 0x45a285 | 0x45a270 | == 3 |
| 0x45c43b | 0x45c340 | == 1 |
| 0x47ac97, 0x47b711, 0x47d073, 0x47d199, 0x47da06 | mission.cpp area | various |
| 0x481f2e | 0x481e70 | == 3 |
| 0x48f7d1, 0x49059a, 0x490772, 0x490b29, 0x49146e | mission.cpp | switches |
| 0x491bfa | 0x491bf0 | == 4 (training) |
| 0x492a38, 0x492a4a | 0x492750 | == 1, == 2 |
| 0x492d93 | 0x492d70 | == 3 |
| 0x49dcae | 0x49dc80 (player.cpp) | == 2 |
| 0x49e920 | 0x49e810 (player.cpp) | == 3, == 2 |
| 0x4a11f4 | 0x4a1140 | switch |
| 0x4cbf30 | 0x4cbeb0 (AI net in/out) | == 4 |
| 0x4d6d5e | 0x4d6d20 (zaitarget/zrdrainet) | == 3, == 2 |

## Suggested names (for docs/symbols.csv later)

- `[0x60861c]` g_pilot (current pilot profile); pilot +0x6c current_game,
  +0x60 campaigns vector, +0x70 instant_action, +0x74 multiplayer,
  +0x78 training, +0x7c lab_game.
- 0x477b20 Pilot::Pilot(name); 0x478250 Pilot::SelectGame(type);
  0x478a40 debug StartCampaignMission(campaign, mission) on "DefaultPlayer" (pilot.md).
- vtable +0x2c Game::GetType(); enum GameType { None=-1, MechLab=0,
  Campaign=1, InstantAction=2, Multiplayer=3, Training=4 }.
- 0x4345a0 CampaignGame, 0x436650 InstantActionGame, 0x4353a0 MultiplayerGame,
  0x436e30 TrainingGame, 0x4373e0 LabGame (name of type 0 is the weakest: it is
  what the Mech Lab and the dev levels use, and the startup default).

## Open

- Type 0's real name (Mech Lab only, or a general "free/test" game).
- What 0x4836e0's id lists add to the pilot.
- The heat.md note "heat on = bit 3 of `[0x5f4d04]` or mode 3": the debug
  menu has a "Heat Tracking" option (0x9c93) with no handler in the release
  message map; likely related to that bit, not checked.
