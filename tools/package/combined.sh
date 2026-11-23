#!/bin/bash
# The combined package for a release (docs/release.md, The combined package): MechWarrior 3 and the launcher from a
# vX.Y.Z tag on main, Pirate's Moon from a pm-vX.Y.Z tag on pm, each built in a clean worktree of its own; then
# three packages for this platform, all in build-pkg/dist/<platform>/:
#   MW3-Rebuild-<v>-mac.dmg / -x86_64.AppImage   the launcher, mw3 and mw3pm (what the launcher's updater takes)
#   MW3-alone-<v>-...                           the launcher and mw3 (the updater never takes it)
#   PM-Rebuild-<pm v>-...                       mw3pm alone (branch pm's own package)
# Then tools/package/sums.py writes their SHA256SUMS (and again over every platform's files once collected).
#
#   tools/package/combined.sh mac   <main ref> <pm ref> [--deps] [--mw1 <mw1 ref> [--mw1-port]]  (on the Mac; --deps for a release)
#   tools/package/combined.sh linux <main ref> <pm ref> PREFIX TOOLS [--mw1 <mw1 ref>]  (in WSL; make-appimage.sh's)
#
# --mw1: MechWarrior 1 Remake (branch mw1, an mw1-vX.Y.Z tag) built too, in build-pkg/mw1, and put in the
# MW3-Rebuild package beside the launcher with its mw1data/ (combined.cmd's third ref); MW3 alone and PM alone never
# carry it. --mw1-port also brings Elthial's MechWarrior (1989) SDL port for PLAY ORIGINAL: tools/package/mw1port/
# build_port.sh at its pins with the proposed patches (read-only opens of the player's game), staged in
# build-pkg/mw1port (Windows: combined.cmd with MW3_PKG_WITH_PORT=1). Off: cut for now (2026-11-20).
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
EXTRA=() MW1_REF="" WITH_PORT=0
while [ $# -gt 0 ]; do
  if [ "$1" = --mw1 ]; then MW1_REF="${2:?--mw1 needs a ref}"; shift 2
  elif [ "$1" = --mw1-port ]; then WITH_PORT=1; shift
  else EXTRA+=("$1"); shift; fi
done
if [ "$PLATFORM" = linux ]; then  # absolute, for the worktrees
  [ ${#EXTRA[@]} -ge 2 ] || { echo "linux needs PREFIX and TOOLS (make-appimage.sh's)" >&2; exit 2; }
  EXTRA=("$(cd "${EXTRA[0]}" && pwd)" "$(cd "${EXTRA[1]}" && pwd)")
fi
W="$ROOT/build-pkg"
OUT="$W/dist/$PLATFORM"
mkdir -p "$W"

# A clean worktree at `ref` in build-pkg/<name>: an earlier one of ours removed first (only these paths).
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
[ -z "$MW1_REF" ] || worktree mw1 "$MW1_REF"
rm -rf "$OUT" && mkdir -p "$OUT"

# mw1, when asked for: a package build (MW3_PACKAGE: no development game paths), mw1 and mw1data/ at its root.
MW1_DIR=""
if [ -n "$MW1_REF" ]; then
  MW1_DIR="$W/mw1/build-$PLATFORM-mw1"
  CONF=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DMW3_PACKAGE=ON -DMW3_BUILD_MW1=ON)
  if [ "$PLATFORM" = mac ] && [[ " ${EXTRA[*]-} " == *" --deps "* ]]; then  # as make_app.sh --deps builds
    CONF+=(-DCMAKE_PREFIX_PATH="$ROOT/build-deps/mac/prefix" -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0)
  elif [ "$PLATFORM" = linux ]; then
    CONF+=(-DCMAKE_PREFIX_PATH="${EXTRA[0]}")
  fi
  rm -rf "$MW1_DIR"
  cmake -S "$W/mw1" -B "$MW1_DIR" "${CONF[@]}" >/dev/null
  cmake --build "$MW1_DIR" --target mw1 >/dev/null
  [ -x "$MW1_DIR/mw1" ] && [ -f "$MW1_DIR/mw1data/zbd/mw1image.zbd" ] || { echo "mw1 or its mw1data was not built in $MW1_DIR" >&2; exit 1; }
  echo "mw1: $(strings "$MW1_DIR/mw1" | grep -m1 '^MW3-PROGRAM-VERSION:' | cut -d: -f2-)"  # read, never run
fi

PORT_DIR=""
if [ -n "$MW1_REF" ] && [ "$WITH_PORT" = 1 ]; then
  PORT_DIR="$W/mw1port"
  KIT="$W/main/tools/package/mw1port/build_port.sh"  # the main ref's kit, as its make_app.sh expects
  [ -d "$ROOT/build-deps/mw1port/src/port/.git" ] || MW1PORT_SRC="$ROOT/build-deps/mw1port/src" "$KIT" fetch
  PORT_ARGS=()
  if [ "$PLATFORM" = linux ]; then PORT_ARGS+=("${EXTRA[0]}")  # make-appimage.sh's PREFIX
  elif [[ " ${EXTRA[*]-} " == *" --deps "* ]]; then PORT_ARGS+=("$ROOT/build-deps/mac/prefix"); fi
  MW1PORT_SRC="$ROOT/build-deps/mw1port/src" "$KIT" "$PLATFORM" "$PORT_DIR" ${PORT_ARGS[@]+"${PORT_ARGS[@]}"} --proposed >/dev/null
  echo "mw1port: $(grep -m1 ' port ' "$PORT_DIR/licences/SOURCES.txt" | awk '{print $NF}')"
fi

if [ "$PLATFORM" = mac ]; then
  # Pirate's Moon alone (pm's own script), which also leaves mw3pm built.
  (cd "$W/pm" && tools/macos/make_app.sh ${EXTRA[@]+"${EXTRA[@]}"})
  cp "$W"/pm/build-mac/dist/PM-Rebuild-*-mac.dmg "$OUT/"
  PM_BIN="$W/pm/build-mac-pkg/mw3pm"
  # MechWarrior 3 alone, then the combined one from the same build.
  (cd "$W/main" && MW3_PKG_STEM=MW3-alone tools/macos/make_app.sh ${EXTRA[@]+"${EXTRA[@]}"})
  cp "$W"/main/build-mac/dist/MW3-alone-*-mac.dmg "$OUT/"
  (cd "$W/main" && MW3_PKG_KEEP_BUILD=1 MW3_PKG_PM_BIN="$PM_BIN" MW3_PKG_MW1_DIR="$MW1_DIR" MW3_PKG_MW1_PORT_DIR="$PORT_DIR" tools/macos/make_app.sh ${EXTRA[@]+"${EXTRA[@]}"})
  cp "$W"/main/build-mac/dist/MW3-Rebuild-*-mac.dmg "$OUT/"
else
  (cd "$W/pm" && tools/linux/make-appimage.sh ${EXTRA[@]+"${EXTRA[@]}"})
  cp "$W"/pm/build-linux-pkg/dist/PM-Rebuild-*-x86_64.AppImage "$OUT/"
  PM_BIN="$W/pm/build-linux-pkg/mw3pm"
  (cd "$W/main" && MW3_PKG_STEM=MW3-alone tools/linux/make-appimage.sh ${EXTRA[@]+"${EXTRA[@]}"})
  cp "$W"/main/build-linux-pkg/dist/MW3-alone-*-x86_64.AppImage "$OUT/"
  (cd "$W/main" && MW3_PKG_KEEP_BUILD=1 MW3_PKG_PM_BIN="$PM_BIN" MW3_PKG_MW1_DIR="$MW1_DIR" MW3_PKG_MW1_PORT_DIR="$PORT_DIR" tools/linux/make-appimage.sh ${EXTRA[@]+"${EXTRA[@]}"})
  cp "$W"/main/build-linux-pkg/dist/MW3-Rebuild-*-x86_64.AppImage "$OUT/"
fi
python3 -I tools/package/sums.py "$OUT"
ls -l "$OUT"
