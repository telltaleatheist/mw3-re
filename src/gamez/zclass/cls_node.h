#pragma once
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// The scene node and its pool (docs/zclass-world.md, docs/gamez.md). The
// original file is unknown: these functions sit between class.c, cls_util.c
// and cls_zbd.c, so the file is named for the struct.
namespace zmod { struct Model; }
namespace platform { class Alert; }

namespace zcls {

struct ListItem;

struct Node {                      // 0xd4-byte pool slot
  uint8_t raw[0xd0];               // as stored; the fields below replace its pointers
  uint32_t link;                   // +0xd0  bits 0-23 free link, bit 24 live
  void* data = nullptr;            // +0x38  class data
  zmod::Model* model = nullptr;    // +0x3c  an index in the file
  void* action = nullptr;          // +0x48  callback
  Node** parents = nullptr;        // +0x58
  Node** children = nullptr;       // +0x60
  ListItem* changed = nullptr;     // +0xcc  its item on list 7

  template <typename T> T get(int off) const {
    T v;
    std::memcpy(&v, raw + off, sizeof v);
    return v;
  }
  template <typename T> void put(int off, T v) { std::memcpy(raw + off, &v, sizeof v); }
  const char* name() const { return reinterpret_cast<const char*>(raw); }
  uint32_t flags() const { return get<uint32_t>(0x24); }
  void set_flags(uint32_t f) { put<uint32_t>(0x24, f); }
  int32_t cls() const { return get<int32_t>(0x34); }
  int32_t parent_count() const { return get<int32_t>(0x54); }
  int32_t child_count() const { return get<int32_t>(0x5c); }
};

constexpr uint32_t kNodeLive = 0x1000000;
constexpr uint32_t kNodeLinkMask = 0xffffff;

// FIX (A): blocks are owned arrays; the exe reallocs a pointer array.
struct NodePool {
  std::vector<std::unique_ptr<Node[]>> blocks;  // 0x0060bf38, count 0x0060bf30
  int32_t block_size = 2500;       // 0x005ea830
  int32_t capacity = 0;            // 0x0060bf34
  int32_t live = 0;                // 0x0060bf3c
  int32_t free_head = -1;          // 0x005ea834
};
NodePool& nodes();

// Frame counters used when a node joins list 7.
struct Frames {
  uint32_t frame = 0;              // 0x0072f338
  uint32_t expiry = 0;             // 0x0073f3a8
};
Frames& frames();

Node* node_alloc();                // 0x004eacd0
int node_pool_grow();              // 0x004f8c80, 0 ok, 1 out of memory
void node_slot_free(Node* node);   // 0x004f8de0
int32_t node_total();              // 0x004eacb0
int32_t node_live();               // 0x004eacc0
[[noreturn]] void gamez_abort(const char* message);  // 0x004eada0
void set_alert(platform::Alert* alert);
Node* node_at(int32_t index);      // 0x004fe1d0
void free_node(int32_t index);     // 0x004f8ec0
void set_expiry(Node* node);       // 0x004ed990

// The null and class checks every class setter starts with: 0, or the
// setter's return code (5 null node or data, 3 wrong class).
int check_class(const Node* node, int cls, const char* file, int line);

}  // namespace zcls
