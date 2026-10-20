#pragma once
#include <cstdint>
#include <list>
#include <vector>

// The engine's numbered objects (docs/ai.md, The net objects). Original file
// unknown: the code sits after gamezmech3\znetwork\znet_dplay.cpp
// (0x0058aec0..0x0058b390). Every object has an id and a list of elements
// and is kept, while it lives, in one registry (0x00603fd8) where others
// find it by id. The AI's nets, regions and waynets are such objects; the
// ids likely let a multiplayer session name them across machines.
namespace znet {

// What an object holds: a polymorphic element (in the AI, a net's nodes,
// a region's edges, a compiled net's steps). Slot 0 is the deleting dtor.
struct Element {
  virtual ~Element() = default;
  // What Object::elem gives for an index out of range, asked of element 0
  // (its slot +0x10): the AI's compiled nets answer with their null step.
  virtual Element* miss() { return nullptr; }
};

// 0x18 bytes behind vtable 0x005cefd0, ctor 0x0058aec0, dtor 0x0058b000.
class Object {
 public:
  explicit Object(int32_t id);  // 0x0058aec0: registered at the list's end
  virtual ~Object();            // 0x0058b000: unregistered (0x0058b310)
  Object(const Object&) = delete;
  Object& operator=(const Object&) = delete;

  int32_t get_id() const { return id; }                     // +0x18 0x0040a850
  int32_t count() const { return int32_t(elems.size()); }  // +0x10 0x00402b40
  Element* elem(int32_t i) const;                           // +0x0c 0x0058b060
  int32_t index_of(const Element* e) const;                 // +0x14 0x0058b130: -1 when not held

  int32_t id = 0;               // +0x04
  uint8_t id_byte = 0;          // +0x08  the id's low byte, never read here
  std::vector<Element*> elems;  // +0x0c..+0x14
};

// The registry, a std::list at 0x00603fd8 (head +0x04, size +0x08, +0x0c set
// while it is being cleared, so the dtors leave the list alone).
struct Registry {
  std::list<Object*> list;
  int32_t clearing = 0;
};
Registry& registry();

Object* find(int32_t id);  // 0x0058b160: the first object of that id
// 0x0058b1c0: a free id in [lo, hi), from `want` (or `lo`) upwards, wrapping;
// -1 when all are taken. A `want` outside the range is taken as it is if free.
int32_t alloc_id(int32_t want, int32_t lo, int32_t hi);
void clear();  // 0x0058b290: deletes every object with an id under 10000

// 0x005f2a48: the first id of the objects clear() keeps (the AI's regions).
constexpr int32_t kKeptIds = 10000;

}  // namespace znet
