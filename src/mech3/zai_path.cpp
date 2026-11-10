#include "mech3/zai_path.h"
#include "mech3/zai_x87.h"

#include <cfloat>
#include <cmath>

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {
namespace {

// Pirate's Moon 1.0 @ 0x00537a00 / 0x00537a50: at 53 bits the sum is held in the register, the answer stored.
double sum_sq(const float* a, const float* b) {
  const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
  return (double{dx} * dx + double{dy} * dy) + double{dz} * dz;
}
float dist_sq(const float* a, const float* b) { return static_cast<float>(sum_sq(a, b)); }
float dist(const float* a, const float* b) { return static_cast<float>(std::sqrt(sum_sq(a, b))); }

}  // namespace

// Mech3.exe 1.2 @ 0x004d51d0
Behaviour* nearest_step(const AiNet& net, const float p[3]) {
  float best = FLT_MAX;
  Behaviour* got = nullptr;
  for (znet::Element* e : net.elems) {
    Behaviour* b = e ? static_cast<AiElem*>(e)->as_behaviour() : nullptr;
    const float* q = b ? b->position() : nullptr;
    if (!q) continue;
    const float d = dist_sq(p, q);
    if (d < best) best = d, got = b;
  }
  if (got) return got;
  // FIX (bug): the exe reads element 0 of an empty net.
  if (net.elems.empty() || !net.elems[0]) return nullptr;
  return static_cast<AiElem*>(net.elems[0])->as_behaviour();
}

// Pirate's Moon 1.0 @ 0x004d33b0 (MW3's 0x004c7f70)
bool PathFinder::search(const Pilot* p, CompiledNet* n, Behaviour* start, Behaviour* t) {
  pilot = p, net = n, target = t;
  return search_again(start);
}

// Mech3.exe 1.2 @ 0x004c7e80: a pilot within 10 of the target has the way
// [start] at once.
bool PathFinder::search_again(Behaviour* start) {
  found = false;
  best.clear(), way.clear();
  best_length = FLT_MAX;
  at_pos = pilot ? pilot->pos : nullptr;
  if (!at_pos || !net->mark(true)) return found;
  if (dist_sq(target->way_point(), at_pos) < 100.0f) {  // Pirate's Moon 0x004d32c0: vt+0x1c
    found = true;
    best.push_back(start);
  } else {
    walk(start);
  }
  net->mark(false);
  way.clear();
  return found;
}

// Mech3.exe 1.2 @ 0x004c8140 (+0x04)
bool PathFinder::keep() {
  if (length < best_length) {
    best = way;
    best_length = length;
  }
  return true;
}

// Mech3.exe 1.2 @ 0x004c7fa0
bool PathFinder::walk(Behaviour* s) {
  bool go = true;
  if (s == target) {
    found = true;
    return keep();
  }
  if (s->visited() || !(length < best_length)) return go;
  s->set_visited(true);
  way.push_back(s);
  const int32_t n = s->link_count();
  const float saved_length = length;
  const float* saved_pos = at_pos;
  for (int32_t i = 0; i < n && go; ++i) {
    Behaviour* nx = s->link(i);
    nx = nx ? nx->as_behaviour() : nullptr;
    if (!nx || !nx->passable(pilot, s)) continue;
    if (const float* q = nx->position()) {
      length = stored(double{dist(at_pos, q)} + length);
      at_pos = q;
    }
    go = walk(nx);
    length = saved_length, at_pos = saved_pos;
  }
  way.pop_back();
  s->set_visited(false);
  return go;
}

// Mech3.exe 1.2 @ 0x004c8330: the way's steps before `at` dropped (all of
// them when `at` is not on it).
void PathFinder::drop_to(Behaviour* at) {
  auto it = best.begin();
  while (it != best.end() && *it != at) ++it;
  best.erase(best.begin(), it);
}

// Mech3.exe 1.2 @ 0x004c8270
bool PathFinder::still_open() const {
  bool ok = !best.empty();
  if (best.size() > 1) {
    auto it = best.begin();
    Behaviour* prev = *it;
    for (++it; it != best.end(); ++it) {
      if (!(*it)->passable(pilot, prev)) break;
      prev = *it;
    }
    ok = it == best.end();
  }
  return ok && target->passable(pilot, best.back());
}

// Mech3.exe 1.2 @ 0x004c83c0
Behaviour* PathFinder::next_after(Behaviour* at) {
  drop_to(at);
  if (best.empty()) return nullptr;
  best.pop_front();
  return best.empty() ? target : best.front();
}

// Mech3.exe 1.2 @ 0x004c8430
Behaviour* PathFinder::follow(Behaviour* at) {
  drop_to(at);
  if (!still_open() || best.empty()) return nullptr;
  best.pop_front();
  return best.empty() ? target : best.front();
}

// Mech3.exe 1.2 @ 0x004c7c10
bool PathHolder::find(const Pilot* p, CompiledNet* net, Behaviour* start, Behaviour* target) {
  if (!finder) finder = new PathFinder;
  const bool ok = finder->search(p, net, start, target);
  if (!ok) reset();
  return ok;
}

// Mech3.exe 1.2 @ 0x004c7cb0: the way kept, or a new search from `at`; the
// finder goes once the way is lost or reaches its target.
Behaviour* PathHolder::next(Behaviour* at) {
  if (!finder) return nullptr;
  Behaviour* r = finder->follow(at);
  if (!r && finder->search_again(at)) r = finder->next_after(at);
  if (!r || r == finder->target) reset();
  return r;
}

}  // namespace mech3::ai
