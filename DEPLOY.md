# The repos, how they connect, and how a release is deployed

This file is the same in every repo of the family. Edit the copy in
[mech3-re](https://github.com/telltaleatheist/mech3-re/blob/main/DEPLOY.md), then
copy it to the others.

## The repos

| Repo | What it is | Its tags | Its Releases page holds |
|---|---|---|---|
| [mw3-re](https://github.com/telltaleatheist/mw3-re) | **The hub.** Where the project started; its code is frozen since 2026-11-21 | `vX.Y.Z` (the package's version) | **The download**: the combined package for every platform (the launcher and all three games), each game alone, and `SHA256SUMS`. The launcher's updates come from here |
| [mech3-re](https://github.com/telltaleatheist/mech3-re) | MechWarrior 3 and the launcher (named BattleTech), and the packaging scripts that build the download | `vX.Y.Z` | MechWarrior 3 alone |
| [mech3-pm-re](https://github.com/telltaleatheist/mech3-pm-re) | Pirate's Moon: a complete copy of the game with Pirate's Moon's changes in place | `pm-vX.Y.Z` | Pirate's Moon alone |
| [mech1-remake-re](https://github.com/telltaleatheist/mech1-remake-re) | MechWarrior 1 Remake: MechWarrior (1989)'s campaign on this engine | `mw1-vX.Y.Z` | The combined package it plays from, and its notes |
| [re-setup-kit](https://github.com/telltaleatheist/re-setup-kit) | The shared "find your game files" kit: installed copies, discs, images and archives on Mac, Windows and Linux | none (pinned by commit) | nothing |

Rebuilds on other engines, not part of the download (yet):
[mech1-re](https://github.com/telltaleatheist/mech1-re) (MechWarrior (1989) as our own C++),
[inception-re](https://github.com/telltaleatheist/inception-re) and
[inception-remake-re](https://github.com/telltaleatheist/inception-remake-re)
(BattleTech: The Crescent Hawk's Inception). They share the setup kit's ideas
and the `-re` name, not this code. Each has its own releases.

## How they connect

```
mw3-re  (the original repo; frozen code, the download hub)
  └─ mech3-re          its full history continues here: MechWarrior 3 + the launcher
       └─ mech3-pm-re       a fork: the whole game with Pirate's Moon's changes in place
            └─ mech1-remake-re   a fork of Pirate's Moon: MW1's campaign on the same engine

re-setup-kit  (a git submodule at extern/re-setup-kit in all three game repos)
```

- **mw3-re is the base of all of them.** mech3-re was pushed with mw3-re's
  whole history, mech3-pm-re from its `pm` branch and mech1-remake-re from
  its `mw1` branch, so every game repo shares one history and old clones and
  links keep working. Pull requests opened on mw3-re are merged into the game
  repos.
- **Fixes flow down by merge**: mech3-re's main is merged into mech3-pm-re,
  and mech3-pm-re's into mech1-remake-re. Nothing merges back up: Pirate's
  Moon's improvements never reach MechWarrior 3, which stays as it shipped
  plus bug fixes. A bug found downstream that MechWarrior 3 also has is fixed
  on mech3-re (cherry-picked or redone) and comes down again by merge.
- **The setup kit** is pinned by commit in each game repo. A kit change
  reaches a game only when that repo moves its submodule, and all three
  should pin the same commit for a release.
- **Ports keep their source's address** (`// Mech3.exe 1.2 @ 0x...`, and on
  Pirate's Moon `// Pirate's Moon 1.0 @ 0x...`). Our own features live in
  `src/upgrades/`.

## What a player downloads

One package per platform, from mw3-re's Releases:

| Platform | The combined package (the one updates take) | MechWarrior 3 alone | Pirate's Moon alone |
|---|---|---|---|
| Windows | `MW3-Rebuild-Setup-<v>.exe` (or `-win64.zip`) | `MW3-alone-Setup-<v>.exe` | `PM-Rebuild-Setup-<pm v>.exe` |
| Mac | `MW3-Rebuild-<v>-mac.dmg` | `MW3-alone-<v>-mac.dmg` | `PM-Rebuild-<pm v>-mac.dmg` |
| Linux | `MW3-Rebuild-<v>-x86_64.AppImage` | `MW3-alone-<v>-x86_64.AppImage` | `PM-Rebuild-<pm v>-x86_64.AppImage` |

The combined package is four programs side by side, each built from its own
repo:

| Program | Built from | Started by |
|---|---|---|
| `mw3launcher` (the icon, named BattleTech) | mech3-re, tag `vX.Y.Z` | the player |
| `mw3` | mech3-re, the same tag | the launcher's MECHWARRIOR 3, or the player directly |
| `mw3pm` | mech3-pm-re, tag `pm-vX.Y.Z` | the launcher's PIRATE'S MOON, or the player directly |
| `mw1` and its `mw1data/` | mech1-remake-re, tag `mw1-vX.Y.Z` | the launcher's MECHWARRIOR 1 |

- **The launcher pulls the games in when the package is built, not at run
  time.** The packaging script checks out each repo's release tag, builds
  each program from its own repo, and puts them beside the launcher. The
  launcher finds each one in its own folder (`Contents/MacOS` in the Mac app,
  `usr/bin` in the AppImage) and starts it as a separate program. A game
  whose program isn't there shows greyed ("Coming later"), as in MW3 alone.
- **Each program carries its own version**, read from its repo's tag when it
  was built (`git describe`), stamped into the file and read back by the
  launcher without running it. The launcher's footer shows the package's
  version (mech3-re's tag), and each game's line shows its own ("official,
  program pm-v1.2.2").
- **No game files are ever in a package.** Each game sets itself up from the
  player's own copy through the setup kit (`mw3 --status` / `--setup`, the
  find screen) and copies what it needs into its own settings folder. (MW1
  for now takes its installs from the launcher's finds or its `mw1.cfg`; its
  own setup recipe comes later.)
- Each game also starts on its own, without the launcher, and keeps its own
  settings, saves and mods.

## How updates work

The launcher's UPDATES (src/upgrades/update, src/frontend/sdl/sdl_update.cpp):

1. Asks GitHub for the releases of **mw3-re** and then **mech3-re** (`curl`,
   HTTPS only). One not answering is fine.
2. Takes the highest bare `vX.Y.Z` tag that is not a draft or a prerelease
   **and carries this platform's combined package** (`MW3-Rebuild-...`). A
   game's own release (mech3-re's MW3 alone, any `pm-v` or `mw1-v` tag) is
   never taken. Copies from 1.2.2 and earlier don't check for the package:
   they take the highest tag, mw3-re first, and on a tie they take mw3-re's.
3. If it's newer than its own version, shows the release's notes and offers
   DOWNLOAD AND INSTALL.
4. Downloads the package and the release's `SHA256SUMS`, and installs only if
   the package's SHA-256 matches its line. A missing line, a mismatch or a cut
   download installs nothing.
5. Installs it each platform's normal way: Windows runs the new Setup over
   the install; the Mac copies the new app beside the old one and swaps it
   in; Linux writes the new AppImage beside the old one and swaps it in. The
   old copy is removed only once the new one is in place. Settings, pilots,
   saves and mods are never touched.

So the combined package, its name, its `SHA256SUMS` and the hub's Releases
page are a contract with every copy already installed. Don't rename the
assets, move the download off mw3-re, or publish without `SHA256SUMS`.

## Deploying a release

Releases are cut by the lead session when the project owner clears them. The full
checklist is mech3-re's [docs/release.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/release.md);
this is the shape of it.

**Setup:** one clone of mech3-re with a remote for each repo (mech3-re,
mech3-pm-re, mech1-remake-re, mw3-re), so the packaging script can check
out all three tags. On a machine with no remotes (the Windows PC), bring the
tags in a `git bundle`.

1. **Get each repo ready.** Each game repo's main has the fixes merged down
   (mech3-re into mech3-pm-re into mech1-remake-re), the same setup kit
   commit, its tests for what changed passing, `CHANGES.md`, and its release
   notes. mech3-re also gets the hub's release page
   (`docs/release-page-<v>.md`) and notes (`docs/release-notes-<v>.md`).
2. **Choose the versions.** Features raise the second number, fixes alone the
   third (1.3.9 is followed by 1.3.10). The package's version is mech3-re's tag; Pirate's Moon and MW1
   keep their own numbers.
3. **Tag locally, don't push yet:** `vX.Y.Z` on mech3-re, `pm-vX.Y.Z` on
   mech3-pm-re, `mw1-vX.Y.Z` on mech1-remake-re.
4. **Build every platform from those tags** (mech3-re's `tools/package`):
   - Mac: `tools/package/combined.sh mac <v> <pm v> --deps --mw1 <mw1 v>`
   - Windows (the PC): `tools\package\combined.cmd <v> <pm v> <mw1 v>`
   - Linux (WSL on the PC, after Windows):
     `tools/package/combined.sh linux <v> <pm v> build-deps/linux/prefix ../deps --mw1 <mw1 v>`

   Each builds the three packages in `build-pkg/dist/<platform>/` and runs its
   checks: every library present, the licences, no game files, no local path
   or user name, and no version ending in `-dirty`. If a platform fails, fix
   it on the repo's main, move the local tag, and rebuild every platform from
   the new tags. Tags are pushed only once all three platforms build.
5. **Gather and sum:** all twelve files in one folder, each checked against
   its platform's `SHA256SUMS`, then `python3 -I tools/package/sums.py <folder>`
   writes the release's `SHA256SUMS`.
6. **Push the tags** to their repos (`vX.Y.Z` to mech3-re and mw3-re,
   `pm-vX.Y.Z` to mech3-pm-re, `mw1-vX.Y.Z` to mech1-remake-re).
7. **Publish the hub first:** the `vX.Y.Z` release on mw3-re with all twelve
   files and `SHA256SUMS`, the page from `docs/release-page-<v>.md`, not a
   draft or a prerelease. It goes first because installed copies check the
   game repos too: a game's own release up before the hub's would be seen by
   1.2.2 copies with no package to download.
8. **Then each game's own release:** MW3 alone on mech3-re (`vX.Y.Z`),
   Pirate's Moon alone on mech3-pm-re (`pm-vX.Y.Z`), and on mech1-remake-re
   (`mw1-vX.Y.Z`) the combined package MW1 plays from, each with its files and
   their lines of `SHA256SUMS`, and a link to the hub for the full package. Every game
   release's version must match a bundle on the hub.
9. **Check it live:** download every asset again and check its checksum, then
   run the previous release's launcher, press UPDATES, and confirm it finds
   the new one on the hub.

After a release: the game repos' mains already sit on their tags. Issue
replies announcing it are the project owner's to approve.

## Rules that keep it working

- The hub's Releases page stays the download and the update source. Don't
  archive mw3-re (archiving locks its releases), and never create another
  repo named mw3-re.
- Every release carries `SHA256SUMS`; the updater installs nothing without
  its line.
- The combined package's file names are fixed (`MW3-Rebuild-...`).
- No original game files, ever, in a repo or a package.
- Code changes go to the game repos. mw3-re only gets release tags and docs.

More detail: mech3-re's `docs/release.md` (the checklist), `docs/launcher.md`
(the launcher, the setup kit's contract, updates) and re-setup-kit's
`DESIGN.md`.
