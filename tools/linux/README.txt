MechWarrior 3 Rebuild for Linux (x86-64)
========================================

A fan rebuild of the programs of MechWarrior 3 and its expansion Pirate's
Moon: the original games' logic, rewritten to run natively on today's
computers. A personal learning project: free, not sold, and not affiliated
with or endorsed by MicroProse, Microsoft or anyone who holds the
MechWarrior name.

It contains no part of either game: you need your own MechWarrior 3, on its
disc, as a disc image (.iso, or .cue with its .bin files) or a download of
one (.zip, .7z, .rar), or an installed copy (a Wine, Proton or Lutris copy
works). Pirate's Moon needs your own Pirate's Moon the same way; each game
works without the other.

Running it
----------
Make the AppImage executable (chmod +x) and run it. It needs your
distribution's Vulkan driver. It opens a launcher: click MECHWARRIOR 3 or
PIRATE'S MOON. On the first start it looks for your installed copy; if it
finds none, FIND THE GAME takes whatever you have (the disc, a disc image, a
download, or Mech3.exe in an installed copy), installs it into a folder of
its own and goes on into the game. Then it converts the game's movies once.
GAME FILES finds or installs either game later and adds music from the disc;
UPDATES looks for a newer release.

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
(not under Wine either; it would overwrite that game's files). Choose MODS,
IMPORT and pick the downloaded file itself (the installer .exe, a .zip, .7z
or .rar, or a file inside an unpacked folder): the rebuild reads the mod out
of it without running it. Then click its box for a campaign and PLAY.

If IMPORT says "N warnings", the mod has files the game cannot use as they
are; the rest is added, and INFO lists each warning. Every button says what
it does when the mouse is over it. Pirate's Moon has its own MODS screen and
takes only mods made for Pirate's Moon. In multiplayer every player needs
the same mods.

Your settings and saves
-----------------------
In ~/.local/share/MicroProse/MechWarrior 3/ : mw3.cfg, your pilots and
saves, the settings, the installed game, its music, the converted movies,
your mods and mw3.log (what the game reported while it ran; send it with a
bug report). Pirate's Moon keeps its own in the folder "Pirate's Moon"
inside it. Deleting the AppImage keeps this folder, and a new version keeps
your pilots, saves, settings and mods.

Reporting a bug
---------------
Open an issue at https://github.com/telltaleatheist/mw3-re/issues with what
you did and what happened, the mission, and mw3.log from the folder above.

Licences
--------
The libraries this program uses and their licences are listed in
THIRD-PARTY.txt beside this file; their own licence files are in licenses/.
