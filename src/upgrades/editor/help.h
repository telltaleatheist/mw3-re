#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "upgrades/editor/form.h"

// Ours (docs/editor-guide.md): the editor's help, one source for the
// tooltips on every form's fields and buttons and every menu row, the Help
// window and the guide (generated: build/mw3 --editor-guide). The tables sit
// beside the forms' models: help_menus.cpp, help_nets.cpp, help_vehicles.cpp,
// help_weapons.cpp.
namespace mech3 {

struct HelpTip {
  const char* item;  // the field's or button's label, or the row's text without its leading space
  const char* name;  // the guide's name for it ("" for the item itself; for the hidden labels "##...")
  const char* text;  // one or two plain lines: what it is, what changing it does
};

struct HelpForm {
  const char* form;     // the form's title (up to "##" or ':'), or "main menu" / "net menu"
  const char* heading;  // the guide's section
  const char* about;    // a paragraph for the guide
  std::vector<HelpTip> tips;
};

// Every table, in the guide's order.
const std::vector<HelpForm>& help_forms();
void help_menus(std::vector<HelpForm>& out);     // help_menus.cpp
void help_nets(std::vector<HelpForm>& out);      // help_nets.cpp
void help_vehicles(std::vector<HelpForm>& out);  // help_vehicles.cpp
void help_weapons(std::vector<HelpForm>& out);   // help_weapons.cpp

// An item's tip on a form ("Node Parameters: node_03" finds "Node
// Parameters"), null when the table has none.
const char* help_tip(const std::string& form_title, const std::string& item);
// The tip of the editor panel's row under the mouse, null when none.
const char* editor_row_tip();

// The quick start (the Help window's text and the guide's first section) and
// the guide's longer walk-through, from a fresh mod to a changed mission.
const std::vector<std::string>& help_quick_start();
const std::vector<std::string>& help_workflow();

void help_open(int32_t y);  // the HELP rows: the Help window
EditorForm& help_window();

std::string editor_guide_markdown();  // docs/editor-guide.md

}  // namespace mech3
