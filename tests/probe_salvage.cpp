// Probe for test_salvage.py (docs/campaign.md section 9): a store filled by
// a plan (mechs, weapons, ammo, equipment through the store's own adds),
// then 0x0047ae10's won path over the test's entity list: each mech's parts,
// weapons and bins, loadout and sinks from the setup, rand's answers from it
// too. Out: the store's items as test_pilot.py's item_text writes them, the
// bins left, the rand calls used.
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <iterator>
#include <tuple>
#include <utility>
#include <sstream>
#include <string>
#include <vector>

#include "crt/msvc_rand.h"
#include "mech3/mech_config.h"
#include "mech3/briefing.h"
#include "mech3/mech_reader.h"
#include "mech3/mech_salvage.h"
#include "mech3/pilot.h"
#include "mech3/salvage_screen.h"
#include "mech3/app_states.h"
#include "mech3/lab_screen.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zgame/unported.h"

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
      m.has_data = has != 0, m.chassis = torso == 0 || head != 0;
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

// The salvage screen's moves: two stores filled as probe_realloc_run's,
// then each "move p|m i" moves the side's i-th item (mod its count) across.
// tests/probe_salvage_defs.cpp: PM's moves' weapon definitions (100 rounds a ton), put back as found.
void* probe_salvage_pm_defs(bool on);
void probe_salvage_defs_back(void* kept);
namespace {
struct PmDefs {
  void* kept;
  explicit PmDefs(bool on) : kept(probe_salvage_pm_defs(on)) {}
  ~PmDefs() { probe_salvage_defs_back(kept); }
};
}  // namespace

extern "C" int probe_moves_run(const char* setup, char* out, int size) {
  std::istringstream ss(setup);
  Store player(0), mission(0);
  int32_t kind = 1, units = 0;
  std::string where;
  ss >> where >> kind >> units;
  // MW3's room is the campaign's (its units, 300 t each); PM's from the game (bases, tons) when given.
  bool pm = false;  // PM's rules (its exe asks the game's type and room)
  int32_t bases = kind == 1 ? units : 0;
  float tons = 300.0f * static_cast<float>(bases);
  {
    int32_t b;
    double t;
    if (ss.peek() != '\n' && ss >> b >> t) bases = b, tons = static_cast<float>(t), pm = true;
    ss.clear();
  }
  const PmDefs defs(pm);
  std::string line, moved;
  std::getline(ss, line);
  auto nth = [](Store& st, int32_t i) -> ConfigItem* {
    if (st.items.empty()) return nullptr;
    auto it = st.items.begin();
    std::advance(it, i % static_cast<int32_t>(st.items.size()));
    return *it;
  };
  while (std::getline(ss, line)) {
    std::istringstream ls(line);
    std::string op;
    ls >> where >> op;
    if (where == "move") {
      int32_t i;
      ls >> i;
      const bool r = op == "p" ? salvage_to_mission(&mission, &player, nth(player, i), pm && kind == 3)
                               : salvage_to_player(&player, &mission, nth(mission, i), bases, tons);
      moved += (moved.empty() ? "" : " ") + std::to_string(r ? 1 : 0);
      continue;
    }
    Store* s = where == "player" ? &player : &mission;
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
    }
  }
  std::ostringstream o;
  const std::pair<const char*, Store*> stores[] = {{"player", &player}, {"mission", &mission}};
  bool first = true;
  for (const auto& [name, st] : stores) {
    o << (first ? "" : "\n") << name << "{n" << st->count() << ' ';
    for (const ConfigItem* it : st->items) o << item_text(it);
    o << '}';
    first = false;
  }
  o << "\nmoved " << moved << '\n';
  const std::string str = o.str();
  if (static_cast<int>(str.size()) >= size) return -1;
  std::memcpy(out, str.data(), str.size() + 1);
  return static_cast<int>(str.size());
}

namespace {
using namespace mech3;

std::vector<std::tuple<int32_t, int32_t, int32_t, int32_t, int32_t>> read_items(std::istringstream& ls) {
  int32_t n = 0;
  ls >> n;
  std::vector<std::tuple<int32_t, int32_t, int32_t, int32_t, int32_t>> v;
  for (int32_t i = 0; i < n; ++i) {
    int32_t k, id, extra, part, count;
    ls >> k >> id >> extra >> part >> count;
    v.emplace_back(k, id, extra, part, count);
  }
  return v;
}

// A loadout as the test's 0x00423d00 + 0x004723c0 build it.
Config* make_config(int32_t mech, const std::string& name,
                    const std::vector<std::tuple<int32_t, int32_t, int32_t, int32_t, int32_t>>& items) {
  auto* c = new Config(mech, name.c_str());
  for (const auto& [k, id, extra, part, count] : items) {
    if (k == 1) {
      WeaponItem w;
      w.weapon = id, w.tech = extra, w.part = part;
      c->add_item(&w, count);
    } else if (k == 2) {
      AmmoItem a;
      a.weapon = id, a.part = part;
      c->add_item(&a, count);
    } else {
      EquipmentItem e;
      e.equipment = id, e.tech = extra, e.part = part;
      c->add_item(&e, count);
    }
  }
  return c;
}

std::string config_text(const Config* c) {
  if (!c) return "none";
  std::ostringstream o;
  o << "cfg{m" << c->mech << " \"" << (c->name ? c->name : "") << "\" mod" << int(c->modified) << " s" << c->sink_tech
    << ',' << c->sinks << ',' << int(c->double_sinks) << " free" << c->free_slots << ' ';
  for (const ConfigItem* it : c->items) o << item_text(it);
  o << '}';
  return o.str();
}
}  // namespace

// test_salvage.py's lance: stand-in definitions (firepoints as test_mechsave's
// fake_parts, a first loadout or none), the slots, the stores; then
// 0x00408c30's loop over lance_slot and the split.
extern "C" int probe_lance_run(const char* setup, char* out, int size) {
  std::istringstream ss(setup);
  static std::vector<Firepoint> fps[19];
  static std::vector<Firepoint*> ptrs[19];
  MechDfn* dfns[19] = {};
  for (int m = 0; m < 19; ++m) {
    if (fps[m].empty()) {
      for (int i = 0; i < 8; ++i) fps[m].push_back(Firepoint{nullptr, 0, 0, ((i * 5 + m) % 10) - 1});
      for (auto& f : fps[m]) ptrs[m].push_back(&f);
    }
    dfns[m] = static_cast<MechDfn*>(::operator new(sizeof(MechDfn)));
    std::memset(static_cast<void*>(dfns[m]), 0, sizeof(MechDfn));
    dfns[m]->firepoints = &ptrs[m];
    dfns[m]->configs = new ConfigList(m, 0);
    mech_dfn_slot(m) = dfns[m];
  }
  Store game(0), salvage(0), lab(0), realloc(0);
  std::vector<GameSlot*> slots;
  int32_t op = 0, mission = 0, units = 0;
  std::string line;
  while (std::getline(ss, line)) {
    std::istringstream ls(line);
    std::string what;
    ls >> what;
    if (what == "op") {
      ls >> op >> mission >> units;
    } else if (what == "dfn") {
      int32_t m;
      ls >> m;
      dfns[m]->configs->configs.push_back(make_config(m, "dfn" + std::to_string(m), read_items(ls)));
    } else if (what == "slot") {
      int32_t mech, cm;
      ls >> mech >> cm;
      auto* s = new GameSlot(mech);
      if (cm != -2) s->config = make_config(cm, "slot" + std::to_string(slots.size()), read_items(ls));
      slots.push_back(s);
    } else {
      std::string op2;
      ls >> op2;
      Store* s = what == "salvage" ? &salvage : &game;
      if (op2 == "mech") {
        int32_t t;
        ls >> t;
        s->add_mech(t);
      } else if (op2 == "weapon") {
        int32_t t, tech;
        ls >> t >> tech;
        s->add_weapon(t, tech);
      } else if (op2 == "ammo") {
        int32_t t, n;
        ls >> t >> n;
        s->add_ammo(t, n);
      } else if (op2 == "equip") {
        int32_t k, tech, n;
        ls >> k >> tech >> n;
        s->add_equipment(k, tech, n);
      }
    }
  }
  Store* target = lab_store_build(lab, &game, &salvage);
  (void)op, (void)mission;
  lance_fit(*target, slots, lance_keeps(kCampaign));
  store_reallocate(lab, game, salvage, &realloc, units);
  std::ostringstream o;
  for (GameSlot* s : slots) o << "slot m" << s->mech << ' ' << config_text(s->config) << '\n';
  const std::pair<const char*, Store*> stores[] = {{"game", &game}, {"salvage", &salvage}, {"realloc", &realloc},
                                                   {"lab", &lab}};
  for (const auto& [name, st] : stores) {
    o << name << "{n" << st->count() << ' ';
    for (const ConfigItem* it : st->items) o << item_text(it);
    o << "}\n";
  }
  for (GameSlot* s : slots) delete s;
  for (int m = 0; m < 19; ++m) {
    delete dfns[m]->configs;
    ::operator delete(dfns[m]);
    mech_dfn_slot(m) = nullptr;
  }
  const std::string s = o.str();
  if (static_cast<int>(s.size()) >= size) return -1;
  std::memcpy(out, s.data(), s.size() + 1);
  return static_cast<int>(s.size());
}

namespace {
using namespace mech3;

// The put-on's world for test_salvage.py's puton part: the shown slot's
// loadout, the lab's store and the definition behind handles, the screen's
// pieces and the list's stand-in slots logged as the test's stubs.
struct PutOnEngine : Engine {
  Config* cfg = nullptr;
  Store* store = nullptr;
  std::vector<LabFirepoint> fps;
  std::vector<std::string>* events = nullptr;
  int32_t part = -1;
  std::vector<std::string> kept;
  std::map<const ConfigItem*, Handle> handles;
  std::map<Handle, const ConfigItem*> objects_;
  static constexpr Handle kSlot = 0x100, kConfig = 0x200, kStore = 0x300, kDfn = 0x400, kScreen = 0x500;
  uint32_t unported(const char* key, std::initializer_list<Arg> args) override {
    (void)args;
    events->push_back(std::string("unported ") + key);
    return 0;
  }
  Handle lab_slot(Handle, int32_t) override { return kSlot; }
  Handle lab_slot_cfg(Handle) override { return kConfig; }
  Config* lab_config_of(Handle h) override { return h == kConfig ? cfg : nullptr; }
  Store* store_of(Handle h) override { return h == kStore ? store : nullptr; }
  Handle item_handle(const ConfigItem* it) override {
    auto f = handles.find(it);
    if (f != handles.end()) return f->second;
    const Handle h = 0x10000 + static_cast<Handle>(handles.size());
    handles[it] = h;
    objects_[h] = it;
    return h;
  }
  const ConfigItem* item_of(Handle h) override {
    auto f = objects_.find(h);
    return f == objects_.end() ? nullptr : f->second;
  }
  Handle mech_dfn_handle(int32_t) override { return kDfn; }
  std::vector<LabFirepoint> lab_dfn_firepoints(Handle h) override { return h == kDfn ? fps : std::vector<LabFirepoint>{}; }
  const char* message(int32_t id) override {
    kept.push_back("MSG_" + std::to_string(id));
    return kept.back().c_str();
  }
  std::string message_format_sd(int32_t id, const char* s, int32_t n, bool) override {
    char b[128];
    std::snprintf(b, sizeof b, "F%x:%s,%d", static_cast<uint32_t>(id), s ? s : "", n);
    return std::string(b).substr(0, 0x45);
  }
  void lab_message(const char* text) override { events->push_back(std::string("message ") + text); }
  int32_t lab_assigned_part() override {
    events->push_back("part");
    return part;
  }
  void lab_page_refresh(Handle h) override { events->push_back(h == kScreen ? "refresh True" : "refresh False"); }
  void lab_put_on(LabUnassignedList& list, int32_t index) override { mech3::lab_put_on(*this, list, index); }
  void lab_take_off(LabAssignedList& list, int32_t index) override { mech3::lab_take_off(*this, list, index); }
};

struct TakeOffList : LabAssignedList {
  std::vector<std::string>* events = nullptr;
  explicit TakeOffList(Engine& e) : LabAssignedList(e) {}
  void on_enter() override { events->push_back("enter"); }
  void on_leave() override { events->push_back("leave"); }
};

struct PutOnList : LabUnassignedList {
  std::vector<std::string>* events = nullptr;
  int32_t rule = 1;
  explicit PutOnList(Engine& e) : LabUnassignedList(e) {}
  void on_enter() override { events->push_back("enter"); }
  void on_leave() override { events->push_back("leave"); }
  bool can_xa8(int32_t index) override {
    events->push_back("rule " + std::to_string(index));
    return rule != 0;
  }
};

struct PutOnHost : ztwod::Host {
  std::vector<std::string>* events = nullptr;
  int32_t sound_play(int32_t sound, float volume) override {
    char b[64];
    std::snprintf(b, sizeof b, "sound %d %x", sound, zgame::bits(volume));
    events->push_back(b);
    return 0;
  }
};

std::string armour_text(const Config& c) {
  char b[64];
  std::snprintf(b, sizeof b, "armour %g t%d f%d ", static_cast<double>(c.armor_tons), c.armor_tech, int(c.ferro));
  std::string s = b;
  for (int i = 0; i < 11; ++i) s += (i ? "," : "") + std::to_string(c.armor[i]);
  return s;
}
}  // namespace

// test_salvage.py's puton: the mech lab's put-on (0x00454600) on the test's
// loadout and store, the list's rows the store's items each time.
extern "C" int probe_puton_run(const char* setup, char* out, int size) {
  std::istringstream ss(setup);
  std::vector<std::string> events;
  PutOnEngine e;
  e.events = &events;
  PutOnHost host;
  host.events = &events;
  ztwod::set_host(&host);
  Store store(0);
  e.store = &store;
  std::unique_ptr<Config> cfg;
  std::ostringstream o;
  // The mech's definition as the test's: zeroed, its firepoints (+0x24) only.
  static std::vector<Firepoint> fp_store;
  static std::vector<Firepoint*> fp_ptrs;
  fp_store.clear();
  fp_ptrs.clear();
  MechDfn* dfn = static_cast<MechDfn*>(::operator new(sizeof(MechDfn)));
  std::memset(static_cast<void*>(dfn), 0, sizeof(MechDfn));
  int32_t dfn_mech = -1;
  {
    PutOnList list(e);
    list.events = &events;
    TakeOffList alist(e);
    alist.events = &events;
    alist.sounds[0] = 7, alist.sounds[1] = 8;
    list.store = PutOnEngine::kStore;
    list.sounds[0] = 7, list.sounds[1] = 8;
    LabState& lab = states().lab;
    lab.pilot_ = 1, lab.slot_ = 0, lab.lab_screen_ = PutOnEngine::kScreen, lab.store_ = PutOnEngine::kStore;
    std::string line;
    while (std::getline(ss, line)) {
      std::istringstream ls(line);
      std::string what;
      ls >> what;
      if (what == "mech") {
        int32_t m, flag, toggle, full;
        ls >> m >> flag >> toggle >> full;
        lab.flag_ = static_cast<uint8_t>(flag);
        cfg.reset(make_config(m, "slot", {}));
        store.full = static_cast<uint8_t>(full);
        e.cfg = cfg.get();
      } else if (what == "config") {
        cfg.reset(make_config(cfg->mech, "slot", read_items(ls)));
        e.cfg = cfg.get();
      } else if (what == "sections") {
        for (int32_t& v : cfg->armor) ls >> v;
      } else if (what == "fps") {
        int32_t n;
        ls >> n;
        for (int32_t i = 0; i < n; ++i) {
          int32_t part;
          uint32_t bits;
          ls >> part >> std::hex >> bits >> std::dec;
          e.fps.push_back({part, zgame::from_bits(bits)});
          fp_store.push_back(Firepoint{nullptr, zgame::from_bits(bits), 0, part});
        }
        for (Firepoint& f : fp_store) fp_ptrs.push_back(&f);
        dfn->firepoints = &fp_ptrs;
        dfn_mech = cfg->mech;
        mech_dfn_slot(dfn_mech) = dfn;
      } else if (what == "store") {
        std::string op;
        ls >> op;
        if (op == "mech") {
          int32_t t;
          ls >> t;
          store.add_mech(t);
        } else if (op == "weapon") {
          int32_t t, tech;
          ls >> t >> tech;
          store.add_weapon(t, tech);
        } else if (op == "ammo") {
          int32_t t, n;
          ls >> t >> n;
          store.add_ammo(t, n);
        } else if (op == "equip") {
          int32_t k, tech, n;
          ls >> k >> tech >> n;
          store.add_equipment(k, tech, n);
        }
      } else if (what == "take") {
        int32_t row;
        ls >> row;
        for (LabItem* it : alist.items) delete it;
        alist.items.clear();
        auto* head = new LabItem;
        head->object = 1;  // a part row
        alist.items.push_back(head);
        for (const ConfigItem* it : cfg->items) {
          auto* li = new LabItem;
          li->object = e.item_handle(it);
          alist.items.push_back(li);
        }
        events.clear();
        const int32_t n = static_cast<int32_t>(alist.items.size()) - 1;
        const int32_t m = std::max(1, n);
        alist.select(1 + ((row % m) + m) % m);  // (Python's modulo, as the test's)
        std::string ev;
        for (const std::string& x : events) ev += (ev.empty() ? "" : " ") + x;
        o << ev << '\n';
        o << armour_text(*cfg) << ' ' << config_text(cfg.get()) << '\n';
        o << "store{n" << store.count() << ' ';
        for (const ConfigItem* it : store.items) o << item_text(it);
        o << "}\n";
      } else if (what == "put") {
        int32_t row, rule, part;
        ls >> row >> rule >> part;
        for (LabItem* it : list.items) delete it;
        list.items.clear();
        for (const ConfigItem* it : store.items) {
          auto* li = new LabItem;
          li->object = e.item_handle(it);
          list.items.push_back(li);
        }
        list.rule = rule;
        e.part = part;
        events.clear();
        list.select(row);
        std::string ev;
        for (const std::string& x : events) ev += (ev.empty() ? "" : " ") + x;
        o << ev << '\n';
        o << armour_text(*cfg) << ' ' << config_text(cfg.get()) << '\n';
        o << "store{n" << store.count() << ' ';
        for (const ConfigItem* it : store.items) o << item_text(it);
        o << "}\n";
      }
    }
    for (LabItem* it : list.items) delete it;
    list.items.clear();
    for (LabItem* it : alist.items) delete it;
    alist.items.clear();
  }
  ztwod::set_host(nullptr);
  if (dfn_mech >= 0) mech_dfn_slot(dfn_mech) = nullptr;
  ::operator delete(dfn);
  const std::string str = o.str();
  if (static_cast<int>(str.size()) >= size) return -1;
  std::memcpy(out, str.data(), str.size() + 1);
  return static_cast<int>(str.size());
}

// test_pm_puton.py: the put-on on a real loadout and store, one put at a time.
namespace {
struct RuleEngine : PutOnEngine {
  float lab_config_weight(Handle) override { return static_cast<float>(cfg->weight(-1)); }
  int32_t config_mech(Handle) override { return cfg->mech; }
  float lab_config_armor_tons(Handle) override { return cfg->armor_tons; }
  int32_t lab_config_armor_tech(Handle) override { return cfg->armor_tech; }
  uint8_t lab_config_armor_ferro(Handle) override { return cfg->ferro; }
  int32_t lab_config_slots_left(Handle) override { return cfg->slots_left(); }
  int32_t lab_config_ferro_slots(Handle) override { return cfg->ferro_slots(); }
  int32_t lab_config_weapons(Handle) override { return cfg->weapon_count(); }
  int32_t lab_config_equipment(Handle, int32_t kind, int32_t tech) override { return cfg->equipment_count(kind, tech); }
  int32_t lab_config_part_free(Handle, int32_t part) override {
    return part >= -1 && part < 9 ? cfg->free_slots_in(part) : 0;
  }
  int32_t lab_config_sink_limit(Handle) override { return cfg->sink_limit(); }
  int32_t lab_config_sink_items(Handle) override { return cfg->sink_items(); }
  std::vector<Handle> lab_config_items(Handle) override {
    std::vector<Handle> out;
    for (const ConfigItem* it : cfg->items) out.push_back(item_handle(it));
    return out;
  }
};

struct PutOnRun {
  std::vector<std::string> events;
  RuleEngine e;
  PutOnHost host;
  Store store{0};
  std::unique_ptr<PutOnList> list;
};
PutOnRun* g_put = nullptr;
}  // namespace

extern "C" void probe_pm_puton_begin(void* config, int flag, int full, int n, const int32_t* parts, const float* radii) {
  delete g_put;
  g_put = new PutOnRun;
  PutOnRun& r = *g_put;
  r.e.events = &r.events, r.host.events = &r.events;
  ztwod::set_host(&r.host);
  r.e.store = &r.store, r.e.cfg = static_cast<Config*>(config);
  for (int i = 0; i < n; ++i) r.e.fps.push_back({parts[i], radii[i]});
  r.store.full = static_cast<uint8_t>(full);
  r.list = std::make_unique<PutOnList>(r.e);
  r.list->events = &r.events;
  r.list->store = PutOnEngine::kStore;
  r.list->sounds[0] = 7, r.list->sounds[1] = 8;
  LabState& lab = states().lab;
  lab.pilot_ = 1, lab.slot_ = 0, lab.lab_screen_ = PutOnEngine::kScreen, lab.store_ = PutOnEngine::kStore;
  lab.flag_ = static_cast<uint8_t>(flag);
}

extern "C" void probe_pm_puton_store(int kind, int a, int b, int c) {
  Store& s = g_put->store;
  if (kind == 1) s.add_weapon(a, b);
  else if (kind == 2) s.add_ammo(a, b);
  else s.add_equipment(a, b, c);
}

extern "C" int probe_pm_puton_put(int row, int rule, int part, uint32_t seed, char* out, int size) {
  PutOnRun& r = *g_put;
  crt::msvc_srand(seed);
  for (LabItem* it : r.list->items) delete it;
  r.list->items.clear();
  for (const ConfigItem* it : r.store.items) {
    auto* li = new LabItem;
    li->object = r.e.item_handle(it);
    r.list->items.push_back(li);
  }
  r.list->rule = rule;
  r.e.part = part;
  r.events.clear();
  r.list->select(row);
  std::ostringstream o;
  for (const std::string& x : r.events) o << x << ' ';
  const Config& c = *r.e.cfg;
  o << '\n' << armour_text(c) << " m" << int(c.modified) << '\n';
  for (const ConfigItem* it : c.items) o << item_text(it);
  o << '\n';
  for (const ConfigItem* it : r.store.items) o << item_text(it);
  o << '\n';
  const std::string str = o.str();
  if (static_cast<int>(str.size()) >= size) return -1;
  std::memcpy(out, str.data(), str.size() + 1);
  return static_cast<int>(str.size());
}

extern "C" void probe_pm_puton_tons(float tons) { g_put->e.cfg->armor_tons = tons; }
extern "C" void probe_pm_puton_engine(int engine) { g_put->e.cfg->engine = engine; }

extern "C" int probe_pm_puton_rule(int row, int part, int armor_tab) {
  PutOnRun& r = *g_put;
  if (states().lab.flag_) return 0;
  const Handle item = row >= 0 && row < static_cast<int>(r.store.items.size())
                          ? r.e.item_handle(*std::next(r.store.items.begin(), row))
                          : 0;
  return lab_item_fits(r.e, PutOnEngine::kConfig, part, item, PutOnEngine::kDfn, armor_tab != 0);
}

extern "C" void probe_pm_puton_end() {
  if (!g_put) return;
  for (LabItem* it : g_put->list->items) delete it;
  g_put->list->items.clear();
  ztwod::set_host(nullptr);
  delete g_put;
  g_put = nullptr;
}

// test_pm_puton.py's chassis rows (0x0044a660): the empty row by game type and shown slot.
namespace {
struct ChassisEngine : PutOnEngine {
  int32_t type = 0;
  Handle pilot_campaign(Handle) override { return type == kCampaign ? 1 : 0; }
  bool pilot_standalone() override { return type == kStandalone; }
  int32_t lab_slot_mech(Handle) override { return 0; }
  uint32_t peek32(uint32_t a) override { return a == 0x60861c ? 1 : 0; }  // the current pilot
};
}  // namespace

extern "C" int probe_pm_chassis_rows(int type, int slot) {
  std::vector<std::string> events;
  ChassisEngine e;
  e.events = &events, e.type = type;
  PutOnHost host;
  host.events = &events;
  ztwod::set_host(&host);
  LabState& lab = states().lab;
  lab.pilot_ = 1, lab.slot_ = slot;
  int rows;
  {
    LabChassisList list(e);
    list.refill();
    rows = list.item_count();
    list.clear_items();
  }
  ztwod::set_host(nullptr);
  return rows;
}
