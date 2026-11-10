#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Ours (docs/editor.md, Dear ImGui for the editor's dialogs): an editor
// dialog as data. Each dialog builds an EditorForm over its own state and
// shows it; the program's view (frontend/sdl/editor_imgui.cpp) draws every
// form shown, and the tests set the same fields and press the same buttons.
// Nothing here knows ImGui.
namespace mech3 {

struct DlgCombo;

struct FormTree {
  std::string text;
  const void* data = nullptr;  // what the item stands for (its owner's; null for none)
  std::vector<FormTree> kids;
  bool open = false;
};

struct FormField {
  enum Kind { kLabel, kInt, kFloat, kFlag, kTri, kText, kChoice, kTree, kTabs, kCheck, kRadio, kButton };
  std::string label;
  Kind kind = kLabel;
  int32_t* i = nullptr;          // kInt: the value; kChoice: the chosen index (-1 none); kTabs: the page shown;
                                 // kCheck: the button's check (0..max); kRadio: the button checked (-1 none)
  float* f = nullptr;            // kFloat
  int32_t* on = nullptr;         // kFloat: whether it is given (null: always)
  uint8_t* set = nullptr;        // kTri: whether it is given ("as the default" when not)
  uint8_t* flag = nullptr;       // kFlag, kTri: the value
  std::string* text = nullptr;   // kText
  const int32_t* limit = nullptr;  // kText: the most characters typed (0 none)
  bool read_only = false;        // kText: shown, not typed in (it can still be left: `left`)
  int32_t max = 1;               // kCheck: 2 for a three-state box (a click goes 0, 1, 2, 0)
  const DlgCombo* combo = nullptr;  // kChoice: the list read from a dialog's combo each frame (ddx.h)
  std::vector<std::string> choices;  // kChoice; kTabs: the pages' titles
  FormTree* tree = nullptr;      // kTree
  std::function<void()> changed;     // after a change through the form; kButton: its press
  std::function<void()> left;        // kText: after the field loses the focus (EN_KILLFOCUS)
  int32_t page = -1;             // shown only while the form's tabs show this page (-1: always)
};

struct FormButton {
  std::string label;
  std::function<void()> press;
};

struct EditorForm {
  std::string title;
  std::vector<FormField> fields;
  std::vector<FormButton> buttons;
  bool modal = false;  // drawn as a modal popup: nothing else in the editor's dialogs takes input

  FormField* field(const std::string& label);
  // A button pressed by its label: false when the form has none so named.
  bool press(const std::string& label);
  // A field changed through the form: its `changed` called (the view and the tests).
  void changed(FormField& f);
};

// The forms shown, in the order they were shown; the view draws each.
std::vector<EditorForm*>& editor_forms();
void form_show(EditorForm* f);  // shown again: moved to the end (on top)
void form_hide(EditorForm* f);
bool form_shown(const EditorForm* f);
// The view's presses and changes, run at the editor's next frame (editor_frame,
// in the game's update), not while the view draws in the renderer's present.
void form_post(std::function<void()> action);
void form_run_posted();

}  // namespace mech3
