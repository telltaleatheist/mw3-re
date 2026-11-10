#!/bin/bash
# The combined package for a release (docs/release.md, The combined package): MechWarrior 3 and the launcher from a
# vX.Y.Z tag on main, Pirate's Moon from a pm-vX.Y.Z tag on pm, each built in a clean worktree of its own; then
# three packages for this platform, all in build-pkg/dist/<platform>/:
#   MW3-Rebuild-<v>-mac.dmg / -x86_64.AppImage   the launcher, mw3 and mw3pm (what the launcher's updater takes)
#   MW3-alone-<v>-...                           the launcher and mw3 (the updater never takes it)
#   PM-Rebuild-<pm v>-...                       mw3pm alone (branch pm's own package)
# Then tools/package/sums.py writes their SHA256SUMS (and again over every platform's files once collected).
#
#   tools/package/combined.sh mac   <main ref> <pm ref> [--deps]        (on the Mac; --deps for a release)
#   tools/package/combined.sh linux <main ref> <pm ref> PREFIX TOOLS    (in WSL; make-appimage.sh's two folders)
#
# Refs: the tags for a release (v1.1.0 pm-v1.0.2); any commit for a check (origin/main origin/pm). The worktrees are
# build-pkg/main and build-pkg/pm (ours, replaced each run); the deps (build-deps/, ../prefix) are linked or passed.
set -euo pipefail
cd "$(dirname "$0")/../.."
ROOT="$PWD"
PLATFORM="${1:?mac or linux}"
MAIN_REF="${2:?the main ref (a vX.Y.Z tag)}"
PM_REF="${3:?the pm ref (a pm-vX.Y.Z tag)}"
shift 3
case "$PLATFORM" in mac|linux) ;; *) echo "mac or linux, not $PLATFORM" >&2; exit 2 ;; esac
EXTRA=("$@")
if [ "$PLATFORM" = linux ]; then  # absolute, for the worktrees
  [ ${#EXTRA[@]} -ge 2 ] || { echo "linux needs PREFIX and TOOLS (make-appimage.sh's)" >&2; exit 2; }
  EXTRA=("$(cd "${EXTRA[0]}" && pwd)" "$(cd "${EXTRA[1]}" && pwd)")
fi
W="$ROOT/build-pkg"
OUT="$W/dist/$PLATFORM"
mkdir -p "$W"

# A clean worktree at `ref` in build-pkg/<name>: an earlier one of ours removed first (only these two paths).
worktree() {
  local name="$1" ref="$2" dir="$W/$1"
  if [ -e "$dir" ]; then
    git worktree remove --force "$dir" 2>/dev/null || { echo "$dir is not a worktree of ours: remove it yourself" >&2; exit 1; }
  fi
  git worktree prune
  git worktree add --detach "$dir" "$ref" >/dev/null
  # The deps the package scripts read from the checkout (build_deps.sh's), linked in.
  [ -d "$ROOT/build-deps" ] && ln -s "$ROOT/build-deps" "$dir/build-deps"
  echo "$name: $(git -C "$dir" describe --tags --always --dirty) ($(git -C "$dir" rev-parse --short HEAD))"
}
worktree main "$MAIN_REF"
worktree pm "$PM_REF"
rm -rf "$OUT" && mkdir -p "$OUT"

if [ "$PLATFORM" = mac ]; then
  # Pirate's Moon alone (pm's own script), which also leaves mw3pm built.
  (cd "$W/pm" && tools/macos/make_app.sh "${EXTRA[@]}")
  cp "$W"/pm/build-mac/dist/PM-Rebuild-*-mac.dmg "$OUT/"
  PM_BIN="$W/pm/build-mac-pkg/mw3pm"
  # MechWarrior 3 alone, then the combined one from the same build.
  (cd "$W/main" && MW3_PKG_STEM=MW3-alone tools/macos/make_app.sh "${EXTRA[@]}")
  cp "$W"/main/build-mac/dist/MW3-alone-*-mac.dmg "$OUT/"
  (cd "$W/main" && MW3_PKG_KEEP_BUILD=1 MW3_PKG_PM_BIN="$PM_BIN" tools/macos/make_app.sh "${EXTRA[@]}")
  cp "$W"/main/build-mac/dist/MW3-Rebuild-*-mac.dmg "$OUT/"
else
  (cd "$W/pm" && tools/linux/make-appimage.sh "${EXTRA[@]}")
  cp "$W"/pm/build-linux-pkg/dist/PM-Rebuild-*-x86_64.AppImage "$OUT/"
  PM_BIN="$W/pm/build-linux-pkg/mw3pm"
  (cd "$W/main" && MW3_PKG_STEM=MW3-alone tools/linux/make-appimage.sh "${EXTRA[@]}")
  cp "$W"/main/build-linux-pkg/dist/MW3-alone-*-x86_64.AppImage "$OUT/"
  (cd "$W/main" && MW3_PKG_KEEP_BUILD=1 MW3_PKG_PM_BIN="$PM_BIN" tools/linux/make-appimage.sh "${EXTRA[@]}")
  cp "$W"/main/build-linux-pkg/dist/MW3-Rebuild-*-x86_64.AppImage "$OUT/"
fi
python3 -I tools/package/sums.py "$OUT"
ls -l "$OUT"
