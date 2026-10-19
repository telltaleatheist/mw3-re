#include "gamez/zclass/window.h"

#include "gamez/zclass/cls_data.h"

namespace zcls {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zClass\\Window.c";
}  // namespace

// Mech3.exe 1.2 @ 0x004fa020
int window_size(Node* node, int32_t* width, int32_t* height) {
  if (int r = check_class(node, 3, kFile, 0xe7)) return r;
  const auto* d = static_cast<const Window*>(node->data);
  *width = d->get<int32_t>(8);
  *height = d->get<int32_t>(0xc);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fa130
int window_origin(Node* node, int32_t* x, int32_t* y) {
  if (int r = check_class(node, 3, kFile, 0x11d)) return r;
  const auto* d = static_cast<const Window*>(node->data);
  *x = d->get<int32_t>(0);
  *y = d->get<int32_t>(4);
  return 0;
}

}  // namespace zcls
