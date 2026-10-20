// Probes for test_ai_orders.py (docs/ai.md 17.2): the brain's order stack
// (mech3/zai_brain) over fake nets and behaviours. Their parts and states
// report every call to the shared callback (calltrace.py), numbered in the
// order they are made, as the emulator's fakes do.
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "mech3/zai_brain.h"
#include "mech3/zai_steps.h"
#include "probe_app.h"

namespace {
using probe_app::call;
namespace ai = mech3::ai;

uint32_t g_serial = 0;  // parts and states made, as the emulator counts them
uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

class FakePart : public ai::NetPart {
 public:
  FakePart(ai::Order* o, ai::AiNet* n, int32_t id) : NetPart(o), target(n), net_id(id), serial(++g_serial) {
    call("net.vt+0x5c", {net_id, serial});
  }
  ~FakePart() override { call("part.vt+0x0", {serial, 0}); }
  void run(float dt) override { call("part.vt+0x4", {serial, bits(dt)}); }
  ai::AiNet* net() const override {
    call("part.vt+0x8", {serial});
    return target;
  }
  ai::AiNet* target;
  int32_t net_id;
  uint32_t serial;
};

class FakeState : public ai::BehaviourState {
 public:
  FakeState(ai::Order* o, ai::Behaviour* b, int32_t id) : BehaviourState(o, b), beh_id(id), serial(++g_serial) {
    call("beh.vt+0x8c", {beh_id, serial});
  }
  ~FakeState() override { call("state.vt+0x0", {serial, 0}); }
  bool may_break_in() const override { return call("state.vt+0x4", {serial}) & 0xff; }
  bool may_fire() const override { return call("state.vt+0x8", {serial}) & 0xff; }
  void suspend() override { call("state.vt+0xc", {serial}); }
  void resume() override { call("state.vt+0x10", {serial}); }
  void run(float dt) override { call("state.vt+0x14", {serial, bits(dt)}); }
  int32_t beh_id;
  uint32_t serial;
};

class FakeNet : public ai::CompiledNet {
 public:
  FakeNet(int32_t id, bool compiled) : CompiledNet(id, 0), compiled_(compiled) {}
  ai::AiNet* as_compiled() override { return compiled_ ? this : nullptr; }
  ai::NetPart* make_part(ai::Order& o) override { return new FakePart(&o, this, get_id()); }
  bool compiled_;
};

struct FakeBehaviour : ai::Behaviour {
  explicit FakeBehaviour(int32_t i) : id(i) {}
  ai::BehaviourState* make_state(ai::Order& o) override { return new FakeState(&o, this, id); }
  int32_t id;
};

// Nets 1..4 compiled, 5 a net that is not compiled, 100 the built-in one;
// behaviours 1..4, 100 the default one.
std::vector<std::unique_ptr<FakeNet>> g_nets;
std::vector<std::unique_ptr<FakeBehaviour>> g_behs;
std::unique_ptr<ai::Brain> g_brain;

FakeNet* net_of(int32_t id) {
  for (auto& n : g_nets)
    if (n->get_id() == id) return n.get();
  return nullptr;
}
FakeBehaviour* beh_of(int32_t id) {
  for (auto& b : g_behs)
    if (b->id == id) return b.get();
  return nullptr;
}

struct Hooks : ai::BrainHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return call(key, args); }
  ai::AiNet* builtin_net() override { return net_of(100); }
  ai::Behaviour* default_behaviour() override { return beh_of(100); }
};
Hooks g_hooks;

ai::Order* order_at(int32_t i) {
  if (i < 0 || i >= static_cast<int32_t>(g_brain->orders.size())) return nullptr;
  return &g_brain->orders[static_cast<size_t>(i)];
}
int32_t index_of(const ai::Order* o) {
  for (size_t i = 0; i < g_brain->orders.size(); ++i)
    if (&g_brain->orders[i] == o) return static_cast<int32_t>(i);
  return -1;
}
}  // namespace

extern "C" {

// A brain with no orders (its constructor's first order dropped unseen).
void probe_ao_new() {
  ai::set_brain_hooks(&g_hooks);
  probe_app::Mute mute;
  g_brain.reset();
  g_nets.clear();
  g_behs.clear();
  for (int32_t i = 1; i <= 4; ++i) g_nets.push_back(std::make_unique<FakeNet>(i, true));
  g_nets.push_back(std::make_unique<FakeNet>(5, false));
  g_nets.push_back(std::make_unique<FakeNet>(100, true));
  for (int32_t i : {1, 2, 3, 4, 100}) g_behs.push_back(std::make_unique<FakeBehaviour>(i));
  ai::AiParams rec;
  g_brain = std::make_unique<ai::Brain>(net_of(1), rec);
  g_brain->orders.clear();
  g_serial = 0;
}

void probe_ao_done() {
  probe_app::Mute mute;
  g_brain.reset();
  ai::set_brain_hooks(nullptr);
}

// One operation (test_ai_orders.py OPS); nets and behaviours by id, 0 none.
// The answer: a bool, an order's index, or 0.
int32_t probe_ao_op(int32_t op, int32_t a, int32_t b, uint32_t f) {
  ai::Brain& br = *g_brain;
  float dt;
  std::memcpy(&dt, &f, 4);
  switch (op) {
    case 0:  // 0x004e9d80, then 0x004e80b0 on the new order
      br.push_order();
      return br.current().reset(net_of(a), beh_of(b));
    case 1: return br.push(net_of(a), beh_of(b));
    case 2: return br.push_net(net_of(a));
    case 3: return br.push_behaviour(beh_of(a));
    case 4: return br.remake(net_of(a));
    case 5: return br.current().set_behaviour(beh_of(a));
    case 6: return br.current().reset(net_of(a), beh_of(b));
    case 7: br.pop(); return 0;
    case 8: return br.pop_if(net_of(a));
    case 9: return br.pop_if_object(net_of(a));
    case 10: return br.push_object(net_of(a));
    case 11: return br.clear_if_object(net_of(a));
    case 12: return index_of(br.top_unmarked());
    case 13: br.unwind_to(order_at(a)); return 0;
    case 14: return br.stop();
    case 15: return br.may_fire();
    case 16: br.current().run(dt); return 0;
    case 17: br.current().pop(); return 0;
    case 18:
      if (ai::Order* o = order_at(a)) o->x04 = static_cast<uint8_t>(b);
      return 0;
    default: return -1;
  }
}

// Set the current order's +0x14..+0x2f (the words given).
void probe_ao_fields(const uint32_t* w) {
  ai::Order& o = g_brain->current();
  o.x14 = static_cast<int32_t>(w[0]);
  std::memcpy(o.x18, w + 1, 12);
  std::memcpy(o.x24, w + 4, 12);
}

// The stack, first to last: per order x04, owner is the brain, its part's
// serial and net id, its state's serial and behaviour id, +0x14..+0x2f (7
// words): 13 words. The count.
int32_t probe_ao_stack(uint32_t* out, int32_t cap) {
  int32_t n = 0;
  for (const ai::Order& o : g_brain->orders) {
    if ((n + 1) * 13 > cap) return -1;
    uint32_t* w = out + 13 * n++;
    const auto* p = dynamic_cast<const FakePart*>(o.net.get());
    const auto* s = dynamic_cast<const FakeState*>(o.state.get());
    w[0] = o.x04;
    w[1] = o.owner == g_brain.get();
    w[2] = p ? p->serial : 0, w[3] = p ? static_cast<uint32_t>(p->net_id) : 0;
    w[4] = s ? s->serial : 0, w[5] = s ? static_cast<uint32_t>(s->beh_id) : 0;
    w[6] = static_cast<uint32_t>(o.x14);
    std::memcpy(w + 7, o.x18, 12);
    std::memcpy(w + 10, o.x24, 12);
  }
  return n;
}

}  // extern "C"
