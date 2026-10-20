#!/bin/sh
# Runs the unit tests and every differential test against the original exe.
#   tests/differential/run_all.sh          (textures take ~4 minutes)
set -e
cd "$(dirname "$0")/../.."
cmake --build build
./build/mw3tests | tail -1
if [ -x build/mw3sdltests ]; then ./build/mw3sdltests | tail -1; fi
for t in \
    spans \
    ztimer \
    zreader \
    heat \
    power \
    damage \
    controls \
    gait \
    skis \
    jets \
    actions \
    motion \
    clip3d \
    clip2d \
    fogsort \
    drawtable \
    zclass \
    mechlib \
    mechdef \
    mechsave \
    gamez \
    anim \
    matrix \
    lighting \
    light_setters \
    bake \
    modeldraw \
    world_params \
    textures \
    texload \
    scenewalk \
    interp \
    app_loop \
    app_options \
    app_states \
    shadows \
    shadow_image \
    puffers \
    sound \
    weapons_zrd \
    weapons_run \
    weapons_hit \
    weapons_fly \
    twod \
    twod_blit \
    twod_msgbox \
    update \
    di \
    collide \
    anim_run \
    mechanim \
    mechdata \
    pose \
    ; do
  .venv/bin/python "tests/differential/test_$t.py" | tail -1
done
