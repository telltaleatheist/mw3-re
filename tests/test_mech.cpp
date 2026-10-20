#include "check.h"
#include "mech3/mech.h"

TEST(heat_sinks_rate_and_loss) {
  mech3::HeatSinks s{3, 10, false};  // tech, count, doubles
  EXPECT(s.rate() == 1.25f);
  s.doubles = true;
  EXPECT(s.rate() == 2.5f);
  EXPECT(!s.lose(3));
  EXPECT(s.count == 7);
  EXPECT(!s.lose(20));
  EXPECT(s.count == 0);
}

TEST(mech_add_heat_accumulates) {
  mech3::Mech m{};
  m.add_heat(0.25f);
  m.add_heat(0.5f);
  EXPECT(m.heat_added == 0.75f);
}
