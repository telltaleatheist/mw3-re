// The mechs' sun shading (mech_shade.cpp) for test_shade.py: a script of
// operations on shades over a fake world that the test gives the original
// too; the calls logged, the state written out as text.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "mech3/mech_shade.h"

namespace {

using namespace mech3;
using zgame::Handle;

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

// A mech of the script: its node, centroid, parts (node, level), shade.
struct FakeMech {
  Handle id, node, centroid;
  std::vector<std::pair<Handle, ShadeLevel>> parts;
  bool has_parts;
  SunShade shade;
  bool made = false;
};

struct World {
  Handle sun = 0;  // 0: no sun
  float diffuse = 1, dir[3] = {0, -1, 0};
  int32_t frame = 0;
  float time = 0;
  std::vector<FakeMech> mechs;
  // centroid -> (stale flag, position)
  std::map<Handle, std::pair<int, std::vector<float>>> cpos;
  std::map<Handle, int32_t> hits;  // the segment's answer by mech node: 0 none, 1 hit, 2 found but empty
  std::map<Handle, uint8_t> flags;
  uint8_t walk = 0;
  int32_t lights = 1, frozen = 0;
};

class Fake : public ShadeHooks {
 public:
  World* w = nullptr;
  std::string log;
  void ev(const std::string& s) { log += s + "\n"; }
  Handle world_sun() override { return w->sun; }
  float sun_diffuse(Handle) override { return w->diffuse; }
  void set_sun_diffuse(Handle, float v) override { w->diffuse = v; }
  void sun_direction(Handle, float out[3]) override { std::memcpy(out, w->dir, 12); }
  FakeMech* mech(Handle id) {
    for (FakeMech& m : w->mechs)
      if (m.id == id) return &m;
    return nullptr;
  }
  Handle entity_node(Handle e) override { return mech(e)->node; }
  Handle entity_centroid(Handle e) override { return mech(e)->centroid; }
  void node_world_cached(Handle node, float out[3]) override {
    auto& c = w->cpos[node];
    if (c.first) {
      ev("refresh " + hex(node));
      c.first = 0;
    }
    std::memcpy(out, c.second.data(), 12);
  }
  void node_intersect(Handle node, int32_t on) override {
    ev("intersect " + hex(node) + " " + hex(on));
    if (!on) last_node = node;
  }
  void query_mode(int32_t on) override { ev("mode " + hex(on)); }
  void query_exclude(uint32_t m) override { ev("exclude " + hex(m)); }
  Handle last_node = 0;
  int32_t segment(const float a[3], const float b[3], uint8_t* first) override {
    std::string s = "segment";
    for (int i = 0; i < 3; ++i) s += " " + hex(bits(a[i]));
    for (int i = 0; i < 3; ++i) s += " " + hex(bits(b[i]));
    ev(s);
    const int32_t h = w->hits[last_node];
    *first = h == 1;
    return h == 0 ? 1 : 0;
  }
  void node_flags(Handle node, uint8_t on) override {
    ev("flags " + hex(node) + " " + hex(on));
    w->flags[node] = on;
  }
  int32_t frame() override { return w->frame; }
  void walk_callbacks(bool on) override {
    ev(std::string("walk ") + (on ? "1" : "0"));
    w->walk = on;
  }
  void static_lights(int32_t on) override { w->lights = on; }
  void stamps_frozen(int32_t on) override { w->frozen = on; }
  ShadeLevel* part_level(Handle node) override {
    for (FakeMech& m : w->mechs)
      for (auto& p : m.parts)
        if (p.first == node) return &p.second;
    return nullptr;
  }
  // 0x0040e9d0: a part's node gives its mech too.
  ShadeLevel* mech_level(Handle node) override {
    for (FakeMech& m : w->mechs) {
      bool mine = m.node == node;
      for (auto& p : m.parts) mine |= p.first == node;
      if (mine && m.made) return &m.shade.level;
    }
    return nullptr;
  }
  int32_t part_count(SunShade& s) override {
    for (FakeMech& m : w->mechs)
      if (&m.shade == &s) return static_cast<int32_t>(m.parts.size());
    return 0;
  }
  ShadeLevel* part(SunShade& s, int32_t i, Handle* node) override {
    for (FakeMech& m : w->mechs)
      if (&m.shade == &s) {
        *node = m.parts[i].first;
        return &m.parts[i].second;
      }
    return nullptr;
  }
  int32_t entity_count() override { return static_cast<int32_t>(w->mechs.size()); }
  SunShade* entity_shade(int32_t i) override { return w->mechs[i].made ? &w->mechs[i].shade : nullptr; }
  float game_time() override { return w->time; }
};

std::string level_text(const ShadeLevel& l) {
  return hex(bits(l.saved)) + "/" + hex(bits(l.target)) + "/" + hex(bits(l.current));
}

}  // namespace

extern "C" {

// The mechs: 4 words each (id, node, centroid, part count; the parts' nodes
// are node + 1 + i, with parts list: has_parts = count >= 0). The ops: 8
// words each (code, a, b, c, d, e, f, g; test_shade.py's). Writes the log
// and the state.
int probe_shade_run(const uint32_t* mechs, int nmechs, const uint32_t* ops, int nops, char* out, int cap,
                    char* log, int logcap) {
  World w;
  for (int i = 0; i < nmechs; ++i) {
    FakeMech m;
    m.id = mechs[4 * i], m.node = mechs[4 * i + 1], m.centroid = mechs[4 * i + 2];
    const int32_t n = static_cast<int32_t>(mechs[4 * i + 3]);
    m.has_parts = n >= 0;
    for (int k = 0; k < n; ++k) m.parts.push_back({m.node + 1 + k, ShadeLevel{0.5f, 0.25f, 0.75f}});
    w.mechs.push_back(m);
    w.cpos[m.centroid] = {0, {0, 0, 0}};
  }
  Fake h;
  h.w = &w;
  set_shade_hooks(&h);
  shade_globals() = ShadeGlobals{};
  for (int k = 0; k < nops; ++k) {
    const uint32_t* o = &ops[8 * k];
    auto f = [&](int i) { return zgame::from_bits(o[i]); };
    switch (o[0]) {
      case 0: {  // make mech a's shade
        FakeMech& m = w.mechs[o[1]];
        shade_init(m.shade, m.id, m.has_parts ? m.id + 0x380 : 0);
        m.made = true;
        h.ev("made " + hex(m.id));
        break;
      }
      case 1: shade_set_option(static_cast<uint8_t>(o[1])); break;
      case 2: shade_apply(); break;
      case 3: shade_off(); break;
      case 4: {  // a frame: each made mech's update, mech order
        w.frame = static_cast<int32_t>(o[1]);
        for (FakeMech& m : w.mechs)
          if (m.made) shade_update(m.shade, f(2));
        break;
      }
      case 5:
      case 6:
        if (o[0] == 5) shade_before(o[1]);
        else shade_after(o[1]);
        h.ev("drawn " + hex(o[1]) + " " + hex(bits(w.diffuse)) + " " + hex(w.lights) + " " + hex(w.frozen));
        break;
      case 7: shade_reset(w.mechs[o[1]].shade); break;
      case 8:  // the world: sun, diffuse, direction
        w.sun = o[1], w.diffuse = f(2), w.dir[0] = f(3), w.dir[1] = f(4), w.dir[2] = f(5);
        break;
      case 9:  // a centroid: stale flag, position
        w.cpos[o[1]] = {static_cast<int>(o[2]), {f(3), f(4), f(5)}};
        break;
      case 10: w.hits[o[1]] = static_cast<int32_t>(o[2]); break;  // a mech node's segment answer
      case 11: w.time = f(1); break;
      default: break;
    }
  }
  const ShadeGlobals& g = shade_globals();
  std::string t = "globals " + hex(g.sun) + " " + hex(g.on) + " " + hex(g.count) + " " + hex(g.turn) + " " +
                  hex(static_cast<uint32_t>(g.frame)) + " " + hex(g.option) + " " + hex(g.result) + "\n";
  t += "sun " + hex(bits(w.diffuse)) + " lights " + hex(w.lights) + " frozen " + hex(w.frozen) + "\n";
  for (FakeMech& m : w.mechs) {
    t += "mech " + hex(m.id);
    if (m.made)
      t += " " + hex(m.shade.differs) + " " + level_text(m.shade.level) + " turn " + hex(m.shade.turn);
    for (auto& p : m.parts) t += " part " + level_text(p.second);
    t += "\n";
  }
  set_shade_hooks(nullptr);
  if (static_cast<int>(t.size()) + 1 > cap || static_cast<int>(h.log.size()) + 1 > logcap) return -1;
  std::memcpy(out, t.c_str(), t.size() + 1);
  std::memcpy(log, h.log.c_str(), h.log.size() + 1);
  return 0;
}

}  // extern "C"
