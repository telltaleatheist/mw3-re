**A crash fix, a safer weapon limit, and walls that match the original more closely.** Install it over any earlier 1.0.x: your pilots, saves and settings are kept.

## Fixed

- **No crash ending a mission with a laser firing.** If a beam was still firing when a mission ended, the cleanup treated it as a shot and could crash, for example on REPLAY at the debriefing. A player found it; earlier releases have it too.
- **A loadout past 16 weapons can't damage the game.** The game took a 17th weapon unchecked and overwrote what came after the list. No normal loadout gets there, since the mech lab stops at 16, but a mod's mech could, and so could a modified player's loadout in multiplayer. The extra weapon is now refused and named in `mw3.log`.
- **Walls and footing match the original more closely.** Mech collision and the feet's ground probes now do their sums at the precision the original game really runs at. In our tests they now agree with the original exactly. In play the difference is rare and small: in walking runs at walls we measured no changed decisions at all, so you're unlikely to notice anything.

- **Windows:** this package passed its checks, but we didn't play a mission from it before release. If a mission won't start for you, please send `mw3.log`.

Everything the rebuild changes from the original is in `CHANGES.md`.

## Still known

- **macOS: window tools can delay the mouse button by about a second**
  (Magnet and similar). Quit the tool while you play, or exclude
  MechWarrior 3 in it.
- **Jump jets lighting up water (Op 3, Mission 5):** one report, not
  reproduced. A screenshot and `mw3.log` help.
- **Linux: stuck at "Downloading Mission Parameters":** one report, not reproduced. Since 1.0.4 the log names the loading step that went quiet, so `mw3.log` from such a run is what we need.
- Mech speed in shallow water and on hills is the original's: speed has no
  slope term, and water only slows a mech deeper than a tenth of its height.
- Pirate's Moon is not supported yet.

## Downloads

| | Download | |
|---|---|---|
| Windows | `MW3-Rebuild-Setup-v1.0.5.exe` | An installer (a `.zip` of the same files is there too). |
| Mac | `MW3-Rebuild-v1.0.5-mac.dmg` | Drag the app to Applications. |
| Linux | `MW3-Rebuild-v1.0.5-x86_64.AppImage` | Make it executable and run it. Built and checked under WSL, not playtested on a Linux machine. |

You need your own MechWarrior 3 (an installed copy, the disc, or a disc
image); see the 1.0 release for getting started.

## Reporting bugs

Open an issue on the project's GitHub page with `mw3.log` from the
settings folder (the README says where it is).

## Checksums

```
5f00ce95701d4cc7b424110fa544bc3dc20d8e8c3d331aea3cb54177461688fc  MW3-Rebuild-Setup-v1.0.5.exe
66b566e8ce72d7522f19f4b642f274d83c7d2df6e717401d8fa029593886e2f5  mw3-v1.0.5-win64.zip
6c20996a7ddf7095726961bf2436515192b175ef80853f75964bc99e843c6bcd  MW3-Rebuild-v1.0.5-mac.dmg
e3f97033596fb1e19e5e5eb7fc5ec586583f273363e86f47dce7db17c9c585b6  MW3-Rebuild-v1.0.5-x86_64.AppImage
```
