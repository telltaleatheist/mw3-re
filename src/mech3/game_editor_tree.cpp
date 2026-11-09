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
std::vector<std::string> g_texts;
std::vector<EditorEntry> g_rows;
std::vector<TreeItem*> g_items;  // the item each shown row is, null for the rest
EditorMenu g_menu{0, nullptr};
constexpr int32_t kRows = 20;  // ours: a page of items

// Mech3.exe 1.2 @ 0x004b57d0: the node, then its children (+0x60, count
// +0x5c), each under it, last after last.
TreeItem item_of(const zcls::Node* n, int depth = 0) {
  TreeItem t;
  t.node = n;
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

void fill();
void up(int32_t) { g_tree->first = std::max(0, g_tree->first - kRows), fill(); }
void down(int32_t) { g_tree->first += kRows, fill(); }
void press(int32_t y);
void close_rows(int32_t) {
  tree_close();
  net_editor_menu();
}

void fill() {
  g_texts.clear(), g_rows.clear(), g_items.clear();
  const auto add = [](const std::string& t, void (*h)(int32_t), TreeItem* it) {
    g_texts.push_back(t), g_rows.push_back({nullptr, h}), g_items.push_back(it);
  };
  add("Tree", nullptr, nullptr);
  const auto items = shown_items();
  const int32_t n = static_cast<int32_t>(items.size()), to = std::min(n, g_tree->first + kRows);
  if (g_tree->first > 0) add("   (up)", up, nullptr);
  for (int32_t i = g_tree->first; i < to; ++i) {
    TreeItem* t = items[size_t(i)].first;
    const std::string mark = t->kids.empty() ? "  " : t->open ? "- " : "+ ";
    add(" " + std::string(size_t(items[size_t(i)].second) * 2, ' ') + mark + t->text, press, t);
  }
  if (to < n) add("   (down)", down, nullptr);
  add(" OK", close_rows, nullptr);
  add(" Cancel", close_rows, nullptr);
  for (size_t i = 0; i < g_rows.size(); ++i) g_rows[i].text = g_texts[i].c_str();
  g_menu = EditorMenu{static_cast<int32_t>(g_rows.size()), g_rows.data()};
  editor_install(1, g_menu);
}

void press(int32_t y) {
  const int32_t r = (y - 0x5f) / 12;
  if (r < 0 || r >= static_cast<int32_t>(g_items.size()) || !g_items[size_t(r)]) return;
  TreeItem* t = g_items[size_t(r)];
  if (!t->kids.empty()) t->open = !t->open;
  fill();
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
  }
  fill();
}

// Mech3.exe 1.2 @ 0x004b5830 (PostNcDestroy): the window deleted and its
// global cleared.
void tree_close() {
  delete g_tree;
  g_tree = nullptr;
}

std::vector<std::string> tree_rows() { return g_texts; }
void tree_press(int32_t row) {
  if (row >= 0 && row < g_menu.count && g_rows[size_t(row)].handler) g_rows[size_t(row)].handler(0x5f + 12 * row + 2);
}

}  // namespace mech3
