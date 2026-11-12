#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

// Ours (Owen, 2026-11-12; docs/valves/README.md): a mission's valve map. For every valve the mission
// touches: its starting value, who sets it and who reads it, gathered from the mission as the port has
// loaded it (the valve registry, valves.zrd, aiv.zrd's vlists, targets.zrd's health_below, the AI nets'
// valve steps, objectives.zrd, the animations bound to valves, the field bases), read through the ported
// readers. Names, record names, numbers and message ids only: never the game's text.
namespace mech3 {

struct ValveMapEntry {
  std::string name;
  bool registered = false;      // in the valve registry at the first frame
  bool has_start = false;       // valves.zrd's `value`
  int start = 0;
  int now = 0;                  // at the first frame
  std::vector<std::string> sets, reads, effects;  // Markdown lines, each a link's "who and how"
};

struct ValveMap {
  std::string mission;          // as --mission names it: c1m1
  std::string dir;              // c1
  int number = 0;               // 1
  std::map<std::string, ValveMapEntry> valves;
  std::vector<std::string> sources;  // what was read, in order
  int vlist_entries = 0, valve_records = 0, objective_links = 0, net_links = 0, anim_links = 0, target_links = 0;
};

// The mission named (dir and number) must be up: its archives mounted, the registries filled (the
// program calls this at the mission's first Core frame). `object_of` turns the engine's handles (the
// animations' valve records hold them) into objects.
ValveMap valve_map_collect(const std::string& mission, const std::string& dir, int number,
                           const std::function<const void*(uint32_t)>& object_of);
std::string valve_map_markdown(const ValveMap& m);
// The anchor a valve's heading carries: "v-" and the name, lower case, with every other character '-'.
std::string valve_map_anchor(const std::string& name);

}  // namespace mech3
