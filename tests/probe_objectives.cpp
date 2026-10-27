// The objectives' reading (mech_objective.cpp) for test_objectives.py: a
// block of an objectives file read through fake engine calls that the test
// gives the original too, then the objects written out as text.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "mech3/mech_config.h"
#include "mech3/mech_objective.h"
#include "mech3/pilot.h"

namespace {

using namespace mech3;

uint32_t fnv(const char* s) {
  uint32_t h = 2166136261u;
  for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
  return h;
}

std::string hex(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%x", v);
  return b;
}

uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

// The same fakes as test_objectives.py's stubs.
class Fake : public ObjectiveHooks {
 public:
  std::string log;
  std::set<Handle> entities, nodes;  // made so far (test_objective_check.py's dicts)
  int32_t type = 1;
  float pt[3] = {};
  std::string text[64];
  int next = 0;

  const char* keep(std::string s) {
    std::string& t = text[next++ & 63];
    t = std::move(s);
    return t.c_str();
  }
  void ev(const std::string& s) { log += s + "\n"; }
  static std::string str(const char* s) { return s ? s : "(null)"; }

  zrdr::Node* load(const char* name) override {
    ev("load " + str(name));
    return zrdr::load(name);
  }
  void free_tree(zrdr::Node* root) override {
    ev("free");
    zrdr::free_node(root);
  }
  const char* localize(const char* name) override {
    ev("loc " + str(name));
    return name ? keep(std::string("L:") + name) : nullptr;
  }
  const char* message(int32_t id) override {
    ev("msg " + hex(id));
    return keep("M" + hex(id));
  }
  std::string message_with(int32_t id, const char* s) override {
    ev("msgw " + hex(id) + " " + str(s));
    return ("F" + hex(id) + ":" + str(s)).substr(0, 63);
  }
  Handle sound_find(const char* name) override {
    ev("snd " + str(name));
    return name ? 0x5000000 | (fnv(name) & 0xffffff) : 0;
  }
  Handle entity_find(const char* name) override {
    ev("ent " + str(name));
    if (!name) return 0;
    const uint32_t h = fnv(name);
    if (h % 5 == 0) return 0;
    entities.insert(0x6000000 | (h & 0xffffff));
    return 0x6000000 | (h & 0xffffff);
  }
  const char* entity_label(Handle e) override {
    ev("label " + hex(e));
    return (e >> 4) & 3 ? keep("E" + hex(e)) : nullptr;
  }
  Handle nav_find(const char* name) override {
    ev("nav " + str(name));
    const uint32_t h = fnv(name);
    return h % 4 == 0 ? 0 : 0x7000000 | (h & 0xffffff);
  }
  void nav_show(Handle nav, uint8_t on) override { ev("navshow " + hex(nav) + " " + hex(on)); }
  Handle node_find(const char* name) override {
    ev("node " + str(name));
    const uint32_t h = fnv(name);
    const Handle n = h % 6 == 0 ? 0 : 0x8000000 | (h & 0xffffff);
    if (n) nodes.insert(n);
    return n;
  }
  Handle subnode_find(Handle node, const char* name) override {
    ev("sub " + hex(node) + " " + str(name));
    const uint32_t h = fnv(name) ^ node;
    const Handle n = h % 6 == 0 ? 0 : 0x9000000 | (h & 0xffffff);
    if (n) nodes.insert(n);
    return n;
  }
  Handle valve_find(const char* name) override {
    ev("valve " + str(name));
    const uint32_t h = fnv(name);
    return h % 7 == 0 ? 0 : 0xa000000 | (h & 0xffffff);
  }
  void valve_set(Handle valve, int32_t v) override { ev("vset " + hex(valve) + " " + hex(v)); }
  int32_t game_type() override {
    ev("type");
    return type;
  }
  void training_free() override { ev("trainfree"); }
  // Training's lookups (test_objectives.py's stubs).
  int32_t command_find(const char* name) override {
    ev("cmd " + str(name));
    return static_cast<int32_t>(fnv(name) % 40);
  }
  int32_t command_key(int32_t c) override {
    ev("ckey " + hex(c));
    return (c * 3) % 5 - 1;
  }
  int32_t command_joy(int32_t c) override {
    ev("cjoy " + hex(c));
    return c % 3 == 0 ? 0 : c;
  }
  int32_t command_mouse(int32_t c) override {
    ev("cmouse " + hex(c));
    return c % 4 == 0 ? c % 3 : 0;
  }
  int32_t key_by_name(const char* name) override {
    ev("key " + str(name));
    const uint32_t h = fnv(name);
    return h % 5 == 0 ? -1 : static_cast<int32_t>(h % 200);
  }
  void report(int32_t level, int32_t line, const char*, const char*, Handle) override {
    ev("zerr " + hex(level) + " " + hex(line));
  }
  void default_point(float out[3]) override { std::memcpy(out, pt, sizeof pt); }
  // Piece 3's: the scenario's vehicle, view and screen, the scripted
  // conditions, Instant Action's world.
  uint32_t seed = 0;
  Handle vehicle = 0, view = 0, screen = 0;
  Store* store = nullptr;
  int32_t mode = 0, number = 0;
  struct Obj {
    Handle id;
    uint8_t active, hostile;
    int32_t x48;
  };
  std::vector<Obj> objs;

  Handle player_vehicle() override { return vehicle; }
  void ask_voice(Handle sound, float priority) override { ev("voice " + hex(sound) + " " + hex(bits(priority))); }
  void hud_message(float seconds, int32_t id, const char* text) override {
    ev("hud " + hex(bits(seconds)) + " " + hex(id) + " " + str(text));
  }
  void hud_message_line(float seconds, const char* text) override { ev("line " + hex(bits(seconds)) + " " + str(text)); }
  Store* salvage_store() override { return store; }
  Handle view_vehicle() override { return view; }
  Handle mech_screen(Handle v) override {
    ev("screen " + hex(v));
    return screen;
  }
  void objectives_box_reset(Handle s) override { ev("box " + hex(s)); }
  // The world over the frames (test_objective_check.py's): -1 at the load.
  int frame = -1;
  static uint32_t mix(uint32_t seed, uint32_t k) {
    uint32_t h = seed * 2654435761u + k * 40503u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
  }
  uint8_t base_flag(Handle e) {
    for (const Obj& o : objs)
      if (o.id == e) return o.active;
    return (e & 0xf0) != 0;
  }
  uint8_t entity_flag(Handle e) override {
    if (frame < 0) return base_flag(e);
    return frame >= static_cast<int>(mix(seed, e) % 6) ? 0 : base_flag(e);
  }
  uint8_t node_active(Handle node) override {
    if (frame < 0) return (node >> 5) & 1;
    return (mix(seed ^ node, static_cast<uint32_t>(frame)) >> 3) & 1;
  }
  int32_t valve_value(Handle v) override {
    ev("vvalue " + hex(v));
    return static_cast<int32_t>(mix(seed ^ v, static_cast<uint32_t>(frame + 1)) % 3);
  }
  static float coord(uint32_t h) { return static_cast<float>(static_cast<int>(h % 61) - 30); }
  void object_position(Handle o, float out[3]) override {
    ev("pos " + hex(o));
    const uint32_t m = o == vehicle ? mix(seed, 0x1000 + static_cast<uint32_t>(frame))
                                    : mix(seed ^ o, 0x1100 + static_cast<uint32_t>(frame));
    out[0] = coord(m), out[1] = coord(m >> 8) / 4.0f, out[2] = coord(m >> 16);
  }
  void node_position(Handle n, float out[3]) override {
    ev("npos " + hex(n));
    const uint32_t m = mix(seed ^ n, static_cast<uint32_t>(frame));
    out[0] = coord(m), out[1] = 1.0f, out[2] = coord(m >> 16);
  }
  void nav_position(Handle nav, float out[3]) override {
    ev("navpos " + hex(nav));
    const uint32_t m = mix(nav, 1);
    out[0] = coord(m), out[1] = 0.5f, out[2] = coord(m >> 16);
  }
  // Training's world (test_objective_check.py's set_training and stubs).
  uint32_t tf(uint32_t k) const { return mix(seed, k * 0x100 + static_cast<uint32_t>(frame)); }
  Handle node_parent(Handle n) override {
    const uint32_t k = mix(seed ^ n, 0xd00 + static_cast<uint32_t>(frame));
    std::vector<Handle> c;
    for (Handle x : nodes)
      if (x < n) c.push_back(x);
    return !c.empty() && k % 3 ? c[(k >> 4) % c.size()] : 0;
  }
  Handle aim_hit_node(Handle s) override {
    ev("aim " + hex(s) + " 0");
    const uint32_t k = tf(2);
    if (nodes.empty() || k % 3 == 0) return 0;
    auto it = nodes.begin();
    std::advance(it, (k >> 4) % nodes.size());
    return *it;
  }
  float screen_speed(Handle) override {
    const uint32_t k = tf(1);
    static const float kV[4] = {0.0f, 5.0f, 12.5f, -3.0f};
    return (k >> 8) % 3 == 0 ? kV[k % 4] : static_cast<float>(static_cast<int32_t>(k % 41) - 10);
  }
  float mission_time() override { return static_cast<float>(frame * 7 % 50) + 0.5f; }
  void view_angles(float out[3]) override {
    static const float kP[3] = {0.0f, 0.3f, -0.5f};
    static const float kY[6] = {0.0f, 1.5708f, 3.14159f, -1.5708f, 0.7854f, 2.0f};
    const uint32_t a = tf(10);
    out[0] = kP[a % 3], out[1] = kY[(a >> 4) % 6], out[2] = 0.0f;
  }
  Handle vehicle_target_slot(Handle) override { return tf(3) % 4 == 0 ? 0 : 0x5107; }
  Handle contact_entity(Handle) override {
    ev("contact");
    const uint32_t i = (tf(3) >> 4) % (entities.size() + 1);
    if (i >= entities.size()) return 0;
    auto it = entities.begin();
    std::advance(it, i);
    return *it;
  }
  int32_t vehicle_aimed_part(Handle) override { return static_cast<int32_t>(tf(4) % 9) - 1; }
  void vehicle_facing(Handle, float out[3]) override {
    static const float kF[8][3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f},
                                   {0.6f, 0.0f, 0.8f}, {0.8f, 0.0f, -0.6f}, {-0.6f, 0.0f, 0.8f}, {0.28f, 0.0f, 0.96f}};
    std::memcpy(out, kF[tf(5) % 8], sizeof kF[0]);
  }
  int32_t vehicle_heat_warning(Handle) override { return static_cast<int32_t>(tf(6) & 1); }
  Handle vehicle_weapons(Handle) override { return tf(7) % 4 == 0 ? 0 : 0x3a3a; }
  int32_t weapon_group(Handle) override { return static_cast<int32_t>((tf(7) >> 4) % 6); }
  int32_t current_weapon_type(Handle) override {
    ev("wcur");
    return static_cast<int32_t>(tf(8) % 40) - 1;
  }
  uint8_t weapon_fired(Handle, int32_t w) override {
    ev("fired " + hex(w));
    return static_cast<uint8_t>(mix(seed ^ static_cast<uint32_t>(w), 0x900 + static_cast<uint32_t>(frame)) & 1);
  }
  uint8_t button(const char* what, uint32_t k, int32_t b) {
    ev(std::string(what) + " " + hex(b));
    return static_cast<uint8_t>(mix(seed ^ static_cast<uint32_t>(b), k + static_cast<uint32_t>(frame)) & 3);
  }
  uint8_t key_down(int32_t k) override { return button("keyd", 0xa00, k); }
  uint8_t joy_button(int32_t b) override { return button("joyb", 0xb00, b); }
  uint8_t mouse_button(int32_t b) override { return button("mouseb", 0xc00, b); }
  int32_t ia_mode() override { return mode; }
  int32_t ia_number() override { return number; }
  std::string message_number(int32_t id, bool has_n, int32_t n) override {
    ev("msgn " + hex(id) + (has_n ? " " + hex(n) : ""));
    return "N" + hex(id) + (has_n ? ":" + std::to_string(n) : "");
  }
  std::vector<Handle> world_objects() override {
    std::vector<Handle> v;
    for (const Obj& o : objs) v.push_back(o.id);
    return v;
  }
  const Obj& obj(Handle id) {
    for (const Obj& o : objs)
      if (o.id == id) return o;
    return objs[0];
  }
  uint8_t object_active(Handle o) override { return entity_flag(o); }
  bool object_hostile(Handle o) override {
    ev("team " + hex(o));
    return obj(o).hostile;
  }
  int32_t object_x48(Handle o) override {
    ev("x48 " + hex(o));
    return obj(o).x48;
  }
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg>) override {
    ev(std::string("unported ") + key);
    return 0;
  }
};

void store_text(std::string& o, const Store* s) {
  if (!s) {
    o += " S-";
    return;
  }
  o += " S[b" + hex(s->b04) + " f" + hex(s->full);
  for (const ConfigItem* i : s->items) {
    o += " " + hex(i->kind()) + ":" + hex(i->count) + ":" + hex(i->group) + ":" + hex(i->part) + ":" + hex(i->b10) +
         ":" + hex(i->id());
    if (i->kind() == 1) o += ":" + hex(static_cast<const WeaponItem*>(i)->tech);
    if (i->kind() == 3) o += ":" + hex(static_cast<const EquipmentItem*>(i)->tech);
    if (i->kind() == 4) o += ":" + hex(static_cast<const EngineItem*>(i)->xl);
  }
  o += "]";
}

std::string opt(const std::optional<std::string>& s) { return s ? "'" + *s + "'" : "-"; }

void obj_text(std::string& o, const Objective* b, int depth) {
  o += std::string(depth * 2, ' ') + "K" + hex(b->kind) + " nav" + hex(b->nav) + " d" + hex(b->done) + " " +
       opt(b->desc) + " v" + hex(b->valve) + "=" + hex(b->value) + " snd" + hex(b->sound) + " vis" + hex(b->visible) +
       " sil" + hex(b->silent) + " per" + opt(b->persist);
  store_text(o, b->salvage.get());
  for (const NodeSalvage& n : b->node_salvage) {
    o += " N" + hex(n.node) + "/" + hex(n.active);
    store_text(o, n.salvage.get());
  }
  switch (b->kind) {
    case Objective::kGroup: {
      auto* g = static_cast<const ObjectiveGroup*>(b);
      o += " | " + hex(g->b34) + " " + hex(g->simultaneous) + " " + hex(g->goal_count) + " " + hex(g->x3c) + "\n";
      for (const auto& c : g->items) obj_text(o, c.get(), depth + 1);
      return;
    }
    case Objective::kDestroy: {
      auto* d = static_cast<const DestroyObjective*>(b);
      o += " | " + opt(d->name) + " " + hex(d->entity) + " " + hex(d->x3c);
      break;
    }
    case Objective::kNodeChange: {
      auto* d = static_cast<const NodeChangeObjective*>(b);
      o += " | " + hex(d->node) + " " + hex(d->active);
      break;
    }
    case Objective::kReachArea: {
      auto* d = static_cast<const ReachAreaObjective*>(b);
      o += " | " + hex(d->node) + " " + hex(bits(d->point[0])) + " " + hex(bits(d->point[1])) + " " +
           hex(bits(d->point[2])) + " " + hex(bits(d->radius)) + " " + hex(bits(d->radius_sq));
      break;
    }
    case Objective::kValveChange: {
      auto* d = static_cast<const ValveChangeObjective*>(b);
      o += " | " + hex(d->watch) + " " + hex(d->target);
      break;
    }    case Objective::kReachVelocity: {
      auto* d = static_cast<const ReachVelocityObjective*>(b);
      o += " | " + hex(d->test) + " " + hex(bits(d->velocity));
      break;
    }
    case Objective::kAimAt:
      o += " | " + hex(static_cast<const AimAtObjective*>(b)->node);
      break;
    case Objective::kTarget: {
      auto* d = static_cast<const TargetObjective*>(b);
      o += " | " + opt(d->name) + " " + hex(d->entity) + " " + hex(d->part);
      break;
    }
    case Objective::kTimeLimit:
      o += " | " + hex(bits(static_cast<const TimeLimitObjective*>(b)->seconds));
      break;
    case Objective::kViewObject:
    case Objective::kFaceObject: {
      auto* d = static_cast<const FacingObjective*>(b);
      o += " | " + opt(d->name) + " " + hex(d->entity);
      break;
    }
    case Objective::kHeatWarning:
      o += " |";
      break;
    case Objective::kUserCommand: {
      auto* d = static_cast<const InputObjective*>(b);
      o += " | " + hex(d->x34) + " " + hex(d->x38) + " " + hex(d->x3c);
      break;
    }
    case Objective::kJoystickPress:
    case Objective::kKeyPress:
    case Objective::kMousePress:
      o += " | " + hex(static_cast<const InputObjective*>(b)->x34);
      break;
    case Objective::kWeaponFire:
    case Objective::kWeaponSelect:
    case Objective::kWeaponGroupSelect:
      o += " | " + hex(static_cast<const WeaponObjective*>(b)->id);
      break;
  }
  o += "\n";
}

int put(const std::string& s, char* out, int cap) {
  if (static_cast<int>(s.size()) + 1 > cap) return -1;
  std::memcpy(out, s.c_str(), s.size() + 1);
  return static_cast<int>(s.size());
}

}  // namespace

extern "C" {

// The block read (op 0) or read then freed (op 1); writes the objects and the
// calls' log. Returns 0 or 1 (what objectives_load returns), -100 without the
// archive, -1 when a buffer is too small.
int probe_objectives_run(const char* archive, const char* file, const char* block, int type, const float* pt, int op,
                         char* out, int cap, char* log, int logcap) {
  zrdr::init();
  zrdr::close_archives(true);
  if (!zrdr::open_archive(archive, true)) return -100;
  Fake h;
  h.type = type;
  std::memcpy(h.pt, pt, sizeof h.pt);
  Objectives o;
  const bool ok = objectives_load(file, block, o, h);
  std::string text = "desc " + opt(o.desc) + "\n";
  const char* names[] = {"primary", "secondary", "final", "fail"};
  const ObjectiveGroup* gs[] = {o.primary.get(), o.secondary.get(), o.final.get(), o.fail.get()};
  for (int i = 0; i < 4; ++i) {
    text += std::string(names[i]) + "\n";
    if (gs[i]) obj_text(text, gs[i], 1);
  }
  if (op == 1) {
    objectives_free(o, h);
    text += std::string("freed ") + (o.primary || o.secondary || o.final || o.fail || o.desc ? "no" : "yes") + "\n";
  }
  zrdr::close_archives(true);
  if (put(text, out, cap) < 0 || put(h.log, log, logcap) < 0) return -1;
  return ok ? 1 : 0;
}

// Piece 3: the block read, Instant Action's group added (ia >= 0: its mode;
// objs: id, active, hostile, x48 each), then `frames` frames of the
// mission's checks: each group's 0x0045e540(1), then 0x0045c180.
int probe_objective_check_run(const char* archive, const char* block, int type, const float* pt, uint32_t seed,
                              int frames, uint32_t vehicle, uint32_t view, uint32_t screen, int ia, int number,
                              const uint32_t* objs, int nobjs, char* out, int cap, char* log, int logcap) {
  zrdr::init();
  zrdr::close_archives(true);
  if (!zrdr::open_archive(archive, true)) return -100;
  Fake h;
  h.type = type;
  std::memcpy(h.pt, pt, sizeof h.pt);
  h.seed = seed, h.vehicle = vehicle, h.view = view, h.screen = screen, h.mode = ia, h.number = number;
  for (int i = 0; i < nobjs; ++i)
    h.objs.push_back({objs[4 * i], static_cast<uint8_t>(objs[4 * i + 1]), static_cast<uint8_t>(objs[4 * i + 2]),
                      static_cast<int32_t>(objs[4 * i + 3])});
  Store salvage(1);
  h.store = &salvage;
  Objectives o;
  objectives_load("objectives.zrd", block, o, h);
  if (ia >= 0) add_enemy_objectives(o.primary, o.secondary, h);
  std::unique_ptr<ObjectiveGroup>* gs[] = {&o.secondary, &o.primary, &o.final, &o.fail};
  for (int f = 0; f < frames; ++f) {
    h.frame = f;
    h.ev("frame " + hex(f));
    for (int i = 0; i < 4; ++i)
      if (*gs[i]) h.ev("check " + hex(i) + " " + hex((*gs[i])->check_group(h, 1)));
    h.ev("visible " + hex(objectives_visible(o.primary.get(), o.final.get(), o.fail.get())));
  }
  std::string text = "desc " + opt(o.desc) + "\n";
  const char* names[] = {"primary", "secondary", "final", "fail"};
  const ObjectiveGroup* g4[] = {o.primary.get(), o.secondary.get(), o.final.get(), o.fail.get()};
  for (int i = 0; i < 4; ++i) {
    text += std::string(names[i]) + "\n";
    if (g4[i]) obj_text(text, g4[i], 1);
  }
  text += "salvage";
  store_text(text, &salvage);
  text += "\n";
  zrdr::close_archives(true);
  if (put(text, out, cap) < 0 || put(h.log, log, logcap) < 0) return -1;
  return 0;
}

}  // extern "C"
