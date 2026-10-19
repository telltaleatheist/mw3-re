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

}  // namespace zcls
