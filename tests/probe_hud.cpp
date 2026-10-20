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

// test_hud.py's order: FontText's fields, then Text's and the rest.
void text(Dump& d, ztwod::Text* p) {
  d.fonttext(p);
  d.kv("font", p->font);
  d.kv("centered", p->centered);
  d.kv("shcol", p->shadow_color);
  d.kv("lead", p->leading);
  d.kv("fixed", p->fixed);
  d.kv("sdx", p->shadow_dx);
  d.kv("sdy", p->shadow_dy);
  d.f("ft", p->flash_timer);
  d.f("frate", p->flash_rate);
  d.kv("atop", p->alt_top);
  if (p->bk_mode == 2) d.kv("bkc", p->bk_color);
  d.r("fr", p->fixed_rect);
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
  d.base(&w.poly);
  for (auto& pt : w.poly.pts)
    for (int32_t v : pt) d.kv("p", v);
  d.kv("count", w.poly.count);
  d.kv("color", w.poly.color);
  d.kv("dl", w.poly.draw_list);
  d.kv("tf", w.poly.transformed);
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
