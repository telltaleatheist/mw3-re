// Ours: the net editor's forms' tips (help.h), beside game_editor_net.cpp,
// game_editor_tree.cpp, node_params.cpp and add_vehicle.cpp.
#include "upgrades/editor/help.h"

namespace mech3 {

void help_nets(std::vector<HelpForm>& out) {
  out.push_back({"AI Network Editor", "Picking a net",
                 "EDIT AI NETS first asks which net to edit (the original's dialog).",
                 {
                     {"##nets", "the list",
                      "The mission's AI nets (paths), by file: net_01 is usually the main one. New Net makes an "
                      "empty one at the view's pick."},
                     {"OK", "", "Edit the net picked."},
                     {"Cancel", "", "The same as OK, as in the original."},
                 }});
  out.push_back({"Node Parameters", "Node Params",
                 "The first picked node's record, as the net file holds it. A field whose box is unticked is left "
                 "to the game's default. OK puts it on the node; Save Changes writes it.",
                 {
                     {"placed (x y z given)", "", "Whether the node has a place of its own (unticked: an edge node, "
                                                  "which only adds settings to other nodes' links)."},
                     {"x", "", "Its place east-west, in the world's units (metres)."},
                     {"y", "", "Its height. The game puts the node on the ground under it when the mission loads."},
                     {"z", "", "Its place north-south."},
                     {"speed", "", "How fast a vehicle goes along the links out of this node (the file's speed)."},
                     {"wait interval", "", "The interval of the wait at this node (the file's wait_interval)."},
                     {"wait time", "", "How long a vehicle waits at this node (the file's wait_time), in seconds."},
                     {"can leave, first", "",
                      "The file's canleave, first word: with it, a vehicle on these links may break off from the "
                      "net (docs/ai.md)."},
                     {"can leave, second", "", "The canleave pair's second word."},
                     {"jump", "", "A jump on the links out of this node (the file's jump): for mechs with jets."},
                     {"arrange nodes, spacing", "",
                      "arrangenodes: the nodes this one links to are put on a circle this far round it."},
                     {"hidden", "", "The file's hidden flag: as the default, yes or no."},
                     {"fixed", "", "The file's fixed flag: as the default, yes or no."},
                     {"autonext", "", "The file's autonext flag: as the default, yes or no."},
                     {"visible", "", "The file's visible flag: as the default, yes or no."},
                     {"backup", "", "A backup node (the file's backup)."},
                     {"shunt", "", "The file's shunt flag on the node's links."},
                     {"name", "", "The node's name (a message key or text); empty clears it."},
                     {"gate", "", "The gate the links out of this node pass (the file's gate); empty clears it."},
                     {"OK", "", "Put these on the node. Save Changes writes them."},
                     {"Cancel", "", "Leave the node as it was."},
                 }});
  out.push_back({"Node Edit - Vehicle", "Add veh to node",
                 "A new vehicle at the first picked node, following this net. OK makes it at once and writes its "
                 "record into the mission's aiv.zrd in your copy of the campaign, its AI settings copied from the first vehicle of "
                 "that type in the mission.",
                 {
                     {"vehicle type", "", "The kind of vehicle: the 19 mechs, then the vehicles vehicle.zrd lists."},
                     {"x", "", "Where it starts, east-west (the node's place to begin with)."},
                     {"y", "", "Its height (the ground under it is used)."},
                     {"z", "", "Where it starts, north-south."},
                     {"yaw", "", "Which way it faces, in degrees."},
                     {"team id", "", "Its team: 1 your side, 2 the enemy, 0 neither (Friend, Foe, Other set it)."},
                     {"side", "", "Friend, Foe or Other: sets the team id to 1, 2 or 0."},
                     {"OK", "", "Make the vehicle now and write its record (said on the line under the menu)."},
                     {"Cancel", "", "Close without making anything."},
                 }});
  out.push_back({"Delete Node", "Deleting",
                 "Delete Node asks first, as the original did.",
                 {
                     {"OK", "", "Delete the picked node (and its links), or the picked line."},
                     {"Cancel", "", "Keep it."},
                 }});
  out.push_back({"Ai Network Editor", "Change Net Params",
                 "The original's net window: it has no parameters, only Open, which shows a second net beside the "
                 "one edited.",
                 {
                     {"OK", "", "Close the window (the second net goes)."},
                     {"Cancel", "", "Close the window."},
                     {"Open", "", "Choose a net file of the mission to show beside the edited net."},
                 }});
  out.push_back({"Open a net file", "Open",
                 "The mission's net and region files, from the open archives.",
                 {
                     {"##files", "the list", "The files: net_NN.zrd are nets, reg_*.zrd regions."},
                     {"Open", "", "Show the file picked, drawn beside the edited net."},
                     {"Cancel", "", "Close the list."},
                 }});
  out.push_back({"Tree", "Tree View",
                 "The world's objects, the world first, then by area (row and column). It only shows.",
                 {
                     {"##tree", "the tree", "Open an item to see what it holds."},
                     {"OK", "", "Close it."},
                     {"Cancel", "", "Close it."},
                 }});
  out.push_back({"Editor help", "The Help window", "The quick start (above), from the HELP rows.",
                 {
                     {"Close", "", "Close the window."},
                 }});
}

}  // namespace mech3
