// The objectives' reading (mech_objective.cpp) for test_objectives.py: a
// block of an objectives file read through fake engine calls that the test
// gives the original too, then the objects written out as text.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

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

// The same fakes as test_objectives.py's stubs.
class Fake : public ObjectiveHooks {
 public:
  std::string log;
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
    return h % 5 == 0 ? 0 : 0x6000000 | (h & 0xffffff);
  }
  uint8_t entity_flag(Handle e) override { return (e & 0xf0) != 0; }
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
    return h % 6 == 0 ? 0 : 0x8000000 | (h & 0xffffff);
  }
  Handle subnode_find(Handle node, const char* name) override {
    ev("sub " + hex(node) + " " + str(name));
    const uint32_t h = fnv(name) ^ node;
    return h % 6 == 0 ? 0 : 0x9000000 | (h & 0xffffff);
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
  void report(int32_t level, int32_t line, const char*, const char*, Handle) override {
    ev("zerr " + hex(level) + " " + hex(line));
  }
  void default_point(float out[3]) override { std::memcpy(out, pt, sizeof pt); }
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg>) override {
    ev(std::string("unported ") + key);
    return 0;
  }
};

uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

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
    }
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

}  // extern "C"
