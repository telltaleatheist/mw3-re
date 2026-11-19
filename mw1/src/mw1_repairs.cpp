#include "mw1_repairs.h"

#include <algorithm>
#include <cmath>

#include "mech3/mech_names.h"
#include "mw1_market.h"
#include "mw1_pilots.h"

namespace mw1::repairs {
namespace {
std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}
int32_t tons_of(int32_t mech) { return static_cast<int32_t>(std::lround(mech3::max_mass(mech))); }
int32_t damage_with(const Bars& b, const float* armor_left, const float* structure_left) {
  double all = 0, lost = 0;
  for (int k = 0; k < 11; ++k) {
    all += b.armor_points[k] + b.structure_points[k];
    lost += b.armor_points[k] * (1.0 - std::clamp(armor_left[k], 0.0f, 1.0f)) +
            b.structure_points[k] * (1.0 - std::clamp(structure_left[k], 0.0f, 1.0f));
  }
  return all > 0 ? static_cast<int32_t>(std::lround(lost / all * kWhole)) : 0;
}
}  // namespace

void roll_premium(Company& c, Rng& rng) { c.repair_premium = (c.tier + 1) * kPremiumStep + rng.upto(kPremiumRandomMax); }

int64_t full_repair(const Company& c, int32_t pm_mech) {
  const int64_t location = std::max<int64_t>(0, int64_t{kRepairPerTon} * tons_of(pm_mech) - kRepairOffset);
  return location * kBodyLocations * (100 + c.repair_premium) / 100 * kRepairScale;
}

int32_t damage_of(const Bars& b) { return damage_with(b, b.armor_left, b.structure_left); }

int32_t mended(const Bars& now, const float armor_before[11], const float structure_before[11]) {
  return std::max(0, damage_with(now, armor_before, structure_before) - damage_of(now));
}

int64_t bill_of(const Company& c, const MechBill& b) { return full_repair(c, b.mech) * std::clamp(b.damage, 0, kWhole) / kWhole; }

int64_t wreck_repairs(const Company& c, int32_t pm_mech, int32_t damage) {
  return full_repair(c, pm_mech) * std::clamp(damage, 0, kWhole) / kWhole;
}

SalvageSum salvage_after_repairs(int64_t pool, const std::vector<int64_t>& bills, int32_t term_percent, int32_t payout_percent) {
  SalvageSum m;
  m.pool = std::max<int64_t>(pool, 0);
  int64_t left = m.pool;
  for (int64_t b : bills) {
    b = std::max<int64_t>(b, 0);
    const int64_t from = std::min(b, left);
    m.from_pool.push_back(from);
    m.repairs += b, m.covered += from, left -= from;
  }
  m.beyond = m.repairs - m.covered;
  m.left = left;
  m.pay = left * std::clamp(term_percent, 0, 100) * std::clamp(payout_percent, 0, 100) / 10000;
  return m;
}

Repairs settle_repairs(Company& c, const std::vector<MechBill>& bills, int64_t pool, int32_t term_percent, int32_t payout_percent) {
  Repairs r;
  // The bills in the pool's order: the commander's mech first (or, every mech lost, the last one's full repair),
  // then the lancemates' in the bills' order. Each with its line's words before the amount.
  struct Line {
    int64_t amount;
    std::string words;
  };
  std::vector<Line> order;
  std::vector<std::string> lost;
  bool commander_lost = false;
  for (const MechBill& b : bills)
    if (b.who == 0) {
      if (b.destroyed) commander_lost = true;
      else if (const int64_t bill = pay_override().commander_repairs >= 0 ? pay_override().commander_repairs : bill_of(c, b); bill > 0)
        order.push_back({bill, "Repairs: the " + mech_title(b.mech)});
    }
  if (commander_lost) {
    const int32_t was = c.mech;
    int32_t take = -1;
    for (size_t h = 0; h < c.hangar.size() && take < 0; ++h)
      if (c.hangar[h].pilot == 0) take = static_cast<int32_t>(h);
    int32_t from = 0;  // the lancemate who gives up their mech
    if (take < 0 && !c.hangar.empty()) take = 0, from = c.hangar[0].pilot;
    if (take >= 0) {
      const OwnedMech o = c.hangar[static_cast<size_t>(take)];
      c.hangar.erase(c.hangar.begin() + take);
      c.mech = o.mech, c.loadout = o.loadout;
      lost.push_back("Lost: your " + mech_title(was) + ". You take the " + mech_title(o.mech) +
                     (from ? "; " + pilot_title(from) + " stays behind next time." : "."));
    } else {
      // The safety net: every mech the company owned destroyed; the last comes back, fully repaired, on the bill.
      order.push_back({full_repair(c, was), "Your last mech, the " + mech_title(was) + ", is recovered and repaired:"});
    }
  }
  for (const MechBill& b : bills) {
    if (b.who == 0 || b.destroyed) continue;  // lost (a lancemate's with them: after_mission), never billed
    if (const int64_t bill = bill_of(c, b); bill > 0)
      order.push_back({bill, "Repairs: the " + mech_title(b.mech) + " (" + pilot_title(b.who) + ")"});
  }
  std::vector<int64_t> amounts;
  for (const Line& l : order) amounts.push_back(l.amount);
  r.sum = salvage_after_repairs(pool, amounts, term_percent, payout_percent);
  r.lines = lost;
  for (size_t i = 0; i < order.size(); ++i) {
    const int64_t from = r.sum.from_pool[i];
    std::string l = order[i].words + " " + thousands(order[i].amount);
    if (from == order[i].amount) l += ", from the wrecks";
    else if (from > 0) l += ", " + thousands(from) + " from the wrecks";
    r.lines.push_back(l);
  }
  r.billed = r.sum.beyond;
  r.paid = std::min<int64_t>(r.billed, std::max<int64_t>(c.funds, 0));
  r.forgiven = r.billed - r.paid;
  c.funds -= r.paid;
  if (r.billed > 0)
    r.lines.push_back("Repair bill: " + thousands(r.billed) + " C-bills" + (r.forgiven ? " (" + thousands(r.forgiven) + " forgiven)" : ""));
  return r;
}

PayOverride& pay_override() {
  static PayOverride o;
  return o;
}

MissionSettled settle_mission(Company& c, Contract& k, const AfterAction& a, Rng& rng) {
  MissionSettled m;
  m.term = k.rung ? kStorySalvagePercent : k.salvage_percent;  // read before settle closes the contract
  m.s = settle(c, k, a.outcome, a.wrecks, rng);
  if (a.outcome == Outcome::kSuccess && pay_override().pool >= 0) m.s.pool = pay_override().pool;  // shots only
  if (pay_override().term >= 0) m.term = pay_override().term;
  if (a.outcome != Outcome::kRetreat) {  // a counted mission: the fates, then the repairs from the pool
    after_mission(c, a.lance);
    std::vector<MechBill> bills;
    for (const AfterAction::Bill& b : a.bills) bills.push_back({b.who, b.mech, b.damage, b.destroyed});
    m.r = settle_repairs(c, bills, m.s.pool, m.term, m.s.payout_percent);
  } else {
    m.r.sum = salvage_after_repairs(m.s.pool, {}, m.term, m.s.payout_percent);  // a retreat: no pool (not a win)
  }
  m.s.salvage = m.r.sum.pay;
  c.funds += m.s.salvage;
  return m;
}

}  // namespace mw1::repairs
