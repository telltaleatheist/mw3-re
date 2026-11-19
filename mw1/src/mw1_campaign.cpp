#include "mw1_campaign.h"

#include <cctype>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

#include "mech3/mech_ids.h"
#include "mech3/mech_names.h"
#include "mech3/pilot.h"
#include "mw1_market.h"
#include "mw1_pilots.h"
#include "mw1_missions.h"
#include "mw1_repairs.h"
#include "mw1_story.h"

namespace mw1 {

// The stand-ins (research/contracts.md, "PM's mechs for MW1's"): each MW1 kind
// by its tonnage to the nearest Pirate's Moon Inner Sphere mech (Annihilator
// 100, Atlas 100, Avatar 70, Awesome 80, Bushwacker 55, Centurion 50, Champion
// 60, Firefly 30, Orion 75, Owens 35, Strider 40, Sunder 90); a tie to the
// lighter. MW1's Wasp and Wolverine are never stocked or fielded
// (MwGenerateUnitStock swaps them), kept for the tables' order.
const MechKind kMechKinds[kKindCount] = {
    {"Locust", 20, 1, 1504, kPmFirefly},         {"Wasp", 20, 1, 2000, kPmFirefly},
    {"Jenner", 35, 1, 3183, kPmOwens},           {"Phoenix Hawk", 45, 2, 4022, kPmStrider},
    {"Shadow Hawk", 55, 2, 4622, kPmBushwacker}, {"Wolverine", 55, 2, 4966, kPmBushwacker},
    {"Rifleman", 60, 3, 5500, kPmChampion},      {"Warhammer", 70, 3, 6021, kPmAvatar},
    {"Marauder", 75, 3, 6729, kPmOrion},         {"BattleMaster", 85, 3, 8410, kPmAwesome}};

const HouseRules kHouses[kHouseCount] = {
    {"Kurita", 10, -1, 5, 5, 10, 0},   {"Steiner", 10, 1, 15, 10, 10, 75}, {"Marik", 5, -1, 20, 15, 30, 50},
    {"Liao", 5, 1, 25, 20, 30, 0},     {"Davion", 0, 0, 20, 25, 0, 100}};

namespace {
// MwGenerateMissionOffer's constants (the port's ECONOMY enum).
constexpr int32_t kBandSecond = 9, kBandThird = 21, kBandFourth = 36;
constexpr int32_t kBudgetScale = 10, kHeavyCost = 25, kMediumCost = 15, kLightCost = 7, kEnemyLimit = 4;
constexpr int32_t kMixRollMax = 10, kMixFirst = 5, kMixSecond = 8;
constexpr int32_t kPaymentRollMax = 9, kPaymentMin = 100, kPaymentMax = 9990, kPaymentRounding = 10;
constexpr int32_t kRelationshipLimit = 10;
constexpr int32_t kTargetStepRollMax = 10;  // MISSION_TARGET_STEP_ROLL_MAXIMUM
constexpr int32_t kTargetGrievance = 2;     // the House you fought, at every ending
// The settlement's (STARTUP).
constexpr int32_t kScrapPerTon = 10, kSaleRounding = 500;  // thousands a ton (research/contracts.md); C-bills

// BtechPrepareEnemyMech: a class's kinds, picked at random within it.
const int32_t kLights[] = {kLocust, kJenner};
const int32_t kMediums[] = {kPhoenixHawk, kShadowHawk};
const int32_t kHeavies[] = {kRifleman, kWarhammer, kMarauder, kBattleMaster};

int32_t band(int32_t kills) {
  return kills >= kBandFourth ? 3 : kills >= kBandThird ? 2 : kills >= kBandSecond ? 1 : 0;
}

int32_t standing(const Company& c, int32_t house) {
  int32_t r = (c.favor[house] - c.grievance[house]) * 4;
  return r > kRelationshipLimit ? kRelationshipLimit : r < -kRelationshipLimit ? -kRelationshipLimit : r;
}
}  // namespace

int32_t lance_weight_of_tons(int32_t tons) { return tons < 40 ? 1 : tons <= 55 ? 2 : 3; }

void add_starting_stores(mech3::Store& s) {
  for (int i = 0; i < 4; ++i) s.add_weapon(0x1f, mech3::kInnerSphere);  // MGUN
  for (int i = 0; i < 2; ++i) s.add_weapon(5, mech3::kInnerSphere);     // ER medium laser
  for (int i = 0; i < 4; ++i) s.add_equipment(10, mech3::kInnerSphere, 1);  // heat sinks
  s.add_ammo(0x1f, 1000);
  s.add_ammo(0x17, 500);
  s.add_ammo(0x18, 1000);
  s.add_ammo(0x15, 100);
}

const StartPlanet kStartPlanets[kStartPlanetCount] = {
    {10, "MATSUIDA", kKurita, 1},   {11, "OSHIKA", kKurita, 1},  {121, "MALLORY'S WORLD", kDavion, 2}, {12, "IRURZUN", kKurita, 0},
    {129, "TANCREDI IV", kDavion, 0}, {119, "KESAI IV", kDavion, 1}, {136, "GREELY", kDavion, 1},       {118, "ROBINSON", kDavion, 0}};

namespace {
}  // namespace

// "MALLORY'S WORLD" -> "Mallory's World", "TANCREDI IV" -> "Tancredi IV" (Roman numerals kept).
std::string title_case_name(const char* name) {
  std::string s = name, out;
  size_t i = 0;
  while (i < s.size()) {
    size_t j = s.find(' ', i);
    if (j == std::string::npos) j = s.size();
    std::string w = s.substr(i, j - i);
    const bool roman = !w.empty() && w.find_first_not_of("IVX") == std::string::npos;
    for (size_t k = 1; k < w.size() && !roman; ++k) w[k] = static_cast<char>(std::tolower(static_cast<unsigned char>(w[k])));
    out += (out.empty() ? "" : " ") + w;
    i = j + 1;
  }
  return out;
}

Company new_company(Rng& rng) {
  Company c;
  // The world first, as the original draws it (081B:181A), then the cash bonus.
  const StartPlanet& p = kStartPlanets[rng.upto(kStartPlanetCount - 1)];
  c.planet = title_case_name(p.name), c.planet_house = p.house, c.tier = p.tier, c.planet_number = p.number;
  c.funds = kStartingFunds + static_cast<int64_t>(rng.upto(0xff)) * 1000;  // startupCashBonus
  c.mech = kPmOwens;
  c.stores = std::make_shared<mech3::Store>();
  add_starting_stores(*c.stores);
  roll_premium(c, rng);  // the start world's repair premium (mw1_repairs.h), after the original's draws
  roll_patience(c, rng);  // and its House's patience (MwApplyCurrentPlanetState, after the premium)
  roll_story_planets(c, rng);  // then the story's planets (mw1_story.h), as MW1 rolls them after the world's state
  return c;
}

Contract generate_contract(const Company& c, int32_t slot, Rng& rng) {
  Contract k;
  k.house = c.planet_house;
  const HouseRules& h = kHouses[k.house];
  // The force budget: (record band + the lance's weight) x 10, the offer slot's x1.5 or x0.5.
  const int32_t weight = lance_weight(c);  // the commander's mech and the lancemates' assigned ones (mw1_pilots.h)
  const int32_t base = (band(c.kills) + weight) * kBudgetScale;
  k.budget = base;
  if (slot == 1 || slot == 4) k.budget += base >> 1;
  else if (slot == 2 || slot == 5) k.budget >>= 1;
  // Spent heavies first, at most four; four heavies may roll down to 3+1 or 2+2.
  int32_t heavy = k.budget / kHeavyCost, rest = k.budget % kHeavyCost;
  int32_t medium = rest / kMediumCost, light = rest % kMediumCost / kLightCost;
  for (;;) {
    const int32_t n = light + medium + heavy;
    if (n <= 0) { light = 1; break; }
    if (n <= kEnemyLimit) break;
    if (light) --light;
    else if (medium) --medium;
    else if (heavy) --heavy;
    else break;
  }
  if (heavy == kEnemyLimit) {
    const int32_t mix = rng.upto(kMixRollMax);
    if (mix >= kMixFirst) heavy = 3, medium = 1;
    if (mix >= kMixSecond) heavy = 2, medium = 2;
  }
  for (int32_t i = 0; i < heavy; ++i) k.enemies.push_back(kHeavies[rng.upto(3)]);
  for (int32_t i = 0; i < medium; ++i) k.enemies.push_back(kMediums[rng.upto(1)]);
  for (int32_t i = 0; i < light; ++i) k.enemies.push_back(kLights[rng.upto(1)]);
  // The pay, in thousands, with the original's word arithmetic kept (the balance is MW1's).
  const uint16_t pay_base = static_cast<uint16_t>(static_cast<uint8_t>((k.budget >> 1) + 1) * 100);
  const uint16_t biased = static_cast<uint16_t>(rng.upto(kPaymentRollMax) + 1 + h.payment_bias);
  const uint16_t product = static_cast<uint16_t>(static_cast<uint32_t>(biased) * pay_base);
  const int32_t adjustment = static_cast<int32_t>(static_cast<int16_t>(product)) * h.payment_adjust / 100;
  uint16_t ceiling = static_cast<uint16_t>(adjustment + pay_base);
  ceiling = static_cast<uint16_t>(ceiling / kPaymentRounding * kPaymentRounding);
  if (ceiling < kPaymentMin) ceiling = kPaymentMin;
  if (ceiling > kPaymentMax) ceiling = kPaymentMax;
  k.payment_ceiling = ceiling;
  int32_t opening = (ceiling >> 2) / kPaymentRounding * kPaymentRounding;
  k.payment = opening < kPaymentMin ? kPaymentMin : opening;
  // Salvage and advance: the House's base shifted by the standing; the opening a quarter.
  k.salvage_ceiling = salvage_ceiling(c, k.house), k.advance_ceiling = advance_ceiling(c, k.house);
  k.salvage_percent = k.salvage_ceiling >> 2;
  k.advance_percent = k.advance_ceiling >> 2;
  // The target (after the pay, as MW1 rolls it): the employer's eligible columns stepped through, round again,
  // until the roll's count is met.
  int32_t steps = rng.upto(kTargetStepRollMax) + 1, column = 0;
  for (;;) {
    if (kHouseTargets[k.house][column] != 0 && --steps == 0) break;
    column = (column + 1) % kTargetColumns;
  }
  k.target = column;
  // What it asks and where: MW1's category, description and mission planet, and the map for them.
  choose_mission(k, rng, c.last_map_world, c.last_map_ia);  // item 46: never the map last drawn
  return k;
}

int32_t salvage_ceiling(const Company& c, int32_t house) { return std::max(0, standing(c, house) + kHouses[house].salvage_base); }
int32_t advance_ceiling(const Company& c, int32_t house) { return std::max(0, standing(c, house) + kHouses[house].advance_base); }

bool never_breaks_off(int32_t house) { return house == kSteiner || house == kDavion; }
void roll_patience(Company& c, Rng& rng) { c.patience = rng.upto(kPatienceRollMax) + kPatienceBase[c.planet_house]; }
bool refuses(const Company& c, int32_t house) { return house >= 0 && house < kHouseCount && c.grievance[house] >= kGrievanceRefusal; }

namespace {
// A term asked for more than the offer: the House's concession, 0..half the room to its ceiling on top of the offer
// (an offer over its ceiling, which MW1's word arithmetic would wrap, concedes nothing).
int32_t concession(int32_t offer, int32_t ceiling, Rng& rng) { return offer + rng.upto(std::max(0, ceiling - offer) >> 1); }
}  // namespace

Answer negotiate(Company& c, Contract& k, const Request& r, Rng& rng) {
  const bool endless = never_breaks_off(c.planet_house);
  if (endless || k.attempts < c.patience) ++k.attempts;
  bool satisfied = true;
  if (r.payment >= 0) {
    bool short_of = false;
    if (r.payment > k.payment) {
      const int32_t conceded = concession(k.payment, k.payment_ceiling, rng) / kPaymentRounding * kPaymentRounding;
      if (conceded < r.payment) k.payment = conceded, short_of = true;
    }
    if (short_of) satisfied = false;
    else k.payment = r.payment, satisfied = true;
  }
  // The later terms, each looked at in turn: a roll short moves the offer and fails; granted only while nothing
  // before has failed (MW1's jumps skip the grant, not the roll).
  auto term = [&](int32_t asked, int32_t& offer, int32_t ceiling) {
    if (asked < 0) return;  // not asked
    if (asked > offer) {
      const int32_t conceded = concession(offer, ceiling, rng);
      if (conceded < asked) {
        offer = conceded, satisfied = false;
        return;
      }
    }
    if (satisfied) offer = asked;
  };
  term(r.salvage, k.salvage_percent, k.salvage_ceiling);
  term(r.advance, k.advance_percent, k.advance_ceiling);
  if (satisfied) return Answer::kAgreed;
  if (!endless && k.attempts >= c.patience) {
    c.withdrawn = true;
    return Answer::kTerminated;
  }
  return Answer::kCounter;
}

bool last_round(const Company& c, const Contract& k) { return !never_breaks_off(c.planet_house) && k.attempts + 1 >= c.patience; }

void accept(Company& c, Contract& k) {
  const int32_t advance = k.advance_percent * k.payment / 100;
  c.funds += static_cast<int64_t>(advance) * 1000;
  k.remaining = k.payment - advance;
  k.accepted = true;
}

int32_t wreck_value(const Company& c, const Wreck& w) {
  const int32_t chassis = kMechKinds[w.kind].pm_mech;
  // VISION ("Wrecks pay what they're worth, less their repairs"): the chassis's sale price at this world less its
  // repairs by the one rule (mw1_repairs.h), +500 and in thousands as MwCalculateUnitConditionPrice rounds; never
  // below the scrap value, tons x 10,000 (provisional Q4).
  const int32_t scrap = static_cast<int32_t>(std::lround(mech3::max_mass(chassis))) * kScrapPerTon;
  const int64_t worth = sell_price(chassis, c.tier) - wreck_repairs(c, chassis, w.damage);
  return std::max(scrap, static_cast<int32_t>((worth + kSaleRounding) / 1000));
}

Settlement settle(Company& c, Contract& k, Outcome o, const std::vector<Wreck>& wrecks, Rng& rng) {
  Settlement s;
  const HouseRules& h = kHouses[c.planet_house];
  if (o == Outcome::kSuccess) {  // the pool: every wreck counts (the original's overwrite fixed, Owen 2026-11-16)
    for (const Wreck& w : wrecks) s.pool += int64_t{wreck_value(c, w)} * 1000;
  }
  if (k.rung) {  // a story battle (FIXES.md 47, mw1_ladder.h): no fee, no deduction roll (MW1's Dark Wing), no standing
    c.kills += static_cast<int32_t>(wrecks.size());
    k.accepted = false;
    return s;
  }
  if (rng.upto(100) <= h.deduction_roll) s.payout_percent = h.reduced_payout;
  if (o == Outcome::kSuccess) s.fee = static_cast<int64_t>(k.remaining) * s.payout_percent / 100 * 1000;
  c.funds += s.fee;
  // The standing (research/contracts.md): success +2 favor, retreat +1 grievance with the employer, nothing on a
  // failure; the House you fought +2 grievance every time.
  if (o == Outcome::kSuccess) c.favor[k.house] += 2;
  else if (o == Outcome::kRetreat) c.grievance[k.house] += 1;
  if (k.target < kHouseCount) c.grievance[k.target] += kTargetGrievance;  // whatever the ending
  c.kills += static_cast<int32_t>(wrecks.size());
  k.accepted = false;
  return s;
}

}  // namespace mw1
