#include "mech3/game_editor_net.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include "crt/msvc_string.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zreader/zreader.h"
#include "gamez/znetwork/znet_object.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/net_save.h"
#include "mech3/zainet.h"
#include "mech3/game_editor_tree.h"
#include "mech3/zainetedit.h"
#include "upgrades/editor/add_vehicle.h"
#include "upgrades/editor/form.h"
#include "upgrades/editor/help.h"
#include "upgrades/editor/node_params.h"

namespace mech3 {

namespace {
NetDialog g_dialog;
NetEditor* g_editor = nullptr;  // [0x005f4c6c]
// [0x005f4c60]'s list after the main editor's slot (0x00404140 puts it
// first): the other slots in the order they were made (0x004043c0).
std::vector<NetEditor**> g_slots;
EditorForm g_dialog_form;  // FIX (A): the dialog as a form (form.h), drawn by ImGui
std::string g_status;

void not_ported(const char* what) { editor_hooks().not_ported(what); }
void none(int32_t) {}  // 0x004bf620

// 0x005e0f18.
const EditorEntry kNetRows[13] = {
    {"Pick CMD or Point", none},   {" GOTO Net", net_editor_goto},        {" Change Net Params", net_params_open},
    {" Node Params", node_params_open}, {" Add veh to node", add_vehicle_open},     {" Move Node", net_editor_move},
    {" Delete Node", net_editor_delete}, {" Delete Net", none},                  {" Save Changes", net_editor_save},
    {" Tree View", tree_view},     {" Help", help_open},                   {"", nullptr},  // Help: ours
    {" Exit Net Editor", net_editor_exit},
};
const EditorMenu kNetMenu = {13, kNetRows};

int32_t row_of(int32_t y) { return (y - 0x5f) / 12; }  // the panel's rows, 12 apart from 0x5f

// The dialog (template 139) as a form: the label, the list (selecting sets
// +0x64), OK and Cancel, which both open the selection, as the exe's do.
void build_dialog() {
  g_dialog_form = EditorForm{};
  g_dialog_form.title = "AI Network Editor";
  FormField label{"Select a network to edit:", FormField::kLabel};
  FormField list{"##nets", FormField::kChoice};
  list.i = &g_dialog.selected;
  for (const auto& it : g_dialog.items) list.choices.push_back(it.first);
  g_dialog_form.fields = {label, list};
  g_dialog_form.buttons = {{"OK", net_dialog_close}, {"Cancel", net_dialog_close}};
}

// The view's calls into the AI net's nodes (vtable 0x005cae60; the
// placeholder's 0x005ca388 has the same slots for these).
ai::NetNode* as_node(znet::Element* e) { return e ? static_cast<ai::AiElem*>(e)->as_node() : nullptr; }
struct NodeHooks : znet::ViewHooks {
  int32_t link_count(znet::Element* e) override { return as_node(e)->link_count(); }          // 0x004c7240
  znet::Element* link(znet::Element* e, int32_t k) override { return as_node(e)->link_node(k); }  // 0x004c71f0
  int32_t link_index(znet::Element* e, znet::Element* to) override { return as_node(e)->link_index(as_node(to)); }
  bool placed(znet::Element* e) override { return as_node(e)->rec.kind == 0; }  // 0x004c72c0
  const float* pos(znet::Element* e) override { return as_node(e)->pos(); }      // 0x004c9170
  // 0x004d5170: from `to`'s place to e's.
  bool sees(znet::Element* e, znet::Element* to) override {
    return editor_hooks().clear_line(as_node(to)->pos(), as_node(e)->pos());
  }
};
NodeHooks g_node_hooks;

constexpr double kSteep = 1.2217304763958;  // [0x005bb938]: 70 degrees down
constexpr double kFarOver = 1.5;            // [0x005bb940]
constexpr float kEdge = 0.9375f;            // [0x005bb930]: off the screen's size for the clip's far side

// The net file the edit read, its archive (the first open one holding it, as
// the reader finds it) and its bytes.
bool net_entry(ai::CZAiNetEdit& n, std::string* archive, std::string* entry, std::vector<uint8_t>* bytes) {
  ai::NetFile* f = n.ref ? n.ref->file : nullptr;
  if (!f || !f->name) return false;
  *entry = f->name;
  for (zutl_zar::Archive* a : zrdr::state().archives) {
    const int i = a ? zutl_zar::find(*a, entry->c_str()) : -1;
    if (i < 0 || !a->path) continue;
    uint32_t size = a->entries[i].length;
    bytes->assign(size, 0);
    if (zutl_zar::read(*a, i, bytes->data(), &size) != 0) return false;
    bytes->resize(size);
    *archive = a->path;
    return true;
  }
  return false;
}

// Ours, for a new net (in no archive yet): the first open archive, in the
// reader's order, that holds a net_NN.zrd, where the mission's nets are read
// from; its path, "" when none.
std::string nets_archive() {
  for (zutl_zar::Archive* a : zrdr::state().archives) {
    if (!a || !a->path) continue;
    for (int i = 0; i < a->count; ++i) {
      char n[sizeof a->entries[i].name + 1] = {};
      std::memcpy(n, a->entries[i].name, sizeof a->entries[i].name);
      if (std::strlen(n) != 10 || crt::msvc_stricmp(n + 6, ".zrd") != 0) continue;
      n[4] = 0;
      if (crt::msvc_stricmp(n, "net_") == 0) return a->path;
    }
  }
  return {};
}
}  // namespace

NetDialog& net_dialog() { return g_dialog; }
NetEditor*& net_editor() { return g_editor; }
const EditorMenu& editor_net_menu_rows() { return kNetMenu; }
EditorForm& net_dialog_form() { return g_dialog_form; }

// Mech3.exe 1.2 @ 0x00402f40: the selection base (0x0058bf50) on the net
// (the world, its other argument, unread), the camera's far clip kept
// (0x004f3b00's second), then 0x004f2490 / 0x004f23e0 put back what they read.
NetEditor::NetEditor(ai::CZAiNetEdit* n, int32_t d) : net(n), dirty(d) {
  view = new znet::View(n, &g_node_hooks);
  float near_z = 0.0f;
  editor_hooks().camera_clip(&near_z, &far_clip);
}

// Mech3.exe 1.2 @ 0x00402fd0: the far clip back when it was kept, the net
// deleted (its vt+0x00(1)), the selection base's dtor (0x0058c640).
NetEditor::~NetEditor() {
  if (far_clip > 0.0f) {
    float near_z = 0.0f, far_z = 0.0f;
    editor_hooks().camera_clip(&near_z, &far_z);
    editor_hooks().set_camera_clip(near_z, far_clip);
  }
  delete net;
  delete view;
}

// Mech3.exe 1.2 @ 0x00403160: the line clip on the screen and the camera's
// depth, the view vehicle's pick; looking down 70 degrees or more the far
// clip pulled in to 1.5 times the camera's distance to the pick (its x, y
// and z apart, added), back to the kept one otherwise; Move Node's states;
// the view (0x0058c950); then any marks to link (the view's +0xfc, a plain
// left-up's picks) each linked to the first pick and the editor made anew.
// With pixels doubled (0x0040d4b0) the exe sets the projection on the half
// window around the view and back after; the port has no doubling
// (options.cpp), so not here.
void NetEditor::frame(float dt) {
  (void)dt;  // 0x0058c950's first argument, unread
  EditorHooks& h = editor_hooks();
  h.view_refresh();  // 0x004b51f0
  zmth::Mat34 m;
  zmth::push(&m);  // 0x0051e9d0
  int32_t w = 0, ht = 0;
  h.screen_size(&w, &ht);  // 0x00558080
  float mn[3] = {0.0f, 0.0f, 0.0f};
  float mx[3] = {static_cast<float>(w) - kEdge, static_cast<float>(ht) - kEdge, 0.0f};
  h.camera_clip(&mn[2], &mx[2]);
  ztwod::set_line_clip(mn, mx);  // 0x0059b2f0
  camera = h.camera_pos();
  zmth::load_view();  // 0x0051ea30
  has_ground = h.ground_pick(ground) ? 1 : 0;  // vt+0xf8 gives back the buffer it filled
  const float* at = has_ground ? ground : nullptr;
  if (-static_cast<double>(h.camera_pitch()) < kSteep) {
    if (0.0f < far_clip) {
      float near_z = 0.0f, far_z = 0.0f;
      h.camera_clip(&near_z, &far_z);
      h.set_camera_clip(near_z, far_clip);  // 0x004f3a70
    }
  } else if (camera && at) {
    const float d[3] = {camera[0] - at[0], camera[1] - at[1], camera[2] - at[2]};
    const float far_z = static_cast<float>((std::fabs(d[0]) + std::fabs(d[1]) + std::fabs(d[2])) * kFarOver);
    float near_z = 0.0f, old = 0.0f;
    h.camera_clip(&near_z, &old);
    h.set_camera_clip(near_z, far_z);
  }
  if (move == 1) {
    if (!(moving->flags & 1)) move = 2;
  } else if (move == 2) {
    if (moving->flags & 1) {
      moving->flags &= ~6u;
      ++move;
    }
  } else if (move == 3) {
    if ((moving->flags & 3) == 1) {
      if (has_ground) {
        ai::NetNode* n = as_node(moving->elem);  // vt+0x5c
        if (n->set_pos(ground)) n->relink();    // vt+0x2c, then vt+0x38
        dirty = 1;
      }
    } else {
      move = 0;
    }
  }
  view->frame(at);
  if (!view->to_link.empty()) {
    ai::NetNode* target = nullptr;
    if (view->picks.empty()) {  // 0x004bd1f0: a new node at the pick
      target = net->add_node(nullptr);
      if (target) {
        if (target->set_pos(ground)) target->relink();
        dirty = 1;
      }
    } else {
      target = as_node(view->picks.front()->elem);
    }
    if (target) {
      for (znet::Mark* k : view->to_link)
        if (k->elem != target && as_node(k->elem)->add_link(target)) dirty = 1;  // vt+0x5c, vt+0x30
      rebuild = 1;
    }
    view->to_link.clear();
  }
  zmth::pop();  // 0x0051ea00
}

void net_editor_free() {
  delete g_editor;
  g_editor = nullptr;
}

// Mech3.exe 1.2 @ 0x00402d60's dialog (0x0040e120 -> 0x004a4c20; its init
// 0x004a4cd0): every net of znet's registry by its name, sorted (CBS_SORT,
// a stable sort as the list's insertion), "New Net" at the top, selected.
void edit_ai_nets(int32_t) {
  g_dialog = NetDialog{};
  for (znet::Object* o : znet::registry().list)
    if (auto* n = dynamic_cast<ai::AiNet*>(o)) {  // vt+0x28 (every object of the registry is a net)
      const char* name = n->name();             // vt+0x4c
      g_dialog.items.emplace_back(name ? name : "", n);
    }
  std::stable_sort(g_dialog.items.begin(), g_dialog.items.end(), [](const auto& a, const auto& b) {
    return crt::msvc_stricmp(a.first.c_str(), b.first.c_str()) < 0;  // the list's order, letter case aside
  });
  g_dialog.items.insert(g_dialog.items.begin(), {"New Net", nullptr});  // 0x005e8340, CB_INSERTSTRING 0
  g_dialog.open = true;
  build_dialog();
  form_show(&g_dialog_form);
}

// The rest of 0x00402d60: a net that is not an edit (vt+0x48) edited through
// a new one of its id (0x004bd360: CZAiNetEdit(id, none, 0)); then the old
// editor freed, the new one kept and its menu installed; New Net, a new net
// with its first node at the pick (0x00402c90).
void net_dialog_close() {
  g_dialog.open = false;
  form_hide(&g_dialog_form);
  ai::AiNet* pick = g_dialog.selected >= 0 && g_dialog.selected < static_cast<int32_t>(g_dialog.items.size())
                        ? g_dialog.items[size_t(g_dialog.selected)].second
                        : nullptr;
  NetEditor* e = nullptr;
  if (!pick) {
    // New Net: at the view vehicle's pick (vt+0xf8), numbered by the first
    // free id from 1 (0x0058b1c0(1, 1, 100)).
    float at[3] = {};
    const int32_t id = editor_hooks().ground_pick(at) ? znet::alloc_id(1, 1, 100) : -1;
    if (id >= 0) e = net_editor_new(id, at);
    if (!e) {
      editor_main_menu();
      return;
    }
  } else {
    auto* edit = dynamic_cast<ai::CZAiNetEdit*>(pick);
    if (!edit) edit = new ai::CZAiNetEdit(pick->get_id(), nullptr, 0);  // 0x004bd360 (vt+0x18 the id)
    // The editor owns it either way: 0x00402fd0 deletes it, and a net leaves
    // the registry as it goes.
    e = new NetEditor(edit, 0);
  }
  net_editor_free();
  g_editor = e;
  net_editor_menu();
}

// Mech3.exe 1.2 @ 0x00403690: 0x0040e7b0(1, 0x005e0f18) (the title "AI NET
// EDITOR" and the net's name, vt+0x4c, passed and unread).
void net_editor_menu() { editor_install(1, kNetMenu); }

// Mech3.exe 1.2 @ 0x004036c0: the view's vehicle ([0x005f4c8c]+0x64, the
// stealth object while editing) put at the net's place (vt+0x04 0x00402b20:
// its first node's, vt+0x0c(0) then vt+0x18) by its vt+0xdc.
void net_editor_goto(int32_t) {
  if (!g_editor || !g_editor->net) return;
  ai::NetNode* first = g_editor->net->node(0);
  const float* at = first ? first->pos() : nullptr;
  if (at) editor_hooks().view_vehicle_place(at);
}

// Mech3.exe 1.2 @ 0x00402e90 -> 0x00402f20 -> 0x004bd340. The release's
// save goes no further than 0x004bd100's fold of the edits into the file's
// record cache and the stub 0x004d72a0; FIX (A), ours: the mission's archive
// written with the cache's records as the editor mod's copy (net_save.h),
// its folder said on screen.
void net_editor_save(int32_t) {
  if (!g_editor) {
    editor_status("no network to save");  // 0x00402ec0(0x30, ...): a box in the exe
    return;
  }
  if (ai::CZAiNetEdit* n = g_editor->net) {
    std::string archive, entry;
    std::vector<uint8_t> bytes;
    // A new net (New Net: its file read nothing) goes in as a new entry of
    // the archive the mission's nets come from; the game finds nets by
    // number (0x00402170: net_01 .. net_99), so it loads from the next start.
    const bool fresh = !net_entry(*n, &archive, &entry, &bytes) && n->ref && n->ref->file &&
                       !n->ref->file->tree && n->ref->file->name;
    if (fresh) {
      archive = nets_archive();
      entry = n->ref->file->name;
      bytes = ai::net_new_entry(ai::net_name(n->header.net));
    }
    if (archive.empty()) {
      editor_status("not saved: the net's file is in no open archive");
    } else {
      n->fold_edits();  // 0x004bd100
      std::vector<std::pair<std::string, const ai::NetNodeRec*>> edits;
      for (const auto& [key, rec] : n->ref->file->cache) edits.emplace_back(key, rec);
      const std::vector<uint8_t> out = ai::net_with_edits(bytes.data(), bytes.size(), n->ref->file->tree, edits);
      const ai::NetSave s = out.empty() ? ai::NetSave{false, "", "", "", entry + " does not read as a net"}
                                        : ai::save_archive_edit(archive.c_str(), entry.c_str(), out, fresh);
      // Ours (decided 2026-11-07, docs/editor.md, The reload): the release's
      // reload (0x004bed90) acts only on version 105, which no file is; here a
      // saved net 106 that is running is built again from the edit (0x004be670)
      // and the vehicles on it go over to the new one (net_hooks().rebuilt).
      bool rebuilt = false;
      if (s.ok && n->header.version == 0x6a && n->net && n->net->as_compiled()) rebuilt = n->build_compiled() != nullptr;
      editor_status(s.ok ? s.message + (rebuilt ? "; running now" : "") : "not saved: " + s.error);
    }
  }
  g_editor->dirty = 0;
}

// Mech3.exe 1.2 @ 0x00403770: the first pick, said to be the one used when
// there are more (a box in the exe).
znet::Mark* net_editor_first_pick() {
  if (!g_editor || g_editor->view->picks.empty()) return nullptr;
  const size_t n = g_editor->view->picks.size();
  if (n > 1) editor_status("Using first of " + std::to_string(n) + " selected nodes");  // 0x005e0330
  return g_editor->view->picks.front();
}

// Mech3.exe 1.2 @ 0x00403ac0 -> 0x00403a90: the first pick follows the
// pick point from the frame after the mouse has left it and come back,
// until it is clicked or the mouse leaves it.
void net_editor_move(int32_t) {
  if (!g_editor) return;
  znet::Mark* m = net_editor_first_pick();
  if (!m) {
    editor_status("Select a node first");  // 0x005e03c0, a box in the exe
    return;
  }
  g_editor->moving = m;
  g_editor->move = 1;
}

// Mech3.exe 1.2 @ 0x00402c90: a new net numbered `id` (0x004bd360:
// CZAiNetEdit(id, none, 0)), its first node (vt+0x50(0)) at `at`, an editor
// on it, the node's mark pressed (0x0058ebc0) so it starts picked.
NetEditor* net_editor_new(int32_t id, const float at[3]) {
  auto* edit = new ai::CZAiNetEdit(id, nullptr, 0);
  ai::NetNode* n = edit->add_node(nullptr);
  if (!n) {
    delete edit;
    return nullptr;
  }
  if (n->set_pos(at)) n->relink();  // vt+0x2c, vt+0x38
  NetEditor* e = new NetEditor(edit, 0);
  e->view->press(edit->node(0));  // vt+0x0c(0)
  return e;
}

namespace {
// Mech3.exe 1.2 @ 0x004037b0: the first picked link's ends, from and to (a
// cursor line's the other way round), said to be the one used when there are
// more (a box in the exe).
bool first_picked_link(znet::Mark** a, znet::Mark** b) {
  std::list<znet::Link*>& l = g_editor->view->picked_links;
  if (l.empty()) return false;
  if (l.size() > 1) editor_status("Using first of " + std::to_string(l.size()) + " selected lines");  // 0x005e0354
  const znet::Link* k = l.front();
  if (k->kind == 2) *a = k->to, *b = k->from;
  else if (k->kind == 1 || k->kind == 3) *a = k->from, *b = k->to;
  else return false;
  return true;
}
}  // namespace

namespace {
EditorForm g_confirm;  // FIX (A): the exe's OK/Cancel box (0x00402ec0(0x21, ...)) as a modal form

// What Delete Node takes now: the first pick, or else the first picked link's ends.
const void* delete_target(znet::Mark** m, znet::Mark** a, znet::Mark** b) {
  *m = net_editor_first_pick();
  *a = *b = nullptr;
  if (!*m && !first_picked_link(a, b)) return nullptr;
  return *m ? static_cast<const void*>(*m) : static_cast<const void*>(g_editor->view->picked_links.front());
}
// OK: the asked-about node or line deleted, when it is still the one picked.
void delete_confirmed() {
  form_hide(&g_confirm);
  if (!g_editor || !g_editor->net || !g_editor->armed) return;
  znet::Mark *m, *a, *b;
  const void* what = delete_target(&m, &a, &b);
  const bool same = what && what == g_editor->armed;
  g_editor->armed = nullptr;
  if (!same) return;
  g_editor->dirty = 1;
  ai::CZAiNetEdit* net = g_editor->net;
  const bool done = m ? net->delete_node(as_node(m->elem)) : net->delete_link(as_node(a->elem), as_node(b->elem));
  g_editor->rebuild = done ? 1 : 0;
}
void delete_cancelled() {
  form_hide(&g_confirm);
  if (g_editor) g_editor->armed = nullptr;
}
}  // namespace

EditorForm& net_delete_form() { return g_confirm; }

// Mech3.exe 1.2 @ 0x00403ae0: the first pick deleted, or else the first
// picked link's line; the editor made anew when it was. The exe asks first
// with an OK/Cancel box (0x00402ec0(0x21, ...)); FIX (A): here a modal form
// with the same question and buttons.
void net_editor_delete(int32_t) {
  if (!g_editor) return;
  ai::CZAiNetEdit* net = g_editor->net;  // vt+0x48
  if (!net) {
    editor_status("Select a node or line first");  // 0x005e03dc
    return;
  }
  znet::Mark *m, *a, *b;
  const void* what = delete_target(&m, &a, &b);
  if (!what) return;
  g_editor->armed = what;
  g_confirm = EditorForm{};
  g_confirm.title = "Delete Node";
  g_confirm.modal = true;
  g_confirm.fields = {{m ? "Confirm node deletion" : "Confirm line deletion", FormField::kLabel}};  // 0x005e0410 / 0x005e03f8
  g_confirm.buttons = {{"OK", delete_confirmed}, {"Cancel", delete_cancelled}};
  form_show(&g_confirm);
}

// Mech3.exe 1.2 @ 0x00403080 (ECX the editor's slot)
void net_editor_step_slot(NetEditor*& slot, float dt) {
  for (;;) {
    if (slot && slot->rebuild) {
      ai::CZAiNetEdit* n = slot->net;
      const int32_t d = slot->dirty;
      slot->net = nullptr;
      delete slot;
      slot = new NetEditor(n, d);
    }
    if (!slot) return;
    slot->frame(dt);
    if (!slot->rebuild) return;
  }
}
void net_editor_step(float dt) { net_editor_step_slot(g_editor, dt); }

void EditorHooks::net_step(float dt) { net_editor_step(dt); }  // 0x00403140

// ---- the Ai Network Editor window (template 181) ----

namespace {
NetWindow* g_window = nullptr;  // [0x0060b31c]
std::vector<std::string> g_open_names;  // Open's list: the open archives' net files
int32_t g_open_pick = 0;
EditorForm g_window_form;  // FIX (A): the window (template 181) as a form, drawn by ImGui
EditorForm g_open_form;    // FIX (A): Open's CFileDialog as a form

void window_build();
void window_close() {  // OK (0x004d5610), Cancel (0x004d55f0): DestroyWindow, then 0x004bf6f0
  net_window_free();
}
void open_close() {
  form_hide(&g_open_form);
  if (g_window) g_window->choosing = false;
}
// Open's file: opened (0x004bf640 -> 0x00404570).
void open_take() {
  if (g_window && g_open_pick >= 0 && g_open_pick < static_cast<int32_t>(g_open_names.size()))
    net_window_open(g_open_names[size_t(g_open_pick)].c_str());
  open_close();
  window_build();
}
// Open's list (FIX (A) for the CFileDialog "Zipper Files (*.zrd)", 0x004bf750):
// every net file the open archives hold, by name, each once.
void open_list() {
  g_open_names.clear();
  for (zutl_zar::Archive* a : zrdr::state().archives) {
    if (!a) continue;
    for (int i = 0; i < a->count; ++i) {
      char n[sizeof a->entries[i].name + 1] = {};
      std::memcpy(n, a->entries[i].name, sizeof a->entries[i].name);
      const size_t l = std::strlen(n);
      if (l < 5 || crt::msvc_stricmp(n + l - 4, ".zrd") != 0) continue;
      char head[5] = {n[0], n[1], n[2], n[3], 0};
      if (crt::msvc_stricmp(head, "net_") != 0 && crt::msvc_stricmp(head, "reg_") != 0) continue;
      if (std::find(g_open_names.begin(), g_open_names.end(), n) == g_open_names.end()) g_open_names.push_back(n);
    }
  }
  std::sort(g_open_names.begin(), g_open_names.end());
  if (!g_window) return;
  g_window->choosing = true;
  g_open_pick = 0;
  g_open_form = EditorForm{};
  g_open_form.title = "Open a net file";
  FormField list{"##files", FormField::kChoice};
  list.i = &g_open_pick, list.choices = g_open_names;
  g_open_form.fields = {list};
  g_open_form.buttons = {{"Open", open_take}, {"Cancel", open_close}};
  form_show(&g_open_form);
}
void window_build() {
  g_window_form = EditorForm{};
  g_window_form.title = "Ai Network Editor";
  const NetEditor* h = g_window ? g_window->hosted : nullptr;
  if (h && h->net && h->net->ref && h->net->ref->file && h->net->ref->file->name)
    g_window_form.fields.push_back({std::string("showing ") + h->net->ref->file->name, FormField::kLabel});  // ours
  g_window_form.buttons = {{"OK", window_close}, {"Cancel", window_close}, {"Open", open_list}};
  form_show(&g_window_form);
}
}  // namespace

EditorForm& net_window_form() { return g_window_form; }
EditorForm& net_open_form() { return g_open_form; }

NetWindow*& net_window() { return g_window; }

// Mech3.exe 1.2 @ 0x00404570 (on the window's slot, +0x64): the editor there
// freed (0x00402fd0); for a name, a net read from it (0x004bd3d0:
// CZAiNetEdit(-1, name, 0)) and an editor on it.
void net_window_open(const char* name) {
  if (!g_window) return;
  delete g_window->hosted;
  g_window->hosted = nullptr;
  if (name) g_window->hosted = new NetEditor(new ai::CZAiNetEdit(-1, name, 0), 0);
}

// Mech3.exe 1.2 @ 0x00404420 (the window's dtor, 0x004bf5d0): its editor
// freed (0x00404490(0)) and off the frame's list.
void net_window_free() {
  form_hide(&g_window_form);
  form_hide(&g_open_form);
  if (!g_window) return;
  net_editor_slot_remove(&g_window->hosted);
  delete g_window;
  g_window = nullptr;
}

// Mech3.exe 1.2 @ 0x004036f0 -> net vt+0x5c (0x004bd330) -> 0x004bf670: the
// window made once and shown (Create(0xb5)), then 0x004bf650 with the
// caller's ECX, which 0x004bd330 zeroed: no file, so 0x00404570 frees what
// the window showed and opens nothing. FIX (A): the window as a form (ImGui);
// OK and Cancel close it, as DestroyWindow on a modeless dialog does
// (0x004d5610 / 0x004d55f0 -> 0x004bf6f0).
void net_params_open(int32_t) {
  if (!g_editor) return;
  net_window_show();
}

// Mech3.exe 1.2 @ 0x004bf670 with ECX 0: the window made once (0x004bf550,
// its slot on the frame's list) and shown, its editor freed (0x004bf650).
void net_window_show() {
  if (!g_window) {
    g_window = new NetWindow;
    net_editor_slot_add(&g_window->hosted);
  }
  net_window_open(nullptr);
  window_build();
}

// Mech3.exe 1.2 @ 0x004043c0: a slot on the frame's list.
void net_editor_slot_add(NetEditor** slot) { g_slots.push_back(slot); }

// Mech3.exe 1.2 @ 0x00404420: the slot's editor freed (0x00404490) and the
// slot off the list.
void net_editor_slot_remove(NetEditor** slot) {
  net_editor_slot_free(*slot);
  g_slots.erase(std::remove(g_slots.begin(), g_slots.end(), slot), g_slots.end());
}

// Mech3.exe 1.2 @ 0x00404490 (with 0)
void net_editor_slot_free(NetEditor*& slot) {
  delete slot;
  slot = nullptr;
}

// Mech3.exe 1.2 @ 0x00404270 -> 0x00404200: an editor on the list (the main
// one too) whose net was built from this one (+0x14's +0x34).
bool net_editor_showing(const ai::AiNet* net) {
  const auto on = [net](const NetEditor* e) { return e && e->net && e->net->net == net; };
  if (on(g_editor)) return true;
  for (NetEditor** s : g_slots)
    if (on(*s)) return true;
  return false;
}

// Mech3.exe 1.2 @ 0x004044c0: the slot shows `net` (an editor on a new edit
// of it, 0x004bd360 then 0x00402f40), unless it shows it already or another
// editor does; the old one freed first; null frees it.
void net_editor_show(NetEditor*& slot, ai::AiNet* net) {
  if (slot && net && slot->net && slot->net->net == net) return;
  net_editor_slot_free(slot);
  if (net && !net_editor_showing(net)) slot = new NetEditor(new ai::CZAiNetEdit(net->get_id(), nullptr, 0), 0);
}

// Mech3.exe 1.2 @ 0x00404290: every editor on the list, the window's too,
// stepped (0x00403080), all but the main one ([0x005f4c6c]) while the
// editor's menus are up (their own step runs it then).
void net_editor_frame_all(bool editing) {
  if (!editing && g_editor) net_editor_step(0.1f);
  const std::vector<NetEditor**> slots = g_slots;  // a step may free a slot's owner
  for (NetEditor** s : slots)
    if (*s && std::find(g_slots.begin(), g_slots.end(), s) != g_slots.end()) net_editor_step_slot(*s, 0.1f);
}
void EditorHooks::net_frame(bool editing) { net_editor_frame_all(editing); }

// Mech3.exe 1.2 @ 0x00402ba0: the editor freed (0x00402fd0), the side
// windows closed (vt+0x60: [0x005f4c70] the Tree View's; [0x005f4c74], which
// nothing in the exe sets), the main menu.
void net_editor_exit(int32_t) {
  net_editor_free();
  tree_close();  // [0x005f4c70] vt+0x60
  editor_main_menu();
}

void editor_status(const std::string& text) {
  g_status = text;
  std::printf("mw3: game editor: %s\n", text.c_str());
  EditorPanel& p = editor_panel();
  ztwod::FontText& r = *p.rows[24];
  r.show_for(-1.0f);
  r.set_text("%s", g_status.c_str());
  if (r.needs_render) r.render();
}

}  // namespace mech3
