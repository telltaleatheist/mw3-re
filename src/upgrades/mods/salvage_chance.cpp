#include "upgrades/mods/salvage_chance.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "mech3/mods.h"

namespace mech3 {

namespace {
SalvageRules g_rules;
int32_t g_now = -1;

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
// A whole number and nothing else.
bool number_of(const std::string& s, int32_t* out) {
  if (s.empty() || s.size() > 6) return false;
  for (char c : s)
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  *out = std::atoi(s.c_str());
  return true;
}
}  // namespace

bool salvage_chance_parse(const std::string& value, int32_t* out) {
  int32_t n = -1;
  if (!number_of(value, &n) || n > 100) return false;
  *out = n;
  return true;
}

bool mission_salvage_parse(const std::string& value, MissionSalvage* out) {
  std::istringstream in(value);
  std::string world, number, chance, more;
  if (!(in >> world >> number >> chance) || (in >> more)) return false;
  MissionSalvage m;
  m.world = world;
  if (!number_of(number, &m.number) || m.number < 1 || !salvage_chance_parse(chance, &m.chance)) return false;
  *out = m;
  return true;
}

SalvageRules& salvage_rules() { return g_rules; }
void salvage_rules_clear() { g_rules = SalvageRules{}; }

void salvage_rules_take(const ModInfo& m) {
  if (!m.salvage_chance.empty()) {
    int32_t n = -1;
    if (!salvage_chance_parse(m.salvage_chance, &n))
      std::printf("mw3: mod %s: salvage_chance=%s is not a number from 0 to 100; left out\n", m.folder.c_str(), m.salvage_chance.c_str());
    else if (g_rules.chance < 0)
      g_rules.chance = n, g_rules.from = m.folder, std::printf("mw3: mod %s: a kill's salvage is offered %d times in 100 (salvage_chance)\n", m.folder.c_str(), n);
    else
      std::printf("mw3: mod %s: salvage_chance=%d is not used: %s set it first (%d)\n", m.folder.c_str(), n, g_rules.from.c_str(), g_rules.chance);
  }
  for (const std::string& line : m.mission_salvage) {
    MissionSalvage ms;
    if (!mission_salvage_parse(line, &ms)) {
      std::printf("mw3: mod %s: mission_salvage=%s does not read (a world, a mission number, 0 to 100); left out\n", m.folder.c_str(), line.c_str());
      continue;
    }
    bool taken = false;
    for (const SalvageRules::Mission& have : g_rules.missions)
      if (lower(have.m.world) == lower(ms.world) && have.m.number == ms.number) {
        std::printf("mw3: mod %s: mission_salvage for %s mission %d is not used: %s set it first (%d)\n", m.folder.c_str(), ms.world.c_str(),
                    ms.number, have.from.c_str(), have.m.chance);
        taken = true;
        break;
      }
    if (taken) continue;
    g_rules.missions.push_back({ms, m.folder});
    std::printf("mw3: mod %s: in %s mission %d a kill's salvage is offered %d times in 100 (mission_salvage)\n", m.folder.c_str(),
                ms.world.c_str(), ms.number, ms.chance);
  }
}

int32_t salvage_chance_for(const std::string& world, int32_t number, std::string* from) {
  for (const SalvageRules::Mission& have : g_rules.missions)
    if (lower(have.m.world) == lower(world) && have.m.number == number) {
      if (from) *from = have.from;
      return have.m.chance;
    }
  if (from) *from = g_rules.from;
  return g_rules.chance;
}

int32_t& salvage_chance_now() { return g_now; }

bool salvage_coin(int32_t r, int32_t chance) {
  if (chance <= 0) return false;
  if (chance >= 100) return true;
  return ((r & 0x7fff) * 100 >> 15) < chance;  // rand() is 15 bits: 0..99, each as likely
}

std::string salvage_chance_sentence(const ModInfo& m) {
  std::string out;
  int32_t n = -1;
  if (salvage_chance_parse(m.salvage_chance, &n))
    out = "It sets how often a kill's salvage is offered: " + std::to_string(n) + " in 100; the original's is 1 in 2.";
  int32_t missions = 0;
  MissionSalvage ms;
  for (const std::string& line : m.mission_salvage) missions += mission_salvage_parse(line, &ms);
  if (missions) {
    if (!out.empty()) out += " ";
    out += missions == 1 && mission_salvage_parse(m.mission_salvage.front(), &ms) && m.mission_salvage.size() == 1
               ? "For " + ms.world + " mission " + std::to_string(ms.number) + " it sets how often a kill's salvage is offered: " + std::to_string(ms.chance) + " in 100."
               : "For " + std::to_string(missions) + " missions it sets its own odds of a kill's salvage.";
    if (n < 0) out += " The original's is 1 in 2.";
  }
  return out;
}

}  // namespace mech3
