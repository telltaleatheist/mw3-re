#include "mech3/zai_attackers_pm.h"

#include <cstring>
#include <iterator>

namespace mech3::ai {

AttackersPm::AttackersPm(float rate, float limit, float clock) : rate_(rate), limit_(limit), stamp_(clock) {}

// Pirate's Moon 1.0 @ 0x004b2950
AttackersPm AttackersPm::for_pilot(float clock) { return AttackersPm(0.3f, 5.0f, clock); }

// Pirate's Moon 1.0 @ 0x004b25c0 with 0x004b2530, 0x004b2510. The fade is the fast exponential:
// -dt * rate * 2^23 / ln 2 (0x005e00c8) added to 1.0's bits. _ftol's low word: a long gap wraps as the exe's does.
bool AttackersPm::age(zgame::Handle attacker, float amount, float clock) {
  bool found = false;
  if (!list.empty()) {
    const double dt = double{clock} - stamp_;
    if (!(dt > 0.0)) {
      if (attacker) {
        if (AttackerPm* r = find(attacker, false, clock)) {
          found = true;
          r->sum = static_cast<float>(double{amount} + r->sum);
          r->last = clock;
        }
      }
    } else {
      const auto low = static_cast<uint32_t>(static_cast<int64_t>(-dt * double{rate_} * 12102200.0));
      const uint32_t faded = low + 0x3f800000u;
      float fade;
      std::memcpy(&fade, &faded, sizeof fade);
      for (auto it = list.begin(); it != list.end();) {
        it->sum = static_cast<float>(double{fade} * it->sum);
        const double gone = double{clock} - it->last;
        if (gone > limit_) {
          it = list.erase(it);
          continue;
        }
        if (attacker == it->attacker) {
          it->sum = static_cast<float>(double{amount} + it->sum);
          it->last = clock;
          found = true;
        }
        ++it;
      }
    }
  }
  stamp_ = clock;
  return found;
}

// Pirate's Moon 1.0 @ 0x004b2740 with 0x004b24e0
void AttackersPm::add(zgame::Handle attacker, float amount, float clock) {
  if (!age(attacker, amount, clock) && attacker) list.push_back({amount, clock, clock, attacker});
}

// Pirate's Moon 1.0 @ 0x004b27d0
bool AttackersPm::hit_within(float secs, float clock) {
  age(0, 0.0f, clock);
  const float since = static_cast<float>(double{clock} - secs);
  for (const AttackerPm& r : list)
    if (r.last > since) return true;
  return false;
}

namespace {
// 0x004b2890, 0x004b2820: the first record is overwritten with the best as the walk goes, so a
// pick loses the record that was first (as written).
template <class Key>
zgame::Handle best(std::list<AttackerPm>& list, Key key) {
  if (list.empty()) return 0;
  AttackerPm& top = list.front();
  for (auto it = std::next(list.begin()); it != list.end(); ++it)
    if (key(top) < key(*it)) top = *it;
  return top.attacker;
}
}  // namespace

// Pirate's Moon 1.0 @ 0x004b2890
zgame::Handle AttackersPm::hardest(float clock) {
  age(0, 0.0f, clock);
  return best(list, [](const AttackerPm& r) { return r.sum; });
}

// Pirate's Moon 1.0 @ 0x004b2820
zgame::Handle AttackersPm::latest(float clock) {
  age(0, 0.0f, clock);
  return best(list, [](const AttackerPm& r) { return r.last; });
}

// Pirate's Moon 1.0 @ 0x004b2900
AttackerPm* AttackersPm::find(zgame::Handle attacker, bool aged, float clock) {
  if (aged) age(0, 0.0f, clock);
  for (AttackerPm& r : list)
    if (r.attacker == attacker) return &r;
  return nullptr;
}

}  // namespace mech3::ai
