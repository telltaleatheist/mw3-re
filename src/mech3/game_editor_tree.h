#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "upgrades/editor/form.h"

namespace zcls { struct Node; }

// The net editor's Tree View (0x00402c70 -> 0x00402bf0 -> 0x004b5580,
// template 155, docs/editor.md, The tree view): the world's node tree as
// the exe's tree control shows it, the world and its children, then each
// partition cell "area R C" with its nodes. Only a viewer: nothing it shows
// can be changed. Original file unknown: the window's class sits at
// 0x004b5580..0x004b5860, among the vehicle editor's (S7). FIX (A): a form
// (form.h) with the tree, drawn by ImGui as a tree control.
namespace mech3 {

// An item: its text, its node (`data`, a zcls::Node; null for an "area"
// item), its children, and the control's expanded state.
using TreeItem = FormTree;
struct TreeWindow {
  TreeItem root;  // the world's
};
TreeWindow*& tree_window();  // [0x005f4c70]

void tree_view(int32_t y);  // 0x00402c70: the window made once (0x00402bf0) and shown
void tree_close();          // OK / Cancel, Exit Net Editor (vt+0x60: DestroyWindow -> 0x004b5830)
// 0x004b56b0 (OnInitDialog) without the window: the items of `world`.
TreeItem tree_of_world(const zcls::Node* world);
EditorForm& tree_form();  // the window (form.h)
// The items shown now (under open ones): "  " per depth, "+ " closed with
// children, "- " open, the item's text. For the tests.
std::vector<std::string> tree_rows();
void tree_press(int32_t row);  // the shown item `row` opened or closed, as a click on it (the tests)

}  // namespace mech3
