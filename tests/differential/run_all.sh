#!/bin/sh
# Runs the unit tests and every differential test against the original exe.
#   tests/differential/run_all.sh          (textures take ~4 minutes)
set -e
cd "$(dirname "$0")/../.."
cmake --build build
./build/mw3tests | tail -1
for t in spans ztimer zreader heat power motion zclass mechlib mechdef mechsave gamez anim textures; do
  .venv/bin/python "tests/differential/test_$t.py" | tail -1
done
