// Probe for test_lab_list.py (docs/mechlab.md, The item lists): the mech
// lab's item list base on the ported zTwoD, bound from a synthetic dialog in
// a screen that logs its on_button, then the test's plan; every call out
// recorded through the calltrace callback, the dialog and the list dumped.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "mech3/lab_items.h"
#include "mech3/mech_config.h"
#include "mech3/pilot.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;
using mech3::Arg;
using mech3::Handle;

std::deque<std::string> g_text;
std::vector<uint32_t> g_items;  // the emulator's fake store items
uint32_t g_store_at = 0;        // and its store
mech3::Store* g_store = nullptr;

const char* text_at(uint32_t a) {
  if (!a) return nullptr;
  std::string s;
  for (uint32_t i = 0; i < 256; ++i) {
    const char c = static_cast<char>(call("peek8", {a + i}));
    if (!c) break;
    s += c;
  }
  g_text.push_back(s);
  if (g_text.size() > 64) g_text.pop_front();
  return g_text.back().c_str();
}

struct ListEngine : mech3::Engine {
  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  int32_t display_width() override { return static_cast<int32_t>(call("0x0040d760", {})); }
  int32_t display_height() override { return static_cast<int32_t>(call("0x0040d770", {})); }
  int32_t fast_menu() override { return static_cast<int32_t>(call("0x0040cec0", {}) & 0xff); }
  const char* message_format(int32_t id, const char* arg) override {
    call("0x00575fd0", {id, arg ? arg : ""});
    char buf[0x100];
    std::snprintf(buf, sizeof buf, "F%x:%s", static_cast<uint32_t>(id), arg ? arg : "");
    g_text.push_back(buf);
    return g_text.back().c_str();
  }
  int32_t lab_item_kind(Handle h) override { return static_cast<int32_t>(call("item.vt+0xc", {h})); }
  std::string lab_item_name(Handle h) override {
    const char* s = text_at(call("item.vt+0x20", {h}));
    return s ? s : "";
  }
  int32_t lab_item_slots(Handle h) override { return static_cast<int32_t>(call("item.vt+0x18", {h})); }
  const char* part_text(int32_t part) override { return text_at(call("0x00484d70", {part})); }
  mech3::Store* store_of(Handle h) override { return h && h == g_store_at ? g_store : nullptr; }
  Handle item_handle(const mech3::ConfigItem* it) override {
    size_t i = 0;
    for (const mech3::ConfigItem* x : g_store->items) {
      if (x == it) return i < g_items.size() ? g_items[i] : 0;
      ++i;
    }
    return 0;
  }
};

// The test's dialog: a screen whose on_button (+0x14) is logged.
struct ProbeScreen : mech3::MenuScreen {
  explicit ProbeScreen(mech3::Engine& e) : MenuScreen(e, 0) {}
  void on_button(int32_t id, int32_t a, int32_t b) override { call("screen.vt+0x14", {id, a, b}); }
};

// test_lab_list.py's dump_list.
struct ListDump {
  std::ostringstream o;
  std::vector<std::string> line;
  void kv(const char* k, int64_t v) {
    char b[48];
    std::snprintf(b, sizeof b, "%s=%x", k, static_cast<uint32_t>(v));
    line.push_back(b);
  }
  void f(const char* k, float v) {
    uint32_t u;
    std::memcpy(&u, &v, 4);
    kv(k, u);
  }
  void tx(const char* k, const zimg::Texture* t) {
    char b[48];
    std::snprintf(b, sizeof b, "%s=T%x", k, id(t));
    line.push_back(b);
  }
  void rc(const char* k, const ztwod::Rect& r) {
    char b[96];
    std::snprintf(b, sizeof b, "%s=%d,%d,%d,%d", k, r.left, r.top, r.right, r.bottom);
    line.push_back(b);
  }
  void s(const char* k, const char* v) {
    std::string t = std::string(k) + "=\"";
    if (!v) {
      t += "(null)";
    } else {
      for (const char* c = v; *c; ++c) t += *c == ' ' ? '_' : *c;
    }
    line.push_back(t + "\"");
  }
  void flush(const std::string& head) {
    o << head << ' ';
    for (size_t i = 0; i < line.size(); ++i) o << (i ? " " : "") << line[i];
    o << " \n";
    line.clear();
  }
  void base(const ztwod::Object2D* p) {
    kv("flags", p->flags);
    kv("x", p->x);
    kv("y", p->y);
    kv("mouse", p->mouse);
  }
  void image(const ztwod::Image* p) {
    base(p);
    tx("img", p->image);
  }
  void text(const ztwod::Text* p) {
    base(p);
    s("text", p->text);
    kv("align", p->align);
    kv("top", p->color_top);
    kv("hfont", p->hfont);
    kv("w", p->width);
    kv("h", p->height);
    kv("nr", p->needs_render);
    kv("named", p->named);
    f("ft", p->flash_timer);
    f("frate", p->flash_rate);
    kv("fon", p->flash_on);
    kv("fmode", p->flash_mode);
    rc("tr", p->text_rect);
  }
  void control(const ztwod::Control* p) {
    image(p);
    kv("en", p->enabled);
    rc("bounds", p->bounds);
  }
  void row(const mech3::LabItemRow* r, bool index) {
    text(r);
    if (index) kv("index", r->index);
    kv("height", r->height);
    image(&r->panel);
    kv("pcolor", r->panel.color);
    line.push_back("\n   count");
    text(&r->count);
    line.push_back("\n   mass");
    base(&r->mass);
    line.push_back("\n   crit");
    base(&r->crit);
    line.push_back("\n   line");
    base(&r->line);
    kv("end", r->line.end_x);
    kv("end_y", r->line.end_y);
    kv("color", r->line.color);
  }
};

void dump_list(mech3::LabItemList& l, std::ostringstream& out) {
  ListDump d;
  d.kv("top", l.top);
  d.kv("end", l.end);
  d.kv("sel", l.selected);
  d.kv("rowh", l.row_height);
  d.kv("sx", l.size[0]);
  d.kv("sy", l.size[1]);
  d.kv("spx", l.spacing[0]);
  d.kv("spy", l.spacing[1]);
  d.kv("ox", l.offset[0]);
  d.kv("oy", l.offset[1]);
  d.rc("rect", l.rect);
  for (auto [k, v] : std::initializer_list<std::pair<const char*, int64_t>>{
           {"fn", l.font_normal}, {"fs", l.font_selected}, {"fr", l.font_rollover}, {"fl", l.font_large},
           {"fst", l.font_static}, {"fc", l.font_count}, {"fm", l.font_mass}, {"fg", l.font_grey},
           {"bdx", l.bar_dx}, {"bdy", l.bar_dy}, {"rdx", l.roll_dx}, {"rdy", l.roll_dy}, {"inside", l.inside},
           {"snd0", l.sounds[0]}, {"snd1", l.sounds[1]}, {"play0", l.playing[0]}, {"play1", l.playing[1]},
           {"alit", l.arrow_lit}})
    d.kv(k, v);
  d.f("rt", l.repeat_timer);
  d.f("rd", l.repeat_delay);
  d.f("rr", l.repeat_rate);
  d.kv("rep", l.repeating);
  int lit = l.lit ? 0xff : 0xfe;
  for (size_t i = 0; i < l.rows.size(); ++i)
    if (l.rows[i] == l.lit) lit = static_cast<int>(i);
  if (l.lit && l.lit == l.selected_row) lit = 0x99;
  d.kv("lit", lit);
  d.control(&l);
  d.flush("list");
  for (size_t i = 0; i < l.items.size(); ++i) {
    const mech3::LabItem* it = l.items[i];
    d.s("text", it->text);
    uint32_t ob = it->object;
    for (size_t k = 0; k < g_items.size(); ++k)
      if (g_items[k] == ob) ob = static_cast<uint32_t>(k + 1);
    d.kv("obj", ob);
    d.kv("count", it->count);
    d.kv("font", it->font);
    d.flush("item" + std::to_string(i));
  }
  for (size_t i = 0; i < l.rows.size(); ++i) {
    d.row(l.rows[i], true);
    d.flush("row" + std::to_string(i));
  }
  if (l.selected_row) {
    d.row(l.selected_row, false);
    d.flush("selrow");
  }
  const std::pair<const char*, ztwod::Object2D*> ends[] = {
      {"up", l.up}, {"down", l.down}, {"bar", l.bar_image}, {"roll", l.roll_image}};
  for (auto [k, o] : ends) {
    if (!o) {
      d.flush(std::string(k) + "=none");
      continue;
    }
    if (o == l.up || o == l.down) d.control(static_cast<ztwod::Control*>(o));
    else d.image(static_cast<ztwod::Image*>(o));
    d.flush(k);
  }
  out << d.o.str();
}

}  // namespace

extern "C" {

// Opens `reader` (the synthetic dialog file), makes the screen and loads
// LISTDLG, runs `plan` (test_lab_list.py's lines) on a list bound as `name`;
// `tokens` are the emulator's fake items and store. Dumps into `out`;
// returns the length, or -1.
int probe_lab_list(const char* reader, const char* name, const char* tokens, const char* plan, char* out, int size) {
  RecordingHost host;
  ztwod::set_host(&host);
  reset();
  ListEngine engine;
  const bool per_second = zfix::options().per_second;
  const bool bugs = zfix::options().bugs;
  zfix::options().per_second = false;
  zfix::options().bugs = false;
  g_items.clear();
  {
    std::istringstream ts(tokens);
    uint32_t v;
    while (ts >> std::hex >> v) g_items.push_back(v);
  }
  g_store_at = g_items.back();
  g_items.pop_back();
  zrdr::init();
  zrdr::close_archives(true);
  zrdr::open_archive(reader, true);
  std::string dump;
  zrdr::Node* tree = zrdr::load("listdlg.zrd");
  {
    auto screen = std::make_unique<ProbeScreen>(engine);
    screen->load_from(tree, "LISTDLG", false);
    mech3::LabItemList* list = nullptr;
    std::istringstream in(plan);
    std::string line;
    static const char* const kNames[] = {"ER Laser (L)", "LRM 20", "Gauss Rifle", "Heat Sink",
                                         "a much longer item name for the row", "50%", ""};
    while (std::getline(in, line)) {
      std::istringstream ls(line);
      char op;
      ls >> op;
      if (op == 'N') {
        list = new mech3::LabItemList(engine, -1);
        call("bind", {});
        screen->bind_button(list, name);
      } else if (op == 'T') {
        int i, n;
        ls >> i >> n;
        list->add_text(i < 7 ? kNames[i] : nullptr, n);
      } else if (op == 'O') {
        int i, n;
        ls >> i >> n;
        list->add_object(g_items[static_cast<size_t>(i)], n);
      } else if (op == 'P') {
        int p;
        ls >> p;
        list->add_part(p);
      } else if (op == 'C') {
        list->clear();
      } else if (op == 'S') {
        int i;
        ls >> i;
        list->select(i);
      } else if (op == 'K') {
        int by;
        ls >> by;
        list->scroll(by);
      } else if (op == 'G') {
        int n;
        ls >> n;
        delete g_store;
        g_store = new mech3::Store;
        for (int k = 0; k < n; ++k) g_store->add_weapon(k, 0);
        list->store = g_store_at;
        list->refill();
      } else if (op == 'L') {
        list->layout();
      } else if (op == 'Q') {
        int i;
        ls >> i;
        if (i < list->item_count()) list->items[static_cast<size_t>(i)]->font = list->font_large;
      } else if (op == 'A') {
        int v;
        ls >> v;
        if (v) list->show_for(-1.0f);
        else list->hide();
      } else if (op == 'D') {
        int v;
        ls >> v;
        screen->set_active(v);
      } else if (op == 'F') {
        uint32_t dt;
        ztwod::MouseState m{};
        uint32_t* w = reinterpret_cast<uint32_t*>(&m);
        ls >> std::hex >> dt;
        for (int i = 0; i < 15; ++i) ls >> w[i];
        g_mouse.push_back(m);
        screen->update(zgame::from_bits(dt));
        ++g_frame;
      } else if (op == 'X') {
        Dump d;
        dump_dialog(*screen, d);
        std::ostringstream o;
        dump_list(*list, o);
        dump = d.o.str() + o.str();
        screen->remove(list);
        delete list;
        list = nullptr;
        screen.reset();
      }
    }
  }
  zrdr::free_node(tree);
  delete g_store;
  g_store = nullptr;
  zrdr::close_archives(true);
  zfix::options().per_second = per_second;
  zfix::options().bugs = bugs;
  ztwod::set_host(nullptr);
  if (static_cast<int>(dump.size()) >= size) return -1;
  std::memcpy(out, dump.data(), dump.size() + 1);
  return static_cast<int>(dump.size());
}

}  // extern "C"
