#pragma once
#include <cstdint>

#include "gamez/zweapon/zweapon.h"

namespace zcls { struct Node; }
namespace zimg { struct TextureEntry; }
namespace zmod { struct Material; }
namespace zeff { struct AnimDef; }
namespace zrdr { struct Node; }

// zDEClient's set-up (zdec_init.cpp): declient.zrd's crater and quicksand
// settings, read at mission load (docs/craters.md, section 1).
namespace zdec {

// A ground material and the crater material and animation used on it
// (12 bytes in the exe; entry 0 the defaults).
struct CraterTexture {
  zmod::Material* ground = nullptr;   // +0x00
  zmod::Material* crater = nullptr;   // +0x04
  zeff::AnimDef* anim = nullptr;      // +0x08
};

struct State {
  zcls::Node* world_node = nullptr;          // 0x0060c0c0
  void* world = nullptr;                     // 0x0060c0c4  its data (+0x38)
  zwep::CraterOrder crater;                  // 0x0060bfc0  (10 dwords)
  zwep::QuicksandOrder quicksand;            // 0x0060c00c  (11 dwords)
  CraterTexture* textures = nullptr;         // 0x0060bfbc  (calloc'd, realloc'd)
  int32_t texture_count = 0;                 // 0x0060bfb8
  uint32_t quicksand_on = 0;                 // 0x0060bfe8
  int32_t frame_count = 0;                   // 0x0060bfec  quicksand frames
  float frame_rate = 0;                      // 0x0060bff0
  const char** frames = nullptr;             // 0x0060bff4  names in the reader tree (freed with it)
  zmod::Material* quicksand_cycle = nullptr; // 0x0060bffc  the animated material
  zmod::Material* quicksand_still = nullptr; // 0x0060c000  its first frame
  uint32_t restore_on = 0;                   // 0x005eba1c  set by 0x00501f90
};
State& state();

// What the set-up calls outside zDEClient; null: the ports.
struct Hooks {
  zrdr::Node* (*load)(const char* name) = nullptr;                  // 0x00556740
  zimg::TextureEntry* (*find_texture)(const char* name) = nullptr;  // 0x00518220
  zimg::TextureEntry* (*add_texture)(const char* name) = nullptr;   // 0x00518c60
  int32_t (*load_textures)() = nullptr;                             // 0x005190d0
  zeff::AnimDef* (*anim_find)(const char* name) = nullptr;          // 0x0050b450
  // 0x005a8a20("zDEClient", 0x00500b50, 0x00500c20, 1000, 0): the restore.
  void (*register_restore)() = nullptr;
};
Hooks& hooks();

void set_mode(uint32_t v);               // 0x00501f90
void set_world(zcls::Node* node);        // 0x00502070
int32_t init(zcls::Node* world);         // 0x00501ab0, 0 or -1 (no world)

// The defaults a weapon's order starts from (zdec_crater.cpp, zdec_qsand.cpp).
void crater_defaults(zwep::CraterOrder* out);        // 0x004ff920
void quicksand_defaults(zwep::QuicksandOrder* out);  // 0x005020f0

}  // namespace zdec
