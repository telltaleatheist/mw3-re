// Pirate's Moon's per-node light override (its model draw 0x00507f30): a node whose +0x2c has bit 7 (ambient,
// bits 9..16) or bit 8 (diffuse, bits 17..24) sets the directed light's for its subtree, the bake's stamp held
// meanwhile (0x004ff730 / 0x004ff750), and puts them back after. The 24 mechlib nodes carrying it are the
// cockpit grills, +0x2c 0xb281: an ambient of 0x59.
#include <cstdio>

#include "check.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/light.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zmodel/gmod_light.h"
#include "pm/gamez/zclass/light.h"
#include "pm/pm_game.h"

TEST(pm_node_light_override_borrows_the_sun_for_its_subtree) {
  zgame::set_game(pm::table());
  const zgame::Table& game = zgame::game();
  EXPECT(game.zcls_light_override != nullptr && game.zcls_light_restore != nullptr);
  zcls::Node* sun = game.zcls_light_create();
  zcls::Node* grill = game.zcls_node_alloc();
  zcls::Node* plain = game.zcls_node_alloc();
  EXPECT(sun && grill && plain);
  if (!sun || !grill || !plain) return;
  auto* l = static_cast<zcls::Light*>(sun->data);
  pm::zcls::light_set_ambient(sun, 0.6f);
  pm::zcls::light_set_diffuse(sun, 0.8f);
  l->put<uint32_t>(0xb0, l->get<uint32_t>(0xb0) | 0x200);  // static: a change makes the bake stale
  zmod::LightList& L = zmod::light_list();
  const zmod::LightList was = L;
  L.list[0].node = sun, L.list[0].data = l, L.count = 1, L.directed = 0;
  grill->put<uint32_t>(0x2c, 0xb281);                      // the grills' word: bit 7, ambient 0x59
  plain->put<uint32_t>(0x2c, 0);
  zcls::frames().frozen = 0;
  zcls::frames().frame = 100, zcls::frames().update_frame = 1;

  zcls::LightOverride s;
  EXPECT(!game.zcls_light_override(plain, &s));  // no bits: nothing borrowed
  const bool lit = game.zcls_light_override(grill, &s);
  const float during = l->get<float>(0xa0), diffuse_during = l->get<float>(0x9c);
  const uint32_t stamp_during = zcls::frames().update_frame;  // held: the static light's change not counted
  const int32_t hold_during = pm::zcls::stale_hold();
  if (lit) game.zcls_light_restore(s);
  const float after = l->get<float>(0xa0), diffuse_after = l->get<float>(0x9c);
  std::printf("  ambient 0.6 -> %g -> %g (0x59/255 %g); diffuse %g -> %g; stamp %u, hold %d -> %d\n", during, after,
              0x59 / 255.0f, diffuse_during, diffuse_after, stamp_during, hold_during, pm::zcls::stale_hold());
  EXPECT(lit && during == static_cast<float>(0x59) * (1.0f / 255.0f) && after == 0.6f);
  EXPECT(diffuse_during == 0.8f && diffuse_after == 0.8f);  // bit 8 clear: the diffuse untouched
  EXPECT(hold_during == 1 && pm::zcls::stale_hold() == 0 && stamp_during == 1 && zcls::frames().update_frame == 1);

  // Both bits: ambient from 9..16, diffuse from 17..24.
  grill->put<uint32_t>(0x2c, (1u << 7) | (1u << 8) | (0x40u << 9) | (0xc0u << 17));
  EXPECT(game.zcls_light_override(grill, &s));
  EXPECT(l->get<float>(0xa0) == 64.0f * (1.0f / 255.0f) && l->get<float>(0x9c) == 192.0f * (1.0f / 255.0f));  // the exe: value x [0x005f0704]
  game.zcls_light_restore(s);
  EXPECT(l->get<float>(0xa0) == 0.6f && l->get<float>(0x9c) == 0.8f);
  // No directed light: nothing.
  L.directed = -1;
  EXPECT(!game.zcls_light_override(grill, &s));
  L = was;
  zgame::set_game(zgame::mw3_table());
}

// Pirate's Moon's node update (0x004fe510) has a case for lights (class 9): the light's update (0x0050f270, the
// shared light_update) as the walk reaches the node, a light no world lists too; MW3's (0x004eca00) reports the
// class unknown (3) and leaves it.
#include "gamez/zclass/class.h"
TEST(pm_node_update_updates_a_light) {
  for (int pm_game = 1; pm_game >= 0; --pm_game) {
    zgame::set_game(pm::table());
    zcls::Node* n = zgame::game().zcls_light_create();
    EXPECT(n != nullptr);
    if (!n) return;
    auto* l = static_cast<zcls::Light*>(n->data);
    n->set_flags(n->flags() | 4);                          // active
    l->put<uint32_t>(0xb0, l->get<uint32_t>(0xb0) | 1);   // to be recalculated
    if (!pm_game) zgame::set_game(zgame::mw3_table());
    const int status = zcls::node_update(n);
    const bool recalc = (l->get<uint32_t>(0xb0) & 1) != 0;
    std::printf("  %s: status %d, still to recalculate %d\n", pm_game ? "PM" : "MW3", status, recalc);
    if (pm_game) EXPECT(status == 0 && !recalc);
    else EXPECT(status == 3 && recalc);
  }
  zgame::set_game(zgame::mw3_table());
}
