#include "gamez/zclass/display.h"

#include "gamez/zclass/cls_data.h"
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

}  // namespace zcls
