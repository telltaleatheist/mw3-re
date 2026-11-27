#!/bin/bash
# The launcher's package for this platform (DEPLOY.md): from a vX.Y.Z tag on the hub's main, built in a clean
# worktree of its own, into build-pkg/dist/<platform>/:
#   MW3-Rebuild-<v>-mac.dmg / -x86_64.AppImage   the launcher (what the launcher's updater takes); it downloads
#                                               each game from the game's own release (docs/launcher.md, Games on
#                                               demand). Each game's package is built in the game's repo.
# Then tools/package/sums.py writes their SHA256SUMS (and again over every platform's files once collected).
#
#   tools/package/combined.sh mac   <ref> [--deps]       (on the Mac; --deps for a release)
#   tools/package/combined.sh linux <ref> PREFIX TOOLS   (in WSL; make-appimage.sh's)
#
# The ref: the tag for a release (v1.6.1); any commit for a check (origin/main). The worktree is build-pkg/main
# (ours, replaced each run); the deps (build-deps/, ../prefix) are linked or passed.
set -euo pipefail
cd "$(dirname "$0")/../.."
ROOT="$PWD"
PLATFORM="${1:?mac or linux}"
REF="${2:?the ref (a vX.Y.Z tag)}"
shift 2
case "$PLATFORM" in mac|linux) ;; *) echo "mac or linux, not $PLATFORM" >&2; exit 2 ;; esac
EXTRA=("$@")
if [ "$PLATFORM" = linux ]; then  # absolute, for the worktree
  [ ${#EXTRA[@]} -ge 2 ] || { echo "linux needs PREFIX and TOOLS (make-appimage.sh's)" >&2; exit 2; }
  EXTRA=("$(cd "${EXTRA[0]}" && pwd)" "$(cd "${EXTRA[1]}" && pwd)")
fi
W="$ROOT/build-pkg"
OUT="$W/dist/$PLATFORM"
mkdir -p "$W"

# A clean worktree at the ref in build-pkg/main: an earlier one of ours removed first (only this path).
dir="$W/main"
if [ -e "$dir" ]; then
  git worktree remove --force "$dir" 2>/dev/null || { echo "$dir is not a worktree of ours: remove it yourself" >&2; exit 1; }
fi
git worktree prune
git worktree add --detach "$dir" "$REF" >/dev/null
# extern/re-setup-kit; a checkout whose .git/config points it at a local clone (the kit's developers) is allowed
# that clone (git refuses the file transport for submodules by default).
git -C "$dir" -c protocol.file.allow=always submodule update --init --quiet
# The deps the package scripts read from the checkout (build_deps.sh's), linked in.
[ -d "$ROOT/build-deps" ] && ln -s "$ROOT/build-deps" "$dir/build-deps"
echo "main: $(git -C "$dir" describe --tags --always --dirty) ($(git -C "$dir" rev-parse --short HEAD))"
# The built-in games catalog is the live one, its badges shipped (DEPLOY.md, step 1): refused otherwise.
python3 -I "$dir/tools/package/check_catalog.py" "$dir"
rm -rf "$OUT" && mkdir -p "$OUT"

if [ "$PLATFORM" = mac ]; then
  (cd "$dir" && tools/macos/make_app.sh ${EXTRA[@]+"${EXTRA[@]}"})
  cp "$dir"/build-mac/dist/MW3-Rebuild-*-mac.dmg "$OUT/"
else
  (cd "$dir" && tools/linux/make-appimage.sh "${EXTRA[@]}")
  cp "$dir"/build-linux-pkg/dist/MW3-Rebuild-*-x86_64.AppImage "$OUT/"
fi
python3 -I tools/package/sums.py "$OUT"
ls -l "$OUT"
