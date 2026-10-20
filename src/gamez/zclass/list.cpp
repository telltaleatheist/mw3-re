#include "gamez/zclass/list.h"

#include <cstdio>
#include <cstring>
#include <new>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_mechlib.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zclass/light.h"
#include "gamez/zclass/seq.h"
#include "gamez/zclass/sound.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmodel/gmod_const.h"

namespace zcls {

namespace {
const char kListFile[] = "C:\\zipper\\GameZMech3\\zClass\\List.c";
const char kOldFile[] = "C:\\zipper\\GameZMech3\\zError\\zerr_old.c";  // 0x0050da70's reports
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
  while (ListItem* item = g_lists.deferred) {  // 0x004f81e0
    g_lists.deferred = item->next;
    if (item->node) node_free(item->node);
    item_free(item);
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x004f8050
int deferred_push(Node* node) {
  ListItem* item = item_alloc();
  item->node = node;
  if (g_lists.deferred) {
    item->next = g_lists.deferred;
    g_lists.deferred->prev = item;
  }
  g_lists.deferred = item;
  return 0;
}

// Mech3.exe 1.2 @ 0x004f8390. Its class's list, list 7 when on it, its
// action list, then list 6. An unknown class is reported (0x0050da70) and
// the node still leaves the other lists.
int node_unlist(Node* node) {
  switch (node->cls()) {
    case 1: list_remove(kCameras, node); break;
    case 2: list_remove(kWorlds, node); break;
    case 3: list_remove(kWindows, node); break;
    case 4: list_remove(kDisplays, node); break;
    case 5: case 6: case 11: break;
    case 7: list_remove(11, node); break;
    case 8: list_remove(12, node); break;
    case 9: list_remove(kLights, node); break;
    case 10: list_remove(kSounds, node); break;
    default: {
      char msg[96];
      std::snprintf(msg, sizeof msg, "%s: Line %d: Unknown class type while deleting node from lists.\n", kListFile,
                    0x954);
      zerr::report(3, kOldFile, 0x23, msg);
      break;
    }
  }
  if (node->flags() & 1) list_remove(kChanged, node);
  const int32_t priority = node->get<int32_t>(0x44);
  if (node->action && priority >= 0 && priority < 6) list_remove(priority, node);
  list_remove(kAllNodes, node);
  return 0;
}

namespace {
// Each child taken off (not destroyed: it stays on list 6 for its own turn).
int drop_children(Node* node) {
  while (node->child_count() > 0)
    if (int r = node_remove_child(node, node->children[0])) return r;
  return 0;
}
int drop_parents(Node* node) {
  while (node->parent_count() > 0)
    if (int r = node_remove_child(node->parents[0], node)) return r;
  return 0;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004f84b0 (_gwListDeleteANode). 0 when the node is gone, 1
// while something still holds it, else the first error.
int node_destroy(Node* node) {
  if (int r = node_drop_model(node)) return r;
  switch (node->cls()) {
    case 1: case 3: case 4: case 7: case 11:
      if (int r = drop_children(node)) return r;
      return node->parent_count() == 0 ? node_release(node) : 1;
    case 2: {
      auto* w = static_cast<World*>(node->data);
      while (w->get<int32_t>(0xa0) > 0)
        if (int r = world_remove_light(node, w->lights[0])) return r;
      if (w->lights) {
        delete[] w->lights, delete[] w->light_data;
        w->lights = nullptr, w->light_data = nullptr;
      }
      while (w->get<int32_t>(0xac) > 0)
        if (int r = world_remove_sound(node, w->sounds[0])) return r;
      if (w->sounds) {
        delete[] w->sounds, delete[] w->sound_data;
        w->sounds = nullptr, w->sound_data = nullptr;
      }
      if (int r = drop_children(node)) return r;
      for (int32_t z = 0; z < w->get<int32_t>(0x8c); ++z)
        for (int32_t x = 0; x < w->get<int32_t>(0x88); ++x) {
          Partition& p = w->rows[z][x];
          while (p.get<int16_t>(0x3a) > 0)
            if (int r = world_remove_child(node, p.nodes[0])) return r;
        }
      return node->parent_count() == 0 ? world_delete(node) : 1;
    }
    case 5: case 6:
      if (int r = drop_children(node)) return r;
      if (int r = drop_parents(node)) return r;
      return node_release(node);
    case 8:
      if (int r = drop_children(node)) return r;
      return node->parent_count() == 0 ? animate_delete(node) : 1;
    case 9:
    case 10: {
      if (int r = drop_children(node)) return r;
      if (int r = drop_parents(node)) return r;
      const bool light = node->cls() == 9;
      const auto* d = static_cast<const uint8_t*>(node->data);
      int32_t worlds;
      std::memcpy(&worlds, d + (light ? 0xc8 : 0x8c), 4);  // the worlds that list it
      if (worlds > 0) return 1;
      return light ? light_delete(node) : sound_delete(node);
    }
    default: {
      if (immediate_delete()) {
        char msg[120];
        std::snprintf(msg, sizeof msg, "_gwListDeleteANode(): Unrecognized node class type:node = %s ptr = 0x%08x class_type = %d",
                      node->name(), 0u, node->cls());
        zerr::report(0x400, kListFile, 0xaeb, msg);
      }
      return 3;
    }
  }
}

// Mech3.exe 1.2 @ 0x004f8ab0. With immediate deletes off, every node on list
// 6 is destroyed, pass after pass, until a pass destroys none; then the
// lists are emptied and their items freed.
int destroy_all() {
  const int32_t immediate = immediate_delete();
  immediate_delete() = 0;
  purge_lists();
  ListItem* head = g_lists.list[kAllNodes].head;
  for (int destroyed = 1; head && destroyed > 0;) {
    destroyed = 0;
    for (ListItem* item = head; item; item = item->next)
      if (item->node && node_destroy(item->node) == 0) {
        item->node = nullptr;
        ++destroyed;
      }
  }
  reset_lists();
  immediate_delete() = immediate;
  free_items();
  return 0;
}

// Mech3.exe 1.2 @ 0x004f7920
void free_items() { g_lists.chunks.clear(); }

// Mech3.exe 1.2 @ 0x004f8fb0 (0x004f8f40 calls it): every node destroyed,
// the pool's blocks freed, the render's pre/post callbacks back to none and
// the remembered gamez.zbd path cleared.
int classes_reset() {
  destroy_all();
  NodePool& p = nodes();
  if (p.capacity > 0) {
    p.blocks.clear();
    p.capacity = 0, p.live = 0, p.free_head = -1;
  }
  scene_walk().pre = nullptr, scene_walk().post = nullptr;  // 0x004f7630(0), 0x004f7650(0)
  zbd_path()[0] = 0;                                        // 0x004fe0f0: the first byte
  return 0;  // [0x0060bf48] (0x004f8f70's periodic callback flag) has no port
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
