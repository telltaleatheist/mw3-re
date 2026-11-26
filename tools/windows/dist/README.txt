BattleTech launcher for Windows (64-bit)
========================================

A launcher for fan rebuilds of the BattleTech games: MechWarrior 3 and its
expansion Pirate's Moon, MechWarrior 1 (the rebuild, and the remake on
MechWarrior 3's engine), and The Crescent Hawk's Inception. Each game is the
original's logic rewritten to run natively on today's computers, without
DDrawCompat, dgVoodoo or other helpers. A personal learning project: free,
not sold, and not affiliated with or endorsed by Microsoft, MicroProse or
anyone who holds the BattleTech or MechWarrior names.

The launcher holds no game. It downloads each rebuilt game when you ask for
it, and each game needs your own copy of the original: MechWarrior 3, for
example, on its disc, as a disc image (.iso, or .cue with its .bin files) or
a download of one (.zip, .7z, .rar), or an installed copy (version 1.2 is
best; 1.0 plays). No part of an original game is in any download.

Installing
----------
Run MW3-Rebuild-Setup.exe and start "BattleTech" from the Start menu or the
desktop. (Or, from the zip: unzip this folder anywhere and run
mw3launcher.exe.) The setup asks for the folder every time. Installed over
an earlier version, it removes that package's game programs from the
folder, since the launcher now downloads each game.

Using it
--------
Each game has a panel in the list, its button at the right.
- DOWNLOAD fetches the game from its own release (its size is shown),
  checks it against the release's checksums and installs it. Nothing is
  downloaded until you press it.
- PLAY starts the game. On its first start a game looks for your own copy
  of the original and offers what it finds (INSTALL copies it into the
  game's folder, so your copy can be deleted afterwards), or asks you to
  point to it: the disc, a disc image, a download or an installed copy. It
  remembers it.
- When a newer version of a game is out, its PLAY button gets an arrow at
  its end with the new version: press the arrow to update that game. Your
  pilots, saves, settings and mods are kept.
- The gear beside the button opens the game's page: its GAME FILES (CHANGE
  picks other ones; ADD FROM MY DISC adds the music from a disc), the copy
  PLAY starts, LINK A COPY ALREADY INSTALLED, INSTALL FROM A DOWNLOADED FILE
  (a game's download from anywhere, checked against a SHA256SUMS file beside
  it when there is one) and UNINSTALL.
- CHECK FOR UPDATES asks again for the launcher and every game; when the
  launcher itself has a new version, UPDATE LAUNCHER installs it.
- The launcher stays open while a game runs: the game's button says
  RUNNING until it closes, and other games can be started meanwhile.
- MechWarrior 1 Remake plays from MechWarrior 3's and Pirate's Moon's game
  files, so it needs both set up first.

Each game also starts on its own, without the launcher, and keeps its own
settings, saves and mods. How to play, the mods and each game's own options:
the read-me in that game's folder and its page on GitHub.

Your settings, saves and the games
----------------------------------
The launcher's settings and its log (mw3launcher.log) are in

    %APPDATA%\BattleTech\MechWarrior 3\

with MechWarrior 3's settings, pilots and saves; each other game keeps its
own beside it (Pirate's Moon in ...\BattleTech\Pirate's Moon\). The games it
downloads go to

    %LOCALAPPDATA%\BattleTech\games\

one folder a game. (Paste either into Explorer's address bar.) Uninstalling
the launcher keeps all of them, and installing a new version keeps them too.
Your original games are only read, never changed. To keep your data on
another drive: GAME FILES, MY DATA FOLDER.

Reporting a bug
---------------
Open an issue at https://github.com/telltaleatheist/mw3-re/issues for the
launcher (a game's own problems: that game's repo, linked from its page)
with what you did and what happened, and mw3launcher.log from the folder
above (mw3launcher.old.log too if you started it again since).

Licenses
--------
The libraries this program uses and their licenses are listed in
THIRD-PARTY.txt; their own license files are in licenses\.
