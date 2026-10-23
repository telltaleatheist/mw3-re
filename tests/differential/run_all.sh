#!/bin/sh
# Runs the unit tests and every differential test against the original exe.
#   tests/differential/run_all.sh          (over 90 minutes; textures alone ~4)
set -e
cd "$(dirname "$0")/../.."
cmake --build build
./build/mw3tests | tail -1
if [ -x build/mw3sdltests ]; then ./build/mw3sdltests | tail -1; fi
# Every test_*.py here, so a new test is never left out of the run.
for f in tests/differential/test_*.py; do
  echo "== $(basename "$f" .py)"
  .venv/bin/python "$f" | tail -1
done
