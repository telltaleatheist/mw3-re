// Pirate's Moon's standalone game, the second campaign (docs/pirates-moon.md). Original
// file unknown: the functions sit at 0x0040d920..0x0040e6b0, before mech3\mech_gameapp's.
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_msg.h"
#include "mech3/mech_reader.h"
#include "mech3/pilot.h"

namespace mech3 {

void salvage_into(Store& s, zrdr::Node* node);  // mech_objective.cpp, 0x00473690

const char* PilotHooks::message_named(const char* name) { return message(zutl::msg_id(name)); }

namespace {
std::vector<StandaloneMission*> g_missions;  // 0x00618848
constexpr int32_t kLastWorld = 7;            // c1..c7
}  // namespace

std::vector<StandaloneMission*>& standalone_missions() { return g_missions; }

// Pirate's Moon 1.0 @ 0x0040def0: every zbd\c<w>\readersa<n>.zbd with a sainfo.zrd
// naming a db_name, sorted by its `order` (0x0040d660, 0x0040d900).
int32_t standalone_read_missions() {
  for (StandaloneMission* m : g_missions) delete m;
  g_missions.clear();
  for (int32_t w = 1; w <= kLastWorld; ++w) {
    const std::string pattern = "zbd\\c" + std::to_string(w) + "\\readersa*.zbd";
    for (const std::string& file : zutl::list(pattern.c_str())) {
      const int32_t n = std::atoi(file.c_str() + 8);  // past "readersa"
      if (n <= 0) continue;
      const std::string path = "zbd\\c" + std::to_string(w) + "\\readersa" + std::to_string(n) + ".zbd";
      if (!zrdr::open_archive(path.c_str(), false)) continue;
      zrdr::Node* root = zrdr::load("sainfo.zrd");
      if (root && zrdr::find(root, "db_name")) {  // 0x0040d920
        auto* m = new StandaloneMission;
        m->archive = path, m->world = w, m->number = n;
        const auto text = [&](const char* key) {
          const char* s = zrdr::get_string(root, key);
          return std::string(s ? pilot_hooks().message_named(s) : "");
        };
        m->name = text("db_name");
        m->text = text("db_verbose");
        zrdr::get_int(root, "order", &m->order);
        g_missions.push_back(m);
      }
      if (root) zrdr::free_node(root);
      zrdr::close_archive(path.c_str());
    }
  }
  std::stable_sort(g_missions.begin(), g_missions.end(),
                   [](const StandaloneMission* a, const StandaloneMission* b) { return a->order < b->order; });
  return static_cast<int32_t>(g_missions.size());
}

// Pirate's Moon 1.0 @ 0x0040e100: the head's slot is mech 5 and no loadout, as a Game's.
StandaloneGame::StandaloneGame(Pilot* p) : Game(p) {
  set_mission(0);
  slot.set_config(nullptr);
}

const char* StandaloneGame::game_text(int32_t which) {
  if (!x40) return nullptr;
  return which == 0x0c ? x40->name.c_str() : which == 0x10 ? x40->text.c_str() : nullptr;
}

// Pirate's Moon 1.0 @ 0x0040e340
bool StandaloneGame::set_mission(int32_t i) {
  if (g_missions.empty()) {
    x38 = 0, x40 = nullptr;
    return false;
  }
  x38 = i;
  const int32_t last = static_cast<int32_t>(g_missions.size()) - 1;
  const bool moved = x38 > last || x38 < 0;
  if (x38 > last) x38 = last;
  if (x38 < 0) x38 = 0;
  x40 = g_missions[static_cast<size_t>(x38)];
  return moved;
}

// Pirate's Moon 1.0 @ 0x0040e3f0 (0x0040da00 opening the reader, 0x0040de60 the lineup): the
// store holds this mission's salvage less what the lance already has.
bool StandaloneGame::start() {
  if (g_missions.empty()) return false;
  x40 = g_missions[static_cast<size_t>(x38)];
  close();
  if (zrdr::open_archive(x40->archive.c_str(), false)) x40->info = zrdr::load("sainfo.zrd");
  store.clear();
  zrdr::Node* salvage = x40->info ? zrdr::find(x40->info, "salvage") : nullptr;
  if (salvage) salvage_into(store, salvage);
  else pilot_hooks().store_add_mech(store, 0x10, pilot_hooks().message(0xf20));
  while (static_cast<int32_t>(lineups.size()) <= x38) lineups.emplace_back();
  lineup = x38;
  for (int32_t i = 0, n = slot_count(); i < n; ++i) {
    GameSlot* s = slot_at(i);
    if (!s || !s->config) continue;
    store.remove_config(s->config);  // 0x00474dd0
    store.remove_mech(s->mech);      // 0x00474b40
  }
  return false;
}

// Pirate's Moon 1.0 @ 0x0040daa0
void StandaloneGame::close() {
  if (!x40 || !x40->info) return;
  zrdr::free_node(x40->info);
  x40->info = nullptr;
}

// Pirate's Moon 1.0 @ 0x0040e4d0
int32_t StandaloneGame::unlocked() const {
  const int32_t n = static_cast<int32_t>(g_missions.size());
  return x3c + 1 >= n ? n : x3c + 1;
}

// Pirate's Moon 1.0 @ 0x0040e510: the pilot's file written with the new mission (Pilot::save
// does 0x0047af50's opening, header and vt+4).
bool StandaloneGame::advance() {
  close();
  const bool past = set_mission(x38 + 1);
  x3c = std::max(x3c, x38);
  if (pilot) pilot->save();
  return past;
}

int32_t StandaloneGame::number() const { return x40 ? x40->number : 0; }
int32_t StandaloneGame::world_index() const { return x40 ? x40->world - 1 : -1; }
zrdr::Node* StandaloneGame::block(const char* key) const {
  return x40 && x40->info ? zrdr::find(x40->info, key) : nullptr;
}

// Pirate's Moon 1.0 @ 0x0040e760. Slot 0 is the player's, made on first asking with no mech;
// a lancemate only up to the number the mission set.
GameSlot* StandaloneGame::slot_at(int32_t i) {
  if (lineup < 0) return nullptr;
  auto& l = lineups[static_cast<size_t>(lineup)];
  if (l.empty()) l.push_back(new GameSlot(-1));
  if (i < 0 || i >= static_cast<int32_t>(l.size()) || i > x48) return nullptr;
  return l[static_cast<size_t>(i)];
}

// Pirate's Moon 1.0 @ 0x0040e830
int32_t StandaloneGame::slot_count() const {
  const int32_t n = lineup < 0 ? 0 : static_cast<int32_t>(lineups[static_cast<size_t>(lineup)].size());
  return x48 <= n - 1 ? x48 + 1 : n;
}

// Pirate's Moon 1.0 @ 0x0040e6c0: lancemate i (slot i + 1) of this mission. A new one gets the
// mission's loadout, taken from the store with its mech; one already in the lineup keeps its
// own, taking only the new one's record index, and the new one is deleted.
void StandaloneGame::set_lancemate(int32_t i, GameSlot* s, const Config* config) {
  auto& l = lineups[static_cast<size_t>(lineup)];
  if (static_cast<int32_t>(l.size()) == i + 1) {
    s->set_config(config, &store, true);
    if (config) store.remove_mech(config->mech);
    l.push_back(s);
    return;
  }
  if (static_cast<size_t>(i + 1) >= l.size()) {  // FIX (bug): past the lineup the exe reads through nothing
    delete s;
    return;
  }
  GameSlot* have = l[static_cast<size_t>(i + 1)];
  have->x14 = s->x14;
  if (!have->config) have->set_config(config, &store, true);
  delete s;
}

}  // namespace mech3
