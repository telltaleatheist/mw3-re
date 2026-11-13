# Weapon ranges

Every weapon's maximum range in metres, read straight from the games' own
data (`weapons.zrd` in `zbd/reader.zbd`, the `RANGE` of each weapon). MW3 is
the 1.2 data; Pirate's Moon is its 1.0 data. A dash means the game does not
have the weapon.

| Weapon | MW3 | Pirate's Moon |
|---|---:|---:|
| **Lasers** | | |
| Small Laser | 120 | 120 |
| Medium Laser | 360 | 360 |
| Large Laser | 600 | 600 |
| ER Micro Laser | — | 160 |
| ER Small Laser | 240 | 240 |
| ER Medium Laser | 600 | 600 |
| ER Large Laser | 1000 | 1000 |
| Small Pulse Laser | 240 | 240 |
| Medium Pulse Laser | 480 | 480 |
| Large Pulse Laser | 800 | 800 |
| Small X-Pulse Laser | — | 300 |
| Medium X-Pulse Laser | — | 600 |
| Large X-Pulse Laser | — | 1000 |
| Small Heavy Laser | — | 120 |
| Medium Heavy Laser | — | 360 |
| Large Heavy Laser | — | 600 |
| **PPCs and flamer** | | |
| PPC | 720 | 720 |
| ER PPC | 920 | 920 |
| Flamer | 75 | 75 |
| **Ballistic** | | |
| Machine Gun | 120 | 120 |
| Light Machine Gun | — | 240 |
| Heavy Machine Gun | — | 120 |
| AC/2 | 720 | 720 |
| AC/5 | 600 | 600 |
| AC/10 | 450 | 450 |
| AC/20 | 270 | **210** |
| Ultra AC/2 | 810 | 810 |
| Ultra AC/5 | 630 | 630 |
| Ultra AC/10 | 540 | 540 |
| Ultra AC/20 | 360 | **260** |
| LB 2-X AC | 900 | 900 |
| LB 5-X AC | 720 | 720 |
| LB 10-X AC | 550 | 550 |
| LB 20-X AC | 360 | **290** |
| Gauss Rifle | 880 | 880 |
| Light Gauss Rifle | — | 1000 |
| **Missiles** | | |
| SRM 2 / 4 / 6 | 360 | 360 |
| Streak SRM 2 / 4 / 6 | 480 | 480 |
| MRM 10 / 20 / 30 / 40 | — | 570 |
| LRM 5 / 10 / 15 / 20 | 800 | 800 |
| Thunderbolt 5 / 10 / 15 / 20 | — | 900 |
| NARC | 1000 | 1000 |
| TAG | 600 | 600 |

Bold: Pirate's Moon shortened it. Pirate's Moon changes no other range
(it does change some heat, damage and speeds: the PPC runs cooler, the
medium lasers too, PPC bolts fly twice as fast, LB-X pellets hit a bit
harder).

## How to read it

- **LB-X autocannons fire a shell that bursts at 30 m** into pellets, and
  each pellet then flies its own range: 870, 690, 520 and 330 m (Pirate's
  Moon's LB 20-X 260). The game adds the two together, so the table shows
  30 + the pellet's range. That sum is the range the game itself uses
  for the weapon.
- **Lasers lose damage with distance:** full damage up close, 75% at
  maximum range. **Under water a laser reaches half as far.**
- **Missiles that miss just vanish** at their maximum range.
- **TAG** itself reaches 600 m; the airstrike it calls (four missiles
  coming in from 1500 m back) reaches 2000 m.
- Not on the list: AMS (150 m, measured flat on the map), and the mission
  props that use the weapon system (mines, a nuke, a gas tank, turrets).

## The zoom window's range

The zoom window shows `[range = ...m]` for whatever is under the
reticle. Aimed at the sky (or at anything past the view distance) it
shows the camera's view distance instead, which is **800 m in every
mission** (the world's far clip, set once in `world.gw`), whatever
weapons you have. 800 m is also the range of every LRM and the Large
Pulse Laser, so in a mech carrying those the sky reading matches the
weapon. The reticle also stops checking for targets at 800 m, so it
stays green over a mech further away even when a weapon above reaches
it (`docs/discoveries.md`, "The reticle stops looking at 800 m").

**Pirate's Moon changed this.** Its reticle looks out to the farther of
the 800 m view distance and the selected weapon group's range, and aimed
at the sky the zoom window reads that distance. So with an ER Large
Laser or a NARC selected it reads 1000 m, with an ER PPC 920 m and with a
Gauss rifle 880 m, but any group whose reach is under 800 m still
reads 800 (PM's reticle, 0x00445cf0). The MW3 rebuild with bug fixes on
does the same: the reticle finds mechs out to the weapons' range, and aimed
at the sky the zoom window reads the farther of 800 m and the selected
group's range, as Pirate's Moon. With bug fixes off it reads 800 m, as the
original MW3.
