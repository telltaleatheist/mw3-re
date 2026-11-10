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
                      "the mod (opened from MODS, EDITOR only). Empty in the original game; ours."},
                     {"HELP", "", "This quick start, in a window. Ours: the original had none."},
                     {"EXIT GAME EDIT", "",
                      "Close the editor. From MODS's EDITOR it goes back to the MODS screen; in a custom campaign's "
                      "mission the view goes back to your mech and the mission goes on."},
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
                      "vehicle list. Did nothing in the original; ours."},
                     {"Move Node", "", "Move the first picked node: then press the mouse, drag it where it should go, let go."},
                     {"Delete Node", "", "Delete the picked node, or the picked line, after asking."},
                     {"Delete Net", "", "Empty in the original game: it does nothing."},
                     {"Save Changes", "",
                      "Write the net into the mission's archive in the mod (one .bak kept of the file replaced). "
                      "The line under the menu says where."},
                     {"Tree View", "", "A window listing the world's objects by area. It only shows."},
                     {"Help", "", "The quick start, in a window. Ours."},
                     {"Exit Net Editor", "", "Close the net editor and its windows; back to the editor's menu."},
                 }});
}

}  // namespace mech3
