#include "gamez/zclass/display.h"

#include <cstring>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zvideo/zvid_format.h"

namespace zcls {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zClass\\Display.c";
}  // namespace

// Mech3.exe 1.2 @ 0x004fa580
int display_set_size(Node* node, int32_t width, int32_t height) {
  if (int ret = check_class(node, 4, kFile, 0xb0)) return ret;
  auto* d = static_cast<Display*>(node->data);
  d->put(0x08, width), d->put(0x0c, height);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fa600
int display_set_origin(Node* node, int32_t x, int32_t y) {
  if (int ret = check_class(node, 4, kFile, 0xee)) return ret;
  auto* d = static_cast<Display*>(node->data);
  d->put(0x00, x), d->put(0x04, y);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fa680
int display_set_clear_color(Node* node, float r, float g, float b) {
  if (int ret = check_class(node, 4, kFile, 0x133)) return ret;
  auto* d = static_cast<Display*>(node->data);
  d->put(0x10, r), d->put(0x14, g), d->put(0x18, b);
  zvid::set_clear_color(zvid::pack_rgb(d->floats(0x10)));
  return 0;
}

// Mech3.exe 1.2 @ 0x004fa4c0, Pirate's Moon 1.0 @ 0x0050e1f0: a display node (class 4), 1 x 1, clear colour (0.392, 0.392, 1), on list 15 (the
// script command NewDisplay; on pm wired for the lab's world.gw). Refused by the list: deleted.
Node* display_create() {
  Node* node = node_alloc();
  if (!node) return zerr::report(0x400, kFile, 0x41, "Null node pointer."), nullptr;
  node->put<int32_t>(0x34, 4);
  auto* d = new Display{};
  std::memset(d->raw, 0, sizeof d->raw);
  node->data = d;
  d->put<int32_t>(0x08, 1), d->put<int32_t>(0x0c, 1);
  d->put<uint32_t>(0x10, 0x3ec8b439u), d->put<uint32_t>(0x14, 0x3ec8b439u), d->put(0x18, 1.0f);
  if (push_front(kDisplays, node) != 0) {
    node_delete(node);
    return nullptr;
  }
  return node;
}

}  // namespace zcls
