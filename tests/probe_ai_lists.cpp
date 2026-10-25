// Probes for test_ai_lists.py (docs/ai.md section 19): the AI's reset list
// 0x004d8610 (mech3/zai_list) over entries whose vt+0x04 follows the test's
// plan: nothing, an entry added at the back, or another taken off.
#include <cstdint>
#include <memory>
#include <vector>

#include "mech3/zai_list.h"
#include "probe_app.h"

namespace {
namespace ai = mech3::ai;

struct Plan {
  const int32_t* kind = nullptr;  // by id: 0 nothing, 1 add id arg, 2 take id arg
  const int32_t* arg = nullptr;
  int32_t ids = 0;
};
Plan g_plan;
ai::ResetList g_list;
std::vector<std::unique_ptr<struct Entry>> g_entries;

struct Entry : ai::ResetEntry {
  int32_t id;
  explicit Entry(int32_t i) : id(i) {}
  void reset() override;
};
Entry* entry(int32_t id) {
  for (auto& e : g_entries)
    if (e->id == id) return e.get();
  g_entries.push_back(std::make_unique<Entry>(id));
  return g_entries.back().get();
}
void Entry::reset() {
  probe_app::call("entry.vt+0x4", {static_cast<uint32_t>(id)});
  if (id < 0 || id >= g_plan.ids) return;
  const int32_t a = g_plan.arg[id];
  if (g_plan.kind[id] == 1) {
    g_list.items.push_back(entry(a));
  } else if (g_plan.kind[id] == 2) {
    for (auto it = g_list.items.begin(); it != g_list.items.end(); ++it)
      if (static_cast<Entry*>(*it)->id == a) {
        g_list.items.erase(it);
        break;
      }
  }
}
}  // namespace

extern "C" {

// The list's first `n` entries (ids), the plan by id; returns the entries left.
int32_t probe_al_run(const int32_t* first, int32_t n, const int32_t* kind, const int32_t* arg, int32_t ids) {
  g_plan = {kind, arg, ids};
  g_list.items.clear();
  g_entries.clear();
  for (int32_t i = 0; i < n; ++i) g_list.items.push_back(entry(first[i]));
  g_list.reset();
  return static_cast<int32_t>(g_list.items.size());
}

}  // extern "C"
