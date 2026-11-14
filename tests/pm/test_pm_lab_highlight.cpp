// Pirate's Moon's mech lab part highlight (0x00451730, the port's lab_highlight with the table's
// lab_head_with_torso): the torso's sections (1, 7, 8) light the torso and the head green (0.3), the arms on at 0;
// the head's own section (6) lights nothing. MW3's 0x004504d0 lights the head for 6 alone and keeps it at 0 with
// the torso. The other sections are the same in both.
#include <cstdio>
#include <string>

#include "check.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zgame/zgame_table.h"
#include "mech3/lab_view.h"
#include "pm/pm_game.h"

TEST(pm_lab_highlight_lights_the_head_with_the_torso) {
  static const char* const kParts[8] = {"torso", "larm", "rarm", "lthigh", "lfoot", "rthigh", "rfoot", "head"};
  zcls::Node* root = zcls::object3d_create();
  zcls::Node* part[8] = {};
  for (int i = 0; i < 8; ++i) {
    part[i] = zcls::object3d_create();
    zcls::node_set_name(part[i], kParts[i]);
    zcls::node_link(root, part[i]);
  }
  // Each part as on (blend bit) and weight: "-" off, "0" on at 0, "3" on at 0.3.
  auto lit = [&] {
    std::string s;
    for (zcls::Node* n : part) {
      const auto* d = static_cast<const zcls::Object3d*>(n->data);
      s += !(d->get<uint32_t>(0) & 4) ? '-' : d->get<float>(0x14) > 0.0f ? '3' : '0';
    }
    return s;
  };
  //                      torso larm rarm lthigh lfoot rthigh rfoot head
  const char* const pm[9] = {"--------", "300----3", "-3------", "--3-----", "---33---", "-----33-", "--------", "300----3", "300----3"};
  const char* const mw3[9] = {"--------", "300----0", "-3------", "--3-----", "---33---", "-----33-", "-------3", "300----0", "300----0"};
  for (const bool is_pm : {true, false}) {
    zgame::set_game(is_pm ? pm::table() : zgame::mw3_table());
    for (int s = 0; s <= 8; ++s) {
      mech3::lab_highlight(root, s);
      const std::string got = lit();
      if (got != (is_pm ? pm : mw3)[s]) std::printf("  %s section %d: %s\n", is_pm ? "PM" : "MW3", s, got.c_str());
      EXPECT(got == (is_pm ? pm : mw3)[s]);
    }
  }
  zgame::set_game(pm::table());
}
