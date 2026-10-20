// The pilot's games, their slots and stores (docs/pilot.md, The games).
#include <cstdlib>
#include <cstring>

#include "gamez/zutil/zutl_msg.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/pilot.h"

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

// vt+0x24 of the item kinds: 0x00409f60 (base, ammo, engine), 0x0044ece0
// (weapons: the tech too), 0x00473480 (equipment). `in` is the listed item.
bool same_item(const ConfigItem* in, const ConfigItem* probe) {
  if (in->kind() == 3) return static_cast<const EquipmentItem*>(in)->same_as(*probe);
  if (probe->kind() != in->kind() || probe->id() != in->id()) return false;
  if (in->kind() == 1)
    return static_cast<const WeaponItem*>(probe)->tech == static_cast<const WeaponItem*>(in)->tech;
  return true;
}

class DefaultPilotHooks : public PilotHooks {};
DefaultPilotHooks g_default_hooks;
PilotHooks* g_hooks = &g_default_hooks;
ActionSetup g_action_setup;
}  // namespace

void set_pilot_hooks(PilotHooks* h) { g_hooks = h ? h : &g_default_hooks; }
PilotHooks& pilot_hooks() { return *g_hooks; }
ActionSetup& action_setup() { return g_action_setup; }

Config* PilotHooks::config_load(const char* name, std::FILE* f) { return Config::from_file(name, f); }
void PilotHooks::config_save(Config* c, std::FILE* f) {
  if (c) c->save(f);
}
const char* PilotHooks::message(int32_t id) {
  const zutl::Messages& m = zutl::messages();
  const char* s = m.get_text ? m.get_text(id) : nullptr;
  return s ? s : "";
}
// FormatMessageA's inserts as 0x00575fd0 passes them: %n, or %n!fmt! with a
// printf format, each the n-th argument; %% is a percent sign.
std::string PilotHooks::format_ints(int32_t id, std::initializer_list<int32_t> args) {
  const std::string m = message(id);
  std::string out;
  for (size_t i = 0; i < m.size(); ++i) {
    if (m[i] != '%' || i + 1 >= m.size()) {
      out += m[i];
      continue;
    }
    const char c = m[i + 1];
    if (c < '1' || c > '9') {
      out += c;
      ++i;
      continue;
    }
    const size_t n = static_cast<size_t>(c - '1');
    const int32_t v = n < args.size() ? args.begin()[n] : 0;
    std::string spec = "d";
    i += 1;
    if (i + 1 < m.size() && m[i + 1] == '!') {
      const size_t end = m.find('!', i + 2);
      if (end != std::string::npos) {
        spec = m.substr(i + 2, end - (i + 2));
        i = end;
      }
    }
    char buf[64];
    std::snprintf(buf, sizeof buf, ("%" + spec).c_str(), v);
    out += buf;
  }
  return out;
}

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

// Mech3.exe 1.2 @ 0x004714c0
ConfigItem* Store::find_like(const ConfigItem* item) {
  for (ConfigItem* i : items)
    if (same_item(i, item)) return i;
  return nullptr;
}

// Mech3.exe 1.2 @ 0x00471460
ConfigItem* Store::find(const ConfigItem* item) {
  for (ConfigItem* i : items)
    if (i == item) return i;
  return find_like(item);
}

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

Game::Game(Pilot* p) : pilot(p), slot(3, nullptr, 0) {}

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
  lineup[0].mech = 3;  // 0x005a2ba0 on each
  lineup[1].mech = 8;
  lineup[2].mech = 13;
}

// Mech3.exe 1.2 @ 0x004374d0
LabGame::~LabGame() = default;

// Mech3.exe 1.2 @ 0x004345a0. Operation 1, mission 1 gets the starting
// store: two mechs 3 with the "Primary" loadout (message 0xf20), one of the
// mech's own taken back, and ammunition (pilot.md, A new campaign).
CampaignGame::CampaignGame(Pilot* p, int32_t c, int32_t m) : Game(p), mission(m), campaign(c) {
  if (c == 1 && m == 1) {
    PilotHooks& h = pilot_hooks();
    h.store_add_mech(store, 3, h.message(0xf20));
    h.store_add_mech(store, 3, h.message(0xf20));
    store.remove_mech(3);
    store.add_ammo(0x11, 750);
    store.add_ammo(0x15, 4000);
    store.add_ammo(2, 1000);
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

// Mech3.exe 1.2 @ 0x00434990
void CampaignGame::save() {
  Game::save();
  std::FILE* f = pilot->file;
  wr(f, &campaign, 4);
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
  if (n)
    for (GameSlot* s : lancemates) s->save(*pilot);  // 0x00434010
}

// Mech3.exe 1.2 @ 0x00434ae0. FIX (bug): a damaged count reads on past the
// file's end, making a slot each time (up to 4 billion: a hang); here the
// reading stops at the end (docs/fixes.md).
void CampaignGame::load() {
  Game::load();
  std::FILE* f = pilot->file;
  rd(f, &campaign, 4);
  rd(f, &mission, 4);
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
  merge_salvage();
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

// Mech3.exe 1.2 @ 0x00434950
const char* CampaignGame::path_for(const char* pilot_name) {
  char buf[0x200];
  std::snprintf(buf, sizeof buf, "pilots//games//%s%02d%02d.mw3", pilot_name, operation(), mission_number());
  text = buf;
  return text.c_str();
}

// Mech3.exe 1.2 @ 0x00434980
const char* CampaignGame::path() { return path_for(pilot->name); }

// Mech3.exe 1.2 @ 0x00435330 (0x00576080: _access)
bool CampaignGame::readable() { return zutl::exists(path()); }

// Mech3.exe 1.2 @ 0x00434900: message 0x21d, "Op %1!d!, Mission %2!d!".
const char* CampaignGame::name() {
  text = pilot_hooks().format_ints(0x21d, {operation(), mission_number()});
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

// Mech3.exe 1.2 @ 0x00436b50
void ActionGame::save() {
  Game::save();
  std::FILE* f = pilot->file;
  const int32_t stats[8] = {player_kills, lancemate_kills, enemy_kills, attack_wins,
                            attack_losses, defend_wins, defend_losses, waves};
  for (int32_t v : stats) wr(f, &v, 4);
  ActionSetup& a = action_setup();
  wr(f, &a.g874, 4);
  wr(f, &a.g878, 4);
  wr(f, &a.g87c, 4);
  for (int32_t& v : a.g820) wr(f, &v, 4);
  for (int i = 0; i < 6; ++i) {
    wr(f, &a.g838[i], 4);
    wr(f, &a.g858[i], 4);
  }
}

// Mech3.exe 1.2 @ 0x00436cc0
void ActionGame::load() {
  Game::load();
  std::FILE* f = pilot->file;
  int32_t* stats[8] = {&player_kills, &lancemate_kills, &enemy_kills, &attack_wins,
                       &attack_losses, &defend_wins, &defend_losses, &waves};
  for (int32_t* v : stats) rd(f, v, 4);
  ActionSetup& a = action_setup();
  rd(f, &a.g874, 4);
  rd(f, &a.g878, 4);
  rd(f, &a.g87c, 4);
  for (int32_t& v : a.g820) rd(f, &v, 4);
  for (int i = 0; i < 6; ++i) {
    rd(f, &a.g838[i], 4);
    rd(f, &a.g858[i], 4);
  }
}

// Mech3.exe 1.2 @ 0x004353a0
MultiplayerGame::MultiplayerGame(Pilot* p) : Game(p) {
  store.full = 1;
  pilot_hooks().store_fill(store, kMultiplayer);
}

// Mech3.exe 1.2 @ 0x00435500
MultiplayerGame::~MultiplayerGame() = default;

// Mech3.exe 1.2 @ 0x00435690
void MultiplayerGame::save() {
  Game::save();
  std::FILE* f = pilot->file;
  wr(f, &kills, 4);
  wr(f, &deaths, 4);
  wr(f, &x70, 4);
  wr(f, &x74, 4);
  wr(f, &b8c, 1);
  wr(f, &b8d, 1);
  wr(f, &b8e, 1);
  wr(f, &ba8, 1);
  wr(f, &x90, 4);
  wr(f, &x94, 4);
  wr(f, &b8f, 1);
  wr(f, &x78, 4);
}

// Mech3.exe 1.2 @ 0x004357d0
void MultiplayerGame::load() {
  Game::load();
  std::FILE* f = pilot->file;
  rd(f, &kills, 4);
  rd(f, &deaths, 4);
  rd(f, &x70, 4);
  rd(f, &x74, 4);
  rd(f, &b8c, 1);
  rd(f, &b8d, 1);
  rd(f, &b8e, 1);
  rd(f, &ba8, 1);
  rd(f, &x90, 4);
  rd(f, &x94, 4);
  rd(f, &b8f, 1);
  rd(f, &x78, 4);
}

// Mech3.exe 1.2 @ 0x00436e30
TrainingGame::TrainingGame(Pilot* p) : Game(p) { pilot_hooks().store_fill(store, kTraining); }

// Mech3.exe 1.2 @ 0x00436f10
TrainingGame::~TrainingGame() = default;

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
