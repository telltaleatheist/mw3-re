#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "gamez/zclass/cls_node.h"

// zClass\list.c: the 16 node lists and their item pool (docs/zclass-world.md).
namespace zcls {

struct ListItem {                  // 0x10 bytes
  Node* node;
  ListItem* prev;                  // +0x04
  ListItem* next;                  // +0x08
  int32_t removable;               // +0x0c
};

struct List {                      // 12 bytes
  ListItem* head;
  ListItem* tail;
  int32_t needs_purge;
};

enum ListId { kAllNodes = 6, kChanged = 7, kCameras = 8, kLights = 9, kSounds = 10,
              kWorlds = 13, kWindows = 14, kDisplays = 15 };

struct Lists {
  List list[16] = {};              // 0x0060be4c.., reached through 0x005ea6d0 / 0x005ea710
  std::vector<std::unique_ptr<ListItem[]>> chunks;  // 0x0060bf18, count 0x0060bf14
  int32_t chunk_size = 2500;       // 0x005ea750
  ListItem* free = nullptr;        // 0x0060bf0c
  ListItem* deferred = nullptr;    // 0x0060bf10
  int32_t enabled = 1;             // 0x005ea6b0
};
Lists& lists();

ListItem* item_alloc();            // 0x004f77d0
void item_free(ListItem* item);    // 0x004f78e0
void reset_lists();                // 0x004f89a0
int list_count(int list);          // 0x004f7f80
int push_front(int list, Node* node);  // 0x004f8080
int push_back(int list, Node* node);   // 0x004f8140
ListItem* list_head(int list);         // 0x004f8040
Node* list_find_by_name(int list, const char* name);  // 0x004f7fe0

// A walk over one list that hands out the nodes a test accepts: start it with
// a key and a list, then call it with no key for each next match (0 at the end).
struct ListWalk {
  ListItem* cursor = nullptr;      // 0x0060be40
  const void* key = nullptr;       // 0x0060be44
};
// Removal is lazy: the item loses its node and is marked removable, and the
// list is flagged for the purge (docs/zclass-world.md section 2). 1 when the
// list is empty, else 0.
int list_remove(int list, Node* node);     // 0x004f8220 ECX list, EDX node
int list_purge(int list);                  // 0x004f7980
int purge_lists();                         // 0x004f7bd0, then the deferred deletes (0x004f81e0)
int deferred_push(Node* node);             // 0x004f8050: freed (0x004eaf90) at the next purge
int node_unlist(Node* node);               // 0x004f8390: off every list it is on
// A node and its ties (docs/zclass-world.md, Destroy): its model let go (and
// freed when unused), its children and parents unlinked, its class's own
// delete. 0 gone, 1 still held (parents, a world listing it), else an error.
int node_destroy(Node* node);              // 0x004f84b0
int destroy_all();                         // 0x004f8ab0: every node, the lists and their items
void free_items();                         // 0x004f7920
// zClass's reset between missions and at shutdown (0x004f8f40 calls it):
// every node destroyed and the pool's blocks freed.
int classes_reset();                       // 0x004f8fb0
// The frame's actions: each live item whose node is active (flag 4) and has
// an action (+0x48) runs it, with purging held off; items without a node or
// an action are marked removable. Then every flagged list is purged.
int run_actions(ListItem* head);           // 0x004f7d20
void run_action_lists();                   // 0x004f7d80: lists 0..5, each followed by 0x004f7f70
// The frame update's pieces (class.h has 0x004f7e00, 0x004f7e50, 0x004f7f70).
int update_subtree(Node* node);            // 0x004f7db0
int sequences_update();                    // 0x004f7ec0  list 11
int animates_update();                     // 0x004f7f10  list 12

ListWalk& list_walk();
Node* list_walk_next(const void* key, int list, bool (*test)(Node* node));  // 0x004f8b20
Node* list_walk_by_name(const char* name, int list);                       // 0x004f8bd0

}  // namespace zcls
