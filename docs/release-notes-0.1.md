# MechWarrior 3 rebuild 0.1: first playtest (DRAFT)

Draft for Owen to edit before anything is published. Not tagged, not
released. The zip is built on the PC by `tools\windows\package.cmd`
(docs/windows.md, Packaging); its name carries the `git describe` version.

---

**An unofficial fan rebuild of MechWarrior 3's program, for modern
Windows.** The original game's logic, rewritten in C++ from the 1.2
executable, so it runs on Windows 10 and 11 without DDrawCompat, dgVoodoo
or other helpers. It is a personal learning project: free, not sold, and
not affiliated with or endorsed by Microsoft or MicroProse. MechWarrior is
a trademark of Microsoft.

**No game files are included. You need your own MechWarrior 3**: an
installed copy (1.2 is best; 1.0 plays and can be patched), or the disc or
a disc image.

This is the first playtest build. Expect bugs, and please report them.

## Getting started

1. Download `mw3-<version>-win64.zip` below and unzip it anywhere.
2. Run `mw3.exe`. It finds your MechWarrior 3 install, or asks you to
   point at `Mech3.exe`.
3. Read `README.txt` in the zip for the rest: the 1.2 patch
   (`mw3.exe --patch`), installing from a disc or image
   (`mw3.exe --install-from`), the music from a disc image
   (`mw3import.exe --cue ... --music-only`), and the keys.

Mac and Linux: there is no download for them yet. The rebuild builds and
runs there from the source (see the repository's README), and
`mw3 --install-from <disc, .iso or .cue>` sets up the game's data from
your disc.

## What is in it

- The single-player game with the original's menus, campaign, mech lab,
  briefings and HUD, played by the original's own logic.
- Fullscreen at your desktop's resolution, widescreen included (missions
  use the whole width; menus stay 4:3, centred). Alt+Enter for a window.
- The movies through the Indeo 5 codec in your own install (by a small
  helper, `mw3indeo.exe`), or converted once at the first start.
- Music from your disc's audio tracks, once written out (README.txt).
- Gamepads as on Windows 98, force feedback as rumble.
- **Bug fixes**: the rebuild fixes bugs whose intent is clear, among
  them the mobile field bases that would not come back, lancemates
  walking into walls, AMS, and everything that ran faster on a fast
  computer. The full list is
  [CHANGES.md](../CHANGES.md). To play exactly as the original did:
  `mw3.exe --original`, or `bugfixes=0` in `mw3.cfg`.

## Known gaps

- No multiplayer.
- Pirate's Moon is not supported yet.
- Resizing the window in the middle of a mission may put the HUD out of
  place (not reproduced by us yet; reports with the window size help).

## Reporting bugs

Open an issue: https://github.com/telltaleatheist/mw3-re/issues

Please attach `mw3.log` from `%APPDATA%\MicroProse\MechWarrior 3\` (or
`mw3.old.log` there if you started the game again after the problem), say
what you did and what happened, and whether `--original` does the same.

## Licences

The rebuild's libraries (SDL 3, SDL_ttf, SDL_sound, FFmpeg for the movie
converter, miniz, pl_mpeg) and their licences are listed in
`THIRD-PARTY.txt` in the zip, with their licence files in `licenses\`.

---

## Notes for Owen (not part of the release text)

- The repository's licence and the README's install steps for Mac and
  Linux should be in place before this is public; the text above links
  to them.
- `--install-from` comes in the second batch: this text assumes it is on
  main when the zip is built.
- The GitHub release needs the zip only; the version tag is Owen's call
  (e.g. `v0.1.0`; package.cmd names the zip from `git describe`, so tag
  first, then build).
- The Indeo helper path and the first-start box were checked on the Mac
  only as far as the code allows; the PC run (docs/worklogs/dev2.md,
  the release section) checks them in a fresh folder.
