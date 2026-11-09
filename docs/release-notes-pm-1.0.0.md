**Pirate's Moon, rebuilt: the first release.** The MechWarrior 3 expansion running natively on Windows, Mac and Linux, with its own AI, its own campaigns and its own saves, beside the MechWarrior 3 rebuild.

## What's in it

- **Both campaigns**: Inner Sphere (the CAMPAIGN button) and Pirate Missions, plus Instant Action with Pirate's Moon's own waves, training and multiplayer.
- **Pirate's Moon's own AI**, ported from the expansion's code, not MechWarrior 3's: its lancemates, its pathing, its repair orders.
- **Everything MechWarrior 3's rebuild has**: any resolution and 16:9, wide menus, the Upgrades page (bug fixes on or off), cheat codes, mods, and your saves in the settings folder.
- **Its own settings and saves**, apart from MechWarrior 3's: `%APPDATA%\MicroProse\Pirate's Moon` on Windows, `~/Library/Application Support/MicroProse/Pirate's Moon` on the Mac.

## What you need

- **Your own installed Pirate's Moon** (the folder with `Mech3.exe` and `zbd`). The first start looks for it and, if it finds none, asks you to pick its `Mech3.exe`. Your Pirate's Moon files are only read, never changed.
- **Music**: Pirate's Moon plays its music from the disc's audio track, which an install doesn't have. Rip track 2 of your disc into the settings folder's `music` folder as `track02.ogg` (or `.flac`, `.wav`).
- **Clear speech and sounds**: some installs have an empty `zbd\soundsH.zbd` (the high-quality sounds), so the game falls back to the low-quality set and speech sounds muffled. Copy `soundsH.zbd` from your disc into the settings folder's `zbd` folder; the rebuild reads it from there first.
- A graphics card that does Direct3D 12 (Windows 10 or 11, 64-bit), Metal (macOS 12 or later) or Vulkan (Linux x86-64).

## Fixed during the playtest

- **Lancemates repair when ordered.** Pirate's Moon's own repair routine wasn't ported, so a lancemate took the order and stood still. With bug fixes on, the starbase also takes lancemates after your own refit there, which the original refuses.
- **No invisible walls after shooting down aircraft or turrets.** A destroyed turret left a large solid box over the space its wreck fell through (an original quirk, fixed behind bug fixes). The wreck itself still blocks you.
- **Typing in the cheat box or chat no longer moves your view** (L, the hold-to-look key, typed last).
- **The mouse moves at your desktop's speed** at any screen size (it was 3 to 4 times too fast on large screens), in menus and in missions. MOUSE SENSITIVITY still adjusts it.
- **The options screen has its UPGRADES page and MOUSE SENSITIVITY slider** in Pirate's Moon too (Pirate's Moon's options file is laid out differently, so they were missing).

## Still known

- **Linux:** built and checked under WSL, not playtested on a Linux machine.
- Report anything else with `mw3.log` from the settings folder.

## Downloads

| | Download | |
|---|---|---|
| Windows | `PM-Rebuild-Setup-pm-v1.0.0.exe` | An installer (a `.zip` of the same files is there too). |
| Mac | `PM-Rebuild-pm-v1.0.0-mac.dmg` | Drag the app to Applications. |
| Linux | `PM-Rebuild-pm-v1.0.0-x86_64.AppImage` | Make it executable and run it. |

Everything the rebuild changes from the original is in `CHANGES.md`.

## Reporting bugs

Open an issue on the project's GitHub page with `mw3.log` from the settings folder.
