// Probes for test_hud.py (docs/hud.md): the HUD (mech3/hud) built on the
// ported zTwoD with every call out recorded; dumped the way test_hud.py dumps
// the original's object from the emulator's memory.
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "mech3/hud.h"
#include "mech3/options.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;

uint32_t g_hud_addr = 0;
uint32_t g_utab = 0, g_stab = 0;  // the aim's answers in the emulator (test_hud.py aim_world)
int g_unprojects = 0, g_segments = 0;
bool g_have_zrd = false;
std::deque<std::string> g_strings;
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
  const char* message(int32_t id) override {
    const uint32_t a = call("0x00576040", {id});
    if (!a) return nullptr;
    std::string t;
    for (uint32_t i = 0; i < 256; ++i) {
      const char c = static_cast<char>(call("peek8", {a + i}));
      if (!c) break;
      t += c;
    }
    g_strings.push_back(t);
    return g_strings.back().c_str();
  }
  ztwod::Rect display_rect() override { return rect_at(0x5f4d20); }
  ztwod::Rect window_rect() override { return rect_at(0x5f4d24); }
  int32_t display_w() override { return static_cast<int32_t>(call("peek32", {rect_ptr(0x5f4d20) + 0x10})); }
  int32_t display_h() override { return static_cast<int32_t>(call("peek32", {rect_ptr(0x5f4d20) + 0x14})); }
  static uint32_t rect_ptr(uint32_t global) { return call("peek32", {call("peek32", {global})}); }
  float player_camera_fov() override {
    const uint32_t node = call("peek32", {call("peek32", {0x5f4c8c}) + 4});
    return zgame::from_bits(call("peek32", {call("peek32", {node + 0x38}) + 0xe8}));
  }
  void* mech_target_slot(mech3::Mech* m) override {
    return m ? reinterpret_cast<void*>(static_cast<uintptr_t>(id(m) + 0x6c)) : nullptr;
  }
  void unproject_world(const float sc[3], float out[3]) override {
    call("0x00523930", {zgame::bits(sc[0]), zgame::bits(sc[1]), zgame::bits(sc[2])});
    for (int i = 0; i < 3; ++i) out[i] = zgame::from_bits(call("peek32", {g_utab + 12u * g_unprojects + 4u * i}));
    ++g_unprojects;
  }
  int32_t segment_list(uint32_t world, const float a[3], const float b[3], zwep::HitList* out) override {
    const uint32_t code = call("0x004eed90", {world, zgame::bits(a[0]), zgame::bits(a[1]), zgame::bits(a[2]),
                                              zgame::bits(b[0]), zgame::bits(b[1]), zgame::bits(b[2])});
    const uint32_t base = g_stab + 0x508u * g_segments++ + 4;
    *out = zwep::HitList{};
    out->count = static_cast<uint8_t>(call("peek8", {base}));
    for (int j = 0; j < out->count && j < 31; ++j) {
      const uint32_t r = base + 4 + 0x28u * j;
      float f[6];
      for (int q = 0; q < 6; ++q) f[q] = zgame::from_bits(call("peek32", {r + 4u * q}));
      zwep::HitRecord& h = out->rec[j];
      h.normal = {f[0], f[1], f[2]};
      h.point = {f[3], f[4], f[5]};
      h.x18 = call("peek32", {r + 0x18});
      h.distance_sq = zgame::from_bits(call("peek32", {r + 0x1c}));
      h.material = reinterpret_cast<zwep::Handle>(static_cast<uintptr_t>(call("peek32", {r + 0x20})));
      h.node = reinterpret_cast<zwep::Handle>(static_cast<uintptr_t>(call("peek32", {r + 0x24})));
    }
    return static_cast<int32_t>(code);
  }
  uint32_t material_soil(zwep::Handle m) override {
    if (!m) return 0;
    const uint32_t soil = call("peek32", {id(m) + 0x20});
    return call("peek32", {0x608c00 + 4 * soil});
  }
  zrdr::Node* load_hud_zrd() override { return g_have_zrd ? zrdr::load("hud.zrd") : nullptr; }
  // The HUD and its pieces by the original's offsets; anything else (the
  // emulator's own handles) as it is.
  uint32_t id(const void* p) override {
    if (!p) return 0;
    if (g_hud) {
      const mech3::Hud& h = *g_hud;
      const std::pair<const void*, uint32_t> at[] = {
          {&h, 0}, {&h.radar, 0x14}, {&h.compass, 0xdd4}, {&h.reticle, 0x144c}, {&h.reticle.zoom, 0x1508},
          {&h.gauges, 0x1da8}, {&h.mfd, 0x3434}, {&h.target, 0x410c}, {&h.weapons, 0x6520},
          {&h.objectives, 0x17964}, {&h.timer, 0x18410}, {&h.equipment, 0x188c0}};
      for (auto& [q, off] : at)
        if (q == p) return g_hud_addr + off;
    }
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
  }
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
  d.kv("dl", static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p->clip)));
  d.kv("tf", p->round);
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
  d.kv("x168", g_hooks.id(v->source));
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

uint32_t fnv(const uint8_t* b, size_t n) {
  uint32_t v = 2166136261u;
  for (size_t i = 0; i < n; ++i) v = (v ^ b[i]) * 16777619u;
  return v;
}

void panel(Dump& d, mech3::HudPanel* p) {
  d.image(p);
  if (const zimg::Texture* t = p->image; t && !g_tex_id.count(t)) {
    const int32_t n = t->pixel_count;
    d.kv("tn", n);
    d.kv("twh", static_cast<uint16_t>(t->width) | static_cast<uint32_t>(static_cast<uint16_t>(t->height)) << 16);
    d.kv("tf", t->flags);
    d.kv("tw2", t->width_again);
    d.kv("tpx", t->pixels == p->colors);
    d.kv("tal", t->alpha == p->alphas);
    d.kv("hc", fnv(reinterpret_cast<const uint8_t*>(p->colors), static_cast<size_t>(n) * 2));
    if (p->alpha < 0xfe) d.kv("ha", fnv(p->alphas, static_cast<size_t>(n)));  // else never filled
  }
  d.r("rect", p->rect);
  for (auto& c : p->corners)
    for (float v : c) d.f("c", v);
  d.kv("color", p->color);
  d.kv("alpha", p->alpha);
  d.kv("colors", p->colors != nullptr);
  d.kv("alphas", p->alphas != nullptr);
}

void damageview(Dump& d, mech3::HudDamageView* v, const std::string& head) {
  d.o << head << ' ';
  d.base(v);
  d.r("rect", v->rect);
  d.o << "\n" << head << ".bg ";
  d.image(&v->bg);
  d.o << "\n";
  for (int i = 0; i < 8; ++i) {
    mech3::HudDamagePart& p = v->parts[i];
    d.o << head << ".part" << i << ' ';
    d.image(&p);
    d.f("f", p.xbc);
    for (uint32_t x : {p.xc0, p.xc4, p.xc8, p.xcc, p.xd0}) d.kv("f", x);
    d.kv("part", p.images - mech3::damage_set().parts);
    d.o << "\n";
  }
}

void mfd(Dump& d, mech3::HudMfd* m, const std::string& head) {
  d.o << head << ' ';
  d.base(m);
  d.kv("x34", g_hooks.id(m->hud));
  d.kv("mode", m->mode);
  d.kv("nav", m->nav);
  d.kv("dmech", g_hooks.id(m->damage.mech));
  d.o << "\n";
  damageview(d, &m->damage, head + ".damage");
  zoomview(d, m->view, head + ".view");
  d.o << head << ".box ";
  poly(d, &m->view->box);
  d.o << "\n" << head << ".title ";
  ftext(d, &m->title);
  d.o << "\n";
}

void dump_hud(Dump& d) {
  mech3::Hud& h = *g_hud;
  d.kv("active", h.active);
  d.o << "head=" << piece_name(h.head) << " tail=" << piece_name(h.tail) << ' ';
  d.kv("mech", g_hooks.id(h.mech));
  d.kv("reader", static_cast<uint32_t>(reinterpret_cast<uintptr_t>(h.reader)));
  d.kv("block", h.block ? 1 : 0);
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
  d.kv("hud", g_hooks.id(eq.hud));
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
  d.kv("x54", zgame::bits(rd.noise));
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
           {"xbd4", rd.xbd4}, {"xbd8", rd.xbd8}, {"xdb4", rd.xdb4}, {"xdb8", rd.xdb8}, {"src", g_hooks.id(rd.source)}})
    d.kv(k, v);
  d.o << "\n";
  for (int i = 0; i < 3; ++i) {
    ztwod::ClipLine2D& l = rd.lines[i];
    d.o << "rline" << i << ' ';
    d.base(&l);
    for (uint32_t v : {static_cast<uint32_t>(l.end_x), static_cast<uint32_t>(l.end_y), l.color, static_cast<uint32_t>(l.clipped),
                      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(l.clip)), static_cast<uint32_t>(l.dash),
                      zgame::bits(l.noise)})
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
  mfd(d, &h.mfd, "mfd");
  mech3::HudTarget& tg = h.target;
  mfd(d, &tg, "target");
  auto shape = [&](ztwod::Shape& q) {
    poly(d, &q);
    d.kv("xf", q.xform);
    for (float f : q.translation) d.f("s", f);
    for (uint32_t v : q.xfc) d.kv("s", v);
    d.kv("s", q.rotation);
  };
  d.o << "tshape1 ";
  shape(tg.shape1);
  d.o << "\n";
  for (int i = 0; i < 3; ++i) {
    ztwod::Line2D& l = tg.lines[i];
    d.o << "tline" << i << ' ';
    d.base(&l);
    for (uint32_t v : {static_cast<uint32_t>(l.end_x), static_cast<uint32_t>(l.end_y), l.color, static_cast<uint32_t>(l.clipped),
                      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(l.clip)), static_cast<uint32_t>(l.dash)})
      d.kv("l", v);
    d.o << "\n";
  }
  d.o << "tshape2 ";
  shape(tg.shape2);
  d.o << "\ntframe ";
  d.base(&tg.frame);
  d.kv("radius", tg.frame.radius);
  d.kv("rsq", tg.frame.radius_sq);
  d.kv("color", tg.frame.color);
  d.kv("clipped", tg.frame.clipped);
  d.r("clip", tg.frame.clip);
  d.o << "\ntfields ";
  for (auto* t : tg.arrows) d.t("a", t);
  d.kv("target", g_hooks.id(tg.target));
  d.kv("shown", tg.shown);
  d.f("x240c", tg.x240c);
  d.kv("lh", tg.line_h);
  d.kv("dmech", g_hooks.id(tg.damage.mech));
  d.o << "\ntmarker ";
  d.image(&tg.marker);
  d.o << "\n";
  for (int i = 0; i < 4; ++i) {
    d.o << "ttext" << i << ' ';
    ftext(d, &tg.texts[i]);
    d.o << "\n";
  }
  mech3::HudReticle& rt = h.reticle;
  d.o << "reticle ";
  d.image(&rt);
  d.kv("state", rt.state);
  d.kv("x950", rt.x950);
  d.f("x954", rt.x954);
  d.kv("locked", rt.locked);
  d.kv("src", g_hooks.id(rt.source));
  for (auto* t : {rt.img_off, rt.img_on, rt.img_over, rt.img_cant, rt.img_x920}) d.t("i", t);
  {
    const zwep::HitRecord& t = rt.hit;
    for (float f : {t.normal.x, t.normal.y, t.normal.z, t.point.x, t.point.y, t.point.z}) d.kv("z", zgame::bits(f));
    d.kv("z", t.x18);
    d.kv("z", zgame::bits(t.distance_sq));
    d.kv("z", static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t.material)));
    d.kv("z", static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t.node)));
  }
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
      {"coolant", &g.coolant_text}, {"fuel", &g.heat_text}, {"speed", &g.speed_text}};
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
  d.kv("hud", g_hooks.id(w.hud));
  d.kv("x38", w.weapons);
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
// The options' rects as the emulator has them (render, display: **[0x005f4d1c],
// **[0x005f4d20]) and the pixel doubling ([0x005f4d4c]), for 0x0040d9e0.
void sync_options() {
  mech3::Options& o = mech3::options();
  const std::pair<uint32_t, mech3::Rect*> rects[] = {{0x5f4d1c, &o.render}, {0x5f4d20, &o.display}};
  for (auto& [g, r] : rects) {
    const uint32_t a = call("peek32", {call("peek32", {g})});
    int32_t v[8];
    for (int i = 0; i < 8; ++i) v[i] = static_cast<int32_t>(call("peek32", {a + 4u * i}));
    r->x = v[0], r->y = v[1], r->x_end = v[2], r->y_end = v[3], r->w = v[4], r->h = v[5], r->x_last = v[6],
    r->y_last = v[7];
  }
  o.pixel_double = static_cast<int32_t>(call("peek32", {0x5f4d4c}));
}

std::string aim_dump(mech3::Hud& h) {
  const mech3::HudReticle& r = h.reticle;
  const zwep::HitRecord& t = r.hit;
  std::vector<uint32_t> v = {static_cast<uint32_t>(r.x), static_cast<uint32_t>(r.y)};
  for (float f : r.aim_point) v.push_back(zgame::bits(f));
  v.push_back(zgame::bits(r.aim_distance));
  for (float f : {t.normal.x, t.normal.y, t.normal.z, t.point.x, t.point.y, t.point.z}) v.push_back(zgame::bits(f));
  v.push_back(t.x18);
  v.push_back(zgame::bits(t.distance_sq));
  v.push_back(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t.material)));
  v.push_back(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t.node)));
  const mech3::HudZoomWindow& z = r.zoom;
  for (int32_t q : {z.rect.left, z.rect.top, z.rect.right, z.rect.bottom, z.x, z.y}) v.push_back(static_cast<uint32_t>(q));
  std::string s = "aim ";
  char b[16];
  for (uint32_t x : v) {
    std::snprintf(b, sizeof b, "%x ", x);
    s += b;
  }
  return s;
}

int probe_hud_build(uint32_t hud_addr, uint32_t mech, const char* zbd, const float* places, int nplaces,
                    uint32_t utab, uint32_t stab, char* out, int size) {
  RecordingHost host;
  ztwod::set_host(&host);
  reset();  // the textures by address: each run's emulator starts over
  mech3::set_hud_hooks(&g_hooks);
  mech3::damage_set() = mech3::DamageSet{};
  mech3::equipment_set() = mech3::EquipmentSet{};
  mech3::weapon_set() = mech3::WeaponSet{};
  g_hud_addr = hud_addr;
  g_hud = nullptr;  // the last one is left: its images' release would call the host
  g_have_zrd = zbd && *zbd;
  g_utab = utab, g_stab = stab, g_unprojects = 0, g_segments = 0;
  sync_options();
  zrdr::init();
  zrdr::close_archives(true);
  if (g_have_zrd) zrdr::open_archive(zbd, true);
  // Zeroed as the emulator's fresh block: +0x22e3c's exe path (hud.md section 3).
  void* storage = ::operator new(sizeof(mech3::Hud));
  std::memset(storage, 0, sizeof(mech3::Hud));
  g_hud = static_cast<mech3::Hud*>(storage);
  new (g_hud) mech3::Hud(reinterpret_cast<mech3::Mech*>(static_cast<uintptr_t>(mech)));
  std::string aims;
  for (int i = 0; i < nplaces; ++i) {
    g_hud->reticle.place(places[2 * i], places[2 * i + 1]);
    aims += aim_dump(*g_hud) + "\n";
  }
  Dump d;
  dump_hud(d);
  const std::string s = d.o.str() + aims;
  if (static_cast<int>(s.size()) >= size) return -1;
  std::memcpy(out, s.c_str(), s.size() + 1);
  zrdr::close_archives(true);
  ztwod::set_host(nullptr);
  return static_cast<int>(s.size());
}

}  // extern "C"
