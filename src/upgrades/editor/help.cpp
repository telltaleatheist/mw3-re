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
      "Open the editor from the MODS screen: choose a campaign at the left, then EDITOR, a mission, and PLAY. The "
      "game starts again inside that campaign with the mission open in the editor. A mod's campaign played from "
      "MODS (PLAY) has it in every mission too: EDITOR in the pause menu, or Shift+F12. Nothing started from the "
      "main menu has the editor.",
      "The menu is on the left; point at a row and click (or press Space). The view is a free camera: fly it with "
      "the movement keys.",
      "EDIT AI NETS: pick a net. Its nodes and links are drawn in the world. Click a node to pick it; picking a "
      "second one links the first to it. Move Node, Delete Node, Node Params and Add veh to node work on the "
      "first picked node.",
      "EDIT VEHICLES: pick a vehicle, change its pages, then Apply (the running mission takes it at once) and Save "
      "(it is written for the next start).",
      "The editor only saves as. The first save (Save Changes in the net menu, the vehicle editor's Save, Add veh "
      "to node, New Weapon, Teleport's OK) asks for a name and makes a copy of the campaign under it, a mod of "
      "your own: mods\\<the name>. That save and every later one go into the copy, and each says where. The "
      "campaign you opened, the game's own files and a mod you imported are never changed. A copy of yours opened "
      "in the editor is saved into itself, nothing asked.",
      "EXIT GAME EDIT goes back to MODS, your copy chosen at the left when a save made one. There PLAY plays it, "
      "TEST plays one of its missions at once, EDITOR opens it again, REPORT says what it changes, EXPORT packs "
      "it into a zip to give away, and DELETE removes it with its pilots and saved games.",
      "To undo a save, the file it replaced in your copy is kept beside it as <file>.bak (the save before, one "
      "kept): put the .bak back in its place. To undo everything, DELETE the copy on MODS: the campaign it was "
      "made from is as it was.",
      "Every field, button and row has a tip: hold the mouse over it. The full guide is docs/editor-guide.md.",
  };
  return v;
}

const std::vector<std::string>& help_workflow() {
  static const std::vector<std::string> v = {
      "Choose what to start from. MODS lists the campaigns at the left: the game's own first, each campaign a mod "
      "brought, each copy you made. The mods ticked for it at the right are mounted with it. To start from someone "
      "else's work, IMPORT it (a zip, a folder, or a game folder a mod's installer was run over): the line says "
      "how many warnings its files gave, and REPORT lists them in three groups, the worst first (what stops part "
      "of the mod working, what works here but may crash the original game, slips that do no harm), and writes "
      "the same to mod-reports\\<the mod's folder>.txt in your settings folder. Missions, mech variants and "
      "animations made in other tools need no editor here: a mod holding them as the game's own files, or as "
      "mech3ax's JSON files beside them, loads as it is (docs/modding.md, Readable files read directly), and a "
      "mech loadout (.m3c) picked with IMPORT goes to the mech lab's Ctrl+I.",
      "Open a mission: with the campaign chosen, EDITOR lists its missions and PLAY opens the one picked. The game "
      "starts again inside the campaign, and the mission starts with the editor up and a stand-in pilot.",
      "Change the AI's paths: EDIT AI NETS, pick a net (net_01 is usually the main one). Pick a node and move it "
      "(Move Node, then press the mouse, drag, let go), link nodes (pick one, then the next), delete what is not "
      "wanted, set a node's speed or wait (Node Params). Save Changes.",
      "Name your copy: the first save asks for a name (offered: \"<campaign>, my edit\") and makes mods\\<the "
      "name>, a copy of the campaign that holds only the files you change (a mission's archive whole, "
      "zbd\\c1\\readerm1.zbd and the like) and names the campaign it was saved from (base= in its mod.txt). A "
      "name already taken is refused, never written over. Cancel saves nothing. Later saves go into the copy, "
      "nothing asked. A mod folder you made by hand is treated as any other: copied, never written.",
      "Add a vehicle: pick a node, Add veh to node, choose its type and side, OK. It is made at once and written "
      "into the mission's vehicle list (aiv.zrd) in your copy.",
      "Change a vehicle's AI: EDIT VEHICLES, pick it, set its pages (when it wakes, how far it follows, how it "
      "attacks and retreats, what valve it sets), Apply to try it in the running mission, Save to keep it.",
      "Add a weapon: EDIT WEAPONS, pick the weapon to copy, set its name and numbers, OK. It is written into your "
      "copy (mod.txt and zbd\\reader.zbd) and is part of that campaign; the mech lab offers it from the next "
      "start.",
      "Try it: EXIT GAME EDIT goes back to MODS with your copy chosen. TEST, a mission, PLAY plays that mission "
      "at once with a stand-in pilot; when it ends a list plays the others, and BACK leaves. PLAY plays the "
      "campaign as a player would: its pilot screen lists the copy's own pilots and your main-game pilots, and "
      "picking one of yours copies it in (the main game's pilot is never changed).",
      "Go on: choose the copy, EDITOR, a mission, PLAY. Its saves go into the copy itself.",
      "Give it away: EXPORT packs the copy into one zip that IMPORT takes. A copy of a mod's campaign needs that "
      "mod on the other machine too; there it is an imported mod, which that player's editor copies and never "
      "writes.",
      "Undo: every save keeps the file it replaced as <file>.bak; put it back to undo that save. DELETE on MODS "
      "removes a copy whole, with its pilots and saved games; RESET, on the game's own campaign, clears its ticks "
      "and its pilots there. Neither touches the game's files or your main-game pilots.",
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
  o += "Pirate's Moon, as MechWarrior 3, shipped with the developers' own editor still inside: a menu over the running mission for "
       "the AI's paths (nets) and the vehicles' AI. The rebuild opens it to players and modders, makes the parts "
       "the release left empty work, and saves what you change as a copy of the campaign, a mod of your own, never "
       "into the game's files or anyone else's mod. The menus are the original's; the "
       "dialogs are windows of the rebuild's own. Every field, button and row in the editor has a tip (hold the "
       "mouse over it); this guide gathers them. How it works inside: docs/editor.md.\n\n";
  o += "Pirate's Moon's editor is MechWarrior 3's, and so is the rebuild's: every menu, window and save in this "
       "guide is in both games. What is Pirate's Moon's own: the vehicle editor's chassis list and Add veh to node "
       "offer its 25 mechs. The original Pirate's Moon kept no edit of a net (a node moved, added or deleted was "
       "dropped); the rebuild keeps and saves them, as in MechWarrior 3. What it lacks: a node's via, terminus and "
       "static_mfb settings, new in Pirate's Moon, have no fields in Node Params (a save writes them as the file "
       "had them), and a vehicle's target settings (the target tag in aiv.zrd) have no page in Mech AI.\n\n";
  o += "## Quick start\n\n";
  int n = 0;
  for (const std::string& p : help_quick_start()) o += std::to_string(++n) + ". " + p + "\n";
  o += "\n## From a campaign to your own changed copy\n\n";
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
