# The repos, how they connect, and how a release is deployed

This file is the same in every repo of the family. Edit the copy in
[mw3-re](https://github.com/telltaleatheist/mw3-re/blob/main/DEPLOY.md) (the
hub, where the download is built), then copy it to the others.

## The repos

| Repo | What it is | Its tags | Its Releases page holds |
|---|---|---|---|
| [mw3-re](https://github.com/telltaleatheist/mw3-re) | **The hub.** The launcher (named BattleTech), its packaging, the games catalog (`catalog/`) and this page | `vX.Y.Z` (the launcher's version) | **The download**: the launcher for every platform and `SHA256SUMS`. The launcher's updates come from here |
| [mech3-re](https://github.com/telltaleatheist/mech3-re) | MechWarrior 3 | `vX.Y.Z` | MechWarrior 3 alone (`MW3-alone-...`), which the launcher downloads |
| [mech3-pm-re](https://github.com/telltaleatheist/mech3-pm-re) | Pirate's Moon: a complete copy of the game with Pirate's Moon's changes in place | `pm-vX.Y.Z` | Pirate's Moon alone (`PM-Rebuild-...`) |
| [mech1-remake-re](https://github.com/telltaleatheist/mech1-remake-re) | MechWarrior 1 Remake: MechWarrior (1989)'s campaign on this engine | `mw1-vX.Y.Z` | MechWarrior 1 Remake (`MW1-Remake-...`) |
| [mech1-re](https://github.com/telltaleatheist/mech1-re) | MechWarrior 1: Reverse Engineered (MW1 Rebuild), MechWarrior (1989) as our own C++ | its own | MW1 Rebuild |
| [inception-re](https://github.com/telltaleatheist/inception-re) and [inception-remake-re](https://github.com/telltaleatheist/inception-remake-re) | BattleTech: The Crescent Hawk's Inception, rebuilt and remade | their own | their own |
| [re-setup-kit](https://github.com/telltaleatheist/re-setup-kit) | The shared "find your game files" kit and the contract between the launcher and each game | none (pinned by commit) | nothing |

The hub's tags before v1.6.1 (v1.5.0 to v1.6.0) point at mech3-re's
commits, from when the launcher was built there. From v1.6.1 the launcher is
tagged on the hub's own main. Each repo's version numbers are its own: a
launcher release and a game release need not share a number.

## How they connect

```
mw3-re  (the original repo; the launcher and the download hub)
  └─ mech3-re          its full history continues here: MechWarrior 3
       └─ mech3-pm-re       a fork: the whole game with Pirate's Moon's changes in place
            └─ mech1-remake-re   a fork of Pirate's Moon: MW1's campaign on the same engine

re-setup-kit  (a git submodule at extern/re-setup-kit in the hub and in each game repo)
```

- **mw3-re is the base of all of them.** mech3-re was pushed with mw3-re's
  whole history, mech3-pm-re from its `pm` branch and mech1-remake-re from
  its `mw1` branch, so every game repo shares one history. The tree as it was
  at the split (2026-11-21) is on mw3-re's branch `archive/frozen-2026-11-21`
  and its tag `frozen-2026-11-21`; mw3-re's main holds only the launcher.
- **The launcher and the games meet only through the setup kit's contract.**
  The launcher runs a game's program and asks it (`--status`, `--setup`), and
  starts it to play; it carries no game code. Each game sets itself up from
  the player's own copy and starts on its own, without the launcher.
- **Fixes flow down by merge**: mech3-re's main is merged into mech3-pm-re,
  and mech3-pm-re's into mech1-remake-re. Nothing merges back up: Pirate's
  Moon's improvements never reach MechWarrior 3, which stays as it shipped
  plus bug fixes. A bug found downstream that MechWarrior 3 also has is fixed
  on mech3-re (cherry-picked or redone) and comes down again by merge.
- **The setup kit** is pinned by commit in each repo. A kit change reaches a
  program only when its repo moves its submodule.
- **Until the old copy is taken out** (docs/launcher-split.md, step d),
  mech3-re still holds an older copy of the launcher's code (mw3's own find
  screen uses it) and MW3 alone's package still carries a launcher. Launcher
  changes are made on the hub.

## What a player downloads

The launcher, from mw3-re's Releases:

| Platform | The launcher (the one updates take) |
|---|---|
| Windows | `MW3-Rebuild-Setup-<v>.exe` (or `MW3-Rebuild-<v>-win64.zip`) |
| Mac | `MW3-Rebuild-<v>-mac.dmg` |
| Linux | `MW3-Rebuild-<v>-x86_64.AppImage` |

- **The launcher downloads each game** from that game's own release when the
  player installs it (docs/launcher.md, Games on demand), into a folder of
  its own, checked against the release's `SHA256SUMS`. Which games there are,
  their repos, tags and asset names come from the games catalog: the copy
  built into the launcher (`src/upgrades/update/games.json`) and the one it
  fetches from the hub (`catalog/games.json` and `catalog/art/` on main, on
  GitHub and on the mirror). The fetched one is used only when its
  `revision` is above the built-in one's.
- **Each program carries its own version**, read from its repo's tag when it
  was built (`git describe`) and stamped into the file. The launcher reads it
  back without running the program.
- **No game files are ever in a package.** Each game copies what it needs
  from the player's own copy into its own settings folder.

## How updates work

The launcher's own update (src/upgrades/update, src/frontend/sdl/sdl_update.cpp):

1. Asks GitHub for the releases of **mw3-re** and then **mech3-re** (`curl`,
   HTTPS only), and the mirror's copy of each. One not answering is fine.
2. Takes the highest bare `vX.Y.Z` tag that is not a draft or a prerelease
   **and carries this platform's launcher package** (`MW3-Rebuild-...`). A
   game's own release (mech3-re's MW3 alone, any `pm-v` or `mw1-v` tag) is
   never taken. So no game repo's release may carry an `MW3-Rebuild-...`
   asset.
3. If it's newer than its own version, shows the release's notes and offers
   DOWNLOAD AND INSTALL.
4. Downloads the package and the release's `SHA256SUMS`, and installs only if
   the package's SHA-256 matches its line. A missing line, a mismatch or a cut
   download installs nothing. From the next launcher on, `SHA256SUMS` from a
   home other than GitHub is taken only with its `SHA256SUMS.sig` checking
   against a key in the launcher's `sources.json` (docs/launcher.md, Where
   the launcher looks, and signed data).
5. Installs it each platform's normal way: Windows runs the new Setup over
   the install; the Mac copies the new app beside the old one and swaps it
   in; Linux writes the new AppImage beside the old one and swaps it in. The
   old copy is removed only once the new one is in place. Settings, pilots,
   saves, mods and installed games are never touched.

Each installed game is checked the same way against its own repo's releases
(the catalog's `repo`, `tag_prefix` and asset names).

So the launcher package's names, its `SHA256SUMS` and the hub's Releases
page are a contract with every copy already installed. Don't rename the
assets, move the download off mw3-re, or publish without `SHA256SUMS`.

## Deploying a release

Releases are cut by the lead session when the project owner clears them.

**A launcher release (the hub):**

1. **Get main ready:** the launcher's changes, its tests for what changed
   passing (`build/launchertests`), the release notes, and the catalog: a
   change to the games list goes in `catalog/games.json` with its `revision`
   raised, and the built-in copy follows it at the launcher's next release,
   its revision kept at or below the fetched one's.
2. **Choose the version.** Features raise the second number, fixes alone the
   third (1.3.9 is followed by 1.3.10).
3. **Tag locally, don't push yet:** `vX.Y.Z` on the hub's main.
4. **Build every platform from that tag** (`tools/package`):
   - Mac: `tools/package/combined.sh mac <v> --deps`
   - Windows (the PC): `tools\package\combined.cmd <v>`
   - Linux (WSL on the PC, after Windows):
     `tools/package/combined.sh linux <v> build-deps/linux/prefix ../deps`

   Each builds the launcher's package in `build-pkg/dist/<platform>/` and
   runs its checks: every library present, the licences, no game program or
   game file, no local path or user name, and no version ending in `-dirty`.
   If a platform fails, fix it on main, move the local tag, and rebuild every
   platform from the new tag. The tag is pushed only once all three build.
5. **Gather, sum and sign:** the four files in one folder, each checked
   against its platform's `SHA256SUMS`, then
   `MW3_SIGNING_KEY=<key file> python3 -I tools/package/sums.py <folder>`
   writes the release's `SHA256SUMS` and `SHA256SUMS.sig`. Without the
   variable it writes no `.sig`, and only GitHub's copy of the release
   installs.
6. **Push the tag and publish** the `vX.Y.Z` release on mw3-re with the four
   files, `SHA256SUMS` and `SHA256SUMS.sig`, not a draft or a prerelease.
7. **Check it live:** download every asset again and check its checksum, then
   run the previous release's launcher, press its update, and confirm it
   finds the new one.

**A game release** is built and published in the game's own repo, by its
own checklist (mech3-re's `docs/release.md`), with its files and their lines
of `SHA256SUMS` (signed the same way, `sums.py` with `MW3_SIGNING_KEY`, so
the mirror's copy installs too). It needs no launcher release: installed launchers find it by
the catalog. A new game, or a game's new asset names, is a catalog change on
the hub.

**The catalog** (a change to `catalog/games.json`, its `revision` raised):

1. `MW3_SIGNING_KEY=<key file> python3 -I tools/package/publish_catalog.py <folder>`
   checks that it reads as a catalog, writes `catalog/games.json.sig` beside
   it, and stages `catalog.json`, `catalog.json.sig` and `art/` in `<folder>`.
2. Commit `catalog/games.json` and `catalog/games.json.sig` together on the
   hub's main (GitHub's raw copy and the mirror's then carry both).
3. Copy `<folder>` to the site's `/launcher/` path, served as plain files by
   the web server (`MW3_CATALOG_DEST`, an rsync destination, does it in
   step 1; no address is kept in the repo).

A catalog that is not signed, or signed by a key the launcher does not know,
is passed over by every launcher from the next release on; released ones
(1.7.1 and older) read the hub's file unsigned, so it stays where it is.

**The keys.** The public keys are in `assets/launcher/sources.json`
(`"keys"`, `sign.py --public` prints them); CMake warns while it lists none,
and such a build takes no fetched catalog. The private key is kept by the
lead outside every repo, with a backup. To move to a new key: list it in a
signed catalog's `next_keys` and in sources.json at the next launcher
release, sign with both (`MW3_SIGNING_KEY=old:new`) until every launcher in
use has it, then drop the old one. A leaked key: publish a catalog signed by
a key that stays (with that key in `next_keys` of an earlier catalog, or in
sources.json) whose `revoked_keys` names the leaked one; every launcher that
takes it stops trusting the leaked key for good.

After a release: issue replies announcing it are the project owner's to
approve.

## Rules that keep it working

- The hub's Releases page stays the launcher's download and update source.
  Don't archive mw3-re (archiving locks its releases), and never create
  another repo named mw3-re.
- Every release carries `SHA256SUMS` and its `SHA256SUMS.sig`; neither the
  launcher nor a game is installed without its line, and a home other than
  GitHub is trusted only through the signature.
- The signing key never goes in a repo, a package or a log.
- The launcher package's file names are fixed (`MW3-Rebuild-...`), and only
  the hub's releases carry them.
- `catalog/games.json` stays on the hub's main (released launchers fetch it
  from there), its revision above the built-in one's when it should be used.
- Never delete a game's entry from `catalog/games.json`: set its `state` to
  `removed`. A removed game's panel stays for players who have it installed
  or linked (it plays and updates as before) and is hidden from everyone else.
  A deleted entry of a game that is only in the fetched catalog (not built
  into the launcher) is forgotten everywhere it is not on the computer;
  launchers from the next release on keep it where it is installed or
  linked (docs/launcher.md, The games catalog), older ones forget it.
- No original game files, ever, in a repo or a package.
- Launcher changes go to the hub; game changes go to the game repos.

More detail: `docs/launcher.md` (the launcher, the setup kit's contract,
updates, games on demand), `docs/launcher-split.md` (how the launcher left
mech3-re) and re-setup-kit's `DESIGN.md`.
