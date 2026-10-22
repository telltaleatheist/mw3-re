#include "mech3/pocket_panel.h"

#include <cstring>

#include "gamez/zvideo/zvid_format.h"
#include "mech3/mech.h"
#include "mech3/mech_pocket.h"
#include "mech3/sensors.h"

namespace mech3 {
namespace {
PocketColours g_colours;
PocketCommands g_commands;
PanelCalls g_calls;

uint32_t note(const char* key, uint32_t arg = 0) {
  PocketCalls& pc = pocket_calls();
  return pc.unported ? pc.unported(key, arg) : 0;
}

void play(int32_t sound) {
  if (sound) ztwod::host().sound_play(sound, 1.0f);  // 0x0056e3d0
}

// 0x004686e0
PocketColour colour(uint8_t r, uint8_t g, uint8_t b) {
  return {static_cast<uint32_t>(r) | static_cast<uint32_t>(g) << 8 | static_cast<uint32_t>(b) << 16,
          zvid::pack_bytes(r, g, b)};
}

Mech* entity_mech(zgame::Handle e) {
  return g_calls.entity_mech ? g_calls.entity_mech(e) : reinterpret_cast<Mech*>(note("entity.vt+0x2c", e));
}

// 0x00467e00 (0x00467e30 on an entity's vt+0x2c): a lancemate under orders
// and up.
// The port's entities that are not mechs (none answers here) are none.
bool under_orders(const Mech* m) { return m && (m->xbc & 0x10) && m->active; }

// The panel's +0x58: the mode's controls' mech (their IControls owner).
Mech* player_of(const PocketPanel* p) { return p->controls ? p->controls->owner : nullptr; }

// 0x00467f70 / 0x00468090's record and 0x00467e90: a line put on the panel
// (on the list's end and in its layer); without text it is hidden, else
// "%3d %s" (its number, 1-based) or "%s %s" (the key, the order), shown,
// rendered and the widest kept; the next line's y below it either way.
struct Lines {
  PocketPanel* panel;
  int32_t* widest;
  int32_t x;
  int32_t* y;
  int32_t line_h;
  void add(PocketItem* item, const char* prefix, const char* text) {
    panel->items.push_back(item);
    panel->add(item);  // 0x0059fbe0
    if (text) {
      if (prefix) item->set_text("%s %s", prefix, text);
      else item->set_text("%3d %s", static_cast<int32_t>(panel->items.size()), text);
      item->show_for(-1.0f);
      if (item->needs_render) item->render();
      if (*widest < item->width) *widest = item->width;
    } else {
      item->hide();
    }
    *y += line_h;
  }
};
}  // namespace

PocketColours& pocket_colours() { return g_colours; }
PocketCommands& pocket_commands() { return g_commands; }
PanelCalls& panel_calls() { return g_calls; }

// Mech3.exe 1.2 @ 0x00467d80 (the rows of 0x005e5050: an action, its
// message, the order; the last is the command mode's own key, order 2)
void pocket_load_commands() {
  static const int32_t kRows[6][3] = {{84, 2195, 10}, {85, 2196, 11}, {82, 2142, 0},
                                      {81, 2143, 1},  {86, 2197, 12}, {79, 2204, 2}};
  int32_t n = 0;
  for (const auto& row : kRows) {
    const zgame::Handle k = g_calls.key_binding ? g_calls.key_binding(row[0]) : note("0x0051d1f0", row[0]);
    if (!k) continue;
    PocketCommand& c = g_commands.rows[n];
    if (g_calls.key_name) g_calls.key_name(k, c.key, 0xf);
    else note("0x0051d330", k);
    c.key[0xf] = 0;
    const char* s = ztwod::host().message_text(static_cast<uint32_t>(row[1]));  // 0x00576040
    std::strncpy(c.name, s ? s : "", 0x3f);
    c.name[0x3f] = 0;
    c.order = row[2];
    ++n;
  }
  g_commands.count = n;
}

// Mech3.exe 1.2 @ 0x0046a930 (a debug line when loaded twice, another
// for a mode left up, which is forgotten)
void pocket_load_sounds() {
  PocketGlobals& g = pocket_globals();
  if (g.active) g.active = nullptr;
  ztwod::Host& h = ztwod::host();
  auto load = [&h](int32_t& s, const char* name) {
    if (!s) s = h.sound_find(name);  // 0x0056f6f0
  };
  load(g.sure_sir, "sure_sir");
  load(g.right_away, "right_away");
  load(g.so_long, "solong");
  load(g.no_way, "noway");
  load(g.cannot, "cannot");
  load(g.select, "pckt_select");
  load(g.rollover, "pckt_rollover");
  g.sounds_loaded = true;
}

// Mech3.exe 1.2 @ 0x0046aaa0
void pocket_mission_start() {
  pocket_load_sounds();
  PocketGlobals& g = pocket_globals();
  g.marker = g_calls.make_marker ? g_calls.make_marker() : note("0x00468100");
  if (g.marker && g_calls.marker_off) g_calls.marker_off(g.marker);
  pocket_load_commands();
}

// Mech3.exe 1.2 @ 0x0046ab10 (0x0046aa40)
void pocket_mission_end() {
  PocketGlobals& g = pocket_globals();
  g.sure_sir = g.right_away = g.so_long = g.no_way = g.cannot = g.select = g.rollover = 0;
  g.sounds_loaded = false;
  if (g.active) {
    delete g.active;  // vt+0x00(1) through its IControls
    g.active = nullptr;
  }
  g.x50 = g.x60 = 0;
  g.marker = 0;
}

// Mech3.exe 1.2 @ 0x004674f0
PocketItem::PocketItem(PocketPanel* p, const char* s, int32_t px, int32_t py) : FontText(nullptr, 0, 0), panel(p) {
  set_text_plain(s);
  x = px, y = py;
  set_font(ztwod::host().font_find("ia_small_grey"));  // 0x00517680 on 0x005e50c0
}

// Mech3.exe 1.2 @ 0x004675e0
void PocketItem::on_enter() {
  state |= 1;
  play(pocket_globals().rollover);
}

// Mech3.exe 1.2 @ 0x00467610
void PocketItem::on_leave() { state &= ~1u; }

// Mech3.exe 1.2 @ 0x00467580
void PocketItem::restate() {
  if (hit(panel->m.x, panel->m.y)) state |= 1;
  else state &= ~1u;
  if (mouse & ztwod::kPressed) state |= 2;
  else state &= ~2u;
}

void PocketItem::refresh() { restate(); }

// Mech3.exe 1.2 @ 0x00467620
PocketMateItem::PocketMateItem(PocketPanel* p, Mech* u, const char* s, int32_t px, int32_t py)
    : PocketItem(p, s, px, py), unit(u) {
  state |= 4;
}

// Mech3.exe 1.2 @ 0x004676c0
void PocketMateItem::on_press() {
  state |= 2;
  panel->controls->x184 = 2;
  panel->picked(1, panel->places);
  play(pocket_globals().select);
}

// Mech3.exe 1.2 @ 0x00467710
void PocketMateItem::on_release() {
  state &= ~2u;
  panel->controls->x184 = 2;
  panel->picked(-1, panel->places);
  play(pocket_globals().select);
}

// Mech3.exe 1.2 @ 0x004676a0
void PocketMateItem::refresh() {
  state |= 4;
  restate();
}

// Mech3.exe 1.2 @ 0x00467690
Mech* PocketMateItem::entity() { return unit; }

// Mech3.exe 1.2 @ 0x00467960 (and each kind's 0x0046a570, 0x0046a640,
// 0x0046a710, 0x0046a780, 0x0046a7e0)
PocketOrderItem::PocketOrderItem(Kind k, int32_t o, PocketPanel* p, int32_t px, int32_t py)
    : PocketItem(p, nullptr, px, py), kind(k), order(o) {
  state = 0;
}

// Mech3.exe 1.2 @ 0x00467d40
void PocketOrderItem::draw() {
  const uint32_t c = g_colours.state[state & 7].ref;
  if (c != color_top || c != color_bottom) {
    color_top = color_bottom = c;
    needs_render = 1;
  }
  FontText::draw();  // 0x005a10d0
}

// Mech3.exe 1.2 @ 0x00467c90: when lit, the mode's order (+0x160) is this
// one's and +0x184 is 1 (the mode then leaves with it, 0x00469d00).
void PocketOrderItem::on_press() {
  const uint32_t was = state;
  state |= 2;
  if (was & 4) {
    panel->controls->order = order;
    panel->controls->x184 = 1;
  } else {
    panel->controls->x184 = 2;
  }
  play(pocket_globals().select);
}

// Mech3.exe 1.2 @ 0x00467d00
void PocketOrderItem::on_release() {
  state &= ~2u;
  panel->controls->x184 = 2;
  play(pocket_globals().select);
}

// Mech3.exe 1.2 @ 0x0046a7b0 / 0x0046a5a0 / 0x0046a670 / 0x0046a740 /
// 0x0046a810: lit when a lancemate is picked and (kEnemy) the cursor's
// target is the marker or not the player's ally, (kAlly) the marker or his
// ally, (kSpot) the cursor has a place (+0x168), (kOne) one is picked and
// the mission has a repair bay.
void PocketOrderItem::refresh() {
  PocketControls* c = panel->controls;
  bool lit = panel->picked_count > 0;
  if (kind == kEnemy || kind == kAlly) {
    const zgame::Handle t = g_calls.node_entity ? g_calls.node_entity(c->x164) : note("0x0040e920", c->x164);
    Mech* player = player_of(panel);
    if (!lit || !t) {
      lit = false;
    } else if (t != pocket_globals().marker) {
      const uint32_t team = g_calls.entity_team ? g_calls.entity_team(t) : note("entity.vt+0x28", t);
      const bool ally = (player->allies & team) != 0;
      lit = kind == kEnemy ? !ally : ally;
    }
  } else if (kind == kSpot) {
    lit = lit && c->cursor_pos;
  } else if (kind == kOne) {
    lit = panel->picked_count == 1 && (g_calls.repair_bays ? g_calls.repair_bays() : note("0x0045a000")) > 0;
  }
  if (lit) state |= 4;
  else state &= ~4u;
  restate();
}

// Mech3.exe 1.2 @ 0x004679a0
PocketOrderItem* make_order_item(int32_t order, PocketPanel* p, int32_t px, int32_t py) {
  PocketOrderItem::Kind k;
  switch (order) {
    case 0: case 1: case 8: k = PocketOrderItem::kAny; break;
    case 3: case 10: k = PocketOrderItem::kEnemy; break;
    case 4: case 11: k = PocketOrderItem::kAlly; break;
    case 5: case 9: k = PocketOrderItem::kSpot; break;
    case 7: case 12: k = PocketOrderItem::kOne; break;
    default: return nullptr;
  }
  return new PocketOrderItem(k, order, p, px, py);
}

// Mech3.exe 1.2 @ 0x00468250 (the mode's set_owner: its +0x178, +0x17c, 12)
PocketPanel::PocketPanel(PocketControls* c, int32_t px, int32_t py, int32_t lh)
    : Window(1, 1), controls(c), line_h(lh), target(this, nullptr, "   target", px, py) {
  if (!g_colours.made) {
    g_colours.made = true;
    for (int i = 0; i < 4; ++i) g_colours.state[i] = colour(0xbe, 0xbe, 0xbe);
    g_colours.state[4] = colour(0, 0xff, 0);
    g_colours.state[5] = colour(0x80, 0x80, 0);
    g_colours.state[6] = colour(0xff, 0xff, 0);
    g_colours.state[7] = colour(0xff, 0, 0);
    g_colours.yellow = colour(0xff, 0xff, 0);
    g_colours.white = colour(0xff, 0xff, 0xff);
  }
  set_active(1);  // vt+0x04
  rect = {px, py, px, py};
  pocket_globals().cursor = 0;
  Mech* player = c ? c->owner : nullptr;
  const std::vector<zgame::Handle>& world = ai::entity_list();  // [0x0073655c..0x00736560]
  int32_t n = 0;
  for (zgame::Handle e : world)
    if (under_orders(entity_mech(e))) ++n;
  mates = n;
  places = n > 3 ? n : 3;
  items.reserve(static_cast<size_t>(places + g_commands.count));
  add(&target);
  target.state |= 7;
  target.set_pos(px, py);
  target.set_text("   target");
  target.show_for(-1.0f);
  const uint32_t red = g_colours.state[7].ref;  // 0x0046b740
  if (red != target.color_top || red != target.color_bottom) {
    target.color_top = target.color_bottom = red;
    target.needs_render = 1;
  }
  if (target.needs_render) target.render();
  int32_t widest = target.width;
  int32_t y = py + lh;
  Lines lines{this, &widest, px, &y, lh};
  // 0x00467fa0 on each: a lancemate under orders, up, the player's seen
  // one (+0xbc bit 3) on his side (its allies, the player's team), named.
  for (zgame::Handle e : world) {
    Mech* m = e ? entity_mech(e) : nullptr;
    if (!under_orders(m) || !(m->xbc & 8)) continue;
    if (!(m->allies & player->team)) continue;  // the player's vt+0x28 +0xc0
    const char* name = g_calls.unit_name ? g_calls.unit_name(*m) : reinterpret_cast<const char*>(note("mech.vt+0xa4"));
    if (!name) continue;
    lines.add(new PocketMateItem(this, m, nullptr, px, y), nullptr, name);
  }
  items.resize(static_cast<size_t>(places), nullptr);  // 0x0049ced0 / 0x004aa700
  for (int32_t i = 0; i < g_commands.count; ++i) {  // 0x004680c0 on each
    PocketCommand& cmd = g_commands.rows[i];
    if (PocketOrderItem* item = make_order_item(cmd.order, this, px, y)) lines.add(item, cmd.key, cmd.name);
  }
  rect.right = widest + rect.left;
  rect.bottom = (places + g_commands.count) * lh + rect.top;
}

// Mech3.exe 1.2 @ 0x00468720
PocketPanel::~PocketPanel() {
  for (PocketItem*& i : items) {
    delete i;  // vt+0x00(1)
    i = nullptr;
  }
}

int32_t pocket_panel_units(PocketControls& c, PocketUnit* out, int32_t cap) {
  auto* p = static_cast<PocketPanel*>(c.panel);
  if (!p) return 0;
  int32_t n = 0;
  for (int32_t i = 0; i < p->mates && n < cap; ++i, ++n) {
    PocketItem* item = static_cast<size_t>(i) < p->items.size() ? p->items[static_cast<size_t>(i)] : nullptr;
    out[n] = PocketUnit{};
    if (item) out[n].picked = (item->state & 2) != 0, out[n].mech = item->entity();
  }
  return n;
}

// Mech3.exe 1.2 @ 0x00469060
bool PocketPanel::pick(int32_t unit) {
  const int32_t i = unit - 1;
  if (i < 0 || i >= static_cast<int32_t>(items.size()) || !items[static_cast<size_t>(i)]) return false;
  PocketItem* item = items[static_cast<size_t>(i)];
  if (item->mouse & ztwod::kPressed) {
    item->mouse = static_cast<uint16_t>(item->mouse & ~ztwod::kPressed);
    item->on_release();
  } else {
    item->mouse = static_cast<uint16_t>(item->mouse | ztwod::kPressed);
    item->on_press();
  }
  return true;
}

// Mech3.exe 1.2 @ 0x004687e0
void PocketPanel::picked(int32_t delta, int32_t from) {
  picked_count += delta;
  for (size_t i = static_cast<size_t>(from); i < items.size(); ++i) items[i]->refresh();
}

}  // namespace mech3
