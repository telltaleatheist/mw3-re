// Probes for test_hud.py (docs/hud.md): the HUD (mech3/hud) built on the
// ported zTwoD with every call out recorded; dumped the way test_hud.py dumps
// the original's object from the emulator's memory.
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

#include "mech3/hud.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;

uint32_t g_hud_addr = 0;
mech3::Hud* g_hud = nullptr;

struct Hooks : mech3::HudHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return call(key, args); }
  float entry_flip(uint32_t entry) override { return zgame::from_bits(call("peek32", {entry + 0xd0})); }
  void player_camera_clip(float* n, float* f) override {
    const uint32_t a = call("0x004f3b00", {});
    *n = zgame::from_bits(call("peek32", {a}));
    *f = zgame::from_bits(call("peek32", {a + 4}));
  }
  static ztwod::Rect rect_at(uint32_t global) {
    const uint32_t r = call("peek32", {call("peek32", {global})});  // [global] -> [+0] -> the rect
    return {static_cast<int32_t>(call("peek32", {r})), static_cast<int32_t>(call("peek32", {r + 4})),
            static_cast<int32_t>(call("peek32", {r + 8})), static_cast<int32_t>(call("peek32", {r + 12}))};
  }
  ztwod::Rect display_rect() override { return rect_at(0x5f4d20); }
  ztwod::Rect window_rect() override { return rect_at(0x5f4d24); }
  uint32_t id(const void* p) override { return p == g_hud ? g_hud_addr : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
};
Hooks g_hooks;

// A piece's name by its place in the HUD (as the emulator's offsets).
std::string piece_name(const ztwod::Object2D* p) {
  if (!p) return "0";
  const mech3::Hud& h = *g_hud;
  const std::pair<const void*, const char*> named[] = {
      {&h.radar, "radar"}, {&h.compass, "compass"}, {&h.reticle, "reticle"}, {&h.gauges, "gauges"},
      {&h.mfd, "mfd"}, {&h.target, "target"}, {&h.weapons, "weapons"}, {&h.objectives, "objectives"},
      {&h.timer, "timer"}, {&h.equipment, "equipment"}};
  for (auto& [q, n] : named)
    if (q == p) return n;
  return "?";
}

// test_hud.py's order: FontText's fields, then the rest; a Text's flash after.
void ftext(Dump& d, ztwod::FontText* p) {
  d.fonttext(p);
  d.kv("font", p->font);
  d.kv("centered", p->centered);
  d.kv("shcol", p->shadow_color);
  d.kv("lead", p->leading);
  d.kv("fixed", p->fixed);
  d.kv("sdx", p->shadow_dx);
  d.kv("sdy", p->shadow_dy);
  if (p->bk_mode == 2) d.kv("bkc", p->bk_color);
  d.r("fr", p->fixed_rect);
}

void text(Dump& d, ztwod::Text* p) {
  ftext(d, p);
  d.f("ft", p->flash_timer);
  d.f("frate", p->flash_rate);
  d.kv("atop", p->alt_top);
  d.kv("fon", p->flash_on);
  d.kv("fmode", p->flash_mode);
  d.kv("phase", p->phase);
}

void textbox(Dump& d, ztwod::TextBox* p, const char* head) {
  d.o << head << ' ';
  d.fonttext(p);
  d.kv("current", p->current);
  d.kv("bup", p->bottom_up);
  d.kv("repos", p->reposition);
  d.kv("nlines", static_cast<int64_t>(p->lines.size()));
  d.o << "\n";
  for (size_t i = 0; i < p->lines.size(); ++i) {
    d.o << head << ".line" << i << ' ';
    text(d, &p->lines[i]);
    d.o << "\n";
  }
}

void poly(Dump& d, ztwod::PolyLine* p) {
  d.base(p);
  for (auto& pt : p->pts)
    for (int32_t v : pt) d.kv("p", v);
  d.kv("count", p->count);
  d.kv("color", p->color);
  d.kv("dl", p->draw_list);
  d.kv("tf", p->transformed);
}

void panel(Dump& d, mech3::HudPanel* p);

void zoomview(Dump& d, mech3::HudZoomView* v, const std::string& head) {
  d.o << head << ' ';
  d.base(v);
  d.kv("cam", v->view.camera);
  d.kv("world", v->view.world);
  d.kv("x10", v->view.x10);
  d.kv("hw", v->view.hardware);
  d.kv("x4c", v->x4c);
  d.r("rect", v->rect);
  d.kv("x168", v->x168);
  const mech3::HudViewWindow& w = *v->view.window;
  d.o << "\n" << head << ".window ";
  d.kv("node", w.node);
  d.r("area", w.area);
  d.r("target", w.target);
  d.kv("scaled", w.scaled);
  if (w.scaled)
    for (float f : {w.sx, w.sy, w.isx, w.isy}) d.f("s", f);
  d.o << "\n" << head << ".panel ";
  panel(d, &v->panel);
  d.o << "\n";
}

void panel(Dump& d, mech3::HudPanel* p) {
  d.image(p);
  d.r("rect", p->rect);
  for (auto& c : p->corners)
    for (float v : c) d.f("c", v);
  d.kv("color", p->color);
  d.kv("alpha", p->alpha);
  d.kv("colors", static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p->colors)));
  d.kv("alphas", static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p->alphas)));
}

void dump_hud(Dump& d) {
  mech3::Hud& h = *g_hud;
  d.kv("active", h.active);
  d.o << "head=" << piece_name(h.head) << " tail=" << piece_name(h.tail) << ' ';
  d.kv("mech", 0);
  for (uint32_t v : h.x22e08) d.kv("z", v);
  d.o << "\n";
  for (const ztwod::Object2D* p : {static_cast<ztwod::Object2D*>(&h.compass), static_cast<ztwod::Object2D*>(&h.gauges),
                                   static_cast<ztwod::Object2D*>(&h.target), static_cast<ztwod::Object2D*>(&h.mfd),
                                   static_cast<ztwod::Object2D*>(&h.weapons), static_cast<ztwod::Object2D*>(&h.equipment)}) {
    d.o << "link " << piece_name(p) << " next=" << piece_name(p->next) << " owner=" << (p->owner == &h ? 1 : 0)
        << "\n";
  }
  mech3::HudCompass& c = h.compass;
  d.o << "compass ";
  d.base(&c);
  d.f("scale", c.scale);
  d.kv("half", c.half);
  d.kv("centre", c.centre);
  d.r("srcr", c.src);
  d.o << "\ntape ";
  d.image(&c.tape);
  d.o << "\nframe ";
  d.image(&c.frame);
  d.o << "\nctext ";
  d.fonttext(&c.text);
  d.o << "\ntimer ";
  d.fonttext(&h.timer);
  d.o << "\n";
  mech3::HudObjectives& ob = h.objectives;
  textbox(d, &ob, "objectives");
  d.o << "opanel ";
  panel(d, &ob.panel);
  d.o << "\n";
  textbox(d, &ob.scores, "scores");
  d.o << "ofonts ";
  for (uint32_t f : {ob.font_title, ob.font_complete, ob.font_incomplete, ob.font_scores}) d.kv("font", f);
  d.o << "\nequipment ";
  mech3::HudEquipment& eq = h.equipment;
  d.base(&eq);
  d.kv("hud", 0);
  d.kv("below", eq.below);
  d.kv("column", eq.column);
  d.o << "\nepanel ";
  panel(d, &eq.panel);
  d.o << "\n";
  for (int i = 0; i < 16; ++i) {
    mech3::HudEquipLine& l = eq.lines[i];
    d.o << "eline" << i << ' ';
    text(d, &l);
    d.o << "\nvalue ";
    d.fonttext(&l.value);
    d.o << "\nlight ";
    d.image(&l.light);
    d.kv("state", l.state);
    d.kv("item", l.item);
    d.o << "\n";
  }
  mech3::HudRadar& rd = h.radar;
  d.o << "radar ";
  d.base(&rd);
  d.kv("radius", rd.radius);
  d.kv("rsq", rd.radius_sq);
  d.kv("color", rd.color);
  d.kv("clipped", rd.clipped);
  d.r("clip", rd.clip);
  d.kv("x54", rd.x54);
  d.kv("x58", rd.x58);
  d.kv("zoomed", rd.zoomed);
  d.kv("active", rd.active);
  d.kv("changed", rd.changed);
  for (int32_t v : {rd.cx[0], rd.cx[1], rd.cy[0], rd.cy[1], rd.r[0], rd.r[1], rd.to_r, rd.to_x, rd.to_y, rd.r_steps,
                    rd.x_steps, rd.y_steps})
    d.kv("v", v);
  for (float v : rd.dir) d.f("dir", v);
  for (float v : {rd.half_fov, rd.cos_half, rd.sin_half}) d.f("fov", v);
  for (auto& [k, v] : std::initializer_list<std::pair<const char*, uint32_t>>{
           {"xbd4", rd.xbd4}, {"xbd8", rd.xbd8}, {"xdb4", rd.xdb4}, {"xdb8", rd.xdb8}, {"src", 0}})
    d.kv(k, v);
  d.o << "\n";
  for (int i = 0; i < 3; ++i) {
    ztwod::ClipLine2D& l = rd.lines[i];
    d.o << "rline" << i << ' ';
    d.base(&l);
    for (uint32_t v : {static_cast<uint32_t>(l.end_x), static_cast<uint32_t>(l.end_y), l.color, l.x40, l.x44, l.x48, l.x4c})
      d.kv("l", v);
    d.o << "\n";
  }
  d.o << "rtext1 ";
  ftext(d, &rd.text1);
  d.o << "\nrtext2 ";
  ftext(d, &rd.text2);
  d.o << "\nrlight ";
  d.image(&rd.light);
  for (int i = 0; i < 3; ++i) d.t("l", rd.light.images[i]);
  d.kv("count", rd.light.count);
  d.kv("cur", rd.light.current);
  d.kv("xc8", rd.light.xc8);
  d.kv("xcc", rd.light.xcc);
  d.o << "\n";
  for (int i = 0; i < 2; ++i) {
    d.o << "rpoly" << i << ' ';
    poly(d, &rd.polys[i]);
    d.o << "\n";
  }
  mech3::HudReticle& rt = h.reticle;
  d.o << "reticle ";
  d.image(&rt);
  d.kv("state", rt.state);
  d.kv("x950", rt.x950);
  d.f("x954", rt.x954);
  d.kv("locked", rt.locked);
  for (auto* t : {rt.img_off, rt.img_on, rt.img_over, rt.img_cant, rt.img_x920}) d.t("i", t);
  for (uint32_t v : rt.x928) d.kv("z", v);
  d.o << "\n";
  mech3::HudZoomWindow& z = rt.zoom;
  zoomview(d, &z, "zoom");
  d.o << "zoomw ";
  d.kv("round", z.round);
  d.f("zoom", z.zoom);
  d.kv("x174", z.x174);
  d.kv("bw", z.base_w);
  d.kv("bh", z.base_h);
  d.o << "\nztext ";
  ftext(d, &z.text);
  d.o << "\nzlight ";
  d.image(&z.light);
  for (int i = 0; i < 4; ++i) d.t("l", z.light.images[i]);
  d.kv("count", z.light.count);
  d.kv("cur", z.light.current);
  d.kv("xc8", z.light.xc8);
  d.kv("xcc", z.light.xcc);
  d.o << "\nzframe ";
  d.base(&z.frame);
  d.kv("radius", z.frame.radius);
  d.kv("rsq", z.frame.radius_sq);
  d.kv("color", z.frame.color);
  d.kv("clipped", z.frame.clipped);
  d.r("clip", z.frame.clip);
  d.o << "\nzbox ";
  poly(d, &z.box);
  d.o << "\n";
  mech3::HudGauges& g = h.gauges;
  d.o << "gauges ";
  d.base(&g);
  const std::pair<const char*, int32_t> ks[] = {{"x143c", g.x143c}, {"x1440", g.x1440}, {"x1444", g.x1444},
      {"hw", g.heat_w}, {"hh", g.heat_h}, {"hb", g.heat_bottom}, {"ty", g.toggler_y}};
  for (auto& [k, v] : ks) d.kv(k, v);
  d.o << "\n";
  const std::pair<const char*, ztwod::FontText*> texts[] = {
      {"coolant", &g.coolant_text}, {"fuel", &g.fuel_text}, {"speed", &g.speed_text}};
  for (auto& [n, t] : texts) {
    d.o << n << ' ';
    ftext(d, t);
    d.o << "\n";
  }
  const std::pair<const char*, mech3::HudBar*> bars[] = {{"sbar", &g.speed_bar}, {"fbar", &g.fuel_bar}, {"cbar", &g.coolant_bar}};
  for (auto& [n, b] : bars) {
    d.o << n << ' ';
    d.base(b);
    for (auto& pt : b->pts)
      for (float v : pt) d.f("p", v);
    d.kv("count", b->count);
    d.kv("height", b->height);
    d.kv("width", b->width);
    if (b != &g.speed_bar) d.kv("color", b->color);
    d.o << "\n";
  }
  const std::pair<const char*, ztwod::Image*> imgs[] = {{"sframe", &g.speed_frame}, {"cframe", &g.coolant_frame},
      {"fframe", &g.fuel_frame}, {"hbar", &g.heat_bar}, {"hmeter", &g.heat_meter}, {"toggler", &g.toggler}};
  for (auto& [n, i] : imgs) {
    d.o << n << ' ';
    d.image(i);
    d.o << "\n";
  }
  mech3::HudWeapons& w = h.weapons;
  d.o << "weapons ";
  d.base(&w);
  d.kv("hud", 0);
  d.kv("x38", w.x38);
  d.t("select", w.select);
  d.t("chain", w.chain);
  d.kv("shown", w.shown);
  d.kv("x1143c", w.x1143c);
  d.kv("column", w.column);
  d.o << "\nwpoly ";
  poly(d, &w.poly);
  d.o << "\nwpanel ";
  panel(d, &w.panel);
  d.o << "\n";
  for (int i = 0; i < 16; ++i) {
    mech3::HudWeaponLine& l = w.lines[i];
    d.o << "wline" << i << ' ';
    d.image(&l);
    d.kv("bx", l.base_x);
    d.kv("by", l.base_y);
    d.kv("en", l.enabled);
    d.f("value", l.value);
    d.r("bounds", l.bounds);
    d.t("fill", l.fill);
    d.t("empty", l.empty);
    d.kv("count", l.count);
    d.kv("red", l.red);
    d.kv("changed", l.changed);
    d.kv("x111b", l.x111b);
    d.kv("weapon", 0);
    d.o << "\nname ";
    d.fonttext(&l.name);
    d.o << "\ntext2 ";
    d.fonttext(&l.text2);
    d.o << "\nctext ";
    d.fonttext(&l.count_text);
    d.o << "\nlight ";
    d.image(&l.light);
    d.o << "\nmark ";
    d.image(&l.mark);
    d.o << "\n";
  }
  const mech3::WeaponSet& ws = mech3::weapon_set();
  d.o << "wset ";
  for (auto* t : ws.light) d.t("light", t);
  for (uint32_t f : {ws.font_red, ws.font_green, ws.font_yellow, ws.font_grey}) d.kv("font", f);
  d.o << "\n";
  const mech3::EquipmentSet& es = mech3::equipment_set();
  d.o << "eset ";
  for (auto* t : es.light) d.t("light", t);
  for (uint32_t f : {es.font_red, es.font_yellow, es.font_green, es.font_grey}) d.kv("font", f);
  d.o << "\n";
  const mech3::DamageSet& s = mech3::damage_set();
  d.kv("loaded", s.loaded);
  d.kv("bgw", s.bg_w);
  d.kv("bgh", s.bg_h);
  for (auto* t : s.bg) d.t("bg", t);
  for (const mech3::DamageImages& p : s.parts) {
    for (auto* t : p.image) d.t("i", t);
    d.kv("px", p.x);
    d.kv("py", p.y);
  }
  d.o << "\n";
}

}  // namespace

extern "C" {

// Builds the HUD as the emulator did at `hud_addr` for `mech`, dumps it.
int probe_hud_build(uint32_t hud_addr, uint32_t mech, char* out, int size) {
  RecordingHost host;
  ztwod::set_host(&host);
  reset();  // the textures by address: each run's emulator starts over
  mech3::set_hud_hooks(&g_hooks);
  mech3::damage_set() = mech3::DamageSet{};
  mech3::equipment_set() = mech3::EquipmentSet{};
  mech3::weapon_set() = mech3::WeaponSet{};
  g_hud_addr = hud_addr;
  g_hud = nullptr;  // the last one is left: its images' release would call the host
  g_hud = static_cast<mech3::Hud*>(::operator new(sizeof(mech3::Hud)));
  new (g_hud) mech3::Hud(reinterpret_cast<mech3::Mech*>(static_cast<uintptr_t>(mech)));
  Dump d;
  dump_hud(d);
  const std::string s = d.o.str();
  if (static_cast<int>(s.size()) >= size) return -1;
  std::memcpy(out, s.c_str(), s.size() + 1);
  ztwod::set_host(nullptr);
  return static_cast<int>(s.size());
}

}  // extern "C"
