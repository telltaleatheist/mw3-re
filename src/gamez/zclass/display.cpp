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

// Mech3.exe 1.2 @ 0x004fa4c0: a display node, 1 x 1 at the origin, its
// clear colour (0.392, 0.392, 1.0); none when the pool is full.
Node* display_create() {
  Node* node = node_alloc();
  if (!node) return zerr::report(0x400, kFile, 0x41, "Null node pointer."), nullptr;
  node->put<int32_t>(0x34, 4);
  auto* d = new Display{};
  std::memset(d->raw, 0, sizeof d->raw);
  node->data = d;
  d->put<int32_t>(8, 1), d->put<int32_t>(0xc, 1);
  d->put<uint32_t>(0x10, 0x3ec8b439u), d->put<uint32_t>(0x14, 0x3ec8b439u), d->put<uint32_t>(0x18, 0x3f800000u);
  push_front(kDisplays, node);  // 0x004f8080 (it never fails)
  return node;
}

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

}  // namespace zcls
