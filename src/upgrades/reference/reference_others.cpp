// The reference's registries for the other reader files a mission reads:
// aiTurrets.zrd (mech3/turret.cpp 0x004a6270), vehicle.zrd (veh_controls.cpp,
// veh_basic_object.cpp, veh_stealth.cpp, player.cpp; docs/vehicles.md),
// world.zrd (mission_world.cpp), landmines.zrd (landmines.cpp 0x004135c0),
// hud.zrd (hud.cpp; docs/hud.md) and sounds.zrd (gamez/zsound/zsnd_init.cpp,
// mech3/sound_extra.cpp 0x004e6b00).
#include "upgrades/reference/reference.h"

namespace upgrades::reference {

File turrets() {
  const char* T = "TURRET/<turret>";
  return {"aiturrets.zrd", "aiTurrets.zrd (the turrets)", "`zbd\\reader.zbd`", "`Turret::setup` 0x004a6270 (`mech3/turret.cpp`)",
          "`GENERIC_DESTROY_ANIM`, then `TURRET` holding one record per turret type, by the name the world's turret nodes "
          "carry (docs/vehicles.md, The turrets).",
          {
              {"", "GENERIC_DESTROY_ANIM", "an animation", "none", "", "Played when a turret without its own is destroyed."},
              {"", "TURRET", "`NAME ( ... )` records", "none", "", "The turret types."},
              {T, "PARTS", "list of 2 to 4 node names", "none", "", "Its turning part, its gun and one or two firepoints."},
              {T, "DEACTIVATE", "list: node [subnodes]", "none", "", "A node whose loss shuts it down."},
              {T, "EFFECT", "list: node time", "none", "", "A scrolling effect node, and how long it runs."},
              {T, "HEALTH", "float", "1", "", "Its health."},
              {T, "DESTROY_ANIM", "an animation", "the node's or the generic", "", "Played when it is destroyed."},
              {T, "ONFIRE_ANIM", "list of animations", "none", "", "Played as it fires."},
              {T, "WAKEUP_ANIM", "list of animations", "none", "", "Played as it wakes."},
              {T, "SLEEP_ANIM", "list of animations", "none", "", "Played as it sleeps."},
              {T, "AUX_ANIM", "list of animations", "none", "", "Others it runs."},
              {T, "IS_IDLE", "flag", "unset", "", "Starts idle."},
              {T, "SOUNDS", "a block", "none", "", "Its sounds: `FIRE`."},
              {"TURRET/<turret>/SOUNDS", "FIRE", "a sound", "none", "", "Played as it fires."},
              {T, "LIMITS", "a block", "none", "", "How far it turns: `YAW`."},
              {"TURRET/<turret>/LIMITS", "YAW", "list: min max (deg)", "-360 360", "", "Its yaw range."},
              {T, "BBOX", "int", "1", "", "Hit by its box (1) or its polygons."},
              {T, "WEAPON", "a block", "none", "", "Its weapon: the keys below."},
              {"TURRET/<turret>/WEAPON", "NAME", "a weapons.zrd name", "none", "", "The weapon."},
              {"TURRET/<turret>/WEAPON", "AMMO", "int", "0 (unlimited)", "", "Its rounds."},
              {"TURRET/<turret>/WEAPON", "DETECTION_RANGE", "float (m)", "the weapon's range (200 without one)", "", "How far it sees."},
              {"TURRET/<turret>/WEAPON", "FIRE_LIMITS", "list: two floats", "none", "", "Its bursts' bounds."},
              {"TURRET/<turret>/WEAPON", "FIRE_RATE", "float", "the weapon's", "", "How fast it fires."},
              {T, "AMS_RANGE", "float (m)", "no AMS", "", "An anti-missile system with this range."},
              {T, "TURRET_HEIGHT", "float (m)", "the model's", "", "Its height."},
          },
          "TURRET"};
}

File vehicles() {
  const char* V = "<vehicle>";
  const char* M = "<vehicle>/common_mode";
  return {"vehicle.zrd", "vehicle.zrd (the vehicles)", "`zbd\\reader.zbd`",
          "the vehicle setup (`mech3/veh_controls.cpp`, `veh_basic_object.cpp`, `veh_stealth.cpp`, `player.cpp`; docs/vehicles.md)",
          "One record per vehicle type by name (`apc`, `owens`, `mobilehq`, `stealth`, ...; aiv.zrd's records name them). "
          "Its `common_mode` block holds the motion's keys (docs/vehicles.md, The motion block).",
          {
              {V, "health", "float", "the default", "", "Its health."},
              {V, "start_anims", "list of animations", "none", "", "Started when it is made."},
              {V, "sounds", "a block", "none", "", "Its sounds: `engine`."},
              {"<vehicle>/sounds", "engine", "a sound", "none", "", "Its engine's loop."},
              {V, "motion_anim", "an animation", "none", "", "Its motion animation."},
              {V, "weapons", "list of (weapon ...)", "none", "", "Its weapons and their slots."},
              {"<vehicle>/weapons", "firepoint", "a node's name", "`firepoint`", "", "Where the weapons fire from."},
              {V, "common_mode", "a block", "none", "", "Its motion: the keys below."},
              {M, "platform", "list of points", "the model's support points", "required without them", "Its supports."},
              {M, "collision", "list of points", "the model's collide points", "required without them", "Its collision points."},
              {M, "rates", "list: two floats", "10 30", "", "Its acceleration rates."},
              {M, "friction", "list: three floats", "10000 10 0", "", "Its friction."},
              {M, "stopping", "float", "8", "", "How fast it stops."},
              {M, "quicksand_slowdown", "float", "0.9", "", "Its speed in quicksand."},
              {M, "lava_slowdown", "float", "0.8", "", "Its speed on lava."},
              {M, "turns", "list: two floats", "0.6 2", "", "How it turns."},
              {M, "turn_damping", "float", "30", "", "How its turning damps."},
              {M, "rate_damping", "list: two floats", "30 30", "", "How its speed damps."},
              {M, "a_damping", "float", "8", "", "How its tilt damps."},
              {M, "mass", "float", "1", "", "Its mass."},
              {M, "gun_pitch", "list: min (deg) and a rate", "-15 0.5", "", "Its gun's pitch."},
              {M, "mode", "`basic`, `fly`, `sub`, `track`, `hover` or `amphib`", "track", "by its first two letters", "How it moves."},
              {M, "amphib_wave", "list", "none", "the last given wins", "Its bobbing on water."},
              {M, "hover_wave", "list", "none", "the last given wins", "Its hovering bob."},
              {M, "sub_wave", "list", "none", "the last given wins", "Its bob under water."},
              {M, "mode_alt", "float", "2", "", "Its height over the ground."},
              {M, "chas_smooth", "float", "0", "", "How its chassis follows."},
              {M, "chas_pitch", "list", "0", "", "Its chassis' pitch."},
              {M, "chas_roll", "list", "0", "", "Its chassis' roll."},
              {M, "collision_d", "list: two floats", "0.5 0.15", "", "Its collision response."},
          },
          ";@top"};
}

File world() {
  const char* M = "m<n>";
  return {"world.zrd", "world.zrd (a mission's sky and weather)", "each world's reader archive",
          "the world parameters (`mech3/mission_world.cpp`; multiplayer's `mpinfo.zrd` `world_params` the same)",
          "One block per mission (`m1`, `m2`, ...).",
          {
              {M, "fog", "a block", "no fog", "", "Fog: the keys below."},
              {"m<n>/fog, underwater", "range", "list: near far", "the world's", "", "Where it starts and ends."},
              {"m<n>/fog, underwater", "hwrange", "list: near far", "range", "hardware rendering only", "The same on hardware."},
              {"m<n>/fog, underwater, weather", "color", "list: r g b", "the world's", "", "Its colour."},
              {"m<n>/fog, underwater", "altitude", "list: low high", "the world's", "", "Its height band."},
              {"m<n>/fog", "state", "int", "0", "", "Its state."},
              {"m<n>/fog", "node", "a node's name", "required with fog", "", "The sky node."},
              {M, "underwater", "a block", "none", "", "Under water: range, hwrange, color, altitude."},
              {M, "jet_ceiling", "float (m)", "150", "", "How high jump jets reach."},
              {M, "global_wind", "a block", "none", "", "The wind: the keys below."},
              {"m<n>/global_wind", "max_speed", "float", "unset", "", "Its top speed."},
              {"m<n>/global_wind", "random_accel", "float", "unset", "", "How it gusts."},
              {"m<n>/global_wind", "random_angvel", "float (deg)", "unset", "", "How it veers."},
              {"m<n>/global_wind", "velocity", "list: x y z", "unset", "", "Its steady part."},
              {M, "sun", "a block", "the world's", "", "The sun: the keys below."},
              {"m<n>/sun", "orientation", "list: three angles (deg)", "the world's", "", "Where it shines from."},
              {"m<n>/sun", "diffuse", "float", "the world's", "", "Its light."},
              {"m<n>/sun", "ambient", "float", "the world's", "", "The ambient light."},
              {M, "ambient_temperature", "float", "20", "> 0, else 20", "The air's temperature (heat)."},
              {M, "weather", "a block", "none", "", "Snow or rain: the keys below."},
              {"m<n>/weather", "particles", "int", "100", "", "How many."},
              {"m<n>/weather", "type", "`SNOW` or `RAIN`", "none", "", "Which."},
              {"m<n>/weather", "wind", "a block: dir vel", "0 0", "", "How it drifts."},
              {"m<n>/weather/wind", "dir", "float", "0", "", "Its direction."},
              {"m<n>/weather/wind", "vel", "float", "0", "", "Its speed."},
              {"m<n>/weather", "gravity", "float", "0", "", "How fast it falls."},
              {"m<n>/weather", "alpha_gradient", "list: two floats", "0 0", "", "How it fades."},
              {M, "fluctuate", "a block", "none", "", "Moving cloud shadows: the keys below."},
              {"m<n>/fluctuate", "period", "float", "5", "", "Their period."},
              {"m<n>/fluctuate", "wavelength", "float", "500", "", "Their size."},
              {"m<n>/fluctuate", "base", "float", "0.7", "", "The light's base."},
              {"m<n>/fluctuate", "amplitude", "float", "0.3", "", "How much it varies."},
              {"m<n>/fluctuate", "heading", "float (deg)", "0", "", "Which way they move."},
          },
          ""};
}

File landmines() {
  return {"landmines.zrd", "landmines.zrd (a world's mines)", "each world's reader archive", "`landmines_load` 0x004135c0 (`mech3/landmines.cpp`)",
          "`weapon` and `radius`, then one block per mission (`m1`, ...) of `(x y z)` points, each an armed mine.",
          {
              {"", "weapon", "a weapons.zrd name", "required", "", "What a mine is."},
              {"", "radius", "float (m)", "20", "", "How close sets one off."},
          },
          "m<n>"};
}

File hud() {
  const char* B = "V<w>x<h>";
  return {"hud.zrd", "hud.zrd (the cockpit's layout)", "`zbd\\reader.zbd`", "the HUD (`mech3/hud.cpp`; docs/hud.md)",
          "One block per screen size (`V640x480`, `V800x600`, ... `V1600x1200`); the port takes `V640x480` and moves each "
          "piece with the screen's edge it belongs to.",
          {
              {B, "MFD", "a block", "none", "", "The multi-function display: position, size."},
              {B, "TARGET", "a block", "none", "", "The target window: position, size."},
              {B, "TIMER", "a block", "none", "", "The timer: position."},
              {B, "WEAPONS", "a block", "none", "", "The weapon list: position, width."},
              {B, "EQUIPMENT", "a block", "none", "", "The equipment list: position, width."},
              {B, "COMPASS", "a block", "none", "", "The compass: tape, frame."},
              {B, "GAUGES", "a block", "none", "", "The gauges: the frames and the heat pieces."},
              {"MFD, TARGET, TIMER, WEAPONS, EQUIPMENT", "position", "list: x y", "none", "", "Where it sits."},
              {"MFD, TARGET", "size", "list: w h", "none", "", "Its size."},
              {"WEAPONS, EQUIPMENT", "width", "int", "the default", "", "Its width."},
              {"COMPASS", "tape", "list: x y", "none", "", "The tape's place."},
              {"COMPASS", "frame", "list: x y", "none", "", "The frame's place."},
              {"GAUGES", "speed_frame", "list: x y", "none", "", "The speed gauge."},
              {"GAUGES", "fuel_frame", "list: x y", "none", "", "The jump fuel gauge."},
              {"GAUGES", "coolant_frame", "list: x y", "none", "", "The coolant gauge."},
              {"GAUGES", "heat_bar", "list: x y", "none", "", "The heat bar."},
              {"GAUGES", "heat_meter", "list: x y", "none", "", "The heat meter."},
              {"GAUGES", "heat_text", "list: x y", "none", "", "The heat number."},
              {"GAUGES", "heat_arrow", "list: x y", "none", "", "The heat arrow."},
          },
          ""};
}

File sounds() {
  const char* S = "SETS/<set>/<sound>";
  return {"sounds.zrd", "sounds.zrd (the sounds)", "`zbd\\reader.zbd`", "zSound's reader (`gamez/zsound/zsnd_init.cpp`)",
          "The sound settings, then `SETS`: each set's sounds by name with their keys, and `SOUND_GROUPS`. A sound's "
          "flags are keys alone.",
          {
              {"", "SYNTAX", "int", "1", "", "The file's form."},
              {"", "CD_TRACKS", "a block", "none", "", "The music tracks."},
              {"", "SOUND_PATH", "text", "none", "", "Added to where loose sound files are looked for."},
              {"", "HIGH_DEFAULT", "a format block", "none", "", "The default sound format at high quality."},
              {"", "MEDIUM_DEFAULT", "a format block", "none", "", "At medium quality."},
              {"", "LOW_DEFAULT", "a format block", "none", "", "At low quality."},
              {"", "SPEED_OF_SOUND", "float", "the default", "", "The speed of sound (the Doppler effect)."},
              {"", "PLAYBACK_FORMAT", "a block: HIGH MEDIUM LOW", "none", "", "The output format by quality."},
              {"PLAYBACK_FORMAT, a sound", "HIGH", "list: rate bits channels", "none", "", "At high quality."},
              {"PLAYBACK_FORMAT, a sound", "MEDIUM", "list: rate bits channels", "none", "read with bug fixes on", "At medium quality."},
              {"PLAYBACK_FORMAT, a sound", "MED", "list", "none", "the original's spelling, read with bug fixes off",
               "Never in the data: the original's medium quality took nothing.", true},
              {"PLAYBACK_FORMAT, a sound", "LOW", "list: rate bits channels", "none", "", "At low quality."},
              {"", "SETS", "`NAME ( ... )` records", "none", "", "The sound sets."},
              {"", "DEMO_SETS", "the same", "none", "the demo build only", "The demo's sets.", true},
              {"SETS/<set>", "SUB_SET", "list of set names", "none", "", "Sets loaded with it."},
              {S, "3D", "a key alone", "off", "", "Placed in the world."},
              {S, "LOOPED", "a key alone", "off", "", "Loops."},
              {S, "FREQUENCY", "a key alone", "off", "", "Its pitch can change."},
              {S, "HARDWARE", "a key alone", "off", "", "Wants hardware mixing."},
              {S, "PURGEABLE", "a key alone", "off", "", "Can be unloaded."},
              {S, "VOICE", "a key alone", "off", "", "A voice (radio)."},
              {S, "QUEUE", "list", "none", "", "Queued after others."},
              {S, "VOLUME", "float", "1", "", "Its volume."},
              {S, "A3DDIST", "float", "the default", "", "Its 3D distance."},
              {S, "RANGE", "list", "none", "", "How far it is heard."},
              {S, "MINDELAY", "float (s)", "0", "", "The least time between two plays."},
              {S, "SUBTITLE", "a message key", "none", "", "Its subtitle."},
              {"", "SOUND_GROUPS", "a block", "none", "", "The sound groups."},
              {"", "DEMO_GROUPS", "a block", "none", "the demo build only", "The demo's groups.", true},
          },
          "SETS"};
}

}  // namespace upgrades::reference
