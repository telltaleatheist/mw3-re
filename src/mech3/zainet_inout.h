#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace zcls { struct Node; }
namespace zmod { struct Polygon; }

// mech3\ZAiNetMechInOut.cpp (0x004c11f0..0x004c3ed0): the mission boundary
// (docs/ai.md section 11). At mission load the world's `inout` polygons
// become areas, one per mission of the operation (or one for Instant Action
// and training), and the `inout` subtree is deleted; in play the AI's
// destinations are pulled back inside the current one (the query, item B2).
namespace mech3::ai {

// A polygon's corner (0x1c bytes): its place, the place pushed out by the
// polygon's width along the corner's bisector (y 0), and the length of the
// side to the next corner.
struct InOutVertex {
  float pos[3] = {};     // +0x00
  float offset[3] = {};  // +0x0c
  float length = 0;      // +0x18
};

// 0x004c1ab0 (0x40 bytes, vtable 0x005cac6c): one boundary polygon, classed
// by its material's colour. Red (kind 0) and blue (2) are tested by the query,
// yellow (1) and cyan (3) are not; blue and cyan are wound the other way and
// count crossings the other way round.
struct InOutPolygon {
  // ECX this; name, the model polygon, its vertices transformed, the width.
  InOutPolygon(const char* name, const zmod::Polygon& poly, const float* verts, float width);
  ~InOutPolygon();  // 0x004c1ff0

  std::vector<InOutVertex> verts;  // +0x04
  float perimeter = 0;             // +0x14
  float width = 0;                 // +0x18  3.0 (0x005e8a18)
  float min[3], max[3];            // +0x1c, +0x28  FLT_MAX, -FLT_MAX before the corners
  uint8_t inverted = 0;            // +0x34  blue, cyan
  uint8_t tested = 0;              // +0x35  red, blue
  int32_t kind = 0;                // +0x38  0 red, 1 yellow, 2 blue, 3 cyan (0 for any other colour)
  char* name = nullptr;            // +0x3c  strdup'd: the node's name past "inoutXX"
};

// One mission's boundary (0x88 bytes, vtable 0x005cac70, ctor 0x004c26b0):
// its polygons, then the query's state (item B2: 0x13 dwords copied from the
// template at 0x0060b348, whose +0x20 is FLT_MAX), a box (FLT_MAX /
// -FLT_MAX) and the last answer.
struct InOutArea {
  InOutArea();
  std::vector<std::unique_ptr<InOutPolygon>> polygons;  // +0x04
  uint32_t query[0x13] = {};                            // +0x14..+0x60
  float min[3], max[3];                                 // +0x60, +0x6c
  uint8_t x78 = 0, x79 = 0;                             // +0x78, +0x79  (the copy's leftover bytes)
  float x7c[3] = {};                                    // +0x7c
};

// The variant of the `inout` node a game reads (0x004c36c0, 0x20 bytes on
// the stack): campaign "inoutm" + the mission number, one area per mission of
// the operation; Instant Action "inoutia" and training "inoutt", one area.
// Multiplayer has none.
struct InOutVariant {
  int32_t operation = 0;  // +0x00  vt+0x48
  int32_t index = 0;      // +0x04  the mission number - 1 (campaign), else 0
  int32_t count = 0;      // +0x08  the operation's missions (0x00484840), else 1
  int32_t game = -1;      // +0x0c  vt+0x2c
  int32_t prefix = 0;     // +0x10  the name's length
  uint8_t numbered = 0;   // +0x14
  uint8_t valid = 0;      // +0x15
  char name[9] = {};      // +0x16
  int32_t match(const char* child) const;  // 0x004c3850: its area, -1 for none
};
InOutVariant inout_variant();  // 0x004c36c0

// What the boundary needs from modules not ported here; the program sets them.
struct InOutHooks {
  bool (*have_game)() = nullptr;                 // [0x0060861c] set
  int32_t (*operation)() = nullptr;              // [0x0060861c]+0x6c vt+0x48
  int32_t (*game_type)() = nullptr;              // vt+0x2c
  int32_t (*mission_number)() = nullptr;         // vt+0x4c
  // [0x00608c84]: with a world, each corner is put on the ground under it
  // (0x004c4240: 0x0041eb90 from 100 above, 0x004abd90 within 20). None: kept.
  void (*ground)(float pos[3]) = nullptr;
};
InOutHooks& inout_hooks();

// The areas read at the mission load ([0x0060b338]+4 = [0x0060b33c], a
// vector of 0x88-byte areas) and the current mission's ([0x0060b344]).
struct InOut {
  std::unique_ptr<std::vector<InOutArea>> areas;  // [0x0060b33c]
  InOutArea* current = nullptr;                   // [0x0060b344]
};
InOut& inout();

// 0x004c3da0 (the listener's slot +0x08) -> 0x004c3930: the world's `inout`
// read into the areas (none without the node or a variant), then the node
// taken off its parents and deleted with its subtree.
void inout_mission_load();
void inout_select();       // 0x004c3b80 (0x00490bde): the current mission's area
void inout_mission_end();  // 0x004c3dd0 (the listener's slot +0x10): the areas freed

// 0x004c3e50 with 0x004c3600 -> 0x004c3380: a node's subtree walked through
// the matrix stack (0x004c3ed0), each model's polygons into the area.
void inout_read_tree(zcls::Node* node, InOutArea& area);

}  // namespace mech3::ai
