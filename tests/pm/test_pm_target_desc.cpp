// PM's target description (0x004e2820): a record with an mpoption gives its description only when the network
// game counts that base (0x00437920); otherwise the target is made with the defaults (0x004e3190 makes it).
#include <algorithm>
#include <cstdint>
#include <list>

#include "check.h"
#include "mech3/zai_anim_owner.h"
#include "mech3/zai_target.h"
#include "mech3/zrdraiv.h"

using namespace mech3::ai;

namespace {
struct Bases : TargetHooks {
  int32_t counts = 0;  // the base the game counts, 0 none
  int32_t asked = 0;
  Handle anim_of_node(const char*, int32_t, Handle) override { return 0x77; }
  bool mp_option(int32_t option) override { return ++asked, option == counts; }
};
}  // namespace

TEST(pm_target_desc_keeps_a_base_only_when_the_game_counts_it) {
  Bases b;
  TargetHooks& old = target_hooks();
  set_target_hooks(&b);
  TargetRec rec;
  rec.destroy_anim = "base_destroy";
  rec.name = "Base";
  rec.flags = 1, rec.health = 500.0f;
  struct Case { int32_t option, counts; bool taken; };
  for (const Case c : {Case{-1, 0, true}, Case{1, 1, true}, Case{1, 0, false}, Case{2, 1, false}, Case{2, 2, true}}) {
    rec.mpoption = c.option;
    b.counts = c.counts, b.asked = 0;
    TargetDesc d;
    EXPECT(desc_build(d, 0x1234, &rec));  // made either way
    EXPECT(b.asked == (c.option >= 0 ? 1 : 0));
    EXPECT((d.destroy_anim == 0x77) == c.taken);
    EXPECT(d.mpoption == (c.taken ? c.option : -1));
    EXPECT(d.health == (c.taken ? 500.0f : 100.0f));
    desc_free(d);
  }
  set_target_hooks(&old);
}

namespace {
struct Bare : TargetHooks {
  void entity_init(Handle) override {}
  void entity_end(Handle) override {}
};
}  // namespace

// Pirate's Moon 0x004e30d0 (MW3's 0x004d2cd0): a collision's damage, but none to a multiplayer base (an mpoption).
TEST(pm_target_collision_spares_a_multiplayer_base) {
  Bare b;
  TargetHooks& old = target_hooks();
  set_target_hooks(&b);
  TargetRec rec;
  rec.height = 5.0f;
  for (const int32_t option : {-1, 1}) {
    TargetDesc d;
    d.health = d.max_health = 100.0f, d.mpoption = option;
    auto* t = new CZAiTarget(0x4321, 0, d, &rec, false);
    EXPECT(t->on_collide(30.0f) == (option < 0 ? 70.0f : 100.0f));
    delete t;
  }
  set_target_hooks(&old);
}

namespace {
struct Binder : AnimOwnerHooks {
  int sets = 0;
  AnimCallback anim_callback(Handle) override { return AnimCallback::none; }
  void anim_set_record(Handle, AnimOwnerRec*) override { ++sets; }
};
}  // namespace

// Pirate's Moon 0x004e50b0 (MW3's 0x004d4c80): an animation whose callback events are none of the owner (0, 0x38)
// or valve (0x2a..0x31) ones still gets a record, unless they are 0x32..0x34; MW3 gave one only for 0x39..0x3c.
TEST(pm_anim_binder_takes_any_other_event) {
  Binder h;
  AnimOwnerHooks& old = anim_owner_hooks();
  set_anim_owner_hooks(&h);
  const std::list<AnimOwnerRec*> kept = anim_owner_records();
  struct Case { int32_t value; bool taken; };
  for (const Case c : {Case{0x39, true}, Case{0x10, true}, Case{0x45, true}, Case{0x32, false}, Case{0x34, false}}) {
    AnimBinding b;
    h.sets = 0;
    anim_owner_bind_event(b, 0x500, c.value);
    EXPECT(anim_owner_bind_commit(b) == c.taken);
    EXPECT(h.sets == (c.taken ? 1 : 0));
  }
  for (AnimOwnerRec* r : anim_owner_records())
    if (std::find(kept.begin(), kept.end(), r) == kept.end()) delete r;
  anim_owner_records() = kept;
  set_anim_owner_hooks(&old);
}
