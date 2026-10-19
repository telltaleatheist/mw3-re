#include "gamez/zclass/display.h"

#include "gamez/zclass/cls_data.h"
#include "gamez/zvideo/zvid_format.h"

namespace zcls {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zClass\\Display.c";
}  // namespace

// Mech3.exe 1.2 @ 0x004fa680
int display_set_clear_color(Node* node, float r, float g, float b) {
  if (int ret = check_class(node, 4, kFile, 0x133)) return ret;
  auto* d = static_cast<Display*>(node->data);
  d->put(0x10, r), d->put(0x14, g), d->put(0x18, b);
  zvid::set_clear_color(zvid::pack_rgb(d->floats(0x10)));
  return 0;
}

}  // namespace zcls
