// The pilot's games, their slots and stores (docs/pilot.md, The games).
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gamez/znetwork/znet_session.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsave/zsave.h"
#include "gamez/zutil/zutl_msg.h"
#include "gamez/zutil/zutl_msgdll.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/mech_config.h"
#include "mech3/mech_ids.h"
#include "mech3/mech_names.h"
#include "mech3/mech_objective.h"
#include "mech3/mech_reader.h"
#include "mech3/options.h"
#include "mech3/pilot.h"
#include "mech3/mission.h"

namespace mech3 {

namespace {
// ReadFile and WriteFile with their counts ignored, as the exe does: a short
// read leaves the rest of the field as it was.
void rd(std::FILE* f, void* p, size_t n) {
  if (f) std::fread(p, 1, n, f);
}
void wr(std::FILE* f, const void* p, size_t n) {
  if (f) std::fwrite(p, 1, n, f);
}

class DefaultPilotHooks : public PilotHooks {};
DefaultPilotHooks g_default_hooks;
PilotHooks* g_hooks = &g_default_hooks;
ActionSetup g_action_setup;
}  // namespace

void set_pilot_hooks(PilotHooks* h) { g_hooks = h ? h : &g_default_hooks; }
PilotHooks& pilot_hooks() { return *g_hooks; }
// Pirate's Moon keeps these in the pilot's Instant Action game (+0x38..+0x84); MW3 in globals.
ActionSetup& action_setup() {
  Pilot* p = current_pilot();
  return p && p->action_game ? p->action_game->setup : g_action_setup;
}

Config* PilotHooks::config_load(const char* name, std::FILE* f) { return Config::from_file(name, f); }
void PilotHooks::save_world(const char* path, int32_t kind) { zsave::save(path, kind); }
bool PilotHooks::load_world(const char* path, int32_t kind) { return zsave::load(path, kind); }
void PilotHooks::config_save(Config* c, std::FILE* f) {
  if (c) c->save(f);
}
const char* PilotHooks::message(int32_t id) {
  const zutl::Messages& m = zutl::messages();
  const char* s = m.get_text ? m.get_text(id) : nullptr;
  return s ? s : "";
}
// 0x00575fd0: FormatMessageA with the numbers as its inserts (%1!d!, ...).
std::string PilotHooks::format_ints(int32_t id, std::initializer_list<int32_t> args) {
  return zutl::format_message_ints(id, args);
}

// Mech3.exe 1.2 @ 0x00484840: operations 1 and 2 have four missions, 3 and 4 six.
// Mech3.exe 1.2 @ 0x004733b0: the dfn's loadout by that name into the store
// (0x00473400, mech and items); a full store takes it as it is.
void PilotHooks::store_fill(Store& s, int32_t) { s.fill_all(); }

int32_t PilotHooks::store_add_mech(Store& s, int32_t mech, const char* config) {
  if (s.full) return 1;
  const MechDfn* d = mech_dfn(mech);
  const Config* c = d && d->configs ? d->configs->find(config) : nullptr;
  return c ? (s.give_config(mech, c) ? 1 : 0) : 0;
}

int32_t PilotHooks::campaign_missions(int32_t campaign) {
  switch (campaign) {
    case 1: case 2: return 4;
    case 3: case 4: return 6;
    default: return 0;
  }
}
Config* PilotHooks::config_copy(const Config& c) { return Config::copy_of(c.name, c); }
void PilotHooks::config_recount(Config& c) { c.recount(); }

// ---- the store ----

// Mech3.exe 1.2 @ 0x00470ce0
Store::Store(uint8_t flag) : b04(flag) {
  if (flag) full = 1;
}

// Mech3.exe 1.2 @ 0x00470d20
Store::~Store() { clear(); }

void Store::clear() {
  for (ConfigItem*& i : items) delete i, i = nullptr;
  items.clear();
}

// Mech3.exe 1.2 @ 0x00470e30: emptied (the exe drops the list's nodes, its
// items leaked), then one of each, unlimited (+0x10): every mech but the
// Elemental; every weapon in each tech it comes in (Inner Sphere
// first) and its ammo when it takes some; every equipment kind, armour in
// any tech (3). FIX (bug) with the plain PPC (weapon_count).
void Store::fill_all() {
  clear();
  full = 1;
  auto put = [this](ConfigItem* it) {
    it->count = 1;
    it->b10 = 1;
    items.push_back(it);
  };
  for (int32_t m = 0; m < kMechCount; ++m) {  // Pirate's Moon lists the Elemental (0x004d5020 is always 0)
    auto* b = new BaseItem;
    b->value = m;
    put(b);
  }
  for (int32_t w = 0; w < weapon_count(); ++w) {
    if (equipment_restricted(-1, w, 3)) continue;
    for (int32_t tech : {2, 1}) {
      if (!weapon_has_tech(w, tech)) continue;
      auto* it = new WeaponItem;
      it->weapon = w;
      it->tech = tech;
      put(it);
    }
    if (weapon_uses_ammo(w)) {
      auto* a = new AmmoItem;
      a->weapon = w;
      put(a);
    }
  }
  for (int32_t k = 0; k < 0xf; ++k) {
    if (equipment_restricted(-1, k, 3)) continue;
    for (int32_t tech : k == 1 ? std::initializer_list<int32_t>{3} : std::initializer_list<int32_t>{2, 1}) {
      if (tech != 3 && !equip_has_tech(k, tech)) continue;
      auto* it = new EquipmentItem;
      it->equipment = k;
      it->tech = tech;
      put(it);
    }
  }
}

// Mech3.exe 1.2 @ 0x004714c0
ConfigItem* Store::find_like(const ConfigItem* item) {
  for (ConfigItem* i : items)
    if (same_item(i, item)) return i;
  return nullptr;
}

// Mech3.exe 1.2 @ 0x00471460
ConfigItem* Store::find(const ConfigItem* item) { return find_item(items, item); }

// Mech3.exe 1.2 @ 0x00472630. A copy of `item`'s kind, id, tech and group
// with `n` of it, or `n` more of the one like it (its place reset: 0 for a
// weapon, else -1). The count never goes below 0.
ConfigItem* Store::add_copy(const ConfigItem* item, int32_t n) {
  if (!item) return nullptr;
  if (ConfigItem* found = find(item)) {
    found->count += n;
    if (found->count < 0) found->count = 0;
    found->part = found->kind() == 1 ? 0 : -1;
    return found;
  }
  ConfigItem* copy = nullptr;
  switch (item->kind()) {
    case 0: {
      auto* c = new BaseItem;
      c->value = item->id();
      copy = c;
      break;
    }
    case 1: {
      auto* c = new WeaponItem;
      c->weapon = static_cast<const WeaponItem*>(item)->weapon;
      c->tech = static_cast<const WeaponItem*>(item)->tech;
      copy = c;
      break;
    }
    case 2: {
      auto* c = new AmmoItem;
      c->weapon = static_cast<const AmmoItem*>(item)->weapon;
      copy = c;
      break;
    }
    case 3: {
      auto* c = new EquipmentItem;
      c->equipment = static_cast<const EquipmentItem*>(item)->equipment;
      c->tech = static_cast<const EquipmentItem*>(item)->tech;
      copy = c;
      break;
    }
    case 4: {
      auto* c = new EngineItem;
      c->engine = static_cast<const EngineItem*>(item)->engine;
      c->xl = static_cast<const EngineItem*>(item)->xl;
      copy = c;
      break;
    }
    default: return nullptr;  // no such item exists (the exe would crash here)
  }
  copy->count = n;
  copy->group = item->group;
  items.push_back(copy);
  return copy;
}

// Mech3.exe 1.2 @ 0x00471940
AmmoItem* Store::add_ammo(int32_t weapon, int32_t n) {
  AmmoItem probe;
  probe.weapon = weapon;
  if (auto* found = static_cast<AmmoItem*>(find_like(&probe))) {
    found->count += n;
    if (found->count < 0) found->count = 0;
    return found;
  }
  auto* a = new AmmoItem;
  a->count = n;
  a->weapon = weapon;
  items.push_back(a);
  return a;
}

// Mech3.exe 1.2 @ 0x00471a20. Kind 1's tech is always 3 (any).
EquipmentItem* Store::add_equipment(int32_t kind, int32_t tech, int32_t n) {
  if (kind == 1) tech = 3;
  EquipmentItem probe;
  probe.count = 1, probe.equipment = kind, probe.tech = tech;
  if (auto* found = static_cast<EquipmentItem*>(find_like(&probe))) {
    found->count += n;
    if (found->count < 0) found->count = 0;
    return found;
  }
  auto* e = new EquipmentItem;
  e->count = n, e->equipment = kind, e->tech = tech;
  items.push_back(e);
  return e;
}

// Mech3.exe 1.2 @ 0x00471b20: one more (the XL flag only for a new one).
EngineItem* Store::add_engine(int32_t engine, uint8_t xl) {
  EngineItem probe;
  probe.count = 1, probe.engine = engine, probe.xl = xl;
  if (auto* found = static_cast<EngineItem*>(find_like(&probe))) {
    if (++found->count < 0) found->count = 0;
    return found;
  }
  auto* e = new EngineItem;
  e->count = 1, e->engine = engine, e->xl = xl;
  items.push_back(e);
  return e;
}

// Mech3.exe 1.2 @ 0x00471840: one more, its place cleared.
WeaponItem* Store::add_weapon(int32_t weapon, int32_t tech) {
  WeaponItem probe;
  probe.count = 1, probe.weapon = weapon, probe.tech = tech;
  if (auto* found = static_cast<WeaponItem*>(find_like(&probe))) {
    if (++found->count < 0) found->count = 0;
    found->part = -1;
    return found;
  }
  auto* w = new WeaponItem;
  w->count = 1, w->weapon = weapon, w->tech = tech;
  items.push_back(w);
  return w;
}

// Mech3.exe 1.2 @ 0x00471d20. A store's own (b10) items are never taken.
bool Store::remove(ConfigItem* item, int32_t n) {
  if (!item || n <= 0) return false;
  ConfigItem* found = find(item);
  if (!found || found->b10) return false;
  found->count -= n;
  if (found->count < 0) found->count = 0;
  if (found->count != 0) return false;
  items.remove(found);
  delete found;
  return true;
}

// Mech3.exe 1.2 @ 0x00472a30
void Store::remove_mech(int32_t mech) {
  BaseItem probe;
  probe.value = mech;
  remove(find_like(&probe), 1);
}

// Mech3.exe 1.2 @ 0x004732b0
void Store::add_all(const Store& from) {
  for (ConfigItem* i : from.items) add_copy(i, i->count);
}

// Mech3.exe 1.2 @ 0x00473300
void Store::assign(const Store& from) {
  clear();
  add_all(from);
}

// Mech3.exe 1.2 @ 0x00471c10. Merged with one like it (its place kept, unlike
// add_copy's); none for -1 or one past the roster.
BaseItem* Store::add_mech(int32_t mech) {
  if (mech == -1 || mech == kMechCount) return nullptr;
  BaseItem probe;
  probe.count = 1, probe.value = mech;
  if (auto* found = static_cast<BaseItem*>(find_like(&probe))) {
    if (++found->count < 0) found->count = 0;
    return found;
  }
  auto* b = new BaseItem;
  b->count = 1, b->value = mech;
  items.push_back(b);
  return b;
}

// Mech3.exe 1.2 @ 0x00473510: the counts of the items like `item`.
int32_t Store::count_like(const ConfigItem* item) const {
  if (!item) return 0;
  int32_t n = 0;
  for (const ConfigItem* it : items)
    if (same_item(it, item)) n += it->count;
  return n;
}

// Mech3.exe 1.2 @ 0x00472d40
bool Store::remove_config(const Config* c) {
  if (!c) return false;
  for (ConfigItem* it : c->items) remove(it, it->count);
  return true;
}

// Mech3.exe 1.2 @ 0x00409f00
ConfigItem* Store::has_mech(int32_t mech) {
  BaseItem probe;
  probe.count = 1, probe.value = mech;
  return find_like(&probe);
}

// Mech3.exe 1.2 @ 0x004735f0. The shortfall is logged (0x0049bfd0, a no-op).
bool Store::has_config(int32_t mech, const Store* other, const Config* c) {
  // The exe compares `other` with the loadout (0x00473614), never equal.
  if (!c || full || static_cast<const void*>(other) == static_cast<const void*>(c)) return true;
  if (mech != -1 && !has_mech(mech)) return false;
  for (const ConfigItem* it : c->items) {
    if (!it) continue;
    int32_t have = count_like(it);
    if (other) have += other->count_like(it);
    if (have < it->count) return false;
  }
  return true;
}

// The same with a loadout as `other` (the mech lab's 0x00449fb0 passes the
// slot's: its items count as the store's, as the exe reads both item lists
// at +0x08 alike); the same loadout is always there.
bool Store::has_config(int32_t mech, const Config* other, const Config* c) {
  if (!c || full || other == c) return true;
  if (mech != -1 && !has_mech(mech)) return false;
  for (const ConfigItem* it : c->items) {
    if (!it) continue;
    int32_t have = count_like(it);
    if (other)
      for (const ConfigItem* o : other->items)
        if (same_item(o, it)) have += o->count;
    if (have < it->count) return false;
  }
  return true;
}

// Mech3.exe 1.2 @ 0x00473440
bool Store::take_config(int32_t mech, const Config* c) {
  if (full) return true;
  if (mech != -1) remove_mech(mech);
  return remove_config(c);
}

// Mech3.exe 1.2 @ 0x00473400
bool Store::give_config(int32_t mech, const Config* c) {
  if (full) return true;
  if (mech != -1) add_mech(mech);
  return add_config(c);
}

// Mech3.exe 1.2 @ 0x00424010
Config* config_copy_store(const char* name, const Config& c, Store* store, bool take) {
  return Config::copy_of(name, c, [&](Config& d) {
    if (!store) return;
    if (!store->has_config(-1, static_cast<const Store*>(nullptr), &d)) {
      // Pirate's Moon 1.0 @ 0x00425970, "Incomplete Configuration": each item takes from a copy of the store, in
      // turn, so two of a kind share it (MW3's each saw it whole), and is cut to what's left. Armour plate a
      // loadout can't have comes off its tons, half a ton a piece, and the points are spread again. Then the
      // emptied items dropped; unlike MW3 the copy isn't marked modified.
      Store left;
      left.add_all(*store);
      for (ConfigItem* it : d.items) {
        if (!it) continue;
        const int32_t want = it->count, have = left.count_like(it);
        left.remove(it, want);
        if (have >= want) continue;
        const int32_t short_by = want - have;
        it->count = want - short_by < 0 ? 0 : want - short_by;
        if (it->kind() == 3) {
          const int32_t e = static_cast<EquipmentItem*>(it)->equipment;
          if (e == 1 || e == 2) {
            d.armor_tons = d.armor_tons - static_cast<float>(short_by) * 0.5f;
            if (d.armor_tons < 0.0f) d.armor_tons = 0.0f;
            d.spread_armor(d.mech);
          }
        }
      }
      for (auto i = d.items.begin(); i != d.items.end();) {
        if (*i && (*i)->count == 0) {
          delete *i;  // the exe leaks it (fixes.md)
          i = d.items.erase(i);
        } else {
          ++i;
        }
      }
      const int32_t sink_tech = c.sink_tech;
      const uint8_t doubles = c.double_sinks;
      d.sinks = d.sink_items() + 10;
      d.sink_tech = sink_tech;
      d.double_sinks = doubles;
    }
    if (take) store->take_config(-1, &d);
  });
}

// Mech3.exe 1.2 @ 0x00472cf0
bool Store::add_config(const Config* c) {
  if (!c) return false;
  for (ConfigItem* i : c->items) add_copy(i, i->count);
  return true;
}

// Mech3.exe 1.2 @ 0x00471790 -> 0x004717a0
uint32_t Store::save(std::FILE* f) const { return save_item_list(items, full, f); }

// Mech3.exe 1.2 @ 0x00471530 -> 0x00471540
void Store::load(std::FILE* f) { load_item_list(items, f); }

// ---- the slot ----

// Mech3.exe 1.2 @ 0x00433cc0
GameSlot::GameSlot(int32_t m, Config* c, int32_t s) : mech(m), config(c), skin(s) {}

// Mech3.exe 1.2 @ 0x00433d60
GameSlot::~GameSlot() {
  delete config;
  config = nullptr;
}

// Mech3.exe 1.2 @ 0x00433d90. A modified loadout stays marked modified in the copy.
void GameSlot::copy_from(const GameSlot& s) {
  mech = s.mech;
  skin = s.skin;
  x0c = 0;
  x14 = s.x14;
  set_config(s.config);
  if (config && s.config && s.config->modified) {
    config->modified = 1;
    pilot_hooks().config_recount(*config);
  }
}

// Mech3.exe 1.2 @ 0x00433df0
bool GameSlot::set_config(const Config* c, Store* store, bool take) {
  if (config == c) return true;
  if (c && c->mech != mech) return false;
  if (store && !store->full && take) {
    if (config) store->give_config(-1, config);
    delete config;
    config = c ? config_copy_store(c->name, *c, store, take) : nullptr;
    return true;
  }
  return set_config(c);
}

// Mech3.exe 1.2 @ 0x00433df0 with no store (the copy's only caller): the
// slot's loadout replaced by a copy of `c`, or by none; a loadout of another
// mech is refused (false) and the old one kept.
bool GameSlot::set_config(const Config* c) {
  if (config == c) return true;
  if (c && c->mech != mech) return false;
  delete config;
  config = c ? pilot_hooks().config_copy(*c) : nullptr;
  return true;
}

// Mech3.exe 1.2 @ 0x00433ef0
void GameSlot::save(Pilot& p) {
  wr(p.file, &mech, 4);
  wr(p.file, &x14, 4);
  wr(p.file, &skin, 4);
  const int32_t has = config != nullptr;
  wr(p.file, &has, 4);
  if (has) pilot_hooks().config_save(config, p.file);
}

// Mech3.exe 1.2 @ 0x00433f80. A configuration read replaces the one held
// without deleting it (a leak in the exe; deleted here).
void GameSlot::load(Pilot& p) {
  rd(p.file, &mech, 4);
  rd(p.file, &x14, 4);
  rd(p.file, &skin, 4);
  int32_t has = 0;
  rd(p.file, &has, 4);
  if (has) {
    Config* c = pilot_hooks().config_load("temp", p.file);
    delete config;
    config = c;
  }
}

// ---- the games ----

Game::Game(Pilot* p) : pilot(p), slot(5, nullptr, 0) {}  // PM 0x00434780(5, 0, 0); MW3 3

// Mech3.exe 1.2 @ 0x00434560
void Game::save() {
  slot.save(*pilot);
  store.save(pilot->file);
}

// Mech3.exe 1.2 @ 0x00434580
void Game::load() {
  slot.load(*pilot);
  store.load(pilot->file);
}

// Mech3.exe 1.2 @ 0x004373e0
LabGame::LabGame(Pilot* p) : Game(p) {
  pilot_hooks().store_fill(store, kMechLab);
  lineup[0].mech = 5;  // Pirate's Moon 0x004381d0: the first only (MW3: mechs 3, 8 and 13)
}

// Mech3.exe 1.2 @ 0x004374d0
LabGame::~LabGame() = default;

// Pirate's Moon 1.0 @ 0x004351b0 (MW3's 0x004345a0). Operation 1, mission 1 gets the starting store (pilot.md, A new campaign).
CampaignGame::CampaignGame(Pilot* p, int32_t c, int32_t m) : Game(p), mission(m), campaign(c) {
  (void)c;
  if (m == 1) {  // the mission alone
    PilotHooks& h = pilot_hooks();
    // Pirate's Moon 0x004351b0: a Centurion, a Puma and an Owens, spare guns, sinks and ammunition.
    h.store_add_mech(store, 7, h.message(0xf20));
    h.store_add_mech(store, 0x11, h.message(0xf20));
    h.store_add_mech(store, 0x10, h.message(0xf20));
    for (int i = 0; i < 4; ++i) store.add_weapon(0x1f, kInnerSphere);  // MGUN
    for (int i = 0; i < 2; ++i) store.add_weapon(5, kInnerSphere);     // ER medium laser
    for (int i = 0; i < 4; ++i) store.add_equipment(10, kInnerSphere, 1);
    store.add_ammo(0x1f, 1000);
    store.add_ammo(0x17, 500);
    store.add_ammo(0x18, 1000);
    store.add_ammo(0x15, 100);
  }
}

// Mech3.exe 1.2 @ 0x00434820. The fields after +0xa0 are the file's (0 here
// before it, heap garbage in the exe).
CampaignGame::CampaignGame(Pilot* p) : Game(p), a8(0), ac(0) { load(); }

// Mech3.exe 1.2 @ 0x00434740, with 0x00435240(1)
CampaignGame::~CampaignGame() {
  for (GameSlot*& s : lancemates) delete s, s = nullptr;
  lancemates.clear();
}

// Pirate's Moon 1.0 @ 0x004356f0: no operation, and the strings at the end.
void CampaignGame::save() {
  Game::save();
  std::FILE* f = pilot->file;
  wr(f, &mission, 4);
  wr(f, &b38, 1);
  wr(f, &a8, 4);
  wr(f, &ac, 4);
  wr(f, &c4, 4);
  wr(f, b4, 0x10);
  store3c.save(f);
  store64.save(f);
  store50.save(f);
  store78.save(f);
  const int32_t n = static_cast<int32_t>(lancemates.size());
  wr(f, &n, 4);
  for (GameSlot* s : lancemates) s->save(*pilot);  // 0x00434b10
  const int32_t k = static_cast<int32_t>(strings.size());
  wr(f, &k, 4);
  for (const std::string& t : strings) {  // 0x00597210
    const int32_t len = static_cast<int32_t>(t.size());
    wr(f, &len, 4);
    if (len) wr(f, t.data(), static_cast<size_t>(len));
  }
}

// Pirate's Moon 1.0 @ 0x00435820. FIX (bug): a damaged count reads on past the
// file's end, making a slot each time (up to 4 billion: a hang); here the
// reading stops at the end (docs/fixes.md).
void CampaignGame::load() {
  Game::load();
  std::FILE* f = pilot->file;
  rd(f, &mission, 4);
  campaign = campaign_world(mission) + 1;
  rd(f, &b38, 1);
  rd(f, &a8, 4);
  rd(f, &ac, 4);
  rd(f, &c4, 4);
  rd(f, b4, 0x10);
  store3c.load(f);
  store64.load(f);
  store50.load(f);
  store78.load(f);
  uint32_t n = 0;
  rd(f, &n, 4);
  if (n) {
    if (lancemates.capacity() < n && n < 0x10000) lancemates.reserve(n);
    for (; n; --n) {  // 0x00434040
      auto* s = new GameSlot(-1, nullptr, 0);
      s->load(*pilot);
      lancemates.push_back(s);
      if (f && std::feof(f)) break;
    }
  }
  int32_t k = 0;
  rd(f, &k, 4);
  for (; k != 0 && !std::feof(f); --k) {  // 0x00597280
    int32_t len = 0;
    rd(f, &len, 4);
    if (len < -1) break;  // FIX (bug): a damaged length; the exe clears 4 GB and faults
    // -1 mallocs nothing and reads nothing: empty. FIX (bug): no more is taken than the file has, as the
    // exe reads, without its allocation of the whole damaged length.
    std::string t;
    if (len > 0) {  // only here: the seek clears a short read's end of file
      const long at = std::ftell(f);
      std::fseek(f, 0, SEEK_END);
      const long left = std::ftell(f) - at;
      std::fseek(f, at, SEEK_SET);
      t.resize(static_cast<size_t>(len < left ? len : left));
      if (!t.empty()) rd(f, t.data(), t.size());
      if (len > left) std::fgetc(f);  // at the end, as the exe's short read leaves it
    }
    t.resize(std::strlen(t.c_str()));  // a CString in the exe: to the first zero
    strings.push_back(std::move(t));
  }
  merge_salvage();
}

// Pirate's Moon 1.0 @ 0x00435d80: a campaign record is kept when its mission enters
// a fresh world, or its saved world is there.
bool CampaignGame::keeps() { return campaign_fresh_world(mission) || readable(); }

StandaloneGame::~StandaloneGame() {
  for (auto& l : lineups)
    for (GameSlot*& s : l) delete s, s = nullptr;
}

// Pirate's Moon 1.0 @ 0x0040e250
void StandaloneGame::save() {
  Game::save();
  std::FILE* f = pilot->file;
  wr(f, &x38, 4);
  wr(f, &x3c, 4);
  const int32_t n = static_cast<int32_t>(lineups.size());  // 0x0040dc00
  wr(f, &n, 4);
  for (const auto& l : lineups) {
    const int32_t k = static_cast<int32_t>(l.size());  // 0x00434b10
    wr(f, &k, 4);
    for (GameSlot* s : l) s->save(*pilot);
  }
}

// Pirate's Moon 1.0 @ 0x0040e2b0. FIX (bug): a damaged count stops at the file's end.
void StandaloneGame::load() {
  Game::load();
  std::FILE* f = pilot->file;
  rd(f, &x38, 4);
  rd(f, &x3c, 4);
  int32_t n = 0;  // 0x0040dc70
  rd(f, &n, 4);
  for (int32_t i = 0; i < n; ++i) {
    lineups.emplace_back();
    int32_t k = 0;  // 0x00434b70
    rd(f, &k, 4);
    for (; k != 0; --k) {
      auto* s = new GameSlot(-1, nullptr, 0);
      s->load(*pilot);
      lineups.back().push_back(s);
      if (std::feof(f)) break;
    }
    if (std::feof(f)) break;
  }
}

// Mech3.exe 1.2 @ 0x00434cb0. What the last mission left in two stores is
// added to the campaign's own; those and the one at +0x64 are emptied.
void CampaignGame::merge_salvage() {
  store.add_all(store50);
  store.add_all(store78);
  a8 = ac;
  store50.clear();
  store64.clear();
  store78.clear();
}

// Mech3.exe 1.2 @ 0x00434e00 (vt+0x24). With `next`, the game of the mission
// after this one (null when the campaign is over): this one's stores, slot,
// record and lancemates carried over, the mission's salvage its +0x3c, the
// world saved into its file; this one's salvage then merged. Without, this one
// settled in place: the slots' mechs and loadouts listed in +0x50 (docs/campaign.md 4).
CampaignGame* CampaignGame::next_mission(bool next_one, bool save) {
  PilotHooks& h = pilot_hooks();
  int32_t m = mission_number(), op = operation();
  const bool over = campaign_advance(op, m);
  if (next_one) {
    if (over) {  // Pirate's Moon 1.0 @ 0x004359e0: the salvage merged all the same
      merge_salvage();
      return nullptr;
    }
    auto* g = new CampaignGame(pilot, op, m);
    g->store.assign(store);
    g->store.add_all(store64);
    std::memcpy(g->b4, b4, sizeof b4);
    if (Store* salvage = h.mission_salvage()) g->store3c.assign(*salvage);
    else g->store3c.clear();  // [0x00609990] is always set in the exe
    g->slot.copy_from(slot);
    g->a8 = a8;  // Pirate's Moon 1.0 @ 0x004359e0: no resets
    g->c4 = c4;
    g->copy_lancemates(*this);
    if (save && !campaign_fresh_world(m)) h.save_world(g->path(), 2);
    b0 = 0;
    g->b0 = 1;
    merge_salvage();
    return g;
  }
  store64.clear();
  store50.clear();
  for (int32_t i = 0; i < slot_count(); ++i) {
    GameSlot* s = slot_at(i);
    if (s && s->mech != -1) {
      store50.add_mech(s->mech);
      store50.add_config(s->config);
    }
  }
  ac = a8;
  store78.clear();
  b0 = 0;
  return this;
}

// Mech3.exe 1.2 @ 0x00435110
GameSlot* CampaignGame::slot_at(int32_t i) {
  if (i == 0) return &slot;
  const auto n = static_cast<uint32_t>(lancemates.size());
  if (n != 0 && i > 0 && static_cast<uint32_t>(i) <= n) return lancemates[static_cast<size_t>(i) - 1];
  return nullptr;
}

// Mech3.exe 1.2 @ 0x004351c0
void CampaignGame::set_lancemate(int32_t i, GameSlot* s) {
  if (static_cast<size_t>(i) == lancemates.size()) {
    lancemates.push_back(s);  // 0x0049ced0
    return;
  }
  lancemates[static_cast<size_t>(i)]->x14 = s->x14;
  delete s;  // 0x00433d60
}

// Mech3.exe 1.2 @ 0x00435180: the player and the lance's first +0x9c
// lancemates (unsigned: a negative +0x9c takes them all).
int32_t CampaignGame::slot_count() const {
  const auto n = static_cast<uint32_t>(lancemates.size());
  const auto want = static_cast<uint32_t>(x9c);
  return static_cast<int32_t>((n < want ? n : want) + 1);
}

// Mech3.exe 1.2 @ 0x00435240: with `del` the lancemates deleted (0x00434270),
// else their +0x14 cleared.
void CampaignGame::reset_lancemates(bool del) {
  if (!del) {
    for (GameSlot* s : lancemates) s->x14 = 0;
    return;
  }
  for (GameSlot*& s : lancemates) delete s, s = nullptr;
  lancemates.clear();
}

// Mech3.exe 1.2 @ 0x00434300: the old ones deleted, a copy of each (0x00433cf0).
void CampaignGame::copy_lancemates(const CampaignGame& from) {
  reset_lancemates(true);
  for (const GameSlot* s : from.lancemates) {
    auto* c = new GameSlot(-1, nullptr, 0);
    c->copy_from(*s);
    lancemates.push_back(c);
  }
}

// Mech3.exe 1.2 @ 0x00434950
const char* CampaignGame::path_for(const char* pilot_name) {
  char buf[0x200];
  std::snprintf(buf, sizeof buf, "pilots//games//%s%02d.mw3", pilot_name, mission_number());  // PM 0x00435680
  text = buf;
  return text.c_str();
}

// Mech3.exe 1.2 @ 0x00434980
const char* CampaignGame::path() { return path_for(pilot->name); }

// Mech3.exe 1.2 @ 0x00435330 (0x00576080: _access)
bool CampaignGame::readable() { return zutl::exists(path()); }

// Mech3.exe 1.2 @ 0x004352d0, Pirate's Moon 1.0 @ 0x00435d30: a game read from
// the pilot's file (not the one the last mission's end made, +0xb0), whose
// file is there: zSave reads it (kind 2) into the world. FIX (bug): never for a
// mission on a fresh world. The exe writes no world for those, but the port's
// builds before 2026-11-08 did (record_mission), and that world, the last
// mission's, read into the new one took the fixed turrets' damage handlers
// (pm-tests item 9).
bool CampaignGame::restore() {
  if (b0 || !readable() || campaign_fresh_world(mission)) return false;
  return pilot_hooks().load_world(path(), 2);
}

// Mech3.exe 1.2 @ 0x00434900, Pirate's Moon 1.0 @ 0x00435640: message 0x21d,
// PM's "Mission %1!d!".
const char* CampaignGame::name() {
  text = pilot_hooks().format_ints(0x21d, {mission_number()});
  return text.c_str();
}

// Mech3.exe 1.2 @ 0x00436650
ActionGame::ActionGame(Pilot* p) : Game(p) {
  store.full = 1;
  pilot_hooks().store_fill(store, kInstantAction);
}

// Mech3.exe 1.2 @ 0x00436770
ActionGame::~ActionGame() {
  std::free(x38), std::free(x3c), std::free(x40);
}

// Mech3.exe 1.2 @ 0x004368b0
GameSlot* ActionGame::slot_at(int32_t i) {
  if (i == 0) return &slot;
  return i > 0 && i < 11 ? &lineup[i - 1] : nullptr;
}

// Mech3.exe 1.2 @ 0x00436a90
bool ActionGame::ally(int32_t i) const {
  if (i == 0) return true;
  return i > 0 && i < 11 && side[i - 1] != 0;
}

// Mech3.exe 1.2 @ 0x00436af0
bool ActionGame::remove(int32_t i) {
  if (i < 0 || i > count) return false;
  for (; i < count - 1; ++i) {
    lineup[i].copy_from(lineup[i + 1]);  // 0x00433d90
    side[i] = side[i + 1];
  }
  --count;
  return true;
}

// Mech3.exe 1.2 @ 0x00436ac0: from the last slot down, each enemy's out.
// It asks slot i's side (lineup[i - 1]) but removes lineup[i]: with the
// allies first, as the screen keeps them, that is the same slot or the one
// past the end (docs/mission-start.md section 13).
void ActionGame::drop_enemies() {
  for (int32_t i = count; i >= 0; --i)
    if (!ally(i)) remove(i);
}

// Pirate's Moon 1.0 @ 0x0040c070: the block's lists, one a wave, each of (mech name, loadout) pairs; a
// name not on the roster is -1 (0x00486730). The exe reads a pair's two values unchecked; a shorter pair
// here keeps -1 and 0 (FIX (bug), a mod's line).
void ActionGame::read_progression(const zrdr::Node* block) {
  if (!block || block->type != zrdr::List) return;
  const int32_t n = zrdr::list_count(block) - 1;
  progression.resize(n > 0 ? static_cast<size_t>(n) : 0);
  for (int32_t w = 0; w < n; ++w) {
    const zrdr::Node& wave = block->list[w + 1];
    const int32_t m = wave.type == zrdr::List ? zrdr::list_count(&wave) - 1 : 0;
    std::vector<WaveSlot>& slots = progression[static_cast<size_t>(w)];
    slots.assign(m > 0 ? static_cast<size_t>(m) : 0, WaveSlot{-1, 0});
    for (int32_t i = 0; i < m; ++i) {
      const zrdr::Node& pair = wave.list[i + 1];
      if (pair.type != zrdr::List || zrdr::list_count(&pair) < 3) continue;
      const zrdr::Node& name = pair.list[1];
      slots[static_cast<size_t>(i)].mech = name.type == zrdr::String && name.s ? mech_index(name.s) : -1;
      slots[static_cast<size_t>(i)].config = pair.list[2].type == zrdr::Int ? pair.list[2].i : 0;
    }
  }
}

// Pirate's Moon 1.0 @ 0x0040c410: the world's block (vt+0x54, the world's folder: c1..), else "default".
void ActionGame::load_progression() {
  zrdr::Node* root = zrdr::load("ia_progression.zrd");
  if (!root) return;  // (the exe looks in nothing)
  zrdr::Node* block = zrdr::find(root, mission_dir(x74));
  if (!block) block = zrdr::find(root, "default");
  read_progression(block);
  zrdr::free_node(root);
}

// Pirate's Moon 1.0 @ 0x0040c390
void ActionGame::set_mode(int32_t m) {
  mode = m;
  if (m == 2) load_progression();
}

// Pirate's Moon 1.0 @ 0x0040c360
void ActionGame::set_world(int32_t world, int32_t mission) {
  x74 = world;
  x78 = mission;
  if (mode == 2) load_progression();
}

// Pirate's Moon 1.0 @ 0x0040c260
int32_t ActionGame::last_wave() const { return static_cast<int32_t>(progression.size()) - 1; }

// Pirate's Moon 1.0 @ 0x0040c280
int32_t ActionGame::wave_slots(int32_t w) const {
  return w >= 0 && static_cast<size_t>(w) < progression.size() ? static_cast<int32_t>(progression[static_cast<size_t>(w)].size()) : 0;
}

// Pirate's Moon 1.0 @ 0x0040c2c0 / 0x0040c300: the wave is checked, the slot not. FIX (bug): the slot too
// (a mod's wave asked past its end read on into the heap).
int32_t ActionGame::wave_mech(int32_t w, int32_t i) const {
  return i >= 0 && i < wave_slots(w) ? progression[static_cast<size_t>(w)][static_cast<size_t>(i)].mech : -1;
}
int32_t ActionGame::wave_config(int32_t w, int32_t i) const {
  return i >= 0 && i < wave_slots(w) ? progression[static_cast<size_t>(w)][static_cast<size_t>(i)].config : 0;
}

// Pirate's Moon 1.0 @ 0x0040c3b0
bool ActionGame::waves_done() const { return mode == 2 && last_wave() < x6c - 1; }

// Pirate's Moon 1.0 @ 0x0040c3e0
int32_t ActionGame::best_wave() const { return last_wave() < waves ? last_wave() : waves; }

// Mech3.exe 1.2 @ 0x00436840
GameSlot* ActionGame::add(int32_t mech, uint8_t s, const Config* config, int32_t paint) {
  if (count > 9) return nullptr;
  side[count] = s;
  GameSlot& g = lineup[count++];
  g.x0c = 0;          // 0x00433ee0
  g.mech = mech;      // 0x005a2ba0
  g.set_config(config);
  g.skin = paint;     // 0x005a2b90
  return &g;
}

// Mech3.exe 1.2 @ 0x004368e0, 0x00436950, 0x004369c0. FIX (bug): always
// reallocated (the exe keeps an old buffer for a text of up to 4 bytes,
// whatever the old buffer's size).
void ActionGame::set_text(char** field, const char* text) {
  if (!text) text = "";
  const size_t n = std::strlen(text) + 1;
  if (void* b = std::realloc(*field, n)) {
    *field = static_cast<char*>(b);
    std::memcpy(*field, text, n);
  }
}

// Mech3.exe 1.2 @ 0x00436b50
void ActionGame::save() {
  Game::save();
  std::FILE* f = pilot->file;
  const int32_t stats[8] = {player_kills, lancemate_kills, enemy_kills, attack_wins,
                            attack_losses, defend_wins, defend_losses, waves};
  for (int32_t v : stats) wr(f, &v, 4);
  ActionSetup& a = setup;
  wr(f, &a.g874, 4);
  wr(f, &a.g878, 4);
  wr(f, &a.g87c, 4);
  for (int32_t& v : a.g820) wr(f, &v, 4);
  for (int32_t& v : a.g838) wr(f, &v, 4);  // Pirate's Moon: the two lists whole, not in turn
  for (int32_t& v : a.g858) wr(f, &v, 4);
}

// Mech3.exe 1.2 @ 0x00436cc0
void ActionGame::load() {
  Game::load();
  std::FILE* f = pilot->file;
  int32_t* stats[8] = {&player_kills, &lancemate_kills, &enemy_kills, &attack_wins,
                       &attack_losses, &defend_wins, &defend_losses, &waves};
  for (int32_t* v : stats) rd(f, v, 4);
  ActionSetup& a = setup;
  rd(f, &a.g874, 4);
  rd(f, &a.g878, 4);
  rd(f, &a.g87c, 4);
  for (int32_t& v : a.g820) rd(f, &v, 4);
  for (int32_t& v : a.g838) rd(f, &v, 4);  // Pirate's Moon: the two lists whole, not in turn
  for (int32_t& v : a.g858) rd(f, &v, 4);
}

// Mech3.exe 1.2 @ 0x004353a0
MultiplayerGame::MultiplayerGame(Pilot* p) : Game(p) {}  // Pirate's Moon 0x00435e80: no fill, the lobby's

// Mech3.exe 1.2 @ 0x00435500
MultiplayerGame::~MultiplayerGame() = default;

// Mech3.exe 1.2 @ 0x00435690
void MultiplayerGame::save() {
  Game::save();
  std::FILE* f = pilot->file;
  // Pirate's Moon 1.0 @ 0x004361c0: +0xb4, +0xb8, +0x78, +0x98, +0x70, +0x74 (no more than 10), +0x7c, +0xa4..+0xa8,
  // +0xc4, +0xac, +0xb0, +0xa9, +0x80, +0xcc.
  wr(f, &kills, 4);
  wr(f, &deaths, 4);
  wr(f, &x70, 4);
  wr(f, &kind, 4);
  wr(f, &realloc, 4);
  const int32_t rounds = rounds_total > 9 ? 10 : rounds_total;
  wr(f, &rounds, 4);
  wr(f, &x74, 4);
  wr(f, &b8c, 1);
  wr(f, &b8d, 1);
  wr(f, &b8e, 1);
  wr(f, &jets_assist, 1);
  wr(f, &radar, 1);
  wr(f, &ba8, 1);
  wr(f, &x90, 4);
  wr(f, &x94, 4);
  wr(f, &b8f, 1);
  wr(f, &x78, 4);
  wr(f, &pmcc, 4);
}

// Mech3.exe 1.2 @ 0x004357d0
void MultiplayerGame::load() {
  Game::load();
  std::FILE* f = pilot->file;
  rd(f, &kills, 4);  // Pirate's Moon 1.0 @ 0x004363b0, 0x004361c0's order
  rd(f, &deaths, 4);
  rd(f, &x70, 4);
  rd(f, &kind, 4);
  rd(f, &realloc, 4);
  rd(f, &rounds_total, 4);
  rd(f, &x74, 4);
  rd(f, &b8c, 1);
  rd(f, &b8d, 1);
  rd(f, &b8e, 1);
  rd(f, &jets_assist, 1);
  rd(f, &radar, 1);
  rd(f, &ba8, 1);
  rd(f, &x90, 4);
  rd(f, &x94, 4);
  rd(f, &b8f, 1);
  rd(f, &x78, 4);
  rd(f, &pmcc, 4);
}

// Mech3.exe 1.2 @ 0x00437920, 0x00436500, 0x00437950, 0x00437930
const char* MultiplayerGame::game_text(int32_t slot) {
  switch (slot) {
    case 0x04: return pilot_hooks().message(0x10d);
    case 0x08: {
      if (x94 > 2) {  // an add-on's: the reader's mpinfo.zrd ([0x00605850] holds it)
        static std::string name;
        zrdr::Node* info = zrdr::load("mpinfo.zrd");
        const char* s = zrdr::get_string(info, "db_name");
        name = s ? s : "";
        if (info) zrdr::free_node(info);
        return name.c_str();
      }
      if (x90 >= 0 && x90 <= 3 && (x94 == 1 || x94 == 2)) return pilot_hooks().message(0x320 + x90 * 2 + x94 - 1);
      return pilot_hooks().message(0xd75);  // "Unknown"
    }
    case 0x0c: return znet::session().desc.name.c_str();
    case 0x10: return roster.c_str();
    default: return nullptr;
  }
}

// Mech3.exe 1.2 @ 0x00436470
void MultiplayerGame::set_roster(const std::vector<std::string>& lines) {
  roster.clear();
  for (const std::string& l : lines) roster += l + "\n";
}

// ---- the lobby's records (multiplayer-port.md, piece 11) ----

// Mech3.exe 1.2 @ 0x00495b10
LobbyPlayer* MultiplayerGame::lobby_player(uint32_t id) {
  for (auto& p : lobby_players)
    if (p->id == id) return p.get();
  return nullptr;
}

// Mech3.exe 1.2 @ 0x00495b60
uint32_t MultiplayerGame::lobby_team_id(uint32_t player) {
  const LobbyPlayer* p = lobby_player(player);
  return p ? p->team : 0;
}

// Mech3.exe 1.2 @ 0x00495bb0
int32_t MultiplayerGame::lobby_skin(uint32_t player) {
  const LobbyPlayer* p = lobby_player(player);
  return p ? p->skin : 0;
}

// Mech3.exe 1.2 @ 0x00495c00
bool MultiplayerGame::in_team(uint32_t player, uint32_t team) {
  for (auto& p : lobby_players)
    if (p->id == player && p->team == team) return true;
  return false;
}

// Mech3.exe 1.2 @ 0x00495d20
LobbyTeam* MultiplayerGame::lobby_team(uint32_t id) {
  for (auto& t : lobby_teams)
    if (t->id == id) return t.get();
  return nullptr;
}

// Mech3.exe 1.2 @ 0x00495a50, 0x00495c60, 0x00495d70
void MultiplayerGame::clear_lobby() {
  lobby_players.clear();
  lobby_teams.clear();
  score_lines.clear();
}

// Pirate's Moon 1.0 @ 0x00436600 (MW3's 0x00435910): in team play the
// team's third data word (+0x18; MW3 its skin, +0x10), else the player's order.
uint32_t MultiplayerGame::net_team(uint32_t player) {
  if (!multiplayer_team_play()) {
    const LobbyPlayer* p = lobby_player(player);
    return p ? static_cast<uint32_t>(p->order) : 0;
  }
  const LobbyTeam* t = lobby_team(lobby_team_id(player));
  return t ? static_cast<uint32_t>(t->word2) : 0;
}

// Mech3.exe 1.2 @ 0x00435960
int32_t MultiplayerGame::net_skin(uint32_t player) {
  if (!multiplayer_team_play()) return lobby_skin(player);
  const LobbyTeam* t = lobby_team(lobby_team_id(player));
  return t ? t->skin : 0;
}

// Mech3.exe 1.2 @ 0x004359b0
int32_t MultiplayerGame::add_kills(uint32_t player, int32_t n) {
  LobbyPlayer* p = lobby_player(player);
  if (!p) return -1;
  p->kills += n;
  rescore();
  return p->kills;
}

// Mech3.exe 1.2 @ 0x004359f0
int32_t MultiplayerGame::set_kills(uint32_t player, int32_t n) {
  LobbyPlayer* p = lobby_player(player);
  if (!p) return -1;
  p->kills = n;
  rescore();
  return p->kills;
}

// Mech3.exe 1.2 @ 0x00435a30
int32_t MultiplayerGame::kills_of(uint32_t player) {
  const LobbyPlayer* p = lobby_player(player);
  return p ? p->kills : -1;
}

// Mech3.exe 1.2 @ 0x00435a50
int32_t MultiplayerGame::score_of(uint32_t player) {
  if (!multiplayer_team_play()) return kills_of(player) - deaths_of(player);
  rescore();
  const LobbyTeam* t = lobby_team(lobby_team_id(player));
  return t && t->score ? t->score->score : -1;
}

// Pirate's Moon 1.0 @ 0x00436820 (MW3's 0x00435ac0): a suicide counts only while the player's kills exceed its
// deaths, so it never takes a score below 0; 0 for none or not counted.
int32_t MultiplayerGame::add_deaths(uint32_t player, int32_t n) {
  LobbyPlayer* p = lobby_player(player);
  if (!p || p->kills - p->deaths < 0 || p->kills == p->deaths) return 0;
  p->deaths += n;
  rescore();
  return p->deaths;
}

// Pirate's Moon 1.0 @ 0x004379b0 (ECX the network game): a player out of the round or back in, unless players
// respawn (+0xa4); its record's old mark (out with none).
bool MultiplayerGame::set_out(uint32_t player, bool out) {
  if (b8c) return false;
  LobbyPlayer* p = lobby_player(player);
  const bool was = p ? p->out != 0 : true;
  if (p) p->out = out ? 1 : 0;
  return was;
}

// Pirate's Moon 1.0 @ 0x00437a10: a round's scores from who is left in it, 1 kill and no deaths each, 0 kills out.
void MultiplayerGame::round_reset() {
  for (auto& p : lobby_players) {
    if (!p->out) p->kills = 1, p->deaths = 0;
    else p->kills = 0;
  }
}

// Mech3.exe 1.2 @ 0x00435b00
int32_t MultiplayerGame::set_deaths(uint32_t player, int32_t n) {
  LobbyPlayer* p = lobby_player(player);
  if (!p) return -1;
  p->deaths = n;
  rescore();
  return p->deaths;
}

// Mech3.exe 1.2 @ 0x00435b40
int32_t MultiplayerGame::deaths_of(uint32_t player) {
  const LobbyPlayer* p = lobby_player(player);
  return p ? p->deaths : -1;
}

namespace {
uint32_t floor0(int32_t kills, int32_t deaths) {
  const int32_t s = kills - deaths;
  return s < 0 ? 0u : static_cast<uint32_t>(s);
}
}  // namespace

// Pirate's Moon 1.0 @ 0x004374f0 (MW3's 0x004361e0): each score kills less
// suicides, not under 0; a team's the sum of its players'. The order key puts
// each team's line over its players' (the +1), the teams by score, then the
// players. PM: the key's low bits are a player's lobby order (MW3's skin) or a
// team's third data word, and with `rounds` (every caller but the tally's first
// pass, 0x00436980) the rounds won (4 bits) sort above the score.
void MultiplayerGame::rescore(bool rounds) {
  if (!multiplayer_team_play()) {  // PM [0x0062cc34]
    for (auto& p : lobby_players) {
      if (!p->score) continue;  // the exe reads +0x34 unchecked
      const uint32_t s = floor0(p->kills, p->deaths);
      const uint32_t won = rounds ? (static_cast<uint32_t>(p->rounds) & 0xf) * 0x1000000 : 0;
      p->score->score = static_cast<int32_t>(s);
      p->score->key = ((static_cast<uint32_t>(p->order) & 7) + (won + (s & 0xfff)) * 8) * 2;
    }
    return;
  }
  for (auto& t : lobby_teams) {
    if (!t->score) continue;
    t->score->score = 0;
    t->kills = 0;
    for (auto& p : lobby_players)
      if (p->team == t->id) {
        t->kills += p->kills;
        t->score->score += static_cast<int32_t>(floor0(p->kills, p->deaths));
      }
  }
  for (auto& t : lobby_teams) {
    if (!t->score) continue;
    const uint32_t ts = static_cast<uint32_t>(t->score->score) & 0xfff, low = static_cast<uint32_t>(t->word2) & 7;
    const uint32_t top = (rounds ? (static_cast<uint32_t>(t->rounds) & 0xf) * 0x1000 : 0) + ts;
    t->score->key = (low + (top * 0x1000 + ts) * 8) * 2 + 1;
    for (auto& p : lobby_players)
      if (p->team == t->id && p->score) {
        const uint32_t s = floor0(p->kills, p->deaths);
        p->score->score = static_cast<int32_t>(s);
        p->score->key = (low + (top * 0x1000 + (s & 0xfff)) * 8) * 2;
      }
  }
}

// Pirate's Moon 1.0 @ 0x004384f0: a place's (0 first, 7 last) number times
// the scale, unsigned; a negative scale reads the table from the other end.
float RoundRules::reward(int32_t place, float scale) const {
  if (scale == 0.0f) return 0.0f;  // [0x005de7b4]
  if (scale < 0.0f) return std::fabs(scale * table[7 - place]);
  return std::fabs(scale * table[place]);
}

// Pirate's Moon 1.0 @ 0x00438460: "table" when a list of 8, "salvage" into
// the rules' store.
bool read_round_block(RoundRules& r, zrdr::Node* block) {
  if (!block) return false;
  zrdr::Node* t = zrdr::find(block, "table");
  if (t && t->type == zrdr::List && zrdr::list_count(t) == 9)
    for (int32_t k = 0; k < 8; ++k) zrdr::get_float_value(&t->list[1 + k], &r.table[k]);  // 0x00578fc0
  if (zrdr::Node* v = zrdr::find(block, "salvage")) round_salvage(r.salvage, v);  // 0x00473690
  return true;
}
void (*round_salvage)(Store& s, zrdr::Node* salvage) = salvage_into;

// Pirate's Moon 1.0 @ 0x00438380
void read_round_rules(RoundRules& r, zrdr::Node* root, const char* fallback, int32_t round) {
  static const float kDefault[8] = {70, 50, 40, 35, 30, 25, 20, 15};  // [0x0060554c]
  std::memcpy(r.table, kDefault, sizeof r.table);
  char key[32];
  std::snprintf(key, sizeof key, "round%d", round);
  if (!read_round_block(r, root ? zrdr::find(root, key) : nullptr))
    read_round_block(r, root ? zrdr::find(root, fallback) : nullptr);
}
void read_round_rules(RoundRules& r, const char* fallback, int32_t round) {
  zrdr::Node* root = zrdr::load("mprounds.zrd");  // 0x00578630
  read_round_rules(r, root, fallback, round);
  if (root) zrdr::free_node(root);  // 0x005787a0
}
void (*round_rules_read)(RoundRules& r, const char* fallback, int32_t round) = read_round_rules;

// Pirate's Moon 1.0 @ 0x00438580: between rounds (not after the last), the
// next round's rules: the local player's place (scaled to 0..7 by the
// players: x7 with 2, x4 with 3, x2 with 4 or 5) earns its reward, a
// REALLOC_LIST-scaled number, added to the bonus tons; the round's salvage
// goes into the round store; the weight limit grows by the first place's
// reward, down to a multiple of 5, kept within 10..100; then the store's
// mechs over the new limit are taken out.
void MultiplayerGame::next_round(uint32_t place, int32_t round) {
  if (round == rounds_total) return;
  const float scale = static_cast<float>(realloc) * 0.5f;  // [0x005de818]
  RoundRules rules;
  round_rules_read(rules, "default", round + 1);  // 0x00438380
  const size_t players = lobby_players.size();  // +0x50
  if (players == 2) place *= 7;
  else if (players == 3) place <<= 2;
  else if (players == 4 || players == 5) place *= 2;
  if (place > 7) place = 7;
  bonus_tons += rules.reward(static_cast<int32_t>(place), scale);
  if (!rules.salvage.items.empty()) round_store.add_all(rules.salvage);  // 0x00475270
  int32_t tons = static_cast<int32_t>(rules.reward(0, std::fabs(scale))) + x78;  // ftol: truncates
  tons -= tons % 5;
  x78 = tons > 100 ? 100 : tons < 10 ? 10 : tons;
  const float limit = static_cast<float>(x78);
  for (auto it = round_store.items.begin(); it != round_store.items.end();) {  // 0x00438790
    ConfigItem* item = *it;
    if (item->kind() == 0 && limit < max_mass(item->id())) {
      delete item;
      it = round_store.items.erase(it);
    } else {
      ++it;
    }
  }
}

bool jets_assist_of(const MultiplayerGame* g) {
  if (g) return g->jets_assist != 0;  // 0x004365f0
  return mw2_jump_jets() == 0;        // 0x00435070
}

// Pirate's Moon 1.0 @ 0x00437a40: 24 (remote_23 on, as MW3's) but in the
// base kinds: the player's team's side (its third data word's bit 0) and the
// side whose base is base 1 (+0xb0, vt+0x48) pick 0, 8 or 16 (one base: side
// 1 at 16; two bases: each side at the 8 of its base).
int32_t MultiplayerGame::spawn_base(uint32_t player) {
  if (kind > 1) {
    if (const LobbyTeam* t = lobby_team(lobby_team_id(player))) {  // 0x00498800, 0x00498930
      const bool side = (static_cast<uint32_t>(t->word2) & 1) != 0;
      if (kind == 4) return side ? (x94 == 1 ? 8 : 0) : (x94 == 1 ? 0 : 8);
      if (kind == 3) return side ? 16 : (x94 == 1 ? 0 : 8);
    }
  }
  return 24;
}

// Pirate's Moon 1.0 @ 0x00437920: in the base kinds, two bases: either;
// otherwise base 1 or 2 only when it is the one vt+0x48 (+0xb0) names.
bool MultiplayerGame::base_counts(int32_t base) {
  if (kind <= 1) return false;
  if (kind == 4) return true;
  if (base == 1) return x94 == 1;
  if (base == 2) return x94 == 2;
  return false;
}

// Pirate's Moon 1.0 @ 0x00472b40 (the game's half; 0x004e2f50 calls it with
// the base): over kind 1, a base that counts ends the round, "Base
// Destroyed" (6), its number kept (+0x8c), the clock sent now (+0x84).
bool MultiplayerGame::base_destroyed(int32_t base, float now) {
  if (kind <= 1 || !base_counts(base)) return false;
  round_end = 6, base_lost = base, clock_next = now;
  return true;
}

// Pirate's Moon 1.0 @ 0x00436650
LobbyTeam* MultiplayerGame::team_numbered(int32_t number) {
  for (auto& t : lobby_teams)
    if (t->word2 == number) return t.get();
  return nullptr;
}

// Pirate's Moon 1.0 @ 0x0049ef20 (the game's half): no score limit at 1000 or
// over (0x7fffffff), a missing pick 0; the boxes into +0xa4..+0xa9 and +0xc4
// and the rules' flags (1 respawn, 2 mobile, 4 stock, 8 ammo, 0x10 outside, 0x80
// jets, 0x100 radar, or'd on); the world's low byte its folder (+0xac), its
// second its number (+0xb0); the clock on under 1000 minutes.
void MultiplayerGame::apply_lobby(const LobbySettings& s, bool* clock_on, float* clock_left) {
  const auto val = [](const LobbyPick& p) { return p.has ? p.value : 0; };
  x70 = s.kills.has && s.kills.value >= 1000 ? 0x7fffffff : val(s.kills);  // +0x78
  b8c = s.respawn, b8d = s.ammo, b8e = s.outside, jets_assist = s.jets, radar = s.radar, b8f = s.stock;
  ba8 = s.mobile;
  x78 = val(s.tons);                                     // +0x80
  x74 = static_cast<float>(val(s.minutes));              // +0x7c
  realloc = val(s.realloc), rounds_total = val(s.rounds), kind = val(s.kind);
  rules.world = static_cast<int16_t>(val(s.world));      // +0x64..+0x6a
  rules.kills = static_cast<int16_t>(val(s.kills));
  rules.time = static_cast<int16_t>(val(s.minutes));
  rules.weight = static_cast<int16_t>(val(s.tons));
  rules.flags |= (s.respawn ? 1u : 0u) | (s.mobile ? 2u : 0u) | (s.stock ? 4u : 0u) | (s.ammo ? 8u : 0u) |
                 (s.outside ? 0x10u : 0u) | (s.jets ? 0x80u : 0u) | (s.radar ? 0x100u : 0u);
  const float minutes = static_cast<float>(val(s.minutes));
  *clock_on = minutes < 1000.0f;  // [0x005e29b0]
  *clock_left = *clock_on ? minutes * 60.0f : 0.0f;
  const uint32_t w = static_cast<uint32_t>(val(s.world));
  x90 = static_cast<int32_t>(w & 0xff);         // +0xac
  x94 = static_cast<int32_t>((w >> 8) & 0xff);  // +0xb0
}

// Pirate's Moon 1.0 @ 0x0049ef20's message, the host's to all, guaranteed:
// 0x1e, then the players' count and each one's id, order and team number
// (0x00436600); MW3's 0x0049b790 sent the 8 ids by order.
std::vector<uint8_t> MultiplayerGame::start_message() {
  const uint32_t n = static_cast<uint32_t>(lobby_players.size());
  std::vector<uint32_t> w(2 + 3 * n, 0);
  w[0] = 0x1e | (8 + 12 * n) << 16;
  w[1] = n;
  size_t k = 2;
  for (auto& p : lobby_players)
    w[k++] = p->id, w[k++] = static_cast<uint32_t>(p->order), w[k++] = net_team(p->id);
  std::vector<uint8_t> b(4 * w.size());
  std::memcpy(b.data(), w.data(), b.size());
  return b;
}

// Pirate's Moon 1.0 @ 0x0049f510 (the handler: sender, message): the count, then each
// player's id, order and team number; a listed player takes its order (the
// team number isn't read). FIX (A): only as far as the message goes.
void MultiplayerGame::read_start_message(const uint8_t* msg, size_t size) {
  if (size < 8) return;
  uint32_t n;
  std::memcpy(&n, msg + 4, 4);
  for (uint32_t k = 0; k < n && 8 + 12 * static_cast<size_t>(k + 1) <= size; ++k) {
    uint32_t w[2];
    std::memcpy(w, msg + 8 + 12 * k, 8);
    if (LobbyPlayer* p = lobby_player(w[0])) p->order = static_cast<int32_t>(w[1]);  // 0x00498930
  }
}

// Pirate's Moon 1.0 @ 0x0049e400's rules: with no score and no time limit
// (1000 each) one base can't play and anything but two bases asks first;
// with either limit two bases can't; the base kinds need respawn; then the
// start asked for.
LobbyRule check_lobby_rules(const LobbySettings& s, bool (*confirm)(int32_t text), bool* team_play) {
  const auto is = [](const LobbyPick& p, int32_t v) { return p.has && p.value == v; };
  if (is(s.kills, 1000) && is(s.minutes, 1000)) {
    if (is(s.kind, 3)) return LobbyRule::one_base_unlimited;
    if (!is(s.kind, 4) && !confirm(0xfbd)) return LobbyRule::declined;  // 0x004afb30(1, 0x1e3, ...)
  } else if (is(s.kind, 4)) {
    return LobbyRule::two_bases_limited;
  }
  if (s.kind.has && s.kind.value > 1 && !s.respawn) return LobbyRule::no_respawn;
  if (!confirm(0x2c4)) return LobbyRule::declined;
  *team_play = s.kind.has && s.kind.value != 0;  // [0x0062cc34]
  return LobbyRule::start;
}

// Pirate's Moon 1.0 @ 0x0049ca00: READY once locked and placed (a team in
// team play, a skin outside it); MECHLAB and OPTIONS once locked (in team
// play with a team) until ready; SALVAGE between rounds until ready; START
// the host's when everyone is; ready: the lists' choices and their buttons
// off; between rounds no skin or team change; NEW_TEAM the host's in team
// play under 8 teams (2 in the Attack/Defend kinds, 3 on), not when ready
// or between rounds. MW3's 0x004993f0 had no SALVAGE or OPTIONS, MECHLAB
// for any locked player and NEW_TEAM to 8 always.
LobbyButtons lobby_buttons(const LobbyView& v) {
  LobbyButtons b;
  const bool placed = v.locked && (v.team_play ? v.my_team != 0 : v.my_skin != 0);
  const bool in = v.locked && (!v.team_play || v.my_team != 0) && !v.ready;
  b.ready = placed, b.mechlab = in, b.salvage = v.between_rounds && !v.ready, b.options = in;
  b.start = v.host && v.all_ready;
  if (v.ready) {
    b.deselect_teams = b.deselect_players = true;
    b.pick_skin = b.join = b.remove = b.kick = b.kick2 = 0;
  }
  if (v.between_rounds) b.pick_skin = b.join = 0;
  if (v.team_play && v.host) {
    const int32_t kind = v.kind.has ? v.kind.value : 0;
    b.new_team = v.teams < (kind < 3 ? 8 : 2) && !v.ready && !v.between_rounds;
  }
  return b;
}

// Pirate's Moon 1.0 @ 0x0049dc50: ready, the choice undone; else JOIN, REMOVE,
// KICK and PICK_SKIN off, then for a team (kept for PICK_SKIN) the host's
// PICK_SKIN (1 to 7 teams) and REMOVE (any team: 0x0049df10 takes its players
// out first; MW3's only an empty one), JOIN when not in it, not between
// rounds; for a player the host's KICK when it isn't the host.
LobbyButtons lobby_team_chosen(const LobbyView& v, int32_t kind, uint32_t id, bool me_in) {
  LobbyButtons b;
  if (kind == 0) return b;
  if (v.ready) {
    b.deselect_teams = true;
    return b;
  }
  b.join = b.remove = b.kick = b.pick_skin = 0;
  if (kind == 2) {
    b.chose_team = true;
    if (v.host && !v.between_rounds) b.pick_skin = v.teams >= 1 && v.teams <= 7;
    if (!me_in && !v.between_rounds) b.join = 1;
    if (v.host) b.remove = 1;
  } else if (kind == 1 && v.host && id != v.me) {
    b.kick = 1;
  }
  return b;
}

// Pirate's Moon 1.0 @ 0x0049dfd0: ready, the choice undone; else KICK2 and
// PICK_SKIN off, then for a player the host's KICK2 when it isn't the host,
// or outside team play our own PICK_SKIN once locked and not between rounds
// (MW3's at any time); any other player chosen, the choice undone.
LobbyButtons lobby_player_chosen(const LobbyView& v, int32_t kind, uint32_t id) {
  LobbyButtons b;
  if (kind == 0) return b;
  if (v.ready) {
    b.deselect_players = true;
    return b;
  }
  b.kick2 = b.pick_skin = 0;
  if (kind != 1) return b;
  if (v.host && id != v.me) {
    b.kick2 = 1;
    return b;
  }
  if (!v.team_play && id == v.me && v.locked && !v.between_rounds) {
    b.pick_skin = 1;
    return b;
  }
  b.deselect_players = true;
  return b;
}

// Pirate's Moon 1.0 @ 0x00491a1e: "Pilot Statistics", a blank line, the
// kills, the shots and, with shots, the kills per thousand shots (x87:
// kills x 1000.0f / shots, a double for "%6.2f").
std::vector<std::string> multiplayer_stat_lines(int32_t kills, int32_t shots) {
  std::vector<std::string> out = {"Pilot Statistics", " "};
  char text[64];
  std::snprintf(text, sizeof text, "Total Kills: %d", kills);
  out.emplace_back(text);
  std::snprintf(text, sizeof text, "Total Shots: %d", shots);
  out.emplace_back(text);
  if (shots != 0) {
    std::snprintf(text, sizeof text, "Kills per 1k Shots: %6.2f", static_cast<double>(kills) * 1000.0 / shots);
    out.emplace_back(text);
  }
  return out;
}

LobbyPick LobbyChoice::picked() const {
  LobbyPick p;
  if (pick >= 0 && pick < static_cast<int32_t>(values.size())) p.has = true, p.value = values[static_cast<size_t>(pick)];
  return p;
}

void LobbyChoice::select_value(int32_t v) {
  for (size_t i = 0; i < values.size(); ++i)
    if (values[i] == v) {
      pick = static_cast<int32_t>(i);
      return;
    }
}

LobbySettings LobbyControls::settings() const {
  LobbySettings s;
  s.kills = kills.picked(), s.minutes = minutes.picked(), s.tons = tons.picked(), s.realloc = realloc.picked();
  s.rounds = rounds.picked(), s.kind = kind.picked(), s.world = world.picked();
  s.respawn = respawn, s.ammo = ammo, s.outside = outside, s.jets = jets, s.radar = radar, s.stock = stock;
  s.mobile = mobile;
  return s;
}

namespace {
uint32_t lobby_flags(const LobbyControls& c) {
  return (c.respawn ? 1u : 0u) | (c.mobile ? 2u : 0u) | (c.stock ? 4u : 0u) | (c.ammo ? 8u : 0u) |
         (c.outside ? 0x10u : 0u) | (c.jets ? 0x80u : 0u) | (c.radar ? 0x100u : 0u) | (c.locked ? 0x20u : 0u);
}
}  // namespace

// Pirate's Moon 1.0 @ 0x0049d220 (the host's, to all, guaranteed): the flags,
// then the world, kills, minutes, tons, kind and rounds (int16) and REALLOC
// (a word); a list without a pick sends 0. MW3's 0x00499b10 had the first
// four lists in 0x10 bytes.
std::vector<uint8_t> lobby_rules_message(const LobbyControls& c) {
  std::vector<uint8_t> m(0x18, 0);
  const uint32_t head = 0x20 | 0x18u << 16, flags = lobby_flags(c);
  const auto v = [](const LobbyChoice& l) { return l.picked().has ? l.picked().value : 0; };
  const int16_t h[6] = {static_cast<int16_t>(v(c.world)), static_cast<int16_t>(v(c.kills)),
                        static_cast<int16_t>(v(c.minutes)), static_cast<int16_t>(v(c.tons)),
                        static_cast<int16_t>(v(c.kind)), static_cast<int16_t>(v(c.rounds))};
  const int32_t realloc = v(c.realloc);
  std::memcpy(m.data(), &head, 4), std::memcpy(m.data() + 4, &flags, 4);
  std::memcpy(m.data() + 8, h, sizeof h), std::memcpy(m.data() + 0x14, &realloc, 4);
  return m;
}

// Pirate's Moon 1.0 @ 0x0049e6f0 (the screen's; inline in 0x0049d450).
void MultiplayerGame::keep_lobby_rules(const LobbyControls& c, int32_t seed) {
  const auto v = [](const LobbyChoice& l) { return static_cast<int16_t>(l.picked().has ? l.picked().value : 0); };
  rules = Rules{};
  rules.world = v(c.world), rules.kills = v(c.kills), rules.time = v(c.minutes), rules.weight = v(c.tons);
  rules.flags = lobby_flags(c);
  rules.seed = seed;
  uint8_t b[0x10];
  std::memcpy(b, &rules.flags, 4);
  std::memcpy(b + 4, &rules.world, 2), std::memcpy(b + 6, &rules.kills, 2), std::memcpy(b + 8, &rules.time, 2);
  std::memcpy(b + 10, &rules.weight, 2), std::memcpy(b + 12, &rules.seed, 4);
  rules_crc = zutl_zar::crc32(0, b, sizeof b);
}

// Pirate's Moon 1.0 @ 0x00438550 (0x00473240 empties a store, `full` off).
void MultiplayerGame::round_start(int32_t played) {
  if (played == 0) {
    store.clear();
    store.full = 0;
    bonus_tons = static_cast<float>(x78);  // +0x80
  }
  round_store.clear();
  round_store.full = 0;
}

// Pirate's Moon 1.0 @ 0x0049d450. The shorts are picked as unsigned (a
// negative value never matches); REALLOC as a word. FIX (A): whole messages
// only (the exe read 0x18 bytes of any).
bool MultiplayerGame::read_lobby_rules(const uint8_t* msg, size_t size, LobbyControls* c, int32_t seed,
                                       bool* team_play) {
  if (size < 0x18) return false;
  uint32_t flags;
  uint16_t h[6];
  int32_t realloc_v;
  std::memcpy(&flags, msg + 4, 4), std::memcpy(h, msg + 8, sizeof h), std::memcpy(&realloc_v, msg + 0x14, 4);
  c->world.select_value(h[0]), c->kills.select_value(h[1]), c->minutes.select_value(h[2]);
  c->tons.select_value(h[3]), c->kind.select_value(h[4]), c->realloc.select_value(realloc_v);
  c->rounds.select_value(h[5]);
  c->respawn = flags & 1, c->mobile = (flags >> 1) & 1, c->stock = (flags >> 2) & 1, c->ammo = (flags >> 3) & 1;
  c->outside = (flags >> 4) & 1, c->jets = (flags >> 7) & 1, c->radar = (flags >> 8) & 1;
  keep_lobby_rules(*c, seed);
  const bool was = c->locked;
  c->locked = (flags >> 5) & 1;
  if (was || !c->locked) return false;
  *team_play = h[4] != 0;  // [0x0062cc34]: the message's kind, not the list's
  return true;
}

// Pirate's Moon 1.0 @ 0x0049bb70's lines. A team: its name, "  {skin} ", in
// the one-base kind (3) " Attack" for side 1 else "Defend" (no space, the
// exe's). A player: its name (indented under a team), "++" the host's, " ",
// the state's text (1 in the mech lab 0x2bf, 2 0x1061, 3 0x1062, 5 ready
// 0x2c0; MW3's 1 and 3), outside team play " {skin} " when it has one, and
// ours our slot's mech's name. Everyone ready: every listed player at 5.
bool MultiplayerGame::lobby_lines(int32_t kinds, uint32_t local, const LobbyText& tx, std::vector<LobbyLine>* teams,
                                  std::vector<LobbyLine>* players) {
  const auto msg = [&](int32_t id) -> std::string {
    const char* m = tx.message ? tx.message(id) : nullptr;
    return m ? m : "";
  };
  const auto mech = [&](int32_t m) -> std::string {  // 0x004867c0
    const int32_t id = mech_name_message(m);
    return id < 0 ? "notmech" : msg(id);
  };
  const auto tag = [&](const char* open, int32_t skin, const char* close) {  // CString::Format, MakeLower
    // FIX (bug): skins 1..8 have names; the exe reads the screen's word before them (its game) for 0 as text.
    const bool named = tx.skin && skin >= 1 && skin <= 8;
    std::string s = open + (named ? tx.skin(skin) : std::string()) + close;
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
  };
  bool ready = true;
  const auto player = [&](LobbyPlayer& p, const char* indent) {
    std::string s = std::string(indent) + p.name;
    if (p.host) s += "++";  // [0x0060a818]
    s += " ";
    switch (p.state) {
      case 1: s += msg(0x2bf); break;
      case 2: s += msg(0x1061); break;
      case 3: s += msg(0x1062); break;
      case 5: s += msg(0x2c0); break;
      default: break;
    }
    if (p.state != 5) ready = false;  // the screen's +0x1679a
    return s;
  };
  const bool team_play = multiplayer_team_play();  // [0x0062cc34]
  if (team_play) {
    for (auto& t : lobby_teams) {
      t->count = 0;
      std::string s = t->name + tag("  {", t->skin, "} ");  // [0x0060a830]
      if (kinds == 3) s += (t->word2 & 1) ? " Attack" : "Defend";                             // [0x0060a828], [0x0060a820]
      teams->push_back({s, t.get()});
      for (auto& p : lobby_players) {
        if (p->team != t->id) continue;
        std::string line = player(*p, "   ");  // [0x0060a81c]
        if (p->id == local) line += mech(slot.mech);  // vt+0x30(0)'s +4
        teams->push_back({line, p.get()});
        ++t->count;
      }
    }
  }
  for (auto& p : lobby_players) {
    if (p->team != 0) continue;
    std::string line = player(*p, "");
    if (!team_play && p->skin != 0) line += tag(" {", p->skin, "} ");  // [0x0060a810]
    if (p->id == local) line += mech(slot.mech);
    players->push_back({line, p.get()});
  }
  return ready;
}

// Pirate's Moon 1.0 @ 0x0049c650's data (MW3's 0x00499030): kind 1's
// state, else 4 (MW3 2); the second word the skin whatever the kind, else 1;
// with the lobby between rounds (its +0x1679d), kind 1's words 4 and 5 the
// rounds won and place, else 0. FIX (bug): a kind 1 record too short for its
// state or those words gives the defaults; the exe reads past it.
void LobbyPlayer::from_data(const std::vector<uint32_t>& d, bool between_rounds) {
  state = 4, skin = 1, rounds = 0, place = 0;
  if (d.size() < 2) return;
  if (d[0] == 1 && d.size() > 2) state = static_cast<int32_t>(d[2]);
  skin = static_cast<int32_t>(d[1]);
  if (between_rounds && d[0] == 1) {
    rounds = d.size() > 4 ? static_cast<int32_t>(d[4]) : 0;
    place = d.size() > 5 ? static_cast<int32_t>(d[5]) : 0;
  }
}

// Pirate's Moon 1.0 @ 0x0049a3c0's rounds. The scores (0x00436db0) set the
// most rounds won and how many have it. Back from a game with rounds played:
// a tie for the most at the match's last round adds a round. With rounds
// played and one leader too far ahead for the rest to catch up in the rounds
// left (0x00437380), the match ends now (its total kept for the line). Then
// the match is over (entered from elsewhere, no round played, or none left:
// the rounds played cleared, the teams gone and team play off for the host)
// or another round follows (the lobby between rounds: ours keeps its place
// and rounds, the host's teams their rounds); the results list either way.
RoundsEntry MultiplayerGame::enter_rounds(bool from_game, int32_t* played, uint32_t local, bool host,
                                          void (*teams_end)(LobbyTeam& t, bool kept)) {
  RoundsEntry r;
  score_texts(0x15, 7);
  const bool checks = !from_game || *played != 0;  // back with none played: straight to the end
  if (from_game && *played != 0 && best_count > 1 && rounds_total <= *played) ++rounds_total, r.extended = true;
  if (checks && *played != 0 && *played < rounds_total && best_count == 1 &&
      rounds_total - *played < best - runner_up_rounds()) {
    r.over_total = rounds_total, rounds_total = *played, r.over = true;
  }
  const bool team_play = multiplayer_team_play();  // [0x0062cc34]
  if (!from_game || *played == 0 || rounds_total <= *played) {
    r.lines = round_lines(*played, r.extended, r.over, r.over_total);  // 0x0049b730
    *played = 0;
    if (host && team_play && teams_end)
      for (auto& t : lobby_teams) teams_end(*t, false);  // 0x005a9480 its players, 0x005a94f0 it
    set_multiplayer_team_play(false);
    return r;
  }
  r.next = true;
  r.lines = round_lines(*played, r.extended, r.over, r.over_total);
  r.place = place_of(local), r.rounds = rounds_of(local);  // 0x004366f0, 0x004366d0
  if (host && team_play && teams_end)
    for (auto& t : lobby_teams) teams_end(*t, true);  // its data's fourth word its rounds (0x005aa550)
  return r;
}

// Pirate's Moon 1.0 @ 0x00436580 (vt+0x58): the objectives block's name, by
// the side whose base is base 1 (vt+0x48).
std::string MultiplayerGame::objectives_key() {
  char buf[24];
  std::snprintf(buf, sizeof buf, "mp%d", x94);  // [0x006055b8]
  return buf;
}

// Pirate's Moon 1.0 @ 0x00437850 (vt+0x08): the scenario's text, two per
// scenario by whether +0xb0 is 1 (0x00596e50 on the id); 0xd75 past 3.
int32_t MultiplayerGame::scenario_message() {
  static const int32_t kIds[4][2] = {{0x321, 0x320}, {0x323, 0x322}, {0x325, 0x324}, {0x327, 0x326}};
  if (x90 < 0 || x90 > 3) return 0xd75;
  return kIds[x90][x94 == 1 ? 1 : 0];
}

// Pirate's Moon 1.0 @ 0x004368d0
int32_t MultiplayerGame::enemies_of(uint32_t player) {
  const uint32_t mine = net_team(player);  // 0x00436600
  int32_t n = 0;
  for (auto& p : lobby_players)
    if (p->id != player && net_team(p->id) != mine) ++n;
  return n;
}

// Pirate's Moon 1.0 @ 0x00436930: the player itself among them.
int32_t MultiplayerGame::award_team(uint32_t player) {
  const uint32_t mine = net_team(player);
  int32_t n = 0;
  for (auto& p : lobby_players)
    if (net_team(p->id) == mine) ++n, ++p->rounds;
  return n;
}

// Pirate's Moon 1.0 @ 0x00437380: the teams' in team play ([0x0062cc34]).
int32_t MultiplayerGame::runner_up_rounds() {
  int32_t r = 0;
  if (multiplayer_team_play()) {
    for (auto& t : lobby_teams)
      if (t->rounds != best && t->rounds > r) r = t->rounds;
  } else {
    for (auto& p : lobby_players)
      if (p->rounds != best && p->rounds > r) r = p->rounds;
  }
  return r;
}

// Pirate's Moon 1.0 @ 0x004366d0, 0x004366f0 (0x00498930 on the players)
int32_t MultiplayerGame::rounds_of(uint32_t player) {
  const LobbyPlayer* p = lobby_player(player);
  return p ? p->rounds : 0;
}
int32_t MultiplayerGame::place_of(uint32_t player) {
  const LobbyPlayer* p = lobby_player(player);
  return p ? p->place : 0;
}

// Pirate's Moon 1.0 @ 0x004373e0: each line's rank, 0 first, lines whose
// keys differ only in their low 4 bits (the order's) sharing it; the players'
// lines (the teams' in team play) in the lines' order, then each player's
// place its line's (its team's in team play) and each team's its line's.
void MultiplayerGame::rank() {
  const bool teams = multiplayer_team_play();  // [0x0062cc34]
  int32_t r = -1, run = 1;
  uint32_t last = 0xf;
  for (auto& l : score_lines) {
    if (l->kind != (teams ? 2 : 1)) continue;
    const uint32_t k = l->key & 0xfffffff0u;
    if (k == last) {
      l->rank = r, ++run;
    } else {
      r += run, l->rank = r, run = 1, last = k;
    }
  }
  if (!teams) {
    for (auto& p : lobby_players)
      if (p->score) p->place = p->score->rank;  // the exe reads +0x34 unchecked
    return;
  }
  for (auto& t : lobby_teams) {
    if (!t->score) continue;
    t->place = t->score->rank;
    for (auto& p : lobby_players)
      if (p->team == t->id) p->place = t->place;
  }
}

// Pirate's Moon 1.0 @ 0x00436d50: a round to each team on `side` (its third
// data word's bit 0) and to each of its players.
void MultiplayerGame::award_side(bool side) {
  for (auto& t : lobby_teams) {
    if ((static_cast<uint32_t>(t->word2) & 1) != (side ? 1u : 0u)) continue;
    ++t->rounds;
    for (auto& p : lobby_players)
      if (p->team == t->id) ++p->rounds;
  }
}

// Pirate's Moon 1.0 @ 0x00436980: the round's winner, given its round when
// `award`. A base destroyed (kinds over 1) wins for the side vt+0x48 names
// for that base; kind 3 timed out with no base lost, side 0. Otherwise the
// lines rescored without the rounds, sorted and ranked, and the first place
// (players, or teams) wins when it is one alone; all those on it get the
// round. Awarding ends with the rescore with the rounds, sorted and ranked.
bool MultiplayerGame::tally(bool award) {
  bool one = true;
  const auto sort_lines = [&] {
    std::stable_sort(score_lines.begin(), score_lines.end(),  // 0x00438820 (std::sort; ties' order unspecified)
                     [](const std::unique_ptr<ScoreLine>& a, const std::unique_ptr<ScoreLine>& b) { return a->key > b->key; });
  };
  if (kind == 3 && round_end == 3 && base_lost == 0) {
    if (!award) return true;
    award_side(false);
  } else if (kind < 2 || base_lost == 0) {
    int32_t first = 0;
    rescore(false);
    sort_lines();
    rank();
    if (!multiplayer_team_play()) {
      for (auto& p : lobby_players)
        if (p->place == 0 && !p->left && (++first, award)) ++p->rounds;
    } else {
      for (auto& t : lobby_teams) {
        if (t->place != 0 || (++first, !award)) continue;
        ++t->rounds;
        for (auto& p : lobby_players)
          if (p->team == t->id && !p->left) ++p->rounds;
      }
    }
    one = first == 1;
  } else {
    if (!award) return true;
    if (base_lost == 1) award_side(x94 == 1);  // vt+0x48
    else if (base_lost == 2) award_side(x94 == 2);
  }
  if (award) {
    rescore(true);
    sort_lines();
    rank();
  }
  return one;
}

// Pirate's Moon 1.0 @ 0x00437970: a winner set already (+0xaa), else the
// tally's, not awarding.
bool MultiplayerGame::round_winner() { return winner_set || tally(false); }

// Pirate's Moon 1.0 @ 0x00436db0 (MW3's 0x00435b60): SCORES_LIST's lines
// (and the end of a round's, 0x0049b730): the name cut to its column, then
// the rounds won (in team play the team's), the score and the kills, each cut
// to its column (MW3: score, kills, suicides); a team's line "<name> (Rounds
// Won: <n>)". The most rounds won and how many have it noted (teams' in team
// play); the lines sorted by their keys (std::sort in the exe, its order of
// equal keys unspecified: stable here).
void MultiplayerGame::score_texts(int32_t name_width, int32_t number_width) {
  const auto left = [](const std::string& s, int32_t n) { return s.substr(0, n < 0 ? 0 : static_cast<size_t>(n)); };
  char buf[256];
  best = 0, best_count = 0;
  const auto note = [&](int32_t rounds_won) {
    if (best < rounds_won) best = rounds_won, best_count = 1;
    else if (rounds_won == best) ++best_count;
  };
  const auto line = [&](LobbyPlayer& p, const char* name_format, int32_t rounds_won) {
    ScoreLine* l = p.score;
    std::snprintf(buf, sizeof buf, name_format, p.name.c_str());
    std::string t = left(buf, name_width) + " ";
    std::snprintf(buf, sizeof buf, "%d             ", rounds_won);  // [0x006055dc]
    t += left(buf, number_width);
    std::snprintf(buf, sizeof buf, "%d             ", l ? l->score : 0);
    t += left(buf, number_width);
    std::snprintf(buf, sizeof buf, "%d", p.kills);  // [0x00604080]
    if (l) l->text = t + buf;  // the exe reads +0x34 unchecked
  };
  if (!multiplayer_team_play()) {  // [0x0062cc34]
    for (auto& p : lobby_players) {
      note(p->rounds);
      line(*p, "%s                        ", p->rounds);  // [0x006055c0]
    }
  } else {
    for (auto& t : lobby_teams) {
      if (t->score) {
        std::snprintf(buf, sizeof buf, "%s (Rounds Won: %d)", t->name.c_str(), t->rounds);  // [0x00605608]
        t->score->text = buf;
      }
      note(t->rounds);
      for (auto& p : lobby_players)
        if (p->team == t->id) line(*p, "  %s                  ", t->rounds);  // [0x006055f0]
    }
  }
  std::stable_sort(score_lines.begin(), score_lines.end(),
                   [](const std::unique_ptr<ScoreLine>& a, const std::unique_ptr<ScoreLine>& b) { return a->key > b->key; });
}

// Pirate's Moon 1.0 @ 0x0049b730 (on the lobby screen: `played` [0x0062cc38],
// `extended` its +0x1679f, `over` +0x167a0, `over_total` +0x167a4). Each line
// is an item of its results list (+0x15e14). Not ported: then, when the game
// came from the MSN Gaming Zone ([0x0062cc2d]), its scores reported to it
// (0x005aaa80, 0x005aab60, 0x005aaba0), FIX (A): no Zone.
std::vector<std::string> MultiplayerGame::round_lines(int32_t played, bool extended, bool over, int32_t over_total) {
  std::vector<std::string> out;
  if (played == 0) {
    const char* m = pilot_hooks().message(0x2bd);  // 0x00596e50
    out.emplace_back(m ? m : "");
    return out;
  }
  char buf[64];
  std::snprintf(buf, sizeof buf, "End of Round %d of %d", played, over ? over_total : rounds_total);  // [0x0060a7f8]
  out.emplace_back(buf);
  if (extended) out.emplace_back(" Rounds extended due to tie.");  // [0x0060a7d8]
  if (over) out.emplace_back(" Game over: No one else can win.");  // [0x0060a7b4]
  score_texts(0x15, 7);  // 0x00436db0
  for (const auto& l : score_lines) out.push_back(l->text);
  return out;
}

// Mech3.exe 1.2 @ 0x00436e30
TrainingGame::TrainingGame(Pilot* p) : Game(p), lessons(std::make_unique<TrainingLessons>()) {
  pilot_hooks().store_fill(store, kTraining);
}

// Mech3.exe 1.2 @ 0x00436f10: its lessons freed (0x0045ef10).
TrainingGame::~TrainingGame() = default;

// Mech3.exe 1.2 @ 0x00437060, 0x004370d0
int32_t TrainingGame::name_id(int32_t i) {
  static const int32_t kNames[] = {0x21e, 0x222, 0x224, 0x226};
  return kNames[(i > 4 ? 4 : i < 1 ? 1 : i) - 1];
}
int32_t TrainingGame::text_id(int32_t i) {
  static const int32_t kTexts[] = {0x21f, 0x223, 0x225, 0x227};
  return kTexts[(i > 4 ? 4 : i < 1 ? 1 : i) - 1];
}

// Mech3.exe 1.2 @ 0x00437980, 0x00437990, 0x004379b0 (+0x38, 0x004487d0, is
// 0 in every training game, 0x00436e30): the briefing's texts.
const char* TrainingGame::text_of(int32_t slot, int32_t mission) {
  switch (slot) {
    case 0x04: return pilot_hooks().message(0x10b);
    case 0x0c: return pilot_hooks().message(name_id(mission));
    case 0x10: return pilot_hooks().message(text_id(mission));
    default: return nullptr;
  }
}

// Mech3.exe 1.2 @ 0x004378a0, 0x004487d0, 0x0040a840, 0x00437970
const char* ActionGame::game_text(int32_t slot) {
  switch (slot) {
    case 0x04: return pilot_hooks().message(0x10c);
    case 0x08: return x38;
    case 0x0c: return x3c;
    case 0x10: return x40;
    default: return nullptr;
  }
}

// Mech3.exe 1.2 @ 0x00436fe0
void TrainingGame::save() {
  Game::save();
  wr(pilot->file, &mission, 4);
  wr(pilot->file, passed, 4);
}

// Mech3.exe 1.2 @ 0x00437020
void TrainingGame::load() {
  Game::load();
  rd(pilot->file, &mission, 4);
  rd(pilot->file, passed, 4);
}

}  // namespace mech3
