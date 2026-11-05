// Pirate's Moon's standalone game for test_pm_standalone.py: the list, the mission, the store, the lineup.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "mech3/mech_config.h"
#include "mech3/mech_reader.h"
#include "mech3/pilot.h"

using namespace mech3;

int probe_pm_zrdr_load(const char* dir, const char* archive, const char* entry);  // probe_pm.cpp

namespace {
StandaloneGame* g_game = nullptr;
std::vector<int32_t> g_log;
struct Hooks : PilotHooks {
  const char* message(int32_t id) override {
    static std::string s;
    s = "MSG_" + std::to_string(id);
    return s.c_str();
  }
  const char* message_named(const char* name) override { return name; }
  int32_t store_add_mech(Store& store, int32_t mech, const char* config) override {
    store.add_mech(mech);
    g_log.push_back(1), g_log.push_back(mech);
    for (const char* c = config; c && *c; ++c) g_log.push_back(*c);
    g_log.push_back(0);
    return 1;
  }
} g_hooks;
}  // namespace

extern "C" {

// The list: for each mission world, number, order; returns how many.
int32_t probe_pm_sa_list(int32_t* out) {
  set_pilot_hooks(&g_hooks);
  const int32_t n = standalone_read_missions();
  int32_t k = 0;
  for (const StandaloneMission* m : standalone_missions()) out[k++] = m->world, out[k++] = m->number, out[k++] = m->order;
  delete g_game;
  g_game = new StandaloneGame(nullptr);
  return n;
}

int32_t probe_pm_sa_op(int32_t op, int32_t a) {
  StandaloneGame& g = *g_game;
  switch (op) {
    case 0: return g.set_mission(a);
    case 1: return g.unlocked();
    case 2: g.x3c = a; return 0;
    case 3: g.start(); return 0;
    case 4: return g.advance();
    case 5: return g.slot_count();
    case 6: return g.x38;
    case 7: return g.x3c;
    case 8: g.set_lancemate_count(a); return 0;
    case 9: return g.number();
    case 10: return g.world_index();
    default: return -1;
  }
}

// A lancemate from the mission: mech and its k-th loadout of the dfn the probe has (`dfn`).
void probe_pm_sa_lancemate(int32_t i, int32_t mech, int32_t record, const void* config) {
  auto* s = new GameSlot(mech);
  s->x14 = record;
  g_game->set_lancemate(i, s, static_cast<const Config*>(config));
}

// The same with the exe's slot stood in (test_pm_standalone's 0x434880): the slot holds the loadout, the store untouched.
void probe_pm_sa_lancemate_notake(int32_t i, int32_t mech, int32_t record, const void* config) {
  auto* s = new GameSlot(mech);
  s->x14 = record;
  auto& l = g_game->lineups[static_cast<size_t>(g_game->lineup)];
  if (static_cast<int32_t>(l.size()) == i + 1) {
    s->set_config(static_cast<const Config*>(config));
    l.push_back(s);
  } else {
    delete s;
  }
}

// The player's slot given a mech and loadout (0x004a19c0's first-time branch).
void probe_pm_sa_player(int32_t mech, const void* config) {
  if (GameSlot* s = g_game->slot_at(0)) s->mech = mech, s->set_config(static_cast<const Config*>(config));
}

// The store's items: kind, id, count, part, tech; returns how many ints. Then the calls logged.
int32_t probe_pm_sa_store(int32_t* out) {
  int32_t k = 0;
  for (const ConfigItem* it : g_game->store.items) {
    out[k++] = it->kind(), out[k++] = it->id(), out[k++] = it->count, out[k++] = it->part;
    out[k++] = it->kind() == 1 ? static_cast<const WeaponItem*>(it)->tech : it->kind() == 3 ? static_cast<const EquipmentItem*>(it)->tech : 0;
  }
  return k;
}
int32_t probe_pm_sa_log(int32_t* out) {
  const int32_t n = static_cast<int32_t>(g_log.size());
  std::memcpy(out, g_log.data(), n * 4);
  g_log.clear();
  return n;
}
// A slot of the lineup: mech, its loadout's mech or -2, its record index; -9 without.
int32_t probe_pm_sa_slot(int32_t i, int32_t* out) {
  GameSlot* s = g_game->slot_at(i);
  if (!s) return -9;
  out[0] = s->mech, out[1] = s->config ? s->config->mech : -2, out[2] = s->x14;
  return 0;
}
}  // extern "C"
