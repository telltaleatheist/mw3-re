// Pirate's Moon's field bases (docs/pm-tests.md): the death of the last base alive aborts the dock in progress
// (0x0045a250 by 0x0045b100), and the dock's aborts say their line only while a base is alive (contact 0x0045bb30,
// repair 0x0045c720, by 0x0045b0d0). MW3 always said them, and its last base's death left the dock running.
#include <vector>

#include "check.h"
#include "mech3/mech_fieldbase.h"

namespace {
struct Hooks : mech3::FieldBaseHooks {
  std::vector<mech3::Handle> said;
  void say(mech3::Handle sound) override { said.push_back(sound); }
  float game_time() override { return 100.0f; }
  uint32_t frame() override { return 0; }
  void marker_clear() override {}
  void base_die(mech3::BasicVehicle& v) override { v.active = false; }
  mech3::Handle campaign() override { return nullptr; }
};

// The registry's three bases, `alive` of them active, its contact and repair lines set, its states `front` alone.
struct Bench {
  Hooks hooks;
  mech3::FieldBase b[3];
  mech3::FieldBases& r = mech3::field_bases();
  std::list<mech3::DockStep*> saved_states = r.states;
  int32_t saved_count = r.count;
  mech3::FieldBase* saved_bases[3] = {r.bases[0], r.bases[1], r.bases[2]};
  Bench(int alive, mech3::DockStep* front) {
    mech3::set_field_base_hooks(&hooks);
    for (int i = 0; i < 3; ++i) b[i].active = i < alive, r.bases[i] = &b[i];
    r.count = 3;
    r.sounds[5] = reinterpret_cast<mech3::Handle>(0x55);
    r.sounds[9] = reinterpret_cast<mech3::Handle>(0x99);
    r.states.clear();
    if (front) r.states.push_back(front);
    mech3::field_bases_quiet_until() = 0.0f;
  }
  ~Bench() {
    r.states = saved_states;
    r.count = saved_count;
    for (int i = 0; i < 3; ++i) r.bases[i] = saved_bases[i];
    mech3::set_field_base_hooks(nullptr);
  }
};
}  // namespace

TEST(pm_dock_contact_abort_speaks_only_with_a_base_alive) {
  for (const int alive : {2, 0}) {
    mech3::FieldBases& r = mech3::field_bases();
    r.contact.sound = nullptr;
    Bench t(alive, &r.contact);
    r.contact.abort();
    EXPECT(t.hooks.said.size() == (alive ? 1u : 0u));
    if (alive) EXPECT(t.hooks.said[0] == reinterpret_cast<mech3::Handle>(0x55));
  }
}

TEST(pm_dock_repair_abort_sets_its_line_only_with_a_base_alive) {
  for (const int alive : {1, 0}) {
    mech3::FieldBases& r = mech3::field_bases();
    r.repair.sound = nullptr;
    Bench t(alive, &r.repair);
    r.repair.abort();
    EXPECT(r.repair.sound == (alive ? reinterpret_cast<mech3::Handle>(0x99) : nullptr));
  }
}

TEST(pm_last_field_base_death_aborts_the_dock) {
  for (const int alive : {2, 1}) {
    mech3::FieldBases& r = mech3::field_bases();
    r.contact.sound = nullptr;
    Bench t(alive, &r.contact);
    mech3::field_base_die(t.b[0]);  // one dies: the last when it was the only one
    const bool aborted = r.states.empty() || r.states.front() != &r.contact;
    EXPECT(aborted == (alive == 1));
    EXPECT(t.hooks.said.empty());  // the last one's abort says nothing: no base alive
  }
}
