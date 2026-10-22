// Probe for test_lab_screen.py (docs/mechlab.md, step 2): the mech lab's
// screen built from dlgMechLabII.zrd (or a synthetic archive) on the ported
// zTwoD, the Lab State's globals and the game's slots read from the
// emulator, then a plan of page switches; every call out recorded through
// the calltrace callback, the screen dumped as text.
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "mech3/app.h"
#include "mech3/app_states.h"
#include "mech3/lab_screen.h"
#include "mech3/mech_config.h"
#include "mech3/pilot.h"
#include <map>
#include "probe_twod.h"

namespace {
using namespace probe_twod;
using mech3::Arg;

std::string hex_s(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%x", v);
  return b;
}

// ALIST_TITLES's messages: their texts show the count (test_lab_screen.py's COUNTED).
bool counted(uint32_t id) { return id == 0xf5b || id == 0x24c || id == 0x24d; }

struct LabHost : RecordingHost {
  uint32_t format_message(char* buf, uint32_t size, uint32_t id, va_list args) override {
    const uint32_t n = RecordingHost::format_message(buf, size, id, args);
    if (n && counted(id)) {
      const std::string s = std::string(buf) + ' ' + std::to_string(va_arg(args, int32_t));
      std::snprintf(buf, size, "%s", s.c_str());
    }
    return n;
  }
  const char* message_text(uint32_t id) override {
    if (!call("0x00576040", {id})) return nullptr;
    return keep("M" + hex_s(id) + (counted(id) ? " %d" : ""));
  }
};

struct LabEngine : mech3::Engine {
  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  int32_t fast_menu() override { return static_cast<int32_t>(call("0x0040cec0", {}) & 0xff); }
  void set_fast_menu(int32_t on) override { call("0x0040ce90", {on}); }
  int32_t display_width() override { return static_cast<int32_t>(call("0x0040d760", {})); }
  int32_t display_height() override { return static_cast<int32_t>(call("0x0040d770", {})); }
  const char* message(int32_t id) override {
    if (!call("0x00576040", {id})) return nullptr;
    return keep("M" + hex_s(static_cast<uint32_t>(id)) + (counted(static_cast<uint32_t>(id)) ? " %d" : ""));
  }
  const char* mech_name_of(int32_t mech) override {
    if (!call("0x00484650", {mech})) return nullptr;
    return keep("mech" + std::to_string(mech));
  }
  void pop_state(uint32_t arg) override { call("0x005905a0", {arg}); }
  const char* message_format(int32_t id, const char* arg) override {  // the test's stub's text
    call("0x00575fd0", {id, arg ? arg : ""});
    char buf[0x100];
    std::snprintf(buf, sizeof buf, "F%x:%s", static_cast<uint32_t>(id), arg ? arg : "");
    return keep(buf);
  }
  void lab_store_remove(Handle store, int32_t mech) override { call("0x00472a30", {store, mech}); }
  void lab_store_add(Handle store, int32_t mech) override { call("0x00471c10", {store, mech}); }
  void lab_slot_set_config(Handle slot, Handle config, Handle store) override {
    call("0x00433df0", {slot, config, store, 1});
    poke32(slot + 8, config);  // as the test's stub
  }
  mech3::LabScreen* screen = nullptr;
  mech3::LabScreen* lab_screen(Handle) override { return screen; }
  // The lab's store, its items by their emulator address.
  mech3::Store store{0};
  Handle store_addr = 0;
  std::map<const mech3::ConfigItem*, Handle> addr_of;
  std::map<Handle, const mech3::ConfigItem*> item_at;
  mech3::Store* store_of(Handle h) override { return h && h == store_addr ? &store : nullptr; }
  Handle item_handle(const mech3::ConfigItem* it) override {
    auto f = addr_of.find(it);
    return f == addr_of.end() ? 0 : f->second;
  }
  const mech3::ConfigItem* item_of(Handle h) override {
    auto f = item_at.find(h);
    return f == item_at.end() ? nullptr : f->second;
  }
  // The loadout lists as the exe changes them in its memory (read from it
  // first), and the names given (its _strdup'd strings).
  std::map<Handle, std::vector<Handle>> lists;
  std::map<Handle, std::string> names;
  std::vector<Handle> lab_list_configs(Handle list) override {
    auto f = lists.find(list);
    if (f == lists.end()) f = lists.emplace(list, mech3::Engine::lab_list_configs(list)).first;
    return f->second;
  }
  void lab_list_append(Handle list, Handle config) override {
    lab_list_configs(list);
    lists[list].push_back(config);
  }
  bool lab_list_remove(Handle list, Handle config) override {
    if (!config || !lab_config_byte(config, 0x1c)) return false;
    lab_list_configs(list);
    auto& v = lists[list];
    v.erase(std::remove(v.begin(), v.end(), config), v.end());
    return true;
  }
  void lab_config_set_name(Handle config, const char* name) override { names[config] = name ? name : ""; }
  std::string lab_config_name(Handle config) override {
    auto f = names.find(config);
    return f != names.end() ? f->second : mech3::Engine::lab_config_name(config);
  }
  std::vector<std::pair<int32_t, Handle>> dfns;
};

std::vector<uint32_t> g_slots;  // the game's slots, by their address in the emulator
struct LabEngine;
LabEngine* g_engine = nullptr;

uint32_t hex(std::istringstream& ls) {
  uint32_t v = 0;
  ls >> std::hex >> v >> std::dec;
  return v;
}

int32_t ref(const Dump& d, const ztwod::Object2D* o) {
  for (size_t i = 0; i < d.objs.size(); ++i)
    if (d.objs[i] == o) return static_cast<int32_t>(i);
  return -1;
}

// The dump test_lab_screen.py builds from the emulator's memory.
void dump_screen(mech3::LabScreen& s, Dump& d) {
  std::ostringstream& o = d.o;
  o << "screen page=" << s.page << '\n';
  for (int i = 0; i < 7; ++i) {
    mech3::HelpButton& t = s.tabs[i];
    o << "tab" << i << " id=" << t.id << " en=" << t.enabled << " sel=" << int(t.selected)
      << " blink=" << int(t.blinking) << " help=";
    bool first = true;
    for (ztwod::Text* h : t.help) {
      o << (first ? "" : ",") << 'o' << ref(d, h);
      first = false;
    }
    o << '\n';
  }
  for (int i = 0; i < 12; ++i) {
    mech3::MechButton& m = s.mechs[i];
    o << "mech" << i << " id=" << m.id << " mech=" << m.mech << " name=o" << ref(d, &m.name) << '\n';
  }
  o << "parts head=" << s.part_head.part << ',' << s.part_head.id;
  for (const mech3::LabPart& p : s.parts) o << ' ' << p.part << ',' << p.id;
  o << '\n';
  const mech3::LabPanel* ps[] = {&s.panel_center, &s.panel_left, &s.panel_right};
  for (const mech3::LabPanel* p : ps)
    o << "panel " << std::hex << id(p->single) << ' ' << id(p->twice) << ' ' << id(p->twice2) << std::dec << '\n';
  o << "message sound=" << std::hex << s.message.x4b0 << " vol=" << zgame::bits(s.message.x4b4) << std::dec << '\n';
  const mech3::LabState& lab = mech3::states().lab;
  o << "lab slot=" << lab.slot_ << " mech=" << lab.mech_ << " chassis sel=" << s.chassis.selected << std::hex
    << " x218=" << s.chassis.x218 << " store=" << s.chassis.store << std::dec << " slots=";
  for (size_t i = 0; i < g_slots.size(); ++i)
    o << (i ? "," : "") << static_cast<int32_t>(call("peek32", {g_slots[i] + 4})) << '/' << std::hex
      << call("peek32", {g_slots[i] + 0xc}) << std::dec << '/' << static_cast<int32_t>(call("peek32", {g_slots[i] + 0x10}));
  o << '\n';
  o << "chassis sort=" << s.chassis.sort << " top=" << s.chassis.top << " items=";
  for (size_t i = 0; i < s.chassis.items.size(); ++i)
    o << (i ? "|" : "") << (s.chassis.items[i]->text ? s.chassis.items[i]->text : "") << '=' << std::hex
      << static_cast<uint32_t>(s.chassis.items[i]->value) << std::dec;
  o << '\n';
  for (size_t i = 0; i < s.chassis.rows.size(); ++i) {
    const auto* t = static_cast<const ztwod::Text*>(s.chassis.rows[i]->attached);
    if (!t) continue;
    const ztwod::Object2D* sel = s.chassis.selected_row ? s.chassis.selected_row->attached : nullptr;
    o << "row" << i << " tons " << t->x << ',' << t->y << " flags=" << std::hex << t->flags << " \"" << t->text
      << "\" named=" << t->named << std::dec << " sel=" << (t == sel ? 1 : 0) << '\n';
  }
  o << "configs sel=" << s.configs.selected << " items=";
  for (size_t i = 0; i < s.configs.items.size(); ++i)
    o << (i ? "|" : "") << (s.configs.items[i]->text ? s.configs.items[i]->text : "") << '=' << std::hex
      << static_cast<uint32_t>(s.configs.items[i]->value) << std::dec;
  o << '\n';
  o << "edit cfg=" << std::hex << s.config_edit.x38c << std::dec << " rename=" << int(s.config_edit.x390)
    << " focus=" << int(s.config_edit.focus) << " flags=" << std::hex << s.config_edit.flags << std::dec
    << " text=\"" << s.config_edit.get_text() << "\"\n";
  for (uint32_t slot : g_slots) {
    const uint32_t c = call("peek32", {slot + 8});
    if (c)
      o << "slotcfg " << std::hex << c << std::dec << " \"" << g_engine->lab_config_name(c) << "\" mod="
        << int(call("peek8", {c + 0x1f})) << '\n';
  }
  for (const auto& d : g_engine->dfns) {
    if (!d.second) continue;
    const uint32_t lst = call("peek32", {d.second + 0x20});
    o << "list" << d.first << ' ';
    bool first = true;
    for (uint32_t c : g_engine->lab_list_configs(lst)) {
      o << (first ? "" : ",") << std::hex << c << std::dec << ':' << g_engine->lab_config_name(c) << ':'
        << int(call("peek8", {c + 0x1f}));
      first = false;
    }
    o << '\n';
  }
  o << "skins arrows=" << s.skins.prev.enabled << ',' << s.skins.next.enabled << " index=" << s.skins.index
    << " items=";
  for (size_t i = 0; i < s.skins.items.size(); ++i)
    o << (i ? "|" : "") << (s.skins.items[i]->text ? s.skins.items[i]->text : "") << '=' << std::hex
      << static_cast<uint32_t>(s.skins.items[i]->value) << std::dec;
  o << '\n';
}

// UNASSIGNED_LIST (step 5b1), as test_lab_screen.py's dump_unassigned.
void dump_unassigned(mech3::LabScreen& s, Dump& d) {
  std::ostringstream& o = d.o;
  const mech3::LabUnassignedList& u = s.unassigned;
  o << "unassigned kind=" << u.kind << " armor=" << int(u.armor_tab) << " sort=" << u.sort << " top=" << u.top
    << " end=" << u.end << " sel=" << u.selected << " size=" << u.size[0] << ',' << u.size[1] << " short="
    << u.short_size[0] << ',' << u.short_size[1] << " long=" << u.long_size[0] << ',' << u.long_size[1]
    << " dy=" << u.down_dy << std::hex << " flags=" << u.flags << std::dec << " arrows=";
  const mech3::LabListArrow* arrows[] = {u.up, u.down};
  for (int i = 0; i < 2; ++i) {
    if (arrows[i]) o << arrows[i]->x << '/' << arrows[i]->y;
    else o << '-';
    o << (i == 0 ? "," : "");
  }
  o << " checks=" << u.weapon.checked << ','
    << u.ammo.checked << ',' << u.equip.checked << ',' << u.armor.checked << '\n';
  o << "uitems ";
  for (size_t i = 0; i < u.items.size(); ++i) {
    const mech3::LabItem* it = u.items[i];
    o << (i ? "|" : "") << (it->text ? it->text : "") << '=' << std::hex << it->object << '/' << it->count << '/'
      << it->font << std::dec;
  }
  o << '\n';
  auto text = [&](const char* k, const ztwod::Text& t) {
    o << ' ' << k << "=\"" << t.text << "\"/" << std::hex << t.flags << '/' << t.font << std::dec;
  };
  for (size_t i = 0; i < u.rows.size(); ++i) {
    const mech3::LabItemRow* r = u.rows[i];
    o << "urow" << i << " idx=" << r->index;
    text("t", *r);
    text("c", r->count);
    text("m", r->mass);
    text("q", r->qty);
    o << '\n';
  }
  o << "titles";
  for (const ztwod::Text* t : u.titles.labels) text("l", *t);
  o << '\n';
}

}  // namespace

extern "C" {

// setup: the slot count the screen is made with, the pilot, the lab's
// store, the slot shown, the flag, the game's slots.
int probe_lab_run(const char* reader, const char* setup, const char* plan, char* out, int size) {
  LabHost host;
  ztwod::set_host(&host);
  reset();
  LabEngine engine;
  g_engine = &engine;
  std::unique_ptr<mech3::Mech3App> app;
  {
    probe_app::Mute mute;
    app = std::make_unique<mech3::Mech3App>(&engine);
    mech3::reset_states();
  }
  mech3::set_app(app.get());
  uint32_t slots = 0;
  {
    std::istringstream ss(setup);
    uint32_t pilot = 0, store = 0, shown = 0, flag = 0, n = 0;
    ss >> std::hex >> slots >> pilot >> store >> shown >> flag >> n;
    g_slots.assign(n, 0);
    for (uint32_t& a : g_slots) ss >> a;
    uint32_t ndfn = 0;
    ss >> ndfn;
    for (uint32_t i = 0; i < ndfn; ++i) {
      uint32_t mech = 0, dfn = 0;
      ss >> mech >> dfn;
      engine.dfns.emplace_back(static_cast<int32_t>(mech), dfn);
    }
    uint32_t full = 0, model = 0, items = 0;
    ss >> full >> model >> items;
    engine.store.full = static_cast<uint8_t>(full);
    engine.store_addr = store;
    for (uint32_t i = 0; i < items; ++i) {
      uint32_t addr = 0, kind = 0, id = 0, count = 0, unlimited = 0, tech = 0, group = 0;
      ss >> addr >> kind >> id >> count >> unlimited >> tech >> group;
      mech3::ConfigItem* it = nullptr;
      if (kind == 0) {
        auto* b = new mech3::BaseItem;
        b->value = static_cast<int32_t>(id);
        it = b;
      } else if (kind == 1) {
        auto* w = new mech3::WeaponItem;
        w->weapon = static_cast<int32_t>(id), w->tech = static_cast<int32_t>(tech);
        it = w;
      } else if (kind == 2) {
        auto* a = new mech3::AmmoItem;
        a->weapon = static_cast<int32_t>(id);
        it = a;
      } else {
        auto* q = new mech3::EquipmentItem;
        q->equipment = static_cast<int32_t>(id), q->tech = static_cast<int32_t>(tech);
        it = q;
      }
      it->count = static_cast<int32_t>(count);
      it->b10 = static_cast<uint8_t>(unlimited);
      it->group = static_cast<int32_t>(group);
      engine.store.items.push_back(it);
      engine.addr_of[it] = addr;
      engine.item_at[addr] = it;
    }
    mech3::states().lab.model_ = model;
    mech3::LabState& lab = mech3::states().lab;
    lab.pilot_ = pilot;
    lab.store_ = store;
    lab.slot_ = static_cast<int32_t>(shown);
    lab.flag_ = static_cast<uint8_t>(flag);
    lab.slots_ = static_cast<int32_t>(slots);
    lab.mech_ = mech3::LabState::kNoMech;
    lab.lab_screen_ = 0x7700;
  }
  const bool per_second = zfix::options().per_second;
  const bool bugs = zfix::options().bugs;
  zfix::options().per_second = false;  // the original's path (zfix.h)
  zfix::options().bugs = false;
  zrdr::init();
  zrdr::close_archives(true);
  zrdr::open_archive(reader, true);
  ztwod::set_page_flip(true);
  std::unique_ptr<mech3::LabScreen> s;
  std::string dump;
  std::istringstream in(plan);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::string op;
    ls >> op;
    if (op == "S") {
      s = std::make_unique<mech3::LabScreen>(engine, static_cast<int32_t>(slots));
      engine.screen = s.get();
      g_kind[&s->message] = 'F';
    } else if (!s) {
      continue;
    } else if (op == "G") {
      const uint32_t p = hex(ls), show = hex(ls);
      s->show_page(static_cast<int32_t>(p), show != 0);
    } else if (op == "B") {
      s->on_button(static_cast<int32_t>(hex(ls)), 0, 0);
    } else if (op == "L") {
      s->chassis.select(static_cast<int32_t>(hex(ls)));
    } else if (op == "Q") {  // the row of the shown slot's loadout's chassis (the test's config_row)
      const mech3::LabState& lab = mech3::states().lab;
      const uint32_t slot = lab.slot_ >= 0 && lab.slot_ < static_cast<int32_t>(g_slots.size())
                                ? g_slots[static_cast<size_t>(lab.slot_)] : 0;
      const uint32_t cfg = slot ? call("peek32", {slot + 8}) : 0;
      if (cfg) {
        const int32_t want = static_cast<int32_t>(call("peek32", {cfg + 0x14}));
        for (int32_t k = 0; k < s->chassis.item_count(); ++k) {
          const auto* it = engine.item_of(static_cast<mech3::Handle>(s->chassis.items[static_cast<size_t>(k)]->value));
          if (it && it->kind() == 0 && it->id() == want) {
            s->chassis.select(k);
            break;
          }
        }
      }
    } else if (op == "Y") {
      const int32_t sh = mech3::states().lab.slot_;
      const uint32_t i = hex(ls);
      if (sh >= 0 && sh < static_cast<int32_t>(g_slots.size()) && call("peek32", {g_slots[static_cast<size_t>(sh)] + 8}))
        s->configs.select(static_cast<int32_t>(i));
    } else if (op == "T") {
      std::string h, t;
      ls >> h;
      for (size_t i = 0; i + 1 < h.size(); i += 2) t += static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16));
      s->config_edit.set_text(t.c_str());
    } else if (op == "N") {
      s->config_edit.on_return();
    } else if (op == "Z") {
      s->config_edit.on_cancel();
    } else if (op == "K") {
      s->skins.select(static_cast<int32_t>(hex(ls)));
    } else if (op == "R") {
      s->refresh();
    } else if (op == "C") {
      mech3::states().lab.set_mech(static_cast<int32_t>(hex(ls)));
    } else if (op == "W") {
      s->unassigned.scroll(static_cast<int32_t>(hex(ls)));
    } else if (op == "X") {
      s->config_edit.x38c = hex(ls);
    } else if (op == "M") {
      const uint32_t x = hex(ls), y = hex(ls);
      s->config_edit.set_pos(static_cast<int32_t>(x), static_cast<int32_t>(y));
    } else if (op == "E") {
      Dump d;
      dump_dialog(*s, d);
      dump_screen(*s, d);
      dump_unassigned(*s, d);
      dump += d.o.str();
    } else if (op == "U") {
      engine.screen = nullptr;
      s.reset();
      dump += "gone\n";
    }
  }
  engine.screen = nullptr;
  s.reset();
  {
    probe_app::Mute mute;
    mech3::reset_states();
  }
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
