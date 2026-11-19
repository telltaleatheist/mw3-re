#include "mw1_results.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "mw1_campaign.h"
#include "mw1_pilots.h"

namespace mw1::results {
namespace {
const std::vector<ScreenButton> kResultsButtons = {{"CONTINUE", "CONTINUE"}, {"REPLAY", "REPLAY"}, {"DETAILS", "DETAILS"},
                                                   {"LIST_UP", "", true}, {"LIST_DOWN", "", true}};

// PM's lance bars (mech_lance_armor's order): the torso's six facings 0..5, the head 6, the arms 7 and 8, the legs 9
// and 10. By location (kLocationNames): the bars it is made of.
const std::vector<std::vector<int>> kLocationBars = {{6}, {0, 1, 2, 3, 4, 5}, {7}, {8}, {9}, {10}};

int32_t percent_left(const float points[11], const float left[11], const std::vector<int>& bars) {
  float all = 0, kept = 0;
  for (int k : bars) all += points[k], kept += points[k] * left[k];
  if (all <= 0) return -1;
  const int32_t p = static_cast<int32_t>(kept * 100.0f / all + 0.5f);
  return p < 0 ? 0 : p > 100 ? 100 : p;
}
}  // namespace

static_assert(sizeof kLocationNames / sizeof kLocationNames[0] == kMechLocations, "a name for every location");

std::vector<std::string> fit_lines(std::vector<std::string> lines, size_t n) {
  if (lines.size() <= n || n == 0) return lines;
  const size_t more = lines.size() - (n - 1);
  lines.resize(n - 1);
  lines.push_back("... and " + std::to_string(more) + " more");
  return lines;
}

std::string time_words(int32_t seconds) {
  if (seconds < 0) return "unknown";
  char b[32];
  std::snprintf(b, sizeof b, "%d:%02d", seconds / 60, seconds % 60);
  return b;
}

void location_left(const Bars& b, std::vector<int32_t>* armor, std::vector<int32_t>* structure) {
  armor->clear(), structure->clear();
  for (const std::vector<int>& bars : kLocationBars) {
    armor->push_back(percent_left(b.armor_points, b.armor_left, bars));
    structure->push_back(percent_left(b.structure_points, b.structure_left, bars));
  }
}

std::vector<std::string> mech_lines(const std::vector<int32_t>& armor, const std::vector<int32_t>& structure, bool destroyed) {
  std::vector<std::string> l;
  if (destroyed) l.push_back("Destroyed in the battle");
  if (armor.size() != kMechLocations || structure.size() != kMechLocations) {
    if (!destroyed) l.push_back("Its damage wasn't read");
    return l;
  }
  auto pc = [](int32_t v) { return v < 0 ? std::string("  -") : (v < 10 ? "  " : v < 100 ? " " : "") + std::to_string(v) + "%"; };
  l.push_back("            ARMOUR STRUCTURE");
  for (int32_t i = 0; i < kMechLocations; ++i) {
    std::string name = kLocationNames[i];
    name.resize(12, ' ');
    l.push_back(name + "  " + pc(armor[static_cast<size_t>(i)]) + "     " + pc(structure[static_cast<size_t>(i)]));
  }
  return l;
}

const std::vector<ListRow>& results_list(const ResultsPanel& p, const ResultsChoice& c) {
  return c.details ? p.kills : p.salvage;
}

void clamp_choice(const ResultsPanel& p, ResultsChoice& c) {
  const int32_t n = static_cast<int32_t>(results_list(p, c).size());
  c.top = std::max(0, std::min(c.top, n - kListRows));
}

std::vector<std::string> pay_lines(const ResultsPanel& p, size_t width) {
  std::vector<std::string> l = p.pay;
  if (p.repairs.empty()) return l;
  std::string line = p.repairs.back();
  for (const std::string& r : p.repairs)
    if (r.size() <= width) {
      line = r;
      break;
    }
  l.insert(l.begin() + static_cast<long>(std::min(p.repairs_at, l.size())), line);
  return l;
}

ScreenView results_view(const ResultsPanel& p, const ResultsChoice& choice, bool wide) {
  ResultsChoice c = choice;
  clamp_choice(p, c);
  const ResultsWidths& w = wide ? kResultsWide : kResultsNarrow;
  ScreenView v;
  v.backdrop = p.scene;
  // The list's rows from `top`, each cut to the panel (a long part's name keeps its start).
  const std::vector<ListRow>& list = results_list(p, c);
  std::vector<std::string> rows;
  for (int32_t i = c.top; i < c.top + kListRows && i < static_cast<int32_t>(list.size()); ++i) {
    std::string l = list[static_cast<size_t>(i)].words;
    if (l.size() > w.status) l = l.substr(0, w.status - 3) + "...";
    rows.push_back(l);
  }
  std::string list_title = c.details ? p.kills_title : "SALVAGE TO STORES";
  if (static_cast<int32_t>(list.size()) > kListRows)
    list_title += " (" + std::to_string(c.top + 1) + "-" + std::to_string(c.top + static_cast<int32_t>(rows.size())) + " of " +
                  std::to_string(list.size()) + ")";
  v.blocks = {at_place("OUTCOME", {p.outcome}),
              at_place("WHERE", fit_lines(wrap(p.where, w.column), wide ? 4 : 7)),
              at_place("PAY_TITLE", {c.details ? p.mech_title : "PAYMENT"}),
              at_place("PAY", c.details ? fit_lines(p.mech) : fit_lines(pay_lines(p, w.status), kPayLines)),
              at_place("CREW_TITLE", {c.details ? "" : "CREW STATUS"}),
              at_place("CREW", c.details ? std::vector<std::string>{} : fit_lines(p.crew, kCrewLines)),
              at_place("LIST_TITLE", {list_title}),
              at_place("LIST", rows),
              at_place("MESSAGE", fit_lines(wrap(p.message, w.message), kMessageLines))};
  v.buttons.assign(kResultsButtonCount, ButtonState{});
  v.buttons[kResultsReplay - 1].there = p.replay;
  v.buttons[kResultsDetails - 1].words = c.details ? "SUMMARY" : "DETAILS";
  const bool scrolls = static_cast<int32_t>(list.size()) > kListRows;
  v.buttons[kResultsListUp - 1].there = v.buttons[kResultsListDown - 1].there = scrolls;
  v.buttons[kResultsListUp - 1].on = c.top > 0;
  v.buttons[kResultsListDown - 1].on = c.top + kListRows < static_cast<int32_t>(list.size());
  // The pictures: the ending's (the larger at 16:9), the House's crest.
  v.pictures.assign(kResultsCrest + 1, "");
  v.pictures[kResultsPicture] = ending_image(p.ending, wide);
  v.pictures[kResultsCrest] = crest_image(p.house);
  v.panels.assign(kResultsCrest + 1, true);
  v.panels[kResultsCrest] = !v.pictures[kResultsCrest].empty();
  return v;
}

namespace {
// MW_MAIN's DS 0x1090 strings (the port's MW_MAIN_1090.c import): offset and the largest size.
struct ExeText {
  size_t at, size;
};
constexpr ExeText kGarrison{0x140D, 0x56};
constexpr ExeText kKuritaFailure{0x149B, 0x8D}, kLiaoFailure{0x1528, 0x73}, kGeneralFailure{0x159B, 0x91};
constexpr ExeText kKuritaSuccess{0x1674, 0x62}, kLiaoSuccess{0x16D6, 0x6C}, kGeneralSuccess{0x1742, 0x44};
constexpr ExeText kDarkWingVictory{0x1A37, 0x185}, kDarkWingFailed{0x1BBC, 0x49};
constexpr ExeText kSupply{0x178C, 0xA8}, kPerformance{0x1834, 0xAC}, kTransport{0x18E0, 0x98}, kNoPay{0x19A2, 0x95};

// The text there as one paragraph (its lines joined with spaces, blank ones dropped); empty when it isn't text (no
// exe, another exe).
std::string exe_text(const ExeText& t) {
  const std::vector<uint8_t>& d = mw_main_data_segment();
  if (d.size() < t.at + t.size) return "";
  std::string out, word;
  size_t n = 0;
  for (size_t i = t.at; i < t.at + t.size && d[i]; ++i, ++n) {
    const uint8_t c = d[i];
    if (c == '\r' || c == '\n') {
      if (!out.empty() && out.back() != ' ') out += ' ';
      continue;
    }
    if (c < 0x20 || c > 0x7e) return "";  // not MW1's text
    out += static_cast<char>(c);
  }
  if (n == t.size || n < 16) return "";  // no end inside its size, or too short: not this text
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}
}  // namespace

Ending ending_of(Outcome o, bool anyone_lost) {
  if (o == Outcome::kSuccess) return Ending::kWin;
  return anyone_lost ? Ending::kLost : Ending::kRetreat;
}

std::string ending_image(Ending e, bool large) {
  const char* n = e == Ending::kWin ? "mw1_end_win" : e == Ending::kRetreat ? "mw1_end_retreat" : "mw1_end_lost";
  return std::string(n) + (large ? "_l" : "");
}

std::string crest_image(int32_t house) {
  if (house < 0 || house >= kHouseCount) return "";
  std::string n = kHouses[house].name;
  for (char& ch : n) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return "mw1_icon_house_" + n;
}

Deduction deduction_of(Outcome o, int32_t payout_percent, int32_t pick) {
  if (o != Outcome::kSuccess || payout_percent >= 100) return Deduction::kNone;
  if (payout_percent <= 0) return Deduction::kNoPay;
  return pick <= 0 ? Deduction::kSupply : pick == 1 ? Deduction::kPerformance : Deduction::kTransport;
}

std::vector<std::string> outcome_message(int32_t house, Outcome o, bool garrison, Deduction d) {
  std::vector<std::string> p;
  std::string first;
  if (garrison) first = exe_text(kGarrison);
  else if (o == Outcome::kSuccess) first = exe_text(house == kKurita ? kKuritaSuccess : house == kLiao ? kLiaoSuccess : kGeneralSuccess);
  else first = exe_text(house == kKurita ? kKuritaFailure : house == kLiao ? kLiaoFailure : kGeneralFailure);
  if (first.empty()) return p;
  p.push_back(first);
  const ExeText* t = d == Deduction::kSupply ? &kSupply : d == Deduction::kPerformance ? &kPerformance
                     : d == Deduction::kTransport ? &kTransport : d == Deduction::kNoPay ? &kNoPay : nullptr;
  if (t) {
    const std::string s = exe_text(*t);
    if (!s.empty()) p.push_back(s);
  }
  return p;
}

std::string dark_wing_failed() { return exe_text(kDarkWingFailed); }
std::string dark_wing_victory() { return exe_text(kDarkWingVictory); }

std::vector<std::string> wrap(const std::vector<std::string>& paragraphs, size_t width) {
  std::vector<std::string> lines;
  for (const std::string& para : paragraphs) {
    std::string line;
    size_t i = 0;
    while (i < para.size()) {
      while (i < para.size() && para[i] == ' ') ++i;
      size_t e = i;
      while (e < para.size() && para[e] != ' ') ++e;
      if (e == i) break;
      const std::string w = para.substr(i, e - i);
      if (!line.empty() && line.size() + 1 + w.size() > width) lines.push_back(line), line.clear();
      line += (line.empty() ? "" : " ") + w;
      i = e;
    }
    if (!line.empty()) lines.push_back(line);
  }
  return lines;
}

ResultsState::ResultsState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Results State", kResultsFile, kResultsDialog, kResultsButtons, kResultsContinue, std::move(pressed)) {}

}  // namespace mw1::results
