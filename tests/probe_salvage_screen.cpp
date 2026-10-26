// Probe for test_salvage_screen.py (docs/campaign.md section 10): the
// salvage screen built from dlgSalvage.zrd on the ported zTwoD, with the
// game's kind, field bases, slots and three stores from the test's setup
// (items by the emulator's addresses), then a plan of frames, the lists'
// choices and presses; every call out recorded through the calltrace
// callback, the screen dumped as text.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "mech3/app.h"
#include "mech3/mech_config.h"
#include "mech3/pilot.h"
#include "mech3/salvage_screen.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;
using mech3::Arg;

std::string hex_s(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%x", v);
  return b;
}

struct SvHost : RecordingHost {
  uint32_t format_message(char* buf, uint32_t size, uint32_t id, va_list args) override {
    va_list c;
    va_copy(c, args);
    const int32_t a = va_arg(c, int32_t);  // (0x163, 0x164: the bases, one number)
    va_end(c);
    const std::string text = "msg" + std::to_string(id) + ":" + std::to_string(a);
    const uint32_t n = call("0x00575ff0", {size, id});
    std::snprintf(buf, size, "%s", n ? text.c_str() : "");
    return n;
  }
  const char* message_text(uint32_t id) override {
    if (!call("0x00576040", {id})) return nullptr;
    return keep("M" + hex_s(id));
  }
};

struct SvEngine : mech3::Engine {
  uint32_t kind = 1, units = 0, count = 0;
  uint32_t addr[3] = {};               // the player's, the campaign's and [0x00609990]'s stores
  mech3::Store stores[3]{mech3::Store(0), mech3::Store(0), mech3::Store(0)};
  std::vector<std::unique_ptr<mech3::GameSlot>> slots;
  std::map<const mech3::ConfigItem*, uint32_t> item_addr;
  std::map<uint32_t, const mech3::ConfigItem*> addr_item;
  uint32_t next_handle = 0xa0000000;   // items the moves make

  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  int32_t fast_menu() override { return static_cast<int32_t>(call("0x0040cec0", {}) & 0xff); }
  void set_fast_menu(int32_t on) override { call("0x0040ce90", {on}); }
  int32_t exchange_fast_menu(int32_t on) override { return static_cast<int32_t>(call("0x0040ce90", {on}) & 0xff); }
  int32_t display_width() override { return static_cast<int32_t>(call("0x0040d760", {})); }
  int32_t display_height() override { return static_cast<int32_t>(call("0x0040d770", {})); }
  const char* message(int32_t id) override {
    if (!call("0x00576040", {id})) return nullptr;
    return keep("M" + hex_s(static_cast<uint32_t>(id)));
  }
  std::string message_format_is(int32_t id, int32_t a, const char* s) override {
    const std::string text = s ? s : "";
    if (!call("0x00575fd0", {id, a, text.c_str()})) return std::string();
    return ("F" + hex_s(static_cast<uint32_t>(id)) + ":" + std::to_string(a) + "," + text).substr(0, 0xff);
  }
  std::string message_format_sd(int32_t id, const char* s, int32_t n, bool has_n) override {
    return message_format_is(id, has_n ? n : 0, s);
  }
  const char* mech_name_of(int32_t mech) override {
    if (!call("0x00484650", {mech})) return nullptr;
    return keep("mech" + std::to_string(mech));
  }
  void pop_state(uint32_t arg) override { call("0x005905a0", {arg}); }

  int32_t game_kind() override { return static_cast<int32_t>(kind); }
  int32_t campaign_units() override { return kind == 1 ? static_cast<int32_t>(units) : 0; }
  int32_t game_slot_count() override { return static_cast<int32_t>(count); }
  mech3::GameSlot* game_slot(int32_t i) override {
    return i >= 0 && i < static_cast<int32_t>(slots.size()) ? slots[static_cast<size_t>(i)].get() : nullptr;
  }
  mech3::Store* game_store() override { return &stores[0]; }
  mech3::Store* campaign_salvage() override { return &stores[1]; }
  Handle mission_salvage_store() override { return addr[2]; }
  Handle store_handle(mech3::Store* s) override {
    for (int i = 0; i < 3; ++i)
      if (s == &stores[i]) return addr[i];
    return 0;
  }
  mech3::Store* store_of(Handle h) override {
    for (int i = 0; i < 3; ++i)
      if (h && h == addr[i]) return &stores[i];
    return nullptr;
  }
  Handle item_handle(const mech3::ConfigItem* it) override {
    auto f = item_addr.find(it);
    if (f != item_addr.end()) return f->second;
    const uint32_t h = next_handle++;
    item_addr[it] = h;
    addr_item[h] = it;
    return h;
  }
  const mech3::ConfigItem* item_of(Handle h) override {
    auto f = addr_item.find(h);
    return f == addr_item.end() ? nullptr : f->second;
  }

  // An item by its store and place, as the test writes it.
  std::string where(uint32_t h) {
    const mech3::ConfigItem* it = item_of(h);
    static const char kNames[] = "pcm";
    for (int i = 0; i < 3; ++i) {
      int k = 0;
      for (const mech3::ConfigItem* x : stores[i].items) {
        if (x == it) return std::string(1, kNames[i]) + std::to_string(k);
        ++k;
      }
    }
    return hex_s(h);
  }
};

uint32_t hex(std::istringstream& ls) {
  uint32_t v = 0;
  ls >> std::hex >> v >> std::dec;
  return v;
}
float fhex(std::istringstream& ls) { return zgame::from_bits(hex(ls)); }

ztwod::MouseState read_mouse(std::istringstream& ls) {
  ztwod::MouseState m{};
  uint32_t* w = reinterpret_cast<uint32_t*>(&m);
  for (int i = 0; i < 15; ++i) w[i] = hex(ls);
  return m;
}

int32_t ref(const Dump& d, const ztwod::Object2D* o) {
  for (size_t i = 0; i < d.objs.size(); ++i)
    if (d.objs[i] == o) return static_cast<int32_t>(i);
  return -1;
}

std::string item_text(const mech3::ConfigItem* it) {
  const int k = it->kind();
  int32_t ident = 0;
  uint32_t extra = 0;
  if (k == 0) ident = static_cast<const mech3::BaseItem*>(it)->value;
  if (k == 1) {
    ident = static_cast<const mech3::WeaponItem*>(it)->weapon;
    extra = static_cast<uint32_t>(static_cast<const mech3::WeaponItem*>(it)->tech);
  }
  if (k == 2) ident = static_cast<const mech3::AmmoItem*>(it)->weapon;
  if (k == 3) {
    ident = static_cast<const mech3::EquipmentItem*>(it)->equipment;
    extra = static_cast<uint32_t>(static_cast<const mech3::EquipmentItem*>(it)->tech);
  }
  std::ostringstream o;
  o << "[k" << k << " n" << it->count << " i" << ident << " x" << std::hex << extra << std::dec << " b"
    << int(it->b10) << ']';
  return o.str();
}

// The dump test_salvage_screen.py builds from the emulator's memory.
void dump_screen(mech3::SalvageScreen& s, SvEngine& e, Dump& d) {
  std::ostringstream& o = d.o;
  static const char kStores[] = "pcm";
  const std::pair<const char*, mech3::SalvageList*> lists[] = {{"player", &s.player}, {"mission", &s.mission}};
  for (const auto& [name, l] : lists) {
    std::string sname = hex_s(l->store);
    for (int i = 0; i < 3; ++i)
      if (l->store == e.addr[i]) sname = std::string(1, kStores[i]);
    o << name << " id=" << l->id << " store=" << sname << " x218=" << std::hex << l->x218 << std::dec
      << " x20d=" << int(l->x20d) << " sel=" << l->selected << " items=";
    for (size_t i = 0; i < l->items.size(); ++i) {
      const uint32_t v = static_cast<uint32_t>(l->items[i]->value);
      o << (i ? "|" : "") << (l->items[i]->text ? l->items[i]->text : "") << '=' << (v ? e.where(v) : "0");
    }
    o << '\n';
  }
  for (int i = 0; i < 6; ++i)
    o << "bay" << i << " id=" << s.bays[i].id << " mech=" << s.bays[i].mech << " name=o" << ref(d, &s.bays[i].name)
      << '\n';
  for (int i = 0; i < 4; ++i)
    o << "lance" << i << " id=" << s.lance[i].id << " mech=" << s.lance[i].mech << " name=o"
      << ref(d, &s.lance[i].name) << '\n';
  for (int i = 0; i < 3; ++i) {
    o << "store " << kStores[i] << ": ";
    for (const mech3::ConfigItem* it : e.stores[i].items) o << item_text(it);
    o << '\n';
  }
}

void read_items(std::istringstream& ss, SvEngine& e, mech3::Store& store) {
  uint32_t n = 0;
  ss >> n;
  for (uint32_t i = 0; i < n; ++i) {
    uint32_t addr = 0, kind = 0, id = 0, extra = 0, count = 0, own = 0;
    ss >> addr >> kind >> id >> extra >> count >> own;
    mech3::ConfigItem* it = nullptr;
    if (kind == 0) {
      auto* b = new mech3::BaseItem;
      b->value = static_cast<int32_t>(id);
      it = b;
    } else if (kind == 1) {
      auto* w = new mech3::WeaponItem;
      w->weapon = static_cast<int32_t>(id), w->tech = static_cast<int32_t>(extra);
      it = w;
    } else if (kind == 2) {
      auto* a = new mech3::AmmoItem;
      a->weapon = static_cast<int32_t>(id);
      it = a;
    } else {
      auto* q = new mech3::EquipmentItem;
      q->equipment = static_cast<int32_t>(id), q->tech = static_cast<int32_t>(extra);
      it = q;
    }
    it->count = static_cast<int32_t>(count);
    it->b10 = static_cast<uint8_t>(own);
    store.items.push_back(it);
    e.item_addr[it] = addr;
    e.addr_item[addr] = it;
  }
}

}  // namespace

extern "C" {

int probe_salvage_screen_run(const char* reader, const char* setup, const char* plan, char* out, int size) {
  SvHost host;
  ztwod::set_host(&host);
  reset();
  SvEngine engine;
  {
    std::istringstream ss(setup);
    ss >> std::hex >> engine.kind >> engine.units >> engine.count >> engine.addr[0] >> engine.addr[1] >>
        engine.addr[2];
    uint32_t n = 0;
    ss >> n;
    for (uint32_t i = 0; i < n; ++i) {
      uint32_t m = 0;
      ss >> m;
      engine.slots.push_back(std::make_unique<mech3::GameSlot>(static_cast<int32_t>(m)));
    }
    for (mech3::Store& st : engine.stores) read_items(ss, engine, st);
  }
  std::unique_ptr<mech3::Mech3App> app;
  {
    probe_app::Mute mute;
    app = std::make_unique<mech3::Mech3App>(&engine);
  }
  mech3::set_app(app.get());
  const bool per_second = zfix::options().per_second;
  const bool bugs = zfix::options().bugs;
  zfix::options().per_second = false;  // the original's path (zfix.h)
  zfix::options().bugs = false;
  zrdr::init();
  zrdr::close_archives(true);
  zrdr::open_archive(reader, true);
  ztwod::set_page_flip(true);
  std::unique_ptr<mech3::SalvageScreen> s;
  std::string dump;
  std::istringstream in(plan);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::string op;
    ls >> op;
    if (op == "S") {
      s = std::make_unique<mech3::SalvageScreen>(engine, 0x608bd8);
    } else if (!s) {
      continue;
    } else if (op == "F") {
      const float dt = fhex(ls);
      g_mouse.push_back(read_mouse(ls));
      s->update(dt);
      ++g_frame;
    } else if (op == "L") {
      std::string side;
      ls >> side;
      (side == "p" ? s->player : s->mission).select(static_cast<int32_t>(hex(ls)));
    } else if (op == "B") {
      const uint32_t id = hex(ls), row = hex(ls);
      s->on_button(static_cast<int32_t>(id), 0, static_cast<int32_t>(row));
    } else if (op == "E") {
      Dump d;
      dump_dialog(*s, d);
      dump_screen(*s, engine, d);
      dump += d.o.str();
    } else if (op == "U") {
      s.reset();
    }
  }
  s.reset();
  zrdr::close_archives(true);
  zfix::options().per_second = per_second;
  zfix::options().bugs = bugs;
  mech3::set_app(nullptr);
  ztwod::set_host(nullptr);
  if (static_cast<int>(dump.size()) >= size) return -1;
  std::memcpy(out, dump.data(), dump.size() + 1);
  return static_cast<int>(dump.size());
}

}  // extern "C"
