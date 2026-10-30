# Work log: mw3 dev 2

The second worker session, from 2026-10-29 (worktree `.claude/worktrees/mac5`,
branches `mac5/<item>`). The protocol is in `README.md`. "Mw3 dev" (mac4)
works alongside it; stay out of its files.

## Item 1: TARGET's mode-1 view crash (2026-10-29, ahead of item 2)

- From Owen's Windows playtest: an access violation in
  `zcls::render_subtree` (`Node::parent_count`) through app_hud.cpp's
  `view_draw_node`.
- Cause: `ProgramHudHooks::object_node` read every target's node as a
  mech's (`Mech::node`). TARGET's mode 1 also shows turrets, whose
  `self` and `node` are 32-bit handles, so the read was a garbage pointer.
  The exe reads the object's +0x04, whatever its class (0x00441410 ->
  0x004428b0).
- Fix: `hud_object_node` (app_sensors.cpp, the sensors' per-class node)
  behind `object_node`. targets.zrd objects never reach mode 1 (vt+0xa8).
  Removed objects resolve to null by handle, and dead mechs keep their node.
- Test: `mission_c1m1_target_views_a_turret_headless`, with the helper
  `program_hud_target_other`. Break: the old read crashes with Owen's stack.
- Branch `mac5/target-view-crash`. hud.md section 22, The target's node.

## Item 2: the damage diagram, mode 2 (2026-10-29)

- Owen: at full health only one arm shows; every part should be blue
  when undamaged, yellow when slightly damaged, and so on.
- Found so far: the logic matches the exe. At full health all eight parts
  read 1.0 and hold their `_blue` image (0x004475c0, 0x00447a50; logged in
  play). In rimage.zbd seven `dmg_<part>_blue` images are pure (0,0,255)
  = 565 0x001f, and `dmg_rarm_blue` is (0,4,255), the only part shown.
- Cause: `dmg_bg_blue` is a translucent 0x001f fill (alpha plane). The
  layer's cover (FIX (A), render-frame.md) records its alpha and colour.
  blit16's keyed path writes the part's 0x001f without clearing the cover,
  so the overlay draws it as the translucent background. Green, yellow and
  red, and the right arm's 0x003f, differ from 0x001f. Fix: cover_opaque on
  every opaque write into the 2D layer (blit16's ten paths, line16,
  prim2d's put), `zrndr::cover_opaque` now public.
- Tests: `fix_layer_opaque_ends_a_blend_of_its_colour` (each writer, 10/10
  breaks caught), mw3sdltests `sdl_host_damage_diagram_shows_every_part`
  (the real silhouettes at each band; unfixed it shows Owen's picture).
- Shots in the outbox (2026-10-29, mfd-damage-diagram-*): all blue at full
  health; after `--vehicle-ahead 40 --vehicle-hold` hits, left arm and leg
  green. The exe's first damage band is green (above 0.7), not yellow.
- Branch `mac5/hud-damage-diagram` on main cd45f5f (item 1 merged).
  every opaque write in blit16; extend `fix_layer_blend_keeps_its_alpha`.

## Item 3: finding the install at start (2026-10-29)

- Owen's request through the lead: mw3.cfg's install=, else the standard
  places, else a message box and a file dialog; save to mw3.cfg; check runs
  never ask. Ours, FIX (A).
- The installer, read from the MW3 disc (Owen's backup ISO; SETUP.INS and
  _USER1.CAB's value.shl): `SOFTWARE\MicroProse\MechWarrior 3\1.0` gets
  "Program" and "Version" (PUBLISHER_NAME, PRODUCT_NAME, PRODUCT_VERSION),
  InstallationInfo's App Paths for PRODUCT_KEY `mech3.exe`, and the default
  folder PROGRAMFILES\MicroProse\MechWarrior 3. Whether "Program" is the
  folder or the exe is not in the script, so both are taken.
  Mech3Register.exe names nothing. Pirate's Moon's key is "MechWarrior 3 EP1".
- `src/frontend/sdl/sdl_install.{h,cpp}` (in mw3sdl). In mw3_main.cpp only
  `find_install(Args&)` and mw3.cfg's install= read into `cfg_install`, to
  keep clear of mw3 dev's start-up item. `MW3_FIND_INSTALL=1` skips the
  development copy (for checking the search and the dialog).
- Tests: `sdl_install_is_found_and_saved` (4/4 breaks caught),
  `sdl_program_check_runs_never_ask_for_the_install`. On the Mac the box
  opened (a 260x352 window of mw3's, by CGWindowList; no accessibility or
  screen-recording access to click or capture it). The dialog's pick flow
  needs a person: the lead tests it on Owen's PC.
- Branch `mac5/find-install` off main cd45f5f.

## 4. W7: TARGET's view of a mech (2026-10-30)

- TARGET's camera (0x00440b00) is the target's centroid (entity +0x14) less
  the contact direction x 1.2 x size, aimed along player centroid -> target
  centroid. 0x00441410 itself places nothing. The program's
  `ProgramHudHooks::entity_position` gave a mech's place (+0x04, the ground)
  for both points, so the view centred on the feet of every mech; the target
  triangle (0x00440510) too. The probe's hook read +0x14 already, so the
  differential test never saw it.
- Fix: `entity_position` reads the centroid node; `object_position` and
  `vehicle_position` keep the place they meant.
- Test: the MFD/TARGET mission test checks TARGET's camera height against
  the Owens' centroid (5.2 m both); the place read put back gives 0.0 and
  fails.
- Shots: outbox/2026-10-30/W7-target-{owens,madcat}-{before,after}.png
  (c1m1 Owens, c3m2 Mad Cat, `--mech-ahead 60 --tap 0x12@860`, frame 960).
- Branch `mac5/target-camera` off main 730a01c.
## 5. W9: the lab's skin change blinks (2026-10-30)

- Reproduced: `--lab-mech 9 --click-at 362,304@2900 --shot-series 12`:
  frame 2903 showed the 3D mech alone on black.
- The skin swap 0x0044fd90 opens and closes the 3D lock (0x00576d00 /
  0x00576d60) while the screen's draw holds it (checked: back.locked 1).
  The exe: the inner lock is a no-op, the inner Unlock really unlocks, the
  screen's unlock does nothing; DirectDraw's pointer stays writable, so the
  2D drawn after it reaches the flip. Nothing holds or skips the frame.
- Ours: `GameEngine::unlock_back` lays the layer over the frame at the
  unlock, so the 2D drawn after the inner unlock was lost.
- Fix (A) in `zvid::lock_3d` / `unlock_3d`: a lock of the locked back buffer
  is counted and its unlock only counts down (`zfix::options().nested_lock`,
  off in the probes). After: frames 2899..2909 all show the lab.
- Tests: `fix_lock_inside_a_lock_keeps_the_outer` (both paths; 2 of 2
  breaks), test_d3d_frame.py (2000 locks identical, the original's path).
- Branch `mac5/lab-skin-blink` off main 730a01c.
## 6. S8 and S10: the lab view (2026-10-30)

- Both as the original; no code change.
- The 3D window is (210, 52)-(430, 288) ([0x005e4688], 0x00450460, its only
  caller 0x0045015d) on every page; the camera's place and rotation are read
  from [0x005e4648] / [0x005e4678] only at 0x00450321 and nothing else
  writes them. The frame draws the screen under the 3D lock (0x00448f10),
  then the mech (0x00450690), so the mech covers anything 2D in its window.
- S8: the criticals line is the page's dialog text at (220, 60), inside
  that window: covered whenever the head turns past.
- S10: shots at frame 3000 of the chassis, criticals and groups tabs, at
  1280x960 and at 1280x720, frame the Bushwacker alike; the visual pass's
  crop (640 scale, origin about (330, 0)) matches this geometry with the
  mech side-on. The turn (0.8 rad/s) decides how it sits.
- Branch `mac5/lab-view-s8-s10` off main 730a01c (docs only).
