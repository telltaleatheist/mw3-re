#pragma once
#include <cstdint>
#include <string>

// Ours (Owen, 2026-11-09; docs/editor.md, Node Params): the net editor's
// Node Params row as it was meant to work. The release opens template 180
// with OK and Cancel only and never hands it the node (0x004c7840 ->
// 0x004c7820 -> 0x004bf620, an empty function); here the first pick's record
// is a form in the editor's panel: a press on a flag cycles it, a press on a
// value takes typed text (Enter keeps it, Esc drops it), OK puts the record
// on the node through its setters (so the edit reaches the save) and Cancel
// leaves it as it was.
namespace mech3 {

void node_params_open(int32_t y);  // the Node Params row (0x00403710's place)
bool node_params_open_now();       // the form is up
// For the tests: a typed character as zInput hands it (ch, its key code).
void node_params_char(int32_t ch, int32_t key);
// For the tests: the form's row `i` pressed.
void node_params_press(int32_t row);

}  // namespace mech3
