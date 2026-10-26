#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// C:\zipper\Mech3\mech_salvage.cpp (func2file: 0x00472da0..0x00472ec0 sit
// between mech_reader.cpp and it): what a mech leaves for the salvage store
// (docs/campaign.md section 9). The store is the missions object's +0xc0
// ([0x00609990], made by its constructor 0x0048f290 with flag 1).
namespace mech3 {

class Store;
struct Config;
struct ConfigItem;

// A weapon slot of the mech's weapon manager (+0xd4: +0x24 count, +0x40
// slots), as 0x00472da0 reads it.
struct SalvageWeapon {
  bool present = false;              // the slot holds a weapon
  int32_t type = -1;                 // its def's WeaponType id (+0x2c +0x1f0)
  bool has_item = false;             // +0x90: its loadout item, whose tech (+0x18) it keeps
  int32_t item_tech = 0;
  bool beam = false;                 // vt+0x08
  int32_t bin = -1;                  // +0x58, an index into SalvageMech::bins (-1 none)
};

// A mech as 0x00472f00 reads it.
struct SalvageMech {
  bool has_data = false;             // +0x684
  int32_t type = -1;                 // +0x684 +0xc (7, the Elemental, leaves nothing)
  int32_t tech = 3;                  // 0x0041df10: its definition's tech
  bool torso_gone = false, head_gone = false;  // +0x39c, +0x3a4: +0x48
  bool leg_gone[2] = {};             // +0x38c, +0x390
  bool has_manager = false;          // +0xd4
  std::vector<SalvageWeapon> weapons;
  std::vector<int32_t> bins;         // each bin's rounds left (0x004b4cf0), -1 infinite
  const Config* config = nullptr;    // +0x3ec
  int8_t parts[16] = {};             // 0x0041d510 by id: 0 none, 1 there, 2 destroyed
  int32_t sink_tech = 3;             // +0x658
  int32_t sinks = 10;                // +0x65c
  bool doubles = false;              // +0x660
  std::vector<const void*> owner_bins;  // the caller's bins, by index (for empty_bin)
};

struct SalvageCalls {
  std::function<int32_t()> rand;               // MSVCRT rand ([0x005bb668])
  std::function<void(const SalvageMech& m, int32_t bin)> empty_bin;  // 0x004b4d10(0) on the mech's bin
};

// Mech3.exe 1.2 @ 0x00472f00 (on the store): the mech itself (its torso
// whole, or its head gone; a coin when a leg is off), its weapons and ammo,
// its loadout's equipment on parts still standing, the sinks past the ten.
bool add_salvage(Store& s, SalvageMech& m, const SalvageCalls& c);
// Mech3.exe 1.2 @ 0x00472da0: each weapon in a tech it comes in, its ammo
// taken out of the mech.
bool add_weapons(Store& s, SalvageMech& m, int32_t tech, const SalvageCalls& c);
// Mech3.exe 1.2 @ 0x00472ec0: another vehicle's weapons, on a coin.
bool add_vehicle_salvage(Store& s, SalvageMech& m, const SalvageCalls& c);

// An entry of zEntity's list for 0x0047ae10: one with a mech (vt+0x2c then
// vt+0x50), the player's mission mech or not, its team (vt+0x28 +0xc0).
struct SalvageEntity {
  bool mech = false;
  bool player = false;
  uint32_t team = 0;
  SalvageMech m;
};
// Mech3.exe 1.2 @ 0x0047ae10, a won mission (CL 0): the player's mission
// mech into the campaign's +0x64 store (`own`), and with `team` (DL) every
// mech of its allies (the player's +0xc4) into the salvage store, in the
// list's order.
void lance_salvage(Store& salvage, Store& own, std::vector<SalvageEntity>& list, uint32_t allies, bool team,
                   const SalvageCalls& c);

// The store items' texts (their vt+0x20 name and vt+0x1c label,
// 0x004738f0..0x004739c0 and 0x00473700..0x004738b0), with Mech3Msg.dll's
// messages through `msg` (null as MSVC's sprintf shows it); an engine's
// 0x004739d0 / 0x004738b0.
using MessageText = std::function<const char*(int32_t)>;
std::string item_name(const ConfigItem& it, const MessageText& msg);
// 0x00432f00 on an engine (index, XL): the maker's name (0x00432ec0, the
// table's message), the rating, " XL".
std::string engine_name(int32_t engine, uint8_t xl, const MessageText& msg);
int32_t engine_maker_message(int32_t engine);  // 0x00432ec0: the maker's message
std::string item_label(const ConfigItem& it, const MessageText& msg);

// The missions object's salvage store (+0xc0, [0x00609990]).
Store& mission_salvage();

}  // namespace mech3
