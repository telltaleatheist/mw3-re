#pragma once
#include <cstdint>

#include "gamez/zgame/unported.h"
#include "gamez/zinput/zinput.h"
#include "gamez/ztwod/twod.h"

// Zipper's Battlesport game editor, as far as v1.0 takes it (docs/editor.md
// S4): its panel of 25 text rows over the running mission, the main menu,
// the frame (the row under the mouse lit, Space or the left button on a row
// calls its entry), the entry on debug command 0x9a and the exit. The AI
// net and vehicle editors behind its rows come after v1.0. Original file
// unknown: these sit at 0x0040e270..0x0040e840, after the HUD's text
// (0x0040dXXX) and before the debug lines (0x0040f880).
namespace mech3 {

// A menu: its rows' text and what each calls (the mouse's y). In the exe
// {int count; {char text[20]; void (*handler)(int);} x count} (0x005e0d30).
struct EditorEntry {
  const char* text;
  void (*handler)(int32_t y);
};
struct EditorMenu {
  int32_t count;
  const EditorEntry* rows;
};
const EditorMenu& editor_main_menu_rows();  // 0x005e0d30: PICK A COMMAND .. EXIT GAME EDIT

// The panel (the static at 0x005f4d78; 0x0040e270(5, 0x5f), vtable
// 0x005bc510): an object at (5, 0x5f) holding a layer (+0x38, vtable
// 0x005bc590, [0x005f4db0]) of 25 rows (+0x48, 0x4b0 bytes each: FontText
// with the font "mech_string", 0x0040e840), 12 pixels apart, hidden; the
// rows' extent and the hovered row.
struct EditorPanel {
  ztwod::Layer layer;                       // +0x38
  ztwod::FontText* rows[25] = {};           // +0x48
  int32_t left = 5, top = 0x5f;             // +0x7578, +0x757c
  int32_t right = 5, bottom = 0x5f;         // +0x7580, +0x7584
  uint32_t color_top = 0, color_bottom = 0;  // +0x7588, +0x758c: the rows' own ([0x005fc300], [0x005fc304])
  int32_t hovered = -1;                     // +0x7590 ([0x005fc308])
  EditorPanel();                            // 0x0040e270(5, 0x5f)
  ~EditorPanel();                           // 0x0040e390
  EditorPanel(const EditorPanel&) = delete;
  EditorPanel& operator=(const EditorPanel&) = delete;
};

// The editor's state: the panel, the menu shown and its mode, whether the
// editor is up, whether the debug lines were on at its entry.
struct EditorState {
  EditorPanel* panel = nullptr;  // made on first use (the exe at start-up)
  int32_t mode = 0;              // [0x005fc30c]: 0 the main menu, 1..3 the net editor's (it steps them)
  const EditorMenu* menu = nullptr;  // [0x005fc318]
  int32_t active = 0;            // [0x005fc314]
  int32_t debug_lines = 0;       // [0x005f4dac]
};
EditorState& editor_state();
EditorPanel& editor_panel();
void editor_reset();  // the panel freed (the tests, the program's end)

// The calls out.
class EditorHooks : public zgame::Unported {
 public:
  virtual zin::MouseInfo mouse() { return *zin::mouse_info(); }        // 0x0051b7b0
  virtual uint32_t command(int32_t cmd) { return zin::command(cmd); }  // 0x0051d300
  virtual void input_reset() { zin::reset(); }                        // 0x0051b1c0
  virtual void push_map() { zin::push_map(nullptr); }                  // 0x0051d3a0
  virtual void pop_map() { zin::pop_map(); }                           // 0x0051d490
  virtual void bind(int32_t cmd, uint32_t key, uint32_t mouse) { zin::bind(cmd, nullptr, key, 0, 0, mouse); }  // 0x0051d2c0
  virtual float frame_dt();                                            // [0x0072c52c]
  virtual ztwod::Handle font(const char* name);                               // 0x00517680
  // The view onto the stealth object ([0x00609a2c] vt+0x108, 0x004acd90);
  // ours, at the exit: back onto the pilot's mech (its vt+0x108, 0x0041ce30).
  virtual void view_on_stealth() { unported("stealth.vt+0x108", {}); }
  virtual void view_on_pilot() {}
  // The debug lines' layers (0x0040f880 [0x005fc33c]; off 0x0040f8b0, on
  // 0x0040f890): never on in the release (hud.md section 20).
  virtual int32_t debug_lines() { return 0; }
  virtual void debug_lines_show(bool on) { unported(on ? "0x0040f890" : "0x0040f8b0", {}); }
  // The net editor's (after v1.0): its 0.1 s step in modes 1..3
  // (0x00403140) and its frame (0x00404290, nothing without a net editor).
  virtual void net_step(float dt);  // game_editor_net.cpp: net_editor_step
  virtual void net_frame(bool editing) { (void)editing; }
  // The net editor's (sitting 3a): the view's vehicle put at a place (its
  // vt+0xdc, GOTO Net).
  virtual void view_vehicle_place(const float p[3]) { unported("view vehicle.vt+0xdc", {zgame::bits(p[0]), zgame::bits(p[2])}); }
  // Its frame's (sitting 3b, 0x00403160): the player view's camera set again
  // (0x004b51f0), the screen's size (0x00558080), the camera's clip planes
  // (0x004f3b00 / 0x004f3a70; a far of 0: none to keep), its place and pitch
  // (its data +0x14, +0x20), the view vehicle's pick (vt+0xf8: the stealth's
  // reticle point, 0x004a5170; false without its overlay) and the world's
  // segment query (0x004eed90 on [0x00608c84] +4: true when it crosses
  // nothing; false without a world).
  virtual void view_refresh() {}
  virtual void screen_size(int32_t* w, int32_t* h) { *w = 640, *h = 480; }
  virtual void camera_clip(float* near_z, float* far_z) { *near_z = 1.0f, *far_z = 0.0f; }
  virtual void set_camera_clip(float near_z, float far_z) { (void)near_z, (void)far_z; }
  virtual const float* camera_pos() { return nullptr; }
  virtual float camera_pitch() { return 0.0f; }
  virtual bool ground_pick(float out[3]) { (void)out; return false; }
  virtual bool clear_line(const float* a, const float* b) { (void)a, (void)b; return false; }
  virtual void not_ported(const char* what);  // a row whose editor comes after v1.0: said in the log
};
void set_editor_hooks(EditorHooks* h);  // null: the defaults
EditorHooks& editor_hooks();

void editor_fill(EditorPanel& p, const EditorMenu& m);         // 0x0040e6a0
void editor_install(int32_t mode, const EditorMenu& m);       // 0x0040e7b0
void editor_main_menu();                                       // 0x0040e7d0
void editor_enter();                                           // 0x0040e430: command 0x9a's
void editor_exit(int32_t y = 0);                               // 0x0040e7f0: EXIT GAME EDIT's (ours: the view back)
void editor_frame();                                           // 0x0040e4b0, each frame from the HUD's
// Ours (docs/editor.md S4): command 0x9a, Shift+F12 under --debug-camera
// (debugcamera=1): the editor's entry, or its exit when it is up.
void editor_key(int32_t cmd);
constexpr int32_t kEditorCommand = 0x9a;
constexpr uint32_t kEditorKey = 0x458;  // Shift+F12: nothing in the default keys uses it

}  // namespace mech3
