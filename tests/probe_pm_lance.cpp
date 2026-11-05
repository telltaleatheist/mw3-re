// The lance's fit at a load's end (0x00407c20, 0x004079b0), for test_pm_lance.py: what the rule does per game type.
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
  lance_fit(store, list, lance_keeps(kind));
  int32_t k = 0;
  for (GameSlot* s : list) {
    out[k++] = s->mech, out[k++] = s->config ? s->config->mech : -2;
    delete s;
  }
  for (const ConfigItem* it : store.items)
    if (it->kind() == 0) out[k++] = it->id(), out[k++] = it->count;
  return k;
}
}  // extern "C"
