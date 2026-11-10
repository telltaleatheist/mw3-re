#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "gamez/znetwork/znet_view.h"
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

// The AI Network Editor dialog (template 139, 0x004a4c20; FIX (A): ours, a
// form drawn by ImGui, not an MFC dialog, and not modal: the mission runs on
// as it does under the editor's menus). Its controls as the template has
// them: the title, "Select a network to edit:", the drop-down list (sorted,
// "New Net" at the top and selected), OK and Cancel. The list is every net
// of znet's registry by name (0x004a4cd0); a row of it selects its net.
// Either button closes it with the selection (0x004a4d90): in the exe Cancel
// opens the selected net too (0x0040e120 takes any close), and so here.
struct NetDialog {
  std::vector<std::pair<std::string, ai::AiNet*>> items;  // "New Net" (none) first, then the nets
  int32_t selected = 0;   // +0x64
  bool open = false;
};
NetDialog& net_dialog();

// The editor object (0x00402f40, 0x140 bytes, in [0x005f4c6c]).
struct NetEditor {
  NetEditor(ai::CZAiNetEdit* net, int32_t dirty);  // 0x00402f40
  ~NetEditor();                                    // 0x00402fd0: the far clip back, the net deleted
  NetEditor(const NetEditor&) = delete;
  NetEditor& operator=(const NetEditor&) = delete;
  void frame(float dt);            // 0x00403160
  const float* camera = nullptr;   // +0x00  the camera's place (its data +0x14)
  int32_t has_ground = 0;          // +0x04  the pick gave its point
  float ground[3] = {};            // +0x08  the view vehicle's pick
  ai::CZAiNetEdit* net = nullptr;  // +0x14, owned
  znet::View* view = nullptr;      // +0x18  the selection base (0x0058bf50), owned
  float far_clip = 0;              // +0x128  the camera's before the editor (0x004f3b00)
  int32_t move = 0;                // +0x130  Move Node: 1 off the node, 2 back on it, 3 following
  znet::Mark* moving = nullptr;    // +0x134
  int32_t dirty = 0;               // +0x138  cleared by Save
  int32_t rebuild = 0;             // +0x13c  the editor made anew before its next frame
  const void* armed = nullptr;     // ours: Delete Node's mark or link waiting for its answer
};
NetEditor*& net_editor();  // [0x005f4c6c]
void net_editor_free();    // 0x00402ba0's first half; the program's end and the tests

const EditorMenu& editor_net_menu_rows();  // 0x005e0f18: Pick CMD or Point .. Exit Net Editor
struct EditorForm;
EditorForm& net_dialog_form();  // the dialog as a form (form.h; the tests press its buttons)

void edit_ai_nets(int32_t y);      // 0x00402d60: the dialog
void net_dialog_close();           // its close (OK or Cancel): the net opened, or New Net
void net_editor_menu();            // 0x00403690: mode 1, the menu
void net_editor_goto(int32_t y);   // 0x004036c0
void net_editor_save(int32_t y);   // 0x00402e90 -> 0x00402f20 (the save ours: net_save.h)
void net_editor_exit(int32_t y);   // 0x00402ba0
void net_editor_move(int32_t y);   // 0x00403ac0 -> 0x00403a90
void net_editor_delete(int32_t y); // 0x00403ae0
NetEditor* net_editor_new(int32_t id, const float at[3]);  // 0x00402c90: New Net
znet::Mark* net_editor_first_pick();  // 0x00403770
// 0x00403140 -> 0x00403080: the editor made anew while it asks (the net and
// its dirty flag kept), then its frame, until a frame does not ask.
void net_editor_step(float dt);
void net_editor_step_slot(NetEditor*& slot, float dt);  // 0x00403080 on any editor's slot

// The Ai Network Editor window (template 181, 0x68 bytes, in [0x0060b31c];
// Change Net Params opens it): OK, Cancel and Open, which shows another
// net, read from a file, beside the edited one, its own editor at +0x64
// stepped each frame (0x00404290); no parameters at all. FIX (A): a form
// drawn by ImGui; Open's file dialog a list of the open archives' net files.
struct NetWindow {
  NetEditor* hosted = nullptr;  // +0x64
  bool choosing = false;        // ours: Open's list up
};
NetWindow*& net_window();
EditorForm& net_window_form();  // the window (form.h)
EditorForm& net_open_form();    // Open's list
EditorForm& net_delete_form();  // Delete Node's question (modal)
void net_params_open(int32_t y);      // 0x004036f0 -> 0x004bd330 -> 0x004bf670
void net_window_open(const char* name);  // 0x00404570 on the window's slot
void net_window_free();                  // 0x00404420
void net_editor_frame_all(bool editing); // 0x00404290
void net_window_show();                  // 0x004bf670 with no file (the vehicle editor's Net Edit too)
// The frame's list of editor slots ([0x005f4c60]; the main one is first).
void net_editor_slot_add(NetEditor** slot);     // 0x004043c0
void net_editor_slot_remove(NetEditor** slot);  // 0x00404420
void net_editor_slot_free(NetEditor*& slot);    // 0x00404490(0)
bool net_editor_showing(const ai::AiNet* net);  // 0x00404270
void net_editor_show(NetEditor*& slot, ai::AiNet* net);  // 0x004044c0

// Ours: a line under the panel's rows (the save's folder, "no network to
// save"), said in the log too; the next menu clears it.
void editor_status(const std::string& text);

}  // namespace mech3
