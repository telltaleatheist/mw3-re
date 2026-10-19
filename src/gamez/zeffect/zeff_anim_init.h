#pragma once
#include <cstdint>
#include <cstdio>

#include "gamez/zclass/cls_data.h"

// zEffect\zeff_anim_init.c: reading anim.zbd, the mission's animation
// definitions (docs/anim.md).
namespace zeff {

using zcls::Node;
using zcls::Raw;

struct SeqDef : Raw<0x40> {
  uint8_t* events = nullptr;       // +0x38  size +0x3c
};

struct NodeRef40 : Raw<0x28> { Node* node = nullptr; };    // +0x24
struct ObjectRef : Raw<0x60> { Node* node = nullptr; };    // +0x24
struct CreatedRef : Raw<0x2c> { void* made = nullptr; };   // +0x24  light, puffer or sound node
struct StaticSoundRef : Raw<0x24> { void* sound = nullptr; };  // +0x20
struct EffectRef : Raw<0x24> {};   // +0x20 effect index
struct AnimDef;
struct Prerequisite : Raw<0x30> {
  AnimDef* def = nullptr;          // +0x28  type 1
  Node* node = nullptr;            // +0x2c  types 2, 3
};
struct AnimRefRec : Raw<0x48> { void* ref = nullptr; };    // +0x44

struct AnimDef : Raw<0x13c> {
  Node* node = nullptr;            // +0x40  found by the name at +0x20
  Node* root = nullptr;            // +0x64  found by the name at +0x44
  SeqDef* seqs = nullptr;          // +0xc4  count +0x108
  uint8_t* reset_events = nullptr; // +0x100 size +0x104
  ObjectRef* objects = nullptr;    // +0x114 count +0x109
  NodeRef40* nodes = nullptr;      // +0x118 count +0x10a
  CreatedRef* lights = nullptr;    // +0x11c count +0x10b
  CreatedRef* puffers = nullptr;   // +0x120 count +0x10c
  CreatedRef* sounds = nullptr;    // +0x124 count +0x10d
  StaticSoundRef* static_sounds = nullptr;  // +0x128 count +0x10e
  EffectRef* effects = nullptr;    // +0x12c count +0x10f
  Prerequisite* prereqs = nullptr; // +0x130 count +0x110
  AnimRefRec* anim_refs = nullptr; // +0x134 count +0x112
  const char* name() const { return reinterpret_cast<const char*>(raw); }
  uint8_t count(int off) const { return raw[off]; }
};

struct AnimMessage : Raw<0x24> {};  // +0x20 result of 0x575f90

struct AnimState {
  Raw<0x44> block{};               // 0x0073f720, read whole from the file
  AnimDef* defs = nullptr;         // 0x0073f72c  count s16 0x0073f72a
  AnimMessage* messages = nullptr; // 0x0073f734  count 0x0073f730
  void* world = nullptr;           // 0x0073f738  set by 0x005090e0, kept over the read
  char path[0x80] = {};            // 0x0060c570
  int32_t loading = 0;             // 0x005ebcbc
  int32_t copy_trees = 0;          // 0x0060c5f0  never set: the copy path is dead
  int16_t def_count() const { return block.get<int16_t>(0x0a); }
  int32_t message_count() const { return block.get<int32_t>(0x10); }
};
AnimState& anim();

// What the loader calls on the scene graph. Each is an engine function not
// ported yet; the front end or a test sets them.
struct AnimHooks {
  Node* (*next_by_name)(const char* name, int list);       // 0x004f8bd0
  Node* (*node_root)(Node* node);                          // 0x004ed8b0
  Node* (*copy_tree)(Node* node);                          // 0x004f9b50 (dead)
  Node* (*find_node)(AnimDef* def, const char* name);      // 0x00509640
  Node* (*create_light)();                                 // 0x004fa720
  void (*set_name)(void* node, const char* name);          // 0x004eb300
  void* (*create_puffer)();                                // 0x0054a710
  Node* (*create_sound_node)();                            // 0x004fd280
  void (*set_sound)(Node* node, const char* name);         // 0x004fd480
  void* (*find_sound)(const char* name);                   // 0x0056f6f0
  int32_t (*effect_index)(const char* name);               // 0x0050d9e0
  Node* (*find_in_tree)(Node* node, const char* name);     // 0x004f9d60
  int32_t (*message_result)(AnimMessage* msg);             // 0x00575f90
};
AnimHooks& anim_hooks();

void anim_set_world(void* world);         // 0x005090e0
void anim_set_zbd_path(const char* path); // 0x005090f0
void anim_allow_loading(int32_t on);      // 0x00509150
int anim_read_zbd();                      // 0x0050a0e0

}  // namespace zeff
