// The PM AI's hooks on the program (zai_wire_pm): what each asks reaches BrainHooks, and a call
// with no answer yet is counted, not dropped.
#include <vector>

#include "check.h"
#include "mech3/zai_wire_pm.h"

using namespace mech3::ai;

namespace {
struct Fake : BrainHooks {
  std::vector<Contact*> list;
  void position(zgame::Handle e, float out[3]) override { out[0] = float(e), out[1] = 2, out[2] = 3; }
  void forward(zgame::Handle, float* x, float* z) override { *x = 0.6f, *z = 0.8f; }
  const std::vector<Contact*>* contacts(zgame::Handle v) override { return v ? &list : nullptr; }
  zgame::Handle contact_entity(const Contact* c) override { return c->seen; }
};
struct Installed {
  BrainHooks* old = &brain_hooks();
  explicit Installed(BrainHooks* h) { set_brain_hooks(h); }
  ~Installed() { set_brain_hooks(old); }
};
}  // namespace

TEST(pm_wire_steer_asks_the_brain_hooks) {
  Fake f;
  Installed in(&f);
  WiredSteerPm w;
  float p[3], x, z;
  w.position(7, p);
  EXPECT(p[0] == 7.0f && p[1] == 2.0f && p[2] == 3.0f);
  w.forward(7, &x, &z);
  EXPECT(x == 0.6f && z == 0.8f);
  Contact c;
  c.seen = 9, c.dist2 = 25.0f;
  f.list.push_back(&c);
  EXPECT(w.contacts(1).size() == 1 && w.contacts(1)[0] == &c);
  EXPECT(w.contacts(0).empty());  // no target part, no contacts
  EXPECT(w.contact_entity(&c) == 9);
  EXPECT(w.contact_dist2(&c) == 25.0f);
}

TEST(pm_wire_counts_a_call_with_no_answer) {
  Fake f;
  Installed in(&f);
  pm_hooks_missing_clear();
  WiredSteerPm w;
  EXPECT(!w.halted(1));
  EXPECT(!w.halted(1));
  EXPECT(!w.targetable(1));
  EXPECT(!w.order_steers(nullptr));
  const auto& m = pm_hooks_missing();
  EXPECT(m.at("vehicle.vt+0x150") == 2);
  EXPECT(m.at("entity.vt+0x48") == 1);
  EXPECT(m.at("0x004fa470") == 1);
  pm_hooks_missing_clear();
}
