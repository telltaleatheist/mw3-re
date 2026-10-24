// A field base lost in a campaign (docs/vehicles.md, A mobile field base
// lost). Original file: mech3\mech_map.cpp's region with the field base
// (0x004590b0); here apart for pilot.h's Handle.
#include "mech3/mech_fieldbase_cut.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "crt/msvc_rand.h"
#include "mech3/mech_config.h"
#include "mech3/mech_names.h"
#include "mech3/pilot.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

int32_t lab_rounds(int32_t weapon, int32_t n);  // weapon.h (whose Handle is mech.h's)

namespace {
FieldCutHooks g_default;
FieldCutHooks* g_hooks = &g_default;

constexpr float kTonsPerBase = 300.0f;    // 0x00470cc0
constexpr int32_t kMechsPerBase = 2;      // 0x00470cd0
constexpr float kRandScale = 3.051851e-05f;  // 0x005c23b0, 1 / 32767

// 0x0045b900 and the loops after each cut: the store's tons, a float sum.
float store_tons(const Store& s) {
  float t = 0.0f;
  for (const ConfigItem* it : s.items) t = static_cast<float>(it->weight(-1) + t);
  return t;
}
}  // namespace

void set_field_cut_hooks(FieldCutHooks* h) { g_hooks = h ? h : &g_default; }
FieldCutHooks& field_cut_hooks() { return *g_hooks; }

int32_t FieldCutHooks::rand() { return crt::msvc_rand(); }
const char* FieldCutHooks::mech_name(int32_t mech) { return mech3::mech_name(mech); }
int32_t FieldCutHooks::lab_rounds(int32_t weapon, int32_t n) { return mech3::lab_rounds(weapon, n); }

// Mech3.exe 1.2 @ 0x004590b0, after the basic vehicle's death. With bases
// left, the player keeps ceil(f n) of the n mechs (at most two a base) and
// f of the tons (at most 300 a base), f 0.7 with two left, 0.5 with one;
// what goes is moved to +0x78, which only a replay gives back.
void field_base_cut(CampaignGame& g) {
  FieldCutHooks& h = *g_hooks;
  Store& store = g.store;
  const int32_t left = --g.a8;
  float f = 1.0f;
  if (left == 2) f = 0.7f;
  else if (left == 1) f = 0.5f;
  if (left <= 0) return;
  // ([0x00605c3c] = 3 x 0x00470cd0 once: written, never read.)

  // The mechs, one entry per one held, up to 64 (checked after each item).
  // FIX (bug): a vector; the exe's 64-entry stack array overran when the
  // item that crossed 64 held more, and a draw of 64 read past it.
  std::vector<ConfigItem*> mechs;
  for (auto it = store.items.begin(); it != store.items.end() && mechs.size() < 64; ++it) {
    ConfigItem* item = *it;
    if (!item || item->kind() != 0) continue;
    for (int32_t j = 0; j < item->count; ++j) mechs.push_back(item->kind() == 0 ? item : nullptr);
  }
  const int32_t n = static_cast<int32_t>(mechs.size());
  h.message(3.0f, 0xf07, nullptr);  // "MCB Destroyed, Updating Inventory..."
  if (n >= left) {
    const float nf = static_cast<float>(n);
    int32_t keep = static_cast<int32_t>(std::ceil(f * nf));
    if (keep > kMechsPerBase * left) keep = kMechsPerBase * left;
    while (keep != 0) {  // the kept ones taken off the list at random
      const auto i = static_cast<size_t>(std::floor(static_cast<float>(h.rand()) * kRandScale * nf));
      if (i < mechs.size() && mechs[i]) mechs[i] = nullptr, --keep;
    }
    for (ConfigItem* m : mechs) {
      std::string text;
      if (m) {
        const int32_t mech = m->id();
        text += h.mech_name(mech);
        text += "  ";  // 0x005e4aa0
        g.store78.add_mech(mech);
        store.remove_mech(mech);
      }
      if (!text.empty()) h.message(3.0f, 0xf05, text.c_str());  // "Chassis Lost: %1"
    }
  }

  // The rest by weight: random items, one at a time (ammo by the lab's
  // rounds), until the tons are down to the limit or ten draws in a row
  // (six after one that cut) change nothing.
  const float total = store_tons(store);
  float tons = total;
  float limit = total * f;
  if (!(limit < static_cast<float>(left) * kTonsPerBase)) limit = static_cast<float>(left) * kTonsPerBase;
  if (limit > 0.0f) {
    int32_t tries = 10;
    while (tons > limit) {
      const int32_t count = store.count();
      const auto r = static_cast<int32_t>(
          std::floor(static_cast<float>(h.rand()) * kRandScale * static_cast<float>(count)));
      auto it = store.items.begin();
      for (int32_t k = r; k != 0 && it != store.items.end(); --k) ++it;
      ConfigItem* item = it == store.items.end() ? store.items.back() : *it;  // past the end: the last
      if (item->kind() != 0) {
        int32_t cut = 1;
        if (item->kind() == 2) cut = h.lab_rounds(item->id(), item->count);
        g.store78.add_copy(item, cut);  // vt+0x00
        store.remove(item, cut);        // vt+0x14: an unlimited item stays (and is copied)
        const float now = store_tons(store);
        const float d = tons - now;
        if (d == 0.0f) --tries;
        else tries = 6;
        tons = tons - d;
      }
      if (tries <= 0) break;
    }
    // The exe then meant to move the last item whole while still over, but
    // its search ends past the list every time, so nothing more goes
    // (0x0045951d..0x00459563; fixes.md candidate).
  }
  char text[32];
  std::snprintf(text, sizeof text, "%.2f", static_cast<double>(total - tons));  // 0x005e3ebc
  h.message(3.0f, 0xf06, text);  // "Lost %1 tons of equipment"
}

}  // namespace mech3
