#pragma once
#include <cstdint>
#include <list>
#include <vector>

// A mech's loadouts (docs/mechdef-configs.md). Original files unknown: the
// Config and its list sit between mech_anim.cpp and mech_data.cpp, the item
// and Config base methods between mech_reader.cpp and mech_salvage.cpp.
namespace mech3 {

struct ConfigItem {                // the shared head of every item
  int32_t count = 0;               // +0x04
  int32_t group = 0;               // +0x08
  int32_t part = -1;               // +0x0c  a part id, or a firepoint index for weapons
  uint8_t b10 = 0;                 // +0x10
  virtual ~ConfigItem() = default;
  virtual int kind() const = 0;    // vt+0x0c
  void set_part(int32_t p) { part = p; }  // vt+0x04, 0x00409f50
};

struct WeaponItem : ConfigItem {   // vtable 0x005c0fc0, 0x1c bytes
  int32_t weapon = -1;             // +0x14  mech-lab weapon index
  int32_t tech = 0;                // +0x18
  int kind() const override { return 1; }
};

struct AmmoItem : ConfigItem {     // vtable 0x005bd150, 0x18 bytes
  int32_t weapon = -1;             // +0x14
  int kind() const override { return 2; }
};

struct EquipmentItem : ConfigItem {  // vtable 0x005bd180, 0x20 bytes
  int32_t equipment = 0;           // +0x18  equip_kind
  int32_t tech = 0;                // +0x1c
  int kind() const override { return 3; }
  bool same_as(const ConfigItem& other) const;  // vt+0x24, 0x00473480
};

struct Config {                    // 0xd8 bytes, vtable 0x005bd3c0
  std::list<ConfigItem*> items;    // +0x08, count +0x0c
  uint8_t base_flag = 0;           // +0x10
  int32_t mech = 0;                // +0x14
  char* name = nullptr;            // +0x18
  uint8_t player_made = 0;         // +0x1c
  uint8_t b1d = 0;                 // +0x1d
  uint8_t training = 0;            // +0x1e
  uint8_t modified = 0;            // +0x1f  not set by the ctor
  uint8_t endo_steel = 0;          // +0x20
  int32_t engine = 0;              // +0x24  index into the engine table
  uint8_t xl = 0;                  // +0x28
  float armor_tons = 0;            // +0x2c
  int32_t armor_tech = 0;          // +0x30
  uint8_t ferro = 0;               // +0x34
  int32_t armor[11] = {};          // +0x38..+0x60
  int32_t sink_tech = 0;           // +0x64
  int32_t sinks = 10;              // +0x68
  uint8_t double_sinks = 0;        // +0x6c
  uint32_t g70 = 0, g78 = 0;       // +0x70, +0x78  copies of 0x0063d870 / 0x0063d878 (0 at load)
  float turn_rate;                 // +0x74
  float accel_rate;                // +0x7c
  float decel_rate;                // +0x80
  float turn_accel;                // +0x84
  Config(int32_t mech, const char* name);                       // 0x00423d00
  ConfigItem* add_item(ConfigItem* item, int32_t n);            // 0x004723c0
  EquipmentItem* add_equipment(int32_t kind, int32_t tech, int32_t n);  // 0x00471fb0
  int32_t sink_items() const;                                   // 0x00425f30
  void set_engine(int32_t rating);                              // 0x00432ea0
  int32_t item_count() const { return static_cast<int32_t>(items.size()); }
};

struct ConfigList {                // 0x18 bytes
  int32_t count;                   // +0x00
  int32_t mech;                    // +0x04
  std::vector<Config*> configs;    // +0x0c
  ConfigList(int32_t mech, int32_t count);  // 0x00424a90
};

}  // namespace mech3
