#pragma once
#include <cstdint>

namespace zcls { struct Node; }
namespace zmod { struct Material; }
namespace pm::zmod { struct Model; struct PolygonAdd; }

// The mission boundary drawn (the vehicle editor's Show InOut; docs/editor.md,
// The vehicle editor's pages). Original file unknown: it sits between
// zainetmechinout.cpp and zaitarget.cpp (0x004c41b0..0x004c5530). The
// current mission's area (zainet_inout.h) becomes walls 10 m high along each
// tested polygon: red on one side, blue (red and blue polygons) or yellow
// (yellow and cyan, never tested, so never drawn) on the other, in models of
// up to 1024 vertices under one object3d in the world.
namespace mech3::ai {

struct InOutPolygon;
struct InOutVertex;

// Pirate's Moon 1.0 @ 0x004ce970 (0x44 bytes on the stack; MW3's 0x004c42e0,
// 0x40): one model's walls as they are made, PM's model (gmod_const PM's).
struct InOutWalls {
  explicit InOutWalls(pm::zmod::Model* m);  // 0x004ce970
  bool add(const InOutPolygon& p);     // 0x004cea50: true when the model is too full for it
  void corner(const InOutVertex& v);   // 0x004cedd0
  void triangle(zmod::Material* m, bool flag);  // 0x004cecc0 / 0x004ced50

  int32_t triangles = 0;             // +0x00
  uint32_t vertices = 0;             // +0x04
  pm::zmod::Model* model = nullptr;  // +0x08
  zmod::Material* side = nullptr;    // +0x0c  the polygon's colour (blue at first)
  zmod::Material* front = nullptr;   // +0x10  red
  uint8_t zone[4] = {};              // +0x14
  float pts[9] = {};                 // +0x18  the triangle: a, b, c
  int32_t last = 0;                  // +0x3c  the last add's answer
  uint8_t outer = 0;                 // +0x40  [0x0060b8d0], 1: along the corners' offset (+0x00), else their place
};

// Tests: the walls' calls out, 0x005275d0, 0x0055cb70 and 0x0055c470 (null: those).
struct InOutWallsCalls {
  zmod::Material* (*color)(float r, float g, float b) = nullptr;
  int (*add)(pm::zmod::Model* m, int32_t n, const pm::zmod::PolygonAdd& a) = nullptr;
  void (*sort)(pm::zmod::Model* m) = nullptr;
};
InOutWallsCalls& inout_walls_calls();

// [0x0060b334]: the walls' object3d, null when nothing is drawn (the exe
// keeps an 8-byte holder, vtable 0x005bd2f4, with the node at +4).
zcls::Node*& inout_view();
bool inout_view_empty();                // 0x004c4a20 -> 0x004c4920(0)
// 0x004c4a40 -> 0x004c4950: drawn for a name (which nothing reads), taken
// down for null.
void inout_view_show(const char* name);
void inout_view_build();                // 0x004c4730
void inout_view_free();                 // 0x004c41b0

}  // namespace mech3::ai
