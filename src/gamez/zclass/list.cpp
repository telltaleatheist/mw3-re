#include "gamez/zclass/list.h"

#include <cstring>
#include <new>

#include "gamez/zclass/class.h"
#include "gamez/zclass/seq.h"

namespace zcls {

namespace {
Lists g_lists;
ListWalk g_walk;

// 0x004f8be0
bool name_matches(Node* node) { return std::strcmp(node->name(), static_cast<const char*>(g_walk.key)) == 0; }

// List 7: the node is marked as on it, and its unlisted ancestors (up to a
// world) are appended after it.
void mark_changed(Node* node, ListItem* item) {
  const uint32_t flags = node->flags() | 1;
  node->changed = item;
  node->set_flags(flags);
  item->removable = (~flags >> 1) & 1;
  for (int32_t i = 0; i < node->parent_count(); ++i) {
    Node* p = node->parents[i];
    if ((p->flags() & 1) == 0 && p->cls() != 2) push_back(kChanged, p);
  }
}
}  // namespace

Lists& lists() { return g_lists; }

// Mech3.exe 1.2 @ 0x004f77d0. A new chunk puts items 1..n-1 on the free list
// and hands out item 0. FIX (A): chunks are owned arrays.
ListItem* item_alloc() {
  Lists& l = g_lists;
  if (ListItem* item = l.free) {
    l.free = item->next;
    if (l.free) l.free->prev = nullptr;
    item->next = nullptr, item->prev = nullptr, item->removable = 0;
    return item;
  }
  ListItem* chunk = new (std::nothrow) ListItem[l.chunk_size]{};
  if (!chunk) return nullptr;
  l.chunks.emplace_back(chunk);
  l.free = chunk + 1;
  chunk[1].prev = nullptr;
  for (int32_t i = 2; i < l.chunk_size; ++i) {
    chunk[i - 1].next = chunk + i;
    chunk[i].prev = chunk + i - 1;
  }
  chunk[l.chunk_size - 1].next = nullptr;
  chunk[0].next = nullptr, chunk[0].prev = nullptr;
  return chunk;
}

// Mech3.exe 1.2 @ 0x004f78e0
void item_free(ListItem* item) {
  item->prev = nullptr;
  item->next = g_lists.free;
  if (g_lists.free) g_lists.free->prev = item;
  g_lists.free = item;
}

// Mech3.exe 1.2 @ 0x004f89a0. The item chunks stay.
void reset_lists() {
  g_lists.enabled = 1;
  for (List& list : g_lists.list) list = {};
  g_lists.free = nullptr, g_lists.deferred = nullptr;
}

// Mech3.exe 1.2 @ 0x004f7f80
int list_count(int list) {
  int n = 0;
  for (ListItem* i = g_lists.list[list].head; i; i = i->next) ++n;
  return n;
}

// Mech3.exe 1.2 @ 0x004f8080
int push_front(int list, Node* node) {
  if (list == kChanged && node->get<uint32_t>(0xc4) <= frames().frame) {
    frames().expiry = frames().frame + 0xa0;
    set_expiry(node);
  }
  ListItem* item = item_alloc();
  item->node = node;
  List& l = g_lists.list[list];
  if (!l.head) {
    l.tail = item;
  } else {
    item->next = l.head;
    l.head->prev = item;
  }
  l.head = item;
  if (list == kChanged) mark_changed(node, item);
  return 0;
}

// Mech3.exe 1.2 @ 0x004f8140
int push_back(int list, Node* node) {
  ListItem* item = item_alloc();
  item->node = node;
  List& l = g_lists.list[list];
  if (!l.head) {
    l.head = l.tail = item;
  } else {
    item->prev = l.tail;
    l.tail->next = item;
    l.tail = item;
  }
  if (list == kChanged) mark_changed(node, item);
  return 0;
}

namespace {
void (*g_destroy)(Node* node) = nullptr;
}  // namespace

void set_node_destroy(void (*destroy)(Node* node)) { g_destroy = destroy; }

// Mech3.exe 1.2 @ 0x004f8220. List 7 goes through the node's own item
// (kept when the node's flag 2 is set); the others take the first item of
// this node that is not already marked.
int list_remove(int list, Node* node) {
  List& l = g_lists.list[list];
  if (!l.head) return 1;
  ListItem* item = nullptr;
  if (list == kChanged && node->changed) {
    if (node->flags() & 2) return 0;
    item = node->changed;
    node->changed = nullptr;
    node->set_flags(node->flags() & ~1u);
  } else {
    for (item = l.head; item; item = item->next)
      if (item->node == node && item->removable == 0) break;
  }
  if (!item) return 0;
  item->node = nullptr;
  item->removable = 1;
  l.needs_purge = 1;
  return 0;
}

// Mech3.exe 1.2 @ 0x004f7980. Unlinks and frees every item marked removable
// or without a node; on list 7 an item whose node has flag 2 is kept (unmarked).
int list_purge(int list) {
  if (!g_lists.enabled) return 0;
  List& l = g_lists.list[list];
  ListItem* head = l.head;
  ListItem* tail = l.tail;
  ListItem* cursor = head;
  for (;;) {
    while (cursor && cursor->removable == 0 && cursor->node) cursor = cursor->next;
    if (!cursor) break;
    ListItem* item = cursor;
    cursor = item->next;
    if (list == kChanged && item->node && (item->node->flags() & 2)) {
      item->removable = 0;
      continue;
    }
    if (item == head) l.head = head = head->next;
    if (item == tail) l.tail = tail = tail->prev;
    if (item->prev) item->prev->next = item->next;
    if (item->next) item->next->prev = item->prev;
    if (list == kChanged && item->node) {
      item->node->set_flags(item->node->flags() & ~1u);
      item->node->changed = nullptr;
    }
    item_free(item);
  }
  l.needs_purge = 0;
  return 0;
}

// Mech3.exe 1.2 @ 0x004f7bd0. In the lists' memory order (6, 0..5, 7..10,
// 13..15, 11, 12), then 0x004f81e0 empties the deferred-delete list.
int purge_lists() {
  if (!g_lists.enabled) return 1;
  static const int kOrder[] = {6, 0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 13, 14, 15, 11, 12};
  for (int list : kOrder)
    if (g_lists.list[list].needs_purge) list_purge(list);
  while (ListItem* item = g_lists.deferred) {
    g_lists.deferred = item->next;
    if (item->node && g_destroy) g_destroy(item->node);
    item_free(item);
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x004f7d20
int run_actions(ListItem* head) {
  const int32_t enabled = g_lists.enabled;
  g_lists.enabled = 0;
  for (ListItem* item = head; item; item = item->next) {
    Node* node = item->node;
    if (!node || !node->action) {
      item->removable = 1;
      continue;
    }
    if (item->removable == 0 && (node->flags() & 4)) reinterpret_cast<NodeAction>(node->action)(node);
  }
  g_lists.enabled = enabled;
  return purge_lists();
}

// Mech3.exe 1.2 @ 0x004f7d80. An empty list is skipped, frame update included.
void run_action_lists() {
  for (int list = 0; list < 6; ++list) {
    if (!g_lists.list[list].head) continue;
    run_actions(g_lists.list[list].head);
    if (update_hooks().frame_update) update_hooks().frame_update(update_hooks().ctx);
  }
}

ListWalk& list_walk() { return g_walk; }

// Mech3.exe 1.2 @ 0x004f8040
ListItem* list_head(int list) { return g_lists.list[list].head; }

// Mech3.exe 1.2 @ 0x004f7fe0. The first node by name; removed items (no node) are skipped.
Node* list_find_by_name(int list, const char* name) {
  for (ListItem* i = g_lists.list[list].head; i; i = i->next)
    if (i->node && std::strcmp(i->node->name(), name) == 0) return i->node;
  return nullptr;
}

// Mech3.exe 1.2 @ 0x004f8b20
Node* list_walk_next(const void* key, int list, bool (*test)(Node* node)) {
  if (key) {
    g_walk.key = key;
    g_walk.cursor = list_head(list);
    return nullptr;
  }
  while (g_walk.cursor) {
    Node* node = g_walk.cursor->node;
    g_walk.cursor = g_walk.cursor->next;
    if (node && test(node)) return node;
  }
  return nullptr;
}

// Mech3.exe 1.2 @ 0x004f8bd0
Node* list_walk_by_name(const char* name, int list) { return list_walk_next(name, list, name_matches); }

namespace {
// 0x004f7ec0 / 0x004f7f10: one class list's frame steps with purging held
// off, then the purge.
template <typename Step>
int run_class_list(int list, Step step) {
  ListItem* item = g_lists.list[list].head;
  if (!item) return 0;
  const int32_t enabled = g_lists.enabled;
  g_lists.enabled = 0;
  for (; item; item = item->next)
    if (item->removable == 0 && item->node) step(item->node);
  g_lists.enabled = enabled;
  purge_lists();
  return 0;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004f7db0. Changed children (flag 2) first, depth first;
// then the node's own update and its item on list 7 released.
int update_subtree(Node* node) {
  for (int32_t i = 0; i < node->child_count(); ++i)
    if (node->children[i]->flags() & 2) update_subtree(node->children[i]);
  node_update(node);
  list_remove(kChanged, node);
  return 0;
}

// Mech3.exe 1.2 @ 0x004f7e00. The node's changed subtree, then each parent's
// (worlds are left to the frame); the lists are purged when not held off.
void update_node(Node* node) {
  if (node->flags() & 2) update_subtree(node);
  for (int32_t i = 0; i < node->parent_count(); ++i)
    if (node->parents[i]->cls() != 2) update_node(node->parents[i]);
  if (g_lists.enabled) purge_lists();
}

// Mech3.exe 1.2 @ 0x004f7e50. Takes list 7's first held item (removable
// 0) until none is left: its node marked (flags 1 and 2) and brought up to
// date with its ancestors. The head's node is marked on list 7 each pass
// even when its item is released.
int update_changed() {
  ListItem* item = g_lists.list[kChanged].head;
  while (item) {
    if (Node* n = item->node; n && !(n->flags() & 1)) n->set_flags(n->flags() | 1);
    if (item->removable == 0 && item->node) {
      Node* n = item->node;
      if (!(n->flags() & 2)) {
        n->set_flags(n->flags() | 2);
        if (n->changed) n->changed->removable = 0;
      }
      update_node(n);
    }
    item = g_lists.list[kChanged].head;
    while (item && item->removable != 0) item = item->next;
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x004f7ec0: the sequences (list 11).
int sequences_update() { return run_class_list(11, seq_update); }

// Mech3.exe 1.2 @ 0x004f7f10: the active animates (list 12).
int animates_update() {
  return run_class_list(12, [](Node* n) {
    if (n->flags() & 4) animate_update(n);
  });
}

// Mech3.exe 1.2 @ 0x004f7f70: after each action list, and where a frame
// needs the scene current (the mission start, the camera's move).
void frame_update() {
  sequences_update();
  animates_update();
  update_changed();
}

}  // namespace zcls
