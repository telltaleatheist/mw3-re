// The reference's registries for the mission readers' records, each world's
// reader archive (zbd\<world>\reader*.zbd): aiv.zrd and targets.zrd
// (mech3/zrdraiv.cpp 0x4df730, 0x4e08c0; docs/ai.md section 3), the AI nets
// and regions (zrdrainet.cpp; ai.md), valves.zrd (zrdrvalve.cpp 0x4e2330;
// ai.md, valves), objectives.zrd (mech_objective.cpp 0x0045c2d0;
// docs/objectives.md).
#include "upgrades/reference/reference.h"

namespace upgrades::reference {

File aiv() {
  const char* R = "a record";
  return {"aiv.zrd", "aiv.zrd (a mission's vehicles)", "each world's reader archive",
          "`ZRdrAiv` 0x004df730 (`mech3/zrdraiv.cpp`)",
          "One record per entity, by the name the world gives it: `\"name\" (kind (x y z) yaw [team] tag value ...)`, the "
          "first items by place, then tags. Most tags are lists of numbers by place (docs/ai.md section 3 says where each "
          "lands). The team comes from the name when it is not given: `player` 1, `stealth` 0, others 2.",
          {
              {R, "params", "list of 9 numbers", "the defaults", "", "Sight range, the time it may leave its net, attack range and the rest."},
              {R, "p2", "list of 8 numbers", "the defaults", "", "The attack strategy, three switches and four more."},
              {R, "p3", "list of 6 numbers", "none", "old form; none shipped", "An older order block."},
              {R, "p4", "list of 23 items", "none", "old form; none shipped", "An older inline order (its 23rd item a valve)."},
              {R, "p5", "list of 5 numbers", "none", "old form; none shipped", "Older jump settings."},
              {R, "p6", "list of 4 numbers", "the defaults", "", "Four more settings."},
              {R, "p7", "list of 8 numbers", "none", "old form; none shipped", "Older scan settings."},
              {R, "p8", "list of 17 numbers", "the defaults", "not with p3, p4, p5 or p7", "A random wait's bounds, the scans and the rest."},
              {R, "p9", "list of 5 numbers", "the defaults", "", "Five more settings, one a switch."},
              {R, "p10", "list of 2 numbers", "the defaults", "", "Two more settings."},
              {R, "p11", "list of 1 number", "the default", "", "One more setting."},
              {R, "p12", "list: mech flag flag", "none", "a mech's name", "The mech it uses, and two switches."},
              {R, "p13", "list of 1 number", "0", "", "A switch."},
              {R, "pilot", "list: name, then `can_configure`", "none", "", "The pilot's name (a message key)."},
              {"a record/pilot", "can_configure", "flag", "0", "", "The pilot's loadout can be changed in the mech lab."},
              {R, "sound_tag", "text", "none (dom, alan, epona for the three lancemates' records)", "", "The radio voice's sounds."},
              {R, "skin", "list of 1 number", "0", "", "Its paint scheme."},
              {R, "lancemate", "0/1/2 or on/off/none/default", "default", "", "Whether it is the player's lancemate."},
              {R, "vlist", "list of order lists (17 or 18 items)", "none", "", "Triggers: ranges, a region, switches and a valve to set."},
              {R, "mech3", "list of 1 number", "unset", "", "A flag the MW3 code reads."},
          },
          ""};
}

File targets() {
  const char* R = "a record";
  return {"targets.zrd", "targets.zrd (the objects one can target)", "each world's reader archive",
          "`ZRdrTarget` 0x004e08c0 (`mech3/zrdraiv.cpp`)",
          "One record per world object by its name: `\"name\" (kind key value ...)`.",
          {
              {R, "health", "number", "none", "", "Its health."},
              {R, "translate_to_anim_root", "a key alone", "no", "", "Moves it with its animation's root."},
              {R, "destroy_anim", "an animation", "none", "", "Played when it is destroyed."},
              {R, "target", "list: node [(x y z)]", "none", "", "The node aimed at, and an offset."},
              {R, "centroid", "list: node [(x y z)]", "none", "", "Its centre, and an offset."},
              {R, "name", "a message key", "none", "", "The name shown when targeted."},
              {R, "radar", "flag", "the default", "", "Shown on radar."},
              {R, "display", "flag", "the default", "", "Shown on the HUD."},
              {R, "reticule_select", "flag", "the default", "", "Can be picked with the reticle."},
              {R, "zoom_select", "flag", "the default", "", "Shown in the zoom window."},
              {R, "height", "float", "none", "", "Its height."},
              {R, "health_below", "list: valve, then health values", "none", "", "Sets a valve as its health falls below each value."},
          },
          ""};
}

File nets() {
  const char* N = "a node";
  return {"net", "net_NN.zrd, waynet_NN.zrd (the AI's paths)", "each world's reader archive",
          "`ZRdrAiNet` (`mech3/zrdrainet.cpp`; docs/ai.md)",
          "A net: `version`, `name`, `netparams`, then `node_N` records (found by their prefix) with their links and the "
          "keys below. Most keys are lists whose first item switches the setting on.",
          {
              {"", "version", "text", "required", "`mech3`", "The format."},
              {"", "name", "text", "the file's", "", "The net's name (a node's `name` too)."},
              {"", "netparams", "a block", "none", "", "The net's own settings."},
              {"netparams", "pos", "list: x y z", "none", "", "The net's place.", true},
              {N, "speed", "list: on speed", "off", "", "The speed along it."},
              {N, "canleave", "list: two numbers", "0 0", "", "Whether a vehicle may leave the net here."},
              {N, "wait_interval", "list", "off", "", "A random wait between visits."},
              {N, "wait_time", "list: on time", "off", "", "How long to wait here."},
              {N, "valve", "list", "none", "", "A valve this node sets or waits on."},
              {N, "auxname", "text", "none", "", "A second name."},
              {N, "jump", "list: on value", "off", "", "Jump here."},
              {N, "scan_aim", "list: two numbers", "0 0", "", "Scan by aim here."},
              {N, "scan_time", "list: two numbers", "0 0", "", "Scan for a time here."},
              {N, "scan_hit", "list: two numbers", "0 0", "", "Scan when hit here."},
              {N, "hidden", "flag", "unset", "", "Hidden."},
              {N, "fixed", "flag", "unset", "", "Fixed."},
              {N, "autonext", "flag", "unset", "", "Goes on to the next node by itself."},
              {N, "visible", "flag", "unset", "", "Visible."},
              {N, "deploy", "list", "none", "", "Deploys something here."},
              {N, "mcbwait", "list", "none", "", "A multiplayer wait."},
              {N, "arrangenodes", "number", "unset", "", "Spreads vehicles over the nodes."},
              {N, "sound", "list", "none", "", "A sound played here."},
              {N, "backup", "a key alone", "no", "", "Backs up to here."},
              {N, "gate", "text", "none", "", "A gate's name."},
              {N, "shunt", "a key alone", "no", "", "Shunts here."},
              {N, "valveunion", "list", "none", "", "Valves joined here."},
              {N, "valve_assign", "list", "none", "", "Valves set here."},
          }};
}

File regions() {
  return {"region", "reg_<name>.zrd (the AI's regions)", "each world's reader archive",
          "`ZRdrAiNet`'s region form (`mech3/zrdrainet.cpp`; docs/ai.md, Regions)",
          "A region is a net file whose `node_N` records outline an area by place; the nets' keys apply, of which only these "
          "are looked up for a region.",
          {
              {"", "version", "text", "required", "`mech3`", "The format."},
              {"", "name", "text", "the file's", "", "The region's name (orders and objectives name it)."},
              {"", "netparams", "a block", "none", "", "Its own settings."},
          }};
}

File valves() {
  const char* R = "a valve";
  return {"valves.zrd", "valves.zrd (a mission's switches)", "each world's reader archive",
          "the valve reader 0x004e2330 (`mech3/zrdrvalve.cpp`; docs/ai.md, valves)",
          "One record per valve by its name. A condition tag makes the valve's value from others named in its `namelist`; "
          "the action tags fire when the valve takes their value (`update` and `delayupdate` on any change). Lists of raw "
          "words; the second of a pair wins (`sound_once` after `sound`).",
          {
              {R, "value", "list: n", "0", "", "Its starting value."},
              {R, "any_nonzero", "a condition", "none", "one condition a valve", "On while any of `namelist` is."},
              {R, "any_zero", "a condition", "none", "one condition a valve", "On while any of `namelist` is off."},
              {R, "all_nonzero", "a condition", "none", "one condition a valve", "On while all of `namelist` are."},
              {R, "all_zero", "a condition", "none", "one condition a valve", "On while all of `namelist` are off."},
              {R, "namelist", "list of valve names", "none", "not empty with a condition", "The valves a condition reads."},
              {R, "sound", "list: sound n", "none", "", "Plays a sound at value n."},
              {R, "sound_once", "list: sound n", "none", "", "The same, the first time only."},
              {R, "animation", "list: animation n", "none", "", "Runs a mission animation at value n."},
              {R, "animation_once", "list: animation n", "none", "", "The same, the first time only."},
              {R, "scan_aim", "list: vehicle on n", "none", "", "Switches a vehicle's aim scan."},
              {R, "scan_hit", "list: vehicle on n", "none", "", "Switches a vehicle's hit scan."},
              {R, "scan_time", "list: vehicle on n", "none", "", "Switches a vehicle's timed scan."},
              {R, "canleave", "list: vehicle on n", "none", "", "Lets a vehicle leave its net."},
              {R, "shutdown", "list: vehicle down n", "none", "", "Powers a mech down or up."},
              {R, "destroy", "list: entity n", "none", "", "Destroys an entity."},
              {R, "setteam", "list: entity team n", "none", "", "Changes an entity's side."},
              {R, "attack_strategy", "list: entity strategy n", "none", "", "Changes a vehicle's attack strategy."},
              {R, "teleport", "list: entity (x y z) yaw n", "none", "", "Moves an entity."},
              {R, "missionreset", "list: n", "none", "only reported", "Nothing happens (the original's)."},
              {R, "missionabort", "list: n", "none", "", "Aborts the mission."},
              {R, "missioncomplete", "list: n", "none", "does nothing", "Nothing happens (the original's)."},
              {R, "update", "list: valve", "none", "", "Fires the named valve's reactions."},
              {R, "delayupdate", "list: valve seconds", "none", "", "Sets the named valve to this one's value after a delay."},
          },
          ""};
}

File objectives() {
  const char* M = "m<n>";
  const char* C = "a condition";
  return {"objectives.zrd", "objectives.zrd (a mission's goals)", "each world's reader archive",
          "`objectives_load` 0x0045c2d0 and the condition reader 0x0045c920 (`mech3/mech_objective.cpp`; docs/objectives.md)",
          "One block per mission (`m1`, `m2`, ...). Each group holds `type (arguments)` pairs; the types are destroy_entity, "
          "node_change, reach_area, valve_change, obj_list, and training's weapon_fire, heat_warning, target_object, "
          "weapon_select, time_limit, face_object, reach_velocity, aim_at_object, view_object, key_press, mouse_press, "
          "joystick_press, weapon_group_select, User_Command. Lookups search depth first, so a nested condition's key can "
          "answer for its parent's.",
          {
              {M, "desc", "a message key", "none", "", "The mission's text (a condition's own `desc` too)."},
              {M, "primary", "a group", "none", "", "The goals that win the mission."},
              {M, "secondary", "a group", "none", "", "Optional goals."},
              {M, "final", "a group", "none", "", "Goals after the primary ones."},
              {M, "fail", "a group", "none", "one failed condition fails", "What loses the mission."},
              {C, "sound", "a sound", "none", "", "Played when it is met."},
              {C, "visible", "flag", "1", "", "Shown in the list."},
              {C, "silent", "flag", "0", "", "Met without a word."},
              {C, "salvage", "a store", "none", "", "Salvage it gives: `mechs`, `weapons`, `equipment`, `ammo`, `engines`."},
              {"salvage", "mechs", "list", "none", "", "Mechs salvaged."},
              {"salvage", "weapons", "list", "none", "", "Weapons salvaged."},
              {"salvage", "equipment", "list", "none", "", "Equipment salvaged."},
              {"salvage", "ammo", "list", "none", "", "Ammunition salvaged."},
              {"salvage", "engines", "list", "none", "", "Engines salvaged."},
              {C, "cond_salvage", "list of node_state entries", "none", "", "Salvage given if a node is (or is not) active."},
              {C, "persist", "a copy", "none", "", "Kept to the next mission."},
              {C, "set_valve", "list: valve value", "none", "", "Sets a valve when it is met."},
              {C, "nav", "a nav point", "none", "", "The nav point it shows."},
              {"node_change, aim_at_object", "node", "a world node [subnodes]", "none", "", "The node watched."},
              {"node_change", "state", "`active` or another word", "none", "", "Met when the node is (or is not) active."},
              {"reach_area", "radius", "float (m)", "25", "", "How close to come."},
              {"reach_area", "point", "list: x y z", "none", "used without node or nav", "The place to reach.", true},
              {"valve_change", "valve", "a valve", "none", "", "The valve watched."},
              {"valve_change", "value", "int", "1", "", "The value it must take."},
              {"obj_list", "list", "conditions", "none", "", "Its conditions."},
              {"obj_list", "goal_count", "int", "all", "", "How many must be met."},
              {"obj_list", "simultaneous", "flag", "0", "", "All at the same time."},
              {"destroy_entity", "group", "a group's name", "none", "", "A group of entities to destroy.", true},
              {"target_object, view_object, face_object", "entity", "an entity's name", "none", "", "The entity.", true},
              {"target_object", "part", "a part's name", "any", "", "The part aimed at.", true},
              {"reach_velocity", "test", "GT GE EQ NE LE LT", "GE", "", "How the speed is compared.", true},
              {"reach_velocity", "velocity", "float", "none", "", "The speed.", true},
              {"User_Command", "cmd", "a CMD_ name", "none", "", "The command whose key or button counts.", true},
              {"key_press", "key", "key names (KEY_CTRL, KEY_ALT, KEY_SHIFT)", "none", "", "The key.", true},
              {"mouse_press, joystick_press", "button", "left/right/middle, or a number", "none", "", "The button.", true},
              {"weapon_fire, weapon_select", "weapon", "a weapon's name", "none", "", "The weapon.", true},
          },
          ""};
}

}  // namespace upgrades::reference
