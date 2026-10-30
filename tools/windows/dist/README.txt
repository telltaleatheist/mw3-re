MechWarrior 3 rebuild for Windows (64-bit)
==========================================

What this is
------------
A fan rebuild of MechWarrior 3's program: the original game's logic, rewritten
to run on modern Windows (and macOS) without DDrawCompat, dgVoodoo or other
helpers. It is a personal learning project. It is free, not sold, and not
affiliated with or endorsed by MicroProse, Microsoft or anyone who holds the
MechWarrior name.

It contains no part of the game. It needs your own MechWarrior 3 install,
any version: 1.0 and 1.2 are tested, and the 1.2 patch is best (mw3.exe
prints which data version it found). Pirate's Moon is not supported yet.

Starting
--------
Unzip this folder anywhere and run mw3.exe.

On the first start:
1. It looks for your MechWarrior 3 install: where the original installer put
   it (the registry, then Program Files\MicroProse\MechWarrior 3 and a few
   usual folders such as C:\Games\MechWarrior3). If it cannot find one, it
   asks you to choose Mech3.exe in your MechWarrior 3 folder. The folder it
   finds is remembered in mw3.cfg (see Settings below). You can also start
   it as:  mw3.exe "C:\path\to\MechWarrior 3"
2. The original's movies use a video codec (Indeo 5) that modern Windows no
   longer has. On the first start mw3.exe runs mw3import.exe to convert
   them once, with a small progress window (about a minute). The game runs
   without movies if this is skipped or fails. To run it again:
   mw3.exe --import

No install? Install from the disc
---------------------------------
You do not need the disc's own installer. With the disc in the drive (or a
disc image: a .iso, or a .cue with its .bin files):

    mw3.exe --install-from D:\ --patch "C:\path\to\the 1.2 patch.zip"

copies the game into a folder of its own (in the settings folder below, or
--install-to "C:\Games\MechWarrior 3") and remembers it. --patch is
optional and makes it 1.2. From a .cue, its CD audio becomes the music too.

Music
-----
The original played its music from the CD's audio tracks, which an install
does not contain. If you have a disc image of MechWarrior 3 (a .cue with its
.bin), this writes its music tracks where the game finds them:

    mw3import.exe --cue "C:\path\to\MechWarrior3.cue" --music-only

You can also put your own rip there as music\track02.wav, track03.wav and so
on (or .ogg, .flac, .mp3), numbered as the CD's tracks.

Options
-------
- The game opens fullscreen at your desktop's resolution.
- Alt+Enter switches between fullscreen and a window while you play.
- mw3.exe --windowed starts in a window; windowed=1 in mw3.cfg makes that
  the default.
- mw3.exe --no-import skips the movie conversion.
- mw3.exe --version prints this build's version.

Settings and saves
------------------
Everything the game writes goes to:

    %APPDATA%\MicroProse\MechWarrior 3\

(paste that into Explorer's address bar). It holds mw3.cfg (the install
folder, windowed=), your pilots and saves, the settings, and the converted
movies (video\) and music (music\). Your MechWarrior 3 install is only read,
never changed. Deleting this folder resets everything.

Licences
--------
The libraries this program uses and their licences are listed in
THIRD-PARTY.txt; their own licence files are in licenses\.
