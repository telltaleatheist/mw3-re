#pragma once
#include <cstdint>
#include <cstdio>

// Animation scripts: each motion in motion.zbd (docs/motion.md).
// zSIScript\si_script.cpp.
namespace zsis {

struct Script;

struct Part {               // 0x4c bytes
  float* translations;      // +0x00  (frames + 1) x vec3
  float* scales;            // +0x04  never in motion.zbd
  float* rotations;         // +0x08  (frames + 1) x quaternion
  uint8_t unk_0c;           // +0x0c
  float matrix[9];          // +0x10  identity
  float offset[3];          // +0x34
  bool owns_buffers;        // +0x41
  char* name;               // +0x44
  Script* script;           // +0x48

  Part();                                    // 0x005aa500
  bool attach(Script* s, const Part* share); // 0x005aa5a0
  bool read(std::FILE* f);                   // 0x005aa6d0
};

struct Script {             // vtable 0x5d0f20
  float unk_04 = 1.0f;      // +0x04
  float unk_08 = 1.0f;      // +0x08  header +0x14, 1.0 in every motion
  float unk_0c = -1.0f;     // +0x0c  header +0x10, -1.0 in every motion
  bool active = false;      // +0x10  SetLoopTime does nothing while clear
  char name[0x43] = {};     // +0x11
  float loop_time = 0;      // +0x54
  float inv_loop_time = 0;  // +0x58
  float frame_time = 0;     // +0x5c
  float inv_frame_time = 0; // +0x60
  uint32_t frame_count = 0; // +0x64
  uint32_t part_count = 0;  // +0x68
  Part** parts = nullptr;   // +0x6c
  bool unk_70 = true;       // +0x70
  float time = 0;           // +0x74  reset by set_loop_time
  int32_t unk_78 = 0, unk_7c = 0, unk_80 = 0;
  int32_t unk_84 = 1;       // +0x84

  bool read(std::FILE* f);  // 0x005a9c70, vtable +4
  void set_loop_time(float t);  // 0x005aa3b0
};

}  // namespace zsis
