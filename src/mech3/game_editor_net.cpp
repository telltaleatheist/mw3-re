#include "mech3/game_editor_net.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

#include "crt/msvc_string.h"
#include "gamez/zreader/zreader.h"
#include "gamez/znetwork/znet_object.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/net_save.h"
#include "mech3/zainet.h"
#include "mech3/zainetedit.h"

namespace mech3 {

namespace {
NetDialog g_dialog;
NetEditor* g_editor = nullptr;  // [0x005f4c6c]
// The dialog's rows as filled (their texts kept here).
std::vector<std::string> g_texts;
std::vector<EditorEntry> g_rows;
EditorMenu g_dialog_menu{0, nullptr};
std::string g_status;

constexpr int32_t kListRows = 18;  // ours: the list's rows the panel has room for

void not_ported(const char* what) { editor_hooks().not_ported(what); }
void none(int32_t) {}  // 0x004bf620
void change_net_params(int32_t) { not_ported("Change Net Params (0x004036f0, the Ai Network Editor dialog 181)"); }
void node_params(int32_t) { not_ported("Node Params (0x00403710, the Node Parameters dialog 180)"); }
void add_vehicle(int32_t) { not_ported("Add veh to node (0x00403820, the Node Edit - Vehicle dialog 141)"); }
void move_node(int32_t) { not_ported("Move Node (0x00403ac0)"); }
void delete_node(int32_t) { not_ported("Delete Node (0x00403ae0)"); }
void tree_view(int32_t) { not_ported("Tree View (0x00402c70, the tree window)"); }

// 0x005e0f18.
const EditorEntry kNetRows[13] = {
    {"Pick CMD or Point", none},   {" GOTO Net", net_editor_goto},        {" Change Net Params", change_net_params},
    {" Node Params", node_params}, {" Add veh to node", add_vehicle},     {" Move Node", move_node},
    {" Delete Node", delete_node}, {" Delete Net", none},                  {" Save Changes", net_editor_save},
    {" Tree View", tree_view},     {"", nullptr},                          {"", nullptr},
    {" Exit Net Editor", net_editor_exit},
};
const EditorMenu kNetMenu = {13, kNetRows};

int32_t row_of(int32_t y) { return (y - 0x5f) / 12; }  // the panel's rows, 12 apart from 0x5f

void fill_dialog();
// The dialog's rows: the title, the label, the list (a page of it), OK, Cancel.
void list_pick(int32_t y) {
  const int32_t i = row_of(y) - 2 + g_dialog.first - (g_dialog.first > 0 ? 1 : 0);
  if (i >= 0 && i < static_cast<int32_t>(g_dialog.items.size())) g_dialog.selected = i;
  fill_dialog();
}
void list_up(int32_t) {
  g_dialog.first = std::max(0, g_dialog.first - kListRows);
  fill_dialog();
}
void list_down(int32_t) {
  g_dialog.first += kListRows;
  fill_dialog();
}
void dialog_button(int32_t) { net_dialog_close(); }

void fill_dialog() {
  g_texts.clear(), g_rows.clear();
  const auto add = [&](const std::string& t, void (*h)(int32_t)) { g_texts.push_back(t), g_rows.push_back({nullptr, h}); };
  add("AI Network Editor", nullptr);
  add(" Select a network to edit:", nullptr);
  const int32_t n = static_cast<int32_t>(g_dialog.items.size());
  if (g_dialog.first > 0) add("   (up)", list_up);
  const int32_t last = std::min(n, g_dialog.first + kListRows);
  for (int32_t i = g_dialog.first; i < last; ++i)
    add(std::string(i == g_dialog.selected ? " > " : "   ") + g_dialog.items[size_t(i)].first, list_pick);
  if (last < n) add("   (down)", list_down);
  add(" OK", dialog_button);
  add(" Cancel", dialog_button);
  for (size_t i = 0; i < g_rows.size(); ++i) g_rows[i].text = g_texts[i].c_str();
  g_dialog_menu = EditorMenu{static_cast<int32_t>(g_rows.size()), g_rows.data()};
  editor_install(0, g_dialog_menu);
}

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
}  // namespace

NetDialog& net_dialog() { return g_dialog; }
NetEditor*& net_editor() { return g_editor; }
const EditorMenu& editor_net_menu_rows() { return kNetMenu; }
const EditorMenu& editor_net_dialog_rows() { return g_dialog_menu; }

// Mech3.exe 1.2 @ 0x00402f40 (the selection base 0x0058bf50 on the net and
// the world, sitting 3b's): the camera's far clip kept (0x004f3b00's
// second), then 0x004f2490 / 0x004f23e0 put back what they read.
NetEditor::NetEditor(ai::CZAiNetEdit* n, int32_t d) : net(n), dirty(d) { far_clip = editor_hooks().far_clip(); }

// Mech3.exe 1.2 @ 0x00402fd0: the far clip back when it was kept, the net
// deleted (its vt+0x00(1)).
NetEditor::~NetEditor() {
  if (far_clip > 0.0f) editor_hooks().set_far_clip(far_clip);
  delete net;
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
  fill_dialog();
}

// The rest of 0x00402d60: a net that is not an edit (vt+0x48) edited through
// a new one of its id (0x004bd360: CZAiNetEdit(id, none, 0)); then the old
// editor freed, the new one kept and its menu installed. New Net (0x00402c90:
// a new net's first node at the camera's ground point) is sitting 4's.
void net_dialog_close() {
  g_dialog.open = false;
  ai::AiNet* pick = g_dialog.selected >= 0 && g_dialog.selected < static_cast<int32_t>(g_dialog.items.size())
                        ? g_dialog.items[size_t(g_dialog.selected)].second
                        : nullptr;
  if (!pick) {
    not_ported("New Net (0x00402c90: a new net's first node)");
    editor_main_menu();
    return;
  }
  auto* edit = dynamic_cast<ai::CZAiNetEdit*>(pick);
  if (!edit) edit = new ai::CZAiNetEdit(pick->get_id(), nullptr, 0);  // 0x004bd360 (vt+0x18 the id)
  // The editor owns it either way: 0x00402fd0 deletes it, and a net leaves
  // the registry as it goes.
  NetEditor* e = new NetEditor(edit, 0);
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
// save goes no further than 0x004bd100's fold (sitting 4's edits) and the
// stub 0x004d72a0; FIX (A), ours: the mission's archive written as the
// editor mod's copy (net_save.h), its folder said on screen.
void net_editor_save(int32_t) {
  if (!g_editor) {
    editor_status("no network to save");  // 0x00402ec0(0x30, ...): a box in the exe
    return;
  }
  if (ai::CZAiNetEdit* n = g_editor->net) {
    std::string archive, entry;
    std::vector<uint8_t> bytes;
    if (!net_entry(*n, &archive, &entry, &bytes)) {
      editor_status("not saved: the net's file is in no open archive");
    } else {
      std::vector<std::pair<std::string, const ai::NetNodeRec*>> edits;
      for (const auto& [key, rec] : n->ref->file->cache) edits.emplace_back(key, rec);
      const std::vector<uint8_t> out = ai::net_with_edits(bytes.data(), bytes.size(), n->ref->file->tree, edits);
      const ai::NetSave s = out.empty() ? ai::NetSave{false, "", "", "", entry + " does not read as a net"}
                                        : ai::save_archive_edit(archive.c_str(), entry.c_str(), out);
      editor_status(s.ok ? s.message : "not saved: " + s.error);
    }
  }
  g_editor->dirty = 0;
}

// Mech3.exe 1.2 @ 0x00402ba0: the editor freed (0x00402fd0), the side
// windows (the tree view's [0x005f4c74], the node window's [0x005f4c70]:
// sitting 4's) closed, the main menu.
void net_editor_exit(int32_t) {
  net_editor_free();
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
