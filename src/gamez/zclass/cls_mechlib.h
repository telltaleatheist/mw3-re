#pragma once
#include <cstdint>
#include <cstdio>

// The model library reader for mechlib.zbd (docs/mechlib.md). Original file
// unknown: it sits between zClass\object3d.c and zClass\switch.c.
namespace zmod {
struct Model;
}

namespace zcls {

struct Material {                // 0xc bytes
  uint8_t* raw;                  // +0x00  the 0x28 file bytes
  bool resolved;                 // +0x04
  char* texture;                 // +0x08
};

struct Node;

struct TreeReader {
  int32_t depth = 0;             // +0x3c of the library object
};

bool read_material(std::FILE* f, Material& m);   // 0x004fc570
Node* read_node(std::FILE* f, Node* parent);     // 0x004fce50
Node* read_tree(TreeReader& lib, std::FILE* f, Node* parent);  // 0x004fcdc0
// When node +0x3c != 0: a model from the world pool (0x00538fe0), its
// polygons' materials left as mechlib's indices (Polygon::material_index).
zmod::Model* read_model(std::FILE* f);           // 0x004fcf20

}  // namespace zcls
