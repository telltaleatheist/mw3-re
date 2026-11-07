// The PM AI's hooks on the program (zai_wire_pm): what each asks reaches BrainHooks, and a call
// with no answer yet is counted, not dropped.
#include <vector>

#include "check.h"
#include "mech3/zai_wire_pm.h"
#include "mech3/zainet.h"

using namespace mech3::ai;

namespace {
struct Fake : BrainHooks {
  std::vector<Contact*> list;
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
  void position(zgame::Handle e, float out[3]) override { out[0] = float(e), out[1] = 2, out[2] = 3; }
  void forward(zgame::Handle, float* x, float* z) override { *x = 0.6f, *z = 0.8f; }
  const std::vector<Contact*>* contacts(zgame::Handle v) override { return v ? &list : nullptr; }
  zgame::Handle contact_entity(const Contact* c) override { return c->seen; }
  bool airborne(zgame::Handle v) override { return v == 5; }
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
  EXPECT(w.halted(5) && !w.halted(6));
}

TEST(pm_wire_counts_a_call_with_no_answer) {
  Fake f;
  Installed in(&f);
  pm_hooks_missing_clear();
  WiredSteerPm w;
  EXPECT(!w.targetable(1));
  EXPECT(!w.targetable(1));
  EXPECT(!w.order_steers(nullptr));
  const auto& m = pm_hooks_missing();
  EXPECT(m.at("entity.vt+0x48") == 2);
  EXPECT(m.at("0x004fa470") == 1);
  pm_hooks_missing_clear();
}

TEST(pm_wire_a_brain_keeps_one_record) {
  pm_wire_clear();
  Fake f;
  Installed in(&f);
  BuiltinNet net(7, 0);
  AiParams rec;
  rec.put<int32_t>(0x3c, 3);
  rec.put<float>(0x1c, 900.0f);
  rec.put<uint8_t>(0xcc, 5);
  Brain b(&net, rec);
  b.xf4 = 12.0f, b.x2a4 = 1;
  PmBrain& r = pm_brain(b);
  EXPECT(r.tick.mode == 3 && r.tick.sight == 900.0f && r.tick.engage_flags == 5);
  EXPECT(r.tick.wait_until == 12.0f && r.tick.runs_down == 1);
  r.tick.mode = 0;  // PM's frame writes it: a second ask must not seed it again
  b.params.put<int32_t>(0x3c, 7);
  EXPECT(pm_brain(b).tick.mode == 0);
  pm_forget(b, 0);
  EXPECT(pm_brain(b).tick.mode == 7);
  pm_wire_clear();
}

TEST(pm_wire_targets_go_by_the_vehicle) {
  pm_wire_clear();
  pm_targets(4).marked = 9;
  EXPECT(pm_targets(4).marked == 9 && pm_targets(5).marked == 0);
  pm_wire_clear();
}

TEST(pm_wire_ids_stand_for_pointers) {
  pm_wire_clear();
  int a = 0, b = 0;
  const uint32_t ia = pm_id(&a), ib = pm_id(&b);
  EXPECT(ia && ib && ia != ib);
  EXPECT(pm_id(&a) == ia && pm_ptr(ia) == &a && pm_ptr(ib) == &b);
  EXPECT(pm_id(nullptr) == 0 && pm_ptr(0) == nullptr);
  pm_ids_forget(&a);
  EXPECT(pm_ptr(ia) == nullptr);
  EXPECT(pm_id(&a) != ia);  // a freed id is not given to another object
  pm_wire_clear();
}
