#pragma once
#include <cstdint>
#include <string>

// Ours (docs/editor.md, Teleport): the developers' teleport (mech3/teleport,
// command 0x95, unreachable in the release) on the editor's main menu, a
// TELEPORT row in one of its blank rows, so it is reached as the editor is
// (from MODS only). Its dialog (132) is a form (form.h), drawn by ImGui; OK
// moves the player and writes the spots back as the original does, into
// the mod as a loose zbd\<world>\location.zrd (text, its earlier one kept
// as location.zrd.bak).
namespace mech3 {

struct EditorForm;
struct TeleportDialog;

void teleport_open(int32_t y);  // the TELEPORT row
EditorForm& teleport_form();
TeleportDialog& teleport_dialog();
const std::string& teleport_last_saved();  // where OK last wrote, "" when it did not

}  // namespace mech3
