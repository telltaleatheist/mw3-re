#include "mech3/game_editor_tree.h"

#include <algorithm>
#include <cstdio>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "mech3/game_editor.h"
#include "mech3/game_editor_net.h"

namespace mech3 {

namespace {
TreeWindow* g_tree = nullptr;
EditorForm g_form;

// Mech3.exe 1.2 @ 0x004b57d0: the node, then its children (+0x60, count
// +0x5c), each under it, last after last.
TreeItem item_of(const zcls::Node* n, int depth = 0) {
  TreeItem t;
  t.data = n;
  t.text = n->name();
  if (depth < 64)
    for (int32_t i = 0; i < n->child_count(); ++i)
      if (n->children[i]) t.kids.push_back(item_of(n->children[i], depth + 1));
  return t;
}

void flatten(TreeItem& t, int depth, std::vector<std::pair<TreeItem*, int>>& out) {
  out.push_back({&t, depth});
  if (t.open)
    for (TreeItem& k : t.kids) flatten(k, depth + 1, out);
}
std::vector<std::pair<TreeItem*, int>> shown_items() {
  std::vector<std::pair<TreeItem*, int>> v;
  if (g_tree) flatten(g_tree->root, 0, v);
  return v;
}

void close_form() { tree_close(); }

void build() {
  g_form = EditorForm{};
  g_form.title = "Tree";
  FormField tree{"##tree", FormField::kTree};
  tree.tree = &g_tree->root;
  g_form.fields = {tree};
  g_form.buttons = {{"OK", close_form}, {"Cancel", close_form}};
}
}  // namespace

TreeWindow*& tree_window() { return g_tree; }

// Mech3.exe 1.2 @ 0x004b56b0: the world's item (0x004b57d0 with no parent),
// then under it, for each partition row and column, "area R C" (0x005e8858)
// holding the cell's nodes (+0x3c, count i16 +0x3a), each with its children.
TreeItem tree_of_world(const zcls::Node* world) {
  TreeItem root = item_of(world);
  const auto* w = static_cast<const zcls::World*>(world->data);
  if (!w || !w->rows) return root;
  const int32_t rows = w->get<int32_t>(0x8c), cols = w->get<int32_t>(0x88);
  for (int32_t r = 0; r < rows; ++r)
    for (int32_t c = 0; c < cols; ++c) {
      char name[64];
      std::snprintf(name, sizeof name, "area %d %d", r, c);
      TreeItem a;
      a.text = name;
      const zcls::Partition& p = w->rows[r][c];
      const int32_t n = p.get<int16_t>(0x3a);
      for (int32_t i = 0; i < n && p.nodes; ++i)
        if (p.nodes[i]) a.kids.push_back(item_of(p.nodes[i]));
      root.kids.push_back(std::move(a));
    }
  return root;
}

// Mech3.exe 1.2 @ 0x00402c70 -> 0x00402bf0: only with an editor; the window
// made once ([0x005f4c70]), on the world ([0x00608c84] +4).
void tree_view(int32_t) {
  if (!net_editor()) return;
  if (!g_tree) {
    const zcls::Node* world = static_cast<const zcls::Node*>(editor_hooks().world_node());
    if (!world) {
      editor_status("no world to show");
      return;
    }
    g_tree = new TreeWindow;
    g_tree->root = tree_of_world(world);
    build();
  }
  form_show(&g_form);
}

// Mech3.exe 1.2 @ 0x004b5830 (PostNcDestroy): the window deleted and its
// global cleared.
void tree_close() {
  form_hide(&g_form);
  delete g_tree;
  g_tree = nullptr;
}

EditorForm& tree_form() { return g_form; }

std::vector<std::string> tree_rows() {
  std::vector<std::string> out;
  for (const auto& [t, depth] : shown_items()) {
    const std::string mark = t->kids.empty() ? "  " : t->open ? "- " : "+ ";
    out.push_back(std::string(size_t(depth) * 2, ' ') + mark + t->text);
  }
  return out;
}
void tree_press(int32_t row) {
  const auto items = shown_items();
  if (row < 0 || row >= static_cast<int32_t>(items.size())) return;
  TreeItem* t = items[size_t(row)].first;
  if (!t->kids.empty()) t->open = !t->open;
}

}  // namespace mech3
