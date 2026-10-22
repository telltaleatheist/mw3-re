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
#include "mech3/app_states.h"
#include "mech3/pilot.h"

namespace mech3 {

// mech_names.h (its Engine clashes with the app's): 0x00484670's message and 0x004846e0.
int32_t mech_name_message(int32_t mech);
float max_mass(int32_t mech);

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

bool LabLaterList::read_control(zrdr::Node*, ztwod::Dialog*) {
  call(0x84);
  return true;
}
void LabLaterList::after_bind() { call(0x88); }
void LabLaterList::show_for(float seconds) { call(0x60, {zgame::bits(seconds)}); }
void LabLaterList::hide() { call(0x64); }

bool LabWeaponList::read_control(zrdr::Node*, ztwod::Dialog*) {
  call(0x84);
  return true;
}
void LabWeaponList::after_bind() { call(0x88); }
void LabWeaponList::show_for(float seconds) { call(0x60, {zgame::bits(seconds)}); }
void LabWeaponList::hide() { call(0x64); }

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
      ungrouped(e),
      grouped(e),
      engine_table(e) {
  const struct {
    LabLater* l;
    uint32_t at;
  } later[] = {{&loadouts, 0x11508}, {&unassigned, 0x19290},
               {&assigned, 0x1a13c}, {&ungrouped, 0x1a36c}, {&grouped, 0x1a570}, {&engine_table, 0x1a8c8}};
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
  // 18..30 the pages' own (steps 5..8); 17 (CONFIG_EDIT's) and any other id
  // the exe ignores (its jump table covers 14..32).
  if (id >= 18 && id <= 30) e.unported("lab.press", {id});
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
    e.lab_groups_refresh();
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
      e.lab_groups_refresh();
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
