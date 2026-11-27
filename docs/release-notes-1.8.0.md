# Launcher 1.8.0 (2026-11-26)

A launcher release. The games are unchanged.

## Where the launcher looks, and signed data

- Every address the launcher uses now comes from data, not the program. A
  small file that ships with the launcher (`sources.json`) says where to find
  the games list: reverse-engineered.com first, then GitHub, then the backup
  copy. The games list says everything else: each game's download homes,
  in order, and where else to find the list itself. Moving the downloads to
  another server is now an edit to that list, with no launcher update.
- The games list and every release's checksum file are signed. The launcher
  takes a list or a download only when its signature checks against the key
  it carries, so a copy from any server is safe to use, and a changed one is
  refused. Releases from before signing still install from GitHub.

## The original game's files

- A game's entry in the list can name where its original files may be
  downloaded. When it does, pressing DOWNLOAD installs the game and then
  fetches and sets up its files too, checked against their checksum. No game
  names a source today, so nothing changes yet: you still bring your own copy.

## Also

- A game you have installed or linked stays on your list even if a later list
  leaves it out.
- The README's music steps: with a disc image that kept the CD audio (a .cue
  with its .bin files), GAME FILES > ADD FROM MY DISC brings the music in.

## Upgrading

Press UPDATE AVAILABLE in your launcher, or install over it. Your games stay
as they are.
