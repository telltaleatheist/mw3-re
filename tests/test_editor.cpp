// The game editor's panel, menu, frame, entry and exit (mech3/game_editor,
// docs/editor.md S4) with its calls out stubbed: the order of the entry's
// and exit's calls, the main menu's rows, the hovered row lit and a press
// dispatched to its row.
#include <string>
#include <vector>

#include "check.h"
#include "gamez/znetwork/znet_object.h"
#include "mech3/game_editor.h"
#include "mech3/game_editor_net.h"
#include "mech3/zainet.h"
#include "mech3/zainetedit.h"

namespace {
struct Stub : mech3::EditorHooks {
  std::vector<std::string> calls;
  zin::MouseInfo m;
  uint32_t pick = 0;      // command 0x9b's state
  int32_t lines = 0;      // the debug lines on at the entry
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg>) override {
    calls.push_back(std::string("unported ") + key);
    return 0;
  }
  zin::MouseInfo mouse() override { return m; }
  uint32_t command(int32_t cmd) override { return cmd == 0x9b ? pick : 0; }
  void input_reset() override { calls.push_back("input_reset"); }
  void push_map() override { calls.push_back("push_map"); }
  void pop_map() override { calls.push_back("pop_map"); }
  void bind(int32_t cmd, uint32_t key, uint32_t mouse) override {
    calls.push_back("bind " + std::to_string(cmd) + " " + std::to_string(key) + " " + std::to_string(mouse));
  }
  float frame_dt() override { return 0.033f; }
  ztwod::Handle font(const char*) override { return 0; }
  void view_on_stealth() override { calls.push_back("view_on_stealth"); }
  void view_on_pilot() override { calls.push_back("view_on_pilot"); }
  int32_t debug_lines() override { return lines; }
  void debug_lines_show(bool on) override { calls.push_back(on ? "lines on" : "lines off"); }
  void net_step(float) override { calls.push_back("net_step"); }
  void net_frame(bool editing) override { calls.push_back(editing ? "net_frame 1" : "net_frame 0"); }
  void not_ported(const char* what) override { calls.push_back(std::string("not_ported ") + what); }
  void view_vehicle_place(const float p[3]) override {
    calls.push_back("place " + std::to_string(int(p[0])) + " " + std::to_string(int(p[1])) + " " + std::to_string(int(p[2])));
  }
};
// A row's text as the panel shows it.
std::string row_text(int32_t i) { return mech3::editor_panel().rows[i]->text; }

std::string joined(const std::vector<std::string>& v) {
  std::string s;
  for (const std::string& c : v) s += (s.empty() ? "" : ", ") + c;
  return s;
}

// Every row 100 pixels wide and 12 high, so the hit test needs no font.
void sized_rows() {
  for (ztwod::FontText* r : mech3::editor_panel().rows) r->needs_render = 0, r->width = 100, r->height = 12, r->leading = 0;
}
void at_row(Stub& h, int32_t row) { h.m.x = 10, h.m.y = 0x5f + 12 * row + 3; }
void press(Stub& h, int32_t row) {
  sized_rows();
  h.pick = 4;
  at_row(h, row);
  mech3::editor_frame();
}
}  // namespace

TEST(game_editor_entry_and_exit_in_the_originals_order) {
  Stub h;
  h.lines = 1;
  mech3::editor_reset();
  mech3::set_editor_hooks(&h);
  mech3::editor_key(mech3::kEditorCommand);
  const mech3::EditorState& st = mech3::editor_state();
  const mech3::EditorPanel& p = mech3::editor_panel();
  EXPECT(st.active == 1 && st.mode == 0 && st.menu == &mech3::editor_main_menu_rows() && p.layer.active);
  EXPECT(joined(h.calls) == "push_map, bind 155 57 1, bind 156 1081 2, view_on_stealth, lines off");
  const char* want[8] = {"PICK A COMMAND", " EDIT AI NETS", " EDIT VEHICLES", " EDIT WEAPONS", "", "", "", " EXIT GAME EDIT"};
  for (int i = 0; i < 25; ++i) {
    const std::string text = p.rows[i]->text;
    EXPECT(text == (i < 8 ? want[i] : ""));
    EXPECT(((p.rows[i]->flags & ztwod::kHidden) == 0) == (i < 8));
    EXPECT(p.rows[i]->x == 5 && p.rows[i]->y == 0x5f + 12 * i);
  }
  EXPECT(p.hovered == -1 && p.bottom == 0x5f + 8 * 12);
  h.calls.clear();
  mech3::editor_key(mech3::kEditorCommand);  // the key again: the exit
  EXPECT(st.active == 0 && !p.layer.active);
  EXPECT(joined(h.calls) == "pop_map, lines on, view_on_pilot");
  h.calls.clear();
  h.lines = 0;  // the release's: no debug lines to hide or bring back
  mech3::editor_enter();
  mech3::editor_exit();
  EXPECT(joined(h.calls) == "push_map, bind 155 57 1, bind 156 1081 2, view_on_stealth, pop_map, view_on_pilot");
  mech3::editor_key(0x61);  // another command: nothing
  EXPECT(st.active == 0);
  mech3::set_editor_hooks(nullptr);
  mech3::editor_reset();
}

TEST(game_editor_frame_lights_the_row_and_calls_it) {
  Stub h;
  mech3::editor_reset();
  mech3::set_editor_hooks(&h);
  mech3::editor_frame();  // down: only the net editor's frame
  EXPECT(joined(h.calls) == "net_frame 0");
  mech3::editor_enter();
  sized_rows();
  mech3::EditorPanel& p = mech3::editor_panel();
  const uint32_t top = p.color_top;
  // Over EDIT VEHICLES: lit green; then over the title: the old row back in
  // the panel's colours, the title never lit.
  at_row(h, 2);
  h.calls.clear();
  mech3::editor_frame();
  EXPECT(p.hovered == 2 && p.rows[2]->color_top == 0xff00 && p.rows[2]->color_bottom == 0xff00);
  EXPECT(joined(h.calls) == "net_frame 1");
  at_row(h, 0);
  mech3::editor_frame();
  EXPECT(p.hovered == 0 && p.rows[2]->color_top == top && p.rows[0]->color_top == top);
  // Off the panel (above it, or past the rows' width): none.
  h.m.x = 10, h.m.y = 0x5f - 20;
  mech3::editor_frame();
  EXPECT(p.hovered == -1);
  at_row(h, 1);
  h.m.x = 150;
  mech3::editor_frame();
  EXPECT(p.hovered == -1);
  // A press: on EDIT AI NETS its dialog, the input cleared first; on the
  // title or a blank row nothing is called; held (not pressed), nothing.
  h.pick = 4;
  at_row(h, 1);
  h.calls.clear();
  mech3::editor_frame();
  EXPECT(joined(h.calls) == "input_reset, net_frame 1");
  EXPECT(mech3::net_dialog().open && mech3::editor_state().menu == &mech3::editor_net_dialog_rows());
  mech3::editor_main_menu();
  sized_rows();
  at_row(h, 0);
  h.calls.clear();
  mech3::editor_frame();
  EXPECT(joined(h.calls) == "net_frame 1");
  at_row(h, 5);
  h.calls.clear();
  mech3::editor_frame();
  EXPECT(joined(h.calls) == "input_reset, net_frame 1");
  h.pick = 3;
  at_row(h, 1);
  h.calls.clear();
  mech3::editor_frame();
  EXPECT(joined(h.calls) == "net_frame 1");
  // EXIT GAME EDIT: the exit, the frame's net call then with the editor down.
  h.pick = 4;
  at_row(h, 7);
  h.calls.clear();
  mech3::editor_frame();
  EXPECT(mech3::editor_state().active == 0);
  EXPECT(joined(h.calls) == "input_reset, pop_map, view_on_pilot, net_frame 0");
  mech3::set_editor_hooks(nullptr);
  mech3::editor_reset();
}

// EDIT AI NETS (S5 sitting 3a): the AI Network Editor dialog lists znet's
// nets sorted by name under "New Net" (selected); a list row selects; OK and
// Cancel both open the selection (the exe's); the net menu's 13 rows
// (0x005e0f18); GOTO Net puts the view's vehicle at the first node's place
// (the placeholder's, 0 0 0, for a net with no file here); Save Changes says
// where it saved, or why not; Exit Net Editor frees it, back to the main menu;
// New Net is sitting 4's.
TEST(game_editor_net_dialog_and_menu) {
  Stub h;
  mech3::editor_reset();
  mech3::set_editor_hooks(&h);
  auto* a = new mech3::ai::AiNet(12), *b = new mech3::ai::AiNet(1), *c = new mech3::ai::AiNet(3);  // net_12, net_01, net_03
  mech3::editor_enter();
  press(h, 1);  // EDIT AI NETS
  const char* want[8] = {"AI Network Editor", " Select a network to edit:", " > New Net", "   net_01", "   net_03", "   net_12", " OK", " Cancel"};
  EXPECT(mech3::editor_net_dialog_rows().count == 8);
  for (int i = 0; i < 8; ++i) EXPECT(row_text(i) == want[i]);
  press(h, 4);  // net_03
  EXPECT(mech3::net_dialog().selected == 2 && row_text(4) == " > net_03" && row_text(2) == "   New Net");
  h.calls.clear();
  press(h, 7);  // Cancel: opens it all the same
  EXPECT(!mech3::net_dialog().open && mech3::net_editor() && mech3::net_editor()->net);
  if (!mech3::net_editor() || !mech3::net_editor()->net) {  // no editor: the rest cannot run
    mech3::editor_exit();
    delete a, delete b, delete c;
    mech3::editor_reset();
    mech3::set_editor_hooks(nullptr);
    return;
  }
  EXPECT(mech3::net_editor()->net->header.net == 3);  // the edit takes its own id (400..499), as the exe's
  EXPECT(mech3::editor_state().mode == 1 && mech3::editor_state().menu == &mech3::editor_net_menu_rows());
  const char* rows[13] = {"Pick CMD or Point", " GOTO Net", " Change Net Params", " Node Params", " Add veh to node",
                          " Move Node", " Delete Node", " Delete Net", " Save Changes", " Tree View", "", "", " Exit Net Editor"};
  for (int i = 0; i < 13; ++i) EXPECT(row_text(i) == rows[i]);
  h.calls.clear();
  press(h, 1);  // GOTO Net
  EXPECT(joined(h.calls).find("place 0 0 0") != std::string::npos);
  press(h, 8);  // Save Changes: its file is in no archive here
  EXPECT(row_text(24).find("not saved") == 0 && mech3::net_editor()->dirty == 0);
  h.calls.clear();
  press(h, 3);  // Node Params: sitting 4's
  EXPECT(joined(h.calls).find("not_ported Node Params") != std::string::npos);
  press(h, 12);  // Exit Net Editor
  EXPECT(!mech3::net_editor() && mech3::editor_state().mode == 0 && row_text(1) == " EDIT AI NETS");
  press(h, 8);  // the main menu's blank row: nothing
  mech3::net_editor_save(0);  // with no editor
  EXPECT(row_text(24) == "no network to save");
  // New Net: sitting 4's, back to the main menu.
  press(h, 1);
  h.calls.clear();
  press(h, 6);  // OK on New Net
  EXPECT(!mech3::net_editor() && joined(h.calls).find("not_ported New Net") != std::string::npos);
  EXPECT(row_text(1) == " EDIT AI NETS");
  mech3::editor_exit();
  delete a, delete b, delete c;
  mech3::editor_reset();
  mech3::set_editor_hooks(nullptr);
}

