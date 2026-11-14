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

// MechWarrior 3's list; Pirate's Moon has its own (its branch). It shows exactly what the switch turns off:
// each line has its gates in tests/test_fixes.cpp's table (small_fixes_list_matches_its_gates), which fails for a
// line with no gate and for a gate with no line.
const std::vector<const char*>& small_fixes_list() {
  static const std::vector<const char*> l = {
      "A turret's charge or glow goes out when it dies.",
      "Two c4 enemies follow their scripted orders.",
      "Tripod turrets aim when facing straight up or down.",
      "A vehicle's gun stops at its true aiming limit.",
      "A vehicle file with pitch and no roll keeps its pitch.",
      "The reticle locks and reads past 800 m when weapons reach.",
      "The mech lab offers the plain PPC the data defines.",
      "TAG's strike lands where the target is, not was.",
      "A held beam stays on a moving mech after release.",
      "A destroyed weapon's selection moves to a working one.",
      "A mech re-placed where it stands keeps to its ground.",
      "A mission's last animation can be waited for.",
      "An animation whose object is missing waits its turn.",
      "Leaving c4b mission 5's or 6's area fails it.",
      "c2 mission 3's objective waits for its two Fireflies.",
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
