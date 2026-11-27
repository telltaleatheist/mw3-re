# Launcher 2.1.0 (2026-11-27)

A launcher release. The games are unchanged.

## Counting updates

GitHub's download counts can't tell a new player from an update, so the
launcher now tells reverse-engineered.com when an update has finished.

- After a game's UPDATE (or UPDATE ALL) installs and passes its checks, and
  after the launcher updates itself, it sends one short message: which game
  (or "launcher"), the old and new version, the launcher's version, and
  whether the download came from GitHub or our mirror.
- Nothing personal is sent: no name, no account, no computer ID, no files.
  The site keeps counts only.
- A first download sends nothing.
- To turn it off, put `"send_update_counts": false` in your
  `launcher-settings.json` (in the launcher's data folder, where
  `mw3launcher.log` is). The site can also switch it off for everyone.
- The launcher also names its version when it fetches the games list, so we
  can see which versions are in use.

## Upgrading

Press UPDATE AVAILABLE in your launcher, or install over it. Your games,
saves and settings stay as they are.
