# MechWarrior 3 Rebuild 1.1.1

**Pirate's Moon plays more like Pirate's Moon.** Where 1.1.0 still used MechWarrior 3's rules, the expansion's own are now in: salvage between multiplayer rounds, field base repairs, which weapons can bear, weapon lights, group fire, and how the AI fires, takes lance orders and reacts when it hits an ally. MechWarrior 3 gets a few pieces of the original that weren't working yet and two crash fixes. Install it over 1.1.0 or any 1.0.x: your pilots, saves, settings and mods are kept.

## Downloads

The same packages as 1.1.0. The combined one holds the launcher, MechWarrior 3 and Pirate's Moon, and it's the one the launcher's UPDATES button finds.

| | Both games and the launcher | MechWarrior 3 alone | Pirate's Moon alone |
|---|---|---|---|
| Windows | `MW3-Rebuild-Setup-v1.1.1.exe` (or `MW3-Rebuild-v1.1.1-win64.zip`) | `MW3-alone-Setup-v1.1.1.exe` (or `MW3-alone-v1.1.1-win64.zip`) | `PM-Rebuild-Setup-pm-v1.1.1.exe` (or `mw3pm-pm-v1.1.1-win64.zip`) |
| Mac | `MW3-Rebuild-v1.1.1-mac.dmg` | `MW3-alone-v1.1.1-mac.dmg` | `PM-Rebuild-pm-v1.1.1-mac.dmg` |
| Linux | `MW3-Rebuild-v1.1.1-x86_64.AppImage` | `MW3-alone-v1.1.1-x86_64.AppImage` | `PM-Rebuild-pm-v1.1.1-x86_64.AppImage` |

You still need your own games: none of the original games' files are in these downloads. The [1.1.0 notes](release-notes-1.1.0.md) say how the launcher sets them up.

## What's new

### Pirate's Moon

Each of these was read in the expansion's program and checked against it in an emulator.

- **Salvage between multiplayer rounds.** The salvage screen gives you three field bases' worth of mech bays (six mechs) and, for weapons and parts, the bonus tons your places in the rounds earned. You can't hand your last mech back to the pile. In the campaign it stays as before.
- **Field bases.** In Instant Action, Pirate Missions and multiplayer a repair refills every ammo bin for free. Everywhere the armour comes back to full (in the campaign from your store when you have some for your mech, otherwise free). A mech whose limp is repaired gets its full speed back at once.
- **Which weapons can bear.** Each time your weapons are switched on, one whose line to the reticle point is beyond its mount's reach is switched off and stays silent, instead of firing along its arm.
- **Weapon lights.** Lasers, PPCs, autocannons and gauss rifles light up their surroundings when they fire, as the expansion's do. On low Object Detail the flashes, lights and impact animations are left out, as in the expansion.
- **Group fire.** The best beam fires first, and each kind of weapon shows its flash and light once per volley.
- **The AI fires its missiles when they're reloaded**, without MechWarrior 3's wait for the last volley to fly.
- **Lance orders as the expansion has them.** A lancemate busy with something it can't leave says "disregard". An attack order makes it keep shooting that target first. The command mode's Attack takes only a real enemy. An attack or defend order on something without a vehicle behind it is answered "disregard" instead of crashing the game.
- **An AI mech that hits an ally keeps fighting**: it picks a target again straight away and passes over anything standing behind that ally (MechWarrior 3's gives up its shot and rests).
- **A lancemate lets go of a target that is gone**, where it held on to it before.

### MechWarrior 3

These change MechWarrior 3 and Pirate's Moon alike.

- **The radar's view wedge on the mission map and your mech's heading line**, which weren't drawn yet.
- **The ER PPC's static on your HUD** when it hits you.
- **Your mech's voice callouts** for destroyed equipment and engine hits.
- **When the camera leaves your mech** your mech and its HUD are paused, and they resume when it comes back, as in the original.
- **The zoom window aimed at the sky** reads the farther of 800 m and your selected group's range (1000 m with ER large lasers), as Pirate's Moon does; the original always read 800 m. One of the small fixes on the UPGRADES page.
- **3D-card mode builds the mechs as triangle strips when they load**, as the original's 3D-card mode did. The shapes are the same; a shot meets overlapping faces in the original's order.

## Fixed

- A laser turret still firing when a mission ends no longer leaves its beam behind for the next mission on the same map (it could crash on CONTINUE).
- The triangle strips no longer read past a mech's own list of faces (harmless in the original, skipped here).

## Still known

As in [1.1.0](release-notes-1.1.0.md#still-known).

## Reporting bugs

Open an issue on the project's GitHub page and attach `mw3.log` from the game's folder (each game has its own). If you started the game again since, send `mw3.old.log`.

## Checksums

Each package's SHA256 is in `SHA256SUMS` on the release, which the launcher's UPDATES also checks.
