#include "mech3/mech_objective.h"

#include <cstdio>
#include <cstring>
#include <utility>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_msg.h"
#include "mech3/mech_names.h"
#include "mech3/pilot.h"

namespace mech3 {

using zrdr::Node;

zrdr::Node* ObjectiveHooks::load(const char* name) { return zrdr::load(name); }
void ObjectiveHooks::free_tree(zrdr::Node* root) { zrdr::free_node(root); }
const char* ObjectiveHooks::localize(const char* name) { return zutl::localize(name); }

NodeSalvage::NodeSalvage() = default;
NodeSalvage::NodeSalvage(NodeSalvage&&) noexcept = default;
NodeSalvage::~NodeSalvage() = default;

// Mech3.exe 1.2 @ 0x0045b9e0
Objective::Objective(Kind k, const char* d) : kind(k) {
  if (d) desc = d;
}

// Mech3.exe 1.2 @ 0x0045bb00. The persist text (+0x2c) is never freed.
Objective::~Objective() = default;

// Mech3.exe 1.2 @ 0x0045f110
void Objective::set_desc(const char* text) {
  desc.reset();
  if (text) desc = text;
}

// Mech3.exe 1.2 @ 0x0045f2b0 (the named target), as 0x0045c920 inlines it.
DestroyObjective::DestroyObjective(const char* d, const char* n, Handle e) : Objective(kDestroy, d), entity(e) {
  if (n) name = n;
}

// Mech3.exe 1.2 @ 0x0045e630
void DestroyObjective::find_entity(ObjectiveHooks& h) {
  if (entity == 0 && name) entity = h.entity_find(name->c_str());
}

// Mech3.exe 1.2 @ 0x0045f340: the description becomes "destroy <its name>"
// (message 0xde0) when the entity has one.
void DestroyObjective::relabel(ObjectiveHooks& h) {
  find_entity(h);
  if (!entity) return;
  if (const char* label = h.entity_label(entity)) set_desc(h.message_with(0xde0, label).c_str());
}

ReachAreaObjective::ReachAreaObjective(const char* d, Handle n, const float p[3], float r)
    : Objective(kReachArea, d), node(n), point{p[0], p[1], p[2]}, radius(r), radius_sq(r * r) {}

namespace {

bool is(const char* a, const char* b) { return a && std::strcmp(a, b) == 0; }

int32_t count_of(const Node* list) { return list->list[0].i; }

// 0x0045c8f0: "nav" by name, when the nav points are loaded.
Handle nav_of(Node* args, ObjectiveHooks& h) {
  const char* name = zrdr::get_string(args, "nav");
  return name ? h.nav_find(name) : 0;
}

// What 0x0045c920 reads for every type (its head and its tail 0x0045dfe5).
struct Shared {
  const char* desc;
  const char* sound;
  bool visible = true, silent = false;
  bool visible_found, silent_found;
  Node* salvage;
  Node* cond_salvage;
  const char* persist;
  Node* set_valve;
  Handle valve = 0;
  int32_t value = 1;
};

std::unique_ptr<DestroyObjective> make_destroy(const char* desc, const char* name, ObjectiveHooks& h) {
  Handle e = h.entity_find(name);
  auto d = std::make_unique<DestroyObjective>(desc, name, e);
  d->find_entity(h);
  d->relabel(h);  // the destroy type's own find (0x0045e630 again), then its label
  return d;
}

std::unique_ptr<ObjectiveGroup> make_group(const char* desc) { return std::make_unique<ObjectiveGroup>(desc); }

// Types the exe reads that the port does not yet (piece 5): reported, not read.
const struct {
  const char* type;
  const char* key;
} kUnported[] = {
    {"reach_velocity", "0x0045c920 reach_velocity"}, {"aim_at_object", "0x0045c920 aim_at_object"},
    {"target_object", "0x0045c920 target_object"},   {"time_limit", "0x0045c920 time_limit"},
    {"view_object", "0x0045c920 view_object"},       {"face_object", "0x0045c920 face_object"},
    {"heat_warning", "0x0045c920 heat_warning"},     {"User_Command", "0x0045c920 User_Command"},
    {"joystick_press", "0x0045c920 joystick_press"}, {"key_press", "0x0045c920 key_press"},
    {"mouse_press", "0x0045c920 mouse_press"},       {"weapon_fire", "0x0045c920 weapon_fire"},
    {"weapon_select", "0x0045c920 weapon_select"},   {"weapon_group_select", "0x0045c920 weapon_group_select"},
};

}  // namespace

// Mech3.exe 1.2 @ 0x0045c810: the "node" list's first name among the world's
// nodes (zClass list 6), then each further name inside the last.
Handle find_node(Node* args, ObjectiveHooks& h) {
  Node* v = zrdr::find(args, "node");
  if (!v || v->type != zrdr::List) return 0;
  const int32_t n = count_of(v);
  // FIX (bug): an empty list; the exe reads the slot past it.
  if (n < 2) return 0;
  const char* first = v->list[1].s;
  Handle node = h.node_find(first);
  if (!node) h.report(0x200, 0x189, "Failed to find node (%s)", first, 0);
  for (int32_t i = 2; i < n; ++i) {
    if (!node) return 0;
    const char* name = v->list[i].s;
    Handle sub = h.subnode_find(node, name);
    if (!sub) h.report(0x200, 0x192, "Failed to find subnode (%s) of root (%s)", name, node);
    node = sub;
  }
  return node;
}

// Mech3.exe 1.2 @ 0x004711a0: a salvage block into a store, none when it
// gives nothing. Weapons must be (name tech) lists and the engines' key is
// "engines": the shipped files' plain weapon names and "engine" give nothing
// (docs/objectives.md).
std::unique_ptr<Store> read_salvage(Node* node) {
  if (!node) return nullptr;
  auto s = std::make_unique<Store>(0);
  auto each = [&](const char* key, auto&& fn) {
    Node* v = zrdr::find(node, key);
    if (!v) return;
    const int32_t n = count_of(v);
    for (int32_t i = 1; i < n; ++i) fn(v->list[i]);
  };
  // FIX (bug): the entries' parts are read by type below; the exe takes a
  // string's pointer or an int's value, whatever the slot holds.
  auto str = [](const Node& e) { return e.type == zrdr::String ? e.s : nullptr; };
  each("mechs", [&](Node& e) {
    if (e.type != zrdr::String) return;
    const int32_t m = mech_index(e.s);
    if (m != -1) s->add_mech(m);
  });
  each("equipment", [&](Node& e) {
    if (e.type != zrdr::List || count_of(&e) <= 2) return;
    const int32_t kind = equip_kind(str(e.list[1]));
    const int32_t tech = tech_id(str(e.list[2]));
    if (kind != -1) s->add_equipment(kind, tech, 1);
  });
  each("weapons", [&](Node& e) {
    if (e.type != zrdr::List || count_of(&e) <= 2) return;
    const int32_t w = weapon_index(str(e.list[1]));
    const int32_t tech = tech_id(str(e.list[2]));
    if (w != -1) s->add_weapon(w, tech);
  });
  each("ammo", [&](Node& e) {
    if (e.type != zrdr::List || count_of(&e) <= 2) return;
    const int32_t w = weapon_index(str(e.list[1]));
    const int32_t n = e.list[2].type == zrdr::Int ? e.list[2].i : 0;
    if (w != -1 && n != 0) s->add_ammo(w, n);
  });
  each("engines", [&](Node& e) {
    if (e.type != zrdr::List || count_of(&e) < 2) return;  // FIX (bug): an empty list, read past in the exe
    const uint8_t xl = count_of(&e) == 2;
    s->add_engine(engine_index(e.list[1].i), xl);
  });
  if (s->items.empty()) return nullptr;
  return s;
}

// Mech3.exe 1.2 @ 0x0045e9f0: cond_salvage's node_state entries, each a node,
// whether its state is "active" and what it gives.
void read_node_salvage(Objective& o, Node* cond, ObjectiveHooks& h) {
  if (!cond) return;
  const int32_t n = count_of(cond);
  for (int32_t k = 1; k < n; k += 2) {
    Node* slot = &cond->list[k];
    if (slot->type != zrdr::String || !is(slot->s, "node_state")) continue;
    Node* args = slot + 1;
    const Handle node = find_node(args, h);
    if (!node) continue;
    const uint8_t active = is(zrdr::get_string(args, "state"), "active");
    std::unique_ptr<Store> s;
    if (Node* v = zrdr::find(args, "salvage")) s = read_salvage(v);
    if (!s) continue;
    NodeSalvage e;
    e.node = node, e.active = active, e.salvage = std::move(s);
    o.node_salvage.push_back(std::move(e));
  }
}

// Mech3.exe 1.2 @ 0x0045c920
std::unique_ptr<Objective> parse_condition(Node* slot, ObjectiveHooks& h) {
  // FIX (bug): a type that is not a name; the exe reads the slot as a string.
  if (slot->type != zrdr::String) return nullptr;
  const char* type = slot->s;
  Node* args = slot + 1;
  Shared sh;
  sh.desc = h.localize(zrdr::get_string(args, "desc"));
  sh.sound = zrdr::get_string(args, "sound");
  sh.visible_found = zrdr::get_bool(args, "visible", &sh.visible);
  sh.silent_found = zrdr::get_bool(args, "silent", &sh.silent);
  sh.salvage = zrdr::find_flat(args, "salvage", 1);
  sh.cond_salvage = zrdr::find(args, "cond_salvage");
  sh.persist = zrdr::get_string(args, "persist");
  sh.set_valve = zrdr::find_flat(args, "set_valve", 1);
  if (Node* v = sh.set_valve; v && v->type == zrdr::List && count_of(v) > 2 && v->list[1].type == zrdr::String &&
                              v->list[2].type == zrdr::Int) {
    sh.valve = h.valve_find(v->list[1].s);
    sh.value = v->list[2].i;
  }
  std::unique_ptr<Objective> obj;

  if (is(type, "destroy_entity")) {
    // The names are what is left after the keys this counts: two slots each
    // for desc, salvage, cond_salvage, visible, silent, set_valve and nav
    // (sound and persist are not counted; docs/objectives.md).
    int32_t first = 1;
    int32_t names = count_of(args) - 1;
    const Handle nav = nav_of(args, h);
    if (sh.desc) names -= 2, first += 2;
    for (bool counted : {sh.salvage != nullptr, sh.cond_salvage != nullptr, sh.visible_found, sh.silent_found,
                         sh.set_valve != nullptr, nav != 0})
      if (counted) names -= 2, first += 2;
    auto name_at = [&](int32_t i) {
      const Node& n = args->list[i];
      return n.type == zrdr::String ? n.s : nullptr;  // FIX (bug): the exe takes any slot as a string
    };
    if (names == 1) {
      obj = make_destroy(sh.desc, name_at(first), h);
      if (nav) obj->nav = nav;
    } else if (names > 1) {
      auto g = make_group(sh.desc);
      if (nav) g->nav = nav;
      for (int32_t i = 0; i < names; ++i) g->items.push_back(make_destroy(sh.desc, name_at(first + i), h));
      obj = std::move(g);
    }
  } else if (is(type, "node_change")) {
    const Handle node = find_node(args, h);
    if (!node) return nullptr;
    const uint8_t active = is(zrdr::get_string(args, "state"), "active");
    obj = std::make_unique<NodeChangeObjective>(sh.desc, node, active);
    if (Handle nav = nav_of(args, h)) obj->nav = nav;
  } else if (is(type, "reach_area")) {
    float radius = 25.0f;
    zrdr::get_float(args, "radius", &radius);
    float point[3];
    h.default_point(point);
    if (const Handle node = find_node(args, h)) {
      obj = std::make_unique<ReachAreaObjective>(sh.desc, node, point, radius);
    } else if (const Handle nav = nav_of(args, h)) {
      h.nav_show(nav, 1);
      obj = std::make_unique<ReachAreaObjective>(sh.desc, 0, point, radius);
      obj->nav = nav;
    } else {
      if (!zrdr::get_vec3(args, "point", point)) return nullptr;
      obj = std::make_unique<ReachAreaObjective>(sh.desc, 0, point, radius);
    }
  } else if (is(type, "valve_change")) {
    Handle valve = 0;
    if (const char* name = zrdr::get_string(args, "valve")) {
      valve = h.valve_find(name);
      if (h.game_type() == 3) h.valve_set(valve, 0);  // multiplayer starts its valves at 0
    }
    if (!valve) return nullptr;
    int32_t value = 1;
    zrdr::get_int(args, "value", &value);
    obj = std::make_unique<ValveChangeObjective>(sh.desc, valve, value);
    if (Handle nav = nav_of(args, h)) obj->nav = nav;
  } else if (is(type, "obj_list")) {
    Node* list = zrdr::find(args, "list");
    if (!list) return nullptr;  // FIX (bug): the exe reads through the null
    const int32_t n = (count_of(list) - 1) / 2;
    if (n <= 0) return nullptr;
    const Handle nav = nav_of(args, h);
    auto g = make_group(sh.desc);
    int32_t goal = 0;
    if (zrdr::get_int(args, "goal_count", &goal)) g->goal_count = goal;
    bool sim = false;
    if (zrdr::get_bool(args, "simultaneous", &sim)) g->simultaneous = sim;
    for (int32_t i = 0; i < n; ++i)
      if (auto c = parse_condition(&list->list[1 + 2 * i], h)) g->items.push_back(std::move(c));
    if (g->items.empty()) return nullptr;
    if (nav) g->nav = nav;
    obj = std::move(g);
  } else {
    for (const auto& u : kUnported)
      if (is(type, u.type)) {
        h.unported(u.key, {});
        break;
      }
    return nullptr;
  }

  if (!obj) return nullptr;
  obj->salvage = read_salvage(sh.salvage);
  read_node_salvage(*obj, sh.cond_salvage, h);
  obj->sound = h.sound_find(sh.sound);
  obj->visible = sh.visible;
  obj->silent = sh.silent;
  obj->persist.reset();
  if (sh.persist) obj->persist = sh.persist;
  if (sh.valve) obj->valve = sh.valve, obj->value = sh.value;
  return obj;
}

// Mech3.exe 1.2 @ 0x0045bf60: the block's `key` list into `out` (made when
// it has conditions, kept only when one reads); its own desc, sound, visible
// and silent.
void read_group(Node* block, const char* key, const char* default_desc, std::unique_ptr<ObjectiveGroup>& out,
                ObjectiveHooks& h) {
  if (!block) return;
  Node* v = zrdr::find(block, key);
  if (!v) return;
  const char* desc = h.localize(zrdr::get_string_flat(v, "desc", 1));
  if (!desc) desc = default_desc;
  const char* sound = zrdr::get_string_flat(v, "sound", 1);
  int32_t visible = 0, silent = 0;
  Node* vis = zrdr::find_flat(v, "visible", 1);
  if (vis) zrdr::get_bool_int(vis, &visible);
  Node* sil = zrdr::find_flat(v, "silent", 1);
  if (sil) zrdr::get_bool_int(sil, &silent);
  const int32_t n = (count_of(v) - 1) / 2;
  if (n <= 0) return;
  if (!out) out = make_group(desc);
  for (int32_t i = 0; i < n; ++i)
    if (auto c = parse_condition(&v->list[1 + 2 * i], h)) out->items.push_back(std::move(c));
  if (out->items.empty()) {
    out.reset();
    return;
  }
  if (vis) out->visible = visible == 1;
  if (sil) out->silent = silent == 1;
  if (sound) out->sound = h.sound_find(sound);
}

// Mech3.exe 1.2 @ 0x0045bc90: a campaign's fail condition: the mobile field
// base's three parts (mobilehq_01..03) destroyed, those that are in the world.
void add_mobile_hq(const char* desc, std::unique_ptr<ObjectiveGroup>& fail, ObjectiveHooks& h) {
  const char* group_desc = h.localize("MSG_OBJ_0xA23");
  const char* part_desc = h.localize("MSG_OBJ_MFB_DESTROYED");
  std::unique_ptr<ObjectiveGroup> outer;
  if (!fail) outer = make_group(desc ? desc : group_desc);
  auto inner = make_group(group_desc);
  for (int i = 1; i <= 3; ++i) {
    char name[16];
    std::snprintf(name, sizeof name, "mobilehq_0%d", i);
    const Handle e = h.entity_find(name);
    if (!e || !h.entity_flag(e)) continue;
    auto d = std::make_unique<DestroyObjective>(part_desc, name, e);
    d->find_entity(h);
    d->relabel(h);
    inner->items.push_back(std::move(d));
  }
  if (inner->items.empty()) return;
  if (outer) {
    outer->items.push_back(std::move(inner));
    fail = std::move(outer);
  } else {
    fail->items.push_back(std::move(inner));
  }
}

// Mech3.exe 1.2 @ 0x0045c340
bool read_objectives(Node* block, Objectives& o, ObjectiveHooks& h) {
  if (!block) return false;
  Node* d = zrdr::find_flat(block, "desc", 1);
  if (d && d->type == zrdr::List && d->list[1].type == zrdr::String) {
    // FIX (bug): a missing message; the exe's strlen reads through the null.
    if (const char* text = h.localize(d->list[1].s)) o.desc = text;
  }
  read_group(block, "primary", h.message(0x16d), o.primary, h);
  read_group(block, "secondary", h.message(0x16e), o.secondary, h);
  read_group(block, "final", h.message(0xf03), o.final, h);
  read_group(block, "fail", h.message(0xf04), o.fail, h);
  if (h.game_type() == 1) add_mobile_hq(h.message(0xf04), o.fail, h);
  if (o.fail) o.fail->goal_count = 1;
  int32_t n = 0;
  for (auto* g : {&o.primary, &o.secondary, &o.final, &o.fail})
    if (*g) n += static_cast<int32_t>((*g)->items.size());
  return n > 0;
}

// Mech3.exe 1.2 @ 0x0045c2d0
bool objectives_load(const char* file, const char* block, Objectives& o, ObjectiveHooks& h) {
  Node* root = h.load(file);
  if (!root) return false;
  Node* b = zrdr::find(root, block);
  if (!b) return false;  // the tree is not freed (original)
  const bool ok = read_objectives(b, o, h);
  h.free_tree(root);
  return ok;
}

// Mech3.exe 1.2 @ 0x0045c250
void objectives_delete(Objectives& o) {
  o.primary.reset(), o.secondary.reset(), o.final.reset(), o.fail.reset();
  o.desc.reset();
}

// Mech3.exe 1.2 @ 0x0045c1d0: training's game frees its own (they are its
// lessons'); a game type below -1 leaks them.
void objectives_free(Objectives& o, ObjectiveHooks& h) {
  const int32_t t = h.game_type();
  if (t >= -1 && t <= 3) {
    objectives_delete(o);
  } else if (t == 4) {
    h.training_free();
  }
  // The pointers are cleared either way.
  o.primary.release(), o.secondary.release(), o.final.release(), o.fail.release();
  o.desc.reset();
}

}  // namespace mech3
