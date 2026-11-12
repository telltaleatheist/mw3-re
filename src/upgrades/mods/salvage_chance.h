#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Ours (Owen, 2026-11-11; docs/modding.md, The salvage chance): how often a
// kill's salvage is offered, opened to mods. The original draws a coin twice
// (rand() & 1, campaign.md section 9): a destroyed vehicle's weapons, all or
// none (0x00472ec0), and a destroyed mech that lost a leg, whole or not
// (0x00472f00). A mod's mod.txt may set the odds, for every mission of the
// campaign it is ticked for (salvage_chance=N, 0 to 100) or for one mission
// (mission_salvage=<world> <n> <N>, which wins over the mod-wide value).
// The first ticked mod that sets a value wins it. Unset, the original's draw
// runs untouched; set, the same single rand() is drawn (the stream stays in
// step) and judged against N. Only a campaign mission launched with such a
// mod mounted is given a value: the main menu's campaign mounts no mod,
// Instant Action and multiplayer take no salvage.
namespace mech3 {

struct ModInfo;

struct MissionSalvage {
  std::string world;    // its folder, any case
  int32_t number = 0;   // world.zrd's m<n>
  int32_t chance = -1;  // 0..100
};

// mod.txt's values: "N", and "<world> <n> <N>". False when it does not read.
bool salvage_chance_parse(const std::string& value, int32_t* out);
bool mission_salvage_parse(const std::string& value, MissionSalvage* out);

// What the mounted mods set, each value its first setter's (the winning order).
struct SalvageRules {
  int32_t chance = -1;  // mod-wide, -1 none
  std::string from;     // the mod (folder) it is from
  struct Mission {
    MissionSalvage m;
    std::string from;
  };
  std::vector<Mission> missions;
};
SalvageRules& salvage_rules();
void salvage_rules_clear();
// A mounted mod's lines, in mount order; one that does not read is said in the log and left out.
void salvage_rules_take(const ModInfo& m);
// The chance for a mission: its own line's, else the mod-wide one, else -1. `from`: the mod it is from.
int32_t salvage_chance_for(const std::string& world, int32_t number, std::string* from = nullptr);

// This mission's (set by the launch, -1 outside a campaign mission): what the salvage draws read.
int32_t& salvage_chance_now();
// The draw judged: r is one rand() (0..32767). N in 100, evenly over the draw's range.
bool salvage_coin(int32_t r, int32_t chance);
// "It sets how often a kill's salvage is offered: 100 in 100; the original's is 1 in 2."
std::string salvage_chance_sentence(const ModInfo& m);

}  // namespace mech3
