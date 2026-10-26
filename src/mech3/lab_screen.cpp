// The mech lab's screen (docs/mechlab.md section 2), step 2: the screen,
// its own controls, the page and slot switches. Original file unknown
// (0x00448830..0x004561ff, between mech_data.cpp and mech_map.cpp).
#include "mech3/lab_screen.h"

#include <algorithm>
#include <cstdarg>
#include <iterator>
#include <string>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zvideo/zvid_format.h"
#include "mech3/app_states.h"
#include "mech3/pilot.h"

namespace mech3 {

// mech_names.h (its Engine clashes with the app's): 0x00484670's message and 0x004846e0.
int32_t mech_name_message(int32_t mech);
float max_mass(int32_t mech);
int32_t internal_points(int32_t mech, int32_t column);    // 0x00485400..0x00485490
int32_t armor_points(int32_t tech, bool ferro, float tons);  // 0x00424760
constexpr int32_t kClan = 1, kAnyTech = 3;               // mech_names.h's Tech
int32_t lab_rounds(int32_t weapon, int32_t n);           // weapon.h (0x004b4900)
int32_t mech_tech(int32_t mech);                         // 0x00484720
int32_t equip_name_message(int32_t kind);                // 0x00484980's table (its text through message)

namespace {
int32_t g_none = -1;  // the tabs' and mech buttons' id until bound

// Mech3.exe 1.2 @ 0x00484670: a mech's name from Mech3Msg.dll, "notmech"
// out of range; CString::Format's "%s" gives "(null)" for none.
std::string display_name(Engine& e, int32_t mech) {
  if (mech < 0 || mech >= 19) return "notmech";
  const char* s = e.message(mech_name_message(mech));
  return s ? s : "(null)";
}

// MSVC 6's std::sort (VC98 <algorithm>), as the exe has it inlined: the
// comparator's calls are in the trace, so its order of comparisons matters.
template <typename It, typename Less>
void vc6_insert(It last, typename std::iterator_traits<It>::value_type v, Less less) {
  for (It m = last; less(v, *--m); last = m) *last = *m;
  *last = v;
}
template <typename It, typename Less>
void vc6_insertion_sort(It f, It l, Less less) {
  if (f == l) return;
  for (It m = f; ++m != l;) {
    auto v = *m;
    if (!less(v, *f)) {
      vc6_insert(m, v, less);
    } else {
      std::copy_backward(f, m, m + 1);
      *f = v;
    }
  }
}
template <typename T, typename Less>
T vc6_median(T a, T b, T c, Less less) {
  if (less(a, b)) return less(b, c) ? b : less(a, c) ? c : a;
  return less(a, c) ? a : less(b, c) ? c : b;
}
template <typename It, typename T, typename Less>
It vc6_partition(It f, It l, T piv, Less less) {
  for (;; ++f) {
    for (; less(*f, piv); ++f) {}
    for (; less(piv, *--l);) {}
    if (!(f < l)) return f;
    std::iter_swap(f, l);
  }
}
template <typename It, typename Less>
void vc6_sort_loop(It f, It l, Less less) {
  while (l - f > 16) {
    It m = vc6_partition(f, l, vc6_median(*f, *(f + (l - f) / 2), *(l - 1), less), less);
    if (l - m <= m - f) {
      vc6_sort_loop(m, l, less);
      l = m;
    } else {
      vc6_sort_loop(f, m, less);
      f = m;
    }
  }
}
template <typename It, typename Less>
void vc6_sort(It f, It l, Less less) {
  if (l - f <= 16) {
    vc6_insertion_sort(f, l, less);
    return;
  }
  vc6_sort_loop(f, l, less);
  vc6_insertion_sort(f, f + 16, less);
  for (f += 16; f != l; ++f) vc6_insert(f, *f, less);
}
}  // namespace

void LabLater::call(uint32_t slot, std::initializer_list<Arg> args) const {
  char key[24];
  std::snprintf(key, sizeof key, "lab.vt+0x%x", slot);
  const Arg* a = args.begin();
  switch (args.size()) {
    case 0: e->unported(key, {at}); break;
    case 1: e->unported(key, {at, a[0]}); break;
    default: e->unported(key, {at, a[0], a[1]}); break;
  }
}

uint32_t LabLater::ask(const char* key) const { return e->unported(key, {at}); }

void LabLater::call_fn(const char* key, std::initializer_list<Arg> args) const {
  const Arg* a = args.begin();
  if (args.size() == 0) e->unported(key, {at});
  else e->unported(key, {at, a[0]});
}

// ---------------------------------------------------------------------------
// The panels

// Mech3.exe 1.2 @ 0x0044b070
LabPanel::~LabPanel() {
  set_image(static_cast<zimg::Texture*>(nullptr));
  if (single) ztwod::host().image_release(single);
  if (twice) ztwod::host().image_release(twice);
  single = twice = nullptr;
}

// Mech3.exe 1.2 @ 0x0044b100
bool LabPanel::read(zrdr::Node* node, ztwod::Dialog* dialog) {
  const bool ok = ztwod::Image::read(node, dialog);
  if (!ok) return ok;
  if (const char* s = zrdr::get_string(node, "double")) twice = ztwod::host().image_get(s);
  if (const char* s = zrdr::get_string(node, "single")) single = ztwod::host().image_get(s);
  if (const char* s = zrdr::get_string(node, "double2")) twice2 = ztwod::host().image_get(s);
  return ok;
}

// Mech3.exe 1.2 @ 0x0044b180
void LabPanel::show_look(int32_t look) {
  if (look == 1) set_image(single);
  else if (look == 2) set_image(twice);
  else if (look == 3) set_image(twice2);
}

// ---------------------------------------------------------------------------
// The edit bar

// Mech3.exe 1.2 @ 0x0044a710
void LabEditBar::set_pos(int32_t px, int32_t py) {
  ztwod::Image::set_pos(label_x + px, label_y + py);
  if (!labels.empty()) labels[0]->set_pos(px, py);
}

// ---------------------------------------------------------------------------
// The pages' controls: their own steps

bool LabLoadoutList::read_control(zrdr::Node*, ztwod::Dialog*) {
  call(0x84);
  return true;
}
void LabLoadoutList::after_bind() { call(0x88); }
void LabLoadoutList::show_for(float seconds) { call(0x60, {zgame::bits(seconds)}); }
void LabLoadoutList::hide() { call(0x64); }

// Mech3.exe 1.2 @ 0x00449180
LabChassisList::~LabChassisList() {
  for (FileRow* r : rows) {
    if (r->attached) {
      delete r->attached;
      r->attached = nullptr;
    }
  }
}
// Mech3.exe 1.2 @ 0x00449220
void LabChassisList::set_store(Handle s) {
  selected = 0;
  x218 = 0;
  store = s;
  refill();
}

// Mech3.exe 1.2 @ 0x00449160
void LabChassisList::set_sort(int32_t by) {
  sort = by;
  refill();
}

int32_t LabChassisList::mech_at(int32_t index) {
  if (index < 0 || index >= item_count()) return -1;
  const ConfigItem* it = e_.item_of(static_cast<Handle>(items[static_cast<size_t>(index)]->value));
  return it && it->kind() == 0 ? it->id() : -1;
}

// Mech3.exe 1.2 @ 0x00449a80
int32_t LabChassisList::find_mech(int32_t mech) {
  for (int32_t i = 0; i < item_count(); ++i) {
    const ConfigItem* it = e_.item_of(static_cast<Handle>(items[static_cast<size_t>(i)]->value));
    if (it && it->kind() == 0 && it->id() == mech) return i;
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00449b00
bool LabChassisList::select_mech(int32_t mech) {
  const int32_t i = find_mech(mech);
  FileList::select(i);  // 0x0048d0d0, not the lab's select
  return i != 0;
}

// Mech3.exe 1.2 @ 0x00449250: the rows from the store (0x00449570 on each
// item: a mech by its name), sorted (std::sort; 0x004496c0 compares), and
// the shown slot's mech chosen by its row before the sort.
void LabChassisList::refill() {
  Engine& e = e_;
  clear_items();
  layout();
  const LabState& lab = states().lab;
  const Handle slot = e.lab_slot(lab.pilot_, lab.slot_);
  if (!slot) return;
  const int32_t mech = e.lab_slot_mech(slot);
  if (e.pilot() && e.pilot_campaign(e.pilot()) && lab.slot_ != 0) add_item(e.message(0x23d), 0);
  int32_t found = 0;
  if (Store* s = store ? e.store_of(store) : nullptr) {
    for (const ConfigItem* it : s->items) {
      if (it->kind() != 0) continue;
      const std::string name = display_name(e, it->id());
      const int32_t n = item_count();
      items.push_back(new Item{strdup(name.c_str()), static_cast<int32_t>(e.item_handle(it))});
      layout();
      if (it->id() == mech) found = n;
    }
  }
  // 0x004496c0: by the mech's number (0, the names are looked up and not
  // used) or its tonnage then number (1); anything else keeps no order.
  // FIX (bug), zfix bugs: SORT_CHASSISNAME by the names it looks up, then the number.
  auto mech_of = [&](const Item* x) {
    const ConfigItem* it = e.item_of(static_cast<Handle>(x->value));
    return x->value && it ? it->id() : -1;
  };
  const int32_t by = sort;
  vc6_sort(items.begin(), items.end(),
           [&](const Item* a, const Item* b) { return chassis_before(e, by, mech_of(a), mech_of(b)); });
  FileList::select(found);  // 0x0048d0d0
}

// Mech3.exe 1.2 @ 0x004496c0
bool chassis_before(Engine& e, int32_t by, int32_t ma, int32_t mb) {
  const std::string na = display_name(e, ma), nb = display_name(e, mb);
  if (by == 0) {
    if (zfix::options().bugs && na != nb) return na < nb;
    return ma < mb;
  }
  if (by != 1) return false;
  if (max_mass(mb) != max_mass(ma)) return max_mass(ma) < max_mass(mb);
  return ma < mb;
}

// Mech3.exe 1.2 @ 0x0044f4c0 (0x0048ccf0, then 0x00449760): each mech's
// tonnage beside its row, the chosen row's in red.
void LabChassisList::layout() {
  StoreList::layout();
  auto tons = [&](FileRow* r, Handle font, bool chosen) {
    const int32_t k = r->index;  // 0x0048d960: the row's item, none out of range
    const ConfigItem* it =
        k >= 0 && k < item_count() ? e_.item_of(static_cast<Handle>(items[static_cast<size_t>(k)]->value)) : nullptr;
    if (!it || it->kind() != 0) {
      if (r->attached) r->attached->hide();
      return;
    }
    auto* t = static_cast<ztwod::Text*>(r->attached);
    if (!t) r->attached = t = new ztwod::Text;
    if (chosen) {
      t->show_for(-1.0f);
      t->set_font(font);
    } else {
      t->set_font(font);
      t->show_for(-1.0f);
    }
    t->set_pos(r->get_x() + 175, r->get_y());
    char b[32];
    std::snprintf(b, sizeof b, "%.1f", static_cast<double>(max_mass(it->id())));
    // The chosen row's number is its own format: harmless, "%.1f" never prints a '%'.
    if (chosen) t->set_text(b);
    else t->set_text("%s", b);
  };
  const Handle green = ztwod::host().font_find("lab_large_greenR");
  for (FileRow* r : rows)
    if (!(r->flags & ztwod::kHidden)) tons(r, green, false);
  if (!selected_row || selected < top) return;
  const Handle red = ztwod::host().font_find("lab_large_redR");
  const uint32_t i = static_cast<uint32_t>(selected - top);
  if (i >= rows.size()) return;
  FileRow* r = rows[i];
  tons(r, red, true);
  selected_row->attached = r->attached;
}

// Mech3.exe 1.2 @ 0x00449b30: a row pressed. A changed loadout asks first
// (0x004ac150 0x137 / 0x1ed with the mech and the loadout).
void LabChassisList::select(int32_t index) {
  Engine& e = e_;
  LabScreen* s = e.lab_screen(states().lab.lab_screen_);
  if (s && s->config_edit.x38c != 0 && !s->config_edit.finish(false)) return;
  const int32_t mech = mech_at(index);
  const LabState& lab = states().lab;
  if (const Handle slot = e.lab_slot(lab.pilot_, lab.slot_)) {
    const Handle cfg = e.lab_slot_cfg(slot);
    if (cfg && mech == e.config_mech(cfg)) return;
    if (cfg && e.lab_config_modified(cfg)) {
      if (s) s->cursor_obj.erase();
      const int32_t m = e.config_mech(cfg);
      const std::string name = e.lab_config_name(cfg);
      const std::string mn = display_name(e, m);
      if (e.message_box2(1, 0x137, 0x1ed, mn.c_str(), name.c_str()) != 1) return;
    }
  }
  play_sound(1, 1.0f);
  FileList::select(index);
  states().lab.set_mech(mech);
}

// Mech3.exe 1.2 @ 0x0044b1c0: the 3D model's skin follows when one is up.
void LabSkinList::select(int32_t i) {
  CycleButton::select(i);
  Engine& e = e_;
  const LabState& lab = states().lab;
  if (lab.model_) e.lab_model_skin(lab.model_, i);
  if (e.lab_slot(lab.pilot_, 0)) e.lab_slot_set_skin(e.lab_slot(lab.pilot_, 0), i);
}

// Mech3.exe 1.2 @ 0x0044f390
bool LabConfigList::read_control(zrdr::Node* node, ztwod::Dialog* dialog) {
  const bool r = FileList::read_control(node, dialog);
  dialog->bind_button(&add, "CONFIG_ADD");
  dialog->bind_button(&del, "CONFIG_DELETE");
  dialog->bind_button(&save, "CONFIG_SAVE");
  dialog->bind_button(&rename, "CONFIG_RENAME");
  return r;
}

// Mech3.exe 1.2 @ 0x0044f400
void LabConfigList::show_for(float seconds) {
  FileList::show_for(seconds);
  add.show_for(seconds);
  del.show_for(seconds);
  rename.show_for(seconds);
  save.show_for(seconds);
}

// Mech3.exe 1.2 @ 0x0044f460
void LabConfigList::hide() {
  FileList::hide();
  add.hide();
  del.hide();
  rename.hide();
  save.hide();
}

Handle LabConfigList::value_at(int32_t i) const {
  return i >= 0 && i < item_count() ? static_cast<Handle>(items[static_cast<size_t>(i)]->value) : 0;
}

// Mech3.exe 1.2 @ 0x00449fb0
void LabConfigList::fill() {
  if (flags & ztwod::kHidden) return;
  Engine& e = e_;
  clear_items();
  layout();
  const LabState& lab = states().lab;
  const Handle slot = e.lab_slot(lab.pilot_, lab.slot_);
  if (!slot) return;
  const int32_t mech = e.lab_slot_mech(slot);
  const Handle dfn = e.mech_dfn_handle(mech);
  if (!dfn) return;
  const Handle list = e.lab_dfn_configs(dfn);
  const Handle store = lab.store_;
  LabScreen* s = e.lab_screen(lab.lab_screen_);
  if (!e.lab_slot_cfg(slot) && list && s) s->default_config(slot, store);
  const Handle cur = e.lab_slot_cfg(slot);
  for (Handle c : e.lab_list_configs(list)) {
    if (!c || e.lab_config_byte(c, 0x1e) || (lab.flag_ && e.lab_config_byte(c, 0x1c))) continue;
    std::string text = e.lab_config_name(c);
    if (!e.lab_store_full(store) && !e.lab_store_has_config(store, mech, cur, c)) {
      const char* m = e.message(0x23e);
      text += m ? m : "";
    }
    items.push_back(new Item{strdup(text.c_str()), static_cast<int32_t>(c)});
    layout();
  }
  select_config(cur);
  enable_buttons();
}

// Mech3.exe 1.2 @ 0x00449d40: every row of that name is chosen in turn (the last stays).
void LabConfigList::select_config(Handle config) {
  Engine& e = e_;
  if (!config || e.lab_config_modified(config)) {
    FileList::select(-1);
    return;
  }
  const std::string name = e.lab_config_name(config);
  for (int32_t i = 0; i < item_count(); ++i) {
    const Handle v = value_at(i);
    if (v && e.lab_config_name(v) == name) FileList::select(i);
  }
}

// Mech3.exe 1.2 @ 0x0044a1f0: CONFIG_ADD is never enabled; RENAME and
// DELETE for the player's own; SAVE with a loadout, not in multiplayer's lab.
void LabConfigList::enable_buttons() {
  if (flags & ztwod::kHidden) return;
  Engine& e = e_;
  const LabState& lab = states().lab;
  const Handle cur = e.lab_slot_cfg(e.lab_slot(lab.pilot_, lab.slot_));
  const Handle v = value_at(selected);
  add.enabled = 0;
  add.refresh();
  rename.enabled = v && e.lab_config_byte(v, 0x1c) ? 1 : 0;
  rename.refresh();
  del.enabled = v && e.lab_config_byte(v, 0x1c) ? 1 : 0;
  del.refresh();
  save.enabled = cur && !lab.flag_ ? 1 : 0;
  save.refresh();
}

// Mech3.exe 1.2 @ 0x00449c40: a modified loadout asks first (as 0x00449b30).
void LabConfigList::select(int32_t index) {
  Engine& e = e_;
  const LabState& lab = states().lab;
  LabScreen* s = e.lab_screen(lab.lab_screen_);
  if (s && s->config_edit.x38c != 0 && !s->config_edit.finish(false)) return;
  const Handle slot = e.lab_slot(lab.pilot_, lab.slot_);
  const Handle cfg = e.lab_slot_cfg(slot);  // (read unchecked)
  if (e.lab_config_modified(cfg)) {
    if (s) s->cursor_obj.erase();
    const std::string mn = display_name(e, e.config_mech(cfg));
    const std::string name = e.lab_config_name(cfg);
    if (e.message_box2(1, 0x137, 0x1ed, mn.c_str(), name.c_str()) != 1) return;
  }
  play_sound(1, 1.0f);
  FileList::select(index);
  e.lab_slot_set_config(slot, value_at(selected), lab.store_);
  if (s) s->refresh();
}

// Mech3.exe 1.2 @ 0x0044f340
void LabPart::on_left_up() {
  if (enabled && active) {
    ztwod::CheckButton::on_left_up();
    notify(1, 0);
  }
  notify(1, part);
}

// Mech3.exe 1.2 @ 0x0044f2c0
int32_t LabCheck::set_checked(int32_t on) {
  const int32_t old = ztwod::CheckButton::set_checked(on);
  for (ztwod::Text* t : labels) {
    if (on) t->hide();
    else t->show_for(-1.0f);
  }
  return old;
}

// ---------------------------------------------------------------------------
// UNASSIGNED_LIST (docs/mechlab.md, The unassigned list)

namespace {
// Mech3.exe 1.2 @ 0x00453770 and 0x00454580 (`armour` true): the tab's kind,
// and armour (equipment 1 armor, 2 armorff) only on the armour tab.
bool tab_item(Engine& e, Handle h, bool armour) {
  const LabScreen* s = e.lab_screen(states().lab.lab_screen_);
  const int32_t want = s ? s->unassigned.kind : 0;  // FIX (bug): read unchecked by the exe
  const int32_t kind = e.lab_item_kind(h);
  bool is_armour = false;
  if (e.lab_item_kind(h) == 3 && h) {
    e.lab_item_kind(h);
    const int32_t k = e.lab_item_equipment(h);
    is_armour = k == 1 || k == 2;
  }
  return kind == want && is_armour == armour;
}
bool kind_item(Engine& e, Handle h) { return tab_item(e, h, false); }
bool armour_item(Engine& e, Handle h) { return tab_item(e, h, true); }

// Mech3.exe 1.2 @ 0x004564a0: the sort's order, `by` 1 the mass (ammo by
// id), 2 the store's count (unlimited by id), 3 the slots; ties by id.
bool unassigned_before(Engine& e, int32_t by, const LabItem* a, const LabItem* b) {
  const Handle ia = a->object, ib = b->object;
  const int32_t ida = e.lab_item_id(ia), idb = e.lab_item_id(ib);
  switch (by) {
    case 1: {
      if (e.lab_item_kind(ia) == 2 || e.lab_item_kind(ib) == 2) break;
      const float wa = e.lab_item_mass(ia);
      const float wb = e.lab_item_mass(ib);
      if (!(wb < wa || wb > wa)) break;  // (fcomp: unordered as equal)
      const float wa2 = e.lab_item_mass(ia);  // both asked again
      return e.lab_item_mass(ib) > wa2;
    }
    case 2:
      if (!e.lab_item_unlimited(ia) && !e.lab_item_unlimited(ib)) {
        const int32_t ca = e.lab_item_count(ia), cb = e.lab_item_count(ib);
        if (ca != cb) return ca < cb;
      }
      break;
    case 3:
      if (a->count != b->count) return a->count < b->count;
      break;
    default:  // 0; nothing sets another
      break;
  }
  return ida < idb;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004535d0
LabUnassignedList::LabUnassignedList(Engine& e)
    : LabItemList(e), sort_item(e, 0x1b), sort_qty(e, 0x1c), sort_mass(e, 0x1d), sort_crit(e, 0x1e) {
  filter = kind_item;
}

namespace {
// Mech3.exe 1.2 @ 0x00456750 (ECX the mech, EDX the part): a part's most
// armour, twice its internal structure (0x00485400..0x00485490); the head 9.
int32_t part_armor_max(int32_t mech, int32_t part) {
  switch (part) {
    case 0: case 3: return 2 * internal_points(mech, 1);
    case 1: case 2: case 4: case 5: return 2 * internal_points(mech, 2);
    case 6: return 9;
    case 7: case 8: return 2 * internal_points(mech, 3);
    case 9: case 10: return 2 * internal_points(mech, 4);
    default: return 0;
  }
}
// Ferro-fibrous's slots by its tech: 7 Clan, else 14.
int32_t ferro_need(int32_t tech) { return tech == kClan ? 7 : 14; }
}  // namespace

// Mech3.exe 1.2 @ 0x00453840 (docs/mechlab.md, The item rules): the item of
// row `index` on the shown slot's loadout (ASSIGNED_LIST's +0x224), into the
// part ASSIGNED_LIST has chosen (none: no part to fit); never while the lab
// is read-only. The weight first, then the armour tab's or the others' rules.
bool LabUnassignedList::can_xa8(int32_t index) {
  Engine& en = e_;
  const LabState& lab = states().lab;
  if (lab.flag_) return false;
  LabScreen* s = en.lab_screen(lab.lab_screen_);  // 0x00453810: [0x00605a58] + 0x1a13c
  if (!s) return false;  // (never: the screen asks)
  const Handle cfg = s->assigned.x224;
  const int32_t part = s->assigned.part();
  const Handle item = object_at(index);
  const Handle slot = en.lab_slot(lab.pilot_, lab.slot_);
  const Handle dfn = en.mech_dfn_handle(slot ? en.lab_slot_mech(slot) : -1);
  if (!item) return false;
  const int32_t k = en.lab_item_kind(item);
  const int32_t equip = en.lab_item_equipment(item);
  const bool armour = k == 3 && (equip == 1 || equip == 2);
  const int32_t mech = en.config_mech(cfg);
  if (!armor_tab) {
    const float one = k == 2 ? 1.0f : en.lab_item_mass(item);  // a ton of ammo
    const float max = max_mass(mech);
    if (max < en.lab_config_weight(cfg) + one) return false;
  } else {
    // Armour: its points within the parts' most (all but the first three),
    // half a ton more when it is the kind on the mech, and the slots for
    // ferro-fibrous.
    if (!armour) return false;
    int32_t most = 0;
    for (int32_t p = 3; p < 11; ++p) most += part_armor_max(mech, p);
    const float tons = en.lab_config_armor_tons(cfg);
    const int32_t tech = en.lab_config_armor_tech(cfg);
    const int32_t same = en.lab_config_armor_ferro(cfg) ? 2 : 1;
    const int32_t itech = en.lab_item_tech(item);
    const float add = tech == itech && same == equip ? 0.5f : 0.0f;
    if (armor_points(itech, equip == 2, add + tons) > most) return false;
    if (max_mass(mech) < en.lab_config_weight(cfg) + add) return false;
    const int32_t left = en.lab_config_slots_left(cfg) + en.lab_config_ferro_slots(cfg);
    return (equip == 2 ? ferro_need(itech) : 0) <= left;
  }
  // 16 weapons, TAG among them; some equipment one only, or up to five.
  if ((k == 3 && equip == 13) || k == 1)
    if (en.lab_config_weapons(cfg) >= 16) return false;
  if (k == 3) {
    const int32_t n = en.lab_config_equipment(cfg, equip, kAnyTech);
    bool ok = true;
    switch (equip) {
      case 0: case 3: ok = n <= 4; break;                        // AMS, FCS
      case 4: case 8: case 12: case 13: case 14: ok = n == 0; break;  // BAP, ECM, MASC, TAG, TC
      case 6: ok = n == 0 && en.lab_config_equipment(cfg, 7, kAnyTech) == 0; break;  // C3, not with a slave
      case 7: ok = n == 0 && en.lab_config_equipment(cfg, 6, kAnyTech) == 0; break;
      default: break;
    }
    if (!ok) return false;
  }
  if (part == -1) return true;
  if (k == 3 && equip == 11 && (part == 2 || part == 3 || part == 6)) return false;  // jump jets
  const int32_t left = en.lab_config_slots_left(cfg);
  const int32_t free = en.lab_config_part_free(cfg, part);
  if (armour) {
    const int32_t rest = left - en.lab_config_ferro_slots(cfg);
    return equip == 1 ? 0 <= rest : ferro_need(en.lab_item_tech(item)) <= rest;
  }
  if (k == 3 && (equip == 9 || equip == 10)) {
    // Never: the engine holds 10 sinks (as in refill, discoveries.md).
    const int32_t limit = en.lab_config_sink_limit(cfg) - 10;
    if (limit > en.lab_config_sink_items(cfg)) return true;
    const int32_t n = en.lab_item_slots(item);
    return !(free < n) && !(left < n);
  }
  const int32_t n = en.lab_item_slots(item);
  if (free < n || left < n) return false;
  if (k != 1) return true;
  for (const Engine::LabFirepoint& f : en.lab_dfn_firepoints(dfn))  // a weapon needs a firepoint in the part
    if (f.part == part) return true;
  return false;
}

// ---------------------------------------------------------------------------
// ASSIGNED_LIST (docs/mechlab.md, The criticals page)

namespace {
// Mech3.exe 1.2 @ 0x00425db0 (ECX the part): its critical slots, 12 for the
// torsos and arms, 6 for the head and legs; 78 for the whole mech (-1).
int32_t part_slots(int32_t part) {
  switch (part) {
    case 1: case 2: case 3: case 7: case 8: return 12;
    case 4: case 5: case 6: return 6;
    case -1: return 0x4e;
    default: return 0;
  }
}
}  // namespace

bool LabAssignedList::editable() const {
  return x224 && e_.lab_config_byte(x224, 0x1c) && !states().lab.flag_;
}

// Mech3.exe 1.2 @ 0x00455d70
bool LabAssignedList::is_part_row(int32_t row) const {
  const Handle o = object_at(row);
  return o >= 1 && o <= 8;
}
// Mech3.exe 1.2 @ 0x00455df0
bool LabAssignedList::is_item_row(int32_t row) const { return object_at(row) != 0 && !is_part_row(row); }
// Mech3.exe 1.2 @ 0x00455e50
Handle LabAssignedList::item_at_row(int32_t row) const { return is_item_row(row) ? object_at(row) : 0; }
// Mech3.exe 1.2 @ 0x00455ec0
int32_t LabAssignedList::part_of_row(int32_t row) const {
  return is_part_row(row) ? static_cast<int32_t>(object_at(row)) : -1;
}
// Mech3.exe 1.2 @ 0x004560d0
int32_t LabAssignedList::row_of_part(int32_t p) const {
  if (!store || p == -1) return -1;
  for (int32_t i = 0; i < item_count(); ++i)
    if (part_of_row(i) == p) return i;
  return -1;
}

// Mech3.exe 1.2 @ 0x00455bf0: a text row in STATIC_FONT.
int32_t LabAssignedList::add_text(const char* text, int32_t n) {
  if (!text) return 0;
  const int32_t i = LabItemList::add_text(text, n);
  if (i >= 0 && i < item_count()) items[static_cast<size_t>(i)]->font = font_static;
  return i;
}

// Mech3.exe 1.2 @ 0x00455b60: an item in NORMAL_FONT when it can come off
// (STATIC_FONT otherwise).
int32_t LabAssignedList::add_object(Handle item, int32_t n) {
  const int32_t i = LabItemList::add_object(item, n);
  if (i >= 0 && i < item_count()) items[static_cast<size_t>(i)]->font = editable() ? font_normal : font_static;
  return i;
}

// Mech3.exe 1.2 @ 0x00455af0: SELECTED_FONT on the chosen row, else LARGE;
// the part listed from now on.
int32_t LabAssignedList::add_part(int32_t p) {
  const int32_t i = LabItemList::add_part(p);
  if (i >= 0 && i < item_count()) items[static_cast<size_t>(i)]->font = i == selected ? font_selected : font_large;
  listing = p;
  return i;
}

// Mech3.exe 1.2 @ 0x004559e0: the arrows step the chosen part in the list's
// order, head, centre, left and right torso, arms, legs; no wrapping at
// either end.
void LabAssignedList::scroll(int32_t by) {
  const bool down = by > 0;
  switch (part()) {
    case 1: choose(down ? 7 : 6); return;
    case 2: choose(down ? 3 : 8); return;
    case 3: choose(down ? 4 : 2); return;
    case 4: choose(down ? 5 : 3); return;
    case 5: choose(down ? 5 : 4); return;  // (the last: down stays)
    case 6: choose(down ? 1 : 6); return;  // (the first: up stays)
    case 7: choose(down ? 8 : 1); return;
    case 8: choose(down ? 2 : 7); return;
    default: choose(6); return;
  }
}

// Mech3.exe 1.2 @ 0x00455f30: part rows, and item rows the player can take off.
bool LabAssignedList::can_light(int32_t index) {
  const bool edit = editable();
  if (is_part_row(index)) return true;
  return edit && is_item_row(index);
}

// Mech3.exe 1.2 @ 0x00456130
void LabAssignedList::choose(int32_t p) { select(row_of_part(p)); }

// Mech3.exe 1.2 @ 0x00456150: the row chosen, the 3D view's part, the part
// buttons, and UNASSIGNED_LIST's rules again.
void LabAssignedList::pick(int32_t p) {
  selected = row_of_part(p);
  Engine& en = e_;
  en.lab_part_pick(p);
  if (LabScreen* s = en.lab_screen(states().lab.lab_screen_)) {
    s->check_part(p);
    s->unassigned.refill();
  }
}

// Mech3.exe 1.2 @ 0x00455fb0: an item row takes the item back to the lab's
// store; a part row is chosen (the list from it, the buttons, the 3D view).
void LabAssignedList::select(int32_t index) {
  on_leave();
  on_enter();
  Engine& en = e_;
  const LabState& lab = states().lab;
  if (!is_part_row(index)) {
    if (is_item_row(index) && !lab.flag_) en.lab_take_off(*this, index);
    return;
  }
  if (index < 0 || index >= item_count()) return;
  top = index;
  selected = index;
  refill();
  if (LabScreen* s = en.lab_screen(lab.lab_screen_)) s->check_part(part_of_row(index));
  en.lab_part_pick(part_of_row(index));
}

// Mech3.exe 1.2 @ 0x004553f0 (and 0x00455360 on each item): with `spread`,
// the part's free slots take ferro-fibrous and then endo steel rows from the
// counts left; then the loadout's items on the part (sinks of no group none
// of its slots).
void LabAssignedList::add_part_items(bool spread, int32_t* ferro, int32_t* endo) {
  Engine& en = e_;
  if (spread) {
    int32_t free = en.lab_config_part_free(x224, listing);
    if (free != 0) {
      if (*ferro != 0) {
        const int32_t n = std::min(free, *ferro);
        if (n != 0) {
          free -= n;
          *ferro -= n;
          add_text(en.message(equip_name_message(2)), n);  // 0x00484980: ferro-fibrous's name
        }
      }
      if (free != 0 && *endo != 0) {
        const int32_t n = std::min(free, *endo);
        if (n != 0) {
          *endo -= n;
          add_text("Endo-Steel Internals", n);
        }
      }
    }
  }
  const int32_t mech = en.config_mech(x224);
  for (Handle it : en.lab_config_items(x224)) {
    if (!it || en.lab_item_location(it, mech) != listing) continue;
    int32_t n = en.lab_item_slots(it);
    if (en.lab_item_kind(it) == 3) {
      const int32_t k = en.lab_item_equipment(it);
      if ((k == 9 || k == 10) && en.lab_item_group(it) == -1) n = 0;
    }
    add_object(it, n);
  }
}

// Mech3.exe 1.2 @ 0x00455c50: each part row's used and total slots (all
// used when the loadout has none left), the line in the row's colour.
void LabAssignedList::show_slots() {
  Engine& en = e_;
  const int32_t left = en.lab_config_slots_left(x224);
  for (int32_t i = 0; i < item_count(); ++i) {
    if (!is_part_row(i)) continue;
    const int32_t p = part_of_row(i);
    if (p == -1) continue;
    LabItemRow* row = nullptr;  // 0x00452de0: the shown row of that item
    for (LabItemRow* r : rows)
      if (r->index >= 0 && r->index < item_count() && items[static_cast<size_t>(r->index)] == items[static_cast<size_t>(i)]) {
        row = r;
        break;
      }
    if (!row) continue;
    const int32_t used = left == 0 ? part_slots(p) : part_slots(p) - en.lab_config_part_free(x224, p);  // 0x00425e00
    row->show_slots(used, part_slots(p));
    const uint32_t c = ztwod::host().font_fields(row->named).color_top;
    row->line.color = zvid::pack_bytes(static_cast<uint8_t>(c), static_cast<uint8_t>(c >> 8), static_cast<uint8_t>(c >> 16));
  }
}

// Mech3.exe 1.2 @ 0x004554d0 (shown only). The chosen part (the head when
// none) stays chosen; the parts in the exe's order, head first, each with
// its fixed slots' rows; then the layout, the parts' slot counts and each
// item row's mass and count.
void LabAssignedList::refill() {
  if (flags & ztwod::kHidden) return;
  Engine& en = e_;
  int32_t chosen = part();
  if (chosen == -1) chosen = 6;
  clear();  // 0x00455d60
  if (!store) return;
  listing = -1;
  const Handle cfg = x224;
  if (!cfg) return;
  const LabState& lab = states().lab;
  const bool clan = mech_tech(en.lab_slot_mech(en.lab_slot(lab.pilot_, lab.slot_))) == 1;
  const int32_t xl = en.lab_config_byte(cfg, 0x28) ? (clan ? 2 : 3) : 0;  // an XL engine's side-torso slots
  en.lab_config_recount(cfg);
  const bool spread = en.lab_config_slots_left(cfg) == 0;
  int32_t ferro = en.lab_config_byte(cfg, 0x34) ? (en.lab_config_armor_tech(cfg) == 1 ? 7 : 14) : 0;
  int32_t endo = en.lab_config_byte(cfg, 0x20) ? (clan ? 7 : 14) : 0;
  auto text = [&](int32_t id, int32_t n) { add_text(en.message(id), n); };
  add_part(6);  // the head: cockpit, life support (2), sensors
  text(0x24e, 2);
  text(0x24f, 2);
  text(0x250, 1);
  add_part_items(spread, &ferro, &endo);
  add_part(1);  // the centre torso: the engine, the gyro
  {
    const std::string name = en.lab_engine_name(cfg);
    const char* t = en.message_format(0x251, name.c_str());  // (into 0x40 bytes)
    add_text(t ? std::string(t).substr(0, 0x3f).c_str() : "", 6);
  }
  text(0x252, 4);
  add_part_items(spread, &ferro, &endo);
  for (int32_t side : {7, 8}) {  // the side torsos: an XL engine's slots
    add_part(side);
    if (xl) text(0x253, xl);
    add_part_items(spread, &ferro, &endo);
  }
  for (int32_t arm : {2, 3}) {  // the arms: shoulder, upper arm
    add_part(arm);
    text(0x255, 1);
    text(0x256, 1);
    add_part_items(spread, &ferro, &endo);
  }
  for (int32_t leg : {4, 5}) {  // the legs: hip, upper leg, lower leg, foot
    add_part(leg);
    text(0x2d5, 1);
    text(0x257, 1);
    text(0x258, 1);
    text(0x259, 1);
    add_part_items(spread, &ferro, &endo);
  }
  pick(chosen);
  layout();
  show_slots();
  const Handle font = lab.flag_ ? font_count : font_mass;
  for (LabItemRow* row : rows) {
    if (row->flags & ztwod::kHidden) continue;
    if (is_part_row(row->index)) continue;
    const Handle it = object_at(row->index);
    if (!it) continue;
    char b[32];
    std::snprintf(b, sizeof b, "%.1f", en.lab_item_weight(it, -1));
    row->mass.set_text("%s", b);
    row->mass.set_font(font);
    row->mass.show_for(-1.0f);
    if (!en.lab_item_unlimited(it)) {
      row->qty.set_text("%d", en.lab_item_count(it));
      row->qty.set_font(font);
      row->qty.show_for(-1.0f);
    }
  }
}

// Mech3.exe 1.2 @ 0x0044d3e0
void LabScreen::check_part(int32_t p) {
  part_head.set_checked(p == 6);
  for (LabPart& b : parts) b.set_checked(p == b.part);
}

// Engine's take-off by default: what the lab test leaves out (its loadouts
// are stand-ins).
void Engine::lab_take_off(LabAssignedList& list, int32_t index) {
  (void)list;
  unported("lab.take_off", {static_cast<uint32_t>(index)});
}

// Mech3.exe 1.2 @ 0x00455fb0's item row: all of the item back to the lab's
// store ([0x00605a80]) and off the shown slot's loadout, then the refresh.
void lab_take_off(Engine& en, LabAssignedList& list, int32_t index) {
  const LabState& lab = states().lab;
  const Handle slot = en.lab_slot(lab.pilot_, lab.slot_);  // 0x0044e680
  Config* cfg = en.lab_config_of(slot ? en.lab_slot_cfg(slot) : 0);
  auto* item = const_cast<ConfigItem*>(en.item_of(list.item_at_row(index)));
  Store* store = en.store_of(lab.store_);
  if (cfg && item && store && store->add_copy(item, item->count)) {
    cfg->remove(item, item->count);
    cfg->modified = 1;
    cfg->recount();
    list.play_sound(1, 1.0f);
  }
  en.lab_page_refresh(lab.lab_screen_);
}

// Engine's put-on by default: the lab test's stand-in for +0xac.
void Engine::lab_put_on(LabUnassignedList& list, int32_t index) { list.call(0xac, {static_cast<uint32_t>(index)}); }

uint8_t& lab_firepoint_toggle() {
  static uint8_t toggle = 1;
  return toggle;
}

// Mech3.exe 1.2 @ 0x00454600. Ammo goes on by the lab's rounds; weapons
// past 16 are refused with a message; armour goes on half a ton at a time,
// or replaces the mech's other kind (the old back to the store, as much of
// the new as there was of the old, the sections spread anew when over);
// sinks of the other kind are refused, and jump jets past the mech's
// limit; then a weapon takes a firepoint in the chosen part, alternately the
// last and the first that fits (missiles only those that scatter).
void lab_put_on(Engine& en, LabUnassignedList& list, int32_t index) {
  list.on_leave();
  list.on_enter();
  const Handle ih = list.object_at(index);
  if (!ih || !list.can_xa8(index)) return;
  const LabState& lab = states().lab;
  // 0x0044dc80: the shown slot (0x0044e680) has a loadout and the lab can change it.
  const Handle shown = en.lab_slot(lab.pilot_, lab.slot_);
  if (!shown || !en.lab_slot_cfg(shown) || lab.flag_) return;
  Store* store = en.store_of(list.store);
  Config* cfg = en.lab_config_of(en.lab_slot_cfg(en.lab_slot(lab.pilot_, lab.slot_)));
  auto* item = const_cast<ConfigItem*>(en.item_of(ih));  // (the store's own item)
  if (!store || !cfg || !item) return;  // (never: the rule passed)
  int32_t n = 1;
  bool put = true;
  const int k = item->kind();
  const int32_t equip = k == 3 ? static_cast<EquipmentItem*>(item)->equipment : -1;
  const int32_t tech = k == 3 ? static_cast<EquipmentItem*>(item)->tech : 0;
  if (k == 2) {
    n = lab_rounds(static_cast<AmmoItem*>(item)->weapon, store->full ? 100000 : item->count);  // 0x004b4900
  } else if (k == 1) {
    if (cfg->weapon_count() >= 16) {
      put = false;
      en.lab_message("Only 16 weapons are allowed");
    }
  } else if (k == 3 && (equip == 1 || equip == 2)) {
    put = false;
    EquipmentItem probe;  // 0x0041ec00
    probe.count = 1, probe.equipment = equip, probe.tech = tech;
    if (ConfigItem* same = cfg->find_like(&probe)) {  // half a ton more
      if (++same->count < 0) same->count = 0;
      store->remove(item, 1);
      cfg->armor_tons = cfg->armor_tons - -0.5f;
      if (cfg->armor_tons < 0.0f) cfg->armor_tons = 0.0f;
      cfg->modified = 1;
      cfg->recount();
    } else {
      EquipmentItem old;  // 0x0044ec90(1), the mech's armour now
      old.count = 1, old.equipment = cfg->ferro ? 2 : 1, old.tech = cfg->armor_tech;
      if (ConfigItem* was = cfg->find_like(&old)) {
        const int32_t had = was->count;
        const int32_t take = item->b10 ? had : std::min(item->count, had);
        store->add_copy(was, had);
        cfg->remove(was, had);
        if (cfg->add_item(item, take)) store->remove(item, take);
        cfg->modified = 1;
        cfg->recount();
        int32_t sum = 0;  // 0x004513f0
        for (int32_t v : cfg->armor) sum += v;
        if (armor_points(cfg->armor_tech, cfg->ferro != 0, cfg->armor_tons) - sum < 0) cfg->spread_armor(cfg->mech);
      } else if (cfg->add_item(item, 1)) {
        store->remove(item, 1);
        cfg->modified = 1;
        cfg->recount();
      }
    }
    item = nullptr;
  } else if (k == 3 && (equip == 9 || equip == 10)) {
    EquipmentItem probe;  // 0x0044ec90(1): one kind of sink on a mech
    probe.count = 1, probe.equipment = equip, probe.tech = tech;
    put = cfg->find_like(&probe) != nullptr || cfg->sink_items() == 0;
    if (!put) en.lab_message(en.message(0xf02));  // "Incompatable Heat Sink Type"
  } else if (k == 3 && equip == 11) {
    put = cfg->jump_jets() < cfg->jump_jet_limit();
    if (!put) {
      const int32_t limit = cfg->jump_jet_limit();
      const std::string name = display_name(en, cfg->mech);  // 0x00484670
      // 0xfc1 "%1 can only carry %2!d! jump jets" into 0x46 bytes, then the
      // text as CString::Format's format and the message's.
      std::string s = en.message_format_sd(0xfc1, name.c_str(), limit, true).substr(0, 0x45);
      en.lab_message(s.c_str());
    }
  }
  const int32_t part = en.lab_assigned_part();
  const Handle dfn = en.mech_dfn_handle(cfg->mech);
  int32_t at = part;
  if (item && item->kind() == 1) {
    const std::vector<Engine::LabFirepoint> fps = en.lab_dfn_firepoints(dfn);
    std::vector<int32_t> fit;  // the part's, by their place in the definition
    for (size_t i = 0; i < fps.size(); ++i)
      if (fps[i].part == part) fit.push_back(static_cast<int32_t>(i));
    if (fit.empty()) {
      put = false;
    } else {
      const int32_t w = static_cast<WeaponItem*>(item)->weapon;
      const bool missile = (w >= 0x11 && w <= 0x14) || (w >= 0x1a && w <= 0x1f);
      const auto ok = [&](int32_t i) { return !missile || fps[static_cast<size_t>(fit[static_cast<size_t>(i)])].radius > 0.0f; };
      int32_t chosen = -1;
      const int32_t count = static_cast<int32_t>(fit.size());
      if (lab_firepoint_toggle()) {  // the last that fits
        lab_firepoint_toggle() = 0;
        for (int32_t i = 0; i < count; ++i)
          if (ok(i)) chosen = i;
      } else {  // the first
        lab_firepoint_toggle() = 1;
        for (int32_t i = count - 1; i >= 0; --i)
          if (ok(i)) chosen = i;
      }
      // None that fits leaves the part as its "firepoint" (the exe's).
      if (chosen != -1) {
        put = true;
        at = fit[static_cast<size_t>(chosen)];
      }
    }
  }
  if (put && item) {
    if (ConfigItem* added = cfg->add_item(item, n)) {
      list.play_sound(1, 1.0f);  // 0x00452c80
      added->set_part(at);
      store->remove(item, n);
      cfg->modified = 1;
      cfg->recount();
    }
  }
  list.on_leave();
  list.on_enter();
  en.lab_page_refresh(lab.lab_screen_);  // 0x0044cf50
  list.on_leave();
  list.on_enter();
}

bool LabUnassignedList::call_bool(uint32_t slot, int32_t index) const {
  char key[24];
  std::snprintf(key, sizeof key, "lab.vt+0x%x", slot);
  return (e->unported(key, {at, index}) & 0xff) != 0;
}

// Mech3.exe 1.2 @ 0x00454db0
void LabUnassignedList::show_for(float seconds) {
  LabItemList::show_for(seconds);
  titles.show_for(seconds);
  weapon.show_for(seconds);
  ammo.show_for(seconds);
  equip.show_for(seconds);
  armor.show_for(seconds);
  sort_item.show_for(seconds);
  sort_mass.show_for(seconds);
  sort_qty.show_for(seconds);
  sort_crit.show_for(seconds);
  set_tab(kind, armor_tab != 0);
}

// Mech3.exe 1.2 @ 0x00454e70: the engine's table goes too.
void LabUnassignedList::hide() {
  LabItemList::hide();
  titles.hide();
  for (LabSelect* b : {&weapon, &ammo, &equip, &armor}) b->set_checked(0);
  for (LabSelect* b : {&weapon, &ammo, &equip, &armor}) b->hide();
  sort_item.hide();
  sort_mass.hide();
  sort_qty.hide();
  sort_crit.hide();
  if (LabScreen* s = e_.lab_screen(states().lab.lab_screen_)) s->engine_table.hide();
}

// Mech3.exe 1.2 @ 0x00454fc0: the base's reading, SHORT_SIZE, then its buttons.
bool LabUnassignedList::read_control(zrdr::Node* node, ztwod::Dialog* dialog) {
  const bool r = LabItemList::read_control(node, dialog);
  zrdr::get_ivec2(node, "SHORT_SIZE", short_size);
  long_size[0] = size[0];
  long_size[1] = size[1];
  down_dy = down->get_y() - size[1];
  dialog->bind_button(&weapon, "SELECT_WEAPON");
  dialog->bind_button(&ammo, "SELECT_AMMO");
  dialog->bind_button(&equip, "SELECT_EQUIP");
  dialog->bind_button(&armor, "SELECT_ARMOR");
  dialog->bind_button(&sort_item, "SORT_ITEM");
  dialog->bind_button(&sort_qty, "SORT_QTY");
  dialog->bind_button(&sort_mass, "SORT_MASS");
  dialog->bind_button(&sort_crit, "SORT_CRIT");
  dialog->bind_button(&titles, "ALIST_TITLES");
  return r;
}

// Mech3.exe 1.2 @ 0x00454f60: the base's, in NORMAL_FONT.
int32_t LabUnassignedList::add_object(Handle item, int32_t n) {
  const int32_t i = LabItemList::add_object(item, n);
  if (i >= 0 && i < item_count()) items[static_cast<size_t>(i)]->font = font_normal;
  // (else the exe writes the font to address 0xc; add never fails)
  return i;
}

// Mech3.exe 1.2 @ 0x004550d0: a tab's button checked; the equipment and
// armour tabs take SHORT_SIZE (the engine's table under the equipment's).
void LabUnassignedList::set_tab(int32_t k, bool arm) {
  LabScreen* s = e_.lab_screen(states().lab.lab_screen_);
  if (kind != k || arm) top = 0;
  kind = k;
  armor_tab = arm ? 1 : 0;
  if ((flags & ztwod::kHidden) || !s) return;
  for (LabSelect* b : {&weapon, &ammo, &equip, &armor}) b->set_checked(0);
  const int32_t* use = long_size;
  if (k == 1) {
    weapon.set_checked(1);
  } else if (k == 2) {
    ammo.set_checked(1);
  } else if (k == 3) {
    equip.set_checked(armor_tab == 0);
    armor.set_checked(armor_tab);
    use = short_size;
  } else {
    return;
  }
  size[0] = use[0];
  size[1] = use[1];
  if (down) down->set_pos(down->get_x(), size[1] + down_dy);
  if (k == 3 && !armor_tab) s->engine_table.show_for(-1.0f);
  else s->engine_table.hide();
  s->panel_right.show_look(1);
}

// Mech3.exe 1.2 @ 0x00453f00
void LabUnassignedList::set_sort(int32_t by) {
  sort = by;
  refill();
}

// Mech3.exe 1.2 @ 0x00453f20: ALIST_TITLES's slot counts, the tab's items
// sorted, each row's mass and count (greyed when +0xa4 says it cannot go
// on), then the screen's controls for the tab: the armour tab hides the
// assigned list and the parts for ARMOR_TITLES.
void LabUnassignedList::refill() {
  Engine& en = e_;
  const LabState& lab = states().lab;
  const Handle cfg = en.lab_slot(lab.pilot_, lab.slot_) ? en.lab_slot_cfg(en.lab_slot(lab.pilot_, lab.slot_)) : 0;
  if (!(flags & ztwod::kHidden)) {
    const int32_t left = cfg ? en.lab_config_slots_left(cfg) : 0;
    const int32_t ferro = cfg ? en.lab_config_ferro_slots(cfg) : 0;
    const int32_t endo = cfg ? en.lab_config_endo_slots(cfg) : 0;
    // FIX (bug): a missing label is a call through null in the exe (a broken dialog file).
    auto label = [&](size_t i) { return i < titles.labels.size() ? titles.labels[i] : nullptr; };
    if (ztwod::Text* t = label(0)) t->set_text_id(0xf5b, left);
    size_t i = 1;
    if (endo) {
      if (ztwod::Text* t = label(1)) t->set_text_id(0x24c, endo);
      if (ztwod::Text* t = label(1)) t->show_for(-1.0f);
      i = 2;
    }
    if (ferro) {
      if (ztwod::Text* t = label(i)) t->set_text_id(0x24d, ferro);
      if (ztwod::Text* t = label(i)) t->show_for(-1.0f);
      ++i;
    }
    for (; i < 3; ++i)
      if (ztwod::Text* t = label(i)) t->hide();
  }
  set_tab(kind, armor_tab != 0);
  if ((flags & ztwod::kHidden) || !store) return;
  filter = armor_tab ? armour_item : kind_item;
  clear();
  if (Store* st = en.store_of(store))
    for (const ConfigItem* it : st->items) {
      const Handle h = en.item_handle(it);
      if (!filter || filter(en, h)) add_object(h, -1);
    }
  // Never true: the engine holds 10 sinks and the test is fewer than none
  // (docs/discoveries.md); it would list the store's sinks with no slots.
  if (en.lab_config_sink_limit(cfg) - 10 > en.lab_config_sink_items(cfg) && !armor_tab && kind == 3) {
    for (LabItem* it : items) {
      const Handle h = it->object;
      if (!h || en.lab_item_kind(h) != 3) continue;
      en.lab_item_kind(h);
      const int32_t k = en.lab_item_equipment(h);
      if (k == 9 || k == 10) it->count = 0;
    }
  }
  vc6_sort(items.begin(), items.end(), [&](LabItem* a, LabItem* b) { return unassigned_before(en, sort, a, b); });
  layout();
  for (LabItemRow* row : rows) {
    if (row->flags & ztwod::kHidden) continue;
    const int32_t i = row->index;
    Handle grey = 0;
    if (!can_light(i)) {
      grey = font_grey;
      row->set_font(font_grey);
    }
    const Handle h = object_at(i);
    if (en.lab_item_kind(h) == 2) {
      en.lab_item_kind(h);
      row->mass.set_text_plain("1.0");  // a ton of ammo
    } else {
      char b[32];
      std::snprintf(b, sizeof b, "%.1f", static_cast<double>(en.lab_item_mass(h)));
      row->mass.set_text("%s", b);
    }
    row->mass.set_font(grey ? font_count : font_mass);
    row->mass.show_for(-1.0f);
    if (!en.lab_item_unlimited(h)) {
      row->qty.set_text("%d", en.lab_item_count(h));
      row->qty.set_font(grey ? font_count : font_mass);
      row->qty.show_for(-1.0f);
    }
    if (grey) row->count.set_font(font_count);
  }
  LabScreen* s = en.lab_screen(lab.lab_screen_);
  if (!s) return;  // FIX (bug): read unchecked by the exe
  LabAssignedList& as = s->assigned;
  if ((as.flags & ztwod::kHidden) && !armor_tab) {
    as.show_for(-1.0f);
    as.refill();
  } else if (armor_tab) {
    en.lab_part_pick(-1);
    as.hide();
  }
  for (LabPart* p : {&s->part_head, &s->parts[0], &s->parts[1], &s->parts[2], &s->parts[3], &s->parts[4],
                     &s->parts[5], &s->parts[6]}) {
    if (armor_tab) p->hide();
    else p->show_for(-1.0f);
  }
  if (armor_tab) en.vcall(s->armor_titles, 0x60, {zgame::bits(-1.0f)});
  else en.vcall(s->armor_titles, 0x64, {});
}

// ---------------------------------------------------------------------------
// The weapon groups page (docs/mechlab.md, The weapon groups page)

// Mech3.exe 1.2 @ 0x0044dfb0
void LabLocationTag::set_pos(int32_t px, int32_t py) {
  HudPanel::set_pos(px, py);
  const int32_t w = image ? image->width : 0;
  text.set_pos(px - 5 + w, py);
}

// Mech3.exe 1.2 @ 0x0044dff0
void LabLocationTag::update(float dt) {
  if (flags & ztwod::kHidden) return;
  HudPanel::update(dt);
  text.update(dt);
}

// Mech3.exe 1.2 @ 0x0044dee0
LabWeaponList::~LabWeaponList() {
  for (FileRow* r : rows) {
    delete r->attached;
    r->attached = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x0044df70
bool LabWeaponList::read_control(zrdr::Node* node, ztwod::Dialog* dialog) {
  const bool r = FileList::read_control(node, dialog);
  font_static = ztwod::host().font_find(zrdr::get_string(node, "STATIC_FONT"));
  return r;
}

// Mech3.exe 1.2 @ 0x0044e630
int32_t LabWeaponList::group_of(int32_t row) const {
  const int32_t v = row >= 0 && row < item_count() ? items[static_cast<size_t>(row)]->value : 0;
  return v < 0 || v > 4 ? -1 : v;
}

// Mech3.exe 1.2 @ 0x0044e560 (the grouped's; the other's 0x0040bad0)
bool LabWeaponList::can_choose(int32_t i) { return !grouped || group_of(i) != -1; }
// Mech3.exe 1.2 @ 0x0044e540
bool LabWeaponList::can_roll(int32_t i) { return !grouped || !can_choose(i); }

// Mech3.exe 1.2 @ 0x0044e450 (the ungrouped's: the row's value) and
// 0x0044e5e0 (the grouped's: not a group's row)
Handle LabWeaponList::weapon_at(int32_t row) const {
  if (grouped && group_of(row) != -1) return 0;
  return row >= 0 && row < item_count() ? static_cast<Handle>(items[static_cast<size_t>(row)]->value) : 0;
}

// Mech3.exe 1.2 @ 0x0044e3f0 (the ungrouped's: into the chosen group, -1
// when none is) and 0x0044e490 (the grouped's: out of its group; a group's
// row is chosen).
void LabWeaponList::select(int32_t index) {
  Engine& en = e_;
  LabScreen* s = en.lab_screen(states().lab.lab_screen_);
  if (!s) return;  // (the exe's [0x00605a58], always there)
  const int32_t group = grouped ? -1 : s->grouped.group_of(s->grouped.selected);
  const Handle it = weapon_at(index);
  if (!it) {
    if (grouped) FileList::select(index);
    return;
  }
  play_sound(1, 1.0f);  // 0x0048d3b0
  en.lab_set_item_group(it, group);
  s->refresh_groups();
}

// Mech3.exe 1.2 @ 0x0044e3d0
void LabWeaponList::add_weapon(Handle item) {
  const std::string label = e_.lab_item_label(item);
  add_item(label.c_str(), static_cast<int32_t>(item));
}

// Mech3.exe 1.2 @ 0x0044e580
void LabWeaponList::add_group(int32_t g) {
  const char* t = e_.message_format_n(0x24b, g + 1);  // (into 0x40 bytes)
  add_item(t ? std::string(t).substr(0, 0x3f).c_str() : "", g);
}

// Mech3.exe 1.2 @ 0x0044e4e0
void LabWeaponList::select_group(int32_t g) {
  if (g < 0 || g > 4) g = 0;
  for (int32_t i = 0; i < item_count(); ++i)
    if (group_of(i) == g) select(i);
}

// Mech3.exe 1.2 @ 0x0044f240
void LabWeaponList::layout() {
  FileList::layout();
  lay_rows();
}

// Mech3.exe 1.2 @ 0x0044e020: down from OFFSET, each shown row (or the
// selected one's place) a row height and 5 apart: a weapon's row in
// NORMAL_FONT with its tag (the part it is on, 172 x 12, 3 to the left), a
// group's in STATIC_FONT, 5 lower after the first, the selected row's text
// and BAR on it.
void LabWeaponList::lay_rows() {
  constexpr int32_t kTagW = 172, kTagH = 12;  // [0x005e42b8], [0x005e42bc]
  int32_t y = get_y() + offset_y;
  if (rows.empty()) return;
  Engine& en = e_;
  const LabState& lab = states().lab;
  if (!lab.lab_screen_) return;
  const Handle slot = en.lab_slot(lab.pilot_, lab.slot_);
  if (!slot) return;
  const int32_t mech = en.lab_slot_mech(en.lab_slot(lab.pilot_, lab.slot_));
  int32_t i = 0;
  for (FileRow* row : rows) {
    const bool here = selected - top == i && selected_row && !(selected_row->flags & ztwod::kHidden);
    if ((row->flags & ztwod::kHidden) && !here) continue;
    const int32_t v = row->index >= 0 && row->index < item_count() ? items[static_cast<size_t>(row->index)]->value : 0;
    if (v >= 5 && v != 0) {  // a weapon
      row->set_pos(row->get_x(), y);
      row->set_font(font_normal);
      auto* tag = static_cast<LabLocationTag*>(row->attached);
      if (!tag) {
        tag = new LabLocationTag;
        row->attached = tag;
        tag->set_size(kTagW, kTagH);
        tag->set_color(zvid::pack_bytes(0x19, 0x19, 0x19));  // 0x0043aae0
        tag->set_opacity(1.0f);
      }
      tag->show_for(-1.0f);
      tag->set_pos(row->get_x() - 3, row->get_y());
      tag->text.set_font(font_normal);
      tag->text.align = 2;
      tag->text.set_text_plain(en.part_text(en.lab_item_location(static_cast<Handle>(v), mech)));  // 0x00484d70
    } else {  // a group
      row->set_font(font_static);
      if (row->attached) row->attached->hide();
      row->set_pos(row->get_x(), y + (i != 0 ? 5 : 0));
      if (here) {
        selected_row->set_pos(row->get_x(), row->get_y());
        if (bar_image) bar_image->set_pos(row->get_x() + bar_dx, row->get_y() + bar_dy);
      }
    }
    ++i;
    y = row->get_y() + kTagH + 5;
  }
}

// Mech3.exe 1.2 @ 0x0044dcf0: on the weapon groups page with a loadout
// shown, both lists emptied and refilled: the weapons of no group, then
// each group's row and its weapons; the chosen group kept.
void LabScreen::refresh_groups() {
  if (page != 4) return;
  Engine& e = engine();
  const LabState& lab = states().lab;
  // The shown slot's loadout, asked for again each time (0x0044e680 twice a read).
  auto shown = [&]() -> Handle {
    return e.lab_slot(lab.pilot_, lab.slot_) ? e.lab_slot_cfg(e.lab_slot(lab.pilot_, lab.slot_)) : 0;
  };
  if (!shown()) return;
  const Handle cfg = shown();
  const int32_t chosen = grouped.group_of(grouped.selected);
  shown();  // (its mech read, unused)
  ungrouped.clear_items();
  ungrouped.layout();
  grouped.clear_items();
  grouped.layout();
  const std::vector<Handle> items = e.lab_config_items(cfg);
  for (Handle it : items)
    if (e.lab_item_kind(it) == 1 && e.lab_item_group(it) == -1) ungrouped.add_weapon(it);
  for (int32_t g = 0; g < 5; ++g) {
    grouped.add_group(g);
    for (Handle it : items)
      if (e.lab_item_kind(it) == 1 && e.lab_item_group(it) == g) grouped.add_weapon(it);
  }
  grouped.select_group(chosen);
}

// Mech3.exe 1.2 @ 0x004569c0 (the panel), the buttons, 0x0044f040(7) (its rows)
LabEngineTable::LabEngineTable(Engine& e)
    : faster(e, 0x13), b2e0(e, 0x14), b438(e, 0x15), b590(e, 0x16) {
  for (int i = 0; i < 7; ++i) rows.push_back(new LabRow);
}
// Mech3.exe 1.2 @ 0x0044ef90
LabEngineTable::Rows::~Rows() {
  for (LabRow*& r : *this) {
    delete r;
    r = nullptr;
  }
}
bool LabEngineTable::read_control(zrdr::Node*, ztwod::Dialog*) {
  call(0x84);
  return true;
}
void LabEngineTable::after_bind() { call(0x88); }
void LabEngineTable::show_for(float seconds) { call(0x60, {zgame::bits(seconds)}); }
void LabEngineTable::hide() { call(0x64); }

// ---------------------------------------------------------------------------
// CONFIG_EDIT

// Mech3.exe 1.2 @ 0x0044a6e0
void LabEditBar::on_return() {
  if (e_.video_mode() != 0) {
    e_.video_begin_3d();
    e_.video_end_3d();
  }
  finish(false);
}

// Mech3.exe 1.2 @ 0x0044f4b0
void LabEditBar::on_cancel() { finish(true); }

// Mech3.exe 1.2 @ 0x0044a2f0
void LabEditBar::start(LabConfigList& list, bool rename) {
  const Handle v = list.value_at(list.selected);
  x38c = v;
  if (!v || !list.selected_row) return;
  x390 = rename ? 1 : 0;
  set_text(e_.lab_config_name(v).c_str());
  set_pos(list.selected_row->get_x(), list.selected_row->get_y());
  list.selected_row->hide();
  show_for(-1.0f);
  set_focus(1);
}

// Mech3.exe 1.2 @ 0x0044a3a0: the typed name taken (or, cancelled or empty,
// SAVE's copy thrown away). A name another loadout has asks to overwrite it
// (0x137 / 0xdcb), unless that one is the game's own: message 0xfb2 and the
// bar stays. The slot's loadout and the named one both get the name; the
// list is written.
bool LabEditBar::finish(bool cancel) {
  if (!x38c) return true;
  Engine& e = e_;
  const LabState& lab = states().lab;
  LabScreen* s = e.lab_screen(lab.lab_screen_);
  const std::string text = get_text();
  const Handle editing = static_cast<Handle>(x38c);
  if (!cancel && !text.empty()) {
    const Handle list = e.lab_dfn_configs(e.mech_dfn_handle(e.config_mech(editing)));
    bool taken = false;
    if (!x390 || text != e.lab_config_name(editing)) taken = e.lab_list_find(list, text.c_str()) != 0;
    auto slot_cfg = [&]() {  // 0x0044e680's slot, asked twice when there is one
      return e.lab_slot(lab.pilot_, lab.slot_) ? e.lab_slot_cfg(e.lab_slot(lab.pilot_, lab.slot_)) : 0;
    };
    auto rename = [&](Handle c) {
      e.lab_config_set_name(c, text.c_str());
      e.lab_config_set_modified(c, 0);
      e.lab_config_recount(c);
    };
    if (taken) {
      const Handle found = e.lab_list_find(list, text.c_str());
      if (!e.lab_config_byte(found, 0x1c)) {
        if (s) s->message.set_text_id(0xfb2);
        return false;
      }
      if (s) s->cursor_obj.erase();
      set_focus(0);
      const bool yes = e.message_box(1, 0x137, 0xdcb) == 1;
      set_focus(1);
      if (!yes) return false;
      rename(slot_cfg());
      rename(editing);
      if (editing == found) {
        e.lab_list_save(list);
      } else {
        e.lab_list_remove(list, found);
        e.lab_config_delete(found);
        e.lab_list_save(list);
      }
    } else {
      rename(slot_cfg());
      rename(editing);
      e.lab_list_save(list);
    }
  } else if (!x390) {
    const Handle list = e.lab_dfn_configs(e.mech_dfn_handle(e.config_mech(editing)));
    e.lab_list_remove(list, editing);
    e.lab_config_delete(editing);
    x38c = 0;
  }
  hide();
  x38c = 0;
  set_focus(0);
  if (s) s->refresh();
  return true;
}

// ---------------------------------------------------------------------------
// MESSAGE_AREA

// Mech3.exe 1.2 @ 0x0044e700: SOUND, played with each text.
bool LabMessage::read(zrdr::Node* node, ztwod::Dialog* dialog) {
  x4b0 = static_cast<uint32_t>(ztwod::host().sound_find(zrdr::get_string(node, "SOUND")));
  x4b4 = 1.0f;
  return ztwod::FontText::read(node, dialog);
}

// Mech3.exe 1.2 @ 0x0044e770
void LabMessage::set_text_id(uint32_t id, ...) {
  va_list args;
  va_start(args, id);
  set_text_id_v(id, args);
  va_end(args);
  if (x4b0) ztwod::host().sound_play(static_cast<int32_t>(x4b0), x4b4);
}

// Mech3.exe 1.2 @ 0x0044e740
void LabMessage::set_text(const char* format, ...) {
  va_list args;
  va_start(args, format);
  set_text_v(format, args);
  va_end(args);
  if (x4b0) ztwod::host().sound_play(static_cast<int32_t>(x4b0), x4b4);
}

// ---------------------------------------------------------------------------
// The screen

// Mech3.exe 1.2 @ 0x0044b920
LabScreen::LabScreen(Engine& e, int32_t slots)
    : MenuScreen(e, 0),
      loadouts(e),
      tabs(e, g_none),
      mechs(e),
      chassis(e),
      configs(e),
      config_edit(e),
      skins(e),
      sort_name(e, 0x1f),
      sort_mass(e, 0x20),
      unassigned(e),
      assigned(e),
      ungrouped(e, false),
      grouped(e, true),
      engine_table(e) {
  const struct {
    LabLater* l;
    uint32_t at;
  } later[] = {{&loadouts, 0x11508}, {&unassigned, 0x19290},
               {&engine_table, 0x1a8c8}};
  for (const auto& x : later) x.l->e = &e, x.l->at = x.at;
  chassis_data = e.construct(0x457600, {});  // new(0x188)
  mech_data = e.construct(0x456cd0, {});     // new(0x5ac)
  armor_titles = e.construct(0x44fad0, {});  // new(0x31e0), made inline
  if (load("dlgMechLabII.zrd", "LABMENU", true)) {
    bind_primitive(&temp_background, "TEMP_BACKGROUND");
    bind_primitive(&panel_left, "PANEL_LEFT");
    bind_primitive(&panel_right, "PANEL_RIGHT");
    bind_primitive(&panel_center, "PANEL_CENTER");
    bind_primitive(&message, "MESSAGE_AREA");
    bind_button(&current_chassis, "CURRENT_CHASSIS");
    bind_primitive(&icon_glow, "ICON_GLOW");
    static const char* const kTabs[] = {"ICON_CHASSIS",    "ICON_WEAPONS",     "ICON_EQUIPMENT", "ICON_CRITICALS",
                                        "ICON_WEAPONGROUP", "ICON_MISSIONDATA", "ICON_ACCEPT"};
    for (int i = 0; i < 7; ++i) {
      tabs[i].id = i;
      bind_button(&tabs[i], kTabs[i]);
    }
    bind_primitive(&pilot_glow, "PILOT_GLOW");
    static const char* const kMechs[] = {"ICON_MECH1", "ICON_MECH2", "ICON_MECH3", "ICON_MECH4"};
    for (int i = 0; i < 4; ++i) {
      mechs[i].id = 8 + i;
      bind_button(&mechs[i], kMechs[i]);
    }
    // The parts by their number (0..8 the mech's parts) and the order bound.
    part_head.part = 6;
    bind_button(&part_head, "PART_HEAD");
    static const struct {
      int idx, part;
      const char* name;
    } kParts[] = {{0, 1, "PART_TORSO"}, {1, 7, "PART_LTORSO"}, {2, 8, "PART_RTORSO"}, {5, 4, "PART_LLEG"},
                  {6, 5, "PART_RLEG"},  {3, 2, "PART_LARM"},   {4, 3, "PART_RARM"}};
    for (const auto& p : kParts) {
      parts[p.idx].part = p.part;
      bind_button(&parts[p.idx], p.name);
    }
    bind_button(&chassis, "CHASSIS_LIST");
    bind_button(&configs, "CONFIG_LIST");
    bind_button(&config_edit, "CONFIG_EDIT");
    bind_button(&engine_table, "ENGINE_TABLE");
    bind_button(&ungrouped, "WEAPONS_UNGROUPED");
    bind_button(&grouped, "WEAPONS_GROUPED");
    bind_button(&sort_mass, "SORT_CHASSISMASS");
    bind_button(&sort_name, "SORT_CHASSISNAME");
    bind_button(&skins, "SKIN_LIST");
    bind_button(&unassigned, "UNASSIGNED_LIST");
    bind_button(&assigned, "ASSIGNED_LIST");
    bind_later(armor_titles, "ARMOR_TITLES");
    bind_later(mech_data, "MECH_DATA");
    bind_later(chassis_data, "CHASSIS_DATA");
    bind_button(&loadouts, "LOADOUT_LIST");
    unload();
  }
  set_flags_all(0);
  set_active(1);
  config_edit.hide();
  engine_table.hide();
  for (int i = 0; i < 7; ++i) show_page(i, false);
  tabs[5].enabled = 0;  // ICON_MISSIONDATA
  tabs[5].refresh();
  for (int i = slots < 4 ? slots : 4; i < 4; ++i) mechs[i].hide();
  message.set_text(e.message(0xdcc));
}

// Mech3.exe 1.2 @ 0x0044c600: the panels made with new deleted first, the
// members after (in the original's order, the reverse of their making).
LabScreen::~LabScreen() {
  Engine& e = engine();
  if (chassis_data) e.vcall(chassis_data, 0x00, {1});
  if (mech_data) e.vcall(mech_data, 0x00, {1});
  if (armor_titles) e.vcall(armor_titles, 0x00, {1});
}

// 0x0059f8b0 on a panel the port does not have yet: its reading and the
// after-bind are its own.
void LabScreen::bind_later(Handle h, const char* name) {
  zrdr::Node* buttons = node ? zrdr::find(node, "BUTTONS") : nullptr;
  if (!buttons || !zrdr::find(buttons, name)) return;
  Engine& e = engine();
  e.vcall(h, 0x84, {});
  e.vcall(h, 0x88, {});
}

// Mech3.exe 1.2 @ 0x0044d4d0: a press. A rename under way (CONFIG_EDIT's
// +0x38c) must finish first (0x0044a3a0, step 4) unless it is CONFIG_SAVE
// (16). 0..6 the pages, 8..11 the lance's slots; the rest are the pages' own.
void LabScreen::on_button(int32_t id, int32_t a, int32_t b) {
  (void)a, (void)b;
  Engine& e = engine();
  if (id != 0x10 && config_edit.x38c != 0 && !config_edit.finish(false)) return;
  if (id >= 0 && id < 7) {
    switch_page(id);
    return;
  }
  const LabState& lab = states().lab;
  auto slot_cfg = [&]() {  // 0x0044e680's slot, asked twice when there is one
    const Handle slot = e.lab_slot(lab.pilot_, lab.slot_);
    return slot ? e.lab_slot_cfg(e.lab_slot(lab.pilot_, lab.slot_)) : 0;
  };
  if (id == 16) {  // CONFIG_SAVE: a copy at the list's end, named over its row when the name is taken
    if (config_edit.x38c) {
      config_edit.finish(false);
      return;
    }
    if (!slot_cfg()) return;
    const Handle list = e.lab_dfn_configs(e.mech_dfn_handle(e.lab_slot_mech(e.lab_slot(lab.pilot_, lab.slot_))));
    const Handle cfg = slot_cfg();
    const std::string name = e.lab_config_name(cfg);
    const Handle existing = e.lab_list_find(list, name.c_str());
    const Handle copy = e.lab_config_copy(name.c_str(), cfg);
    e.lab_list_append(list, copy);
    configs.fill();
    configs.select_config(copy);
    if (existing) edit_name(true, false);
    return;
  }
  if (id == 15) {  // CONFIG_DELETE: the player's own only; the list written
    const Handle v = configs.value_at(configs.selected);
    if (!v || !e.lab_config_byte(v, 0x1c)) return;
    const Handle list = e.lab_dfn_configs(e.mech_dfn_handle(e.config_mech(v)));
    e.lab_list_remove(list, v);
    e.lab_config_delete(v);
    e.lab_list_save(list);
    refresh();
    return;
  }
  if (id == 14) {  // CONFIG_RENAME
    if (slot_cfg()) edit_name(true, true);
    return;
  }
  if (id >= 8 && id < 12) {
    states().lab.show_slot(id - 8);
    return;
  }
  if (id == 31 || id == 32) {  // SORT_CHASSISNAME, SORT_CHASSISMASS
    chassis.set_sort(id - 31);
    return;
  }
  if (id >= 23 && id <= 26) {  // SELECT_AMMO, _EQUIP, _WEAPON, _ARMOR: the tab, from the top
    static const int32_t kKind[] = {2, 3, 1, 3};
    unassigned.kind = kKind[id - 23];
    unassigned.armor_tab = id == 26 ? 1 : 0;
    unassigned.top = 0;
    unassigned.refill();
    return;
  }
  if (id >= 27 && id <= 30) {  // SORT_ITEM, SORT_QTY, SORT_MASS, SORT_CRIT
    static const int32_t kSort[] = {0, 2, 1, 3};
    unassigned.set_sort(kSort[id - 27]);
    return;
  }
  // 18..22 the pages' own (steps 6..8); 17 (CONFIG_EDIT's) and any other id
  // the exe ignores (its jump table covers 14..32).
  if (id >= 18 && id <= 22) e.unported("lab.press", {id});
}

// Mech3.exe 1.2 @ 0x0044ce30: ACCEPT takes the shown slot's mech back out of
// the lab's store and leaves (0x00448870); a page hides the old one's
// controls, shows its own and redraws (0x0044cf50).
void LabScreen::switch_page(int32_t p) {
  Engine& e = engine();
  LabState& lab = states().lab;
  if (p == 6) {
    const int32_t mech = e.lab_slot_mech(e.lab_slot(lab.pilot_, lab.slot_));
    e.lab_store_remove(lab.store_, mech);
    lab.accept();
    return;
  }
  show_page(page, false);
  page = p;
  show_page(p, true);
  refresh();
}

// Mech3.exe 1.2 @ 0x0044ce90
void LabScreen::show_slots(int32_t slot) {
  Engine& e = engine();
  LabState& lab = states().lab;
  for (int i = 0; i < 4; ++i) {
    const int32_t m = e.lab_slot(lab.pilot_, i) ? e.lab_slot_mech(e.lab_slot(lab.pilot_, i)) : -1;
    mechs[i].set_mech(m, false, nullptr);
  }
  const int32_t m = e.lab_slot(lab.pilot_, lab.slot_) ? e.lab_slot_mech(e.lab_slot(lab.pilot_, lab.slot_)) : -1;
  mechs[slot].set_mech(m, true, &pilot_glow);
  chassis.set_store(lab.store_);
  chassis.select_mech(m);
  refresh();
}

// Mech3.exe 1.2 @ 0x0044cf50: the shown slot read unchecked (one is always shown).
void LabScreen::refresh() {
  Engine& e = engine();
  const LabState& lab = states().lab;
  refresh(e.lab_slot_cfg(e.lab_slot(lab.pilot_, lab.slot_)));
}

// Mech3.exe 1.2 @ 0x0044cf70: what every page shows for a loadout (none:
// the slot's mech without one). CONFIG_LIST emptied, SKIN_LIST from the
// mech's skins with the player's (slot 0's) chosen, CURRENT_CHASSIS's second
// label, the data panels, the item lists, ARMOR_TITLES, ENGINE_TABLE and
// the groups (later steps' hooks); CRITICALS and WEAPON GROUPS only with a
// loadout, WEAPONS, EQUIPMENT and MISSION DATA hidden, ACCEPT only while
// every slot's loadout passes its check (0x004254d0).
void LabScreen::refresh(Handle config) {
  Engine& e = engine();
  const LabState& lab = states().lab;
  configs.clear_items();  // 0x0044e7b0, 0x004aa700
  configs.layout();
  const int32_t mech = config ? e.config_mech(config) : e.lab_slot_mech(e.lab_slot(lab.pilot_, lab.slot_));
  if (const Handle dfn = e.mech_dfn_handle(mech)) {
    std::vector<Engine::LabSkin> list;
    e.lab_dfn_skins(dfn, &list);
    const int32_t n = static_cast<int32_t>(list.size());
    int32_t skin = e.lab_slot(lab.pilot_, 0) ? e.lab_slot_skin(e.lab_slot(lab.pilot_, 0)) : 0;
    skins.clear_items();
    for (const auto& s : list)
      if (s.skin) skins.add_item(s.named ? s.name.c_str() : nullptr, static_cast<int32_t>(s.skin));
    if (skin > n) skin = n;  // (n itself: past the last)
    else if (skin < 0) skin = 0;
    skins.select(skin);
    if (config_edit.x38c == 0) skins.set_arrows(1, 1);
    configs.fill();
  }
  const Handle cfg = e.lab_slot_cfg(e.lab_slot(lab.pilot_, lab.slot_));
  ztwod::Text* line = current_chassis.labels.size() > 1 ? current_chassis.labels[1] : nullptr;
  if (cfg) {
    e.lab_config_recount(cfg);
    if (line) {
      // (0x00484700 and 0x00484720 asked and dropped)
      const std::string name = display_name(e, e.config_mech(cfg));
      const std::string cname = e.lab_config_name(cfg);
      if (e.lab_config_modified(cfg)) line->set_text_id(0xdcd, name.c_str(), cname.c_str());
      else line->set_text("%s: %s", name.c_str(), cname.c_str());
    }
    e.unported("0x00457940", {chassis_data, cfg});  // step 8
    e.unported("0x00457330", {mech_data, cfg});
    loadouts.call_fn("0x0044b230", {cfg});
    unassigned.x224 = 0;
    unassigned.store = lab.store_;
    unassigned.refill();
    assigned.x224 = cfg;
    assigned.store = cfg;
    assigned.refill();
    if (armor_titles) e.unported("0x00450e00", {armor_titles, cfg});
    engine_table.call_fn("0x0044b4b0", {cfg});
    refresh_groups();
  } else {
    if (line) line->set_text_plain(e.message(0x249));
    e.unported("0x00457940", {chassis_data, 0});
    e.unported("0x00457330", {mech_data, 0});
    loadouts.call_fn("0x0044b230", {0});
  }
  const int32_t has = cfg ? 1 : 0;
  tabs[4].enabled = has;  // WEAPON GROUPS
  tabs[4].refresh();
  tabs[3].enabled = has;  // CRITICALS
  tabs[3].refresh();
  tabs[1].hide();
  tabs[2].hide();
  tabs[5].hide();
  bool ok = true;
  for (int32_t i = 0; i < lab.slots_ && ok; ++i) {
    const Handle c = e.lab_slot_cfg(e.lab_slot(lab.pilot_, i));
    if (c) ok = e.lab_config_valid(c);
  }
  tabs[6].enabled = ok ? 1 : 0;  // ACCEPT
  tabs[6].refresh();
}

// Mech3.exe 1.2 @ 0x0044dca0
void LabScreen::edit_name(bool start, bool rename) {
  if (!start) {
    config_edit.finish(false);
    return;
  }
  skins.set_arrows(0, 0);
  config_edit.start(configs, rename);
}

// Mech3.exe 1.2 @ 0x00449e10
void LabScreen::default_config(Handle slot, Handle store) {
  Engine& e = engine();
  if (!slot || e.lab_slot_cfg(slot)) return;
  const int32_t mech = e.lab_slot_mech(slot);
  const Handle dfn = e.mech_dfn_handle(mech);
  if (!dfn) return;
  const std::vector<Handle> list = e.lab_list_configs(e.lab_dfn_configs(dfn));
  const LabState& lab = states().lab;
  Handle done = 0;
  for (size_t i = 0; i < list.size() && !done; ++i) {
    const Handle c = list[i];
    if (!c || e.lab_config_byte(c, 0x1e) || (lab.flag_ && e.lab_config_byte(c, 0x1c))) continue;
    if (!e.lab_store_has_config(store, mech, 0, c)) continue;
    e.lab_slot_set_config(slot, c, store);
    done = e.lab_slot_cfg(slot);
  }
  if (!done) e.lab_slot_set_config(slot, list.empty() ? 0 : list[0], store);
}

// Mech3.exe 1.2 @ 0x0044ca20
HelpButton* LabScreen::tab(int32_t p) { return p >= 0 && p < 7 ? &tabs[p] : nullptr; }

// Mech3.exe 1.2 @ 0x0044ca50: a page's controls shown or hidden. Every call
// shows the mech's data panel and CURRENT_CHASSIS; the glow goes to the
// page's tab (0x0044af90).
void LabScreen::show_page(int32_t p, bool show) {
  Engine& e = engine();
  // The shown slot's mech's tech (0x00484720), asked and dropped: only the
  // slot's lookups (twice) remain.
  const LabState& lab = states().lab;
  if (e.lab_slot(lab.pilot_, lab.slot_)) e.lab_slot(lab.pilot_, lab.slot_);
  e.vcall(mech_data, 0x60, {zgame::bits(-1.0f)});
  e.vcall(mech_data + 0x188, 0x98, {0, 0});
  current_chassis.show_for(-1.0f);
  uint32_t text = 0;
  if (p == 0) {
    if (show) {
      panel_left.show_look(2);
      panel_right.show_look(2);
    }
    if (show) chassis.show_for(-1.0f);
    else chassis.hide();
    if (show) configs.show_for(-1.0f);
    else configs.hide();
    if (show) e.vcall(chassis_data, 0x60, {zgame::bits(-1.0f)});
    else e.vcall(chassis_data, 0x64, {});
    if (show) loadouts.show_for(-1.0f);
    else loadouts.hide();
    const bool skins_on = show && e.vcall(e.field32(lab.pilot_, 0x6c), 0x2c, {}) != 3;
    if (skins_on) skins.show_for(-1.0f);
    else skins.hide();
    if (show) sort_name.show_for(-1.0f);
    else sort_name.hide();
    if (show) sort_mass.show_for(-1.0f);
    else sort_mass.hide();
    text = 0x18c;
  } else if (p == 3) {
    if (show) {
      panel_left.show_look(1);
      panel_right.show_look(1);
      const uint8_t on = lab.flag_ == 0 ? 1 : 0;
      e.vcall(mech_data + 0x188, 0x98, {on, on});
    }
    e.vcall(armor_titles, 0x64, {});
    if (show) assigned.show_for(-1.0f);
    else assigned.hide();
    if (show) part_head.show_for(-1.0f);
    else part_head.hide();
    for (LabPart& part : parts) {
      if (show) part.show_for(-1.0f);
      else part.hide();
    }
    if (show) unassigned.show_for(-1.0f);
    else unassigned.hide();
    if (!show) e.lab_part_pick(-1);
    text = 0x1aa;
  } else if (p == 4) {
    if (show) {
      panel_left.show_look(1);
      panel_right.show_look(1);
      refresh_groups();
    }
    if (show) ungrouped.show_for(-1.0f);
    else ungrouped.hide();
    if (show) grouped.show_for(-1.0f);
    else grouped.hide();
    text = 0x18d;
  }
  if (text) {
    ztwod::Text* first = current_chassis.labels.empty() ? nullptr : current_chassis.labels[0];
    first->set_text_plain(e.message(static_cast<int32_t>(text)));
  }
  HelpButton* t = tab(p);
  icon_glow.hide();
  if (t) lab_tab_glow(*t, show, &icon_glow);
}

// Mech3.exe 1.2 @ 0x0044a890
void MechButton::set_mech(int32_t m, bool sel, ztwod::Image* glow) {
  if (m != mech) {
    e_.mech_name_of(m);  // 0x00484650, its answer unused
    zimg::Texture* old = img_normal;
    img_normal = set_image(static_cast<zimg::Texture*>(nullptr));
    ztwod::host().image_release(old);
    ztwod::host().image_release(img_disabled);
    img_disabled = nullptr;
    ztwod::host().image_release(img_activate);
    img_activate = nullptr;
    img_normal = set_image(mech_icon(e_, m, 1));
    img_disabled = mech_icon(e_, m, 3);
    img_activate = mech_icon(e_, m, 0);
    mech = m;
    if (!(name.flags & ztwod::kHidden)) show_name(true);  // the name already showing
  }
  LabScreen::lab_tab_glow(*this, sel, glow);
}

// Mech3.exe 1.2 @ 0x0044af90: the page's tab chosen (no blink, its
// ACTIVATE image) with the glow placed at its GLOW_OFFSET, or not.
void LabScreen::lab_tab_glow(HelpButton& t, bool on, ztwod::Image* glow) {
  t.selected = on ? 1 : 0;
  if (on) {
    t.set_blink(false);
    if (glow) {
      glow->show_for(-1.0f);
      glow->set_pos(t.get_x() + t.glow[0], t.get_y() + t.glow[1]);
    }
    t.set_image(t.img_activate);
    return;
  }
  t.set_image(t.img_normal);
}

}  // namespace mech3
