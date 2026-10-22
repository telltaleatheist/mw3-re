#include "mech3/pocket_panel.h"

#include <cstring>

#include "gamez/zvideo/zvid_format.h"
#include "crt/msvc_math.h"
#include "gamez/zmath/zmth_matrix.h"
#include "mech3/zai_brain.h"
#include "mech3/mech.h"
#include "mech3/mech_pocket.h"
#include "mech3/sensors.h"

namespace mech3 {

// options.h's (not includable with mech.h)
void render_to_screen(float p[2]);  // 0x0040da30
int32_t pixel_double();             // 0x0040d4b0

namespace {
PocketColours g_colours;
PocketCommands g_commands;
PanelCalls g_calls;
FrameCalls g_frame;
DrawCalls g_draw;
ztwod::Rect g_clip;

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
FrameCalls& frame_calls() { return g_frame; }
DrawCalls& draw_calls() { return g_draw; }
ztwod::Rect& pocket_clip() { return g_clip; }

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
  if (g.marker && g_calls.marker_on) g_calls.marker_on(g.marker, false);
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

namespace {
// A point on the screen: projected (0x0053cf10), or on the mission map in
// pixels (0x004586e0, depth 1) when one is given; the code's bit 3 is off
// the screen (0 on the map).
uint32_t screen_point(zgame::Handle map, const float p[3], float out[3]) {
  if (!map) return g_draw.project ? g_draw.project(p, out) : (out[0] = out[1] = out[2] = 0, 8u);
  int32_t ip[2] = {};
  if (g_draw.map_point) g_draw.map_point(map, p, ip);
  out[0] = static_cast<float>(ip[0]), out[1] = static_cast<float>(ip[1]), out[2] = 1.0f;
  return 0;
}
void entity_position(zgame::Handle e, float out[3]) {
  if (g_draw.entity_position) g_draw.entity_position(e, out);
  else out[0] = out[1] = out[2] = 0;
}
}  // namespace

// Mech3.exe 1.2 @ 0x00467760: a line from the text's edge nearer its unit
// (the left, or the right once the unit is past it), at the text's middle,
// to the unit's place on the screen or the map, in the line's colour,
// clipped to the screen. The target line's unit is the cursor's entity.
void PocketMateItem::draw() {
  const PocketColour& c = g_colours.state[state & 7];
  if (c.ref != color_top || c.ref != color_bottom) {
    color_top = color_bottom = c.ref;
    needs_render = 1;
  }
  FontText::draw();  // 0x005a10d0
  const zgame::Handle e = this == &panel->target ? panel->target_entity
                          : unit                 ? ai::brain_hooks().vehicle_handle(unit)
                                                 : 0;
  if (!e) return;
  float p[3], s[3];
  entity_position(e, p);
  screen_point(pocket_globals().x50, p, s);
  const float mid = static_cast<float>(static_cast<double>(get_y()) -
                                       static_cast<double>(text_rect.top + text_rect.bottom) * -0.5);  // 0x005c2d60
  const float gx = static_cast<float>(get_x());
  const float left = static_cast<float>(text_rect.left) + gx;
  float at = left;
  if (s[0] > left) {
    const float right = static_cast<float>(text_rect.right) + gx;
    at = right > s[0] ? left : right;
  }
  render_to_screen(s);  // 0x0040da30
  ztwod::Line2D line(crt::msvc_ftol(at), crt::msvc_ftol(mid), crt::msvc_ftol(s[0]), crt::msvc_ftol(s[1]), c.pixel);
  line.clip = &g_clip;
  line.clipped = 1;
  line.draw();  // 0x005a68b0
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

// Mech3.exe 1.2 @ 0x00468900: the clip made from the screen, the lines
// drawn (0x0059fd20), then the cursor marked with a triangle: on its
// entity, sized by the entity (+0x68) and its depth when on the screen
// (none off it, nor on the map), yellow; or on the cursor's spot (+0x168),
// 13.86 across, yellow when its node is marked (0x0040ee50), else white.
void PocketPanel::update(float dt) {
  ztwod::Rect r;
  if (g_draw.screen_rect) g_draw.screen_rect(&r);
  g_clip = {r.left, r.top, r.right - 1, r.bottom - 1};
  Layer::update(dt);
  PocketControls& c = *controls;
  const zgame::Handle e = g_calls.node_entity ? g_calls.node_entity(c.x164) : note("0x0040e920", c.x164);
  if (e) {
    ztwod::Shape s;  // (vtable 0x005c2e70: the Shape's but for its destructor)
    s.color = g_colours.yellow.pixel;  // [0x00605ccc]
    s.clip = &g_clip;
    float p[3], q[3];
    entity_position(e, p);
    const float size = g_draw.entity_size ? g_draw.entity_size(e) : 0.0f;
    const float fy = zmth::projection().fy;  // 0x0051e8d0's second half, [0x0073fba0]
    const zgame::Handle map = pocket_globals().x60;
    const uint32_t code = screen_point(map, p, q);
    render_to_screen(q);
    s.set_translation(q);
    float scale = 0.0f;
    if (!map && !(code & 8) && q[2] > 0.0f && !(q[2] > 1.0f)) {
      float t = fy * q[2];
      t = t * size;
      t = t * (pixel_double() ? 2.0f : 1.0f);  // 0x005c2d80 / 0x005c2d78
      scale = t * 1.1547009944915771f;           // 0x005c2d88: 2 / sqrt(3)
    }
    s.set_scale(scale);
    s.update(dt);  // 0x005a3550
  } else if (c.cursor_pos) {
    ztwod::Shape s;
    s.clip = &g_clip;
    float q[3];
    if (g_draw.project) g_draw.project(c.cursor_pos, q);
    else q[0] = q[1] = q[2] = 0;
    render_to_screen(q);
    s.set_translation(q);
    s.color = g_draw.node_marked && g_draw.node_marked(c.x164) ? g_colours.yellow.pixel : g_colours.white.pixel;
    s.set_scale(13.856412f);  // 0x415db3dd
    s.update(dt);
  }
}

// Mech3.exe 1.2 @ 0x0046a410 (and 0x0046a3f0)
void pocket_draw(float dt) {
  PocketControls* p = pocket_globals().active;
  if (!p) return;
  if (g_draw.view_prepare) g_draw.view_prepare();
  else note("0x004b51f0");
  if (p->panel) static_cast<PocketPanel*>(p->panel)->update(dt);
  p->x184 = 0;
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

// Mech3.exe 1.2 @ 0x00468820 (its dt unread): an object under the mouse is
// entered or hovered, a left click (bit 2, released) toggles its press, then
// its left-up; a right click its right-up; one the mouse left is left.
void PocketPanel::route() {
  m = g_frame.route_mouse ? (g_frame.route_mouse(&m), m) : ztwod::host().mouse();  // 0x0051ba60
  for (ztwod::Object2D* o = head; o; o = o->next) {
    if (o->hit(m.x, m.y)) {
      if (!(o->mouse & ztwod::kHover)) {
        o->mouse = static_cast<uint16_t>(o->mouse | ztwod::kHover);
        o->on_enter();
      } else {
        o->on_hover();
      }
      if (m.left & 4) {
        if (o->mouse & ztwod::kPressed) {
          o->mouse = static_cast<uint16_t>(o->mouse & ~ztwod::kPressed);
          o->on_release();
        } else {
          o->mouse = static_cast<uint16_t>(o->mouse | ztwod::kPressed);
          o->on_press();
        }
        o->on_left_up();
      }
      if (m.right & 4) o->on_right_up();
    } else if (o->mouse & ztwod::kHover) {
      o->on_leave();
      o->mouse = static_cast<uint16_t>(o->mouse & ~ztwod::kHover);
    }
  }
}

namespace {
const char* entity_name(zgame::Handle e) {
  const char* n = e ? (g_frame.entity_name ? g_frame.entity_name(e) : reinterpret_cast<const char*>(note("entity.vt+0xa4", e)))
                    : nullptr;
  return n ? n : ztwod::host().message_text(0x89d);  // 0x00576040
}

uint32_t node_of(zgame::Handle e) {
  // The exe reads the marker's +0x04 through null when there is none (the
  // mission's start always makes one).
  return e && g_frame.entity_node ? g_frame.entity_node(e) : 0;
}

void marker_on(bool on) {
  const zgame::Handle mk = pocket_globals().marker;
  if (g_calls.marker_on) g_calls.marker_on(mk, on);
  else note("0x004eb170", on);
}

// The cursor on `e`: the panel's target line names it (the name, else the
// message 0x89d) and the marker goes when it is another.
void name_cursor(PocketPanel* p, zgame::Handle e) {
  pocket_globals().cursor = e;
  p->target_entity = e;
  p->target.set_text("   %s", entity_name(e));
}

// The cursor on the marker, put at the mode's place and shown.
void cursor_on_marker(PocketControls& c, PocketPanel* p) {
  const zgame::Handle mk = pocket_globals().marker;
  c.x164 = node_of(mk);
  name_cursor(p, mk);
  if (mk) {
    if (g_frame.marker_move) g_frame.marker_move(mk, c.cursor_pos);
    else note("marker.vt+0x18", mk);
    marker_on(true);
  }
}
}  // namespace

void pocket_frame(PocketControls& c, float dt) {
  (void)dt;
  auto* p = static_cast<PocketPanel*>(c.panel);
  Mech* player = c.owner;
  const zgame::Handle hud = player && g_frame.view_hud ? g_frame.view_hud(*player) : 0;
  p->route();
  const uint32_t net = pocket_calls().net_element ? pocket_calls().net_element() : 0;
  ai::Contact* target = player ? player->target : nullptr;  // +0x6c
  // FIX (bug): the exe reads the HUD's +0x70 through null when the player is
  // not the view's mech; here no HUD is no map.
  const bool map_up = hud && g_frame.hud_map_up && g_frame.hud_map_up(hud);
  c.x1b8 = map_up ? (g_frame.map ? g_frame.map() : note("0x00491c80")) : 0;
  pocket_globals().x60 = pocket_globals().x50 = c.x1b8;
  const zgame::Handle mk = pocket_globals().marker;
  if (!c.x184) {
    bool moved = true;
    const uint32_t trig = g_frame.trigger ? g_frame.trigger() : note("0x0051d300", 0x3d);
    if (trig & 4) {
      bool hit = false;
      uint32_t node = 0;
      auto from_reticle = [&] {
        const zgame::Handle rec = g_frame.reticle_pick ? g_frame.reticle_pick(hud) : note("0x00444330");
        hit = rec != 0;
        node = hit && g_frame.record_node ? g_frame.record_node(rec) : 0;
        if (g_frame.reticle_place) g_frame.reticle_place(hud, c.place);
      };
      if (c.x1b8) {
        int32_t x = 0, y = 0, w = 0, h = 0;
        if (g_frame.map_mouse) g_frame.map_mouse(hud, &x, &y);
        if (g_frame.map_size) g_frame.map_size(c.x1b8, &w, &h);
        if (x < w && y < h) {
          hit = g_frame.map_pick && g_frame.map_pick(static_cast<float>(x), static_cast<float>(y), c.place, &node);
        } else {
          from_reticle();
        }
      } else {
        from_reticle();
      }
      if (hit && node) {
        c.x164 = node;
        c.cursor_pos = c.place;
        const zgame::Handle e = g_calls.node_entity ? g_calls.node_entity(c.x164) : note("0x0040e920", c.x164);
        pocket_globals().cursor = e;
        p->target_entity = g_calls.node_entity ? g_calls.node_entity(c.x164) : e;
        p->target.set_text("   %s", entity_name(p->target_entity));
        const zgame::Handle t = g_calls.node_entity ? g_calls.node_entity(c.x164) : e;
        if (g_frame.pick_target) g_frame.pick_target(*player, t);
        else note("0x004a7b40", t);
        if (!pocket_globals().cursor) {
          cursor_on_marker(c, p);  // a bare spot: the marker there
        } else if (mk) {
          marker_on(false);  // (even when the cursor is the marker)
        }
      } else {
        moved = false;
      }
    } else {
      const bool same = target && c.aim.entity == target->entity && c.aim.owner == target->owner;
      if (target && !same) {
        const zgame::Handle e = g_frame.contact_entity ? g_frame.contact_entity(target) : note("0x004e6fe0");
        c.x1c0 = 0, c.x1c1 = 1;
        c.x164 = node_of(e);
        c.cursor_pos = g_frame.entity_place ? g_frame.entity_place(e) : nullptr;
        name_cursor(p, e);
        if (e && mk && e != mk) marker_on(false);
      } else {
        moved = false;
        if (!target) c.x1c0 = 0, moved = true;
        if (net && c.net_element != net) {
          c.net_element = net;
          c.x1c0 = 1, c.x1c1 = 0;
          const zgame::Handle e = g_frame.net_entity ? g_frame.net_entity(net) : 0;
          if (e) {
            c.x164 = node_of(e);
            c.cursor_pos = g_frame.entity_place ? g_frame.entity_place(e) : nullptr;
            name_cursor(p, e);
            if (mk && e != mk) marker_on(false);
          } else {
            c.x164 = node_of(mk);
            c.cursor_pos = g_frame.net_place ? g_frame.net_place(net) : nullptr;
            name_cursor(p, mk);
            if (mk) {
              if (g_frame.marker_move) g_frame.marker_move(mk, c.cursor_pos);
              marker_on(true);
            }
          }
          moved = true;
        }
      }
    }
    if (moved) p->picked(0, p->places);
  }
  c.aim = target ? *target : ai::Contact{};  // the template 0x005e9d10
  switch (c.x148) {
    case 0:
      if (c.x184 == 1) c.x148 = 1, c.x14c = 3;
      break;
    case 1:
    case 3:
      if (--c.x14c < 1) ++c.x148;
      break;
    case 2:
      if (pocket_calls().view_pocket) pocket_calls().view_pocket(false);  // 0x004b5290(0)
      c.x154 |= 1;
      if (pocket_calls().set_camera_mode) pocket_calls().set_camera_mode(c.saved_camera);  // 0x0040ae70
      c.x14c = 1;
      ++c.x148;
      break;
    case 4:
      c.wrapped = 1;  // +0x38: the mech's update unwraps (0x00469cf0)
      break;
    default:
      break;
  }
}

}  // namespace mech3
