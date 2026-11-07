#include "mech3/hit_log.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "gamez/zclass/cls_node.h"
#include "mech3/app_mech.h"
#include "mech3/mech.h"
#include "mech3/mission_world.h"

namespace mech3 {
namespace {
constexpr float kGap = 0.25f;    // seconds between lines for one kind on one node
constexpr int kMost = 4000;      // lines in a run
constexpr float kNear = 60.0f;   // a ray's start this close to the player is taken as the player's

// The node and its parents, up to the world's child: "healthy < g13 < ptank01".
std::string chain(zcls::Node* n) {
  std::string out;
  for (int i = 0; n && i < 8; ++i) {
    if (i) out += " < ";
    out += reinterpret_cast<const char*>(n);
    n = n->parent_count() > 0 && n->parents ? n->parents[0] : nullptr;
  }
  return out.empty() ? "(terrain)" : out;
}
}  // namespace

bool hit_log_on() {
  static const bool on = std::getenv("MW3_HITLOG") != nullptr;
  return on;
}

void hit_log(const char* kind, zcls::Node* node, const void* owner, const float point[3], const float normal[3],
             const float from[3]) {
  if (!hit_log_on()) return;
  Mech* player = world().mission_player;
  if (!player) return;
  float me[3] = {};
  mech_world_position(player, me);
  if (from && std::hypot(from[0] - me[0], from[2] - me[2]) > kNear) return;
  static int lines = 0;
  static std::map<std::pair<std::string, zcls::Node*>, float> last;
  const float t = mech_game_time();
  auto it = last.find({kind, node});
  if (lines >= kMost || (it != last.end() && t >= it->second && t - it->second < kGap)) return;
  last[{kind, node}] = t;
  ++lines;
  std::printf("mw3: hit %s t %.2f %s flags %08x owner %p at %.1f %.1f %.1f normal %.2f %.2f %.2f player %.1f %.1f %.1f\n",
              kind, t, chain(node).c_str(), node ? node->flags() : 0u, owner, point[0], point[1], point[2], normal[0],
              normal[1], normal[2], me[0], me[1], me[2]);
}

}  // namespace mech3
