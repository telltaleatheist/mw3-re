# The cockpit sounds

Mech3.exe 1.2, playtest #17 (mac 2, 2026-10-21). The player's cockpit has
30 sounds of its own, played by index: the fire modes, the hit callouts,
ammo, coolant, the gyro, heat, MASC, the lock tones.

## The table (0x004141a0)

The mechs' set-up at mission load (0x00413e60, from the setup 0x00490ab0)
finds each by name in zSound (0x0056f6f0) into [0x00605598 + 4i]:

| i | name | i | name | i | name |
|---|---|---|---|---|---|
| 0 | mech_missilelock | 10 | mech_chit_lleg | 20 | single_fire_engaged |
| 1 | damaged | 11 | mech_chit_rleg | 21 | group_fire_engaged |
| 2 | destroyed | 12 | mech_acquirelock | 22 | linked_fire_engaged |
| 3 | mech_chit_main | 13 | mech_low_ammo | 23 | mech_friendly_fire |
| 4 | mech_chit_head | 14 | mech_no_ammo | 24 | snd_gyro_overload |
| 5 | mech_chit_ltorso | 15 | ammo_explosion | 25 | snd_gyro_ok |
| 6 | mech_chit_torso | 16 | ammo_jettison | 26 | mech_heat_critical |
| 7 | mech_chit_rtorso | 17 | mech_flush_coolant | 27 | masc_engaged |
| 8 | mech_chit_larm | 18 | coolant_depleted | 28 | tc_online |
| 9 | mech_chit_rarm | 19 | ecm_engaged | 29 | mech_traitor |

## Playing them

- **0x0041e770** (ECX the index; a volume, and a second argument it does
  not read): 0x004e6d80 plays the sound with no position (where the
  listener is), and the voice is kept in [0x00605520 + 4i]. Every caller
  passes 1.0.
- **0x0041e7a0** (ECX the index, EDX fade): a slot with no sound does
  nothing; with fade, 0x0041e7e0(1, 0.0) (a fade to silence that stops
  it); without, the voice is stopped (0x0056e9f0) and forgotten.
- **0x0041e7e0**: the voice handed to zSound's fade 0x00571d20, and what it
  gives back kept.
- **0x0041e800**: the voice.

The callers: the fire modes (0x004b0830: linked 22, single 20, group 21),
jettison (16), the knock-down's gyro (24), MASC (27), coolant (17), heat
critical (26), the ammo explosion (15, two callers), no ammo (14, two), a
part's critical hit (3, `mech_chit_main`; 0x00460d50), the HUD's lock tones
(12, 0) and its channels (0x0041e7a0, 0x0041e800). Nothing in 1.2 plays
the others: damaged, destroyed, the per-part hit callouts (4..11), low
ammo, coolant depleted, ECM engaged, friendly fire, gyro OK, targeting
computer online, traitor. The mech's death stops
all 30 (0x0041e7a0(i, 0)).

## The port

`mech3/cockpit_sound.{h,cpp}`. The program loads the table in the
Missions hook `mechs_init` (0x00413e60; the rest of that function, three
network-dependent globals, stays logged as unported). The weapons' and
the mech's `play_mech_sound`, the death's stop and the HUD's three
sound hooks use it. Before, every one of them was unwired, so the
cockpit was silent: \ changed the fire mode with no voice saying which
(playtest #17), and the gyro, MASC, coolant and lock tones were silent
too.

## Checks

- `tests/differential/test_cockpit_sound.py`: 200 random sequences of the
  load (with names missing), plays, stops (with and without the fade),
  fades and voice reads. zSound's four calls are stubbed alike on both
  sides; the table, the voices and the calls must match.
- `mission_c1m1_default_keys_fire_headless`: \ plays the new mode's sound
  object from the table. zSound is closed headless, so the play is
  recorded rather than heard.
