#include "mw1_office.h"

#include <algorithm>
#include <cctype>

#include "mw1_pilots.h"

namespace mw1::office_ui {
namespace {
const std::vector<ScreenButton> kOfficeButtons = {{"SUBMIT", "NEGOTIATE"},  {"ACCEPT", "ACCEPT"},   {"BACK", "LEAVE"},
                                                  {"PAY_DOWN", "", true},          {"PAY_UP", "", true},         {"SALVAGE_DOWN", "", true},
                                                  {"SALVAGE_UP", "", true},        {"ADVANCE_DOWN", "", true},   {"ADVANCE_UP", "", true},
                                                  {"REQUEST", "REQUEST MISSION"}, {"NEXT", "NEXT OFFER"}};
// MW1's office texts in MW_MAIN's data segment: where, and their room (the port's MW_MAIN_1090 imports).
constexpr int32_t kTerminatedAt = 0x4C25, kTerminatedSize = 0x47;
constexpr int32_t kRefusalAt = 0x4C87, kRefusalSize = 0x5F;
constexpr int32_t kAcceptedAt = 0x4C6C, kAcceptedSize = 0x1B;

std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}
std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

// A text of MW1's: carriage-return lines up to its NUL within its room, printable; none when it isn't there so.
std::vector<std::string> exe_lines(int32_t at, int32_t size) {
  const std::vector<uint8_t>& d = mw_main_data_segment();
  std::vector<std::string> lines;
  std::string l;
  for (int32_t i = 0; i < size; ++i) {
    const size_t p = static_cast<size_t>(at + i);
    if (p >= d.size()) return {};
    const uint8_t b = d[p];
    if (b == 0) {
      if (!l.empty()) lines.push_back(l);
      return lines;
    }
    if (b == '\r') {
      if (!l.empty()) lines.push_back(l);
      l.clear();
    } else if (b >= 0x20 && b < 0x7f) {
      l += static_cast<char>(b);
    } else {
      return {};
    }
  }
  return {};  // no NUL in its room: not MW1's text
}

std::string money(int32_t k) { return thousands(int64_t{k} * 1000); }
}  // namespace

const char* office_button_name(int32_t id) { return id >= 1 && id <= kOfficeButtonCount ? kOfficeButtons[id - 1].name : ""; }

void step_ask(const Contract& k, Request& a, int32_t button) {
  switch (button) {
    case kOfficePayUp:
    case kOfficePayDown: {
      if (a.payment < 0) a.payment = k.payment;
      const int32_t to = a.payment + (button == kOfficePayUp ? kRequestPaymentStep : -kRequestPaymentStep);
      if (to >= 0 && to <= kRequestPaymentMax) a.payment = to;
      return;
    }
    case kOfficeSalvageUp:
    case kOfficeSalvageDown:
      if (a.salvage < 0) a.salvage = k.salvage_percent;
      a.salvage = std::clamp(a.salvage + (button == kOfficeSalvageUp ? 1 : -1), 0, kRequestPercentMax);
      return;
    case kOfficeAdvanceUp:
    case kOfficeAdvanceDown:
      if (a.advance < 0) a.advance = k.advance_percent;
      a.advance = std::clamp(a.advance + (button == kOfficeAdvanceUp ? 1 : -1), 0, kRequestPercentMax);
      return;
    default: return;
  }
}

std::vector<std::string> terminated_words() {
  std::vector<std::string> l = exe_lines(kTerminatedAt, kTerminatedSize);
  return l.empty() ? std::vector<std::string>{"The House has broken off negotiations."} : l;
}
std::vector<std::string> refusal_words(int32_t house) {
  std::vector<std::string> l = exe_lines(kRefusalAt, kRefusalSize);
  return l.empty() ? std::vector<std::string>{"House " + std::string(kHouses[house].name) + " won't do business with you."} : l;
}
std::string accepted_words() {
  const std::vector<std::string> l = exe_lines(kAcceptedAt, kAcceptedSize);
  return l.empty() ? std::string("The contract is accepted.") : l[0];
}

std::vector<std::string> answer_lines(Answer a, const Company& c, const Contract& k) {
  if (a == Answer::kAgreed) return {accepted_words()};
  if (a == Answer::kTerminated) return terminated_words();
  std::vector<std::string> l = {"The House counters: " + money(k.payment) + ", salvage " + std::to_string(k.salvage_percent) +
                                "%, advance " + std::to_string(k.advance_percent) + "%."};
  if (last_round(c, k)) l.push_back("The House's liaison is losing patience.");
  return l;
}

std::string office_hint(int32_t id) {
  if (id == kOfficeSalvageDown || id == kOfficeSalvageUp) return "Salvage pays this % of the wrecks' worth after your repairs.";
  return "";
}

ScreenView office_view(const Company& c, const Contract& k, const OfficeChoice& o, const std::vector<std::string>& offer,
                       const std::vector<std::string>& closed, const std::string& hint) {
  ScreenView v;
  const bool open = closed.empty(), up = open && !offer.empty();  // the House deals; an offer is up
  auto row = [&](const char* name, const std::string& theirs, int32_t asked, const std::string& ask_words) {
    // The House's offer highlighted (FIXES.md item 20: the pay among the key facts).
    return std::vector<std::string>{name, "  Offer: " + highlighted(theirs) + (asked >= 0 ? "    Ask: " + ask_words : std::string())};
  };
  std::vector<std::string> notes = o.answer;
  if (up && notes.empty()) notes = {"Change a term with its arrows, then NEGOTIATE;", "ACCEPT takes the offer as it stands."};
  if (open && !up && notes.empty()) notes = {"REQUEST MISSION: the House's liaison", "puts a contract on offer."};
  const std::string house = kHouses[c.planet_house].name;
  v.blocks = {at_place("PLANET", {upper(c.planet)}),
              at_place("HOUSE", {"HOUSE " + upper(house)}),
              at_place("PLACE", {"HOUSE OFFICE"}),
              at_place("PLACE_SUB", {"FUNDS " + thousands(c.funds) + " C-BILLS"}),
              at_place("STATUS_WORLD", {upper(c.planet)}),
              at_place("STATUS", {!hint.empty() ? hint
                                  : up        ? "The House's liaison will hear your terms."
                                  : open      ? "Ask the liaison for a mission."
                                              : "Nothing to negotiate here now."}),
              at_place("C_TITLE", {"CONTRACT ON OFFER"}),
              at_place("C_LINES", up ? offer : std::vector<std::string>{"No contract on offer."}),
              at_place("NOTES", notes),
              at_place("T_TITLE", {"TERMS"})};
  if (up) {
    v.blocks.push_back(at_place("ROW1", row("Payment (C-bills)", money(k.payment), o.ask.payment, money(o.ask.payment))));
    v.blocks.push_back(at_place("ROW2", row("Salvage", std::to_string(k.salvage_percent) + "%", o.ask.salvage,
                                            std::to_string(o.ask.salvage) + "%")));
    v.blocks.push_back(at_place("ROW3", row("Advance", std::to_string(k.advance_percent) + "%", o.ask.advance,
                                            std::to_string(o.ask.advance) + "%")));
  } else {
    v.blocks.push_back(at_place("ROW1", open ? std::vector<std::string>{} : closed));
    v.blocks.push_back(at_place("ROW2", {}));
    v.blocks.push_back(at_place("ROW3", {}));
  }
  // The world's House's liaison (MW1's office: the House's own).
  v.pictures.assign(kOfficePanelLiaison + 1, "");
  std::string h;
  for (char ch : house) h += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  v.pictures[kOfficePanelLiaison] = "mw1_liaison_" + h + "_office";  // item 50: lined up with the buttons
  v.buttons.resize(kOfficeButtonCount);
  v.buttons[kOfficeSubmit - 1].on = up && o.ask.any();
  v.buttons[kOfficeAccept - 1].on = up;
  v.buttons[kOfficeBack - 1].on = true;
  for (int32_t b = kOfficePayDown; b <= kOfficeAdvanceUp; ++b) v.buttons[b - 1].there = up;
  // REQUEST MISSION until an offer is up, then NEXT OFFER in its place.
  v.buttons[kOfficeRequest - 1].there = !up, v.buttons[kOfficeRequest - 1].on = open;
  v.buttons[kOfficeNext - 1].there = up;
  return v;
}

OfficeState::OfficeState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Office State", kOfficeFile, kOfficeDialog, kOfficeButtons, kOfficeBack, std::move(pressed)) {}

}  // namespace mw1::office_ui
