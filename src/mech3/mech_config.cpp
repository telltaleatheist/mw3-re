#include "mech3/mech_config.h"

#include <cstring>

#include "gamez/zclass/cls_node.h"
#include "mech3/mech_names.h"

namespace mech3 {

namespace {
char* dup(const char* s) {
  char* d = new char[std::strlen(s) + 1];
  std::strcpy(d, s);
  return d;
}

EquipmentItem* new_equipment(int32_t n, int32_t kind, int32_t tech) {
  auto* e = new EquipmentItem;
  e->count = n, e->equipment = kind, e->tech = tech;
  return e;
}
}  // namespace

// Mech3.exe 1.2 @ 0x00473480. Same kind of equipment and tech, 3 matching either.
bool EquipmentItem::same_as(const ConfigItem& other) const {
  const auto* e = other.kind() == 3 ? static_cast<const EquipmentItem*>(&other) : nullptr;
  const int32_t other_tech = e ? e->tech : 0;
  if (other.kind() != kind() || !e || e->equipment != equipment) return false;
  return tech == other_tech || tech == kAnyTech || other_tech == kAnyTech;
}

// Mech3.exe 1.2 @ 0x00423d00, with the base 0x00470ce0(0).
Config::Config(int32_t m, const char* n)
    : mech(m), name(dup(n)), armor_tech(mech_tech(m)), sink_tech(mech_tech(m)),
      turn_rate(1.25663686f), accel_rate(0.5f), decel_rate(1.3f), turn_accel(3.0f) {}

// Mech3.exe 1.2 @ 0x00424350, with the item list's 0x00470d20
Config::~Config() {
  delete[] name;
  name = nullptr;
  for (ConfigItem*& i : items) delete i, i = nullptr;
  items.clear();
}

// Mech3.exe 1.2 @ 0x004723c0. Only equipment (kind 3) comes from the reader;
// the other kinds copy items in the mech lab and are not ported yet.
ConfigItem* Config::add_item(ConfigItem* item, int32_t n) {
  if (!item) return nullptr;
  modified = 1;
  if (item->kind() != 3) zcls::gamez_abort("Config::add_item: copying weapons and ammo is not ported yet");
  const auto* e = static_cast<const EquipmentItem*>(item);
  return add_equipment(e->equipment, e->tech, n);
}

// Mech3.exe 1.2 @ 0x00471fb0. Armor (kinds 1, 2) is one item counting half
// tons, merged with a matching one; each heat sink is its own item.
EquipmentItem* Config::add_equipment(int32_t kind, int32_t tech, int32_t n) {
  const bool is_ferro = kind == 2;
  if (is_ferro || kind == 1) {
    if (!is_ferro) tech = kAnyTech;
    EquipmentItem probe;
    probe.equipment = kind, probe.tech = tech;
    EquipmentItem* found = nullptr;
    for (ConfigItem* i : items) {
      auto* e = i->kind() == 3 ? static_cast<EquipmentItem*>(i) : nullptr;
      if (e && e->same_as(probe)) {
        found = e;
        break;
      }
    }
    if (found) {
      found->count += n;
      if (found->count < 0) found->count = 0;
    } else {
      found = new_equipment(n, kind, tech);
      items.push_back(found);
    }
    ferro = is_ferro;
    armor_tons = static_cast<float>(found->count) * 0.5f;  // 0x005c36d4
    armor_tech = is_ferro ? tech : kAnyTech;
    modified = 1;
    return found;
  }
  if (kind == 9 || kind == 10) {
    EquipmentItem* last = nullptr;
    for (int32_t i = 0; i < n; ++i) items.push_back(last = new_equipment(1, kind, tech));
    sinks = sink_items() + 10;
    sink_tech = tech;
    double_sinks = kind == 10;
    modified = 1;
    return last;
  }
  EquipmentItem* e = new_equipment(n, kind, tech);
  items.push_back(e);
  modified = 1;
  return e;
}

// Mech3.exe 1.2 @ 0x00425f30
int32_t Config::sink_items() const {
  int32_t n = 0;
  for (const ConfigItem* i : items)
    if (i->kind() == 3) {
      const int32_t k = static_cast<const EquipmentItem*>(i)->equipment;
      if (k == 9 || k == 10) ++n;
    }
  return n;
}

// Mech3.exe 1.2 @ 0x00432ea0
void Config::set_engine(int32_t rating) { engine = engine_index(rating); }

// Mech3.exe 1.2 @ 0x00424a90
ConfigList::ConfigList(int32_t m, int32_t n) : count(n), mech(m) {
  if (n > 0) configs.reserve(n);
}

// Mech3.exe 1.2 @ 0x00425110
ConfigList::~ConfigList() {
  for (Config*& c : configs) delete c, c = nullptr;
  configs.clear();
}

}  // namespace mech3
