#include "mech3/mech_reader.h"

#include "gamez/zreader/zreader.h"
#include "mech3/mech_names.h"

namespace mech3 {

// Mech3.exe 1.2 @ 0x0046b890. The defaults every mech starts from.
MechDfn::MechDfn(int32_t i, zrdr::Node* tree) : index(i), reader(tree) {
  put<int32_t>(0x10, 10), put<int32_t>(0x14, 400);
  static const struct { int off; uint32_t bits; } kDefaults[] = {
      {0x148, 0x3f800000}, {0x14c, 0x42be0000}, {0x150, 0x42aa0000}, {0x154, 0x3edf66f3},
      {0x158, 0x3f800000}, {0x15c, 0x3f800000}, {0x160, 0x3efa35dd}, {0x164, 0x3edf66f3},
      {0x168, 0x3e61307a}, {0x16c, 0x41600000}, {0x170, 0x4039999a}, {0x180, 0x3f000000},
      {0x188, 0x3eb2b8c2}, {0x18c, 0x40400000}, {0x190, 0xbfb2b8c2}, {0x194, 0x3f9c61aa},
      {0x198, 0xbfb2b8c2}, {0x19c, 0x3fb2b8c2}, {0x1a0, 0x3e99999a}};
  for (const auto& d : kDefaults) put(d.off, d.bits);
}

// Mech3.exe 1.2 @ 0x0046ba20. The node is never read.
bool PartSlot::set(int32_t part, zrdr::Node*) {
  name = part_name(part);
  id = part;
  return true;
}

// The list is always made, empty when the file has no firepoints. Missing
// radius or share read as 0.
void read_firepoints(MechDfn& dfn) {
  zrdr::Node* found = zrdr::find(dfn.reader, "firepoints");
  dfn.firepoints = new std::vector<Firepoint*>();
  if (!found) return;
  zrdr::Node* list = found->list;
  const int32_t pairs = (list[0].i - 1) / 2;
  if (pairs > 0) dfn.firepoints->reserve(pairs);
  for (int32_t i = 0; i < pairs; ++i) {
    zrdr::Node* body = &list[2 + 2 * i];
    float radius = 0, share = 0;
    zrdr::get_float(body, "radius", &radius);
    zrdr::get_float(body, "share", &share);
    const int32_t part = part_id(zrdr::get_string(body, "part"));
    dfn.firepoints->push_back(new Firepoint{list[1 + 2 * i].s, radius, share, part});
  }
}

// The file's "parts" (no shipped file has one) is looked up and ignored: every
// mech gets the same 13 slots, and the side torsos (7, 8) none.
void read_part_slots(MechDfn& dfn) {
  zrdr::Node* node = zrdr::find(dfn.reader, "parts");
  for (int32_t id : {15, 1, 2, 3, 9, 10, 4, 5, 11, 12, 13, 14, 6}) dfn.parts[id].set(id, node);
}

}  // namespace mech3
