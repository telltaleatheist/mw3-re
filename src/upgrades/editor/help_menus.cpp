// Ours: the editor menus' tips (help.h), beside game_editor.cpp's and
// game_editor_net.cpp's row tables.
#include "upgrades/editor/help.h"

namespace mech3 {

void help_menus(std::vector<HelpForm>& out) {
  out.push_back({"main menu", "The editor's menu",
                 "The menu over the mission when the editor is up (the original's, with HELP added in one of its "
                 "blank rows). The view is a free camera, flown with the movement keys.",
                 {
                     {"PICK A COMMAND", "", "The menu's title. Point at a row and click, or press Space."},
                     {"EDIT AI NETS", "",
                      "Edit the AI's paths: pick a net, then move, link, add and delete its nodes. Opens the net "
                      "editor's menu."},
                     {"EDIT VEHICLES", "",
                      "The Mech AI window: pick one of the mission's vehicles and change its AI settings."},
                     {"EDIT WEAPONS", "",
                      "New Weapon: a weapon copied from one the game knows, its name and numbers set, written into "
                      "your copy of the campaign (in a mission opened from MODS, MISSIONS only). Empty in the "
                      "original game; ours."},
                     {"HELP", "", "This quick start, in a window. Ours: the original had none."},
                     {"MISSION SETTINGS", "",
                      "Settings of the mission that is open, written into your copy of the campaign (in a mission "
                      "opened from MODS, MISSIONS only). For now one: how often a kill's salvage is offered in this "
                      "mission. Ours."},
                     {"TELEPORT", "",
                      "Move your mech to a place: the mission's nav points, the world's saved places, or one you "
                      "type in. The developers' own tool, which the original never let you reach; ours."},
                     {"EXIT GAME EDIT", "",
                      "Close the editor: the view goes back to your mech and the mission goes on in play. "
                      "Shift+F12 does the same, and opens the editor again; so does the pause menu's switch "
                      "(EDIT MISSION, PLAY MISSION). Quitting the mission from the pause menu goes back to the "
                      "MODS screen, your copy chosen when a save made one."},
                 }});
  out.push_back({"Teleport", "Teleport",
                 "The developers' teleport, which the original carries but never lets you reach. It lists your "
                 "own place (Current Location), the mission's nav points and the world's saved places (each world "
                 "ships some: mission starts, the Instant Action base). OK moves your mech there and saves the "
                 "places, nav points included, into your copy of the campaign as zbd\\<world>\\location.zrd "
                 "(the one before kept as .bak), as the original saved them. The first save asks the copy's name.",
                 {
                     {"place", "", "Pick a place to fill the numbers, or type a new name and leave the box to add one "
                                   "at the numbers below."},
                     {"x", "", "Where to go: east-west, in metres."},
                     {"y", "", "Height."},
                     {"z", "", "North-south."},
                     {"yaw (deg.)", "", "Which way to face, in degrees (-360 to 360)."},
                     {"OK", "", "Go there, and save the places into your copy of the campaign (the first save asks its name)."},
                     {"Cancel", "", "Close it; stay where you are."},
                     {"Delete", "", "Take the picked place off the list (asked first); saved with OK."},
                 }});
  out.push_back({"Save as a new campaign", "Save as",
                 "The editor never changes the campaign it opened. The first save of a session asks for a name "
                 "and makes a copy of the campaign under it: mods\\<the name>, a mod of your own that holds only "
                 "the files you change and names the campaign it was saved from. The save then goes on into it, "
                 "and so does every later one. The copy is listed among MODS's campaigns with the ticks of the "
                 "campaign it came from. A copy of yours opened in the editor is saved into itself, and this box "
                 "does not come up.",
                 {
                     {"Name", "",
                      "The copy's name on MODS and its folder under mods. Offered: \"<campaign>, my edit\", then "
                      "\"..., my edit 2\". A name whose folder exists is refused and another offered."},
                     {"Save", "", "Make the copy and save into it."},
                     {"Cancel", "", "Close the box: nothing is saved, and the next save asks again."},
                 }});
  out.push_back({"net menu", "The net editor's menu",
                 "The net editor's own menu, shown once a net is picked. Click a node in the world to pick it; "
                 "picking another links the first to it.",
                 {
                     {"Pick CMD or Point", "", "The menu's title: pick a row, or click a node in the world."},
                     {"GOTO Net", "", "Fly the view to the net's first node."},
                     {"Change Net Params", "",
                      "The original's net window: Open shows a second net file beside this one, to compare."},
                     {"Node Params", "",
                      "The first picked node's settings: place, speed, waits, flags, name, gate. Empty in the "
                      "original; ours."},
                     {"Add veh to node", "",
                      "Make a vehicle at the first picked node, on this net, and write it into the mission's "
                      "vehicle list in your copy of the campaign. Did nothing in the original; ours."},
                     {"Move Node", "", "Move the first picked node: then press the mouse, drag it where it should go, let go."},
                     {"Delete Node", "", "Delete the picked node, or the picked line, after asking."},
                     {"Delete Net", "", "Empty in the original game: it does nothing."},
                     {"Save Changes", "",
                      "Write the net into the mission's archive in your copy of the campaign (the first save asks "
                      "the copy's name; one .bak kept of the file replaced). The line under the menu says where."},
                     {"Tree View", "", "A window listing the world's objects by area. It only shows."},
                     {"Help", "", "The quick start, in a window. Ours."},
                     {"Exit Net Editor", "", "Close the net editor and its windows; back to the editor's menu."},
                 }});
}

}  // namespace mech3
