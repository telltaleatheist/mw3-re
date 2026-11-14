#include "upgrades/settings/upgrades.h"

namespace upgrades {
namespace {
bool g_hosts = false;
uint32_t g_own = 0;  // the joiner's own five while the host's are in place
}  // namespace

const std::vector<Switch>& switches() {
  static const std::vector<Switch> s = {
      {"water", &zfix::Options::water, "WATER MOVEMENT"},
      {"mfb_regroup", &zfix::Options::mfb_regroup, "MFB REGROUP"},
      {"improved_ai", &zfix::Options::improved_ai, "IMPROVED AI"},
      {"artemis", &zfix::Options::artemis, "ARTEMIS TRACKING"},
      {"small_fixes", &zfix::Options::small_fixes, "SMALL FIXES"},
  };
  return s;
}

bool read_line(const std::string& line, Config& c) {
  if (line == "bugfixes=0" || line == "bugfixes=1") {
    c.bugfixes = line.back() - '0';
    return true;
  }
  const auto& s = switches();
  for (size_t i = 0; i < s.size(); ++i) {
    const std::string k = std::string("upgrade_") + s[i].key + "=";
    if (line == k + "0" || line == k + "1") {
      c.value[i] = line.back() - '0';
      return true;
    }
  }
  return false;
}

bool needs_migrating(const Config& c) {
  if (c.bugfixes < 0) return false;
  for (int v : c.value)
    if (v >= 0) return false;
  return true;
}

void apply(const Config& c) {
  const auto& s = switches();
  for (size_t i = 0; i < s.size(); ++i) {
    const int v = c.value[i] >= 0 ? c.value[i] : c.bugfixes;
    zfix::options().*s[i].field = v != 0;  // absent (-1): on
  }
}

// Pirate's Moon's list (MechWarrior 3's is main's): what the Small fixes switch turns off here, each line's code
// named in docs/upgrades-sort.md. Not on it: MechWarrior 3's lines for its own data (two c4 enemies' orders, c4b's
// areas, c2 mission 3's Fireflies: the code is shared and finds nothing to change in Pirate's Moon's files), the
// plain PPC (Pirate's Moon's table has it), the destroyed weapon's selection (Pirate's Moon's exe moves it), and
// Pirate's Moon's three AI guards (always on: a crash or an unset value has no original to give back).
const std::vector<const char*>& small_fixes_list() {
  static const std::vector<const char*> l = {
      "A turret's charge or glow goes out when it dies.",
      "Tripod turrets aim when facing straight up or down.",
      "A vehicle's gun stops at its true aiming limit.",
      "A vehicle file with pitch and no roll keeps its pitch.",
      "TAG's strike lands where the target is, not was.",
      "A held beam stays on a moving mech after release.",
      "A mech re-placed where it stands keeps to its ground.",
      "A mission's last animation can be waited for.",
      "An animation whose object is missing waits its turn.",
      "The Sunder Alternate B loads all 360 LRM 20 missiles.",
      "The Strider has its recoil slide (a number typed as text).",
      "The Vulture's DESERT camo has its cockpit canopy.",
  };
  return l;
}

uint32_t bits() {
  uint32_t b = 0;
  const auto& s = switches();
  for (size_t i = 0; i < s.size(); ++i)
    if (zfix::options().*s[i].field) b |= 1u << i;
  return b;
}

void set_bits(uint32_t b) {
  const auto& s = switches();
  for (size_t i = 0; i < s.size(); ++i) zfix::options().*s[i].field = (b >> i & 1) != 0;
}

void take_hosts(uint32_t b) {
  if (!g_hosts) g_own = bits();
  g_hosts = true;
  set_bits(b);
}

void give_back() {
  if (!g_hosts) return;
  g_hosts = false;
  set_bits(g_own);
}

bool hosts() { return g_hosts; }

}  // namespace upgrades
