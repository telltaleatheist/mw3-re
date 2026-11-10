// The game editor's panel, menu, frame, entry and exit (mech3/game_editor,
// docs/editor.md S4) with its calls out stubbed: the order of the entry's
// and exit's calls, the main menu's rows, the hovered row lit and a press
// dispatched to its row.
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/znetwork/znet_object.h"
#include "gamez/znetwork/znet_view.h"
#include "mech3/game_editor.h"
#include "mech3/game_editor_net.h"
#include "mech3/zainet.h"
#include "mech3/zainetedit.h"
#include "mech3/game_editor_vehicles.h"
#include "mech3/mech_config.h"
#include "mech3/zai_valve.h"
#include "upgrades/editor/aiv_save.h"
#include "upgrades/editor/form.h"
#include "upgrades/editor/help.h"
#include "upgrades/editor/node_params.h"
#include "upgrades/editor/add_vehicle.h"

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
  // The net editor's frame.
  float near_z = 1.0f, far_z = 500.0f, pitch = 0.0f, cam[3] = {}, ground[3] = {};
  bool has_ground = false;
  std::vector<float> clips;  // each set_camera_clip's far
  void camera_clip(float* n, float* f) override { *n = near_z, *f = far_z; }
  void set_camera_clip(float n, float f) override { near_z = n, far_z = f, clips.push_back(f); }
  const float* camera_pos() override { return cam; }
  float camera_pitch() override { return pitch; }
  bool ground_pick(float out[3]) override {
    for (int i = 0; i < 3; ++i) out[i] = ground[i];
    return has_ground;
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
  const char* want[8] = {"PICK A COMMAND", " EDIT AI NETS", " EDIT VEHICLES", " EDIT WEAPONS", " HELP", "", "", " EXIT GAME EDIT"};  // HELP: ours
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
  EXPECT(mech3::net_dialog().open && mech3::form_shown(&mech3::net_dialog_form()));
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
  mech3::EditorForm& dialog = mech3::net_dialog_form();
  const std::vector<std::string> want = {"New Net", "net_01", "net_03", "net_12"};
  EXPECT(mech3::form_shown(&dialog) && dialog.title == "AI Network Editor" && dialog.fields.size() == 2);
  EXPECT(dialog.fields[0].label == "Select a network to edit:" && dialog.fields[1].choices == want);
  EXPECT(mech3::net_dialog().selected == 0 && dialog.buttons.size() == 2);
  *dialog.fields[1].i = 2;  // net_03
  EXPECT(mech3::net_dialog().selected == 2);
  h.calls.clear();
  EXPECT(dialog.press("Cancel"));  // Cancel: opens it all the same
  EXPECT(!mech3::form_shown(&dialog));
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
                          " Move Node", " Delete Node", " Delete Net", " Save Changes", " Tree View", " Help", "", " Exit Net Editor"};  // Help: ours
  for (int i = 0; i < 13; ++i) EXPECT(row_text(i) == rows[i]);
  h.calls.clear();
  press(h, 1);  // GOTO Net
  EXPECT(joined(h.calls).find("place 0 0 0") != std::string::npos);
  press(h, 8);  // Save Changes: its file is in no archive here
  EXPECT(row_text(24).find("not saved") == 0 && mech3::net_editor()->dirty == 0);
  press(h, 3);  // Node Params (ours): nothing picked
  EXPECT(row_text(24) == "Select a node first");
  press(h, 12);  // Exit Net Editor
  EXPECT(!mech3::net_editor() && mech3::editor_state().mode == 0 && row_text(1) == " EDIT AI NETS");
  press(h, 8);  // the main menu's blank row: nothing
  mech3::net_editor_save(0);  // with no editor
  EXPECT(row_text(24) == "no network to save");
  // New Net without a pick: back to the main menu.
  press(h, 1);
  mech3::net_dialog_form().press("OK");  // OK on New Net
  EXPECT(!mech3::net_editor() && row_text(1) == " EDIT AI NETS");
  // With one: a new net, the first free number from 1 (net_02), a node at
  // the pick, picked.
  h.has_ground = true, h.ground[0] = 5.0f, h.ground[2] = -7.0f;
  press(h, 1);
  mech3::net_dialog_form().press("OK");
  mech3::NetEditor* fresh = mech3::net_editor();
  EXPECT(fresh && fresh->net && fresh->net->header.net == 2 && fresh->net->count() == 1);
  EXPECT(fresh && fresh->net->node(0)->pos()[0] == 5.0f && fresh->view->picks.size() == 1);
  EXPECT(mech3::editor_state().menu == &mech3::editor_net_menu_rows());
  mech3::net_editor_free();
  mech3::editor_exit();
  delete a, delete b, delete c;
  mech3::editor_reset();
  mech3::set_editor_hooks(nullptr);
}

namespace {
// A numbered object's elements for the view alone: each one's links by number.
struct ViewElem : znet::Element {
  std::vector<int32_t> to;
  float pos[3] = {};
  bool placed = true;
};
struct ViewStub : znet::ViewHooks {
  znet::Object* net = nullptr;
  uint32_t shift = 0;
  ViewElem* at(znet::Element* e) { return static_cast<ViewElem*>(e); }
  int32_t link_count(znet::Element* e) override { return int32_t(at(e)->to.size()); }
  znet::Element* link(znet::Element* e, int32_t k) override {
    const int32_t j = at(e)->to[size_t(k)];
    return j >= 0 && j < net->count() ? net->elem(j) : nullptr;
  }
  int32_t link_index(znet::Element* e, znet::Element* t) override {
    if (!e) return -1;
    const int32_t j = net->index_of(t);
    for (size_t k = 0; k < at(e)->to.size(); ++k)
      if (j >= 0 && at(e)->to[k] == j) return int32_t(k);
    return -1;
  }
  bool placed(znet::Element* e) override { return at(e)->placed; }
  const float* pos(znet::Element* e) override { return at(e)->pos; }
  bool sees(znet::Element*, znet::Element*) override { return true; }
  uint32_t key_state(int32_t key) override { return key == 0x2a ? shift : 0; }
};
bool in_window(znet::View& v, ztwod::Object2D* o) {
  ztwod::Object2D* prev = nullptr;
  return v.window.find(o, &prev);
}
}  // namespace

// The selection base (0x0058bf50): a link per pair (both ways made once, by
// the lower end; one way from either end), a circle in the window only for a
// placed element; a press picks (lit, the cursor's line to it), a release
// drops the pick unless shift is held (then the circle stays pressed), a
// plain left-up on the cursor hands the picks over to be linked.
TEST(net_view_links_and_picks) {
  znet::Object net(777);
  ViewElem e[5];
  e[0].to = {1, 9};     // to 1 one way; 9 is no element
  e[1].to = {2};        // 1 <-> 2
  e[2].to = {1};
  e[3].to = {0};        // 3 -> 0, from the higher end
  e[3].placed = false;
  e[4].to = {4};        // to itself: it links back, so not made
  for (ViewElem& x : e) net.elems.push_back(&x);
  ViewStub h;
  h.net = &net;
  {
    znet::View v(&net, &h);
    EXPECT(v.marks.size() == 5 && v.links.size() == 3);
    if (v.links.size() == 3) {
      const auto ends = [&](const znet::Link& l) { return std::make_pair(int(l.from - &v.marks[0]), int(l.to - &v.marks[0])); };
      EXPECT(ends(v.links[0]) == std::make_pair(0, 1) && v.links[0].kind == 1);
      EXPECT(ends(v.links[1]) == std::make_pair(1, 2) && v.links[1].kind == 3);
      EXPECT(ends(v.links[2]) == std::make_pair(3, 0) && v.links[2].kind == 1);
      EXPECT(in_window(v, &v.links[1].back) && !in_window(v, &v.links[0].back) && in_window(v, &v.links[0].ahead));
    }
    EXPECT(in_window(v, &v.marks[0].obj) && !in_window(v, &v.marks[3].obj) && in_window(v, &v.cursor.obj));
    znet::Mark& a = v.marks[0];
    znet::Mark& b = v.marks[1];
    a.obj.on_enter();
    EXPECT(a.flags == 1 && v.hovered.size() == 1 && a.obj.color == znet::view_colors().pressed);
    a.obj.on_press();
    EXPECT(v.picks.size() == 1 && v.picks.front() == &a && v.cursor_lines.size() == 1 && a.obj.color == znet::view_colors().lit);
    a.obj.on_release();  // no shift: dropped
    EXPECT(v.picks.empty() && v.cursor_lines.empty() && a.obj.color == znet::view_colors().normal);
    h.shift = 1;
    a.obj.on_press(), b.obj.on_press();
    a.obj.on_release();  // shift: kept, and pressed again
    EXPECT(v.picks.size() == 2 && (a.obj.mouse & ztwod::kPressed));
    v.cursor.obj.on_left_up();  // shift held: nothing handed over
    EXPECT(v.to_link.empty());
    h.shift = 0;
    v.cursor.obj.on_left_up();
    EXPECT(v.to_link.size() == 2 && v.to_link.front() == &a);
    a.obj.on_leave();
    EXPECT(a.flags == 0 && v.hovered.empty() && a.obj.color == znet::view_colors().lit);  // still pressed: lit
    // Shift and a click on a circle other than the last pressed drops its pick.
    h.shift = 1;
    v.last = &b.obj;
    a.obj.on_click();
    EXPECT(v.picks.size() == 1 && v.picks.front() == &b && v.last == &a.obj && (a.flags & 2));
  }
  net.elems.clear();
}

// The net editor's frame (0x00403160) on a net of two nodes: looking down
// 70 degrees or more the far clip is 1.5 times the camera's x, y and z
// distance to the pick, otherwise the kept one again; Move Node waits for the
// mouse to leave the node and come back, then puts it at the pick each frame
// until it is clicked; linking the picks to the first one makes the editor
// anew, the net and its dirty flag kept.
TEST(net_editor_frame_far_clip_move_and_link) {
  Stub h;
  mech3::editor_reset();
  mech3::set_editor_hooks(&h);
  mech3::editor_enter();
  auto* edit = new mech3::ai::CZAiNetEdit(5, nullptr, 0);
  mech3::ai::NetNodeRec r0, r1;
  r0.pos[0] = 1, r1.pos[0] = 2;
  edit->elems = {new mech3::ai::NetNode(edit, r0), new mech3::ai::NetNode(edit, r1)};
  h.far_z = 500.0f;
  mech3::net_editor() = new mech3::NetEditor(edit, 0);
  mech3::NetEditor* e = mech3::net_editor();
  EXPECT(e->far_clip == 500.0f && e->view && e->view->marks.size() == 2);
  // The far clip.
  h.pitch = -1.3f, h.cam[1] = 100.0f, h.has_ground = true, h.ground[0] = 10.0f, h.ground[2] = -20.0f;
  mech3::net_editor_step(0.1f);
  EXPECT(!h.clips.empty() && h.clips.back() == 195.0f && e->has_ground == 1);
  h.pitch = -0.5f;
  mech3::net_editor_step(0.1f);
  EXPECT(h.clips.back() == 500.0f);
  // Move Node.
  mech3::net_editor_move(0);
  EXPECT(row_text(24) == "Select a node first" && e->move == 0);
  znet::Mark& m = e->view->marks[0];
  e->view->pick(&m);
  mech3::net_editor_move(0);
  EXPECT(e->move == 1 && e->moving == &m);
  mech3::net_editor_step(0.1f);  // the mouse is off it
  EXPECT(e->move == 2);
  m.flags = 1 | 4;
  mech3::net_editor_step(0.1f);  // back on it: the clicks cleared
  EXPECT(e->move == 3 && m.flags == 1 && edit->node(0)->pos()[0] == 1.0f);
  mech3::net_editor_step(0.1f);
  EXPECT(edit->node(0)->pos()[0] == 10.0f && edit->node(0)->pos()[2] == -20.0f && e->dirty == 1);
  m.flags |= 2;  // clicked
  mech3::net_editor_step(0.1f);
  EXPECT(e->move == 0);
  // Linking: node 1 to node 0, then the editor made anew.
  e->view->pick(&e->view->marks[1]);
  e->view->on_left_up();
  EXPECT(e->view->to_link.size() == 2);
  mech3::net_editor_step(0.1f);
  mech3::NetEditor* again = mech3::net_editor();
  EXPECT(edit->node(1)->link_count() == 1 && edit->node(1)->rec.links[0] == 0 && edit->node(0)->link_count() == 0);
  EXPECT(again && again->net == edit && again->dirty == 1 && again->rebuild == 0 && again->view->links.size() == 1);
  EXPECT(again && again->view->picks.empty());
  // One there already adds none; a new one goes after the last.
  EXPECT(edit->node(1)->add_link(edit->node(0)) && edit->node(1)->link_count() == 1);
  EXPECT(edit->node(1)->add_link(edit->node(1)) && edit->node(1)->link_count() == 2 && edit->node(1)->rec.links[1] == 1);
  mech3::net_editor_free();
  mech3::editor_exit();
  mech3::editor_reset();
  mech3::set_editor_hooks(nullptr);
}

// Delete Node (0x00403ae0) on a net of three nodes 0 <-> 1 -> 2: the first
// press asks, the second deletes node 1 with every link to and from it, its
// slot back to the placeholder and the editor made anew; a picked line is
// taken off the same way; a new node (0x004bd1f0) takes the first free slot.
TEST(net_editor_delete_node_line_and_new_node) {
  Stub h;
  mech3::editor_reset();
  mech3::set_editor_hooks(&h);
  mech3::editor_enter();
  auto* edit = new mech3::ai::CZAiNetEdit(6, nullptr, 0);
  mech3::ai::NetNodeRec r[3];
  for (int i = 0; i < 3; ++i) r[i].index = i, r[i].pos[0] = float(i);
  r[0].links = {1}, r[1].links = {0, 2};
  edit->elems = {new mech3::ai::NetNode(edit, r[0]), new mech3::ai::NetNode(edit, r[1]), new mech3::ai::NetNode(edit, r[2])};
  mech3::net_editor() = new mech3::NetEditor(edit, 0);
  mech3::NetEditor* e = mech3::net_editor();
  EXPECT(!edit->node(0)->placeholder && edit->placeholder->placeholder);
  mech3::net_editor_delete(0);  // nothing picked, no line: nothing
  EXPECT(e->armed == nullptr && edit->node(1)->link_count() == 2);
  e->view->pick(&e->view->marks[1]);
  mech3::net_editor_delete(0);
  mech3::EditorForm& ask = mech3::net_delete_form();
  EXPECT(e->armed == &e->view->marks[1] && mech3::form_shown(&ask) && ask.modal && ask.fields[0].label == "Confirm node deletion");
  EXPECT(edit->node(1)->link_count() == 2);
  EXPECT(ask.press("Cancel"));  // nothing deleted
  EXPECT(!mech3::form_shown(&ask) && e->armed == nullptr && edit->node(1)->link_count() == 2);
  mech3::net_editor_delete(0);
  EXPECT(ask.press("OK"));
  EXPECT(!mech3::form_shown(&ask) && e->rebuild == 1 && e->dirty == 1 && edit->node(1) == edit->placeholder);
  EXPECT(edit->node(0)->link_count() == 0 && edit->node(2)->link_count() == 0);
  mech3::net_editor_step(0.1f);  // made anew
  e = mech3::net_editor();
  EXPECT(e && e->net == edit && e->view->links.empty() && e->armed == nullptr);
  // A new node goes into the free slot, numbered by it.
  mech3::ai::NetNode* n = edit->add_node(nullptr);
  EXPECT(n && edit->node(1) == n && n->rec.index == 1 && !n->placeholder);
  EXPECT(n->add_link(edit->node(2)) && n->link_count() == 1);
  // A picked line: its arrow pressed, then Delete Node twice.
  e->rebuild = 1;
  mech3::net_editor_step(0.1f);
  e = mech3::net_editor();
  EXPECT(e->view->links.size() == 1);
  if (e->view->links.size() == 1) {
    znet::Link& l = e->view->links[0];
    l.ahead.on_press();
    EXPECT(e->view->picked_links.size() == 1);
    mech3::net_editor_delete(0);
    EXPECT(mech3::net_delete_form().fields[0].label == "Confirm line deletion");
    mech3::net_delete_form().press("OK");
    EXPECT(n->link_count() == 0 && e->rebuild == 1);
  }
  // The net's only node is never deleted.
  mech3::ai::CZAiNetEdit one(7, nullptr, 0);
  mech3::ai::NetNode* only = one.add_node(nullptr);
  EXPECT(only && !one.delete_node(only) && one.node(0) == only);
  mech3::net_editor_free();
  mech3::editor_exit();
  mech3::editor_reset();
  mech3::set_editor_hooks(nullptr);
}

// Change Net Params (0x004036f0 -> 0x004bf670): the Ai Network Editor
// window, OK, Cancel and Open, no parameters; Open's list (the open
// archives' net files: none here) and a file opened (0x00404570) as a second
// editor that the frame steps (0x00404290) apart from the main one while the
// menus are up; OK frees it and gives the net menu back.
TEST(net_window_open_and_hosted_editor) {
  Stub h;
  mech3::editor_reset();
  mech3::set_editor_hooks(&h);
  mech3::editor_enter();
  auto* edit = new mech3::ai::CZAiNetEdit(8, nullptr, 0);
  mech3::net_editor() = new mech3::NetEditor(edit, 0);
  mech3::net_params_open(0);
  EXPECT(mech3::net_window() && !mech3::net_window()->hosted);
  mech3::EditorForm& w = mech3::net_window_form();
  EXPECT(mech3::form_shown(&w) && w.title == "Ai Network Editor" && w.buttons.size() == 3 && w.buttons[2].label == "Open");
  EXPECT(w.press("Open"));
  mech3::EditorForm& open = mech3::net_open_form();
  EXPECT(mech3::net_window()->choosing && mech3::form_shown(&open) && open.fields[0].choices.empty());
  EXPECT(open.press("Cancel"));
  EXPECT(!mech3::net_window()->choosing && !mech3::form_shown(&open) && mech3::form_shown(&w));
  mech3::net_window_open("net_33.zrd");
  mech3::NetEditor* hosted = mech3::net_window()->hosted;
  EXPECT(hosted && hosted->net && hosted->net != edit);
  h.has_ground = true;
  mech3::net_editor_frame_all(true);  // the window's editor stepped, not the main one
  EXPECT(hosted->has_ground == 1 && mech3::net_editor()->has_ground == 0);
  EXPECT(w.press("OK"));  // closed, its editor freed
  EXPECT(!mech3::net_window() && !mech3::form_shown(&w));
  mech3::net_editor_frame_all(false);  // the menus down: the main one stepped
  EXPECT(mech3::net_editor()->has_ground == 1);
  mech3::net_editor_free();
  mech3::editor_exit();
  mech3::editor_reset();
  mech3::set_editor_hooks(nullptr);
}

// Node Params as meant (ours; the release's 180 is empty): the first pick's
// record as a form (form.h): values with whether they are given, the
// three-state flags, a name; OK puts it on the node and makes the editor anew,
// Cancel leaves it.
TEST(node_params_form_edits_the_picked_node) {
  Stub h;
  mech3::editor_reset();
  mech3::set_editor_hooks(&h);
  mech3::editor_enter();
  auto* edit = new mech3::ai::CZAiNetEdit(9, nullptr, 0);
  mech3::ai::NetNodeRec r0;
  r0.index = 0, r0.pos[0] = 4.0f;
  edit->elems = {new mech3::ai::NetNode(edit, r0)};
  mech3::net_editor() = new mech3::NetEditor(edit, 0);
  mech3::NetEditor* e = mech3::net_editor();
  mech3::node_params_open(0);
  EXPECT(!mech3::node_params_open_now() && row_text(24) == "Select a node first");
  e->view->pick(&e->view->marks[0]);
  mech3::node_params_open(0);
  mech3::EditorForm& f = mech3::node_params_form();
  EXPECT(mech3::node_params_open_now() && mech3::form_shown(&f) && f.title == "Node Parameters: node_00");
  EXPECT(*f.field("x")->f == 4.0f && *f.field("speed")->on == 0);
  *f.field("speed")->on = 1, *f.field("speed")->f = 0.5f;
  *f.field("x")->f = 6.0f;
  *f.field("hidden")->set = 1, *f.field("hidden")->flag = 1;
  *f.field("name")->text = "Gate A";
  EXPECT(f.press("OK"));
  mech3::ai::NetNode* n = edit->node(0);
  EXPECT(!mech3::node_params_open_now() && !mech3::form_shown(&f));
  EXPECT(n->rec.speed_on == 1 && n->rec.speed == 0.5f && n->rec.hidden_set == 1 && n->rec.hidden == 1);
  EXPECT(n->rec.name && std::string(n->rec.name) == "Gate A" && n->rec.pos[0] == 6.0f);
  EXPECT(e->dirty == 1 && e->rebuild == 1 && row_text(24) == "node_0 set; Save Changes keeps it");
  // Cancel leaves it; an empty name clears it on OK.
  mech3::net_editor_step(0.1f);
  e = mech3::net_editor();
  e->view->pick(&e->view->marks[0]);
  mech3::node_params_open(0);
  *f.field("speed")->f = 2.0f;
  EXPECT(f.press("Cancel"));
  EXPECT(!mech3::node_params_open_now() && edit->node(0)->rec.speed == 0.5f);
  e->view->pick(&e->view->marks[0]);
  mech3::node_params_open(0);
  EXPECT(*f.field("name")->text == "Gate A");
  f.field("name")->text->clear();
  f.press("OK");
  EXPECT(edit->node(0)->rec.name == nullptr);
  mech3::net_editor_free();
  mech3::editor_exit();
  mech3::editor_reset();
  mech3::set_editor_hooks(nullptr);
}

// Add veh to node (ours; the release's OK makes nothing): the form's fields
// from the first pick (the node's place, yaw 0, Friend), the side cycling
// the team (1, 2, 0), a typed yaw; and the record helpers on a small aiv.zrd
// of our own: a type's tags copied, the next free name, the record added
// after the last.
TEST(add_vehicle_form_and_record) {
  Stub h;
  mech3::editor_reset();
  mech3::set_editor_hooks(&h);
  mech3::editor_enter();
  auto* edit = new mech3::ai::CZAiNetEdit(10, nullptr, 0);
  mech3::ai::NetNodeRec r0;
  r0.index = 0, r0.pos[0] = 7.0f;
  edit->elems = {new mech3::ai::NetNode(edit, r0)};
  mech3::net_editor() = new mech3::NetEditor(edit, 0);
  mech3::NetEditor* e = mech3::net_editor();
  mech3::add_vehicle_open(0);
  EXPECT(!mech3::add_vehicle_open_now() && row_text(24) == "Select a node first");
  e->view->pick(&e->view->marks[0]);
  mech3::add_vehicle_open(0);
  mech3::EditorForm& f = mech3::add_vehicle_form();
  EXPECT(mech3::add_vehicle_open_now() && mech3::form_shown(&f) && f.title == "Node Edit - Vehicle");
  EXPECT(*f.field("x")->f == 7.0f && *f.field("yaw")->f == 0.0f && *f.field("team id")->i == 1 && *f.field("side")->i == 0);
  EXPECT(mech3::add_vehicle_types().size() >= 19 && f.field("vehicle type")->choices == mech3::add_vehicle_types());
  // The side's radio buttons: the team follows.
  *f.field("side")->i = 1, f.changed(*f.field("side"));
  EXPECT(*f.field("team id")->i == 2);
  *f.field("side")->i = 2, f.changed(*f.field("side"));
  EXPECT(*f.field("team id")->i == 0);
  EXPECT(f.press("Cancel"));
  EXPECT(!mech3::add_vehicle_open_now() && !mech3::form_shown(&f));
  // The helpers: an aiv.zrd of (("apc_01" (3 (1 2 3) 0.5 2 "params" (9))))
  std::vector<uint8_t> b;
  const auto w = [&](uint32_t v) { b.insert(b.end(), reinterpret_cast<uint8_t*>(&v), reinterpret_cast<uint8_t*>(&v) + 4); };
  const auto str = [&](const char* t) { w(3), w(uint32_t(std::strlen(t))), b.insert(b.end(), t, t + std::strlen(t)); };
  const auto flt = [&](float f) { uint32_t u; std::memcpy(&u, &f, 4); w(2), w(u); };
  w(4), w(2), w(4), w(3);
  str("apc_01");
  w(4), w(7), w(1), w(3), w(4), w(4), flt(1), flt(2), flt(3), flt(0.5f), w(1), w(2), str("params"), w(4), w(2), w(1), w(9);
  int32_t items = 0;
  const std::vector<uint8_t> tags = mech3::aiv_tags_of_type(b.data(), b.size(), "apc", &items);
  EXPECT(items == 2 && !tags.empty());
  EXPECT(mech3::aiv_free_name(b.data(), b.size(), "apc") == "apc_02" && mech3::aiv_free_name(b.data(), b.size(), "harasser") == "harasser_01");
  const float pos[3] = {4, 5, 6};
  const std::vector<uint8_t> rec = mech3::aiv_record(10, pos, -1.0f, 1, tags, items);
  const std::vector<uint8_t> out = mech3::aiv_with_record(b.data(), b.size(), "apc_02", rec);
  uint32_t pairs = 0;
  if (out.size() > 16) std::memcpy(&pairs, out.data() + 12, 4);
  EXPECT(out.size() == b.size() + 8 + 6 + rec.size() && pairs == 5);
  EXPECT(std::equal(b.begin() + 16, b.end(), out.begin() + 16));  // the shipped record as it was
  mech3::net_editor_free();
  mech3::editor_exit();
  mech3::editor_reset();
  mech3::set_editor_hooks(nullptr);
}

// The vehicle editor's shell (S7c; 0x004b86e0, docs/editor.md, The vehicle
// editor): the Mech AI form lists the hooks' vehicles; the first is loaded
// (its brain's settings, no aiv record here, so one is made and kept in the
// cache); a team set and Apply puts it on the brain with the masks the store
// builds (0x004d71a0's round trip first); picking another stores the first
// and loads it; Refresh finds the shown one again by name; OK closes.
namespace {
struct VehStub : mech3::VehicleEditorHooks {
  mech3::ai::AiParams a, b;
  int a_tag = 0, b_tag = 0;
  std::vector<mech3::VehicleEditorItem> list{{"apc_01", &a_tag}, {"tank_02", &b_tag}};
  uint32_t masks[3] = {};
  int pauses = 0;
  std::vector<mech3::VehicleEditorItem> vehicles() override { return list; }
  mech3::ai::AiParams* brain_params(void* v) override { return v == &a_tag ? &a : v == &b_tag ? &b : nullptr; }
  void set_masks(void*, uint32_t t, uint32_t al, uint32_t en) override { masks[0] = t, masks[1] = al, masks[2] = en; }
  uint32_t player_allies() override { return 2; }
  uint32_t player_enemies() override { return 4; }
  void pause(bool on) override { pauses += on ? 1 : 10; }
};
}  // namespace

TEST(vehicle_editor_shell_loads_and_applies) {
  mech3::ai::aiv_file_reset();
  VehStub h;
  h.a.put<int32_t>(0x18, 2), h.b.put<int32_t>(0x18, 1);
  mech3::set_vehicle_editor_hooks(&h);
  mech3::edit_vehicles(0);
  mech3::VehicleEditor* e = mech3::vehicle_editor();
  EXPECT(e && mech3::form_shown(&e->form) && e->form.title == "Mech AI");
  if (!e) return;
  EXPECT(e->form.fields[0].choices == std::vector<std::string>({"apc_01", "tank_02"}) && e->selected == 0);
  EXPECT(e->form.fields[1].kind == mech3::FormField::kTabs && e->form.fields[1].choices.size() == 7);
  // Team 2's mask is bit 2 (4), the player's enemies here: the Foe button.
  EXPECT(e->params.team() == 2 && e->team_side == 1);
  EXPECT(mech3::ai::aiv_file().cached("apc_01") != nullptr);  // the record made from the brain, kept
  e->params.put<int32_t>(0x18, 1);  // team 1 (the ally)
  e->form.press("Apply");
  EXPECT(h.a.team() == 1 && h.masks[0] == 2 && h.masks[1] == 2 && h.masks[2] == (~2u & ~1u));
  e->params.put<int32_t>(0x18, 2);  // team 2
  EXPECT(e->form.press("Apply"));
  EXPECT(h.a.team() == 2 && h.masks[0] == 4 && h.masks[1] == 4 && h.masks[2] == (~4u & ~1u));
  EXPECT(mech3::ai::aiv_file().cached("apc_01")->team() == 2);
  // Another vehicle: the first stored as shown, the second loaded.
  e->params.put<int32_t>(0x18, 3);
  *e->form.fields[0].i = 1;
  e->form.changed(e->form.fields[0]);
  EXPECT(h.a.team() == 3 && e->selected == 1 && e->params.team() == 1);
  // Refresh: the list again, tank_02 found by name ahead of a new first.
  h.list.insert(h.list.begin(), {"jeep_03", &h.a_tag});
  EXPECT(e->form.press("Refresh"));
  e = mech3::vehicle_editor();
  EXPECT(e && e->selected == 2 && e->items.size() == 3);
  // Pause's box asks the app; OK applies and closes.
  e->main.at(1218).check = 1;  // the box clicked
  for (mech3::FormField& f : e->form.fields)
    if (f.label == "Pause") e->form.changed(f);
  EXPECT(h.pauses == 1);
  EXPECT(e->form.press("OK"));
  EXPECT(!mech3::vehicle_editor());
  mech3::set_vehicle_editor_hooks(nullptr);
  mech3::ai::aiv_file_reset();
  mech3::aiv_notes_clear();  // the stubs' records (their orders' valves freed next)
}

// The pages p1 and p2 (S7d; docs/editor.md, The vehicle editor's pages): the
// controls are filled from the settings when a page is shown and read back
// by MFC's rules on Apply; a field that does not read stops the pass with
// its box; p2's lists (nets, loadouts, chassis) and its handlers.
namespace {
struct PageStub : VehStub {
  mech3::ConfigList loadouts{9, 3};
  int homes = 0;
  float home_at[4] = {};
  mech3::ai::AiNet* home_net = nullptr;
  PageStub() {
    loadouts.configs = {new mech3::Config(9, "Standard"), new mech3::Config(9, "mine"), new mech3::Config(9, "Assault")};
    loadouts.configs[1]->player_made = 1;
  }
  const mech3::ConfigList* configs(void* v) override { return v == &a_tag ? &loadouts : nullptr; }
  void send_home(void*, mech3::ai::AiNet* n, const float at[3], float yaw) override {
    ++homes, home_net = n, home_at[0] = at[0], home_at[1] = at[1], home_at[2] = at[2], home_at[3] = yaw;
  }
};
mech3::FormField* field_named(mech3::VehicleEditor* e, const char* label) { return e->form.field(label); }
}  // namespace

TEST(vehicle_editor_pages_show_and_take_back) {
  mech3::ai::aiv_file_reset();
  mech3::ai::AiNet net7(7), net3(3);
  PageStub h;
  h.a.put<int32_t>(0x04, 7);         // its home net
  h.a.put<float>(0x08, 12.5f);       // x
  h.a.put<float>(0x14, 1.5707964f);  // the yaw, radians in play
  h.a.put<int32_t>(0x18, 2);
  h.a.put<int32_t>(0xbc, 2);         // loadout 2, Assault
  h.a.put<int32_t>(0xc4, 9);         // madcat
  h.a.put<int32_t>(0xc8, 40);        // the skin
  h.a.put<int32_t>(0x8c, 1);         // range to the player
  h.a.pilot = strdup("Dom");
  mech3::set_vehicle_editor_hooks(&h);
  mech3::edit_vehicles(0);
  mech3::VehicleEditor* e = mech3::vehicle_editor();
  EXPECT(e && e->page == mech3::kPageP1);
  if (!e) return;
  mech3::Dialog& p1 = e->pages[mech3::kPageP1];
  mech3::Dialog& p2 = e->pages[mech3::kPageP2];
  EXPECT(p1.at(1229).combo.cur == 0 && p1.at(1257).check == 2);  // "player"; Enable Now "as the brain has it"
  // p2 is filled when it is shown: the place, the yaw back in degrees, the pilot, the lists.
  *field_named(e, "##pages")->i = mech3::kPageP2;
  e->form.changed(*field_named(e, "##pages"));
  EXPECT(e->page == mech3::kPageP2);
  EXPECT(p2.at(1200).text == "12.5" && p2.at(1203).text == "90" && p2.at(1243).text == "Dom" && p2.at(1243).limit == 32);
  EXPECT(p2.at(1284).text == "40" && p2.at(1111).text == "2" && p2.at(1197).check == 1);  // team 2: Foe
  const mech3::DlgCombo& nets = p2.at(1238).combo;
  EXPECT(nets.texts() == std::vector<std::string>({"net_03", "net_07"}) && nets.cur == 1);
  const mech3::DlgCombo& cfg = p2.at(1273).combo;
  EXPECT(cfg.texts() == std::vector<std::string>({"Assault", "Standard"}) && cfg.cur == 0 && cfg.data(0) == 2);
  const mech3::DlgCombo& ch = p2.at(1274).combo;
  EXPECT(ch.items.size() == 20 && ch.cur >= 0 && ch.items[size_t(ch.cur)].text == "madcat");
  // Typed and picked, then Apply: on the brain in play units.
  p2.at(1200).text = " 30 ";
  p2.at(1203).text = "180";
  p2.at(1243).text = "Alan";
  p2.at(1273).combo.cur = 1;  // Standard
  e->form.changed(*field_named(e, "Configuration"));
  EXPECT(e->params.get<int32_t>(0xbc) == 0);
  p2.at(1238).combo.cur = 0;  // net_03
  e->form.changed(*field_named(e, "Home Net"));
  EXPECT(e->params.get<int32_t>(0x04) == 3 && p2.at(1117).text == "3");
  e->form.press("Apply");
  EXPECT(h.a.get<float>(0x08) == 30.0f && std::fabs(h.a.get<float>(0x14) - 3.14159265f) < 1e-5f);
  EXPECT(h.a.pilot && std::string(h.a.pilot) == "Alan" && h.a.get<int32_t>(0xbc) == 0 && h.a.get<int32_t>(0x04) == 3);
  // The Friend radio: the first of the player's allies (2: team 1).
  p2.at(1197).check = 0;
  e->form.changed(*field_named(e, "Team"));
  EXPECT(e->params.team() == 1 && p2.at(1111).text == "1");
  // A text that is no number stops the pass at its field, with MFC's box;
  // the fields before it are taken.
  p2.at(1201).text = "5";
  p2.at(1202).text = "-0";  // MFC refuses a zero read from anything but '0'
  p2.at(1284).text = "77";
  e->form.press("Apply");
  EXPECT(mech3::form_shown(&e->message));  // (the show pass after the store clears ddx_message)
  EXPECT(e->message.fields.size() == 1 && e->message.fields[0].label == "Please enter a number.");
  EXPECT(h.a.get<float>(0x0c) == 5.0f && h.a.get<int32_t>(0xc8) == 40);  // y taken, the skin not
  e->message.press("OK");
  EXPECT(!mech3::form_shown(&e->message));
  // Send home: at the place last exchanged, on the home net.
  e->form.changed(*field_named(e, "Send home"));
  EXPECT(h.homes == 1 && h.home_net == &net3 && h.home_at[0] == 30.0f && h.home_at[3] == 180.0f);
  // p1: Enable Now checked is put on the brain's +0x3c at Apply, then reset.
  *field_named(e, "##pages")->i = mech3::kPageP1;
  e->form.changed(*field_named(e, "##pages"));
  p2.at(1202).text = "0";
  EXPECT(e->page == mech3::kPageP1);
  p1.at(1257).check = 1;
  p1.at(1229).combo.cur = 1;  // "nearest enemy"
  e->form.press("Apply");
  EXPECT(h.a.get<int32_t>(0x3c) == 1 && e->keep[4] == 2 && h.a.get<int32_t>(0x8c) == 0);
  EXPECT(e->form.press("OK") && !mech3::vehicle_editor());
  std::free(h.a.pilot), h.a.pilot = nullptr;
  for (mech3::Config*& c : h.loadouts.configs) delete c, c = nullptr;
  mech3::set_vehicle_editor_hooks(nullptr);
  mech3::ai::aiv_file_reset();
  mech3::aiv_notes_clear();  // the stubs' records (their orders' valves freed next)
}

// S7e1: Attack with its strategy list and its own tab (Circle first), a
// field two pages share (Direct's approach is p1's), Retreat's flag bits,
// p5's "set value" box into the store's word, Attack's 0..1000 check.
TEST(vehicle_editor_attack_retreat_p5_fire) {
  mech3::ai::aiv_file_reset();
  VehStub h;
  h.a.put<int32_t>(0x40, 2);      // Circle
  h.a.put<float>(0x44, 75.0f);    // its radius
  h.a.put<float>(0x28, 300.0f);   // the approach range
  h.a.raw[0xcc] = 0x02;           // Retreat on Time
  mech3::set_vehicle_editor_hooks(&h);
  mech3::edit_vehicles(0);
  mech3::VehicleEditor* e = mech3::vehicle_editor();
  EXPECT(e && e->strategy_page == mech3::kPageCircle);
  if (!e) return;
  mech3::Dialog& attack = e->pages[mech3::kPageAttack];
  mech3::vehicle_editor_tab(mech3::kPageAttack);
  const mech3::DlgCombo& st = attack.at(1233).combo;
  EXPECT(st.texts() == std::vector<std::string>({"Circle", "Direct", "Jump", "Random Move", "Zig Zag"}));
  EXPECT(st.cur == 0 && e->pages[mech3::kPageCircle].at(1220).text == "75");
  // Another strategy; Direct's tab shows p1's approach range.
  attack.at(1233).combo.cur = 4;  // Zig Zag
  mech3::vehicle_editor_strategy_tab(mech3::kPageDirect);
  EXPECT(e->strategy_tab == 1 && e->pages[mech3::kPageDirect].at(1212).text == "300");
  e->pages[mech3::kPageDirect].at(1212).text = "250";
  attack.at(1272).text = "1000.5";  // past Attack's 0..1000: taken, then the box
  e->form.press("Apply");
  EXPECT(mech3::form_shown(&e->message) && e->message.fields[0].label == "Please enter a number between 0 and 1000.");
  // The pass stopped at 1272: the strategy list after it was not read; Direct's
  // field, exchanged first, was.
  EXPECT(h.a.get<int32_t>(0x40) == 2 && h.a.get<float>(0x28) == 250.0f);
  EXPECT(e->params.get<float>(0xb8) == 1000.5f);  // the DDV runs after the field is taken
  e->message.press("OK");
  EXPECT(attack.at(1233).combo.cur == 0);  // shown again from the settings: Circle
  attack.at(1272).text = "1";
  attack.at(1233).combo.cur = 4;
  e->form.press("Apply");
  EXPECT(h.a.get<int32_t>(0x40) == 1 && !mech3::form_shown(&e->message));
  // p1 shows Direct's change; Retreat its bits; p5's Time "set" box into the store.
  mech3::vehicle_editor_tab(mech3::kPageP1);
  EXPECT(e->pages[mech3::kPageP1].at(1212).text == "250");
  mech3::vehicle_editor_tab(mech3::kPageRetreat);
  mech3::Dialog& retreat = e->pages[mech3::kPageRetreat];
  EXPECT(retreat.at(1224).check == 1 && retreat.at(1221).check == 0);
  retreat.at(1221).check = 1;  // Net Distance
  mech3::vehicle_editor_tab(mech3::kPageP5);
  e->pages[mech3::kPageP5].at(1245).check = 1;  // Time, set
  e->form.press("Apply");
  EXPECT((h.a.raw[0xcc] & 0x03) == 0x03 && h.a.get<int32_t>(0xa0) == 1 && e->keep[2] == 2);
  EXPECT(e->form.press("OK") && !mech3::vehicle_editor());
  mech3::set_vehicle_editor_hooks(nullptr);
  mech3::ai::aiv_file_reset();
  mech3::aiv_notes_clear();  // the stubs' records (their orders' valves freed next)
}

// S7e2: the Valve page: the valves listed; one picked gets an entry of the
// defaults in the settings (0x004d9580), its numbers and conditions shown and
// taken; a name typed makes the valve (0x004d3c40) and lists it.
TEST(vehicle_editor_valve_page) {
  mech3::ai::aiv_file_reset();
  mech3::ai::valves_clear();
  mech3::ai::valve_get("door_a");
  mech3::ai::valve_get("alarm");
  VehStub h;
  mech3::set_vehicle_editor_hooks(&h);
  mech3::edit_vehicles(0);
  mech3::VehicleEditor* e = mech3::vehicle_editor();
  EXPECT(e != nullptr);
  if (!e) return;
  mech3::Dialog& valve = e->pages[mech3::kPageValve];
  mech3::vehicle_editor_tab(mech3::kPageValve);
  EXPECT(valve.at(1176).combo.texts() == std::vector<std::string>({"alarm", "door_a"}) && valve.at(1176).combo.cur == -1);
  valve.at(1176).combo.cur = 1;  // door_a
  e->form.changed(*e->form.field("Valve"));
  EXPECT(e->valve_name == "door_a" && e->params.vlist && e->params.vlist->size() == 1);
  EXPECT(valve.at(1192).text == "0.1" && valve.at(1176).combo.cur == 1);  // the defaults' f04 (0x005e8da0)
  valve.at(1180).check = 1;  // Health
  valve.at(1192).text = "0.5";
  valve.at(1178).check = 1;  // open (one)
  e->form.press("Apply");
  EXPECT(h.a.vlist && h.a.vlist->size() == 1);
  if (h.a.vlist && !h.a.vlist->empty()) {
    const mech3::ai::VListEntry& v = h.a.vlist->front();
    EXPECT(v.valve && std::string(v.valve->name) == "door_a" && v.f04 == 0.5f && (v.flags & 0x12) == 0x12);
  }
  // A name typed and sent: the valve made and listed, an entry for it.
  valve.at(1176).combo.cur = -1;
  valve.at(1176).text = "gate_9";
  e->form.field("Valve")->left();
  EXPECT(mech3::ai::valve_find("gate_9") && valve.at(1176).combo.texts().size() == 3 && e->params.vlist->size() == 2);
  EXPECT(e->form.press("OK") && !mech3::vehicle_editor());
  mech3::set_vehicle_editor_hooks(nullptr);
  mech3::ai::aiv_file_reset();
  mech3::aiv_notes_clear();  // the stubs' records (their orders' valves freed next)
  mech3::ai::valves_clear();
}

// The editor's guide (ours, docs/editor-guide.md) is the help tables' output:
// when it is not, `build/mw3 --editor-guide docs/editor-guide.md` brings it up
// to date. Every tip filled, each item once in its table; the HELP rows open
// the quick start.
TEST(editor_guide_is_current) {
  std::ifstream f(std::string(MW3_SOURCE_DIR) + "/docs/editor-guide.md", std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  const bool same = ss.str() == mech3::editor_guide_markdown();
  if (!same) std::printf("  stale: run build/mw3 --editor-guide docs/editor-guide.md\n");
  EXPECT(same);
  for (const mech3::HelpForm& form : mech3::help_forms()) {
    std::set<std::string> seen;
    for (const mech3::HelpTip& t : form.tips) {
      const bool fresh = seen.insert(t.item).second;
      if (!fresh) std::printf("  %s: %s twice\n", form.form, t.item);
      EXPECT(fresh && *t.text);
    }
  }
  mech3::help_open(0);
  EXPECT(mech3::form_shown(&mech3::help_window()) &&
         mech3::help_window().fields.size() == mech3::help_quick_start().size());
  EXPECT(mech3::help_window().press("Close") && !mech3::form_shown(&mech3::help_window()));
  EXPECT(mech3::help_tip("Node Parameters: node_07", "speed") != nullptr);  // the title's node number aside
  EXPECT(mech3::help_tip("Mech AI", "no such field") == nullptr);
}
