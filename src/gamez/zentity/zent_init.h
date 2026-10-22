#pragma once

// zEntity's node set-up (zentity\zent_init.cpp): of 0x005a8390, the two
// marker nodes. An entity's centroid and target (what others aim at) are
// nodes of its tree found by name, else object3ds of that name made under
// its node (docs/mech-build.md, section 2).
namespace zcls {
struct Node;
}

namespace zent {

// Mech3.exe 1.2 @ 0x005a8390 (the nodes): `centroid` and `target` null take
// "centroid" (0x005e97a8) and "target" (0x005e1710). A node that cannot be
// made or added is none (its report not kept).
void marker_nodes(zcls::Node* node, const char* centroid, const char* target, zcls::Node** centroid_out,
                  zcls::Node** target_out);

}  // namespace zent
