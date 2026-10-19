#pragma once
#include <cstdint>
#include <cstring>

#include "gamez/zclass/cls_node.h"

// The per-class node data as gamez.zbd stores it (docs/gamez.md, section 4).
// Each keeps its bytes as read; the fields after `raw` replace the pointers
// the exe writes over them. Each class's own .c file is the original home.
namespace zcls {

template <int N>
struct Raw {
  uint8_t raw[N];
  template <typename T> T get(int off) const {
    T v;
    std::memcpy(&v, raw + off, sizeof v);
    return v;
  }
  template <typename T> void put(int off, T v) { std::memcpy(raw + off, &v, sizeof v); }
  float* floats(int off) { return reinterpret_cast<float*>(raw + off); }
};

struct Camera : Raw<0x1e8> {
  Node* nodes[4] = {};             // +0x00..+0x0c  indices in the file
};

struct Partition : Raw<0x48> {
  Node** nodes = nullptr;          // +0x3c  count i16 at +0x3a
};

struct World : Raw<0xbc> {
  uint32_t** area_pending = nullptr;  // +0x0c  count +0x04
  Partition** rows = nullptr;      // +0x90  +0x8c rows of +0x88
  Node** lights = nullptr;         // +0xa4  count +0xa0
  void** light_data = nullptr;     // +0xa8
  Node** sounds = nullptr;         // +0xb0  count +0xac
  void** sound_data = nullptr;     // +0xb4
};

struct Window : Raw<0xf8> {};
struct Display : Raw<0x1c> {};     // clear colour floats at +0x10
struct Object3d : Raw<0x90> {};
struct Lod : Raw<0x50> {};

struct Light : Raw<0xd0> {
  Node** nodes = nullptr;          // +0xcc  count +0xc8
};

struct Sound : Raw<0x94> {
  void* sound = nullptr;           // +0x00  zSound's sound, from the name at +0x08
  Node** nodes = nullptr;          // +0x90  count +0x8c
};

}  // namespace zcls
