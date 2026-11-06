#pragma once
#include <cfloat>
#include <cstdint>
#include <memory>
#include <vector>

namespace zcls { struct Node; }
namespace pm::zmod { struct Polygon; }

// mech3\ZAiNetMechInOut.cpp (0x004c11f0..0x004c3ed0): the mission boundary
// (docs/ai.md section 11). At mission load the world's `inout` polygons
// become areas, one per mission of the operation (or one for Instant Action
// and training), and the `inout` subtree is deleted; in play the AI's
// destinations are kept out of the current area's tested polygons (0x004c3000).
namespace mech3::ai {

struct InOutArea;
struct InOutQuery;

// A way through the boundary (PM's, 0x10 bytes, 0x004c7b20): a net's via node and the node it leads to,
// with the corner nearest each. Listed on the polygon and on each corner within 100 of the node's side.
struct InOutVertex;
struct InOutVia {
  const void* net = nullptr;            // +0x00
  const void* node = nullptr;           // +0x04
  const float* pos = nullptr;           // the node's vt+0x1c
  const float* other_pos = nullptr;     // its +0x30 node's vt+0x1c
  const void* other = nullptr;          // its +0x30
  const InOutVertex* other_corner = nullptr;  // +0x08
  const InOutVertex* corner = nullptr;        // +0x0c
};

// Pirate's Moon 1.0: a polygon's corner (0x54 bytes; MW3's was 0x1c): the place pushed out by
// the polygon's width, the place, half a unit further out, its side's length and how far along
// the outline it starts, the side's normal and direction.
struct InOutVertex {
  float offset[3] = {};  // +0x00
  float pos[3] = {};     // +0x0c
  float inner[3] = {};   // +0x18
  float length = 0;      // +0x24
  float along = 0;       // +0x28
  float normal[3] = {};  // +0x2c
  float dir[3] = {};     // +0x38
  std::vector<InOutVia> vias;  // +0x48
};

// Pirate's Moon 1.0 @ 0x004c9af0 (0x64 bytes): one boundary polygon, classed by its material's
// colour. Red (kind 0) and blue (2) are tested by the query, yellow (1) and cyan (3) are not;
// blue and cyan are wound the other way and count crossings the other way round.
struct InOutPolygon {
  // ECX this; name, the model polygon, its vertices transformed, the width.
  InOutPolygon(const char* name, const pm::zmod::Polygon& poly, const float* verts, float width);
  ~InOutPolygon();
  void cross(InOutArea& area) const;  // 0x004ca690: when tested, its sides against the query
  bool blocks(const float* from, const float* p) const;  // 0x004cb2d0: a side crosses from -> p
  void add_via(const InOutVia& via);  // 0x004cf350
  // 0x004caef0 / 0x004cb0e0: the side nearest `from`, of all or of those within `limit` behind `ref`.
  struct Near {
    const InOutVertex* corner = nullptr;
    float along = 0, dist = FLT_MAX, perp = FLT_MAX;
  };
  Near nearest(const float* from, const InOutVertex* ref = nullptr, float limit = 0) const;
  const InOutVertex* next(const InOutVertex* v) const;

  std::vector<InOutVertex> verts;  // +0x08
  float perimeter = 0;             // +0x14
  float width = 0;                 // +0x18  3.0, 12.0 for a long outline
  float min[3], max[3];            // +0x1c, +0x28  the pushed-out outline's box
  float far_limit = 15000.0f;      // +0x34  3500 or 10000 past 20,000 or 35,000 round
  float near_limit = 1000.0f;      // +0x38  500
  float x3c = 50.0f;               // +0x3c  10; not read in play
  float in_limit = 2.0f;           // +0x40  4
  uint8_t x44 = 0;                 // +0x44  1; not read in play
  uint8_t inverted = 0;            // +0x45  blue, cyan
  uint8_t tested = 0;              // +0x46  red, blue
  int32_t kind = 0;                // +0x48  0 red, 1 yellow, 2 blue, 3 cyan (0 for any other colour)
  char* name = nullptr;            // +0x4c  strdup'd: the node's name past "inoutXX"
  std::vector<InOutVia> vias;      // +0x58
};

// A query's state (0x7c bytes; its fresh copy is the template at 0x0062f9c8, set at start-up by
// 0x004c8140): the segment from -> to against the polygons' sides. A polygon's own pass
// (0x004ca690) fills one, the area keeps the nearest crossing's.
struct InOutQuery {
  const float* from = nullptr;                       // +0x00
  const float* to = nullptr;                         // +0x04
  InOutPolygon* poly = nullptr;                      // +0x08  the nearest crossing's polygon
  const InOutVertex *near_a = nullptr, *near_b = nullptr;  // +0x0c, +0x10  the side crossed first
  const InOutVertex *far_a = nullptr, *far_b = nullptr;    // +0x14, +0x18  the side crossed last
  float length = 0;                                  // +0x1c  from -> to in x and z
  float t_near = FLT_MAX;                            // +0x20
  float t_far = 0;                                   // +0x24
  float near_point[3] = {};                          // +0x28
  float far_point[3] = {};                           // +0x34
  int32_t crossings = 0;                             // +0x40
  int32_t from_rays = 0;                             // +0x44  sides the ray from `from` through `to` meets
  int32_t to_rays = 0;                               // +0x48  sides the ray from `to` back through `from` meets
  const InOutVia* via = nullptr;                     // +0x4c
  float arc = FLT_MAX;                               // +0x50  the way round from the near side to the far one
  const InOutVertex* nearest = nullptr;              // +0x54  the side nearest `from`
  float dist = FLT_MAX, along = FLT_MAX, perp = FLT_MAX;  // +0x58, +0x5c, +0x60  and how far
  float near_along = FLT_MAX, far_along = FLT_MAX;   // +0x64, +0x68
  float back = 0;                                    // +0x6c  0.1 / length: how far short of a side
  float scratch[3] = {};                             // +0x70  a side's middle, when that's the answer
};

// One mission's boundary (0xc0 bytes, ctor 0x004cb370): its polygons, the query's state, a box
// (FLT_MAX / -FLT_MAX, never set) and the last answer.
struct InOutArea {
  InOutArea();
  std::vector<std::unique_ptr<InOutPolygon>> polygons;  // +0x08
  InOutQuery q;                                         // +0x14
  float min[3], max[3];                                 // +0x90, +0x9c
  uint8_t from_in = 0, to_in = 0;  // +0xa8, +0xa9  inside the nearest polygon (outside, for blue and cyan)
  int32_t outcome = 0;             // +0xbc  which way the answer was reached
  // Pirate's Moon 1.0 @ 0x004cc7f0: where the way from -> to may go: `to` itself or a point in this
  // area. `first` is the polygon this vehicle met last, tried before the others; `ways` lets the
  // via nodes count (a mech with jump jets).
  const float* query(const float* from, const float* to, InOutPolygon* first = nullptr, bool ways = false);
  const float* resolve(bool ways);       // 0x004cbbb0
  const float* way_through(bool ways, const InOutVertex* corner);  // 0x004cba50
  const float* round_to(const InOutVertex* from_side, const InOutVertex* side);  // 0x004cb8b0
  const InOutVertex* shorter_way();      // 0x004cca00
  bool clear(const float* from, const float* p) const;  // 0x004cc970: no polygon's side crosses
};

// The variant of the `inout` node a game reads (PM's 0x004cdb30, 0x1c bytes on
// the stack): campaign "inoutm" and standalone "inoutsa" + the mission number,
// of ten areas; Instant Action "inoutia" and training "inoutt", one area.
// Multiplayer has none.
struct InOutVariant {
  int32_t index = 0;      // +0x00  the mission number - 1 (campaign, standalone), else 0
  int32_t count = 0;      // +0x04  ten (0x00435db0), else 1
  int32_t game = -1;      // +0x08  vt+0x28
  int32_t prefix = 0;     // +0x0c  the name's length
  uint8_t numbered = 0;   // +0x10
  uint8_t valid = 0;      // +0x11
  char name[9] = {};      // +0x12
  int32_t match(const char* child) const;  // PM's 0x004cdce0: its area, -1 for none
};
InOutVariant inout_variant();  // PM's 0x004cdb30

// What the boundary needs from modules not ported here; the program sets them.
struct InOutHooks {
  bool (*have_game)() = nullptr;                 // [0x0060861c] set
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

// Pirate's Moon 1.0 @ 0x004ccaf0: with a current area, `in` as `vehicle` at `pos` may reach it;
// true with `out` the place it is pulled to (at `in`'s height). `ways`: its mech has jump jets
// (+0x3e8). The exe's record of each query on the vehicle (0x54 bytes at +0xd8) is kept only for
// what's read: the polygon it met last, which the next query tries first, and the via it chose,
// which the behaviour 0x004e0790 reads; the rest have no reader found yet (docs/pm-map.md).
struct InOutLast {
  InOutPolygon* poly = nullptr;  // +0x3c
  const InOutVia* via = nullptr;  // +0x4c
  bool at_via = false;            // +0x50  the answer is the via's own place
};
bool inout_pull(uintptr_t vehicle, const float pos[3], const float in[3], float out[3], bool ways);
InOutLast inout_last(uintptr_t vehicle);
// 0x004c7b20 over every net: a via node and the node it leads to, onto the current area's polygons.
void inout_add_via(const void* net, const void* node, const float* pos, const void* other, const float* other_pos);
// Mech3.exe 1.2 @ 0x004c3bd0 (ECX the animation, EDX the AI message; from
// 0x004d4a20): messages 0x39..0x3c test, of the current area's polygons named
// as the animation's node, only the yellow, red, cyan or blue ones.
void inout_set_state(const char* name, int32_t message);
int32_t inout_changes();  // ours (W58): counts the messages that switch shapes, and new areas

// 0x004c3e50 with 0x004c3600 -> 0x004c3380: a node's subtree walked through
// the matrix stack (0x004c3ed0), each model's polygons into the area.
void inout_read_tree(zcls::Node* node, InOutArea& area);

}  // namespace mech3::ai
