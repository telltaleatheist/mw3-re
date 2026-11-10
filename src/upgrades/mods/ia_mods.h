#pragma once
#include <string>
#include <vector>

// Ours (docs/mods-campaigns-plan.md, Instant Action): the rules of Instant
// Action's MODS apart from its screen (ia_mods_menu.h), for the tests.
namespace mech3 {

constexpr const char* kIaEntry = "ia";
// The line about the mods: `ticked` of them as the tick list shows them now,
// or (-1) what this run has mounted: "MODS: off", "MODS: 2 on".
std::string ia_mods_line(const std::string& write_dir, int ticked = -1);
// What ACCEPT does with the ticks as they were and as they are now.
enum class IaModsStep { kBack, kIntoEntry, kPlain };
IaModsStep ia_mods_step(const std::vector<std::string>& before, const std::vector<std::string>& now, bool inside);

}  // namespace mech3
