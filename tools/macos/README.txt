MechWarrior 3 Rebuild for macOS
===============================

A fan rebuild of the programs of MechWarrior 3 and its expansion Pirate's
Moon: the original games' logic, rewritten to run natively on today's
computers. The app opens a launcher to pick the game. A personal learning project: free, not
sold, and not affiliated with or endorsed by MicroProse, Microsoft or anyone
who holds the MechWarrior name.

It contains no part of the game: you need your own MechWarrior 3, on its disc,
as a disc image (.iso, or .cue with its .bin files) or a download of one
(.zip, .7z, .rar), or an installed copy from a Windows PC. Pirate's Moon
needs your own Pirate's Moon the same way; each game works without the other.

Installing
----------
It runs on macOS 12 (Monterey) or later, on Apple silicon and Intel Macs.
Drag "MechWarrior 3" into Applications and start it.

The first time, macOS says it cannot check the app for malicious software
(it is a free fan project, not registered with Apple). Open it once this way:
right-click (or Control-click) the app, choose Open, then Open again. Or, after
macOS has blocked it once: System Settings > Privacy & Security, and click
"Open Anyway" beside its name. After that it starts normally.

On the first start it looks for your installed copy and goes straight into
the game when it finds one. If not, its setup screen's POINT TO YOUR GAME
takes whatever you have: the disc, a disc image (.iso, or .cue with its .bin
files, whose CD audio becomes the music), the file you downloaded (.zip, .7z
or .rar, even a zip inside a zip; nothing to unpack by hand), or a copy from
a Windows PC. It says what it found, installs it with a progress bar and
Cancel, remembers it, and goes on into the game.
Then it converts the game's movies once (about 3 seconds each, with a
progress bar; closing it stops, and the next start does the rest) into the
folder below, so the intro and the mission movies play.

Mods
----
A start from the launcher or the main menu always plays the original game.
Mods are turned on inside the game, per campaign:

1. MODS on the main menu. Campaigns are at the left (the game's own first),
   your mods at the right.
2. IMPORT adds a mod: pick the file you downloaded.
3. Click a mod's box to turn it on for the campaign chosen at the left.
4. PLAY starts that campaign with its ticked mods. It has its own pilots
   and saved games; your main-menu pilots are never changed.

Importing a community mod (the weapons mods, map packs): download it. Its
installer is a Windows program made for the original game: do not run it
anywhere. Choose MODS, IMPORT and pick the downloaded file itself (the
installer .exe, a .zip, .7z or .rar, or a file inside an unpacked folder):
the rebuild reads the mod out of it without running it. Then click its box
for a campaign and PLAY.

If IMPORT says "N warnings", the mod has files the game cannot use as they
are; the rest is added, and INFO lists each warning. Every button says what
it does when the mouse is over it. Pirate's Moon has its own MODS screen and
takes only mods made for Pirate's Moon. In multiplayer every player needs
the same mods.

Your settings and saves
-----------------------
In ~/Library/Application Support/MicroProse/MechWarrior 3/ (in Finder: Go >
Go to Folder). It holds mw3.cfg, your pilots and saves, the settings, the
installed game, its music, the converted movies, your mods and mw3.log (what
the game reported while it ran; send it with a bug report). Pirate's Moon
keeps its own in the folder "Pirate's Moon" inside it. Deleting the app
keeps this folder. To keep it on
another drive: in the launcher, GAME FILES, MY DATA FOLDER.

Licences
--------
The libraries this app uses and their licences: THIRD-PARTY.txt inside the
app (Contents/Resources), with their licence files in Contents/Resources/licenses.
