#include "gamez/zclass/list.h"

#include <new>

namespace zcls {

namespace {
Lists g_lists;

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

}  // namespace zcls
