// Probe for test_salvage.py (docs/campaign.md section 9): a store filled by
// a plan (mechs, weapons, ammo, equipment through the store's own adds),
// then 0x0047ae10's won path over the test's entity list: each mech's parts,
// weapons and bins, loadout and sinks from the setup, rand's answers from it
// too. Out: the store's items as test_pilot.py's item_text writes them, the
// bins left, the rand calls used.
#include <cstdint>
#include <cstring>
#include <iterator>
#include <utility>
#include <sstream>
#include <string>
#include <vector>

#include "mech3/mech_config.h"
#include "mech3/briefing.h"
#include "mech3/mech_salvage.h"
#include "mech3/pilot.h"

namespace {
using namespace mech3;

std::string item_text(const ConfigItem* it) {
  std::ostringstream o;
  const int k = it->kind();
  o << "[k" << k << " n" << it->count << " g" << it->group << " p" << it->part << " b" << int(it->b10);
  if (k == 3) {
    auto* e = static_cast<const EquipmentItem*>(it);
    o << " id" << e->equipment << " t" << e->tech;
  } else {
    o << " id" << it->id();
    if (k == 1) o << " t" << static_cast<const WeaponItem*>(it)->tech;
    if (k == 4) o << " x" << int(static_cast<const EngineItem*>(it)->xl);
  }
  o << "] ";
  return o.str();
}
}  // namespace

extern "C" int probe_salvage_run(const char* setup, char* out, int size) {
  std::istringstream ss(setup);
  Store store(0), own(0);
  std::string op;
  std::vector<int32_t> rands;
  std::vector<SalvageEntity> list;
  std::vector<Config*> configs;
  uint32_t allies = 0, team = 0, campaign = 0;
  while (ss >> op) {
    if (op == "mech") {
      int32_t t;
      ss >> t;
      store.add_mech(t);
    } else if (op == "weapon") {
      int32_t t, tech;
      ss >> t >> tech;
      store.add_weapon(t, tech);
    } else if (op == "copyw") {
      WeaponItem w;
      int32_t n;
      ss >> w.weapon >> w.tech >> n;
      store.add_copy(&w, n);
    } else if (op == "ammo") {
      int32_t t, n;
      ss >> t >> n;
      store.add_ammo(t, n);
    } else if (op == "equip") {
      int32_t k, tech, n;
      ss >> k >> tech >> n;
      store.add_equipment(k, tech, n);
    } else if (op == "rand") {
      int32_t v;
      ss >> v;
      rands.push_back(v);
    } else if (op == "run") {
      ss >> allies >> team >> campaign;
    } else if (op == "ent") {
      SalvageEntity e;
      int mech, player;
      ss >> mech >> player >> e.team;
      e.mech = mech != 0;
      e.player = player != 0;
      SalvageMech& m = e.m;
      int has, torso, head, l0, l1, mgr, doubles, nw, nb, np, ni;
      ss >> has >> m.type >> m.tech >> torso >> head >> l0 >> l1 >> m.sink_tech >> m.sinks >> doubles;
      m.has_data = has != 0, m.torso_gone = torso != 0, m.head_gone = head != 0;
      m.leg_gone[0] = l0 != 0, m.leg_gone[1] = l1 != 0, m.doubles = doubles != 0;
      ss >> np;
      for (int i = 0; i < np && i < 16; ++i) {
        int v;
        ss >> v;
        m.parts[i] = static_cast<int8_t>(v);
      }
      ss >> mgr >> nw;
      m.has_manager = mgr != 0;
      for (int i = 0; i < nw; ++i) {
        SalvageWeapon w;
        int present, hi, beam;
        ss >> present >> w.type >> hi >> w.item_tech >> beam >> w.bin;
        w.present = present != 0, w.has_item = hi != 0, w.beam = beam != 0;
        m.weapons.push_back(w);
      }
      ss >> nb;
      for (int i = 0; i < nb; ++i) {
        int32_t v;
        ss >> v;
        m.bins.push_back(v);
      }
      ss >> ni;
      if (ni >= 0) {
        auto* c = new Config(0, "salvage");
        for (int i = 0; i < ni; ++i) {
          int k, n, part, id, tech;
          ss >> k >> n >> part >> id >> tech;
          ConfigItem* it = nullptr;
          if (k == 3) {
            auto* q = new EquipmentItem;
            q->equipment = id, q->tech = tech;
            it = q;
          } else if (k == 1) {
            auto* q = new WeaponItem;
            q->weapon = id, q->tech = tech;
            it = q;
          } else {
            auto* q = new AmmoItem;
            q->weapon = id;
            it = q;
          }
          it->count = n, it->part = part;
          c->items.push_back(it);
        }
        configs.push_back(c);
        m.config = c;
      }
      list.push_back(e);
    }
  }
  size_t next = 0;
  int32_t used = 0;
  SalvageCalls calls;
  calls.rand = [&] {
    ++used;
    return next < rands.size() ? rands[next++] : 0;
  };
  if (campaign) lance_salvage(store, own, list, allies, team != 0, calls);
  std::ostringstream o;
  o << "store{n" << store.count() << ' ';
  for (const ConfigItem* it : store.items) o << item_text(it);
  o << "}\nown{n" << own.count() << ' ';
  for (const ConfigItem* it : own.items) o << item_text(it);
  o << "}\nbins";
  for (const SalvageEntity& e : list)
    for (int32_t b : e.m.bins) o << ' ' << b;
  o << "\nrand " << used << '\n';
  for (Config* c : configs) delete c;
  const std::string s = o.str();
  if (static_cast<int>(s.size()) >= size) return -1;
  std::memcpy(out, s.data(), s.size() + 1);
  return static_cast<int>(s.size());
}

// test_salvage.py's reallocation: the game's store and the salvage filled
// by the plan through the ported adds, the lab's store built
// (lab_store_build, 0x00408b40), the lab's edits, then 0x00408bd0's
// store_reallocate for a campaign.
extern "C" int probe_realloc_run(const char* setup, char* out, int size) {
  std::istringstream ss(setup);
  Store game(0), salvage(0), lab(0), realloc(0);
  int32_t kind = 1, units = 0;
  std::string where;
  ss >> where >> kind >> units;
  Store* target = nullptr;
  bool built = false;
  auto build = [&] {
    if (built) return;
    built = true;
    target = lab_store_build(lab, &game, kind == 1 ? &salvage : nullptr);
  };
  std::string line;
  std::getline(ss, line);
  while (std::getline(ss, line)) {
    std::istringstream ls(line);
    std::string op;
    ls >> where >> op;
    Store* s = &game;
    if (where == "salvage") s = &salvage;
    if (where == "edits") {
      build();
      s = target;
    }
    if (op == "mech") {
      int32_t t;
      ls >> t;
      s->add_mech(t);
    } else if (op == "weapon") {
      int32_t t, tech;
      ls >> t >> tech;
      s->add_weapon(t, tech);
    } else if (op == "ammo") {
      int32_t t, n;
      ls >> t >> n;
      s->add_ammo(t, n);
    } else if (op == "equip") {
      int32_t k, tech, n;
      ls >> k >> tech >> n;
      s->add_equipment(k, tech, n);
    } else if (op == "remove") {
      int32_t i, n;
      ls >> i >> n;
      if (!s->items.empty()) {
        auto it = s->items.begin();
        std::advance(it, i % static_cast<int32_t>(s->items.size()));
        s->remove(*it, n);
      }
    }
  }
  build();
  if (kind == 1) store_reallocate(lab, game, salvage, &realloc, units);
  std::ostringstream o;
  const std::pair<const char*, Store*> stores[] = {{"game", &game}, {"salvage", &salvage}, {"realloc", &realloc},
                                                   {"lab", &lab}};
  bool first = true;
  for (const auto& [name, st] : stores) {
    o << (first ? "" : "\n") << name << "{n" << st->count() << ' ';
    for (const ConfigItem* it : st->items) o << item_text(it);
    o << '}';
    first = false;
  }
  o << "\nlab_is " << (target == &lab ? "lab" : "game") << '\n';
  const std::string s = o.str();
  if (static_cast<int>(s.size()) >= size) return -1;
  std::memcpy(out, s.data(), s.size() + 1);
  return static_cast<int>(s.size());
}
