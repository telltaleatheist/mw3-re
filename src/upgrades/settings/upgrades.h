#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "gamez/zutil/zfix.h"

// Ours (Owen, 2026-11-10; docs/upgrades-sort.md): the Upgrades page's five
// switches, their mw3.cfg keys, the Small fixes list a player can read, and
// the word the multiplayer host sends so everyone plays on its switches.
namespace upgrades {

struct Switch {
  const char* key;              // mw3.cfg's upgrade_<key>=0/1
  bool zfix::Options::*field;
  const char* label;            // the page's line
};
const std::vector<Switch>& switches();

// mw3.cfg's lines as read: -1 where a key is not there.
struct Config {
  int value[5] = {-1, -1, -1, -1, -1};
  int bugfixes = -1;  // 1.0.x's one switch, read once as all five
};
// True when `line` is one of the keys.
bool read_line(const std::string& line, Config& c);
// Into zfix's options: each key's value, else bugfixes= for all five, else on.
void apply(const Config& c);
// bugfixes= with no upgrade_ key: the next start writes the five and drops it.
bool needs_migrating(const Config& c);

// The Small fixes switch's list, in plain words (upgrades-sort.md, the in-game
// list), one line each.
const std::vector<const char*>& small_fixes_list();

// The five as bits (switches()' order), for the lobby's word.
uint32_t bits();
void set_bits(uint32_t b);

// A joiner plays on the host's switches: kept here while it does, given back
// as the network game ends (GameEngine::network_set).
void take_hosts(uint32_t b);
void give_back();
bool hosts();  // the host's are in place (the page shows them greyed)

}  // namespace upgrades
