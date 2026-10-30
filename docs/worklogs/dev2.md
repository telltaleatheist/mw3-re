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

## Item 2: the damage diagram, mode 2 (assigned 2026-10-29, in progress)

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
