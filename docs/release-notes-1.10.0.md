# Launcher 1.10.0 (2026-11-27)

A launcher release, with new versions of the games beside it: MechWarrior 3
1.7.0, Pirate's Moon 1.3.0 and MechWarrior 1 Remake 1.1.5. After updating the
launcher, press UPDATE ALL. (Launcher 1.9.0 was not released; its changes are
here too.)

## Downloading the original games

- When you press DOWNLOAD and a game finds no copy of its own (an install, a
  disc, a disc image, a folder you pointed it at), the launcher now fetches
  the game's original files, checks them against their checksum and sets
  them up, music included: MechWarrior 3 and Pirate's Moon (the Internet
  Archive's copies of the US discs), MechWarrior 1 for the Rebuild and the
  Remake, and The Crescent Hawk's Inception.
- If you already have the game, nothing extra is downloaded. To use your own
  copy instead, press the gear, then GAME FILES > CHANGE.
- A MechWarrior 3 or Pirate's Moon disc in a drive is found and used first,
  so nothing is downloaded that your disc already has.
- When the Internet Archive cannot be reached, MechWarrior 3 and Pirate's
  Moon fall back to a compact copy of their original files on
  reverse-engineered.com (310 MB and 167 MB). The game checks every file
  before installing it; the result is the same as a disc install.
- A dropped download picks up where it stopped instead of starting over: it
  is tried again 5 seconds later, up to five times while each try gets
  further.

## One progress bar

- Each press of DOWNLOAD, UPDATE or UPDATE ALL, and the launcher's own update,
  shows one bar and one line for everything it fetches: the game, its
  original files and their setup. For example: "MechWarrior 3: 287 of 668 MB ·
  4.1 MB/s · about 2 min left", with a smaller line naming the part under way.
- "waiting for the server..." shows when no data has arrived for 10 seconds.
- CANCEL keeps what is already installed.

## The games list

- The list has headings: BUILT ON MECHWARRIOR 3 (MechWarrior 3, Pirate's Moon,
  the MechWarrior 1 Remake) and THE ORIGINAL DOS GAMES (MechWarrior 1,
  Inception and the games still coming).
- A newer games list is put in use as soon as the launcher fetches it, with no
  restart. The launcher fetches it at start, on CHECK FOR UPDATES, after an
  install or update, and every 4 hours, and waits while an install is running.
- Offline, only your installed games are listed. With none installed, the
  list says to check your connection or firewall and press CHECK FOR UPDATES.

## Fixed

- The launcher could crash as it quit after the computer's sound output
  changed while it was open (headphones or a headset plugged in or out), a
  1.8.0 report. Its menu sounds now share one audio device, and it uses SDL
  3.4.20, which fixes the crash.
- Its log is written as it goes, so a crash keeps every line.

## A settings file

- The launcher's timings and a few preferences are in
  `launcher-settings.json`, which comes with it: how long it waits for each
  server and program, when a slow download is given up, how often the games
  list is fetched, the window's size at start, vsync, and the menu sounds'
  volume. Each is listed at its default with its range. To change one, copy
  the file into the launcher's data folder (where `mw3launcher.log` is) and
  edit the copy: a value out of range is held to the nearest end, anything
  else wrong falls back to the default, and the log says which. Nothing in
  it can change what the launcher trusts (where it downloads from, the
  checksums or signatures).

## Upgrading

Press UPDATE AVAILABLE in your launcher, or install over it, then UPDATE ALL.
Your games, saves and settings stay as they are.
