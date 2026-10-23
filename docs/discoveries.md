# How MechWarrior 3 really works

Findings from taking `Mech3.exe` 1.2 apart, written for players. Each one comes
from the game's own code; the address and the technical write-up are given so
anyone can check. "Likely" marks a guess.

## Heat

From the heat update at 0x4192f0 (details: `heat.md`).

- **Heat runs from 0 to 2.0, and the warnings sit at fixed points.** The beep
  starts above 0.7. "Shutdown imminent" plays above 0.95. Dropping back to 0.9
  cancels the shutdown. At 2.0, critical, the mech dies (except in
  multiplayer, below).
- **A mech standing still with ten standard heat sinks sheds 0.0625 heat a
  second**, about 30 seconds from the 2.0 maximum down to zero. Double heat sinks
  cool exactly twice as fast. Each extra sink adds 10%.
- **Wading is a flat x8 cooling, however deep the water.** The code divides the
  water depth by the mech's height and squares the result, which looks like it
  was meant to cool more the deeper you stand. But it takes the larger of that
  ratio and 1, not the smaller, so the result is always at least 16 and gets
  capped at 8. Ankle-deep water cools as well as being neck-deep.
- **Only the left foot counts.** The water check reads the left foot alone. Wade
  in with just your right foot and you get no cooling.
- **Hot missions cool you slower.** Every mission sets an ambient temperature
  (`world.zrd`), and heat sinks shed heat in proportion to 1 / temperature.
  At the default 20 degrees that is the usual rate; the 40-degree missions
  (c1, c4b mission 5) cool at half speed, t1 and c4 (about 31) about 1.5 times
  slower, and the cold c4b missions much faster: 10 degrees twice as fast,
  2 degrees ten times.
- **Each mission caps how high jump jets take you**: 150 by default, far lower
  in the underground and lava missions (c2 m2, c4b m5) (mission_world).
- **Powering down triples your cooling.** While shut down (heat shutdown or the
  power button), dissipation is x3.
- **Lava ignores the frame rate.** Standing on lava (either foot) adds 0.05 heat
  every frame, not every second. Everything else in the heat code scales with
  time. So the faster the game ran, the faster lava cooked you: about 1.5 heat a
  second at 30 fps, 3 at 60 fps. The game waits for the monitor's refresh, so
  in 1999 that topped out around 60-85 fps; at 144 fps today it would be over
  7 a second. The rebuild fixes it at 1.5 a second, what lava did at 30 fps.
- **Critical hits can destroy heat sinks, but your pilot never says so.** One
  kind of critical hit on a part (0x460d50, type 0) knocks out a heat sink, and
  cooling drops 10% for each one lost. It is meant to play a voice callout, but
  only when the heat sink routine (0x45fbc0) reports success, and that routine
  always reports failure. The sink is gone, silently. Losing a whole part takes
  its sinks with it (0x4617f0). The rebuild plays the callout (fixes.md).
- **Jump jets and MASC each add 0.05 heat a second** while running, on top of
  weapons. Running hard also cools you slower: past 60% throttle (either
  direction), cooling drops, to 60% of normal at full throttle.
- **Shutdown has stages.** Above 0.95 you get the "shutdown imminent" warning,
  and about 4 seconds later the mech powers down unless you've dropped to 0.9.
  Hitting 1.5 skips the warning and shuts you down at once. Once shut down, the
  mech restarts when heat falls to 0.4.
- **Overriding a shutdown is a dice roll.** If you override the automatic
  shutdown and stay hot (above 0.9), the alarm starts and, every few seconds
  (a random 0.1 to 3, then 3 more), the game rolls. 80% of the time you get
  away with it, but each of those rolls may also cook off an ammo bin, the one
  with the least ammo x damage left, with an explosion sound. The other 20%,
  the torso's damage routine runs and heat resets to zero (exactly what that
  does to the mech isn't traced yet). At 2.0 there is no roll: the torso is
  destroyed. A mech that does shut down skips all of this and just waits to
  cool to 0.4.
- **AI mechs play by the same heat rules as you.** There is a separate, simpler
  rule in the code ("non-player mechs explode at 2.0") behind a switch that is
  always on, so it never runs. AI mechs warn, shut down, cook off ammo and die
  exactly like the player; they just don't hear the beeps.
- **"Heat Death disabled" is the invincibility cheat talking.** The debug
  menu's "Invincible" item (or an option bit) sets a flag on your mech (+0x630)
  that blocks damage and criticals; when heat would have killed you, the game
  prints "Heat Death disabled" instead (damage.md).
- **The shutdown override only works while the alarm is beeping.** "Override
  Thermal Shutdown" (0x419190) does nothing unless the heat warning is already
  sounding. Once on, it cancels the automatic power-down, but not at 1.5 heat,
  which still shuts you down, and it switches itself off when you cool below
  0.4.
- **You can't restart early from a heat shutdown.** The power on/off routine
  (0x41cc30) does nothing while a heat shutdown is in progress; the game powers
  you back up itself once heat is down to 0.4. (Likely what the power button
  calls; not traced from the key yet. The in-game help says the startup key
  "can also restart your 'Mech after an automatic shutdown", which this code
  does not allow.)
- **Powering down waits for your legs.** Pressing power-down only asks; the
  mech actually shuts off once its animation reaches a stop pose (states 1, 12
  or 13), and a power-down asked for while a flag at +0x584 is set (likely
  mid-air) is ignored.
- **Heat can't kill you in multiplayer.** In multiplayer, heat is always on,
  but reaching critical heat never destroys your torso: it only shuts you down
  and resets your heat. In campaign, instant action and training, critical heat
  is fatal (gametypes.md).

## Damage

From the damage code (damage.md).

- **Criticals only happen through thin armor.** A hit can roll a critical only
  when the part's armor is at 15% or less, and then one roll decides: 0.8%
  heat sink, 0.5% ammo, 0.3% equipment, 0.3% weapon, 0.1% "location special"
  (engine hit in the torso, instant death in the head, a limp in a leg).
- **Lasers never cause criticals.** Every laser is a beam weapon, and beams are
  excluded from the critical roll; PPCs, autocannons, missiles and Gauss can.
- **A critical replaces the hit's damage.** When a critical is rolled, that
  hit's armor and structure damage is skipped, even if the critical has nothing
  to destroy.
- **Ammo criticals don't explode.** An ammo critical empties the smallest bin;
  no damage follows. CASE in the part blocks it.
- **Losing one leg kills the mech**; losing an arm or a side torso never does
  (a lost side torso takes its arm with it).
- **A part can't be destroyed while it has armor.** Structure damage can never
  take the internal value below the remaining armor fraction.
- **Difficulty only scales damage you take**: x0.7, x1.0 or x1.3, and not in
  multiplayer. Your allies' mechs likely get the same scaling (a team-mask
  test); enemies always take full damage.

From the port of the damage code, checked in an emulator against the original
(damage.md, section 4):

- **Rear armor is only hit from behind.** The torso decides front or rear by
  which face of its footprint the shot enters through. Shots through the sides
  always hit the side torso's front armor; a shot through the middle of the
  back only counts as rear within about 45 to 60 degrees of straight behind
  (0x460350).
- **Grazing shots count as centre-front.** A shot whose line misses the
  torso's box (it clipped the outline) hits the centre torso's front armor.
- **Side torsos are hollow** (from the code, not yet seen in play). Losing a
  side torso takes its arm, but the side torso itself is never treated as a
  lost part: weapons mounted in it keep firing, and heat sinks, jump jets and
  other equipment listed there keep working. Criticals on torso hits only look
  at what is listed in the centre torso, so ammo and equipment in the side
  torsos can never be hit by a critical (only the overheat cook-off reaches
  side-torso ammo).
- **CASE in a side torso does nothing.** Since side-torso ammo can't be hit by
  a critical anyway and the CASE check on torso hits only looks at the centre
  torso, CASE is only useful in the centre torso or an arm. The stock Mad Cat
  carries its two CASE in its side torsos.
- **ECM makes you immune to NARC.** A NARC hit sets a 30-second beacon, unless
  the target carries ECM (0x41ac10).
- **Equipment criticals favour bulky gear.** The item destroyed is chosen with
  odds proportional to its critical slots (0x460d50).
- **A crippled engine slows you each time.** The torso's "location special"
  critical cuts your top speed by 15%, again on every such hit, with the
  engine callout.
- **Knockback builds up.** Every hit with impact force pushes along the shot
  and adds to a running total; when it passes a threshold set by your tonnage
  (light mechs about 3.8, assault mechs about 15) you stagger. A fall clears it.
  Such hits on the torso, arms, head or hip also rock the torso, twisting it
  away from the hit, harder for heavier weapons (0x464af0); leg hits do not.
- **Area damage skips the head** and spreads a seventh of the damage over the
  other seven locations, a fifth of that to each rear torso. Unlike direct hits
  it is scaled by difficulty even in multiplayer.
- **The hip has no armor of its own** and hip hits go straight to the torso,
  without the screen static that torso, arm and head hits give you.
- **XL engines have no damage rule.** The `xl` setting changes only the
  engine's weight in the mech lab; nothing in combat reads it.
- **In Instant Action, wrecks disappear after three minutes.** A dead mech
  lies where it fell for 170 seconds, fades out over the next ten, and is
  then removed; only then are its kills added to the match's tallies.
- **Enemy mechs are silent while you move.** The game spends its one
  "full detail" slot per frame only while your throttle is at zero, and a
  mech's footsteps, engine and power sounds need that detail, so with your
  throttle up no other mech makes a sound; stop, and the first one within
  250 m is heard again. Beyond 800 m a mech also stops moving its legs in
  full and switches to its low-detail model (unless you have it targeted).
- **Pilots eject when their mech is destroyed** (unless it explodes, or was
  already lying on the ground): an ejection seat effect starts at the hip,
  the mech sags at the waist as it starts to fall, and the eject sound is
  played, then cut off in the same instant by the code that silences a dead
  mech, so nobody has ever heard it.
- **The ejection seat ignores which way the mech faces.** Its offset from the
  hip (for the Owens 3.6 m up and 1.46 m along the world's z axis) is added
  in the world's directions, not turned with the mech.
- **Shots were meant to leave marks on walls.** The engine can paint a mark
  image into the texture wherever a weapon hits, picked by the weapon's
  impact type, but only once a mission script hands it the images with
  `WriteTextureSetMap`, and no mission in MechWarrior 3 or Pirate's Moon
  does, so the feature never shows.
- **Kill points come from a weight class that ignores tonnage.** A mech kill
  scores 300, 400, 500 or 700 by the class the roster gives it, and that
  class is often off: the 100-ton Annihilator scores as a heavy (500), the
  75-ton Mad Cat as a medium (400), the 35-ton Owens and Puma as mediums too.
  An Elemental scores 100 for any mech it kills; a vehicle is worth 75 and a
  building 200.
- **Only your lance's kills fill the salvage.** In the campaign a mech
  killed by you or a lancemate goes to the mission's salvage; one finished
  off by anyone else (or with no attacker, such as a heat death) does not.
- **Blown-off arms and legs fly out sideways**, away from the mech's own
  side, at 20 m/s plus three quarters of the mech's speed, with a little
  lift; an empty stump model is left in their place. In Instant Action the
  wreckage fades out.
- **A Blackhawk or Owens that loses a leg loses the arm on that side too.**
  No other mech does.
- **An AI mech remembers who hit it last forever.** Only the player's own
  mech forgets its attacker each frame, so an enemy that later dies from a
  fall, heat or its own ammo still gives the kill (and the salvage) to
  whoever hit it last.
- **A slow death alternates between falling left and right** for all mechs
  together: one shared switch flips at each death below 50 km/h, so two
  mechs dying one after another fall to opposite sides.
- **Difficulty changes how tough vehicles are, not how hard they hit.**
  Every hit on a tank, truck or other vehicle counts double on low
  difficulty and half on high (0x4af550), for allied vehicles you escort
  as well as enemy ones. Hard missions with convoys to protect are easier
  on that score, and convoys to destroy take twice the fire.
- **Lasers never shake a vehicle.** A hit of more than 5 damage jolts a
  vehicle along the shot (heavier vehicles less), but lasers carry an
  impact-heat effect and the jolt skips any weapon with one; they warm
  the vehicle's heat glow by the share of health each hit takes instead.
- **Vehicles aim past their gun's limits.** A vehicle's gun is meant to
  reach about 29 degrees up and 15 down (no vehicle file changes that),
  and the game has code to bring an aim back onto those limits, but the
  AI never uses its result: a tank aims straight at you however steep the
  angle (that code also has a slip that would put the aim off the limit).
- **Turrets swing half way to their target every eighth of a second**,
  however far they have to turn, so a turret is never caught facing the
  wrong way for long.
- **The targeting view shows a vehicle's damage in red.** In TARGET's
  picture of a vehicle, the model is tinted red by how much health it has
  lost, up to 60% just before it dies.
- **The game can freeze vehicles, but no weapon does.** Vehicle damage
  has a whole freeze path: a freezing weapon chills instead of hurting,
  and a vehicle cold enough is frozen solid and ignores further freezing
  hits. No weapon in MW3 or Pirate's Moon has the `FREEZE` effect.

## Movement

From the gait, the controls, the ground probes, the jump jets and the falls
(details: `movement.md` and the `movement-*.md` files). The ones marked
"code reading" have not been run in the emulator yet.

- **Every gait animation is 60 frames.** Whatever the motion file holds, each
  walk, trot or run is resampled to 60 keys at load and turned to start with
  the left foot furthest forward; the step animation is then shifted so its
  footfall lines up with the walk's, which is what makes stepping off into a
  walk seamless.
- **The right leg's limp is the left one mirrored**, and so is the left-side
  collapse: the files only have one of each. A mech without a collapse
  animation falls over backwards instead.
- **Falls and getting up play at 20 frames a second, collapses at 15**, slower
  than the 30 the gaits run at.
- **When you start walking, the right foot snaps.** For the first two thirds
  of a second the legs ease from their last pose into the walk, except that
  the right foot's angle jumps straight to the walk's while the left one
  eases twice over (a mix-up in the original code: the right foot's result
  is written over the left's). Fixed in the rebuild.
- **In the cockpit your torso barely sways.** The player's torso takes only a
  tenth of the walk animation's sway when you look out of the cockpit, the
  full amount in the outside views.
- **Mid-jump, the legs hold a frozen standing pose.** The game has a slot for a
  jump pose but never loads one, so while the jets fire the legs take the
  stand animation at the moment it was left.
- **MASC is all or nothing.** Turning MASC on raises the top speed by a
  quarter and keeps your current speed; turning it off drops the top speed at
  once, so the mech slows immediately.
- **Your throttle is not a speed, it picks a gait.** Each mech's file lists
  where its walk, trot and run start, as fractions of full throttle; the
  animation of that gait then carries the mech: its legs' stride is its
  speed. Stand and step move nothing at all.
- **Throttle up and down at different rates.** Opening the throttle eases in at
  the mech's `accel_rate`, closing it at `decel_rate` (each file sets its own,
  from 0.4 for the assault mechs to 1.5 for the Elemental). Every shipped mech
  closes its throttle exactly twice as fast as it opens it. The throttle closes
  in on what you ask exponentially, so the last few percent take longest. The turn eases in at
  `turn_accel_factor`. Jump-jet thrust builds at 7 a second and dies at 9.
- **The file's stand and step speeds are ignored.** Whatever a mech file says,
  the game forces stand to end at 0.01, step to run from 0 to 0.05 and walk to
  start at 0.02 of full throttle.
- **Turning in place makes a standing mech creep forward**, at just under walking
  speed; the throttle is pushed up for you while you turn. Turning is slower
  the faster you go: at full throttle a mech turns at 20% of its turn rate.
- **Heat slows you down.** Above 0.6 heat the mech is slowed, down to 60% of
  its speed at a heat of 1.0 and above.
- **Deep water almost stops a mech, and deeper water lets it go.** Wading
  slows you once the water is a tenth of your height deep, to 70% shrinking
  towards nothing as the water nears the top of the mech. But once the water
  is deeper than the mech is tall the slowdown snaps back to a flat 70%. The
  shallower foot decides.
- **Limping and reversing each cost 40%** of your speed, and they stack.
- **Walking blends in.** For the first 20 frames after starting to walk the
  speed is blended up from where it was, so how fast it builds depends on the
  frame rate. Between stepping and walking it stays stuck at 15% a frame.
- **Mechs far away walk, always.** Past 800 m (unless it is your target) a mech
  plays its walk animation whatever its speed, and uses one ground probe
  instead of seven.
- **The ground is found by seven probes, not by the legs.** Three along each
  side and one in the middle; the body rises and sinks with their
  average. The body never tilts on slopes; the legs bend to the ground instead.
- **Gravity is four times stronger on the ground** (120 against 30 in the air),
  which keeps the mech glued to the terrain over bumps.
- **Steep slopes slide you.** On a slope steeper than about 32 degrees (unless
  its material is "noslip") the mech is pushed downhill. Past 35 degrees the
  push grows, and a step of more than 3 m in front of the feet shoves the mech
  back.
- **Water is not ground.** The probes ignore water polygons and stand on the
  bottom; the water only tells the game how deep you are.
- **Jump jets climb at 8 m/s per second at full thrust** against gravity, and a
  full tank burns in `100 x jets / tonnage` seconds (a 50-ton mech with 4 jets:
  8 s), refilling at half that rate on the ground. Jets need a quarter tank to
  light from the ground. Losing jets later does not change the burn rate.
- **Your jets push you along your throttle; AI jets steer.** The player's jets
  push the mech forward (or back, in reverse) with no speed limit once it is
  half its height off the ground. With the MW2 jump-jet option, and for AI
  mechs, the jets steer forward/back and sideways instead, capped at your top
  speed and 40 m/s sideways.
- **The jump ceiling is a speed limit.** Rising speed is capped so the mech
  closes on the ceiling over about 4 seconds and never reaches it; above it, it
  is pushed down.
- **Hard landings rarely trip you.** A landing faster than 80 m/s, or with more
  than 20 m/s of sideways speed, has a 20% (40% sideways) chance to knock you
  down; landing faster than 56 m/s hurts both legs a little. Landing sets your
  throttle to your landing speed.
- **A fall costs both legs `tonnage / 10000`** of their structure when the mech
  hits the ground, then it lies there 5 seconds before getting up.
- **Remote mechs in multiplayer** take no landing falls, no knockdowns and no
  fall damage locally; their falls come from the network.
- **Momentum in a fall fades per frame**, not per second (60% kept each frame),
  so how far a falling mech slides depends on the frame rate.
- **Ramming hurts both mechs** (code reading): damage grows with the square of
  your speed against your top speed, split by tonnage so the heavier mech takes
  less. Allies take none. Vehicles take 20 times more and are shoved, or
  crushed if that kills them.
- **Buildings break under you frame by frame** (code reading): touching a
  building or animated object does 20 damage to it every frame, so it falls
  faster at a higher frame rate. Walking under an overhang hurts your head and
  torso and makes the mech crouch.
- **AI mechs bounce off walls** (code reading): an AI's throttle flips to reverse
  when it hits a wall; yours drops to 10%. Only your mech has the low probes at
  its feet, so AI mechs walk over low things you would bump into.

- **A mech wading into water about its own height grinds to a halt.** The
  slowdown for wading is computed from how much of the mech is still above
  the water, so it reaches zero just before the water covers the mech; once
  the water is deeper the mech walks at 70% again. In a river of your mech's
  height you stand still. (The rebuild fixes this to a smooth slowdown down to
  70%.)
- **Many movement steps happen once per frame**, so they depend on the frame
  rate: how long a fall keeps its momentum, how hard slopes and ledges shove
  you, how quickly the walk blends in from a standstill (20 frames), and the
  20 damage a frame you deal to a building you lean on. At 144 fps a building
  takes 2,880 damage a second instead of 600. (The rebuild makes them per
  second, as at 30 fps.)
- **Pushing into another mech hurts both of you, by weight.** Each frame of
  contact deals damage from your speed (squared, against your top speed), and
  the heavier mech takes the smaller share; the other mech is also shoved a
  little along your direction. Allies are spared the damage.
- **Ramming a vehicle damages it with 20 x (speed / top speed)^2 a frame**; if
  that kills it you drive straight through, otherwise it is shoved.
- **Hitting your head on a ceiling while jumping bounces you down** at two
  thirds of your climb and costs a sliver of head armour; walking into
  something low crushes your head and torso by the volume of what you hit, and
  your mech ducks.
- **Bumping into a mech that stands within 9 m** pushes you apart, swings you
  round at almost twice your turn rate and cuts your throttle to a tenth.
- **Your mech leans into turns** (by its `lean_angle`), but only half as much
  in the cockpit view.
- **The "last part hit" resets to the torso one second after a hit**, and
  keeps being refreshed every frame after that.
- **A mech lying on the ground that is powered down still turns its torso
  and moves its parts**; one that is powered up holds still until it gets up.
- **"Center legs" swings the legs round at 1.75 times the twist per second**
  (0x464e80, `movement-ground.md`): with the torso twisted 40 degrees, the legs
  start turning at 70 degrees a second and slow as they catch up. Throttle
  does not slow it: the game divides the turn by the same throttle factor a
  normal turn multiplies by. Touching any torso or look control cancels it.
- **The torso twist has a servo sound only while it moves freely**: it starts
  with your input and stops the moment the twist hits its limit, or when you
  let go.
- **A foot only "lands" while it is coming down.** It counts as touching once
  it is within 25 cm of the ground and not rising, and lifts off again above
  20 cm. Each landing plays the ground's step sound, at 45% volume standing
  and full volume at full throttle, and leaves a footprint 5 cm above the
  ground, except in water (a splash instead) or while you are being shoved
  (sliding off a slope or a ledge).
- **Every footfall hits what it lands on.** The moment a foot touches down
  the game hands whatever object is underfoot a 1000-point hit through its
  damage handler (likely what flattens small things a mech walks over).
- **Feet bank to the ground.** At close range a planted foot turns to lie
  flat on the slope under it; at a distance feet keep the walk's angle.
- **Arms track your target with their own limits.** Each arm turns toward the
  weapons' target at its own rate (`aim_slerp_freq`), within the pitch and
  yaw limits in the mech file (the right arm uses the left's limits
  mirrored), and drifts back to rest at half that rate when there is no
  target. Some arms leave the whole aim to the hand.
- **Weapon recoil twists your torso**: a shot kicks the arm back and turns
  the torso, a left-arm weapon one way and a right-arm weapon the other (by
  the mech's `recoil_twist`). The kick fades in about a third of a second.
- **The game knows when an arm is under water**: when both feet are in water
  and a part is below the surface, the player's HUD redraws its weapon list.
- **You can only crouch below walking pace** (slower than the step gait's
  top speed), and Elementals never crouch. Going faster stands you up; only
  your own mech plays the stand-up sound.
- **Ducking under a low ceiling is instant**: the hip drops at once by a
  share of the crouch depth (the crouch itself eases down at 2 m a second).
- **Landing dust looks 50 m down**: a landing or a fall throws dust from
  the ground under your torso, or a splash when that ground's material is
  water; nothing when there is no ground within 50 m below.

- **The radio has subtitles nobody can turn on.** `sounds.zrd` gives 40
  radio lines (the lance mates' calls for help, the answers to your orders)
  a subtitle message, and the sound code would print it in the message
  panel, but the switch that allows it is never set anywhere in the game,
  so they never appear.

## Weapons

From the weapon code (weapons.md).

- **Craters really dig into the ground.** A crater is not a painted mark:
  the game cuts the terrain polygons under the hole away, clips the ones on
  its edge, and puts in a sloped ring and a floor, textured to match the
  ground it hit (beach, road, sand and mud each have their own crater
  texture, set per world in `declient.zrd`). Each patch of ground only
  takes so many craters, a crater is not made within 5 m of an earlier
  one in the same patch, and none cut ground that must stay whole (nodes
  marked so, among them the craters themselves: a crater never cuts
  another). The crater's ring is the ground around it at the hit's height,
  the slope runs down to points half way to the centre one crater depth
  lower, and the floor's centre sits two depths below the hit.
- **The overlap test reads a crater the game already threw away.** The
  record a crater keeps for the 5 m test is freed as soon as the crater is
  finished, so the next crater in that patch compares itself with whatever
  the memory holds by then: sometimes the old crater, sometimes the new
  one (refusing it), sometimes junk. The rebuild keeps the outlines.
- **Cut ground can have its texture slid by one corner.** When a crater's
  cut leaves a piece with a corner on a straight edge, the game drops that
  corner from the shape but not from the texture coordinates or colours,
  so the rest of that piece is textured and shaded as if shifted along by
  one corner (about 1 polygon in 200 that a crater adds). Fixed in the
  rebuild.
- **An untextured crater would crash the game.** The crater code writes the
  floor's texture position even when the crater has no texture. Every
  shipped crater has its texture flags, so it never happens.
  The cut ground is rebuilt from triangles and four-sided pieces: whatever
  is left of a ground polygon around the hole is split into convex pieces,
  and a crater that lands wholly inside one big ground polygon gets the
  ground around it rebuilt as a ring of triangles joining the polygon's
  corners to the crater's rim.
- **Craters keep the ground's shading.** The new crater polygons take the
  vertex colours of the ground they cut: each crater vertex copies a cut
  ground vertex within half a metre of it (else the first one cut), so a
  crater dug in a shadow stays dark and one in the open stays bright.
- **Two ways a crater can freeze the game.** The crater's cutter (a
  Weiler–Atherton polygon clipper) has two endless loops. One happens when a
  ground polygon's corner sits on the crater's outline to within a float's
  error. The other happens when the crater's outline runs exactly along a
  ground polygon's edge and touches it at a corner. In both the game hangs
  while digging the crater. A shot landing on a slope or a bank, where
  crater outlines meet the ground's corners, is the likeliest trigger. In
  multiplayer the host sends every crater to a player who joins, whose game
  rebuilds them one by one; in testing, two in about 300 such replays hit
  one of these loops. A
  third case, close to the second, writes points without end instead (a
  crash). The rebuild stops each one: the piece of ground is left uncut, or
  in the third case is cut correctly.
- **Unused: rainbow craters.** A crater that cuts untextured ground gets
  pieces of random colour, a leftover debug fallback (whose colours are even
  packed from the wrong bits); one made without its texture flags would
  crash the game before it got that far (see above). Every shipped world's
  diggable ground is textured and every crater keeps its flags, so neither
  shows.
- **Unused: quicksand weapons.** The same system can sink an animated
  quicksand pit (seven frames, looping, with its own soil type) where a
  shot lands, for a weapon with the `QUICKSAND` flag. Every world sets the
  pits up in `declient.zrd`, but no shipped weapon has the flag. Had one
  been made, it would not trap anything: the pit is covered by a moving lid
  at ground level that mechs walk on like any ground (its soil is none the
  game knows, so it counts as default ground), with the texture repeated
  4.5 times across it. `player.zrd` even sets a gravity for quicksand
  (and one for water), but the game never uses either.
- **One crater setting is ignored.** Every world's `declient.zrd` has a
  `TEXTURE_PARMS (1.0 1.0)` line that the game never reads.
- **The plain PPC can't be mounted.** `weapons.zrd` defines a `PPC`, but the
  MechLab's weapon table (0x5e64f0) only lists the ER PPC, so no mech can
  carry the plain one. Its name is in the game's text (message 0x2fd, right
  after the last weapon's) and the slot it once had between NARC and the
  pulse lasers is empty: it was moved to the end of the list and its row was
  never added. The rebuild adds it, so the mech lab offers the PPC.
- **Hot lasers recharge slower.** Above heat 0.5, a beam weapon's recharge is
  multiplied by 0.5 / heat (0x4b43bf): at heat 1.0 lasers refill at half speed.
- **Small lasers are the slowest to recharge**: 4 seconds, against 3 for medium
  and large (`weapons.zrd`).
- **Only a weapon's first group counts.** A loadout can list a weapon in two
  groups, like `group (1 2)`, and 59 entries across 11 stock mechs do, but the
  game reads only the first number.
- **One ammo bin per weapon type.** All launchers of the same weapon share one
  bin, whatever part the ammo is listed in; the part only decides which
  critical hit can reach it.
- **CASE stops ammo crits but not heat cook-offs.** CASE in a hit part cancels
  an ammo critical there (0x460e30), but the overheating cook-off roll never
  checks for it.
- **Clan CASE weighs nothing and works.** `equipment.zrd` gives Clan CASE 0 tons
  and 0 critical slots (Inner Sphere: 0.5 and 1). The protection check only asks
  whether the part lists a CASE (0x460c10), not its slots, so zero-slot CASE
  protects like any other, and since equipment criticals are weighted by slots,
  it can never be destroyed by one.
- **"Fire rate" in the data is really a period.** The weapon reader stores
  1 / `FIRE_RATE` (0x583e87), the time between shots.
- **Ultra ACs never jam**: they simply fire twice as often as the standard AC
  (`FIRE_RATE` 1.0 against 0.5); nothing random in the firing code.
- **Missile volleys heat per missile**: an LRM20 volley is 5 shots, each adding
  the weapon's HEAT (5 x 0.06), while ammo is taken once for all 20 missiles.
- **Pulse lasers stop when you let go**; the other lasers always finish their
  0.125 s shot.
- **Under water** a part's ballistic and missile weapons cannot fire at all
  (0x4b2f90); its lasers can.
- **Weapons can be moved between groups in the cockpit** (CMD_MOVETOWG_1..5).
- **Two weapons on one firepoint** are moved apart by a random offset up to
  the firepoint's `share` (0x4b2530); a salvo's shots scatter up to its
  `radius` instead.
- **Group fire with several lasers** plays the ground impact of the best one
  only (0x4b2100); every laser still does its damage.
- **Bullet pock marks do appear, on dirt, mud and snow.** The impact table
  looks like it only knows default, water, seafloor, quicksand, lava and fire,
  but each mission's script loads `soils.zrd` first, which adds dirt, mud,
  grass, concrete, snow, mech, silt and noslip, and the weapons are read after
  that. So the autocannons', Gauss rifle's, missiles' and machine gun's
  surface marks for those three ground types are read and shown (28 weapons
  have one).
- **Lasers lose up to a quarter of their damage with distance**, on a cosine
  curve: full damage up close, 75% at maximum range (`DAMAGE_AT_MAX_RANGE`
  0.75, 0x583020). A short laser shot deals its damage once, to the first
  thing it touches.
- **Under water a laser's reach halves**, and so does the distance its damage
  falls off over; it is not simply half damage.
- **LRMs inside 90 m do a quarter of the damage**, and their splash radius
  shrinks to a quarter too (`RANGE_MINIMUM (90 0.25)`, 0x586402).
- **Missile splash hits everyone near the impact except the target hit**,
  with damage falling off with the square of the distance: half damage at
  about 70% of the 30 m radius (0x5829b0). A missile that bursts in the air
  spares no one.
- **Missiles that miss just vanish** at their maximum range; they do not
  explode (every missile has `EXPIRES`).
- **A salvo's missiles all do full damage.** When several land within 10 m of
  each other within half a second, only the first plays its explosion sound,
  crater and animation (0x4afc00); the damage is not reduced.
- **The FCS "double tracking" does nothing.** Locking with FCS in the firing
  arm doubles a missile turn-rate setting, but the shot resets it before any
  missile reads it (0x58518a).
- **SRM wobble is held within about a metre** of the line: the random drift
  is pushed back once it strays past 1 m sideways (0.75 m up and down).
- **LB-X shells burst at 30 m** into three pellets each, in a cone of about one
  degree around the shell's path.
- **TAG calls four missiles**, from 1500 m back along the beam and 400 m up
  (so from far behind you, out of sight), each up to 25 m to the side. They
  hit for 18 each plus splash within 30 m.
- **Arm weapons can only aim about 27 degrees off the arm's facing**; centre
  torso weapons aim anywhere (0x4b38b6).
- **The AI waits for its LRMs**: it fires a launcher type again only after the
  last volley has flown 70% of its range (0x4b4c70).
- **The whole mission shares 164 shots in the air** (the projectile pool,
  sized at load); in a very big fight a new shot can silently fail to appear,
  its ammo and heat already spent.
- **Three LRM20 launchers on one mech share one pool of missiles**, and the
  pool counts its users oddly (the third launcher adds two references, four in
  all): harmless.
- **LRMs never reach their full turn rate.** A missile turns at TURN_RATE
  times a factor that is 0 for its TURN_SUSPEND_TIME, then (time - suspend) /
  LOCK_ON, and its clock stops one frame past LOCK_ON (0x585900): at 30 fps
  LRMs top out at about 93% of their 0.25 rad/s, TAG's strike missiles at
  about 80%, SRMs and Streaks at about 100% (the last frame's length decides
  the exact figure). Targets more than 90 degrees off the nose count as 90.
- **Every turn costs a homing missile speed**: it keeps 0.8 + 0.2 cos(turn) of
  its speed each frame it turns, and over its first LOCK_ON seconds the
  shooter's own speed fades out of it.
- **Long straight shots can pass through the nearest wall.** A cannon shot
  looks up every building along its whole range once, when it is fired, and
  then meets them in that list's order. The list is not fully sorted (the
  sort does one pass: only the farthest is sure to be last), so with three or
  more buildings in the line, listed in the wrong order, the shot can fly
  through a nearer one to hit one behind it (0x585010, likely rare: it depends
  on the order the world lists them).
- **A shot is drawn a frame behind where it is**: its model is moved to where
  the shot was at the start of the frame, not where it ends up (0x5864c0).
- **Lasers play their impact sound every frame** they touch something, and a
  laser plays its firing sound twice: when it starts and when it stops
  (0x581230, 0x581400).
- **TAG needs a full second on a target**: the beam counts up while it touches
  something that can be hurt and calls the airstrike after 1 s (DESIGNATE's
  last value). On the ground the count runs back down (a frame there costs
  two), on open sky it starts over. Its spot light is green (0.51, 1.0,
  0.03).
- **TAG works, but almost never visibly.** Checked frame by frame in the
  original code:
  - Its beam lasts only a hair longer than the second it needs: 32 frames
    at 30 fps for 30 needed, and at 60 fps 62 for 61. At 30 fps you can
    slip off the target for one frame; at 60 fps or more, not once.
  - Hold the trigger the whole time: letting go stops the beam, and the
    charge it used is gone. TAG only fires from a full charge, which takes
    67 seconds at 30 fps (83 at 60), so a short tap leaves it dead for up
    to a minute with nothing to say why.
  - The airstrike comes about 7 seconds later: the missiles start from a
    standstill 1.2 km or more behind you and speed up as they go. They fly
    to the spot the beam touched, not to the target, so a standing mech
    takes four direct hits (72 damage), one walking away at 18 kph only
    splash (about 58), and one moving faster than about 30 kph nothing.
  - Ground and buildings that cannot be damaged never call it.
  - On a fast PC the original's TAG recharges slower, and above about 166 fps
    (or at 144 fps when running hot) it never recharges after its first
    use. The rebuild recharges it at its 30 fps pace at any frame rate.
  - The rebuild keeps TAG hard but fair: the beam lasts what it did at 30
    fps (1.07 s) and the hold is a true second at every frame rate, so you
    always have about 67 ms to spare, and the airstrike homes on the mech
    you marked. Flown by the original's own missile code, all four hit a
    mech walking, crossing or running at 20 m/s.
- **The ERPPC's HUD static lasts 2 seconds** in the data, but the original
  read the first number twice and held it 0.5 s; the rebuild uses the 2 s.
- **Unused weapon features**: zWeapon supports mines that bounce, lie in wait
  and go off when something comes near (in multiplayer they expire after two
  minutes), wire-guided missiles that hug the ground, missiles that lead their
  target, bouncing lasers and lasers that chain between up to four locked
  targets, but no shipped weapon uses any of them. The landmines in missions
  are not mines in this sense: they fire the LANDMINE weapon at whoever walks
  close. A wire-guided missile would also lose its slot in the shared pool for
  the rest of the mission (0x586600).
- **AMS measures range on the map, not in the air**: it shoots down the
  nearest missile within 150 m counted as east-west plus north-south distance
  (|dx| + |dz|), ignoring height, one every 0.6 s (two AMS: every 0.3 s), 100
  rounds per AMS. Only missiles that show on the radar can be shot down.
- **Vehicles fire every weapon from one spot**: each weapon in a vehicle's
  file names two firepoints, but the game looks the name up in the whole list
  and always finds the first weapon's first one (0x4b4660).
- **What FCS does for missiles is terrain following**: with FCS in the firing
  part (and no ECM on the target), missiles flying under 10 m with ground in
  the way climb (0x5859fb, likely).

## Sound

- **Medium sound quality is low quality.** The detail setting picks
  `soundsH.zbd`, `soundsM.zbd` or `soundsL.zbd`, but no medium archive ever
  shipped, so the game moves on to the next one: medium loads the 11 kHz
  low set. The "medium" playback format is never read either: the setup
  asks `sounds.zrd` for `"MED"` (0x570ccf) where the file says `MEDIUM`
  (sound.md). The rebuild reads `MEDIUM`.
- **The game asks the sound card for mono.** The playback formats in
  `sounds.zrd` say stereo for high quality, but the setup stores 1 channel
  whatever the file says, so on the Windows 9x DirectSound of the time the
  whole mix was mono and the left/right panning of 3D sounds was lost. Since
  Vista Windows ignores that request, so players today hear it in stereo.
  The rebuild reads the file's channel count, so it asks for stereo too.
- **60 sounds were never shipped.** `sounds.zrd` names 1058 sounds, but 60
  WAVs are in neither archive: Dom's four ejection lines, Alan's fourth
  "disregard", the wingmen's acknowledgements ("sure sir", "affirmative",
  "right away sir", "consider it done", "I'm on it", "no problem") and
  taunts ("you'll pay", "yeah right", "don't take", "not gonna", "can't do",
  "goodbye", "returning"), the "occupied" calls, the mech's eject voice, and
  36 lines of mission dialogue and one ambient loop. They simply stay silent.
- **Wingmen never say they are low or out of ammo.** The game looks up
  `dom_low_ammo`, `alan_out_of_ammo` and so on (0x4e7dc0, "%s_%s"), but in
  `sounds.zrd` those six groups list `"PLAY_COUNT" 2` after their name, and
  PLAY_COUNT is not a group keyword: the reader takes it as the group's
  name, so all six are called "PLAY_COUNT" and the lookups find nothing.
  The rebuild restores them: Dom, Alan and Epona now call out low and empty
  ammo, each line at most twice a mission (a hint to rearm at the MFB).
- **Radio chatter queues newest first.** Lines marked QUEUE wait their turn,
  half a second apart, but the queue is a stack: the most recent line plays
  next, and a line that waited more than 5 seconds is dropped unheard.
- **Halving a sound's volume takes 10 dB off, not 6.** The game converts
  volume to decibels as 1000 x log2, so quiet settings fall off faster than
  the slider suggests.
- **Distant sounds fade by 6 dB per "near" distance.** A 3D sound is full
  volume to its RANGE's first value, then loses 6 dB for each further such
  distance, and cuts off past the second (a chopper: 350 and 900, so -9 dB at
  the cut-off). The distance is an approximation that can be 6% off, and
  panning reaches at most 16 dB, so sounds never come from one ear only.
- **No sound changes pitch.** Doppler and the weapons' pitch changes call
  DirectSound's SetFrequency, which only works on buffers made with
  frequency control, and no sound in `sounds.zrd` asks for it (`FREQUENCY`),
  so every such call fails silently. Had it worked, the doppler would have
  compounded: it scales the current pitch each update and never restores it.
- **Eight copies of a sound at most.** A ninth play of a sound already
  sounding eight times restarts the first one.
- **Volume fades rarely finish.** A fade moves 25 dB a second and ends only
  when it lands exactly on its target, which happens only at full volume or
  silence; a fade to anything in between keeps nudging the volume up and
  down one step around the target for as long as the sound plays. Fixed in
  the rebuild.
- **Unpausing replays sounds.** Leaving the pause menu does not resume what
  was playing: for every sound that was playing it starts all its copies
  again (any that had already finished included, from the start), and twice
  for a sound that had two copies playing. Fixed in the rebuild: only what
  was playing resumes, from where it stopped.

- **Sounds are plain WAV files** packed in `soundsL.zbd` (11 kHz, 8-bit) and
  `soundsH.zbd` (22 kHz), 999 of them, named in `sounds.zrd` by set: menus,
  briefings, mech voices, weapons. The briefing voices carry cue points
  (50 sounds have them); the briefing screen is called at each one (0x405e10
  sets the callback).

## Startup

- **The exe checks itself for stat hacks.** The very first thing the game does
  is a CRC-32 over two tables built into `Mech3.exe`: the mech roster (each
  mech's name, tonnage and class) and the BattleTech internal-structure table
  (structure points by tonnage for head, torsos, arms and legs). If the result
  isn't 0x00efd77d, someone has hex-edited the stats: the intro movies still
  play, but instead of the main menu the game goes to its "Exit State" and
  closes, with no error. Likely aimed at multiplayer cheats (startup.md).
  (Removed in the rebuild, with the other traps: protection.md.)
- **The CD check only looks for the first intro movie**, `video\c1.avi`, on
  any drive Windows calls a CD-ROM. Any disc or mounted image with that file
  passes; no label or key is checked. Without it: "Please insert the
  MechWarrior 3 CD!", OK looks again, Cancel quits.
- **The well-known 1.2 NoCD exe is also a bug fix.** Besides four jumps in
  the disc check, it stops a startup crash on fast PCs (a CPU speed test that
  divides by zero) and keeps a software-mode line from drawing outside the
  screen. Likely why some found it "more stable" than the original.
- **Hidden launch switches**: `-nofmv` skips the intro, attract and
  campaign-opening movies (not the MicroProse/Zipper logos), `-cdN` forces CD
  music track N in missions (N must be below the disc's track count), and
  `-seasick` turns on the hidden "FastMenu" option, which the menu screens
  read to turn off their motion (likely: named for players who got seasick).
  The switches only need to start right: `-nofmvx` counts as `-nofmv`
  (game-loop.md, startup.md).
- **Leave the main menu alone for a minute** and the attract movie plays.
- **The random mission music can pick the data track.** Without `-cdN`, on a
  disc with more than five tracks a mission plays a random track from 1 up,
  and track 1 of a game CD is data: that mission is silent (on a disc of 2-5
  tracks the mission number picks instead). In the rebuild there is no
  track01 file, so the same silence.
- **Turn a cheat on during a campaign mission and it doesn't count.**
  Invincibility, unlimited ammo or heat off, even switched off again, mean
  "Replay Mission" instead of "Next Mission" and no score added. The mark
  lasts until the next mission starts: each mission's start looks again at
  the three settings and clears it when all are off (`mission-frame.md`).
- **In multiplayer the host re-checks the locked rules every 51 seconds**;
  changing them in memory (say, unlimited ammo after the lock) ends the game
  a minute or two later as if the clock ran out.
- **Only one copy runs at a time.** Started a second time, the game finds the
  first window and brings it to the front instead. Launched from a multiplayer
  lobby, it closes the old copy instead.
- **The default mech is the Bushwacker**, set in the app defaults at startup.

## Settings

From the settings loader at 0x40c420 and the registry code around 0x5977d0
(details: `settings.md`).

- **The three hidden resolutions are real.** The mode a mission starts in is
  the registry value `InGameVMode` under
  `HKEY_CURRENT_USER\Software\MicroProse\MechWarrior 3\1.0`. The menu offers
  5, 6 and 7 (640x480, 800x600, 1024x768), but the game reads any number,
  and its mode switch also knows 8 (1152x864), 9 (1280x1024) and 10
  (1600x1200).
- **The menus' own resolution is never saved.** It is picked fresh at every
  start: 640x480 on a Pentium Pro or better, 320x240 doubled below that.
- **Your settings are only read if the installer's key exists.** The game
  opens both its user key and the machine-wide key
  (`HKEY_LOCAL_MACHINE\...\MechWarrior 3\1.0`) and reads nothing at all
  unless both open. A copied install that never ran the installer always
  starts at the defaults.
- **And they are only saved if Windows lets it write the machine-wide key.**
  The save creates both keys first; if the machine key is refused, nothing
  is written, not even your own settings. On Windows Vista and later the
  32-bit game is likely redirected to a private copy of that key, which would be
  why it mostly works there.
- **Settings are saved when the game quits normally**, from the
  application's destructor. A crash loses everything changed that session.
- **The detail settings are chosen by a little rule file**, `detail.zrd`,
  that compares your PC with thresholds: Lighting needs over 200 MHz,
  Perspective correction 166 MHz, high object detail over 233 MHz, the
  biggest software texture memory 63 MB of RAM. The "video memory" test it
  offers can never pass: it reads an empty slot next to the real value, so
  it always sees 0 (the shipped file never uses it). It also has an
  "about equal" test (within 2%) nobody used.
- **The hidden "FastMenu" option (the `-seasick` switch) defaults on below
  200 MHz**, so slow machines got the still menus.

## Loading a mission

From mission.cpp, 0x48faf0 and 0x490ab0 (details: `mission-load.md`).

- **The mission loads while you read the briefing.** The briefing starts a
  second thread at idle priority that loads the whole mission behind it, and
  checks between steps whether you left; that is why the briefing could feel
  sluggish on slow machines.
- **The "Texture Memory" setting has fewer real steps than it shows.** 12 MB,
  10 MB and 8 MB all cap the textures at 8 MB; then 6, 4 and 2 MB; "Max" uses
  whatever the card reports.
- **In software mode the mechs wear a cheaper texture set** (`mechtexS`, the
  palettised one); with a 3D card they use `mechtex`.
- **Every mission opens with an animation script, not code.** The start is
  `start_single_player_m<n>` (instant action `switch_ia_m<n>`, multiplayer
  `start_multiplayer_m<n>`, training `start_training_m<n>`) from the
  mission's `anim.zbd`, falling back to the name without the number. What
  happens in a mission's first seconds is data.
- **Multiplayer maps from the third on take their objectives from
  `mpinfo.zrd`**, the first two from `objectives.zrd` like the campaign.
- **Two in three campaign enemy mechs start the mission switched off.**
  The vehicle file marks each mech that starts powered down (106 of the
  168 AI mechs in the campaign and training missions), so they sit cold
  until the AI wakes them; most of operation 1's do, while some later
  groups start up and moving (c2 mission 3, c4 mission 1, c4b mission 4).
  Dominic starts c1 mission 4 powered down too (`mission-start.md`).
- **Lancemates join your lance after their first mission.** Where Dominic
  (c1 mission 4), Epona (c2 mission 4) and Alan (c3 mission 2) first
  appear, their files say you cannot configure them: they fight in that
  mission's own mech and loadout. From the next mission on they are kept
  by the campaign, with whatever you give them in the mech lab, and each
  mission's file only says where they start and what orders they follow.
- **An enemy mech without an AI path is never made.** A mech whose path
  number has no path file is skipped at mission start ("no mech net"),
  while a vehicle without one is made but has no driver. Every shipped
  mission has all its paths.
- **Training picks your loadout.** In the training missions the player's
  record chooses one of the mech's stock loadouts.
- **Gravity is in a data file**: `player.zrd` sets 40 on land, 9 in water
  and 3 in quicksand.
- **A team value can hold several teams**, 5 bits each (33 is team 1
  twice, 65 teams 1 and 2), so a vehicle could fight for two sides at once;
  the shipped files use only 0, 1 and 2.
- **The release exe calls itself "PRODUCTION"**: the loading text reads
  "MechWarrior3 (PRODUCTION) (1.2)", a build marker left in.
- **Missions in the same operation share one world load.** Going from
  c1m1 to c1m2 keeps the whole world in memory (terrain, animations,
  textures); only moving to another operation's world unloads it and loads
  the new one. The unload frees the animations twice over, harmlessly.
- **The sound system never forgets it was started.** Its shutdown leaves
  the "initialised" flag set, so the game could not restart sound without
  restarting the program; it only shuts sound down on exit, so it never
  matters.

## Playing a mission

From mission.cpp's frame 0x48f470 and its neighbours (details:
`mission-frame.md`).

- **The mission keeps the game's clock.** The game time every timer reads
  (trigger delays, weapon recycling, the AI's fire timers) is a counter in
  the mission object, started at 0 when the mission loads and moved on by
  each frame's step; the cleanup at the end puts it back to 0.
- **Exit Mission (Ctrl+Q) only works once the mission is over.** Before you
  win or lose it does nothing; after, it skips straight to the debriefing.
- **Losing gives you eight seconds.** When the mission fails, "mission
  failed" plays at once and the debriefing follows 8 seconds later (Exit
  Mission skips the wait).
- **Winning doesn't end the mission by itself.** When the objectives are
  done the game marks the mission won and plays "mission complete", and
  the mission runs on: the mission code's only ways to the debriefing are
  the loss's countdown and Exit Mission, so after a win you leave with
  Ctrl+Q (or another screen's way out, not traced).
- **Mission voice lines take turns by priority.** Only one line plays at a
  time: a new one waits out a line of higher priority but cuts off one of
  equal or lower priority. Objective lines have priority 0 and "mission
  complete" 2, so the win's line interrupts an objective's.
- **Instant Action's wave mode stops at wave 20.** In its endless and wave
  modes, winning a wave loads the map's objectives again for the next one
  (and records the best wave in wave mode), with no "mission complete" line;
  wave mode stops after the 20th wave and the mission then ends, the endless
  mode never does.
- **Training goes on lesson by lesson.** Completing a training mission's
  objectives moves it to its next lesson when there is one, silently; only
  after the last does it end.
- **In multiplayer, failing doesn't end your game**: your mech is put down
  instead and the game goes on.
- **Every mech starts a mission one centimetre above the ground.** The
  game finds the ground under the mission file's spot, looking from 100 m
  above it, and puts the mech 0.01 m over it.
- **Where a mech starts on stacked ground depends on how high its spot is.**
  When the game drops a mech onto its spot, it should take the highest
  surface no more than 10 m above the spot, but a slip in the code makes
  that "the spot's height plus 10, plus the spot's height again". A spot
  below zero over a bridge puts the mech under the bridge; a high spot can
  take a surface well above it. It never shows in the shipped missions:
  every one of the game's 768 starting spots (campaign, Instant Action and
  multiplayer) lands on the same ground either way, including the 55 that
  sit under a bridge or a roof. It could only bite when a mech is put
  back down where it stands (a mech swap). The rebuild fixes it.
- **Your mech starts every mission powered down, and the fade-in powers it
  up.** The game puts you in the world with the reactor off; the moment
  the opening fade ends it switches the reactor on, while the "power up"
  sound plays.
- **The campaign starts underwater.** In Operation 1's first mission your
  Mad Cat stands on the seabed under almost 5 m of ocean and wades ashore.
- **A destroyed section vanishes from the damage diagram instead of turning
  black.** The diagram colours each section blue, green, yellow or red as it
  takes damage, and asks for a black picture once the section is gone. No
  black pictures were ever shipped (not in MechWarrior 3, not in Pirate's
  Moon), so a destroyed arm or leg simply disappears from the outline.
- **No arrow points to an off-screen target that is neither friend nor
  foe.** The target display has arrows for an enemy and for an ally at the
  edge of the screen, and asks for a third set for anything else (a
  building, a neutral unit); that art was never shipped either ("othererrow"
  as the code spells it), so such a target gets no arrow. The game does
  ship arrows for nav points and mobile field bases that this display never
  uses.

## Multiplayer

From the network code (network-messages.md).

- **Your own PC decides whether your mech dies.** Each player's machine runs its
  own mech's damage and death and tells the others. A hacked client could likely
  shrug off hits.
- **The host re-checks the rules every 51 seconds** with a CRC-32 of the game
  settings block, and ends the game if it changed. Right after the check it may
  set bits in that same block, which looks like a deliberate way to end the game
  at the next check (likely).
- **If the host leaves, the game carries on**: a new host is picked instead of
  ending the match.
- **Fewer players, smoother play.** Each mech's position goes out about 7 times
  a second with 2-4 players, down to 3 with 8; the tables stop at 8 players.
- **Craters are shared.** The host checks each crater and sends it to everyone,
  so the terrain damage matches on every screen.

## Graphics

- **In software mode, screen flashes and fades miss the right edge.** The
  software renderer colours the screen four pixels at a time (MMX), and the
  screen's right edge is kept one pixel short, so on a 640-wide screen the
  last four columns stay their normal colour while the rest flashes or fades
  (0x558610, `render-frame.md`). On a PC without MMX the whole width is
  coloured. In Direct3D the colour is one quad over the whole screen.
- **A software-mode flash never wears off by itself:** the colour is kept and
  laid over every following frame until the game sets another (0x558610).
- **Fades and flashes in Direct3D are one see-through rectangle** drawn over
  everything, depth ignored, in the frame's last scene (0x57dd80, 0x57e380).
- **The software renderer draws with the stack pointer** (`blit-trick.md`). To
  fill a row of pixels fast, it points the CPU's stack at the screen and
  "pushes" colours onto it, two bytes at a time, backwards. It is quick on a
  Pentium, but anything else that uses the stack meanwhile writes straight into
  the picture. It is also why some modern wrappers (dgVoodoo) choke on the game.
- **The game picks the best texture set it can find.** For each texture
  library it looks for a numbered file first (`rmechtex16.zbd`, then 15, 14,
  ... down to the plain `rmechtex.zbd`) and uses the first one present. On a
  15-bit display every texture's colours are converted as they load, and green
  loses a bit of precision.
- **`lab.flt` in `mechlib.zbd` is likely never loaded.** Every model name the
  game builds starts with `mech_` (0x484760, 0x4847e0), so the one model
  without that prefix sits unused (mechlib.md).
- **All mechs share one set of 442 materials**, read once from `mechlib.zbd`.
- **In GameZ, an animation is a "script".** Every mech animation in
  `motion.zbd` is loaded as a `CZSIScript`, with keyframes for each body part.
  A loose `<name>.zan` file would replace one, a developer hook like the loose
  data files.

## Timing

- **The game clock went bad on machines left on for days.** It reads Windows'
  "milliseconds since the PC booted" into a 32-bit float. After about 4.7 hours
  of uptime the clock can only step in whole milliseconds, and after 10 days in
  64 ms steps, so frames start to judder. A PC that sleeps instead of shutting
  down got there easily. (The rebuild counts from game start: `fixes.md`.)
- **Game speed does not depend on CPU speed.** Everything is scaled by the real
  time each frame took, capped at 1/8 second a frame. The exceptions found so
  far are per-frame, like the lava heat above.

## Mission worlds

From the world files (gamez.md).

- **Each mission world is a memory dump.** `gamez.zbd` is the engine's
  in-memory pools of textures, materials, models and scene nodes written out as
  they were, free slots and stale pointers included.
- **Deleted objects are still in the files.** Free node slots keep the names of
  objects the level designers deleted without clearing; mission 1's world holds
  1,393 of them.
- **The engine's error messages go nowhere.** The GameZ error reporter
  (0x49bfd0) is an empty stub in the shipped game, so every engine error is
  silent; a broken node is just marked dead and loading carries on.
- **Field of view is capped at 80 degrees.** The camera setter clamps each
  view angle to 1.396 radians (0x4f3b90), whatever the data asks for.
- **Sound in the world files was never finished.** Reading a sound node logs
  `Reading sound node data: Must complete software.`, and none of the six
  worlds has one; the animation loader creates the sound nodes at mission
  load instead (anim.md).
- **Fog colour changes under 1% are ignored.** The renderer keeps its fog
  colour unless some channel moves by at least 0.01 (0x567eb0), and clamps
  each to 0..1 first, writing the clamp back into the world.
- **Each world file has exactly one light node.** The other lights are made
  by the animation loader at mission load (anim.md).
- **Worlds are loaded by a script.** Mission load runs a script whose
  `GameZReadZBDFile` command reads the world; the matching
  `GameZWriteZBDFile` survives inside a disabled `ifdef COMPILE` block, a
  leftover from the tool that built the files.

- **A typo in mission 1's world script.** The c1 build script says
  `Object3DSetScrllAlways` after finding the node `ocean52`; the command is
  `Object3DSetScrollAlways`, so that water surface never scrolls (interp.md).
- **The world compiler's commands are still in the scripts.** 385 lines call
  `LoadGameGen`, a command the shipped exe doesn't have; they sit in a branch
  that only runs if `gamez.zbd` fails to load, so the fallback could never
  have worked. `SetModelDirectory` runs every mission and is silently ignored.
- **There is a hidden "GameZ Interpreter" console** (0x5960a0) where you could
  type commands like `var DamageScale = 0` or `PrintTree`, but in 1.2 it is
  likely unreachable (interp.md).
- **Modding catch:** if any loose script next to the exe has a different file
  time from the one stored in `interp.zbd`, the game rejects the whole archive,
  so a modder has to extract all 44 scripts.
- **A mission script could change gravity.** MW3 added one command of its own
  to the GameZ scripts, `WorldSetGravity` (0x4107e0), and registers `gravity`
  as a script variable (`var gravity = 10` works too). The mech physics reads
  that value (0x417492 and others). No shipped script uses either, so a modded
  mission script could make a low-gravity map (likely; the physics is not
  ported yet).
- **`PerspectiveTexture on` does nothing.** Every mission's common script turns
  it on, but the setting it writes (0x74a3cc) is overwritten by every model
  draw and read nowhere. `Verbose` is the same: written, never read.
- **Turning a texture scroll off from a script crashes the game.**
  `Object3DSetScroll off` (or `...ScrollAlways off`) makes the engine write
  through a null pointer (0x596370). No shipped script does it; modders, leave
  scrolls on.
- **Every script word starting with "I" is accepted.** The interpreter has no
  command beginning with I and waves any such word through silently; anything
  starting with `SEQ` too. Most command names also match longer words:
  `CameraSetFOVwide 90 70` works as `CameraSetFOV`.
- **The scripts are otherwise clean.** On all six worlds every one of the 235
  `FindNode` lookups finds its object; the only words the game does not know in
  the live part of the scripts are `SetModelDirectory` and c1's typo above.
- **Most scrolling textures tick from one hidden object.** 170 script lines
  use `Object3DSetScrollAlways`, which lists the object under a node the
  interpreter makes called `ScrollAlways`; that node's action moves all of
  them together. The other 54 (`Object3DSetScroll`) scroll through each
  object's own action (likely only while that object is processed).
- **Two `%macros%` in one command share one buffer.** Each expanded argument is
  built in the same place, so a command keeping two expanded words as text
  gets the second one twice (`SEQAddChild %A% %N% ...` looks up the node named
  by `%N%`). Numbers are safe; the shipped scripts never hit it.

## Drawing the world

From the render path (render.md).

- **Every map has invisible mission boundaries.** Flat red and yellow models
  under `inout` mark each mission variant's in/out-of-bounds area. At
  mission load the AI reads the one it needs, then the whole set is deleted
  before the first frame, so nobody ever sees them.
- **Trees, docks and bridges carry invisible helpers**: loudly coloured
  (magenta, pink, white) shapes with zero opacity, at the trunk, canopy height
  or deck. Likely collision or targeting helpers the artists coloured to see
  them in the editor.
- **The fake shadows are see-through black polygons** under crates, tanks and
  buildings, not computed shadows.
- **Fog starts at the cockpit in hardware mode.** The missions ask for fog from
  500 to 750 m in the software renderer but from 32 m in hardware, and it is
  ground fog: thick below 80 m altitude, gone above 100.
- **The fog is always the colour of the sky band**, in every world.
- **The sun only darkens.** Lighting can make a surface darker than its
  texture, never brighter.
- **Hardware mode may never have shown the moving cloud shadows.** The code
  for the sun's drifting cloud pattern rounds with a trick that only works at
  full FPU precision, and Direct3D 6 puts the FPU in single precision, so the
  pattern freezes to one value (in c1, a constant 70% sun). Software mode
  should show it. Found in the emulator; still to be confirmed in play.
- **The same trick may turn surfaces black next to coloured lights** in
  hardware mode (textured surfaces lit by a coloured point light). Also
  emulator-only so far.
- **Copies of the same object share one lighting bake.** The game caches each
  model's sunlight per vertex, but the cache lives on the shared model, not on
  each placed copy, so every copy is lit as if facing the way the last-baked
  copy faces, until a light changes (c1: 678 models used more than once, one
  58 times). Found in the code; not yet seen in play.
- **Four effect textures are missing from every world** (`sizzle`,
  `laspulstrc`, `gaustrac`, `gaustrac2`): the hardware texture files lack
  them, so the game uses an 8x8 red and green placeholder.
- **Past the edge of the map the ground goes on.** The world's grid of cells
  wraps around: look out beyond the edge and the game draws the terrain
  pieces again (only the terrain, none of the buildings) as if the map
  repeated (render-scenewalk.md).
- **Objects with nothing next to them are never culled on their own**: the
  visibility test only runs for objects that have siblings in the scene
  tree, so lone objects are always sent on to be drawn.
- **The sky is a dome that follows you** (position only), drawn past the far
  plane so it never gets cut off.
- **Mechs cast real shadows** (render-shadows.md). Every mech within 100 m of you, and your own,
  gets a small picture of its silhouette, blurred, laid on the ground along
  the sun's direction and multiplied over whatever it falls on (up to seven
  shadows overlapping on one surface). The code that paints the silhouette
  rounds with a trick that needs full FPU precision; with the FPU cut to
  24 bits (as DirectX 6 can leave it) the picture comes out plain white and
  the shadows vanish. On Windows 11 with DDrawCompat they show (checked in
  play, 2026-10-20); whether they showed on 1999 Windows 98 machines is not
  known. The game's own code draws every mech's
  shadow: a 64x64 picture for yours, 32x32 for the others, covering a
  quarter to half of it in the mechs' standing pose (more for a low sun, as
  on the second campaign's maps and the training map).
- **How dark a mech's shadow is depends on the sun, the clouds and the
  distance.** The shade is the share of the sunlight that is direct (a map
  with strong ambient light gets pale shadows), lighter where the cloud
  pattern dims the sun, and from 50 m to 100 m away from you a mech's shadow
  fades to nothing; beyond 100 m it is dropped. The shadow's size never
  changes with distance, only its darkness.
- **A shadow only covers the mech's standing box.** The picture spans the
  mech's bounding box slid along the sunlight; any part that sticks out of
  it (an arm swung far out) makes that whole part vanish from the shadow
  rather than being cut off at the edge.
- **Weapon lights were meant to light up the ground around them.** The PPC,
  autocannon, LBX, Gauss, pulse laser, flamer and AMS lights, a flickering
  light and the cave lights of the training mission are flagged to cut the
  nearby ground into small triangles lit by the light's falloff. At the
  same reduced precision the colour of those triangles rounds to black.
  Also emulator-only so far.
- **Hardware fog is switched on by the shadow code.** Nothing else in the
  game turns Direct3D's fog on: it comes on once the first shadow has been
  drawn (normally your own mech's, on the first frame). On a video card
  without the blend the shadows need, shadows stay off, and so would the
  fog.
- **The original's 3D view is about 8% narrower than it should be.** The
  camera sees 80 degrees across and 60 up and down, but on a 4:3 screen
  that crams a little more width in than square pixels would hold: round
  things are drawn about 8% thinner than tall. Every player saw it this
  way, so the rebuild keeps it.
- **The hidden big resolutions would have squeezed a wide screen.** The
  game sets its sideways view angle by simple proportion: the up-down
  angle times width over height. At 4:3 that gives the intended 80
  degrees, but at 16:9 it would be 106.7 degrees, squeezing everything to
  three quarters of its width. The rebuild widens the view the correct way
  instead (96.4 degrees at 16:9).
- **The main menu's "green ripples" are not drawn, they are the screen
  pushed around.** Hovering a button starts a water ring that moves the
  pixels already there (the green background, other buttons) in and out
  along the radius by up to 7 pixels: three 12-pixel waves growing from
  the button's centre at 120 pixels a second, fading out in about half a
  second. The ring finds each pixel's distance with a bit trick on the
  float instead of a square root.
- **A heat spike can be outrun.** When "shutting down" sounds, the mech's
  throttle is cut and it only switches off once it has come to a stand.
  If the heat drops back to 90% before then (it takes a running mech
  several seconds to stop), the shutdown is called off. Only sustained
  heat shuts down a running mech.
- **The sun's lens flare only shows in software mode.** Every map has a
  light flagged for a lens flare (two on the third campaign's maps), and
  the maps load four flare pictures. In the software renderer the flare
  appears as four glows: one at the light and three on the line through
  the middle of the screen, full size up close and shrinking with
  distance. The Direct3D renderer collects the same flares every frame and
  then throws them away without drawing them, although the flare code can
  draw in Direct3D. So hardware-mode players never saw it.
- **Mechs never shadow themselves**, and shadows only land on lit, solid,
  textured surfaces: not on foliage with see-through textures, not on the
  fake black shadow polygons.

## Mechs: camos

From the mech definitions (mechdef-skins.md). The camo names are GUNMETAL,
SUNFIRE, DESERT, WASP, SLATE, JUNGLE, ARCTIC and SPECTRE.

- **With bullet holes off, a camo change is shared by every mech of that
  type.** A mech's camo is swapped onto the materials of its model. With
  the bullet-hole detail on (enough texture memory), each mech gets its own
  copies of the materials that can take holes. With it off, every mech of
  the same type shares one set, so the camo given to the last one built
  shows on all of them.
- **Every surface of a mech counts as the "mech" soil.** When a mech type
  loads, all its model's materials are set to the soil named "mech",
  whatever their textures, so a hit anywhere on a mech uses the "mech"
  soil's impact effects.
- **The Firefly has two WASP camos and no SLATE.** Its fifth skin is
  labelled `MSG_SKIN_VARIANTD` instead of `VARIANTE`, so both the fourth and
  fifth skins are named WASP; the second one uses the `_e` (Slate) textures.
  The data is certain; that the skin picker shows the name twice is not checked
  in game.
- **The Mad Cat's damage list is shifted by two.** Damage textures pair with
  skin textures by position, and the Mad Cat's damage list has two extra zeros
  before its six damage masks. So `mcat_d01`/`mcat_d02` get none, and
  `mcat_d03`/`mcat_d04` (64x64) get a 128x128 mask, which the size check
  0x474740 rejects (sizes from `rmechtex.zbd`, by the primary name). The
  Supernova has one extra damage item too; `supern_d03` (128x128) is paired
  with a 128x64 mask, which fits only its alternate `supernova18`. Whether the game logs and drops these at run time is
  unverified (it depends on which name the entry loads, +8 or +0xc). The
  rebuild drops the extra slots, so both show their battle damage as drawn.
- **Variant camos are only suffixes.** Every camo after the first is the
  primary list with `_b`, `_c` ... appended to each texture name; the files
  never list variant textures one by one.
- **Elementals never show bullet holes**: their damage list is all zeros.
- **A dead special case**: the texture loader treats an entry named
  `vult_d03_c` specially, but no file makes that name (the Vulture's is
  `vultur_d03_c`), and no texture library has it.
- **Five primary textures are missing from every library**: `bushwacker14`,
  `blkhwk16`, `cauldron01`, `cauldron08`, `owens13` (not in `rmechtex*.zbd` or
  `rimage.zbd`). Probably unused by the models; not checked.

## Mechs: firepoints

From the mech definitions (mechdef-parts.md).

- **Firepoint `radius` and `share` do different jobs.** `radius` is how far
  each missile of a salvo scatters from its firepoint; `share` is the
  sideways offset, random but chosen once when it is mounted, given to a
  second weapon on the same firepoint, so
  the two do not fire from exactly the same spot. Almost every mech has
  0.5 for both, so it rarely shows.
- **Elemental weapons do not spread.** The Elemental's arm firepoints have no
  radius or share at all (0), and its torso ones only a radius of 0.2.
- **TAG always fires from the first firepoint** in the mech's list
  (`firepoint01`, a right-arm point on every shipped mech), whatever the
  loadout, so losing the right arm loses TAG even when the equipment sits
  elsewhere (the Cauldron-Born's Alternate B, the only stock loadout with
  TAG, has it in the left torso). It is always weapon group 5 (F5).
- **A leftover `parts` lookup.** The reader asks every mech file for a
  `parts` entry that no file has, then ignores whatever it would have found:
  the same 13 part names are set for every mech. The left and right side
  torsos are not among them (their slots stay unnamed); whether anything
  reads these slots is not known yet.
- **Firepoints in the dfn name a part, but the game aims from the model.**
  The weapons find their firing part from the 3D model node with the
  firepoint's name (0x46cb80), not from the `part` written in the file; the
  file's part is only reached through the config item (0x473220).

## Mechs: loadouts

From the mech definitions (mechdef-configs.md).

- **The Bushwacker has four hidden training loadouts**, `training1`..`training4`
  (one with no weapons and three jump jets, one with only two machine guns,
  one with 2 tons of armor). Their names start with `train`, which sets a
  flag on the loadout (+0x1e). What the flag does (likely: the training
  missions use them and the mech lab hides them) is not traced.
- **The Sunder's Alternate B loses 240 LRM missiles to a typo**: its file puts
  `extra (120 120)` beside the LRM20's `ammo` instead of inside it, so only
  the first 120 are loaded (checked in the emulator).
- **The Elemental's armor does not add up**: 1 ton of standard armor is worth
  16 points and the file hands out 11. The game checks and writes an
  "ARMOR ALLOCATION ERROR", but error output is compiled out, so nobody ever
  saw it. Every other loadout adds up exactly.
- **Armor points per ton**: 16 standard, 19.2 Clan ferro-fibrous, 17.92 Inner
  Sphere ferro-fibrous; armor tonnage is rounded to the nearest half ton.
- **Heat sinks**: every loadout gets 10 free, plus one per listed sink. Single
  or double is decided by the last sink in the list, so a mix would count as
  all one kind (no shipped loadout mixes them).
- **The `max_speed` in every loadout's engine block is ignored**: nothing in
  the game reads it from a mech file.
- **Turning comes from the mech, not the loadout**: every loadout gets half of
  the file's `max_turn_rate` and the mech's acceleration values; the per-engine
  override the code allows is never used.

## Mechs: how a mech is built

From the mech's constructor and the launch (mech-build.md).

- **Every mech is built from two models.** Each part looks for its node in
  the full model and in the low-detail one (`mech_<name>_1`) and claims
  both, and the low-detail copy hangs under the mech just while its hit
  handler is set, so shots on either model count as hits on the mech.
- **A mech's height is its model's**: the box of the model, top to bottom
  (12 m when it has no box). Its target node (likely where others aim)
  sits at 65% of that height, its centre node at half.
- **Armor and internal structure follow BattleTech.** Each section's
  internal structure comes from a table by tonnage (a 100-ton mech: 31
  centre torso, 21 side torso, 17 arm, 21 leg); the head always has 3,
  whatever the tonnage. A section's armor is its loadout's points; one with
  no armor at all takes damage as if it had a single point.
- **Two "skis" under every mech follow the ground**: 12 m long (4 m for the
  Elemental), one under each foot's track, each with three probes (front,
  middle, back). They carry a hidden debug model the developers could show.
- **The torso's twist limits start at 100 degrees and the look at -12 to
  +20** until the mech's own file sets them.
- **A joystick counts only when the joystick option is on**: the launch
  picks the joystick control mode then, and keyboard and mouse otherwise,
  whatever is plugged in.
- **Unnamed mechs are numbered**: a mech made without a name is called
  after its type and a count of every mech made since the game started
  (`madcat07`). The player's is always `player`.
- **Every mech has its own copy of its model and its materials.** The game
  loads each mech type's model once, but each mech built from it gets a
  full copy, colours and textures included (one set per model piece), so a
  camo or battle damage on one Mad Cat never shows on another.
- **Instant action and multiplayer use different destruction effects.** A
  mech's death, disable and limb-loss effects come in two sets, and in
  instant action and multiplayer the game picks the "m" set
  (`madcat_mdestroy`, `madcat_larm_mdetach`, ...). The Elemental is the
  exception: it always uses the campaign's.
- **Footsteps and footprints are per mech and per ground.** Each mech type
  looks for its own step sound and footprint on each of the 11 ground types
  (`madcatDirt`, `madcatSnow`, ...) and falls back to the shared one
  (`soilDirt`) when it has none; prints on grass are drawn at 90% size.
- **A broken mech type stays broken.** If a mech's data fails to load, the
  game remembers the failure and does not try again while the mission
  lasts; if a mission loads before the type is forgotten, the original
  crashes (fixed in the rebuild).
- **Lights, sounds and a few other scene objects can't be copied.** The
  engine's copy of a 3D object skips them with "Function not yet
  implemented" messages still in the game: anything copied (mechs,
  wreckage, effects) carries only models, levels of detail and cameras.

## Mechs: the definition file and saved loadouts

From the mech definitions (mechdef-head.md).

- **The Strider has no recoil slide because of a typo**: its file says
  `recoil_slide ("1.5.0")`, which is text, not a number, so the game keeps 0.
  Its neighbours (Cauldron, Bushwacker, Champion) have 1.5.
- **Every mech's maximum engine is set to 400** right after it is read; the
  per-mech maximums in the files (315 for the Bushwacker, 290 for the light
  mechs) are thrown away. Whether the mech lab uses this limit is not traced.
- **The Elemental has no crouch height or engine range** in its file, so it
  gets the defaults (1.0, 10..400).
- **Saved loadouts are tamper-checked.** The mech lab's loadouts go to
  `configs\<mech>.mw3`, with a CRC-32 checksum. Edit one by hand and the game
  silently drops it at load. The game also re-checks weight (0.24 t of
  slack), armor points and jump jets (at most 2 for mechs up to 40 t, 3 up to
  65 t, 4 above). **Elemental loadouts are never checked.** (The rebuild
  drops the checksum: hand-edited loadouts load if they are legal.)
- **The Mad Cat gets half a ton for free.** The loadout weight check takes
  0.5 t off for the Mad Cat alone (0x425387), on top of the 0.24 t of slack
  every mech gets. Whether the mech lab's own weight display does the same is
  not checked.
- **Every mech's weight includes a fixed 3 t cockpit** plus engine shielding
  of 1 t per 100 rating, rounded up (a 300 engine adds 3 t).
- **A damaged loadout file can hang or crash the game** at load: a broken
  item count makes it read 4 billion times past the end, a broken name length
  makes it ask for 4 GB of memory. (Both fixed in the rebuild.)
- Your loadouts are written when the game frees the mech data (at exit),
  not when you leave the mech lab (as far as traced).
- **The mech lab has two hidden keys for sharing loadouts.** In the lab
  opened from a briefing, **Ctrl+X** saves the shown mech's loadout to a
  `.m3c` file of your choice ("Mech3 Configurations" dialog), and **Ctrl+I**
  loads one from a file into the shown mech's list of loadouts (even a
  loadout of another mech); if a loadout with the same name is already
  there, the loaded one gets a random number added to its name. They only
  work while the mech shown has a loadout. (In the rebuild the files are
  the `export` and `import` folders.)
- **The mech lab's loadout buttons.** "Add" is never enabled (its press
  does nothing anyway); Rename and Delete only for your own loadouts.
  Saving copies the shown loadout to the end of the list; if its name is
  taken you type a new one over it, and pressing Escape then throws the
  copy away. Renaming onto another of your loadouts asks to overwrite it;
  onto one of the game's own it refuses ("can't overwrite"). Loadouts the
  lab's store can't fill are marked in the list.
- **The mech lab's item duplication** (the speedrunners' trick): the lab
  checks that the store has each item of a loadout, one item at a time,
  not how many of it the loadout needs. A loadout with two of the same
  weapon passes with one in the store, and choosing it gives you both:
  the store just runs out at 0. (Kept as in the original.)
- **The mech lab writes your loadout file only on rename and delete**:
  saving a copy adds it to the list, and the file is written when the game
  frees the mech data (at exit) or on the next rename or delete.
- **The mech lab was built with three more tabs.** Its code still handles
  WEAPONS, EQUIPMENT and MISSION DATA tabs (it hides the first two and
  MISSION DATA on every page and disables MISSION DATA as it opens), but the
  shipped dialog leaves all three out: weapons and equipment go on the
  CRITICALS page instead.
- **"Sort by name" in the mech lab sorts by the mech's number** in the
  game's own list of 19 mechs, not by the name you see: the comparison
  looks the names up and then ignores them (fixed in the rebuild). "Sort
  by mass" sorts by maximum tonnage, ties by that number.
- **The paint scheme you pick in the mech lab is the whole lance's.** The
  lab's list always reads and sets your own (slot 0's) scheme whichever
  lance member is shown, and the mission start paints every lancemate with
  that same scheme (0x0049e5e0).
- **For a lancemate the chassis list starts with an empty entry**
  (message 0x23d) in a campaign, to leave that slot without a mech.
- **The mech lab never leaves on its own:** its frame's "done" value is
  never set, so ACCEPT is the only way out (Escape does nothing there).
- **The lab's item lists were meant to recolour the item under the mouse,**
  but the shipped dialog puts that colour (`FONT`) inside the arrow
  marker's entry, where the game never looks. So hovering an item only
  shows the little arrow beside it and plays the rollover sound; the text
  stays its colour.
- **Holding the mouse on a lab list's up or down arrow scrolls it** after
  half a second, then a row every 0.15 seconds.
- **The lab meant to show the first ten heat sinks as taking no space.**
  On the equipment tab the supply list has code to show heat sinks with no
  critical slots while the mech has fewer sinks than its engine holds (ten,
  BattleTech's engine-mounted sinks), but the test subtracts the ten before
  comparing, so it asks whether the mech has fewer than none. It never
  fires, and sinks always show their slots. (Kept as in the original.)
- **Ammunition in the lab always weighs "1.0"**: the supply list prints a
  plain 1.0 for every ammo row (you buy it by the ton) instead of asking
  the weight. So "sort by mass" on the ammunition tab does the same as
  "sort by item": the game's own weapon order. Sorting by quantity puts the
  unlimited items (Instant Action's store) in that order too.
- **The armour tab borrows the criticals page.** Choosing it hides the
  mech's parts and the list of what is fitted, and shows the armour panel
  instead; the engine's table shows only with the equipment tab, where the
  supply list is shortened to make room for it.
- **A mod with a wrong `version` in a mech file crashes the game** instead of
  skipping the mech. (Fixed in the rebuild: the mech is skipped.)
- Seventeen of the mech files carry a developer comment that the build tool
  kept as data: "torso limits: maxYaw minPitch maxPitch (degrees)", which is
  how we know the torso fields' names.

## Mission animations

From the animation files (anim.md).

- **Your cockpit glass cracks one mark per head hit, from six.** Every
  cockpit carries three cracks and three dings, hidden until the head is
  hit. Each hit on the head shows one of the six, each about one time in six
  (the game draws a new random number for each of five tests: 16%, 20%, 25%,
  33%, 50%, then the last). A crack also hides the whole pane under it and
  plays the cockpit crack sound at its spot; a ding just appears. The
  effect is one of the mission's animations, aimed at a stand-in cockpit
  stored in every mission world until your mech's own cockpit is built and
  takes it over (cockpit-damage.md).
- **Most cockpits have little animated screens.** Every cockpit but the
  Mad Cat's among those checked has one to three panels (cpit_ania..c)
  whose pictures flip through a loop of frames (15 and 8 on the Owens' two),
  set up by three small scripts when you launch. A screen only moves on
  frames it is drawn.

- **Copies of the same effect find their own object in order.** When several
  animations in a row are meant for objects with the same name (up to 58 in
  a row on c1), the game hooks the first to the first such object, the
  second to the second, and so on. If there are more animations than
  objects, it wraps round to the first.
- **The last animation in a mission cannot be a prerequisite.** When one
  animation waits for another, the search stops one short of the end of the
  list, so the very last animation is never found (0x50aaa3). No shipped
  mission depends on this. (Fixed in the rebuild.)
- **One missing object wipes all of an animation's prerequisites.** If an
  animation's "wait until this object is active" condition names an object
  that is not in the level, the game drops every condition of that
  animation, not just the broken one, and the animation can then fire
  without waiting. None of the six shipped missions trips over it (checked
  against every level), so it only bites in modded missions. (Fixed in the
  rebuild: only the broken condition is dropped.)
- **Every animation's reset script runs once while the mission loads**, and
  what it leaves is the starting state of the level (anim-postload.md).
- **Lights, sounds and smoke tied to animations start switched off**: in the
  first training mission that is 63 lights, 17 sounds and 58 smoke puffers,
  waiting for their animation.
- **94 "when shot" animations have 0 health**, so any hit sets them off, even
  one doing no damage.
- **The file remembers where the developers' PC kept things in memory.**
  Every anim.zbd still holds the memory addresses from the machine that
  built it (mech3ax uses them to tell the versions apart). The game throws
  them away and looks every object up again by name.
- **The list of source files is read and ignored.** Each anim.zbd starts with
  the names and dates of the text files it was built from; the game reads
  past them without looking.
- **Distance- and zone-triggered effects check only every few frames.** An
  animation that runs only near the player or in a zone tests that once,
  then waits 1 to 10 frames (a counter handed round in turn) before testing
  again, so such effects start or freeze a few frames late (anim-run.md).
- **Flying debris gives up after 15 seconds of falling.** A piece thrown by
  an animation with gravity (a torn-off mech limb, rubble) that finds no
  ground under it stops moving after 15 seconds in the air, wherever it is.
- **Debris lands softly or bounces.** On touching the ground a piece moving
  sideways bounces up by half its step; one falling straight is set on the
  ground. If it lands slower than it accelerates it stops; otherwise it keeps
  a fifth of its speed. Pieces can start a different sequence and sound
  depending on the surface (two special surface types).
- **A flying piece's first ground check can happen in the wrong place.** The
  matrix the game works out for a piece thrown "in its parent's frame" gets
  leftover memory as its position, and that matrix is then kept as the
  piece's world position, so in the original its next ground test looks
  under an undefined spot (fixed in the rebuild with the matrix code).
- **An "if" with several "else if" branches tests the first branch's object
  in every branch.** Conditions about an object (is the ground under it,
  is it below a height) always read the object named by the first "if",
  whatever object the later branches name.
- **Light animations cannot shrink a light to nothing.** A light's near range
  pushed below 0 jumps to 1, and a far range pushed below the near one is
  set one unit past it.
- **Every animation start is written down for the savegame and the network.**
  Each start, reset and connector call is logged with its arguments (1000
  entries, doubled when full), except effects added to the world for the
  run and effects without a name.
- **An animation clock never passes a day.** A looping animation's clock is
  pulled back to 86,400 seconds when it gets past that.
- **"-99" means "never restart while running".** An animation whose reset
  time is -99 ignores calls to start it while it is still running.

## Smoke and fire

From the puffer system, `gmod_puff.cpp` (puffers.md). Missile trails, muzzle
smoke, jump-jet plumes, burning helicopters, steam and the smoke of wrecks
are all "puffers": emitters that drop little textured squares.

- **Every puff is a flat square that always faces you.** It never turns or
  tilts; it grows (in every shipped effect it doubles in size over its life)
  and fades out evenly from birth to death.
- **Trails are measured in distance, not time.** A missile or vehicle trail
  drops a puff every 0.75 to 12 metres it moves, so a fast missile leaves as
  dense a trail as a slow one, and a parked one leaves none. Muzzle smoke
  comes out five times a second. A jump of more than 200 metres in one frame
  (a teleport) leaves no line of smoke behind.
- **Puffs between frames are aged to match.** When several puffs come out in
  one frame along a path, each starts as old as it would be had it left at
  its own point on the path, so trails stay smooth at low frame rates.
- **Smoke fades with distance** (most effects between 400 and 600 metres,
  some from 80) and a puff less than about two pixels across is not drawn at
  all.
- **There is a wind, and it wanders.** Every mission has the same random
  wind: no steady direction, a speed that wanders between 0 and 13 m/s and
  a direction that drifts about 10 degrees a second; when the speed would
  drop below zero it turns round instead. Only smoke with "friction" feels
  it (muzzle smoke, missile trails, black smoke, boilers, sparks); fire and
  jet plumes fly straight. The wind is never reset between missions: a new
  mission starts with the old one's gusts.
- **At high frame rates the smoke stops drifting.** The game slows smoke
  toward the wind with a lookup table that rounds the frame's share down to
  steps of 1/51. At 60 frames a second that rounds muzzle smoke and missile
  trails to "no slowing at all", so they ignore the wind and keep their
  launch speed; at 30 fps they slow and drift. Above about 90 fps no smoke
  drifts. The wind's gusts also change per frame, so they change faster the
  faster the game runs. The rebuild makes both work per second (as at 30
  fps), so the smoke drifts the same at any frame rate.
- **There is no limit on smoke.** Every puff of every effect is kept in one
  list until it dies; a busy battle simply makes it longer.
- **Unused features.** The engine can colour and tint smoke over its life,
  animate its texture over time, release several puffs at once and fade
  smoke in as it moves away from the camera, but no mission uses any of
  them. The fade-in has a typo that would make the smoke vanish up close
  instead (fixed in the rebuild).

## Data files

- **Every setting lives in `reader.zbd`**, 178 small files of nested lists
  (mechs, weapons, sounds, HUD, briefings) (`zreader.md`).
- **The data lookup can't tell names from values.** Asking for a key finds the
  first matching string anywhere in a file, even if that string is someone's
  value. And when the match is the last item of a list, the game reads whatever
  happens to be next in memory.
- **The code knows two cameras no mech uses.** Each mech's data can set up
  six camera kinds (0x430e70): `tether`, `attach` (the cockpit), `area`,
  `mcbtether` (likely the mobile field base), `zippercam`, named after Zipper
  Interactive, and `godseye`. No data file in the game defines the last two.
- **Some keys never match the data.** Comparing every key the exe looks up
  with every string in the 55 reader files finds 118 keys the data never
  sets. Most are optional settings, but some are spelling mismatches: the
  weapon code reads `BOUNCES` while `weapons.zrd` says `BOUNCE` (always 0, so
  harmless), and the sound code's `MED` (above).
- **Loose text files override the archive.** The game first looks for a
  developer text version of each file in `data\common\zrdr` and only falls back
  to `reader.zbd` when there is none. None ship with the game, but the hook is
  still there.

## The AI

- **Every mission ships its AI as small text-like files.** Each mission's
  reader archive holds the AI paths (`net_NN`, 308 of them in all), regions
  (`reg_*`, 393: areas such as the mission boundary drawn on your map,
  `reg_inside_*` and `reg_outside_*`), one record per vehicle (`aiv.zrd`),
  the scored targets (`targets.zrd`) and the mission's triggers
  (`valves.zrd`) (`ai.md`).
- **Triggers are named on/off switches.** The game calls them valves: a path
  node can wait for one, set one, or open only when several are set; a
  target sets one when its health drops below a value (the radio lines when
  you damage and destroy a mission objective come from these).
- **How far an enemy sees is in its file.** Each vehicle's record gives its
  sight and attack ranges (the first numbers after `params`); the AI takes
  the nearest contact of the side it wants within its sight range.
- **Game intensity stretches an AI delay.** One of the AI's timings is
  multiplied by the Game Intensity setting each time it is set.
- **Alan may be silent in two missions.** Your wingmen's radio voice is picked
  from their name when the file gives none, and Alan's name is misspelt in
  the first Instant Action map of the first world (`Alan_Mattilla`) and in
  mission 6 of operation 3 (`Alan_Matilla`), so his voice lines likely don't
  play there. The rebuild recognises both spellings.
- **Two Instant Action enemies forgot their orders.** In two Instant Action
  maps on the c4 world one enemy's order list lost its `vlist` label in the
  data, so the game ignores the list and that enemy follows no scripted
  orders. The rebuild reads them.
- **Regions are invisible walls of the AI's mind.** A region file is a
  closed loop of points; the game turns it into a convex area, and a
  vehicle's orders can set a trigger when it is inside or outside one. The
  loop must run clockwise seen from above (x to the right, z up) or nothing
  is ever inside it, and only each point's first link counts.
- **One mission cannot be failed by leaving it.** In mission 6 on the c4b
  world the player's orders watch regions named `..._c4m7`, but
  the mission ships them as `..._c4bm6` and `..._c4bm7`, so the trigger that
  fails you for leaving the mission area never fires, and the map likely
  shows no boundary there. The pair it meant is `..._c4bm7`: every mech of
  the mission starts inside it (the region files are numbered one mission
  ahead; `..._c4bm6` is two small unrelated areas). The rebuild reads it
  so, and leaving fails the mission.
- **Each mission starts by checking the last one's triggers.** Your own
  mech carries the mission's player triggers (the boundary, dialogue
  regions and the like), and the game keeps them from one mission to the
  next: when it places you in a new mission it runs the old mission's
  list once, over switches it has already thrown away, before it hands
  you the new one. Usually harmless (the memory is still there), a crash
  risk in principle; the rebuild clears them at each mission's end.
- **Mission 5 on the c4b world can't be failed by leaving it either.** Its
  boundary trigger sets a switch named after the region
  (`reg_outside_c4m5`), while the failure watches `outside_c4m5`. The
  rebuild sets the one the failure watches.
- **Mission 4 on the c4b world fails you for entering a zone, not
  leaving.** Its "outside" region is a place away from where everyone
  starts, and its trigger fires when you are inside it.
- **Four region files are dead.** In c1's first mission one region was
  saved as an AI path instead (`reg_firefly_02_go`) and its replacement
  file name is misspelt (`reg_friefly_02_go`, which calls itself
  `reg_firefly_02_go`); in c3's mission 2 two regions are AI paths too.
  Nothing uses them, so nothing is lost.
- **A vehicle's first number is the path it walks.** The first number of
  each record in `aiv.zrd` is the number of the AI path (`net_NN`) the
  vehicle's pilot follows; 0 or a path that does not exist gives it the
  built-in default.
- **Paths are compiled into little programs.** At mission start every
  path file is turned into a list of steps: each point becomes a "go
  here" step, and each of its settings (speed, waits, triggers, scans,
  may-leave, jumps, sounds, gates) becomes a step on every link out of it,
  so a pilot meets them on the way to the next point. Settings can also
  sit on chosen links only: a path entry without a position, listing a
  point and some of its neighbours, puts its settings on those links.
- **A trigger on a path point is a switch.** A point that names a trigger
  (`valve`) without a value opens its last link only while the trigger is
  on and all its other links only while it is off, so the mission flips a
  patrol onto another route by setting one trigger. With a value given,
  every link opens at that value.
- **One AI path points at nothing.** In the c2 world's mission 3, path 13
  is a single point linking to a point that does not exist; the original
  reads leftover memory there. The rebuild takes it as no link.
- **Every pilot starts with one order: its path.** As a mission starts each
  AI mech and vehicle gets a pilot that copies its record and starts with a
  single order on its path network (a mech whose path is missing is not
  made at all, a vehicle is made without a driver).
- **Enemies get a radio too.** A pilot with a radio voice (the lancemates'
  `dom`, `alan` and `epona`, or any record's `sound_tag`) loads its sixteen
  radio lines (call for help, engaging, low ammo, ...) only if your mech
  counts its team as an ally, but at mission start your mech does not exist
  yet (it is built at launch), so every voiced pilot made then gets its
  lines, enemies included. Lancemates who join at launch are checked
  against your mech.
- **A trigger's "shutdown" wakes mechs.** valves.zrd's shutdown
  action powers a mech up or down by its second number, and every one of
  the 186 in the shipped missions says 0: up. They are how the mechs that
  start a mission powered down come alive (c1 mission 1's Owens wakes on
  the trigger `bramp`, which also starts the trucks and a radio line).
- **"Mission complete" triggers do nothing.** A trigger can name
  `missioncomplete`, but the function it calls is empty in the released
  game (`missionabort` works); `missionreset` is not even made. No shipped
  mission uses them: missions end through their objectives.
- **Delayed triggers copy the value.** `delayupdate` fires on any change of
  its trigger, and after the delay sets the other trigger to the value the
  first had when it changed; a trigger that flips twice inside the delay
  sets the other twice.
- **A delayed trigger can swallow the next one.** When the only pending
  delayed trigger fires and starts another delayed trigger, the game drops
  the new one. In operation 3 mission 2, `ambush_powerup_down_a` starts the
  three 10-second radio lines ig3214, ig3215 and ig3205 this way: when
  nothing else is waiting at that second, they never play (likely; the
  rebuild keeps them).
- **"Any" and "all" triggers re-fire.** A trigger defined as any/all of a
  list of others runs its actions every time one of them changes, even when
  its own any/all answer stays the same: a sound on it can play more than
  once.
- **A trigger loop crashes the game.** A trigger told to update itself, or
  triggers that update each other in a ring (an any/all trigger over one
  it updates included), set each other off for ever until the game runs
  out of stack and crashes. None of the shipped missions has one; a
  hand-made mission could (the rebuild runs each trigger's actions once
  per change).
- **Operation 2 mission 2 can destroy you outright.** Its triggers
  `destroy_player` and `destroy_dominic` blow up your mech and Dominic's
  (likely the mission's scripted failure).
- **The Owens has no hands.** Its model's arms end in the guns, so it is
  built with 11 parts where most mechs have 13.
- **AI pilots plan by straight lines.** When a pilot needs a route to a
  point of its path network, it picks the shortest one measured in straight
  lines from point to point (from where it stands), ignoring hills and
  water between them. Of two routes the same length, the one found first
  (by the order the points list their links) wins.
- **Some links are only for some pilots.** A link marked `jump` is taken
  only by a mech with jump jets, a `gate` link only by the vehicle the
  gate names, and a link with a trigger only while the
  trigger has the right value; the route finder leaves the others out.
- **Pilots re-plan only when their way shuts.** A pilot keeps its route and
  checks at each point that the rest of it is still open (a trigger may
  have changed); only then does it search again from where it is.
- **The route finder tries every route.** It walks every loop-free route
  through the network, giving up on one only once it is no shorter than
  the best so far. On today's machines that takes milliseconds even in the
  biggest networks (246 points).
- **There was an AI tester in the game.** A developer dialog for steering a
  unit by hand ("Direct", "Zig Zag", "Circle", "Jump", "Random Move") and
  picking its target ("player", "nearest enemy") is still in the exe behind
  the debug menu, along with an in-game editor for the AI paths that could
  save them back to files.
- **"Target next enemy" picks anything that isn't a friend.** The E,
  Shift+E and Ctrl+E keys are written as "not an ally", so they also stop on
  neutral things in sight (team 0, such as some buildings); W, Shift+W and
  Ctrl+W take only allies. Each key first returns to the last enemy (or
  ally) you had selected, if that isn't your current target.
- **Your sensors are nearly unlimited in Instant Action and multiplayer.**
  Your own mech's sensor range is capped at 1000 m in the campaign and
  training, but set to 50,000 m in Instant Action and multiplayer.
- **Only your C3 shares contacts, and only if you carry the master.** With
  a C3 computer (`c3`) on your own mech, every allied mech carrying C3 or a
  C3 slave adds what it senses to your contact list, all of it counted in
  sight. A C3 slave alone on your mech does nothing, and an enemy's C3
  never helps the enemy: the game only ever builds the player's network.
  Allies do not otherwise share what they see.
- **Sensors see through walls, not through power-downs.** Every vehicle
  senses everything within its range (1000 m in the campaign) whatever is
  in between. A powered-down (or powering-down) mech stays on the list but
  is never "in sight", so the target keys skip it, unless the one looking
  carries an active Beagle (BAP) and the hidden mech has no ECM. A mech
  shut down itself sees nothing in sight. Targets whose `radar` flag is off
  in targets.zrd (such as c1 mission 1's satellite dish) are never in sight.
- **Passive radar doesn't hide you from the AI.** The radar mode key
  (Alt+R) cycles 1000 m, then 500 m "passive" when at the top, 500 m, 1000
  m... Passive only stops your own Beagle and hides you from other passive
  radars; the AI never switches to passive, so it sees you as before. With
  C3 the cycle goes up to 2000 m, but the campaign caps the sensors at
  1000 m anyway, so the extra range never shows.
- **The lance lines up in a fixed order.** At the start of a mission the
  game sorts every vehicle: you first, your side next with your lancemates
  under orders ahead (Dominic Paine, then Epona Rhi, then Alan Mattila),
  then everyone else; the stealth object last.
- **Keep shooting a friend and your whole side turns on you.** If you keep
  hitting an allied mech for more than 3 seconds without a 3-second pause,
  every allied mech starts treating you as an enemy (though they stay
  "allies" to your targeting keys).
- **A lost target hands over to the nearest.** When your selected target
  leaves sensor range, the sensors pick the nearest one in sight instead:
  the nearest friend if the lost one was a friend, else the nearest
  non-friend.
- **Mission targets take their health from their destruction animation.**
  A targets.zrd object without its own `health` gets its hit points from the
  animation that plays when it is destroyed; one whose destruction animation
  is not on its node is not a target at all. The "hit" and "down" radio
  triggers (`health_below`) fire from the highest threshold down as its
  health drops, all at once when it is destroyed. If two objects share a
  name, only the first gets the triggers.
- **Old formats still load.** The vehicle reader also understands an older
  layout of the AI settings (tags p3, p4, p5, p7) that no shipped mission
  uses, and complains if a file mixes it with the new one.
- **Enemy pilots can be told to give up a chase.** Read from the code, not
  yet seen in play: before an AI pilot breaks off its path to attack, it
  checks switches in its vehicle record: whether it may fight at all (some
  only after a delay), a leash (too far from its home point and it will not
  start a fight), a health floor (oddly, only checked until the first time the
  pilot is found above it: a pilot healthy when it first weighs a fight never
  checks it again, one already hurt stays out until repaired), a time limit on each attack, and how much damage it takes before
  giving an attack up. The time limit scales with the game's intensity
  setting (`ai.md` 17.3).
- **Your lancemates shoot first, but never chase.** An allied pilot picks
  its own target all the time: any enemy (or neutral) within its weapons'
  reach, scored by distance, facing, damage and a clear line, whether or
  not anyone has attacked. What it never does on its own is walk toward a
  fight: every lancemate's file has the "may engage" switch off, and none
  of your orders turns it on. "Attack my target" only makes it follow that
  enemy, and it still shoots whatever scores best, usually that one once
  it is the nearest. Being shot does not change any of this: a hit only
  gets you a radio line (a call for help when you are more than about 22 m
  away, "friendly fire" when it was you).
  Checked in the rebuild with Dominic in operation 1 mission 4: an enemy
  60 m in front of him draws his fire at once; at 150 m behind a rise he
  aims and stands there, but on an open line he fires from 400 m and more.
- **The lance command mode freezes your mech's legs and guns.** While it
  is open (the command key), the game switches to the overhead "area"
  camera, the number keys 1-4 pick lancemates, and your mech neither
  jumps nor fires; it keeps the throttle and turn it had, and gets them
  back as they were when you close the mode.
- **The command mode can send your lance to a spot on the ground.**
  Every mission quietly makes an invisible "command target" marker. With
  the command mode open, pointing at open ground moves the marker there,
  and the order goes to every lancemate you picked at once. Ordered on the
  marker, "enemy only" makes the spot count as that lancemate's enemy and
  "ally only" as its friend, so it heads for the place as it would for a
  real unit.
- **In the command mode your trigger points instead of firing.** With the
  lance command panel open, a click of the fire button (on letting go) picks whatever is
  under your reticle (or under the mouse on the mission map, when it is
  shown): it becomes your target if you have it on sensors, and the panel
  names it. Point at bare ground and the hidden command marker jumps to
  that spot instead. Pressing an order's line on the panel waits three
  frames, gives your camera back, then sends the order to every lancemate
  you picked and closes the panel.
- **Your mech handles differently with the command panel open.** While
  the lance command panel is up, the game drives your mech with its own
  copy of the controls: the joystick is ignored (the mouse still looks
  around), torso twist from the keys is slower (1 instead of 1.5) but
  pitch and twist settle faster, and the zoom key opens the zoom box
  without its click.
- **No two fights play out quite the same.** The game seeds its random
  numbers from the clock when a mission starts, and the AI picks each
  volley at random among the weapon groups that can reach. So the same
  mission, played the same way, has the enemy fire different weapons,
  run at different heat and so move at slightly different speeds each
  time; small differences that grow over a long fight.
- **Your lancemates shoot buildings but never walk to them.** A lancemate
  (or an enemy) fires on any hostile structure it can see and reach, a
  hut, a tower, a satellite dish, but the AI only ever walks towards mechs
  and vehicles: a building in sight but out of reach is ignored until the
  pilot happens to come near it.
- **The command panel only lists lancemates you have seen.** A lancemate
  under your orders gets a line on the command panel only once you have
  had it in your sensors at least once; until then its place is empty, and
  the orders that need a pick stay dark. "Repair" lights only with exactly
  one lancemate picked and a repair bay still standing.
- **A fallen lancemate's mission triggers become yours.** Some mission
  switches are thrown when a particular pilot reaches a place. When an
  allied pilot dies, the game hands every one of its place triggers to
  you, so the mission goes on when you get there instead.
- **"Follow me" and "attack my target" aim at different things.** The lance
  order keys either act on the lancemate's view of you (follow, form up,
  stop) or on a fresh view from the lancemate to your current target
  (attack and the like). The order goes to the lancemate you last
  selected (your saved target, else your last ally picked).
- **What your lancemates say, and when.** Their radio has sixteen lines;
  the game itself (not the mission scripts) uses: "low ammo" and "out of
  ammo"; a call for help when an enemy hits them and you are more than
  about 22 m away; "friendly fire" when you hit them; a kill line when
  they destroy something that is not on your side; and "ejecting" when
  they die. Each line can carry a minimum delay before it repeats. The
  rest are answers to your orders.
- **Everyone starts a mission "just hit".** The game remembers when each
  mech was last hit, starting from zero, and the mission clock starts at
  zero too, so for the first second every mech counts as having been hit
  a moment ago. A pilot awake that early would dodge or fire triggers as
  if shot; in the shipped missions they all wake later.
- **Your own lancemates' splash can save you.** The three-second count of
  friendly fire starts again whenever someone else hits the same pilot,
  his own missiles' splash on a close target included.
- **Shoot your own lancemates for three seconds and the whole map turns on
  you.** The game counts a run of hits from the same attacker (it restarts
  after a 3-second pause). When you keep hitting an ally for more than 3
  seconds, your team is made the enemy of your allies (every allied unit
  the game lists after you, your lancemates among them), so they turn
  their guns on you (and, as written, on each other).
- **Pilots without a chosen target may not pick one to attack.** Read from
  the code: when an AI pilot weighs an attack and has no target selected,
  it takes the nearest enemy only if it has not already looked one up that
  frame, and the usual sight check has. So most pilots attack only what is
  selected for them (`ai.md` 17.1).
- **A pilot that cannot see you stands still.** When its sight check fails
  and whoever is attacking it is out of range, an AI mech skips its whole
  update for the frame: it neither moves nor animates until something
  comes into sight (a cheap way to idle distant enemies).
- **Mission switches tripped by a pilot's own state.** Each AI vehicle's
  record can carry triggers that flip a mission switch (a valve) the moment
  its pilot is hit, runs hot, drops below a health or ammo level, has an
  enemy within some range (one needs a clear line of sight too), or enters
  or leaves an area. That is how a mission can react to, say, an escort
  taking damage (`ai.md` 17.5).
- **Pilots stand down when the mission ends.** One switch turns every
  pilot's thinking on when the mission starts and off in the cleanup; off,
  they only stop and settle (`ai.md` 17.8).
- **How an enemy walks its path.** An AI mech follows a "carrot" sliding
  back along the line from where it last was to the next waypoint. It turns
  on the spot until it faces within 45 degrees of it, then throttles up;
  its turns ignore anything within 5 degrees and are never gentler than a
  tenth of full. It counts a waypoint as reached once the carrot is there
  and it is within its own stopping distance (its speed over its
  deceleration, about 15 m for c1m1's Owens), so it rounds the waypoint
  rather than touching it. Behind the carrot it backs up instead of turning
  around (`ai.md` 17.4a).
- **Deploy points snap a pilot into place.** At a "deploy" waypoint (288 of
  them in the shipped paths) a pilot walks within a metre, waits until it
  has stopped bobbing, turns to face away from the point the waypoint names
  as its target, then is slid the last stretch at half a metre a second and
  set exactly on the spot. Only while the switch `mcbdeploy` is on (it is
  reset to off at every mission start) does it stay there, setting its
  own switch; otherwise it carries on at once (`ai.md` 17.4a).
- **The unused "backup" waypoint.** The AI paths can mark a waypoint as
  "backup": the pilot turns its back to it, reverses in, then turns round
  before going on. No shipped mission uses one, and the original turns
  toward a direction it never sets (whatever was left in memory); the
  rebuild uses the way the pilot came (`ai.md` 17.4a).
- **Waypoints switch a pilot's habits.** The commonest marks on the AI
  paths (over 5,000 of them) are "scan" and "canleave" points: passing one
  turns a pilot's habits on or off for the rest of its walk: whether it may
  break off its route to attack, and whether it dodges when hit or now and
  then at random. So one enemy can patrol peacefully until it reaches a
  certain spot and turn aggressive from there (`ai.md` 17.4a).
- **Enemies won't shoot through their friends.** Before each shot an AI
  pilot checks its contacts, nearest first, for anything inside a cone
  around its line of fire: 10 m wide out to 30 m, then narrowing to 10 m
  at 200 m. The first thing in that cone blocks the shot unless it is the
  target itself. The trigger is only pulled while the weapon has ammo, has
  reloaded, and (for missiles) the last salvo has had time to fly.
- **How an enemy picks its target.** Each time it looks for a shot an AI
  pilot scores what its sensors see, nearest first, and takes the lowest:
  the score is the squared distance (anything within 40 m counts the same),
  made worse when the target is off to the side or above or below, doubled
  if it cannot turn to face it, lower the more damaged and the hotter the
  target is, and ten times worse without a clear line. So enemies prefer
  close, wounded, overheating mechs in front of them. Allies, anything
  within a metre, anything too steep and anything past its range are
  skipped. Finding nothing it widens its sensor range; finding something
  it narrows it to just past that target (`ai.md` 17.7).
- **Enemies lead their shots.** An AI pilot aims where its target will be
  when the shot arrives, from the target's speed and its own, for every
  weapon but beams and instant-hit ones (those aim straight at it). On
  the ground a mech's speed is judged from its throttle and facing, not
  from how it actually moves; in the air from its real velocity. Its
  torso then turns toward that point within its twist limits, and the
  shot waits until the torso faces it closely enough (`ai.md` 17.7).
- **Enemies close to a set range, and back off if you get too close.**
  When an enemy goes for you, it drives straight at you until it is within
  its pilot's attack range (80 m for c1m1's Owens), and if you are inside
  its minimum range (60 m for the Owens) it reverses away while facing
  you. Only then does it switch to its fighting style, if its pilot has
  one. It gives up a chase on a leash: c1m1's Owens turns back once it is
  130 m from where the fight began, and any fight of its ends after 10
  seconds (on medium difficulty); it attacks again some 15 seconds later
  (`ai.md` 17.2, "The fight").
- **Enemies get out of your way.** An enemy that is not fighting you keeps
  clear of you once you are within about 40 m (c1m1's Owens). While you
  close in it swerves aside; if you get closer than about 27 m it backs away
  facing you when you are in front of it, drives off when you come up
  behind it, or runs alongside when you are beside it, and it keeps doing
  so for as long as you push it. Once you are more than about 44 m away it
  stops minding you. In a test the Owens backed away 135 m before the
  player, walking at it head on, let it go (`ai.md` 17.2, "Its run").
- **After a fight, enemies walk back to their patrol.** When an enemy
  breaks off, it heads back to the nearest point of the route it was
  walking, slowing so it can stop in time, and is done once it is within
  its pilot's approach range of it (80 m for c1m1's Owens); only then does
  its wait before the next fight start. If the mission boundary pushes it
  back, it gives up the walk (`ai.md` 17.2, "The attack").
- **Enemies never alpha strike.** The AI was written to fire everything at
  once when its pilot rolls for it and the total heat fits, but a slip in
  the code (each weapon doubles a running total that starts at 0, instead
  of adding to it) means the totals never grow, so that choice never comes
  up. Instead an enemy nearly always fires a whole weapon group, any group
  that is in range and within its heat budget, and fires a single weapon
  only when no group qualifies. The hotter an enemy runs, the smaller its
  heat budget for a burst, and past about two thirds of its heat scale it
  holds fire altogether, even with weapons that make no heat (`ai.md`
  17.7, "The volley").
- **Enemy fire sweeps across you.** An AI pilot never aims dead on: it
  picks a point beside you, across its line of sight, and lets the aim
  drift back over you and out the other side, picking a new point when it
  strays too far. How wide depends on the pilot, the range (about 2% of the
  distance for an average pilot) and the difficulty: on high the error is
  0.6 of medium's, on low 1.2. A targeting computer or fire control system
  on the enemy mech changes it by a per-pilot factor (`ai.md` 17.7).
- **Between bursts an AI mech looks around.** Resting after a burst, an
  enemy's torso sweeps: behind it for 5 s, to its left for 2.5 s, to its
  right for 5 s, and round again, until it picks its next shot.
- **AI pilots prefer to keep going straight.** At a waypoint with several
  ways on, a pilot picks one at random, weighted so that a way straight
  ahead is a hundred times likelier than one straight back the way it came
  (`ai.md` 17.4a).
- **A dead-end waypoint makes a pilot wait 5 seconds.** A pilot that reaches
  a waypoint with no open way on (every link closed by a valve, gate or the
  like) stops and asks again every 5 seconds (`ai.md` 17.4a).
- **Valve links count in whole numbers.** The weight a pilot gives a way
  through a valve or a valve union is cut to a whole number. It changes
  nothing in the shipped game, because every waypoint weighs the same round
  1,048,575, but it would round away any fraction a modder's weights had.
- **Empty weapon groups.** A weapon group with no weapons left in it (or none
  picked) gave the HUD a "has ammo" answer that depended on where the game
  happened to keep its weapon list in memory; the rebuild says no ammo.
- **AI mechs can circle their target.** An enemy whose attack mode is 1
  (a per-vehicle setting in the mission's AI files) closes in, then orbits
  at its preferred distance, steering along the circle and correcting in or out. It turns
  round to circle the other way when it drifts too far along the circle,
  or when the point a few metres ahead would take it out of bounds (the
  mission's no-go zones). It only throttles up while it faces within about
  60 degrees of where it wants to go. (`ai.md`, mode 1)
- **Mode 2 orbits and turns round whenever it is interrupted.** An enemy in
  attack mode 2 also circles at its preferred distance, but steers by the
  line to the target instead, and every time something else takes its
  attention for a moment (another order on top of the attack, then gone),
  it comes back circling the other way.
- **The combat manoeuvre: brake, face, hop.** When an enemy that reacts to
  being hit (or that scans while aiming) "manoeuvres", it first brakes to a
  stop, then turns to face the nearest enemy (or whoever just shot it, or
  anyone it can see), aiming where its selected weapon would lead the
  target, and fires its jump jets in hops while it is below a set height,
  for a few seconds at random. It lets its jets cool below 10% fuel and
  uses them again past 60%.
- **Everyone counts as freshly hit for the first second of a mission.**
  The record of the last hit starts at time 0, so in the first second an AI
  pilot that reacts to hits sees itself as just hit and may manoeuvre at
  once, before anything has fired.
- **How AI pilots jump.** An AI jump is planned once, from where the pilot
  stands to the waypoint it lands on: a full-jets climb, a coast, a glide,
  a coast down and a full-jets landing burn, each timed from the height to
  clear (the waypoint's clearance added on top) and the world's gravity. A
  pilot too slow to cover the gap at the planned speed glides longer. The
  pilot steers at the landing spot all the way until the landing burn, and
  any phase ends early once its feet touch down. (`ai.md`, the jump)
- **Each mission has hidden no-go zones for the AI.** Every campaign map
  file carries, for each mission played on it, a set of invisible red
  shapes (4 to 62 per mission) that the AI pilots keep out of, and a
  larger set for Instant Action (49 to 139). They are flat shapes in the map; the game reads
  them as the mission loads and then deletes them, so they are never seen.
  A pilot whose way would cross one stops 3 m short of it, or goes round it
  by the shorter side, corner by corner (each corner 3 m out from the
  shape); a pilot standing inside one is sent to the nearest point 3 m
  outside it. A few missions (Op 2's second and fourth, the last two of Op
  4) also have one large blue shape that works the other way round: an
  outer fence the AI keeps inside. Bridges, a drawbridge (`draw01`) and
  gates (`portdoors`) have two sets under one name, red and yellow: the
  yellow set is off at the start, and a message switches which set counts,
  most likely as a bridge falls or a gate opens; which missions send it is
  not yet known.
  Training has its own set; multiplayer has one in the map files that the
  game never reads. (`ai.md` section 11)

## The cameras

From the view and its cameras (0x40aca0, 0x430e70; details: `view.md`).

- **Vehicles have three cameras of their own, and only the first is used.**
  All vehicles share one set (behind, on top, high above), made with the
  first vehicle; nothing ever switches it from "behind" (`vehicles.md`).

- **The outside cameras slip in front of walls, but not of water.** When
  something stands between a chase camera and your mech, the camera jumps
  to one unit in front of it. It also shifts sideways off anything within
  two units beside it. Water surfaces are meant to be ignored, but the
  game only skips the water it meets before the first solid surface in its
  list. A water surface listed after a solid one can still stop the camera
  when it is the nearer of the two.
- **Every mech sits you in a different spot in its cockpit.** The cockpit
  camera hangs at the torso plus an offset from the mech's file: the Mad Cat
  2.45 m up and 1.3 m forward, the Vulture 3.75 m up but 1.75 m *behind*, the
  Thor a metre to the right, the Owens 0.18 m to the right.
- **How far you can turn your head differs by mech too:** the Annihilator,
  Bushwacker, Firefly, Supernova and Vulture look 89 degrees to each side; the
  Orion and Sunder only 55. Looking up ranges from 17 degrees (Sunder) to 35,
  down from 10 (Vulture) to 18 (Daishi).
- **There are six cameras, not three.** Besides the cockpit and the two the
  camera key cycles to (tether behind, zippercam beside), each mech has
  godseye (150 m up, looking straight down), mcbtether (30 m back) and area
  (40 m off); what switches to those is not traced yet.
- **The cockpit view bobs with the mech's motion,** and twisting or pitching
  the torso adds to the tilt; the bob is 0.15 of the motion, behind a switch
  that is on in 1.2 ([0x5e2448]; no menu sets it that we know of).
- **The chase camera hangs back when you slow down:** its distance grows with
  your speed and closes slowly when you brake (15% of the gap a second), but
  never comes closer than 90% of its base distance.
- **Under water the fog changes**: when the camera is below a water surface
  that is above the ground there, the world takes the mission's water fog;
  an outside camera that ends up below the ground is lifted half a metre
  above it.
- **Rain and snow stop under a roof:** each frame the game looks 100 m up
  from the camera; anything there hides the weather.

## Game states

- **Escape doesn't pause a multiplayer mission**: a message box asks instead
  and the mission runs on. In single player, Escape pushes the pause menu.
- **During a network game the game turns off Alt-Tab** (Windows 9x's "screen
  saver running" trick, also blocking Ctrl-Alt-Del) for the missions, and the
  cheat options no longer touch your mech.
- **The release build's error messages are switched off.** Every internal
  check ("Stack Depth Underflow" and the like) calls a function that is a
  single `ret` in 1.2, so problems were never reported, only lived with.
- **The menu/state stack has no real guard rails** (`zgameapp.cpp`). Too many
  "pops" leave it resuming the bottom screen, and too many "pushes" overwrite
  the top one. Pushing onto an empty stack would crash; the game always starts
  with a "replace", so it never happens.
- **Every frame runs in five bands, in a fixed order** (0x4f7d80,
  `action-lists.md`): the scripts' scrolling textures, then your mech and
  the other vehicles (and the radar), then every shot and beam, then the
  mission's animations, then smoke and fire. After each band the world
  catches up: moved objects get their new bounds and map square. So a shot
  always leaves from where your mech is this frame, and the animations see
  where the shots ended up.
- **A mission starts with two seconds frozen.** Before the mission runs,
  the game takes a still of the screen and fades it in from black over 2
  seconds (0x48eea0, `game-loop.md`, The fade state). The fade sits on top of
  the mission, so nothing moves and the mission clock does not run until it
  is over; only then does the mission really start (resume 7: the AI's lists
  are sorted then). In the 320x240 doubled mode there is no fade. Leaving a
  mission through its end screen or the pause menu fades the screen to black
  the same way.
- **The fade's picture is a snapshot,** grabbed the moment the fade begins,
  not the live screen. The fade in grabs the back buffer, the fade outs the
  screen as shown. What the back buffer holds at a mission's start (likely
  the last loading frame) is not checked in play.
- **The fades follow real time, not frames:** each frame adds its share of
  the 2 seconds, so they take as long on a fast PC as on a slow one. The
  frame that reaches the end is still drawn.
- **A developer's "haha" is still in the game.** Each time an object named
  `blackhawk_01` has its bounds rebuilt, the game prints "haha" (0x4ecc00),
  to a console a Windows game does not have, so nobody ever saw it.

## Keyboard, mouse and joystick

From the input module, zInput (details: `input.md`).

- **The keypad's part targeting needs a Targeting Computer.** Keypad 8, 5,
  4, 6, 7, 9, 1 and 3 pick the target's head, torsos, arms or legs only when
  your mech carries a targeting computer (and you have a target). Without
  one, the keys do nothing at all. Pressing the same key again drops the
  choice, and a part that has been shot off can't be picked.
- **Aiming at a side torso uses your own mech's width.** With a left or
  right torso picked, the game aims at a point three quarters of a torso's
  half width out from the target's centre. The width it uses is your own
  mech's torso, not the target's, so the aim point is off on mechs much
  wider or narrower than yours.
- **Your torso's pitch stays where you leave it.** Pitching the torso with
  the keys moves it, and it stays at that angle when you let go. It drifts
  a little further as the controls ease off. Only after looking left or
  right does the pitch level out by itself again.
- **N, Shift+N and Ctrl+N (op points) only work in multiplayer.** They step
  through multiplayer's stations; in the campaign and Instant Action there
  are none, and the keys do nothing.
- **"Look back" doesn't turn the cockpit view round.** In the cockpit and
  the chase cameras the look-back key leaves the view where it was (it only
  releases the torso's pitch). Only the area camera turns round with it.
- **Half of the cockpit's voice lines are never played.** The game loads
  30 cockpit sounds at every mission start. It plays single, group and
  linked fire engaged, a critical hit, no ammo, ammo explosion, ammo
  jettisoned, coolant flushed, gyro overload, heat critical, MASC engaged
  and the two lock tones. "Damaged", "destroyed", the hit callouts for
  each body part, low ammo, coolant depleted, ECM engaged, friendly fire,
  gyro OK, targeting computer online and "traitor" are loaded, but nothing
  in version 1.2 ever plays them.
- **The main menu's hotkeys are plain keys, not your key bindings:** O
  options, C credits, M multiplayer, I instant action, T training, Q quit and
  R (the seventh button), whatever you rebound in the controls screen. Escape
  is fixed the same way.
- **Left Alt is called "Home" in the key lists.** The game looks up Left
  Alt's name under the message for Home, so a command bound to Left Alt
  shows as Home (Right Alt is named correctly).
- **Alt-F4 runs its command at once,** ahead of every other key and even
  while you are typing into a text field.
- **A tap shorter than one frame still counts as a press** for the menus,
  but not for screens that wait for a key "pressed this frame" exactly.
- **Holding Ctrl, Alt and Shift and pressing a newer keyboard's Power, Sleep
  or web key could crash the original** (it wrote past its key table). The
  rebuild ignores such keys.
- **Without a mouse the original would not start at all.**
- **The joystick's trigger survives a reset.** Clearing the buttons skips
  button 1 and clears a 'button 11' instead (an off-by-one), so holding the
  trigger through a menu change keeps it held.
- **The joystick setup is fixed in the code:** every axis -1000..1000, a 10%
  dead zone on the stick, none on the throttle, 25% on the twist and other
  rotations.
- **The mouse wheel is read but drifts:** each input reset adds the last
  wheel turn again.
- **Your key bindings are saved with the other settings** (in the registry,
  under the name `DefaultCmdMap`).

From the player's controls, 0x427d60 (details: `controls.md`):

- **The throttle keys move the throttle by a step each frame**, 2.5% of full
  a frame: at 30 fps from stop to full in 1.3 seconds, at 60 fps in 0.7.
  The rebuild moves it at the 30 fps pace whatever the frame rate.
- **With "Constant throttle" off (the code's default), the throttle eases
  back to zero when you let go of the throttle up and down keys** (about half
  its value a second; it drops to zero below 5%). A throttle set with the
  number keys holds. With the option on, the throttle stays where you left
  it.
- **A joystick throttle takes over only once you move it.** From then on it
  sets the throttle outright, until you touch a throttle key again. Setting
  it this way does not switch the reverse flag.
- **Zooming in slows your torso to 30%.** While the reticule zoom is held,
  the twist and pitch keys turn the torso at 0.3 the rate. Holding the zoom
  key zooms in, letting go zooms out, with a click each way.
- **The joystick hat drives the camera:** one camera command each time it
  changes direction, the command depending on the camera view (not in the
  free-look view).
- **"MW2 jump jets" changes the jets:** with that option the jet forward,
  back, left and right keys fire the jets in that direction, and the jet
  rotate keys turn you while airborne. Without it only "jets up" fires.
- **The free look follows the twist and pitch keys smoothly** (eased in at
  six times the distance a second), and a mouse bound to twist and pitch
  becomes a pointer, a free look at 1.5 times the speed, or the torso,
  depending on the view.

From the default bindings and the controls profiles, 0x411150 and 0x4120a0
(details and the full default key list: `controls.md`, Setup):

- **Number key 2 sets 20% throttle, not the 10% the controls screen
  says.** The keys give 0, 20, 30, 40 .. 90 and 100%; there is no 10%.
  In reverse they give the same speeds backwards.
- **Insert and Page Up both turn you and rotate you on the jump jets:**
  they are the turns' second keys and the jet rotations' first keys at once
  (a key can sit in both slots), so in the air with "MW2 jump jets" on they
  do both.
- **Look up and look down are on Alt + keypad 2 and Alt + keypad 8**, the
  other way round from the arrows printed on the keypad (like pulling back
  on a stick).
- **Ctrl+Q is "Exit Mission"**, and Alt-F4 is a hidden "Quick Exit" command
  that is in no list: the controls screen does not show it and a saved
  profile does not store it.
- **Loading a saved control profile unbinds everything it does not list**,
  Alt-F4's Quick Exit included, until you reset to the defaults.
- **The default joystick buttons:** 1 fire, 2 lock zoom, 3 next enemy, 4
  next weapon group, 5 fire mode, 6 alpha strike, 7 jump jets, 8 center
  legs, 10 throttle direction; button 9 does nothing. The left mouse button
  fires and the right one is the free look.
- **Profiles are plain text** (`keys\<name>.zrd`): keys as hex codes, axes
  with a source, slope, intercept and dead zone, and can be edited by hand,
  with some traps: a key written as a plain number (`keya(28)`) is read as
  nothing (it must be `0x1c`), yet `keya(28x)` reads as 28; a command name
  the game does not know takes those keys away from every command; an axis
  line without `joystick(...)` or `mouse(...)` copies the source of the line
  above it. An axis name other than Throttle, Twist, Pitch or LR corrupted
  the original's memory (a crash later); the rebuild skips it.
- **The game notices when a profile's axes are exactly the stock joystick
  setup**: only then may the game switch them to the mouse by itself at the
  next start (when no joystick is found). Custom axes are left alone by the
  "use joystick" option and the game.
- **A profile named like one of the game's own data files** (for example
  "detail" or "weapons") saves fine but loads the game's file of that name
  instead, so loading it changes nothing.

From the mech's command handler, 0x41be80 (details: `controls.md`, The
mech's commands; `heat.md`, Coolant flush):

- **Coolant flush is strongest the first time.** Each flush doubles what is
  left in the tank, takes that much heat off (or all of it), and keeps half
  of what was not used. A full tank can wipe out a whole heat bar in one
  press; after a flush that took h, the next can take at most 2 - h (on the
  game's 0 to 2 heat scale). The tank never refills during a mission, and
  every mech has one: no equipment needed.
- **The radar range key (Alt+R) cycles 500 m, 1000 m, then passive.** A mech
  with a C3 computer gets an extra 2000 m step before passive. Passive
  always comes back at 500 m. In the campaign the sensors never actually
  reach past 1000 m, whatever the setting; in Instant Action and multiplayer
  your own mech sees 50 km, whatever the setting.
- **Shift+F1 to F5 move the selected weapon into that group** and select it
  there; Enter selects the weapon the HUD highlights, switching to its group.
- **Crouch, alpha strike and the HUD key do nothing while powered down;**
  the handler does not hold back the other keys (getting up, targets, weapon
  groups, the AMS).
- **The thermal shutdown override (O) only takes while the heat warning is
  beeping,** and only for the mech you are looking from.
- **Lance orders (F6 to F11) are ignored in multiplayer.**

From the entity manager's frame, 0x5a8850 (details: `ai.md` section 16):

- **At most 64 vehicles get a full update each frame.** The rest only save
  up their frame time for a later frame, and the next frame's walk starts
  one vehicle further on, so in a big battle the turns rotate. The player's
  own mech is updated separately and never waits.
- **Powering up is instant.** A mech asked to power up is up on the spot;
  only powering down waits, until the mech is standing still or in one of
  two other poses that allow it.

From the weapons' effects (details: `weapon-effects.md`):

- **Machine-gun tracers are single dots.** The tracer model has no shape at
  all, only one coloured light point, drawn as a single pixel that fades out
  with distance. At 640x480 it is one dot per bullet.
- **Weapons can name sprite "effects" that never show.** 53 entries in
  weapons.zrd have an `EFFECT`, but the game's effect list (effects.zrd) is
  empty in both MechWarrior 3 and Pirate's Moon, so none of them appear.
- **Impacts turn to the surface they hit:** the surface animations (the pock
  marks on the ground, the splashes) are rotated to lie on the slope.
- **Only eight impact lights exist at once;** a ninth impact while they
  are all lit gets no flash of light.
- **The machine gun's rattle comes from the mission's "common" sound set,**
  loaded with the mission, not with the weapon: the weapons' own looping
  sounds are found by name there.

## Menus

From the 2D front end, zTwoD (details: `twod.md`).

- **Menus only redraw what changes.** Nothing clears the screen between menu
  frames: each button repaints its own rectangle when it changes, first
  painting back a snapshot of the empty menu taken when it opened. Text is
  the exception and repaints every frame.
- **Check boxes play their hover sound twice.** Moving onto an unticked
  toggle button starts its rollover sound, then the ordinary button code
  starts it again at the same moment (0x59db00, then 0x59c650).
- **Every menu could have had movies, music and text in its background.** The
  menu loader reads background movies (with a transparent colour), looping
  sounds or CD tracks, and fixed texts for any menu, but no shipped menu uses
  them: the menus' movies and music come from the game's own code.
- **A blinking-text mode exists but nothing uses it.** Flashing labels only
  ever swap colours; the code for text that blinks on and off is there, with
  no way to switch it on.
- **An unanswered question box closes itself.** The yes/no and OK/Cancel boxes
  run their own loop for at most 100,000 frames, about 28 minutes at 60
  frames a second, then close with no answer: the main menu's "quit?" then
  counts as "no" and you stay (0x5a7288).
- **Menu text is real Windows fonts, and could have been two-tone.** Labels
  are drawn by Windows with TrueType fonts (Lucida Console for nearly all of
  them, Arial for a few, Impact once; the game ships the three .ttf files)
  into a picture, usually with a drop shadow. The code can also draw text
  with a gradient, white first and then recoloured row by row from a top
  colour to a bottom one, but no shipped font asks for two colours.

### The main menu

From the main menu's code, 0x00486260 and its buttons (details: `mainmenu.md`).

- **The water ripple on the main menu.** Moving onto a main menu button
  sends a ring through the screen around it: the picture inside the ring is
  pushed out along a sine wave, as if a stone dropped in water. The ring
  grows 120 pixels a second and fades from strength 7 to 2, losing 0.3 every
  frame, so at 30 fps a ripple lasts about half a second and spreads to about
  90 pixels; on a fast machine it died almost at once. The rebuild fades it
  at the 30 fps pace whatever the frame rate.
- **The menu background and the MECH3 logo crawl by the frame.** The
  background slides one pixel right and one down every frame and wraps
  around; the big MECH3 logo slides two pixels left a frame and comes back in
  on the right. Both ran faster the faster the computer drew the menu; the
  rebuild moves them at the 30 fps pace.
- **`-seasick` stops it all.** The `-seasick` switch (the FastMenu option)
  draws the main menu once, takes a picture of it, and from then on only
  redraws the button under the mouse: no crawling background, no sliding
  logo, no ripples.
- **Every button has a key:** I Instant Action, C Campaign, M Multiplayer,
  T Training, O Options, Q Quit, and R for the cRedits.
- **The game puts you back where you were.** After an Instant Action game the
  main menu presses Instant Action for you, after a training mission
  Training; after the campaign's ending movie (c4win) it opens the credits by
  itself (0x0047aa50 sets the flag, 0x00486c17 presses the button).
- **Quit is the one silent button.** Every other main menu button plays its
  click and shows its pressed picture before opening its menu; Quit goes
  straight to the "quit?" question.
- **Leftovers from the demo.** The full game's main menu file still gives
  Campaign, Training and Multiplayer an "Only Available In Full Version"
  help text, and the same file holds the demo's exit screen with "Buy Now"
  and Quit buttons. The 1.2 program never reads either.
- **The version line assumes 640x480.** The "MechWarrior 3 1.2" text in
  the corner is placed for a 640x480 screen whatever the resolution, while
  the background and the logo use the real screen size.

### Options and credits

From the option and credits screens, 0x004379f0, 0x00438f60, 0x004289a0
(details: `menus.md`).

- **Difficulty is GameIntensity.** The Options screen's Difficulty (Low,
  Medium, High) is the same setting as the registry's GameIntensity
  (`options.md`).
- **The red switches are the cheats.** Invincible and Infinite Ammo show ON
  in red, Heat Management shows OFF in red: the three settings that mark a
  game as cheated (`protection.md`). Everything else is green.
- **Inverting the mouse resets its sensitivity.** "Invert mouse Y" is just
  the sign of the vertical mouse speed, and pressing Accept on the game
  options writes it back as exactly 1.3 (or -1.3), so a vertical speed set
  in the registry is lost every time you accept that screen.
- **Back saves nothing, Accept writes the registry.** Back leaves the
  options as they were; Accept on the game options saves every setting to
  the registry at once. Back and the credits' Back are also the only menu
  buttons that make no click.
- **The graphics menu never offers more than 1024x768.** It shows the modes
  a card can run by its free video memory: 640x480 to 1024x768 for a 3D card
  (800x600 below 4.5 MB free, 640x480 below 2.75 MB), 320x240 and 640x480 in
  software. The game has names ready for 1152x864, 1280x1024 and 1600x1200
  and its list would show them, but the check never reaches them, so they
  stay registry-only; and accepting the graphics menu replaces a
  registry-set one with the mode shown.
- **The texture size is a choice of 2, 4 or 8 MB.** "MAX" appears only for
  the software renderer, and a card shows only the sizes its memory holds.
- **Changing the video card restarts the game.** Picking another device and
  pressing Accept asks to restart; yes ends the game so it can start again
  on the new card (the game options screen's choices are applied too), no
  keeps the old card. While you browse cards
  the menu shows each card's own lighting, effects and shadow settings: the
  game keeps a separate set for software and for 3D.
- **Shadows need a 3D card and one feature of it.** Lighting and self
  shadows are greyed out in software; Shadows also needs the card to report
  capability 3.
- **The credits take four minutes.** 246 entries scroll by at one second
  each, then start again from the bottom, at the same speed on any computer.
- **The "Other Voices" heading is out of line.** Its position in the credits
  file has one number instead of two, so it borrows the position of the name
  above it: it is indented like a name, with no gap, unlike every other
  heading (likely a typo). The rebuild places it as the other headings.
- **Accepting the game options resets your vertical mouse sensitivity** to
  1.3 (or -1.3 with Invert Mouse Y), whatever it was set to before. The
  rebuild keeps your sensitivity and only flips its direction.

### The controls screen

From the controls screen and its axis popup, 0x0042a590 and 0x0042da80
(details: `menus.md` section 6).

- **Every command has a description.** The text under the lists is the
  command's own help line (its name's message plus 0x100 in the game's
  message table).
- **Click a row, then click again to rebind.** Clicking a key in another
  row only selects that row; clicking the selected row's key (or the column
  itself) starts listening. Right-clicking a key or button clears it.
- **Escape can never be a key binding.** While the screen listens for a
  key, Escape cancels instead.
- **Taking a key from another command asks first** ("Reassign Key"), and
  takes it from whichever of its two slots it was in.
- **Binding a joystick or mouse button to the throttle, turn, twist or
  pitch drops that command's stick or mouse axis.**
- **Moving the stick while the joystick column listens picks an axis.**
  Push any axis more than half its travel and the "assign axis" box opens
  with that axis chosen.
- **The axis box always offers six stick axes and three mouse axes,** X, Y,
  Z and the three rotations whether or not your stick has them, and never
  the two sliders: a slider (or a throttle wheel the game sees as one) can
  only be set by editing a saved profile by hand.
- **Assigning an axis there centres it where it is when you press OK**, at
  full scale (inverted if ticked) and with no dead zone. A throttle lever
  assigned this way has its stop wherever the lever was, with reverse
  below it; the stock throttle's half scale, offset and small dead zone come
  back only with "Joystick defaults".
- **The two default buttons save a profile.** "Mouse defaults" and
  "Joystick defaults" reset every key and the axes, then save them as the
  profile `DefaultCtlConfig` and make it the current one.
- **Cancel does not just undo.** With changes made, Cancel resets the
  stock keys and axes, then reads the current profile again from disk. If
  you never saved a profile, that file does not exist, so you are left with
  the stock defaults: any bindings you had only in the registry are gone.
- **"Save changes?" No keeps the changes.** Accept with changes asks to
  save; answering No leaves the screen with the new bindings still active
  (they are only not written to a profile).
- **The mouse column is deaf for ten frames after a mouse button is
  bound,** so the click that set it does not start listening again.
- **The screen has an unused "press a key" prompt** (PRESS_A_KEY in its
  dialog file) that the game never shows.

### The controls' load and save box

From the box Export, Import and "save changes?" open, 0x0042e890 (details:
`menus.md` section 6, the file list in section 2).

- **Profile names are 19 characters at most,** and `: | \ < > / ? " *`
  cannot be typed at all.
- **Click the name before typing.** The box only takes keys once its name
  field has been clicked; Enter and Escape just stop the typing. Only OK
  loads or saves. OK and No leave the field listening, so until you leave
  the main menu, the next time the box opens you can type straight away
  (and keys typed on the controls screen meanwhile go to the hidden field,
  harmlessly: the box puts the current profile's name back on opening).
- **OK with the name field empty saves (or loads) a profile with no name**:
  the file `keys\.zrd`, which then shows in the list as an empty row.
- **The list is in the order Windows lists the folder**: alphabetical on
  NTFS, but on a Windows 98 FAT drive the order the files were made.
- **The list's up arrow is invisible while it works.** Its dialog gives it a
  greyed picture and a highlighted one, but no normal one: at the top of the
  list it shows greyed, and once you have scrolled down it vanishes until
  the mouse is on it. The down arrow has all three.
- **Holding an arrow scrolls on its own** after half a second, then about
  seven rows a second.
- **Each opening of the box leaves a Windows file search open** (the game
  never closes it, FindClose is not even imported): harmless, a handle a time.

### The pause screen

- **Escape is not the same as RESUME.** Both close the pause screen, but
  only the RESUME button restarts the sounds that pausing stopped and gives
  back the sound groups that were playing (the game's multi-part,
  repeating sounds). Leaving with Escape skips both, so whatever was
  sounding when you paused stays silent until the game plays it again.
  Found in the code (0x492d70 against the Pause State's escape 0x493250),
  not yet heard in play.
- **REPLAY on the pause menu is an instant mission failure.** It does not
  restart anything by itself: it ends the mission as lost on the spot,
  without the "mission failed" voice, the red flash or the usual 8-second
  wait, and the end-of-mission screen follows, from where you replay. It
  only appears in a campaign or Instant Action, not in training or
  multiplayer.
- **Quitting a campaign mission from the pause menu is a clean retreat.**
  Gear a lost field base cut comes back, and your lancemates are reset, as
  after a failed mission; then you are back at the main menu. Quitting
  Instant Action instead fades to its results screen, and quitting training
  brings the training menu back up.
- **An unused switch hides the pause menu's OPTIONS button.** The pause
  screen checks a flag that would remove Options, but nothing in the game
  ever sets it: perhaps a leftover from a demo or a multiplayer plan.
- **The pause menu keeps the mission's screen size.** Unlike the main menus,
  pausing does not switch to 640x480: the 640x480 dialog sits in the top
  left corner of a still of the battlefield at the mission's resolution.
- **Every mission carries a hidden "leave the game?" box,** made when the
  mission starts and kept switched off. Only a multiplayer game ever shows
  it, when you ask to quit: OK leaves the session and fades back to the
  multiplayer screens, Cancel returns to the fight. While it is up, whatever
  your first mouse button does (fire, by default) is unbound, so clicking
  the box does not also shoot. It also holds a spare menu button that
  nothing uses.

### Pilots and the pilot screen

The screen INSTANT ACTION, NEW CAMPAIGN, TRAINING and MULTIPLAYER open
first, 0x00486f40, and the pilot files behind it (details: `pilot.md`,
`menus.md` section 10).

- **Pilot names are 15 characters at most** and cannot hold
  `: | \ < > / ? " *` or a tab. Accented letters are fine (typed in
  Windows' character set; the rebuild stores them as ordinary file names).
- **Delete removes the chosen pilot, Ctrl+R renames it,** besides the DELETE
  and RENAME buttons (both keys are ignored while a name is being typed).
  The question is "Ok to delete pilot: <name>?"; deleting also deletes every
  saved campaign game of that pilot.
- **Each pilot is one file, `pilots\<name>.mw3`,** holding everything: the
  campaign's saves (each also a file in `pilots\games`, named after the
  pilot, the operation and the mission, `Bob0102.mw3`), the Instant Action
  record, the multiplayer score, the training missions passed and the mech
  lab's line-up. A saved campaign whose `pilots\games` file is gone is
  dropped from the pilot when it is read.
- **Hidden statistics.** The pilot screen shows, for Instant Action, player
  kills, lancemate kills, enemy kills, attack and defend wins and losses and
  "Progression Waves"; for multiplayer, kills and deaths. Each pilot file
  also carries the Instant Action screen's settings, which are not the
  pilot's own: the pilot read last decides them (at the pilot screen, the
  last in the folder's order), and every pilot saved takes the current ones.
- **Every new pilot's campaign starts with the same store:** two Bushwackers
  with their "Primary" loadout, of which the code takes one Bushwacker back
  straight away, plus 750 rounds of LRM-5 ammunition, 4000 of machine gun
  and 1000 of AC/10.
- **Salvage is merged when a saved game is loaded,** not when the mission
  ends: two of the campaign's five stores (what the last mission left) are
  added to the main store on reading, and a third is emptied.
- **The newest save is the one you play.** The pilot screen's campaign list
  shows every save as "Op N, Mission M" with the newest chosen; choosing
  another row plays that one. The fresh start (Op 1, Mission 1) is always
  the first row and is never saved itself.
- **Training remembers which missions you passed** (PASS / INCOMPLETE) and
  opens on the first one not passed.
- **Names that differ only in capitals share one file.** "Bob" and "BOB" are
  two pilots to the game but one file to Windows: making "BOB" when "bob"
  exists writes a new pilot over bob's file and lists both, and whichever is
  saved last (when you leave the main menu or quit) decides what the file
  holds. Renaming a pilot to another pilot's name does the same. Fixed in the
  rebuild: "BOB" picks the existing bob, and a rename onto a taken name is
  refused (`fixes.md`).
- **A pilot file of another version is not refused:** its header is simply
  rewritten and the rest read as if it were this version's.
- **The campaign is four operations of 4, 4, 6 and 6 missions.** When a
  mission is won, the next one's saved game is made at once (before the
  debriefing screen): the mechs and parts you own, the salvage the mission
  left, your mech and loadout and your lancemates are carried into it, and
  it becomes the save you play. Winning the last mission of Operation 4
  makes no save: the campaign is over.
- **"Replace existing game?" No still replaces the file.** Replaying a
  mission you already won asks whether to replace the saved game of the
  next mission. The new save's world is written to that file before the
  question, so No keeps the old entry in the list but its file already holds
  the new one (fixed in the rebuild: the world is written after the answer,
  and not on No).
- **The briefing screen has keyboard shortcuts:** once the mission has
  loaded, L launches and M opens the Mech Lab; Space or S stops the briefing
  voice at any time.
- **At Op 1 Mission 1 the briefing's Mech Lab and Salvage buttons stay grey.**
  Instant Action's briefing has no Salvage or Replay button, training's no
  Mech Lab either, and a multiplayer briefing shows only Launch and launches
  by itself the moment the mission has loaded.
- **Launching with salvage you have not looked at asks first.** In a
  campaign, if the last mission left salvage and you have not pressed Salvage
  on this briefing, Launch asks before it goes.
- **The briefing map follows cue marks in the voice.** Each briefing's
  voice file carries markers; as the voice reaches one, the next step of the
  map (circles, waypoints, pictures, labels) appears. Without the voice file
  every step shows at once. Replay hides them and starts over.
- **The briefing map is drawn from a list in briefing.zrd:** circles that
  shrink into place with a sound, waypoints sitting on the mission's real
  nav points (labelled "Nav ..."), pictures, texts and lines, each tied to a
  marker of the voice. A waypoint finds its nav point by its label: the
  file's "name" key is never read. The circles shrink a pixel a frame, so on
  a fast computer they snap into place almost at once.
- **The debriefing's mission time goes wrong past an hour**, as the HUD's
  timer does: 3700 seconds shows 01:01:3640.
- **After a failed (or cheated) campaign mission, CONTINUE replays it:** your
  lancemates are put back as they were and the same briefing comes again.
  ABORT saves the pilot and goes to the main menu. A cheat also takes away
  the debriefing's REPLAY button and its salvage.
- **The lance cards on the debriefing ignore the mouse**; only CONTINUE,
  REPLAY and ABORT react.
- **How the debriefing scores a campaign mission:** each primary or
  secondary objective you completed is worth 2,500 points (one not done
  shows "Failed" and 0); the lance's points for mechs, vehicles and turrets
  destroyed are added. That is the Mission Total; the Campaign Total is the
  campaign's running score plus the Mission Total, but only when the
  mission succeeded and no cheat was used. Objectives hidden from you
  are not listed or scored.
- **What a wrecked mech leaves for salvage:** the whole mech only if its
  centre torso is still standing, or if it was killed by a head shot (so
  head shots are the way to capture mechs); if a leg was shot off as well,
  it is a coin flip. Elementals never leave anything. Its weapons are
  salvaged even when the mech is not (except TAG), each with whatever
  ammunition was left in its bin, its equipment unless the part it sat in
  was destroyed, and any heat sinks beyond the engine's ten.
- **Salvaged weapons can change tech:** a weapon is salvaged in the tech of
  its owner's loadout; if that weapon does not exist in that tech, it comes
  out as the other side's version (an Inner Sphere-only weapon on a Clan
  mech arrives as Inner Sphere, and the other way round).
- **Your lancemates' mechs go to the salvage pool after a win**, under the
  same rules (except at Operation 4 Mission 4), and your own mech is put
  back into your stores the same way, coin flip for a missing leg included
  (whether a lost flip costs you the mech depends on the mech lab's slots,
  not traced yet).
- **The mech lab has a weight and mech limit in a campaign:** 300 tons and
  2 mechs per step of the campaign's capacity, which is 3 in practice, so
  900 tons and 6 mechs. When you leave the mech lab, what you owned before
  is kept first (as much as fits), then anything new; whatever does not
  fit goes back into the salvage pool, and the briefing shows it in its
  reallocation list until you press OK.
- **Lancemates' mechs come out of your inventory before each mission:**
  when a briefing loads, each lancemate's mech must be in your stores; if
  it is not, the lancemate gets the next mech type you do have (in the
  game's internal order of mechs), or none at all. Their loadouts are cut
  down to the weapons and ammo you actually have, and those items leave
  your inventory while they are equipped. At Operation 4 Mission 6 this is
  skipped: your lancemates keep whatever they had.
- **The debriefing's armor bars are scaled per card:** each lance card's
  little H-T-A-L display scales its bars to that mech's own thickest armor
  section, so a full bar on a light mech and on an assault mech mean
  different amounts; the bars only compare sections within one card. The
  rear torso bars are drawn darker.
- **Your rank is the Campaign Total in steps of 100,000:** Sergeant below
  100,000, then Master Sergeant, Captain, Major, and Colonel from 400,000
  on.
- **The debriefing keeps a career kill count for each pilot:** under
  "'Mechs Defeated" every pilot in the lance (you, Dominic Paine, Epona Rhi,
  Alan Mattila) shows the mechs they have destroyed over the whole
  campaign, this mission's included. After a win the original copies these
  back by card position and writes 0 for a card without a matching pilot,
  so a lancemate missing from their card loses the whole tally (fixed in
  the rebuild). Lancemates are recognised by their
  names. Each lance card shows that pilot's mechs and vehicles destroyed
  this mission and their sum.
- **Instant Action's score:** 1,000 flat, or in its wave modes 1,000 a wave
  plus 100 for each round left at each wave (one round less unless you
  won), plus the mechs' points. Its debriefing shows a "Primary
  Objectives" heading with nothing under it, and the rank by that score.
- **A click on the briefing's loading bar moves it** to where you clicked;
  it then creeps on towards the real progress from there. The bar creeps 0.005
  a frame, so it filled faster on faster computers (the rebuild: 0.15 a
  second at any frame rate).
- **The briefing has a hidden debug log:** a box of the load's lines at the
  top, shown only with a debug switch of the app, and the game's version in
  the bottom right corner in the debug font.
- **The game crashes** if you press DELETE before any pilot exists (the
  rebuild does nothing), and on a `pilots\` file whose name is 64 characters
  or longer (the rebuild skips it).
- **The debug menu's campaign jumps use a pilot named "DefaultPlayer"**, the
  debug Mech Lab start one named "Default Player".
- **The HUD's lists stop at sixteen:** the weapon list and the equipment
  list each have exactly sixteen lines, ten pixels apart.
- **The radar has two sizes:** small, a radius of 50 pixels around (55, 55)
  near the corner, and zoomed, a radius of 180 around the middle of a
  640x480 screen; it slides between them ten pixels a step.
- **The zoom window magnifies ten times** through a 5-degree camera that
  borrows your main camera's draw distance.
- **In software mode the HUD's green panels are solid;** in Direct3D they
  are 60% see-through.
- **TARGET remembers its display mode** (and the MFD its own) for the next
  mission; the first time it starts in mode 1 in Direct3D and 3 in software.
- **The HUD's arrows for "other" targets are misspelled in the data:**
  `hud_othererrow_left/right/up/down`.
- **The zoomed radar grows with the screen,** to a radius of 180 pixels at
  480 lines (225 at 600, 450 at 1200) around the middle; the small radar
  stays 50 pixels in the corner at every resolution.
- **The equipment list is told to make room when it is long:** with 17 or
  more lines (weapons plus equipped parts) its next placing puts it left of
  its spot instead of below it.
- **Your weapons aim where the reticle's line of sight ends,** worked out
  every frame from the camera through the reticle: at whatever it first
  meets, or at the far edge of your view distance when it meets nothing.
- **The reticle sees through water:** the aim skips water surfaces, so you
  can aim at things under or beyond the water instead of the water itself.
- **In the cockpit view the reticle stays centred;** only the free-look
  mouse mode moves it around the screen.
- **The zoom window opens in about a fifth of a second and closes faster:**
  it grows at 4.8 times its size a second and shrinks at 7, and its light
  only comes on once it is full size.
- **A weapon's recharge bar is its own name:** each line of the weapon list
  holds the weapon's name twice, in two fonts, and the second copy fills
  over the first as the bar's value rises. A ready weapon's name is all
  green; just fired, it turns red and the green creeps back from the left
  as it recharges. Out of ammo (or switched off) the whole name is yellow,
  destroyed it is grey. For lasers and other beams the bar is the charge,
  which a shot drains; for the rest it is the time to the next shot.
- **The weapon list animates only after a shot.** A shot starts it
  refreshing the bars every frame, and it stops once every bar is full;
  otherwise they change only when the groups or weapons do.
- **TARGET shows only the 3D view for targets that are not mechs:** its
  damage and nav modes fall back to the view, and with no live target it
  shows nothing.
- **The zoom key also picks a target.** Pressing Z (or keypad 0) makes
  whatever the reticle is over your target, unless it is an ally, as long
  as your sensors have it among their contacts. Q does the same without
  skipping allies, so Q can target a lancemate. Over something your sensors
  have not picked up, either key drops your current target instead; over
  open ground or sky nothing changes. Buildings and other mission objects
  can each be set to accept or ignore the zoom key and Q separately.
- **Keypad 0 locks the zoom window open** with a single click, and the zoom
  key then leaves it alone until keypad 0 closes it again. The torso's
  slow turn (30%) applies whenever the window is open, locked or held.
- **D and Ctrl+D step the MFD and TARGET through their views** (your mech
  in 3D, its damage, the armor display), but only from the view actually
  showing: with no live target Ctrl+D does nothing, and on a building or
  other non-mech target one press moves the setting off the 3D view and
  then it sticks (TARGET keeps showing the 3D view) until a mech is
  targeted. At the 320x240 pixel-doubled resolution both keys jump straight
  to the armor display.
- **Shift+R lays the radar over the mission map**, picking a map named for
  the radar's size ("small" or "large") or any map; with no map it turns
  itself straight back off. Turned off, the radar snaps to its size and
  place instead of sliding.
- **Alt+R turns the radar amber** once it reaches passive mode, and back to
  green when you leave it.
- **Targeting a mech was meant to play a sound of that mech's own:** every
  new target asks the target's sound slot named `mech_<its name>` (such as
  `mech_owens`). No such sound ships, so it stays silent.
- **R's radar size change clicks:** `radar_up` growing, `radar_down`
  shrinking.
- **The equipment list stops at the first hidden line:** lines after a
  hidden one are not drawn even if they are shown.
- **In Direct3D the equipment list's green panel needs the weapon list:**
  the see-through panel behind the equipment list is only drawn while the
  weapon list is shown too, so an equipment list shown on its own would have
  no panel behind it (whether play ever does that is not checked).
- **How a missile lock works:** keep the reticle on the target with a
  weapon group that locks, and the lock tone starts; after 0.6 seconds the
  lock is on and the reticle lights up. It holds for 3.5 seconds after the
  reticle leaves the target. Either the target or the aim point has to be
  within the group's longest range.
- **Elementals can't be missile-locked:** the lock skips any target whose
  pilot is an Elemental.
- **A NARCed target locks without aiming at it:** while a NARC beacon is on
  a target (30 seconds, and never on a mech carrying ECM), the lock counts
  the reticle as on the target wherever you point it.
- **ECM doubles the lock time:** against a target carrying ECM, a missile
  lock takes 1.2 seconds instead of 0.6.
- **What the weapon list's colours mean:** each weapon's light turns
  yellow past half damage and red past 80%; its name turns grey when the
  weapon is destroyed, and its texts go red only while it has ammo and is
  working. In chain fire the weapon that fires next has the select mark and
  the rest of its group the chain mark; in group fire the whole group is
  marked.
- **The targeting computer draws a lead circle:** with a targeting computer
  fitted, TARGET puts a small green circle where you have to aim the
  selected weapon so its shot meets the moving target (the shot's speed and
  the target's movement against yours taken into account; beams aim
  straight at it).
- **ECM scrambles the lead circle:** against a target carrying ECM the lead
  circle turns yellow and jitters by up to 4 pixels each frame.
- **TARGET's triangle shows the target's side:** red for an enemy, blue for
  an ally, green for anything else; it starts large when you pick a new
  target and shrinks onto it. Off screen, an arrow in the same colour points
  to it from the edge.
- **The mission timer breaks after an hour:** the HUD's clock works out the
  seconds without taking the hours off, so an hour and 40 seconds in it
  reads 01:01:3640 (the rebuild shows 01:01:40).
- **A hidden debug readout:** the HUD fills five debug texts every frame
  (the mech's gait, speeds and more) while a debug text is shown; a debug
  switch toggles it, and which key or option reaches that switch is not
  traced yet.

- **Your current target is a triangle on the radar,** not a blip: blue for
  an ally, red for an enemy, green otherwise. It shows even when its
  contact is not on the radar (the others need their radar flag), and a
  contact past your sensor range sits on the radar's rim instead of
  vanishing.
- **PPC-type static scrambles the radar:** while the HUD shows ZAPS static,
  every blip, shot and mine on the radar is thrown up to 100 metres either
  way on each axis, freshly every frame, so the radar jitters.
- **Missiles in flight show on the radar** as single pixels (only weapons
  marked to show on radar, which is also what anti-missile systems can
  shoot down), and **armed landmines within 100 metres** show as small red
  X marks.
- **In multiplayer your radar marks a player with a distance readout:**
  in kilometres with two decimals past 1000 m, else in whole metres
  (rounded up).
- **The radar's range readout** under the circle shows your sensor range
  in kilometres with one decimal; the small light beside it is green,
  yellow while an enemy sees you, and red while someone has you targeted.
- **Over the mission map (the map key), the radar draws on the map:** its
  circle grows to your sensor range at the map's scale, and the blips sit
  at their map places.
- **A powered-down mech is invisible on the radar:** only contacts in
  sight get a blip, and a shut-down mech is not in sight, so an ambush of
  sleeping mechs (like the Owens at the start of the first mission) shows
  nothing until they power up.
- **The damage display shows only internal structure, not armor:** a part
  keeps its undamaged picture until its internal structure is touched at
  all (exactly 100%), then steps down above 70%, above 40%, and once more
  above 0% (the 0-20% band looks the same as 20-40%); it shows destroyed
  only at 0. Armor hits never change the silhouette.
- **TARGET names a mech and its loadout** ("Mad Cat: <loadout>"; a
  network player's mech shows its type only) and its distance: "(500m)" up to
  1 km, but past 1 km the kilometres run straight on after the name with
  no space ("...1.23km"), a slip in the game's text. The rebuild writes
  " (1.23km)".
- **TARGET shows the part you aim at** in brackets ("[Left Arm]") after the
  distance, and its third line names what your saved or ally target is.
- **TARGET's colours tell the side:** all blue for an ally; the name red
  for an enemy; the first line green for anyone else; the silhouette's
  background and the target view's frame follow (a target with no vehicle
  gets a yellow-brown frame).
- **The speed readout is in km/h** (the game's metres a second x 3.6) and
  turns red when you walk backwards.
- **The throttle bar shows reverse at about half scale:** full reverse fills
  0.525 of the bar's height, downward and in red.
- **The heat gauge's pointer looks ahead:** the bar shows your heat now, the
  little toggler beside it your heat plus how fast it is changing, so it
  leads the bar when you are heating up and trails when cooling.
- **The coolant gauge glides** a tenth of the way to the real value each
  frame, so it moves faster on a faster computer. The rebuild glides it at
  the 30 fps pace at any frame rate.
- **The equipment list blinks green for one frame** whenever a line changes
  to a different item: the rename sets the green font over its yellow or
  grey state until the next frame.
- **On a fast computer the original never shows its mission messages:**
  the message panel slides up 60 pixels a second, rounded down to whole
  pixels each frame, so above 60 frames a second it moves 0 pixels a frame
  and stays under the screen's edge (at 30 fps it crawls at half speed).
  The rebuild carries the fraction: 60 pixels a second at any rate.
- **The message panel holds four lines** and slides up when a message comes
  with a time; a newer, longer time extends its stay. The objectives list
  sits on top of it and rides up and down with it.

- **Losing a Mobile Field Base costs you gear on the spot.** Each field base
  is room for 300 tons and 2 mechs of what your unit carries (3 bases: 900
  tons and 6 mechs). When one is destroyed in a campaign mission, the game
  immediately cuts what you carry: to 70% with two bases left, 50% with one,
  removing random mechs and random gear until you are under the new limit.
  Your salvage pool is not touched.

- **Salvage you leave behind is gone after the next mission.** A won
  mission's salvage is offered in the next briefing's mech lab next to what
  you carry. Whatever you move into your carried gear (up to 300 tons and 2
  mechs per field base) stays with you; what you leave in the salvage only
  lasts until you win the next mission, when the new mission's salvage
  replaces it. That is what Launch's "You have not allocated salvage from
  the last mission. Continue?" warns about. Your own mech always comes back.
  Gear cut when a field base dies is lost on a win, returned on a failed or
  restarted mission (with the field bases).

- **Losing all three field bases is not the worst case: it lifts the limit.**
  The first base lost cuts what you carry to 70%, the second to 50% of what
  is left (at most 300 tons and 2 mechs); the third cuts nothing. With no
  base left the next briefing's room is 0, and the split treats a room of 0
  as no limit at all: everything you take in the mech lab is kept. (The
  count goes back to 3 only after Op 1 Mission 1 and Op 4 Mission 4.) Not
  yet checked: how many field bases a mission gives you when the count is 0.

- **Some objective rewards in the mission files are never given.** An
  objective can carry salvage you get for completing it. The game reads
  weapons there only as a name with its tech (`("LRM20" "clan")`) and an
  engine only under the key `engines`. Three rewards are written another
  way and lose part of their contents: Op 1 Mission 2's primary objective
  lists a 120 XL engine under `engine` (never given; its weapons, ammo and
  equipment are), Op 2 Mission 2's secondary objective lists an ER PPC,
  a large pulse laser, an Ultra AC/5, an LRM 10 and a Streak SRM 4 as plain
  names (only their ammo is given), and Op 3 Mission 3's primary objective
  does the same with an ER PPC, a Gauss rifle, an LRM 20 and a Streak SRM 6
  (its two mechs, the TAG and the ammo are given). (docs/objectives.md)
- **In a campaign the field bases are always a fail condition.** The
  mission files never say so: the game adds a "Mobile Field Base
  destroyed" condition to every campaign mission's fail list itself, made
  of the field bases (`mobilehq_01` to `_03`) present in the mission.
- **Op 1 Mission 2 pays you for sparing the city.** When you complete the
  "destroy the APCs" objective, each of ten city buildings
  (`citybuild01`..`citybuild011`) whose intact model is still standing
  adds a little to the objective's salvage: AC/10, LB-X, LRM and machine
  gun ammo, and three of them a Clan heat sink. A building already wrecked
  by then gives nothing. Op 4 Mission 1 does the same once: destroying the
  train adds a Mad Cat to the salvage if the part `seg2tkt` is still
  active at that moment (what that part is in the world is not yet
  known). The salvage is decided at the moment the objective completes.
  (docs/objectives.md)
- **The objective messages.** A completed visible objective puts its
  description in the message panel for 5 seconds (the first line as
  "objective complete", the others below it) and then one line per item
  of its salvage; a silent one completes without its voice line.
- **Object detail "Low" puts every mech on its low-detail model.** The
  option scales each mech type's two switching distances (Medium by 0.75,
  Low by 0): at Low a mech switches to its low-detail model at any
  distance and only comes back while it is your target. (The effects
  detail option counts the other way inside the game: High is level 2 of
  the mission animations' "if level" tests, Low is 0.)
- **"Shadow parts" makes mechs darken in shadow, and brightens them on dim
  maps.** With the option on, the game checks one mech a frame with a ray
  from its middle towards the sun: if a building or a hill is in the way the
  mech fades to black lighting over a second, in the open it fades back.
  But "back" is always full light (1.0), not the map's own sunlight: on Op
  3's night-like world (sun at 0.2) a mech in the open is drawn five times
  brighter than the ground around it, on Op 4's second world and in
  training (0.7) somewhat brighter. (docs/shade.md)
