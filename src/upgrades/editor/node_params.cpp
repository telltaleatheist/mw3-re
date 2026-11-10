#include "upgrades/editor/node_params.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>

#include "mech3/game_editor_net.h"
#include "mech3/zainet.h"
#include "mech3/zainetedit.h"
#include "upgrades/editor/form.h"

namespace mech3 {

namespace {
using ai::NetNodeRec;

struct Form {
  bool open = false;
  ai::NetNode* node = nullptr;
  NetNodeRec rec;             // the copy edited
  float pos[3] = {};
  uint8_t placed = 0;         // kind 0
  std::string name, gate;
  EditorForm form;
};
Form g;

void ok();
void cancel();

FormField value(const char* label, int32_t* on, float* v) {
  FormField f{label, FormField::kFloat};
  f.on = on, f.f = v;
  return f;
}
FormField number(const char* label, int32_t* v) {
  FormField f{label, FormField::kInt};
  f.i = v;
  return f;
}
FormField tri(const char* label, uint8_t* set, uint8_t* v) {
  FormField f{label, FormField::kTri};
  f.set = set, f.flag = v;
  return f;
}
FormField flag(const char* label, uint8_t* v) {
  FormField f{label, FormField::kFlag};
  f.flag = v;
  return f;
}
FormField text(const char* label, std::string* v) {
  FormField f{label, FormField::kText};
  f.text = v;
  return f;
}

void build() {
  NetNodeRec& r = g.rec;
  char title[64];
  std::snprintf(title, sizeof title, "Node Parameters: node_%02d", r.index);
  g.form = EditorForm{};
  g.form.title = title;
  g.form.fields = {
      flag("placed (x y z given)", &g.placed),
      value("x", nullptr, &g.pos[0]),
      value("y", nullptr, &g.pos[1]),
      value("z", nullptr, &g.pos[2]),
      value("speed", &r.speed_on, &r.speed),
      value("wait interval", &r.wait_interval_on, &r.wait_interval),
      value("wait time", &r.wait_time_on, &r.wait_time),
      number("can leave, first", &r.canleave[0]),
      number("can leave, second", &r.canleave[1]),
      value("jump", &r.jump_on, &r.jump),
      value("arrange nodes, spacing", nullptr, &r.arrange),
      tri("hidden", &r.hidden_set, &r.hidden),
      tri("fixed", &r.fixed_set, &r.fixed),
      tri("autonext", &r.autonext_set, &r.autonext),
      tri("visible", &r.visible_set, &r.visible),
      flag("backup", &r.backup),
      flag("shunt", &r.shunt),
      text("name", &g.name),
      text("gate", &g.gate),
  };
  g.form.buttons = {{"OK", ok}, {"Cancel", cancel}};
}

void close_form() {
  form_hide(&g.form);
  g.open = false, g.node = nullptr;
  g.rec = NetNodeRec{};
}

// The record put on the node: its place through set_pos (and the links
// stored again, as Move Node does), the rest field by field, then the fold
// into the file's cache (0x004befc0); the editor made anew (the view's marks).
void ok() {
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
    std::free(r.name), r.name = g.name.empty() ? nullptr : strdup(g.name.c_str());
    std::free(r.gate), r.gate = g.gate.empty() ? nullptr : strdup(g.gate.c_str());
    if (n->set_pos(g.pos)) n->relink();
    e->net->fold(n);
    e->dirty = 1;
    e->rebuild = 1;
  }
  const int32_t index = n ? n->rec.index : -1;
  close_form();
  if (index >= 0) editor_status("node_" + std::to_string(index) + " set; Save Changes keeps it");
}

void cancel() { close_form(); }
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
  form_hide(&g.form);
  g = Form{};
  g.open = true, g.node = n;
  g.rec = n->rec;  // a copy (the assignment drops the auxname: the node keeps its own)
  g.rec.index = n->rec.index;
  for (int i = 0; i < 3; ++i) g.pos[i] = n->rec.pos[i];
  g.placed = n->rec.kind == 0 ? 1 : 0;
  g.name = n->rec.name ? n->rec.name : "";
  g.gate = n->rec.gate ? n->rec.gate : "";
  build();
  form_show(&g.form);
}

bool node_params_open_now() { return g.open; }

EditorForm& node_params_form() { return g.form; }

}  // namespace mech3
