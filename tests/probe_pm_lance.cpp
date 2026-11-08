// The lance's fit at a load's end (0x00407c20, 0x004079b0), for test_pm_lance.py: what the rule does per game type.
#include <cstring>
#include <cstdint>
#include <vector>

#include "mech3/briefing.h"
#include "mech3/mech_config.h"
#include "mech3/mech_reader.h"
#include "mech3/pilot.h"

using namespace mech3;

extern "C" {
// slots: n of (mech, loadout index or -1). store: the mechs in it. out: for each slot mech, its loadout's mech or -2, its
// loadout's name's index in that mech's list or -1; then the mechs left in the store, one each by count.
int32_t probe_pm_lance(int32_t kind, const int32_t* slots, int32_t n, const int32_t* store_mechs, int32_t m, int32_t* out) {
  Store store(0);
  for (int32_t i = 0; i < m; ++i) store.add_mech(store_mechs[i]);
  std::vector<GameSlot*> list;
  for (int32_t i = 0; i < n; ++i) {
    auto* s = new GameSlot(slots[2 * i]);
    if (slots[2 * i + 1] >= 0 && s->mech >= 0) s->set_config(mech_dfn(s->mech)->configs->configs[static_cast<size_t>(slots[2 * i + 1])]);
    list.push_back(s);
  }
  lance_fit_game(kind, store, list);
  int32_t k = 0;
  for (GameSlot* s : list) {
    out[k++] = s->mech, out[k++] = s->config ? s->config->mech : -2;
    delete s;
  }
  for (const ConfigItem* it : store.items)
    if (it->kind() == 0) out[k++] = it->id(), out[k++] = it->count;
  return k;
}
// 0x004079b0 with the loadout given (the lobby's return from salvage): one slot (its mech and loadout, -1 none),
// the given loadout (a mech's and its index, -1 none), `keep`, the store's mechs. Out as probe_pm_lance's.
int32_t probe_pm_lance_given(int32_t keep, const int32_t* in, const int32_t* store_mechs, int32_t m, int32_t* out) {
  Store store(0);
  for (int32_t i = 0; i < m; ++i) store.add_mech(store_mechs[i]);
  GameSlot s(in[0]);
  if (in[1] >= 0 && s.mech >= 0) s.set_config(mech_dfn(s.mech)->configs->configs[static_cast<size_t>(in[1])]);
  const Config* given = in[2] >= 0 && in[3] >= 0 ? mech_dfn(in[2])->configs->configs[static_cast<size_t>(in[3])] : nullptr;
  lance_slot(s, store, keep != 0, given);
  int32_t k = 0;
  out[k++] = s.mech, out[k++] = s.config ? s.config->mech : -2;
  int32_t at = -1;  // which of its mech's loadouts, by name (the slot's is a copy)
  if (s.config && s.config->name && s.config->mech >= 0)
    for (size_t i = 0; i < mech_dfn(s.config->mech)->configs->configs.size(); ++i)
      if (const char* n = mech_dfn(s.config->mech)->configs->configs[i]->name; n && std::strcmp(n, s.config->name) == 0) {
        at = static_cast<int32_t>(i);
        break;
      }
  out[k++] = at;
  for (const ConfigItem* it : store.items)
    if (it->kind() == 0) out[k++] = it->id(), out[k++] = it->count;
  return k;
}
}  // extern "C"
