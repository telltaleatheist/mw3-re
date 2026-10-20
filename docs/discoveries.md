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

## Weapons

From the weapon code (weapons.md).

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
- **Pock marks never appear.** The ballistic weapons' data asks for bullet
  marks on `dirt`, `mud` and `snow`, but the impact code only knows default,
  water, seafloor, quicksand, lava and fire, so those blocks are never read.
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
- **TAG calls four missiles**, from 1500 m behind along the beam and 400 m up,
  spread 50 m apart.
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
- **Turn a cheat on once and the campaign mission doesn't count.**
  Invincibility, unlimited ammo or heat off, even switched off again, mean
  "Replay Mission" instead of "Next Mission" and no score added, until you
  restart the game.
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
- **The release exe calls itself "PRODUCTION"**: the loading text reads
  "MechWarrior3 (PRODUCTION) (1.2)", a build marker left in.

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
- **Mechs never shadow themselves**, and shadows only land on lit, solid,
  textured surfaces: not on foliage with see-through textures, not on the
  fake black shadow polygons.

## Mechs: camos

From the mech definitions (mechdef-skins.md). The camo names are GUNMETAL,
SUNFIRE, DESERT, WASP, SLATE, JUNGLE, ARCTIC and SPECTRE.

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
  loadout.
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
- **A mod with a wrong `version` in a mech file crashes the game** instead of
  skipping the mech. (Fixed in the rebuild: the mech is skipped.)
- Seventeen of the mech files carry a developer comment that the build tool
  kept as data: "torso limits: maxYaw minPitch maxPitch (degrees)", which is
  how we know the torso fields' names.

## Mission animations

From the animation files (anim.md).

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
- **There was an AI tester in the game.** A developer dialog for steering a
  unit by hand ("Direct", "Zig Zag", "Circle", "Jump", "Random Move") and
  picking its target ("player", "nearest enemy") is still in the exe behind
  the debug menu, along with an in-game editor for the AI paths that could
  save them back to files.
- **Old formats still load.** The vehicle reader also understands an older
  layout of the AI settings (tags p3, p4, p5, p7) that no shipped mission
  uses, and complains if a file mixes it with the new one.

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

## Keyboard, mouse and joystick

From the input module, zInput (details: `input.md`).

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
