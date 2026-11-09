#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "mech3/game_editor.h"

namespace mech3::ai {
class AiNet;
class CZAiNetEdit;
}  // namespace mech3::ai

// The game editor's AI net editor (docs/editor.md, The AI net editor; S5
// sitting 3a): EDIT AI NETS's dialog, the editor object and its menu with
// GOTO Net, Save Changes and Exit Net Editor. Original file unknown: these
// sit at 0x00402ba0..0x00403b00, the exe's first, unnamed module.
namespace mech3 {

// The AI Network Editor dialog (template 139, 0x004a4c20; FIX (A): ours, in
// the editor's panel, not an MFC dialog, and not modal: the mission runs on
// as it does under the editor's menus). Its controls as the template has
// them: the title, "Select a network to edit:", the drop-down list (sorted,
// "New Net" at the top and selected), OK and Cancel. The list is every net
// of znet's registry by name (0x004a4cd0); a row of it selects its net.
// Either button closes it with the selection (0x004a4d90): in the exe Cancel
// opens the selected net too (0x0040e120 takes any close), and so here.
struct NetDialog {
  std::vector<std::pair<std::string, ai::AiNet*>> items;  // "New Net" (none) first, then the nets
  int32_t selected = 0;   // +0x64
  int32_t first = 0;      // ours: the list's first row shown, when it is longer than the panel
  bool open = false;
};
NetDialog& net_dialog();

// The editor object (0x00402f40, 0x140 bytes, in [0x005f4c6c]).
struct NetEditor {
  NetEditor(ai::CZAiNetEdit* net, int32_t dirty);  // 0x00402f40
  ~NetEditor();                                    // 0x00402fd0: the far clip back, the net deleted
  NetEditor(const NetEditor&) = delete;
  NetEditor& operator=(const NetEditor&) = delete;
  ai::CZAiNetEdit* net = nullptr;  // +0x14, owned
  float far_clip = 0;              // +0x128  the camera's before the editor (0x004f3b00)
  int32_t dirty = 0;               // +0x138  cleared by Save
  int32_t rebuild = 0;             // +0x13c
  // The selection base 0x0058bf50 (its view of the nodes and links, the
  // selection box) is sitting 3b's.
};
NetEditor*& net_editor();  // [0x005f4c6c]
void net_editor_free();    // 0x00402ba0's first half; the program's end and the tests

const EditorMenu& editor_net_menu_rows();  // 0x005e0f18: Pick CMD or Point .. Exit Net Editor
const EditorMenu& editor_net_dialog_rows();  // the dialog's rows as last filled

void edit_ai_nets(int32_t y);      // 0x00402d60: the dialog
void net_dialog_close();           // its close (OK or Cancel): the net opened, or New Net
void net_editor_menu();            // 0x00403690: mode 1, the menu
void net_editor_goto(int32_t y);   // 0x004036c0
void net_editor_save(int32_t y);   // 0x00402e90 -> 0x00402f20 (the save ours: net_save.h)
void net_editor_exit(int32_t y);   // 0x00402ba0

// Ours: a line under the panel's rows (the save's folder, "no network to
// save"), said in the log too; the next menu clears it.
void editor_status(const std::string& text);

}  // namespace mech3
