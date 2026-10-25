#include "gamez/zclass/window.h"

#include <cstring>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"

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

// Mech3.exe 1.2 @ 0x004f9e90. +0xe4 -1; +0xe8..+0xf4 the software
// renderer's target (0x00558080) are left 0: it is not ported (CLAUDE.md,
// Rendering). The exe also prints the new window.
Node* window_create() {
  Node* node = node_alloc();
  if (!node) return zerr::report(0x400, kFile, 0x61, "Null node pointer."), nullptr;
  node->put<int32_t>(0x34, 3);
  auto* w = new Window{};
  std::memset(w->raw, 0, sizeof w->raw);
  node->data = w;
  w->put<int32_t>(8, 1), w->put<int32_t>(0xc, 1);
  w->put<int32_t>(0xe4, -1);
  push_front(kWindows, node);
  return node;
}

// Mech3.exe 1.2 @ 0x004f9fa0
int window_set_size(Node* node, int32_t width, int32_t height) {
  if (int r = check_class(node, 3, kFile, 0xcd)) return r;
  auto* d = static_cast<Window*>(node->data);
  d->put<int32_t>(8, width), d->put<int32_t>(0xc, height);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fa0b0
int window_set_origin(Node* node, int32_t x, int32_t y) {
  if (int r = check_class(node, 3, kFile, 0x102)) return r;
  auto* d = static_cast<Window*>(node->data);
  d->put<int32_t>(0, x), d->put<int32_t>(4, y);
  return 0;
}

}  // namespace zcls
