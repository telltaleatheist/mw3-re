#pragma once
#include <cstdint>
#include <string>

// Ours (Owen, 2026-11-09; docs/editor.md, Node Params): the net editor's
// Node Params row as it was meant to work. The release opens template 180
// with OK and Cancel only and never hands it the node (0x004c7840 ->
// 0x004c7820 -> 0x004bf620, an empty function); here the first pick's record
// is a form (form.h, drawn by ImGui): each value with whether it is given,
// the three-state flags, the name and gate; OK puts the record on the node
// through its setters (so the edit reaches the save), Cancel leaves it.
namespace mech3 {

struct EditorForm;

void node_params_open(int32_t y);  // the Node Params row (0x00403710's place)
bool node_params_open_now();       // the form is up
EditorForm& node_params_form();    // the form (the tests set its fields and press its buttons)

}  // namespace mech3
