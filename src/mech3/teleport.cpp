#include "mech3/teleport.h"

#include <cstdio>
#include <cstring>

#include "crt/msvc_string.h"
#include "gamez/zreader/zreader.h"

namespace mech3 {

namespace {
TeleportHooks g_default;
TeleportHooks* g_hooks = &g_default;

constexpr const char* kCurrent = "Current Location";  // 0x005e0428
constexpr int32_t kCaption = 0x1dd, kNoName = 0x1de, kDeleteIt = 0x1df;

float number(const zrdr::Node& n) { return n.type == zrdr::Float ? n.f : n.type == zrdr::Int ? static_cast<float>(n.i) : 0.0f; }

// CB_FINDSTRINGEXACT from the start: the item equal to `s` in any case, -1 none.
int32_t find_exact(const DlgCombo& c, const std::string& s) {
  for (size_t i = 0; i < c.items.size(); ++i)
    if (crt::msvc_stricmp(c.items[i].text.c_str(), s.c_str()) == 0) return static_cast<int32_t>(i);
  return -1;
}
}  // namespace

TeleportHooks& teleport_hooks() { return *g_hooks; }
void set_teleport_hooks(TeleportHooks* h) { g_hooks = h ? h : &g_default; }
bool teleport_hooks_default() { return g_hooks == &g_default; }

// Mech3.exe 1.2 @ 0x004a37a0 (the list): the player's place first, unsaved,
// yaw 0; then each nav point, yaw 0; then location.zrd's pairs, each one's
// place and yaw by its name (0x005569e0: the first of that name).
std::vector<TeleportSpot> teleport_spots(const float player[3], const zrdr::Node* location) {
  std::vector<TeleportSpot> out;
  TeleportSpot cur;
  for (int k = 0; k < 3; ++k) cur.pos[k] = player[k];
  cur.name = kCurrent, cur.unsaved = 1;
  out.push_back(cur);
  for (TeleportSpot v : teleport_hooks().navs()) {
    v.yaw = 0, v.unsaved = 0;
    out.push_back(v);
  }
  if (!location || location->type != zrdr::List || zrdr::list_count(location) < 2) return out;
  const zrdr::Node& pairs = location->list[1];
  if (pairs.type != zrdr::List) return out;
  for (int32_t i = 1; i + 1 < zrdr::list_count(&pairs); i += 2) {
    if (pairs.list[i].type != zrdr::String || !pairs.list[i].s) continue;
    const zrdr::Node* v = zrdr::find(const_cast<zrdr::Node*>(location), pairs.list[i].s);
    // FIX (bug): unchecked in the exe, which reads a value of another shape as
    // ((x y z) yaw) and a short one past its end; such a pair is left out.
    if (!v || v->type != zrdr::List || zrdr::list_count(v) < 3) continue;
    const zrdr::Node& xyz = v->list[1];
    if (xyz.type != zrdr::List || zrdr::list_count(&xyz) < 4) continue;
    TeleportSpot s;
    for (int k = 0; k < 3; ++k) s.pos[k] = number(xyz.list[1 + k]);
    s.yaw = number(v->list[2]);
    s.name = std::string(pairs.list[i].s).substr(0, 59);
    out.push_back(s);
  }
  return out;
}

// Mech3.exe 1.2 @ 0x004a37a0 (the save): its header, then every saved spot.
std::string location_zrd_text(const std::vector<TeleportSpot>& spots) {
  std::string t = "(\n";
  t += "  ;; LOCATION.ZRD stores teleport data for this mission.\n";
  t += "  ;; Last updated Sep 20 1999 at 16:29:38.\n\n";  // the build's __DATE__ and __TIME__
  char line[256];
  for (const TeleportSpot& s : spots) {
    if (s.unsaved) continue;
    const std::string quoted = "\"" + s.name + "\"";
    std::snprintf(line, sizeof line, "%s ( ( %8.1f %8.1f %8.1f ) %8.1f )  ;; position/yaw\n", quoted.c_str(),
                  static_cast<double>(s.pos[0]), static_cast<double>(s.pos[1]), static_cast<double>(s.pos[2]),
                  static_cast<double>(s.yaw));
    t += line;
  }
  return t + ")\n";
}

// Mech3.exe 1.2 @ 0x00404630
TeleportDialog::TeleportDialog() { dlg.at(kList).combo.sorted = true; }

// Mech3.exe 1.2 @ 0x00404720
void TeleportDialog::exchange(DataExchange& dx) {
  ddx_text(dx, kX, x);
  ddx_text(dx, kY, y);
  ddx_text(dx, kZ, z);
  ddx_text(dx, kYaw, yaw);
  ddv_min_max_float(dx, yaw, -360.0f, 360.0f);
}

// CWnd::UpdateData. Writing the fields sends each changed edit's EN_CHANGE
// (MFC's AfxSetWindowText sets a text only when it differs), as Windows does.
bool TeleportDialog::update(bool save) {
  std::string was[4];
  const int32_t ids[4] = {kX, kY, kZ, kYaw};
  for (int k = 0; k < 4; ++k) was[k] = dlg.at(ids[k]).text;
  const bool ok = update_data(dlg, save, [&](DataExchange& dx) { exchange(dx); });
  if (!save)
    for (int k = 0; k < 4; ++k)
      if (dlg.at(ids[k]).text != was[k]) changed_text();
  return ok;
}

// Mech3.exe 1.2 @ 0x004047b0 (after CDialog::OnInitDialog's UpdateData(FALSE)):
// each spot's name in the list, its data the spot; "Current Location" picked.
void TeleportDialog::init() {
  update(false);
  DlgCombo& c = dlg.at(kList).combo;
  for (size_t i = 0; spots && i < spots->size(); ++i) {
    const int32_t at = c.add((*spots)[i].name);
    if (at != -1) c.set_data(at, static_cast<int32_t>(i));
  }
  c.set_cur(find_exact(c, kCurrent));
  dlg.at(kList).text = c.cur >= 0 ? c.items[size_t(c.cur)].text : std::string();
}

// Mech3.exe 1.2 @ 0x004049b0: the picked spot's place and yaw in the fields.
void TeleportDialog::pick() {
  const DlgCombo& c = dlg.at(kList).combo;
  if (c.cur < 0 || !spots) return;
  const int32_t i = c.data(c.cur);
  if (i < 0 || static_cast<size_t>(i) >= spots->size()) return;
  const TeleportSpot& s = (*spots)[size_t(i)];
  x = s.pos[0], y = s.pos[1], z = s.pos[2], yaw = s.yaw;
  dlg.at(kList).text = c.items[size_t(c.cur)].text;
  update(false);
}

// Mech3.exe 1.2 @ 0x00404a20: after Yes to "delete <the list's text>?", the
// picked spot unsaved and out of the list.
void TeleportDialog::remove() {
  const std::string text = dlg.at(kList).text.substr(0, 79);
  if (teleport_hooks().message_box(4, kCaption, kDeleteIt, text) == 6) remove_picked();
}

void TeleportDialog::remove_picked() {
  DlgCombo& c = dlg.at(kList).combo;
  // FIX (bug): with nothing picked the exe writes through CB_GETITEMDATA's
  // CB_ERR (-1) at 0x4b, a crash; nothing is deleted then.
  if (c.cur < 0 || !spots) return;
  const int32_t i = c.data(c.cur);
  if (i >= 0 && static_cast<size_t>(i) < spots->size()) (*spots)[size_t(i)].unsaved = 1;
  c.items.erase(c.items.begin() + c.cur);  // CB_DELETESTRING: the selection gone with it
  c.cur = -1;
}

// Mech3.exe 1.2 @ 0x00404860: a name typed into the list that it does not
// hold (any case) added as a spot at the fields' place; an empty one refused.
void TeleportDialog::left_list() {
  const std::string text = dlg.at(kList).text.substr(0, 78);
  if (text.empty()) {
    teleport_hooks().message_box(0x30, kCaption, kNoName, "");
    return;
  }
  DlgCombo& c = dlg.at(kList).combo;
  if (find_exact(c, text) != -1 || !spots) return;
  const int32_t at = c.add(text);
  update(true);  // its result unread: a bad number leaves that field as it was
  TeleportSpot s;
  s.pos[0] = x, s.pos[1] = y, s.pos[2] = z, s.yaw = yaw;
  s.name = text.substr(0, 59);
  spots->push_back(s);
  if (at != -1) c.set_data(at, static_cast<int32_t>(spots->size() - 1));
}

// Mech3.exe 1.2 @ 0x00404ab0
void TeleportDialog::changed_text() { changed = 1; }

// Mech3.exe 1.2 @ 0x0040dfe0 (to DoModal): the fields at the player's place
// and yaw, in degrees (57.29578).
void teleport_dialog_begin(TeleportDialog& d, std::vector<TeleportSpot>* spots, const float pos[3], float yaw) {
  d = TeleportDialog{};
  d.spots = spots;
  d.x = pos[0], d.y = pos[1], d.z = pos[2];
  d.yaw = yaw * 57.29578f;
  d.changed = 0;
  d.init();
}

// Mech3.exe 1.2 @ 0x0040dfe0 (from DoModal's IDOK: CDialog::OnOK's
// UpdateData(TRUE) first, which keeps the dialog up when a field fails).
// The exe takes any non-zero result, IDCANCEL too (0x0040e05f); ours calls
// this on OK only (upgrades/editor/teleport.cpp, FIX (bug)).
bool teleport_dialog_ok(TeleportDialog& d, float pos[3], float* yaw, int32_t* changed) {
  if (!d.update(true)) return false;
  pos[0] = d.x, pos[1] = d.y, pos[2] = d.z;
  *yaw = d.yaw * 0.017453292f;
  *changed = d.changed;
  return true;
}

}  // namespace mech3
