// Pirate's Moon's MCB waynet (docs/pm-tests.md): next and previous step over the points not shown (a hidden point,
// +0x11, or no point; 0x0047bd00 / 0x0047bd60), and the nearest is the nearest shown point, none selected when none
// is (0x0047be40); MW3 stepped by index and asked the net's nearest step, falling back to the selected point.
#include "check.h"
#include "mech3/mech_waynet.h"

namespace {
// Element 0 the default behaviour (no point), then four dock points at x 0, 10, 20, 30; `hidden` a mask of them.
mech3::McbWaynet* make(uint32_t hidden) {
  auto* w = new mech3::McbWaynet(0x7e01, "test_mcb");
  for (int i = 0; i < 4; ++i) {
    const float p[3] = {10.0f * static_cast<float>(i), 0, 0};
    w->elems.push_back(new mech3::WaynetPoint(nullptr, nullptr, p, nullptr, 0, 1, 0, 0, 0, (hidden >> i) & 1 ? 1 : 0));
  }
  return w;
}
}  // namespace

TEST(pm_mcb_waynet_steps_over_hidden_points) {
  mech3::McbWaynet* w = make(0b0110);  // points 1 and 2 (elements 2, 3) hidden
  w->current = 1;                      // point 0
  w->next();
  EXPECT(w->current == 4);             // over elements 2 and 3 to point 3
  w->next();
  EXPECT(w->current == 1);             // over element 0 (no point) round to point 0
  w->prev();
  EXPECT(w->current == 4);
  delete w;
  mech3::McbWaynet* all = make(0b1111);
  all->current = 2;
  all->next();
  EXPECT(all->current == 2);           // none shown: round to where it started
  delete all;
}

TEST(pm_mcb_waynet_nearest_is_a_shown_point) {
  mech3::McbWaynet* w = make(0b0010);  // point 1 (at x 10) hidden
  w->current = 1;
  const float near1[3] = {11.0f, 0, 0};
  w->nearest(near1);
  EXPECT(w->current == 3);             // point 2 at x 20, not the hidden one at 10
  EXPECT(w->picked == w->elem(3) && w->passed == 0);
  delete w;
  mech3::McbWaynet* all = make(0b1111);
  all->current = 2;
  all->nearest(near1);
  EXPECT(all->current == 2);           // none shown: nothing selected
  delete all;
}
