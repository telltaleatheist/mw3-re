#pragma once
#include <cstdint>
#include <cstdio>

#include "gamez/zclass/cls_node.h"

// zClass\cls_zbd.c: the node part of gamez.zbd (docs/gamez.md, section 4).
namespace zcls {

int read_node_list(Node** out, int32_t count, std::FILE* f);  // 0x004fea80
int read_node(Node* node, std::FILE* f);                      // 0x004feaf0
int read_nodes(int32_t size, std::FILE* f);                   // 0x004ff200

}  // namespace zcls
