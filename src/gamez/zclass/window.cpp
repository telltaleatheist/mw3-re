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

// The script engine's window commands (docs/interp.md, The commands the shipped scripts never use): ported for
// preservation, the engine's entries left null. The clear polygons: data +0xe0 the polygon being added (bit 31 the
// polygons on), up to four of 0x34 bytes from +0x10, each four vertices then their count (+0x30, bit 31 set).

// Mech3.exe 1.2 @ 0x004fa1c0, Pirate's Moon 1.0 @ 0x0050ded0: +0xe4.
int window_set_buffer(Node* node, int32_t buffer) {
  if (int r = check_class(node, 3, kFile, 0x137)) return r;
  static_cast<Window*>(node->data)->put<int32_t>(0xe4, buffer);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fa230, Pirate's Moon 1.0 @ 0x0050df50: +0xe0 bit 31, on only for 1.
int window_set_clear_polygon(Node* node, int32_t on) {
  if (int r = check_class(node, 3, kFile, 0x150)) return r;
  auto* d = static_cast<Window*>(node->data);
  const uint32_t v = d->get<uint32_t>(0xe0);
  d->put<uint32_t>(0xe0, on == 1 ? v | 0x80000000u : v & 0x7fffffffu);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fa2c0, Pirate's Moon 1.0 @ 0x0050dff0: a vertex onto the polygon being added, its height 100; 1 (reported) with four
// polygons or four vertices.
int window_add_clear_vertex(Node* node, const float* xyz) {
  if (int r = check_class(node, 3, kFile, 0x170)) return r;
  auto* d = static_cast<Window*>(node->data);
  const uint32_t poly = d->get<uint32_t>(0xe0) & 0x7fffffffu;
  if (poly == 4) return zerr::report(0x400, kFile, 0x178, "ERROR adding window clear polygon vertex.  Clear polygon buffer is full."), 1;
  const int base = 0x10 + 0x34 * static_cast<int>(poly);
  const uint32_t count = d->get<uint32_t>(base + 0x30);
  const uint32_t v = count & 0x7fffffffu;
  if (v == 4) {
    return zerr::report(0x400, kFile, 0x182,
                        "ERROR adding window clear polygon vertex.  Clear polygon vertex buffer is full."), 1;
  }
  const uint32_t marked = count | 0x80000000u;
  d->put(base + 0xc * static_cast<int>(v), xyz[0]);
  d->put(base + 0xc * static_cast<int>(v) + 4, xyz[1]);
  d->put(base + 0xc * static_cast<int>(v) + 8, 100.0f);
  d->put<uint32_t>(base + 0x30, ((marked + 1) & 0x7fffffffu) | (marked & 0x80000000u));
  return 0;
}

// Mech3.exe 1.2 @ 0x004fa3e0, Pirate's Moon 1.0 @ 0x0050e110: the polygon closed: the next one is added, bit 31 set; the result the closed one's
// index (1, reported, with four). The exe also hands the polygon to the software renderer's clear list
// (0x00564660), not ported (CLAUDE.md, Rendering).
int window_close_clear_polygon(Node* node) {
  if (int r = check_class(node, 3, kFile, 0x1a7)) return r;
  auto* d = static_cast<Window*>(node->data);
  const uint32_t poly = d->get<uint32_t>(0xe0) & 0x7fffffffu;
  if (poly == 4) return zerr::report(0x400, kFile, 0x1af, "ERROR closing window clear polygon.  Clear polygon buffer is full."), 1;
  d->put<uint32_t>(0xe0, (d->get<uint32_t>(0xe0) + 1) | 0x80000000u);
  return static_cast<int>(poly);
}

}  // namespace zcls
