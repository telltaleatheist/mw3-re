#include "upgrades/editor/node_params.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>

#include "gamez/zinput/zinput.h"
#include "mech3/game_editor_net.h"
#include "mech3/zainet.h"
#include "mech3/zainetedit.h"

namespace mech3 {

namespace {
using ai::NetNodeRec;

enum Kind { kInt, kFloat, kFlag, kTri, kText };
// A field of the record: its label, its kind, and where it lives.
struct Field {
  const char* label;
  Kind kind;
  int32_t* on = nullptr;      // kFloat: its "on" word (null: always on); kInt: the value
  float* value = nullptr;     // kFloat
  uint8_t* set = nullptr;     // kTri: set, then value; kFlag: the value
  uint8_t* flag = nullptr;
  char** text = nullptr;      // kText
};

struct Form {
  bool open = false;
  ai::NetNode* node = nullptr;
  NetNodeRec rec;             // the copy edited
  float pos[3] = {};
  int32_t placed = 0;         // kind 0
  std::vector<Field> fields;
  int32_t typing = -1;        // the field taking typed text
  std::string buffer;
  std::vector<std::string> texts;
  std::vector<EditorEntry> rows;
  EditorMenu menu{0, nullptr};
};
Form g;

constexpr int32_t kFirstField = 1;  // row 0 the title

std::string shown(const Field& f) {
  char b[64];
  switch (f.kind) {
    case kInt: std::snprintf(b, sizeof b, "%d", *f.on); return b;
    case kFloat:
      if (f.on && !*f.on) return "off";
      std::snprintf(b, sizeof b, "%.3f", *f.value);
      return b;
    case kFlag: return *f.flag ? "yes" : "no";
    case kTri: return !*f.set ? "as the default" : *f.flag ? "yes" : "no";
    case kText: return *f.text && **f.text ? *f.text : "(none)";
  }
  return {};
}

void fill();
void press(int32_t y);
void ok(int32_t);
void cancel(int32_t);

void build_fields() {
  NetNodeRec& r = g.rec;
  g.fields = {
      {"placed (x y z given)", kInt, &g.placed},
      {"x", kFloat, nullptr, &g.pos[0]},
      {"y", kFloat, nullptr, &g.pos[1]},
      {"z", kFloat, nullptr, &g.pos[2]},
      {"speed", kFloat, &r.speed_on, &r.speed},
      {"wait interval", kFloat, &r.wait_interval_on, &r.wait_interval},
      {"wait time", kFloat, &r.wait_time_on, &r.wait_time},
      {"can leave, first", kInt, &r.canleave[0]},
      {"can leave, second", kInt, &r.canleave[1]},
      {"jump", kFloat, &r.jump_on, &r.jump},
      {"arrange nodes, spacing", kFloat, nullptr, &r.arrange},
      {"hidden", kTri, nullptr, nullptr, &r.hidden_set, &r.hidden},
      {"fixed", kTri, nullptr, nullptr, &r.fixed_set, &r.fixed},
      {"autonext", kTri, nullptr, nullptr, &r.autonext_set, &r.autonext},
      {"visible", kTri, nullptr, nullptr, &r.visible_set, &r.visible},
      {"backup", kFlag, nullptr, nullptr, nullptr, &r.backup},
      {"shunt", kFlag, nullptr, nullptr, nullptr, &r.shunt},
      {"name", kText, nullptr, nullptr, nullptr, nullptr, &r.name},
      {"gate", kText, nullptr, nullptr, nullptr, nullptr, &r.gate},
  };
}

void fill() {
  g.texts.clear(), g.rows.clear();
  const auto add = [](const std::string& t, void (*h)(int32_t)) { g.texts.push_back(t), g.rows.push_back({nullptr, h}); };
  char title[64];
  std::snprintf(title, sizeof title, "Node Parameters: node_%02d", g.rec.index);
  add(title, nullptr);
  for (size_t i = 0; i < g.fields.size(); ++i) {
    const Field& f = g.fields[i];
    const std::string v = static_cast<int32_t>(i) == g.typing ? g.buffer + "_" : shown(f);
    add(std::string(static_cast<int32_t>(i) == g.typing ? " > " : "   ") + f.label + ": " + v, press);
  }
  add(" OK", ok);
  add(" Cancel", cancel);
  for (size_t i = 0; i < g.rows.size(); ++i) g.rows[i].text = g.texts[i].c_str();
  g.menu = EditorMenu{static_cast<int32_t>(g.rows.size()), g.rows.data()};
  editor_install(1, g.menu);
}

void close_form() {
  zin::set_char_callback(nullptr, 0);
  g.open = false, g.typing = -1, g.node = nullptr;
  g.rec = NetNodeRec{};
  net_editor_menu();
}

// The typed text into its field: a number for a value ("off" turns a
// switchable value off), the text for a name ("-" clears it); nothing typed
// or a bad number leaves it as it was.
void take() {
  if (g.typing < 0) return;
  Field& f = g.fields[size_t(g.typing)];
  const std::string& b = g.buffer;
  char* end = nullptr;
  if (b.empty()) {
    // as it was
  } else if (f.kind == kInt) {
    const long v = std::strtol(b.c_str(), &end, 10);
    if (end && !*end) *f.on = static_cast<int32_t>(v);
  } else if (f.kind == kFloat) {
    if (b == "off" && f.on) {
      *f.on = 0;
    } else {
      const float v = std::strtof(b.c_str(), &end);
      if (!b.empty() && end && !*end) *f.value = v, f.on ? (*f.on = 1, 0) : 0;
    }
  } else if (f.kind == kText) {
    std::free(*f.text);
    *f.text = b == "-" ? nullptr : strdup(b.c_str());
  }
  g.typing = -1, g.buffer.clear();
}

void on_char(int32_t ch, int32_t key, uintptr_t) { node_params_char(ch, key); }

void press(int32_t y) {
  const int32_t i = (y - 0x5f) / 12 - kFirstField;
  if (i < 0 || i >= static_cast<int32_t>(g.fields.size())) return;
  Field& f = g.fields[size_t(i)];
  if (g.typing >= 0) take();
  if (f.kind == kFlag) {
    *f.flag = !*f.flag;
  } else if (f.kind == kTri) {  // as the default -> yes -> no -> as the default
    if (!*f.set) *f.set = 1, *f.flag = 1;
    else if (*f.flag) *f.flag = 0;
    else *f.set = 0;
  } else {
    g.typing = i;  // typed text replaces the value; Enter on none keeps it
    g.buffer.clear();
  }
  fill();
}

// The record put on the node: its place through set_pos (and the links
// stored again, as Move Node does), the rest field by field, then the fold
// into the file's cache (0x004befc0); the editor made anew (the view's marks).
void ok(int32_t) {
  if (g.typing >= 0) take();
  NetEditor* e = net_editor();
  ai::NetNode* n = g.node;
  if (e && n && e->net && n->owner == e->net) {
    NetNodeRec& r = n->rec;
    const NetNodeRec& c = g.rec;
    r.kind = g.placed ? 0 : 1;
    r.speed_on = c.speed_on, r.speed = c.speed;
    r.wait_interval_on = c.wait_interval_on, r.wait_interval = c.wait_interval;
    r.wait_time_on = c.wait_time_on, r.wait_time = c.wait_time;
    r.canleave[0] = c.canleave[0], r.canleave[1] = c.canleave[1];
    r.jump_on = c.jump_on, r.jump = c.jump;
    r.arrange_set = c.arrange != 0.0f ? 1 : 0, r.arrange = c.arrange;
    r.hidden_set = c.hidden_set, r.hidden = c.hidden, r.fixed_set = c.fixed_set, r.fixed = c.fixed;
    r.autonext_set = c.autonext_set, r.autonext = c.autonext, r.visible_set = c.visible_set, r.visible = c.visible;
    r.backup = c.backup, r.shunt = c.shunt;
    std::free(r.name), r.name = c.name ? strdup(c.name) : nullptr;
    std::free(r.gate), r.gate = c.gate ? strdup(c.gate) : nullptr;
    if (n->set_pos(g.pos)) n->relink();
    e->net->fold(n);
    e->dirty = 1;
    e->rebuild = 1;
  }
  const int32_t index = n ? n->rec.index : -1;
  close_form();
  if (index >= 0) editor_status("node_" + std::to_string(index) + " set; Save Changes keeps it");  // after the menu's refill
}

void cancel(int32_t) { close_form(); }
}  // namespace

void node_params_open(int32_t) {
  NetEditor* e = net_editor();
  if (!e) return;
  znet::Mark* m = net_editor_first_pick();
  ai::NetNode* n = m && m->elem ? static_cast<ai::AiElem*>(m->elem)->as_node() : nullptr;
  if (!n || n->placeholder) {
    editor_status("Select a node first");
    return;
  }
  g = Form{};
  g.open = true, g.node = n;
  g.rec = n->rec;  // a copy (the assignment drops the auxname: the node keeps its own)
  g.rec.index = n->rec.index;
  for (int i = 0; i < 3; ++i) g.pos[i] = n->rec.pos[i];
  g.placed = n->rec.kind == 0 ? 1 : 0;
  build_fields();
  zin::set_char_callback(on_char, 0);
  fill();
}

bool node_params_open_now() { return g.open; }

void node_params_char(int32_t ch, int32_t key) {
  if (!g.open || g.typing < 0) return;
  if (ch == '\r' || ch == '\n' || key == 0x1c) {  // Enter
    take();
  } else if (ch == 27 || key == 0x01) {  // Esc
    g.typing = -1, g.buffer.clear();
  } else if (ch == 8 || key == 0x0e) {  // Backspace
    if (!g.buffer.empty()) g.buffer.pop_back();
  } else if (ch >= 0x20 && ch < 0x7f && g.buffer.size() < 40) {
    g.buffer += static_cast<char>(ch);
  } else {
    return;
  }
  fill();
}

void node_params_press(int32_t row) {
  if (row >= 0 && row < g.menu.count && g.rows[size_t(row)].handler) g.rows[size_t(row)].handler(0x5f + 12 * row + 2);
}

}  // namespace mech3
