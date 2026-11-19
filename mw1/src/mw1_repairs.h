#pragma once
// MW1's repair prices on Pirate's Moon's damage (VISION.md, "Damage and repairs"; research/repairs.md). Every mission
// starts fresh (MW3 repairs every mech); only the money is ours: one bill per counted mission for the company's
// mechs' damage, field-base repairs included, and the same rule deducts the wrecks' repairs from their pay. No SDL
// and no screens here.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_campaign.h"

namespace mw1::repairs {

// MW1's body repair (MwQuoteRepairComponent, REPAIR 081B:3E28; MwQuoteMechRepairTotal 3763): the kind's price
// (bodyRepairBasePricesByKind, DS:3CAB, thousands) for each of the 9 body locations damaged, plus the world's
// premium. A full repair is all 9.
inline constexpr int32_t kBodyRepairPrices[kKindCount] = {4, 3, 4, 8, 10, 10, 12, 10, 12, 15};
inline constexpr int32_t kBodyLocations = 9;  // CAMPAIGN_BODY_LOCATION_COUNT
// Pirate's Moon's mechs aren't MW1's kinds: a location's price is the straight line through MW1's ten by tons
// (least squares, provisional Q1): 172 x tons - 150 C-bills.
inline constexpr int32_t kRepairPerTon = 172, kRepairOffset = 150;
// The tuning knob (provisional Q3: MW1's numbers as they are).
inline constexpr int32_t kRepairScale = 1;
// The world's premium (planetMarketPremiumPercent, DS:0477; PLANET 081B on arrival): (tier + 1) x 18 + rand(0..7)
// percent, on every repair there, ours and the wrecks' (provisional Q2).
inline constexpr int32_t kPremiumStep = 18, kPremiumRandomMax = 7;
void roll_premium(Company& c, Rng& rng);

// A mech's full repair in C-bills at this world: 9 locations, the premium, the scale.
int64_t full_repair(const Company& c, int32_t pm_mech);

// A mech's damage as PM's lance bars read it (mech_lance_armor, mech_lance_structure): each bar's armor points and
// fraction left, its structure points and fraction left. A bar's share of the full repair is its points' share
// (armor and structure together; without structure points, armor alone).
struct Bars {
  float armor_points[11] = {}, armor_left[11] = {}, structure_points[11] = {}, structure_left[11] = {};
};
inline constexpr int32_t kWhole = 10000;  // damage in ten-thousandths of a full repair
int32_t damage_of(const Bars& b);
// What a field base mended: the damage before (the bars' fractions then, the points now) less the damage now.
int32_t mended(const Bars& now, const float armor_before[11], const float structure_before[11]);

// A company mech's mission: who flew it (0 the commander, else the lancemate's pilot id), the mech, the damage it
// came back with plus what field bases mended (ten-thousandths of a full repair), and whether it was destroyed.
struct MechBill {
  int32_t who = 0;
  int32_t mech = -1;
  int32_t damage = 0;
  bool destroyed = false;
};
int64_t bill_of(const Company& c, const MechBill& b);

// A wreck's repairs (the same rule, on its chassis), deducted from its sale price; never below its scrap value
// (provisional Q4).
int64_t wreck_repairs(const Company& c, int32_t pm_mech, int32_t damage);

// FIXES.md item 51 (Owen, 2026-11-18): a win's salvage pays after the repairs. The pool (the wrecks' worth) pays the
// repairs first, the commander's mech's then the lancemates' in turn; the salvage term is paid on what's left (less
// any deduction), and what the pool can't cover is billed as before. A story battle has no employer to split with:
// all of what's left (the mw1 lead's default).
inline constexpr int32_t kStorySalvagePercent = 100;
struct SalvageSum {
  int64_t pool = 0, repairs = 0;
  int64_t covered = 0;  // the repairs the pool paid
  int64_t beyond = 0;   // the repairs past the pool: billed
  int64_t left = 0;     // the pool after the repairs
  int64_t pay = 0;      // left x term% x payout%
  std::vector<int64_t> from_pool;  // each bill's part the pool paid, in the bills' order
};
SalvageSum salvage_after_repairs(int64_t pool, const std::vector<int64_t>& bills, int32_t term_percent, int32_t payout_percent);

// The repairs after a counted mission (won, or failed and accepted; after the fee and the lancemates' fates): the
// bill for each mech come back (a destroyed one is lost, never billed), the commander's first, paid from `pool` (a
// win's wrecks' worth: salvage_after_repairs) and the rest taken from the funds, never below 0, the rest of that
// forgiven. The commander's destroyed mech is lost: they take the first idle hangar mech, else a lancemate's (who
// stays behind); with none left, the last comes back fully repaired, on the bill (the commander's). The lines for
// AFTER ACTION, the hub and DETAILS; `sum` the salvage's arithmetic (its pay not added here: settle_mission's).
struct Repairs {
  int64_t billed = 0, paid = 0, forgiven = 0;  // billed: past the pool, from the funds
  SalvageSum sum;
  std::vector<std::string> lines;
};
Repairs settle_repairs(Company& c, const std::vector<MechBill>& bills, int64_t pool = 0, int32_t term_percent = 0,
                       int32_t payout_percent = 100);

// A mission settled, as CONTINUE settles it (and the results screen and the hub's lines show it, on copies): settle's
// fee, standing and pool; on a counted mission (not a retreat) the lancemates' fates (after_mission) and the repairs
// from the pool; then the salvage pay (Settlement::salvage) added to the funds.
struct MissionSettled {
  Settlement s;
  Repairs r;
  int32_t term = 0;  // the salvage term the pay used (percent)
};
MissionSettled settle_mission(Company& c, Contract& k, const AfterAction& a, Rng& rng);
// Development shots only (MW1_SHOT_PAY=pool,repairs[,term]; the loop sets it): a win's pool, the commander's mech's
// repairs and the salvage term as given, so the PAYMENT block can be shown at any numbers (today's repairs are a few percent of a pool).
// Negative: as computed.
struct PayOverride {
  int64_t pool = -1, commander_repairs = -1;
  int32_t term = -1;
};
PayOverride& pay_override();

}  // namespace mw1::repairs

namespace mw1 {
using namespace repairs;
}  // namespace mw1
