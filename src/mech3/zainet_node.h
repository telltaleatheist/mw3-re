#pragma once
#include <cstdint>
#include <vector>

#include "mech3/zai_valve.h"

// One node of an AI net or region as its file describes it (docs/ai.md, Net
// files). Original file unknown: the record's functions sit between
// zainetmechinout.cpp and zaitarget.cpp (0x004c5fb0..0x004c6d00), the valve
// union's between zaitarget.cpp and zrdrainet.cpp (0x004d44b0, 0x004d45c0).
namespace mech3::ai {

struct ValveCount {      // 8 bytes; 0x004c7760 clears one
  Valve* valve = nullptr;
  int32_t count = 0;
};

// valveunion ( (1 "valve" n) ... ): 0x10 bytes, a byte then a std::vector.
struct ValveUnion {
  uint8_t tag = 0;                  // +0x00  the ctor's byte argument, never read
  std::vector<ValveCount> valves;   // +0x04..+0x0c
};

struct ValveAssign {     // valve_assign ( "valve" value ): 8 bytes
  Valve* valve = nullptr;
  int32_t value = 0;
};

// 0xb4 bytes behind vtable 0x005cae54 (base 0x005cae50), made by 0x004c6ca0
// (0x004c5fb0 with the defaults at 0x005e8af8). The file's values are copied as
// 32-bit words whatever their reader type (an int where a float belongs keeps
// its bits); the names are the editor's (the writer 0x004db8d0).
struct NetNodeRec {
  int32_t kind = 0;                  // +0x04  0: node_XX (12 (pos) (links) ...), 1: node_XX ((links) ...)
  int32_t index = -1;                // +0x08  XX
  float pos[3] = {};                 // +0x0c
  int32_t speed_on = 0;              // +0x18  speed ( on rate )
  float speed = 1.0f;                // +0x1c
  int32_t wait_interval_on = 0;      // +0x20
  float wait_interval = 0;           // +0x24
  int32_t wait_time_on = 0;          // +0x28
  float wait_time = 0;               // +0x2c
  int32_t valve_on = 0;              // +0x30  valve ( on "name" [value] )
  Valve* valve = nullptr;            // +0x34
  uint8_t valve_has_value = 0;       // +0x38  the third item was there
  int32_t valve_value = 0;           // +0x3c
  int32_t canleave[2] = {0, 1};      // +0x40
  int32_t jump_on = 0;               // +0x48
  float jump = 10.0f;                // +0x4c
  int32_t scan_aim[2] = {};          // +0x50
  int32_t scan_hit[2] = {};          // +0x58
  int32_t scan_time[2] = {};         // +0x60
  uint8_t hidden_set = 0, hidden = 0;        // +0x68
  uint8_t fixed_set = 0, fixed = 0;          // +0x6a
  uint8_t autonext_set = 0, autonext = 1;    // +0x6c
  uint8_t visible_set = 0, visible = 1;      // +0x6e
  uint8_t deploy_set = 0;            // +0x70  deploy ( "valve" byte "valve" byte short )
  uint8_t deploy_a = 0;              // +0x71
  uint8_t deploy_b = 0;              // +0x72
  int16_t deploy_c = 0;              // +0x74
  Valve* deploy_valve_a = nullptr;   // +0x78
  Valve* deploy_valve_b = nullptr;   // +0x7c
  uint8_t arrange_set = 0;           // +0x80  arrangenodes ( spacing )
  float arrange = 0;                 // +0x84
  uint8_t mcbwait_set = 0;           // +0x88  mcbwait ( "valve" byte time )
  uint8_t mcbwait_b = 0;             // +0x89
  float mcbwait_time = 0;            // +0x8c
  Valve* mcbwait_valve = nullptr;    // +0x90
  void* sound = nullptr;             // +0x94  zSound's object for "sound"
  uint8_t backup = 0;                // +0x98
  uint8_t shunt = 0;                 // +0x99
  // Owned below here (0x004c62d0 frees them, 0x004c6050 deep-copies them).
  char* name = nullptr;              // +0x9c  strdup of the localized name
  char* auxname = nullptr;           // +0xa0  strdup of auxname, or of the name as written
  std::vector<int32_t> links;        // +0xa4  a pointer to a 0x10-byte vector in the exe
  char* gate = nullptr;              // +0xa8
  ValveUnion* valve_union = nullptr; // +0xac
  ValveAssign* valve_assign = nullptr;  // +0xb0

  NetNodeRec() = default;                       // 0x004c6ca0
  NetNodeRec(const NetNodeRec& other);          // 0x004c6ce0 -> 0x004c6050
  NetNodeRec& operator=(const NetNodeRec& o);   // 0x004c63a0
  ~NetNodeRec();                                // 0x004c62d0
  void reset();                                 // 0x004c6bd0: frees, then the defaults
};

}  // namespace mech3::ai
