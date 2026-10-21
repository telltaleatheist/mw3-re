# The player's view and its cameras

The view ([0x005f4c8c], made by the mission load at 0x0048ff99) owns the
scene's camera node, camera1. Each frame its camera mode, one of the target
mech's six cameras, puts that node in the cockpit, behind, above or beside
the mech. Ported in `mech3/player_view` (all of it, emulator-checked) and wired
into the program in `mech3/app_view`.

## 1. The view (0x0040aca0, vtable 0x005bc2e8; base zView 0x004b5060)

| Offset | Field |
|---|---|
| +0x04 | the camera node (camera1) |
| +0x08 / +0x0c | zView's window and the object whose +4 is the world node the ground query asks (0x004b4fe0) |
| +0x14 | [0x0072f348] when made: the render's path (0x004b52c0: 0x004f7030 in Direct3D, else 0x004f6dc0) |
| +0x18 | the target's speed, smoothed |
| +0x1c / +0x2c / +0x3c | the offset's rotation, the look, the look times the mode's delta (quaternions w, x, y, z) |
| +0x4c | where the camera is put |
| +0x58 / +0x5c | the mode, the last mode's id (1 at first) |
| +0x60 | the distance used |
| +0x64 | the target (a mech) |
| +0x68 / +0x6a | handled (nothing in the family sets it), under water |
| +0x78 | the last frame time |

The constructor sets the camera's horizon to the `horizon` node (list 6).
Slots: +0x00 0x0040bab0 adds a node under the camera, +0x04 0x004b52c0
renders, +0x08 0x0040ada0 updates.

- **Update** (0x0040ada0, from the player's tick 0x004a1160 each frame, and
  once from the mission start): the frame time kept, the mode's update; then
  the ground and water check 0x0040af30 (with a mode and a target) and the
  weather check 0x0040b9a0.
- **Attach** (0x0040adf0): the old mode's switch off and the old target's
  vt+0x104, the new target's vt+0x100 and the mode's activation, then one
  update with the last frame time. The mech's attach (0x0041ce30) passes its
  data's camera list's current camera (+0x10, 0 at first: tether), then sets
  up the HUD (0x00430aa0, 0x0043bb80).
- **Camera switch** (0x0040ae70): the target's camera `id` (mech vt+0x10c)
  when it is usable: the last mode's id kept, its switch off, the new one on.
  The launch (0x00491150) switches to the options' camera (GameOptions bit 0
  or 8, 0x0040ce70), the cockpit by default; the camera key (0x0040aed0)
  goes attach, zippercam (when the pilot's game allows, +0x8e), tether.
- **Placing** (0x0040b7e0): the position (0x0040b600) at the mech's point,
  plus the offset turned by the rotation and times the distance; with a
  distance the rotation follows at dt x the mode's turn rate (+0x38) by a
  scaled relative quaternion (0x00525690, 0x00525be0), and tether, zippercam
  and area are pulled in before walls (0x0040b310, not ported: segment
  queries). Then the look (0x0040b750) follows at dt x the look rate (+0x3c),
  times the mode's delta, as the camera's Euler angles (0x00525f80). The
  reset form (0x0040b210) sets both at once.
- **Ground and water** (0x0040af30): the ground under the camera (from 50 m
  above, 0x004edab0) and the water there (0x004abd90): under the water (its
  level above the camera and above the ground) the world's fog becomes
  world.zrd's water fog (0x0040b0e0), else the air's. An outside camera below
  the ground is lifted to half a metre above it. The camera's zone comes from
  the ground's hit (0x0040b080).
- **Weather** (0x0040b9a0): a segment 100 m up from the camera; something
  over it hides the weather (vt+0x64), the open shows it again (vt+0x60).

## 2. The camera modes

Base (0x0040a950, vtable 0x005bc2c0), 0x78 bytes as the mech cameras are:

| Offset | Field (the camera block's key) |
|---|---|
| +0x04 | id: 0 tether, 1 attach (cockpit), 2 zippercam, 3 godseye, 4 mcbtether, 5 area |
| +0x08 | the delta rotation (`delta_rot`, degrees as Euler angles) |
| +0x18 | the offset (`desired_pos`) |
| +0x24 / +0x28 / +0x2c / +0x30 / +0x34 | `dist_vary_factor`, `dist`, `dist_factor`, `dist_vary`, `dist_catch_up` |
| +0x38 / +0x3c | the turn and look rates (2, 3; the cockpit 2, 4) |
| +0x40 | the free look (0 left, 1 right, 2 ahead, 3 none, 4 up, 5 down) |
| +0x44 | the look's rotation (mech cameras) |
| +0x54 | look along the torso |
| +0x5c | the speed, smoothed |
| +0x60 | the view |
| +0x64 | the cockpit's fixed state: its camera node a child of the torso node |
| +0x68..+0x74 | `look_limits` left, right, down, up (degrees; 89, -89, -15, 45 by default) |

- **The mech cameras** (0x00421530, vtable 0x005bd2f8): tether, zippercam,
  godseye, mcbtether. Update (0x00421760): the speed's absolute forward value
  (local velocity z) drops slowly (0.15 x the gap per second) and rises at
  once; the distance `dist + dist_factor x speed + dist_vary x (speed - the
  view's)`, never under 0.9 x `dist`; the mech's rotation (times the torso's
  aim when looking along it), times the free look.
- **attach** (vtable 0x005be9f0, update 0x00421e40): at the torso node plus
  the offset turned by the mech (or by the AI's held view, torso +0xa4 when
  +0xa2), the head's bob (the mech's +0x58 vector less the torso's twist and
  pitch inputs x -0.2, times 0.15, [0x005e244c], when [0x005e2448]).
  Activating it hands the torso the look (0x0041ce10, 0x00426780) and, with
  the mech switched off or powering down, fixes the camera: its node becomes
  the torso node's child (0x00421d50).
- **area** (vtable 0x005be9c8, update 0x004221d0): 40 m off at the height the
  view camera's field of view gives (tan of half of it), looking along the
  free look's quarter turns (0x004220c0).
- **Usable** (0x00421a30): the cockpit always; the others unless the pilot's
  game bars outside cameras (+0x8e).
- **The set-up** (0x00430e70, mech_data.cpp): the six with their defaults
  (tether's height by the mech: 0.42 to 0.7 of a unit up), then each one's
  block under the definition's `camera` node (0x004215d0). Every shipped
  `attach` block has `dist (0.0)`, its own `desired_pos` and `look_limits`.

FIX (bug): the cockpit's update leaves its offset unset when its distance is
not 0 (the original reads the stack there); the port uses 0. No shipped
mech reaches it.

## 3. Testing

- `test_view.py` (`tests/probe_view.cpp`): 300 runs of a view, a mech and up
  to four modes of the four classes with random fields, through random calls
  (update over frames, attach, the camera switch, the free look, reset, the
  camera key, the set-up on stubbed reader calls); everything outside stubbed
  and answered from one script. Compared: the call traces (the camera's
  position and angles each frame, the angles within one unit in the last
  place: asin and atan2), the view's and every mode's fields. Breaking the
  port: 21 of 24 caught; the misses cannot change a result (the distance
  floor's NaN rule, the handled flag nothing sets, the ground check's
  default height that a target always replaces).
- mw3tests `mission_c1m1_camera_follows_the_cockpit_headless`: c1m1 from the
  launch, the Mad Cat driven by the keyboard: the view is on it in the
  cockpit, camera1 within 4 m of the torso node every frame for 359 frames,
  walking 20 m and more, the offset turning with the mech.

## 4. In the program (app_view.cpp)

The mission load's 0x0040aca0 makes the port's view on camera1; the mech
data's 0x00430e70 makes each type's cameras; the launch's 0x0041ce30 and
camera switch, the options' camera choice, the player's tick and the Core
state's reset reach it. The app code still reads the view by address: after
each change its mode (a block whose +4 is the id) and target go into the
view's block. Not wired yet (logged): the ground, water and weather queries
(cls_di.c), the wall pull-in 0x0040b310, the HUD's 0x0043bb80, the torso's
cockpit switch 0x0041ce10.

## 5. Left

- The wall pull-in 0x0040b310 (segment queries 0x004efcd0, 0x004a3290).
- The world queries the checks use (0x004edab0, 0x004abd90, 0x004eed90) and
  the zone set 0x004f6c80, 0x005a80d0.
- The HUD's camera calls. (The torso's cockpit switch 0x0041ce10 acts now
  that the cockpit block is made: playtest #4, 2026-10-21.)
