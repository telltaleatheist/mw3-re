// The reference's registry for the mech definitions, dfn_<mech>.zrd: the
// reader (mech3/mech_reader.cpp, read_dfn 0x0046ba40 and its blocks;
// mech_skin.cpp read_skins), the cameras (player_view.cpp 0x004215d0 and
// 0x00421530), the canned effects, the gaits and the HUD (mech_data.cpp
// 0x00431750, 0x00431350, 0x00431a70, 0x00430ad0). Defaults from the
// definition's constructor 0x0046b890 (docs/mechdef-head.md).
#include "upgrades/reference/reference.h"

namespace upgrades::reference {

File dfn() {
  const char* C = "configurations/<name>";
  const char* A = "configurations/<name>/armor";
  const char* Cam = "camera/<view>";
  return {"dfn", "dfn_<mech>.zrd (a mech's definition)", "`zbd\\reader.zbd`, `dfn_<mech>.zrd` (a mod's new mech: mod.txt's `mech=`)",
          "`mech3::read_dfn` 0x0046ba40 and its blocks; the cameras 0x004215d0; the gaits and canned effects 0x00431a70",
          "A reader file as weapons.zrd's (keys exact, case included; a key the game does not know is never looked at). "
          "Angles are degrees in the file. `configurations`, `firepoints`, `skins` and the `camera`, `canned` and `motion` "
          "blocks hold `NAME ( ... )` records: the names are the modder's (a loadout's message key, a firepoint's node, a "
          "view's or effect's or gait's fixed name listed below). Every lookup searches the block depth first, so a key "
          "inside a nested record can answer for its parent's.",
          {
              {"", "version", "int", "required", "must be 2", "The format; any other refuses the mech."},
              {"", "chicken", "flag 0/1", "0", "", "Walks with backward-bending (chicken) legs."},
              {"", "max_turn_rate", "float (rad/s)", "1.2566 (72 deg/s)", "", "How fast it turns (the game uses half)."},
              {"", "accel_rate", "float", "0.5", "", "How fast it speeds up."},
              {"", "decel_rate", "float", "1.3", "", "How fast it slows down."},
              {"", "turn_accel_factor", "float", "1.5", "", "How quickly turning gets up to speed."},
              {"", "crouch_height", "float (m)", "1.0", "", "How far it crouches."},
              {"", "engine_minmax", "list: min max (int)", "10 400", "the maximum is always 400", "The engine ratings the mech lab offers."},
              {"", "torso_limits", "list: max yaw, min pitch, max pitch (deg)", "unset", "three values", "How far the torso turns and tilts."},
              {"", "torso_shake", "a block", "the defaults below", "", "How the torso shakes on a hit: the keys below."},
              {"torso_shake", "pitch_amp", "float (deg)", "28", "", "Its pitch."},
              {"torso_shake", "yaw_amp", "float (deg)", "25", "", "Its yaw."},
              {"torso_shake", "roll_amp", "float (deg)", "12.6", "", "Its roll."},
              {"torso_shake", "freq", "float", "14", "", "How fast it shakes."},
              {"torso_shake", "damp_freq", "float", "2.9", "", "How fast it dies down."},
              {"", "lod_range", "list: two floats", "95 85", "", "The distances its simpler models take over."},
              {"", "lean_angle", "float (deg)", "25", "", "How far it leans into a turn."},
              {"", "lean_factor", "float", "1", "", "How much speed adds to the lean."},
              {"", "lean_speed", "float", "1", "", "How fast it leans."},
              {"", "recoil_bend", "float (deg)", "30", "", "How far firing bends it back."},
              {"", "recoil_slide", "float", "0", "", "How far firing slides it back."},
              {"", "recoil_twist", "float (deg)", "20", "", "How far firing twists it."},
              {"", "aim_slerp_freq", "float", "3", "", "How fast the arms swing to the aim."},
              {"", "aim_min_pitch", "float (deg)", "-80", "", "The lowest the arms aim."},
              {"", "aim_max_pitch", "float (deg)", "70", "", "The highest the arms aim."},
              {"", "aim_min_yaw", "float (deg)", "-80", "", "The furthest left the arms aim."},
              {"", "aim_max_yaw", "float (deg)", "80", "", "The furthest right the arms aim."},
              {"", "aim_arm_pitch_factor", "float", "0.3", "", "How much of the pitch the arms take."},
              {"", "aim_ignore_arms", "flag 0/1", "0", "", "The arms do not aim."},
              {"", "skins", "`NAME (texture ...)` records", "none", "",
               "Its paint schemes: the first in full; `damage` in full; any other is the first plus a suffix."},
              {"", "firepoints", "`NODE ( ... )` records", "none", "", "Where its weapons fire from: the node, then the keys below."},
              {"firepoints/<node>, ammo", "part", "a part's name", "none", "", "The part it sits in (a firepoint's; ammunition's)."},
              {"firepoints/<node>", "radius", "float", "0", "", "A firepoint's spread."},
              {"firepoints/<node>", "share", "float", "0", "", "A firepoint's share of the part."},
              {"", "parts", "a block", "none", "looked up and ignored", "No shipped file has one: every mech gets the same parts."},
              {"", "configurations", "`NAME ( ... )` records", "required", "one at least", "Its loadouts, the name a message key; the keys below."},
              {C, "equipment", "list of names or (name part [tech])", "none", "", "Fitted equipment (AMS, ECM, ...)."},
              {C, "internal", "`endosteel` or another word", "standard", "", "Its internal structure."},
              {C, "engine", "a block", "rating 200", "", "The engine: `rating`, `xl`, `max_turn_rate`."},
              {"configurations/<name>/engine", "rating", "int", "200", "", "The engine's rating."},
              {"configurations/<name>/engine", "xl", "flag", "0", "", "An XL engine."},
              {"configurations/<name>/engine", "max_turn_rate", "float (rad/s)", "the mech's", "", "The loadout's turn rate (not halved, unlike the mech's)."},
              {C, "armor", "a block", "none", "the points must match the mass", "Its armour: the keys below."},
              {A, "ferro-fibrous", "flag", "0", "", "Ferro-fibrous armour."},
              {A, "mass", "float (t)", "0", "> 0; its points must add up", "The armour's weight."},
              {A, "head", "int", "0", "", "Points on the head."},
              {A, "arms", "int", "0", "", "Points on each arm."},
              {A, "legs", "int", "0", "", "Points on each leg."},
              {A, "torso", "list: front rear (int)", "0 0", "", "The centre torso's points."},
              {A, "torsolr", "list: front rear (int)", "0 0", "", "Each side torso's points."},
              {C, "weapons", "list of (weapon firepoint ...)", "none", "16 at most (more are cut)", "Its weapons: weapons.zrd's name, the firepoint, then the keys below."},
              {"configurations/<name>/weapons/(...)", "ammo", "`ammo N [part P] [extra (a b c)]`", "none", "", "Rounds carried, where, and up to three more bins."},
              {"configurations/<name>/weapons/(...)/ammo", "extra", "list of up to 3 ints", "none", "3 at most", "More bins of ammunition."},
              {"configurations/<name>/weapons/(...)", "group", "int", "1", "1 to 5 (clamped)", "Its weapon group."},
              {"", "camera", "a block of views", "none", "", "Its cameras: `tether`, `attach`, `zippercam`, `godseye`, `mcbtether`, `area`."},
              {"camera", "tether", "a camera block", "none", "", "The chase camera."},
              {"camera", "attach", "a camera block", "none", "", "The cockpit camera."},
              {"camera", "zippercam", "a camera block", "none", "", "The zipper camera."},
              {"camera", "godseye", "a camera block", "none", "", "The overhead camera."},
              {"camera", "mcbtether", "a camera block", "none", "", "The second chase camera."},
              {"camera", "area", "a camera block", "none", "", "The fixed camera that keeps the mech in view."},
              {Cam, "dist", "float (m)", "the view's", "", "Its distance."},
              {Cam, "dist_vary_factor", "float", "the view's", "", "How much speed changes the distance."},
              {Cam, "dist_vary", "float", "the view's", "", "The most the distance changes."},
              {Cam, "dist_factor", "float", "the view's", "", "How the distance follows."},
              {Cam, "dist_catch_up", "float", "the view's", "", "How fast the distance catches up."},
              {Cam, "desired_pos", "list: x y z", "the view's", "", "Where it sits."},
              {Cam, "delta_rot", "list: three angles (deg)", "none", "", "How it is turned."},
              {Cam, "look_limits", "a block: left right up down (deg)", "the view's", "", "How far the pilot can look."},
              {"camera/<view>/look_limits", "left", "float (deg)", "the view's", "", "Look left."},
              {"camera/<view>/look_limits", "right", "float (deg)", "the view's", "", "Look right."},
              {"camera/<view>/look_limits", "up", "float (deg)", "45", "", "Look up."},
              {"camera/<view>/look_limits", "down", "float (deg)", "the view's", "", "Look down."},
              {"", "hud", "a block", "none", "", "Puts the HUD model in the cockpit."},
              {"", "animations", "a block", "none", "", "Its effects and motions: `canned`, `motion`."},
              {"animations", "canned", "a block", "none", "", "Effects at fixed places: `eject`, `jumpjets`, `narc`."},
              {"animations/canned", "eject", "`(effect ofs (x y z) node NAME)`", "none", "", "The ejection's effect."},
              {"animations/canned", "jumpjets", "the same", "none", "", "The jump jets' effect."},
              {"animations/canned", "narc", "the same", "none", "", "A NARC beacon's effect."},
              {"animations/canned/<effect>", "ofs", "list: x y z", "0 0 0", "", "The effect's offset."},
              {"animations/canned/<effect>", "node", "a node's name", "none", "", "The node it follows."},
              {"animations", "motion", "a block", "the mech's own motions", "", "Its gaits: `stand`, `step`, `walk`, `trot`, `run`, `limp`."},
              {"animations/motion", "stand", "a gait block", "`<mech>_stand`", "", "Standing."},
              {"animations/motion", "step", "a gait block", "`<mech>_step`", "required somewhere", "Stepping in place."},
              {"animations/motion", "walk", "a gait block", "`<mech>_walk`", "required somewhere", "Walking."},
              {"animations/motion", "trot", "a gait block", "`<mech>_trot`", "", "Trotting."},
              {"animations/motion", "run", "a gait block", "`<mech>_run`", "", "Running."},
              {"animations/motion", "limp", "a gait block", "`<mech>_limp`", "", "Limping (the right limp is the left mirrored)."},
              {"animations/motion/<gait>", "anim", "a motion's name", "`<mech>_<gait>`", "", "The motion it plays."},
              {"animations/motion/<gait>", "trans", "list: low high", "the gait's", "", "The throttle range it plays over."},
          }};
}

}  // namespace upgrades::reference
