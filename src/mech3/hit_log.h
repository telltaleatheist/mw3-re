#pragma once

namespace zcls { struct Node; }

// Ours, a line in the log and nothing else, for a player's report of something solid
// where nothing shows: MW3_HITLOG=1 writes each wall the player's mech stops at and the
// first thing each weapon ray near the player hits (docs/program.md, Debug logs).
namespace mech3 {

bool hit_log_on();
// `kind` a short word; `owner` the query's object, or null; `from` the ray's start, or null.
void hit_log(const char* kind, zcls::Node* node, const void* owner, const float point[3], const float normal[3],
             const float from[3]);

}  // namespace mech3
