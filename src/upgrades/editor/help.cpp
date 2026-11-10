#include "upgrades/editor/help.h"

#include "mech3/game_editor.h"

namespace mech3 {

namespace {
EditorForm g_window;

std::string form_key(const std::string& title) {
  size_t end = title.find("##");
  const size_t colon = title.find(':');
  if (colon < end) end = colon;
  return title.substr(0, end);
}
std::string trimmed(const char* s) {
  std::string t = s ? s : "";
  while (!t.empty() && t.front() == ' ') t.erase(t.begin());
  return t;
}
}  // namespace

const std::vector<HelpForm>& help_forms() {
  static const std::vector<HelpForm> all = [] {
    std::vector<HelpForm> v;
    help_menus(v);
    help_nets(v);
    help_vehicles(v);
    help_weapons(v);
    return v;
  }();
  return all;
}

const char* help_tip(const std::string& form_title, const std::string& item) {
  for (const std::string& key : {form_title, form_key(form_title)})
    for (const HelpForm& f : help_forms())
      if (key == f.form)
        for (const HelpTip& t : f.tips)
          if (item == t.item) return t.text;
  return nullptr;
}

const char* editor_row_tip() {
  const EditorState& s = editor_state();
  if (!s.active || !s.panel || !s.menu || s.panel->hovered < 0 || s.panel->hovered >= s.menu->count) return nullptr;
  const char* table = s.menu == &editor_main_menu_rows() ? "main menu" : "net menu";
  return help_tip(table, trimmed(s.menu->rows[s.panel->hovered].text));
}

const std::vector<std::string>& help_quick_start() {
  static const std::vector<std::string> v = {
      "Open the editor from the MODS screen: pick a mod or custom campaign, then EDITOR and a mission, and it opens "
      "with the editor up. A custom campaign played from MODS (ENTER) has it in every mission too: EDITOR in the "
      "pause menu, or Shift+F12. Its saves go into that mod. The game's own campaign, Pirate Missions, Instant "
      "Action, training and multiplayer have no editor.",
      "The menu is on the left; point at a row and click (or press Space). The view is a free camera: fly it with "
      "the movement keys.",
      "EDIT AI NETS: pick a net. Its nodes and links are drawn in the world. Click a node to pick it; picking a "
      "second one links the first to it. Move Node, Delete Node, Node Params and Add veh to node work on the "
      "first picked node.",
      "EDIT VEHICLES: pick a vehicle, change its pages, then Apply (the running mission takes it at once) and Save "
      "(it is written for the next start).",
      "Save Changes (in the net menu) writes the nets. Each save says where: mods\\<the mod>\\zbd\\... or "
      "mods\\editor edits\\zbd\\...",
      "To undo a save, the file it replaced is kept beside it as <file>.bak: put the .bak back in its place. For "
      "\"editor edits\", turning that mod off in MODS undoes them all.",
      "Every field, button and row has a tip: hold the mouse over it. The full guide is docs/editor-guide.md.",
  };
  return v;
}

const std::vector<std::string>& help_workflow() {
  static const std::vector<std::string> v = {
      "Make the mod: a folder under mods in your write folder with a mod.txt (name=My Mod) and campaign=my.txt, "
      "and my.txt listing the missions to change (name=My Campaign, then mission=c1 1, one line each; "
      "docs/modding.md).",
      "Open it: on the MODS screen pick the campaign, EDITOR lists its missions, ENTER opens the one picked. The "
      "mission starts with the editor up and a stand-in pilot. Its saves go into the mod itself (zbd\\c1\\"
      "readerm1.zbd and the like), each keeping the file it replaced as .bak.",
      "Change the AI's paths: EDIT AI NETS, pick a net (net_01 is usually the main one). Pick a node and move it "
      "(Move Node, then press the mouse, drag, let go), link nodes (pick one, then the next), delete what is not "
      "wanted, set a node's speed or wait (Node Params). Save Changes.",
      "Add a vehicle: pick a node, Add veh to node, choose its type and side, OK. It is made at once and written "
      "into the mission's vehicle list (aiv.zrd).",
      "Change a vehicle's AI: EDIT VEHICLES, pick it, set its pages (when it wakes, how far it follows, how it "
      "attacks and retreats, what valve it sets), Apply to try it in the running mission, Save to keep it.",
      "Add a weapon: EDIT WEAPONS (in a mission opened from MODS, EDITOR), pick the weapon to copy, set its name and "
      "numbers, OK. It is written into the mod (mod.txt and zbd\\reader.zbd); the mech lab offers it from the next "
      "start.",
      "Try it: EXIT GAME EDIT goes back to MODS. Play the campaign (ENTER on the MODS screen) to see the changes "
      "as a player would.",
      "Undo: every save keeps the file it replaced as <file>.bak; put it back to undo that save.",
  };
  return v;
}

EditorForm& help_window() { return g_window; }

// The HELP rows (ours, in the menus' empty rows).
void help_open(int32_t) {
  g_window = EditorForm{};
  g_window.title = "Editor help";
  for (const std::string& p : help_quick_start()) g_window.fields.push_back({p, FormField::kLabel});
  g_window.buttons = {{"Close", [] { form_hide(&g_window); }}};
  form_show(&g_window);
}

std::string editor_guide_markdown() {
  std::string o;
  o += "# The game editor: a guide\n\n";
  o += "<!-- Generated from src/upgrades/editor/help*.cpp (the editor's tooltips): build/mw3 --editor-guide "
       "docs/editor-guide.md. Do not edit by hand; the test editor_guide_is_current fails when it is stale. -->\n\n";
  o += "MechWarrior 3 shipped with the developers' own editor still inside: a menu over the running mission for "
       "the AI's paths (nets) and the vehicles' AI. The rebuild opens it to players and modders, makes the parts "
       "the release left empty work, and saves what you change into a mod. The menus are the original's; the "
       "dialogs are windows of the rebuild's own. Every field, button and row in the editor has a tip (hold the "
       "mouse over it); this guide gathers them. How it works inside: docs/editor.md.\n\n";
  o += "## Quick start\n\n";
  int n = 0;
  for (const std::string& p : help_quick_start()) o += std::to_string(++n) + ". " + p + "\n";
  o += "\n## From a fresh mod to a changed mission\n\n";
  n = 0;
  for (const std::string& p : help_workflow()) o += std::to_string(++n) + ". " + p + "\n";
  for (const HelpForm& f : help_forms()) {
    o += "\n## " + std::string(f.heading) + "\n\n";
    if (*f.about) o += std::string(f.about) + "\n\n";
    o += "| Item | What it does |\n|---|---|\n";
    for (const HelpTip& t : f.tips) {
      std::string name = *t.name ? t.name : t.item;
      std::string text = t.text;
      for (size_t p = 0; (p = text.find('|', p)) != std::string::npos; p += 2) text.replace(p, 1, "\\|");
      o += "| " + name + " | " + text + " |\n";
    }
  }
  return o;
}

}  // namespace mech3
