// MW1's repairs on PM's damage (src/mw1_repairs.h): the body prices against the player's MW_MAIN.EXE, the line by
// tons, the premium, a mech's damage from its bars (armor and structure, or armor alone), what a field base mended,
// the bill taken and forgiven, the commander's lost mech and the safety net, and save version 5.
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.h"
#include "mw1_install.h"
#include "mech3/pilot.h"
#include "mw1_campaign.h"
#include "mw1_pilots.h"
#include "mw1_repairs.h"
#include "mw1_save.h"

namespace fs = std::filesystem;

namespace {
std::vector<uint8_t> mw_main_data() {
  std::error_code ec;
  fs::path exe;
  for (const auto& e : fs::directory_iterator(MW1_DOS_GAME_DIR, ec)) {
    std::string n = e.path().filename().string();
    for (char& ch : n) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if (n == "MW_MAIN.EXE") exe = e.path();
  }
  std::ifstream f(exe, std::ios::binary);
  const std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (b.size() < 0x1c || b[0] != 'M' || b[1] != 'Z') return {};
  const size_t at = static_cast<size_t>(b[8] | b[9] << 8) * 16 + (0x1090 - 0x0800) * 16;
  return at < b.size() ? std::vector<uint8_t>(b.begin() + static_cast<long>(at), b.end()) : std::vector<uint8_t>{};
}
mw1::Company company() {
  mw1::Company c;
  c.funds = 1000000, c.repair_premium = 18;
  return c;
}
// An Owens-sized set of bars: 11 bars of 10 armor and 10 structure points, all whole.
mw1::Bars whole() {
  mw1::Bars b;
  for (int k = 0; k < 11; ++k) b.armor_points[k] = 10, b.armor_left[k] = 1, b.structure_points[k] = 10, b.structure_left[k] = 1;
  return b;
}
}  // namespace

TEST(mw1_repairs_body_prices_are_mw_main_exes_and_the_line) {
  // MW1's table against the exe (skipped without the install; the line below still checked).
  if (!mw1_test::no_mw1()) {
    const std::vector<uint8_t> d = mw_main_data();
    EXPECT(d.size() > 0x3CAB + mw1::kKindCount);
    for (int k = 0; k < mw1::kKindCount && d.size() > 0x3CAB + mw1::kKindCount; ++k)
      EXPECT(d[0x3CAB + static_cast<size_t>(k)] == mw1::kBodyRepairPrices[k]);
  }
  // The line through MW1's ten (provisional Q1), each kind's own price beside it.
  for (int k = 0; k < mw1::kKindCount; ++k) {
    const int32_t line = mw1::kRepairPerTon * mw1::kMechKinds[k].tons - mw1::kRepairOffset;
    std::printf("  %-12s %2d t: MW1 %2d,000 a location, the line %5d\n", mw1::kMechKinds[k].name, mw1::kMechKinds[k].tons,
                mw1::kBodyRepairPrices[k], line);
    EXPECT(std::abs(line - mw1::kBodyRepairPrices[k] * 1000) <= 3500);
  }
  // A full repair: 9 locations, the premium. The Owens (35 t) at 18%: 5,870 x 9 x 1.18 = 62,339.
  mw1::Company c = company();
  EXPECT(mw1::full_repair(c, mw1::kPmOwens) == 62339);
  c.repair_premium = 0;
  EXPECT(mw1::full_repair(c, mw1::kPmOwens) == 52830);
  // The premium on arrival: (tier + 1) x 18 + 0..7.
  mw1::Rng rng(5);
  for (int32_t tier = 0; tier < 4; ++tier)
    for (int i = 0; i < 50; ++i) {
      c.tier = tier;
      mw1::roll_premium(c, rng);
      EXPECT(c.repair_premium >= (tier + 1) * 18 && c.repair_premium <= (tier + 1) * 18 + 7);
    }
}

TEST(mw1_repairs_damage_by_points_and_what_a_field_base_mended) {
  mw1::Bars b = whole();
  EXPECT(mw1::damage_of(b) == 0);
  // One bar's armor gone: 10 of 220 points.
  b.armor_left[3] = 0;
  EXPECT(mw1::damage_of(b) == 455);  // 10 / 220 = 4.55%
  // Everything gone.
  for (int k = 0; k < 11; ++k) b.armor_left[k] = b.structure_left[k] = 0;
  EXPECT(mw1::damage_of(b) == mw1::kWhole);
  // Without structure points, armor alone.
  mw1::Bars a = whole();
  for (int k = 0; k < 11; ++k) a.structure_points[k] = 0, a.structure_left[k] = 0;
  a.armor_left[0] = 0.5f;
  EXPECT(mw1::damage_of(a) == 455);  // 5 of 110
  // A field base mends a leg's structure from half to whole: what it mended is that share.
  mw1::Bars now = whole();
  float armor[11], structure[11];
  for (int k = 0; k < 11; ++k) armor[k] = 1, structure[k] = 1;
  structure[9] = 0.5f;
  EXPECT(mw1::mended(now, armor, structure) == 227);  // 5 of 220
  EXPECT(mw1::mended(now, now.armor_left, now.structure_left) == 0);
}

TEST(mw1_repairs_the_bill_taken_forgiven_and_the_lost_mechs) {
  // A typical bill beside a typical fee (for Owen, Q3): the Owens back with a quarter of its repairs and a Firefly
  // with a tenth, at an 18% premium, against a tier 0 contract's 150,000.
  mw1::Company c = company();
  mw1::Repairs r = mw1::settle_repairs(c, {{0, mw1::kPmOwens, 2500, false}, {1, mw1::kPmFirefly, 1000, false}});
  for (const std::string& l : r.lines) std::printf("  %s\n", l.c_str());
  EXPECT(r.billed == 62339 / 4 + 53206 / 10 && r.paid == r.billed && r.forgiven == 0 && c.funds == 1000000 - r.billed);
  // Never below 0: the rest forgiven.
  c = company();
  c.funds = 2000;
  r = mw1::settle_repairs(c, {{0, mw1::kPmOwens, mw1::kWhole, false}});
  EXPECT(r.billed == 62339 && r.paid == 2000 && r.forgiven == 60339 && c.funds == 0);
  // A destroyed mech is lost, never billed. The commander takes the first idle hangar mech.
  c = company();
  mw1::OwnedMech firefly, centurion;
  firefly.mech = mw1::kPmFirefly, firefly.pilot = 1, centurion.mech = mw1::kPmCenturion;
  c.hangar = {firefly, centurion};
  r = mw1::settle_repairs(c, {{0, mw1::kPmOwens, 3000, true}});
  EXPECT(r.billed == 0 && c.mech == mw1::kPmCenturion && c.hangar.size() == 1 && c.hangar[0].mech == mw1::kPmFirefly);
  for (const std::string& l : r.lines) std::printf("  %s\n", l.c_str());
  // No idle one: a lancemate's, who stays behind next time.
  c.recruits = {1};
  mw1::hire(c, 0);
  r = mw1::settle_repairs(c, {{0, mw1::kPmCenturion, 0, true}});
  EXPECT(c.mech == mw1::kPmFirefly && c.hangar.empty() && c.lance.size() == 1 && mw1::mech_of(c, 0) == -1);
  EXPECT(!r.lines.empty() && r.lines[0] == "Lost: your Centurion. You take the Firefly; Elli Gujar stays behind next time.");
  // None left at all: the last comes back fully repaired, on the bill (with 0 in the bank, forgiven).
  c.funds = 0;
  r = mw1::settle_repairs(c, {{0, mw1::kPmFirefly, mw1::kWhole, true}});
  EXPECT(c.mech == mw1::kPmFirefly && r.billed == 53206 && r.forgiven == 53206 && c.funds == 0);
  for (const std::string& l : r.lines) std::printf("  %s\n", l.c_str());
}

TEST(mw1_repairs_saved_and_a_version_4_save_read) {
  mw1::Rng rng(9);
  mw1::Company c = mw1::new_company(rng);
  c.repair_premium = 23;
  mw1::Contract k = mw1::generate_contract(c, 0, rng);
  mw1::accept(c, k);
  mw1::AfterAction p;
  p.outcome = mw1::Outcome::kFailure;
  p.wrecks = {{mw1::kLocust, 40, 6100}};
  p.bills = {{0, mw1::kPmOwens, 1234, false}, {41, mw1::kPmFirefly, 0, true}};
  p.salvage = std::make_shared<mech3::Store>();
  p.launch_stores = std::make_shared<mech3::Store>();
  p.launch_stores->assign(*c.stores);
  const std::string text = mw1::save_text(c, &k, &p);
  mw1::Company back;
  mw1::Contract kb;
  mw1::AfterAction pb;
  bool has = false, pend = false;
  std::string why;
  EXPECT(mw1::load_text(text, back, kb, &has, &why, &pb, &pend));
  std::printf("  %s\n", why.c_str());
  EXPECT(pend && back.repair_premium == 23 && pb.wrecks.size() == 1 && pb.wrecks[0].damage == 6100 && pb.bills.size() == 2);
  EXPECT(pb.bills[0].who == 0 && pb.bills[0].mech == mw1::kPmOwens && pb.bills[0].damage == 1234 && !pb.bills[0].destroyed);
  EXPECT(pb.bills[1].who == 41 && pb.bills[1].destroyed);
  EXPECT(mw1::save_text(back, &kb, &pb) == text);
  // Version 4: no premium line (the world's without its roll), a wreck kind:condition (its damage the parts lost),
  // no bills.
  std::string v4;
  for (size_t at = 0, nl; at < text.size(); at = nl + 1) {
    nl = text.find('\n', at);
    std::string line = text.substr(at, nl - at);
    if (line.rfind("check=", 0) == 0 || line.rfind("repair_premium=", 0) == 0 || line.rfind("pending_bills=", 0) == 0) continue;
    if (line.rfind("version=", 0) == 0) line = "version=4";
    if (line.rfind("pending_wrecks=", 0) == 0) line = line.substr(0, line.rfind(':'));
    v4 += line + "\n";
  }
  uint32_t h = 2166136261u;
  for (unsigned char ch : v4) h = (h ^ ch) * 16777619u;
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", h);
  mw1::Company old;
  mw1::AfterAction po;
  EXPECT(mw1::load_text(v4 + check, old, kb, &has, &why, &po, &pend));
  EXPECT(pend && old.repair_premium == (old.tier + 1) * 18 && po.wrecks.size() == 1 && po.wrecks[0].damage == 6000 && po.bills.empty());
}

// FIXES.md item 51 (Owen, 2026-11-18): the wrecks' worth pays the repairs first, the salvage term on what's left.
TEST(mw1_repairs_salvage_after_repairs_owens_numbers) {
  // Owen's example: 3,000,000 of wrecks, 2,000,000 of damage, a 10% term: 100,000 paid, the mech repaired, nothing
  // billed.
  mw1::SalvageSum m = mw1::salvage_after_repairs(3000000, {2000000}, 10, 100);
  EXPECT(m.repairs == 2000000 && m.covered == 2000000 && m.beyond == 0 && m.left == 1000000 && m.pay == 100000);
  // Repairs over the pool: the pool spent, the rest billed, nothing paid.
  m = mw1::salvage_after_repairs(1000000, {1400000}, 10, 100);
  EXPECT(m.covered == 1000000 && m.beyond == 400000 && m.left == 0 && m.pay == 0);
  // The order: the commander's first, then the lancemates' in turn; a short pool leaves the last one's billed.
  m = mw1::salvage_after_repairs(1000000, {700000, 200000, 300000}, 20, 100);
  EXPECT(m.from_pool.size() == 3 && m.from_pool[0] == 700000 && m.from_pool[1] == 200000 && m.from_pool[2] == 100000);
  EXPECT(m.beyond == 200000 && m.left == 0 && m.pay == 0);
  // A deduction (MW1's reduced payout) docks the pay, never the repairs: 60% of 10% of 1,000,000.
  m = mw1::salvage_after_repairs(3000000, {2000000}, 10, 60);
  EXPECT(m.covered == 2000000 && m.left == 1000000 && m.pay == 60000);
  // A story battle: all of what's left.
  m = mw1::salvage_after_repairs(3000000, {2000000}, mw1::kStorySalvagePercent, 100);
  EXPECT(m.pay == 1000000);
  // No wrecks (a loss): every repair billed.
  m = mw1::salvage_after_repairs(0, {500000}, 10, 100);
  EXPECT(m.covered == 0 && m.beyond == 500000 && m.pay == 0);
}

TEST(mw1_repairs_settled_from_the_pool) {
  // The Owens with a quarter of its repairs (62,339 / 4 = 15,584) and a Firefly with a tenth (5,320), at an 18%
  // premium (company()), against a pool of 100,000 and a 10% term: both repaired from the pool, 79,096 left, 7,909 paid
  // (settle_mission adds it); nothing from the funds.
  mw1::Company c = company();
  mw1::Repairs r = mw1::settle_repairs(c, {{1, mw1::kPmFirefly, 1000, false}, {0, mw1::kPmOwens, 2500, false}}, 100000, 10, 100);
  for (const std::string& l : r.lines) std::printf("  %s\n", l.c_str());
  EXPECT(r.sum.repairs == 62339 / 4 + 53206 / 10 && r.sum.covered == r.sum.repairs && r.billed == 0 && c.funds == 1000000);
  EXPECT(r.sum.left == 100000 - r.sum.repairs && r.sum.pay == r.sum.left * 10 / 100);
  // DETAILS' lines (FIXES.md item 57): a heading, then each mech's, the commander's first, in plain words.
  EXPECT(r.lines.size() == 3 && r.lines[0] == "Repairs:" && r.lines[1] == "Owens (you): 15,584, paid from salvage");
  EXPECT(r.mechs.size() == 2 && r.mechs[0].who == 0 && r.mechs[0].from_pool == 15584 && r.mechs[1].who == 1);
  // PAYMENT's line: two mechs, their total, fullest first, each shorter.
  std::vector<std::string> pl = mw1::repair_line_choices(r);
  EXPECT(pl.size() >= 2 && pl[0] == "2 'Mechs repaired: 20,904 C-bills, paid from salvage" && pl[1] == "2 'Mechs repaired: 20,904, paid from salvage");
  for (size_t i = 1; i < pl.size(); ++i) EXPECT(pl[i].size() < pl[i - 1].size());
  // One mech: its own name (Owen's words).
  c = company();
  r = mw1::settle_repairs(c, {{0, mw1::kPmOwens, 2500, false}}, 100000, 10, 100);
  pl = mw1::repair_line_choices(r);
  EXPECT(!pl.empty() && pl[0] == "Owens repaired: 15,584 C-bills, paid from salvage");
  EXPECT(mw1::repair_line_choices(mw1::settle_repairs(c, {}, 100000, 10, 100)).empty());
  // A pool of 16,000: the commander's first (15,584 from it), the Firefly's 416 from it and 4,904 billed.
  c = company();
  r = mw1::settle_repairs(c, {{1, mw1::kPmFirefly, 1000, false}, {0, mw1::kPmOwens, 2500, false}}, 16000, 10, 100);
  for (const std::string& l : r.lines) std::printf("  %s\n", l.c_str());
  EXPECT(r.sum.from_pool.size() == 2 && r.sum.from_pool[0] == 62339 / 4 && r.sum.from_pool[1] == 16000 - 62339 / 4);
  EXPECT(r.billed == r.sum.repairs - 16000 && r.paid == r.billed && c.funds == 1000000 - r.billed && r.sum.pay == 0);
  EXPECT(r.lines.back().rfind("Repair bill: ", 0) == 0);
  pl = mw1::repair_line_choices(r);
  EXPECT(!pl.empty() && pl[0] == "2 'Mechs repaired: 20,904 C-bills, 4,904 billed to you" && pl.back() == "Repairs: 20,904, 4,904 billed");
  // Nothing from the pool (a failure): "billed" alone.
  c = company();
  r = mw1::settle_repairs(c, {{0, mw1::kPmOwens, 2500, false}}, 0, 10, 100);
  pl = mw1::repair_line_choices(r);
  EXPECT(!pl.empty() && pl[0] == "Owens repaired: 15,584 C-bills, billed to you" && r.lines[1] == "Owens (you): 15,584, billed");
  // The commander's last mech lost: its full repair comes off the top of the pool.
  c = company();
  c.mech = mw1::kPmFirefly;
  r = mw1::settle_repairs(c, {{0, mw1::kPmFirefly, mw1::kWhole, true}}, 100000, 10, 100);
  EXPECT(r.sum.covered == 53206 && r.billed == 0 && r.sum.left == 100000 - 53206);
}

// The whole settlement as CONTINUE makes it (FIXES.md item 51): a win's funds move by the fee and the salvage paid on
// what the wrecks left after the repairs; a story win (no employer) is paid all of what's left; a loss has no pool.
TEST(mw1_repairs_settle_mission_pays_after_the_repairs) {
  mw1::Rng rng(5);
  mw1::Company c = mw1::new_company(rng);
  c.repair_premium = 18;
  mw1::Contract k = mw1::generate_contract(c, 0, rng);
  k.salvage_percent = 10;
  mw1::accept(c, k);
  mw1::AfterAction a;
  a.outcome = mw1::Outcome::kSuccess;
  a.wrecks = {{mw1::kJenner, 100, 0}, {mw1::kLocust, 0, mw1::kWhole}};
  a.bills = {{0, c.mech, 5000, false}};
  const int64_t before = c.funds;
  mw1::MissionSettled m = mw1::settle_mission(c, k, a, rng);
  std::printf("  won: pool %lld, repairs %lld, left %lld, paid %lld (%d%%), fee %lld\n", static_cast<long long>(m.r.sum.pool),
              static_cast<long long>(m.r.sum.repairs), static_cast<long long>(m.r.sum.left), static_cast<long long>(m.s.salvage),
              m.term, static_cast<long long>(m.s.fee));
  EXPECT(m.term == 10 && m.s.pool == (2772 + 2291) * 1000 && m.r.sum.repairs > 0 && m.r.billed == 0);
  EXPECT(m.r.sum.left == m.s.pool - m.r.sum.repairs && m.s.salvage == m.r.sum.left * 10 * m.s.payout_percent / 10000);
  EXPECT(c.funds == before + m.s.fee + m.s.salvage);
  // A story battle won: no fee, all of what's left.
  mw1::Contract story = mw1::generate_contract(c, 0, rng);
  story.rung = 1;
  const int64_t before2 = c.funds;
  m = mw1::settle_mission(c, story, a, rng);
  EXPECT(m.term == mw1::kStorySalvagePercent && m.s.fee == 0 && m.s.salvage == m.r.sum.left && c.funds == before2 + m.s.salvage);
  // Lost: no pool, the repairs billed from the funds.
  mw1::Contract f = mw1::generate_contract(c, 0, rng);
  mw1::accept(c, f);
  a.outcome = mw1::Outcome::kFailure;
  const int64_t before3 = c.funds;
  m = mw1::settle_mission(c, f, a, rng);
  EXPECT(m.s.pool == 0 && m.s.salvage == 0 && m.r.sum.covered == 0 && m.r.billed == m.r.sum.repairs && c.funds == before3 - m.r.paid);
}
