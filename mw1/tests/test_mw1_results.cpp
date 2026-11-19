// MW1's AFTER ACTION (src/mw1_results.h, FIXES.md items 31 and 38): the picture by the outcome and the fates, the
// House's crest, the deduction's choice, and the message read from the player's MW_MAIN.EXE (never kept here).
#include <string>
#include <vector>

#include "check.h"
#include "mw1_install.h"
#include "mw1_pilots.h"
#include "mw1_results.h"

using mw1::Deduction;
using mw1::Ending;
using mw1::Outcome;

TEST(mw1_results_the_picture_by_the_outcome) {
  EXPECT(mw1::ending_of(Outcome::kSuccess, false) == Ending::kWin);
  EXPECT(mw1::ending_of(Outcome::kSuccess, true) == Ending::kWin);  // a win is a win, losses or not (MW1's 0x4C)
  EXPECT(mw1::ending_of(Outcome::kRetreat, false) == Ending::kRetreat);
  EXPECT(mw1::ending_of(Outcome::kFailure, false) == Ending::kRetreat);  // a failure, nobody lost: the rock kicked
  EXPECT(mw1::ending_of(Outcome::kRetreat, true) == Ending::kLost);
  EXPECT(mw1::ending_of(Outcome::kFailure, true) == Ending::kLost);
  EXPECT(mw1::ending_image(Ending::kWin, false) == "mw1_end_win");
  EXPECT(mw1::ending_image(Ending::kRetreat, true) == "mw1_end_retreat_l");
  EXPECT(mw1::ending_image(Ending::kLost, false) == "mw1_end_lost");
  EXPECT(mw1::crest_image(mw1::kKurita) == "mw1_icon_house_kurita");
  EXPECT(mw1::crest_image(mw1::kDavion) == "mw1_icon_house_davion");
  EXPECT(mw1::crest_image(-1).empty() && mw1::crest_image(mw1::kHouseCount).empty());
}

TEST(mw1_results_the_deduction_line) {
  EXPECT(mw1::deduction_of(Outcome::kSuccess, 100, 0) == Deduction::kNone);
  EXPECT(mw1::deduction_of(Outcome::kSuccess, 75, 0) == Deduction::kSupply);
  EXPECT(mw1::deduction_of(Outcome::kSuccess, 50, 1) == Deduction::kPerformance);
  EXPECT(mw1::deduction_of(Outcome::kSuccess, 50, 2) == Deduction::kTransport);
  EXPECT(mw1::deduction_of(Outcome::kSuccess, 0, 1) == Deduction::kNoPay);  // the breach of contract
  EXPECT(mw1::deduction_of(Outcome::kFailure, 0, 1) == Deduction::kNone);   // only after a win
  EXPECT(mw1::deduction_of(Outcome::kRetreat, 50, 1) == Deduction::kNone);
}

TEST(mw1_results_wrap) {
  const std::vector<std::string> l = mw1::wrap({"ONE TWO THREE FOUR", "FIVE"}, 9);
  EXPECT((l == std::vector<std::string>{"ONE TWO", "THREE", "FOUR", "FIVE"}));
  EXPECT((mw1::wrap({"ABCDEFGHIJKL X"}, 5) == std::vector<std::string>{"ABCDEFGHIJKL", "X"}));
  EXPECT(mw1::wrap({}, 10).empty());
}

TEST(mw1_results_the_message_is_the_exes) {
  if (mw1_test::no_mw1()) return;
  mw1::set_mw1_game_dir("");
  EXPECT(mw1::outcome_message(mw1::kKurita, Outcome::kSuccess, false, Deduction::kNone).empty());  // no MW1: none
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  const auto win_k = mw1::outcome_message(mw1::kKurita, Outcome::kSuccess, false, Deduction::kNone);
  const auto win_l = mw1::outcome_message(mw1::kLiao, Outcome::kSuccess, false, Deduction::kNone);
  const auto win_d = mw1::outcome_message(mw1::kDavion, Outcome::kSuccess, false, Deduction::kNone);
  const auto win_s = mw1::outcome_message(mw1::kSteiner, Outcome::kSuccess, false, Deduction::kNone);
  const auto fail_k = mw1::outcome_message(mw1::kKurita, Outcome::kFailure, false, Deduction::kNone);
  const auto fail_m = mw1::outcome_message(mw1::kMarik, Outcome::kRetreat, false, Deduction::kNone);
  const auto garrison = mw1::outcome_message(mw1::kMarik, Outcome::kSuccess, true, Deduction::kNone);
  std::vector<std::vector<std::string>> docked;
  for (Deduction d : {Deduction::kSupply, Deduction::kPerformance, Deduction::kTransport, Deduction::kNoPay})
    docked.push_back(mw1::outcome_message(mw1::kSteiner, Outcome::kSuccess, false, d));
  mw1::set_mw1_game_dir("");
  if (win_k.empty()) {
    std::printf("  MW1 install at %s, but its exe doesn't read\n", MW1_DOS_GAME_DIR);
    EXPECT(false);
    return;
  }
  // One paragraph each: MW1's own words, its line breaks joined (no blank lines, no control bytes).
  for (const auto* m : {&win_k, &win_l, &win_d, &fail_k, &fail_m, &garrison}) {
    EXPECT(m->size() == 1);
    if (m->empty()) continue;
    EXPECT((*m)[0].size() > 30 && (*m)[0].find('\r') == std::string::npos && (*m)[0][0] != ' ');
  }
  EXPECT(win_k != win_l && win_k != win_d && win_l != win_d);  // Kurita's, Liao's, and the others' own
  EXPECT(win_d == win_s);                                       // Davion and Steiner share theirs
  EXPECT(fail_k != fail_m);
  EXPECT(!garrison.empty() && garrison[0].find("GARRISON") != std::string::npos);
  std::printf("  a win for Kurita: \"%s\"\n", win_k.empty() ? "" : win_k[0].c_str());
  // A short-paid win: the win, then its deduction line, each of the four its own.
  for (size_t i = 0; i < docked.size(); ++i) {
    EXPECT(docked[i].size() == 2 && docked[i][0] == win_s[0]);
    for (size_t j = 0; j < i; ++j) EXPECT(docked[i].size() == 2 && docked[j].size() == 2 && docked[i][1] != docked[j][1]);
  }
  EXPECT(docked[2].size() == 2 && docked[2][1].find("ABANDONED") != std::string::npos);
}

namespace {
std::vector<std::string> place(const mw1::ScreenView& v, const char* at) {
  for (const mw1::TextBlock& t : v.blocks)
    if (t.at && std::string(t.at) == at) return t.lines;
  return {};
}
}  // namespace

// The salvage, all of it, a list its arrows scroll (item 38); DETAILS puts your mech and the kills in the panel.
TEST(mw1_results_the_list_scrolls_and_details) {
  mw1::ResultsPanel p;
  for (int i = 0; i < 20; ++i) p.salvage.push_back({"part " + std::to_string(i), i});
  p.salvage[3].words = std::string(80, 'x');  // longer than the panel: cut
  p.kills = {{"Jenner by you 120,000", 0}};
  p.mech_title = "YOUR OWENS";
  p.mech = {"head", "torso"};
  p.pay = {"Fee: 1"};
  mw1::ResultsChoice c;
  mw1::ScreenView v = mw1::results_view(p, c, false);
  EXPECT(place(v, "LIST").size() == mw1::kListRows && place(v, "LIST")[0] == "part 0");
  EXPECT(place(v, "LIST_TITLE") == std::vector<std::string>{"SALVAGE TO STORES (1-6 of 20)"});
  EXPECT(place(v, "LIST")[3].size() == mw1::kResultsNarrow.status && place(v, "LIST")[3].substr(40) == "...");
  EXPECT(v.buttons[mw1::kResultsListUp - 1].there && !v.buttons[mw1::kResultsListUp - 1].on);
  EXPECT(v.buttons[mw1::kResultsListDown - 1].on);
  c.top = 99;  // scrolled past the end: kept to the last page
  mw1::clamp_choice(p, c);
  EXPECT(c.top == 20 - mw1::kListRows);
  v = mw1::results_view(p, c, true);
  EXPECT(place(v, "LIST").back() == "part 19" && !v.buttons[mw1::kResultsListDown - 1].on);
  EXPECT(place(v, "LIST")[3 - 3].size() < 62);
  // DETAILS: the mech's table in PAYMENT's place, the kills in the list's; no arrows for one row.
  c.details = true, c.top = 0;
  v = mw1::results_view(p, c, false);
  EXPECT(place(v, "PAY_TITLE") == std::vector<std::string>{"YOUR OWENS"} && place(v, "PAY") == p.mech);
  EXPECT(place(v, "LIST_TITLE") == std::vector<std::string>{p.kills_title} && place(v, "LIST")[0] == p.kills[0].words);
  EXPECT(place(v, "CREW").empty() && !v.buttons[mw1::kResultsListUp - 1].there);
  EXPECT(v.buttons[mw1::kResultsDetails - 1].words == "SUMMARY");
  // A short list: no arrows.
  p.salvage.resize(3);
  EXPECT(!mw1::results_view(p, {}, false).buttons[mw1::kResultsListDown - 1].there);
}

// FIXES.md item 57: PAYMENT's repair line, the fullest wording that fits the panel (16:9's, 4:3's), at its place;
// none without repairs.
TEST(mw1_results_repair_line_fits_the_panel) {
  mw1::ResultsPanel p;
  p.pay = {"Salvage: 50% of 46,037 = 23,018", "Fee: 400,000", "Total: +423,018   Funds: 1,000,000"};
  const std::vector<std::string> pay = p.pay;
  EXPECT(mw1::pay_lines(p, mw1::kResultsNarrow.status) == pay);
  p.repairs = {"Owens repaired: 4,963 C-bills, paid from salvage", "Owens repaired: 4,963, paid from salvage",
               "Repairs: 4,963, from salvage"};
  p.repairs_at = 0;
  std::vector<std::string> l = mw1::pay_lines(p, mw1::kResultsWide.status);
  EXPECT(l.size() == 4 && l[0] == p.repairs[0] && l[1] == pay[0]);
  l = mw1::pay_lines(p, mw1::kResultsNarrow.status);
  EXPECT(l.size() == 4 && l[0] == p.repairs[1] && l[0].size() <= mw1::kResultsNarrow.status);
  EXPECT(mw1::pay_lines(p, 10)[0] == p.repairs.back());  // none fits: the shortest
  p.repairs_at = 9;                                          // past the end: last
  EXPECT(mw1::pay_lines(p, 80).back() == p.repairs[0]);
  // On the screen: PAYMENT's four lines, the repair line among them.
  p.repairs_at = 0;
  for (bool wide : {false, true}) {
    const mw1::ScreenView v = mw1::results_view(p, {}, wide);
    const std::vector<std::string> shown = place(v, "PAY");
    EXPECT(shown.size() == mw1::kPayLines && shown[0] == p.repairs[wide ? 0 : 1]);
    for (const std::string& s : shown) EXPECT(s.size() <= (wide ? mw1::kResultsWide : mw1::kResultsNarrow).status);
  }
}
