#pragma once
// MW1's results screen, AFTER ACTION (FIXES.md items 19, 31 and 38), its layout mw1results.zrd in MW1's data folder: a
// mission ends here, never on Pirate's Moon's debriefing (mech3/program_flow.h's skip_debriefing). Laid out as MW1's
// own after-action screen (Owen, item 38), over the world's scenery dimmed:
// - top left, the picture (item 31: by the outcome and the fates), the outcome (VICTORY, DEFEAT, RETREAT) and where
//   under it: the world and the employer, the objective and how long it took;
// - top right, the status: PAYMENT (the fee with any deduction, the salvage share, the repair bill, the total and the
//   funds), CREW STATUS (each mech that flew, its pilot and its condition, MW1's "JENNER 93 %"; a lancemate KILLED or
//   MISSING), then SALVAGE TO STORES, every part kept, a list its arrows (the wheel, the keys) scroll;
// - along the bottom, the outcome's message from the player's MW_MAIN.EXE (none without it) and the House's crest.
// DETAILS shows what doesn't fit there in the same panel: the enemy mechs destroyed, by whom and their values (in the
// list's place), and your mech by location. CONTINUE settles it and goes back to the hub; REPLAY (a failure) plays the
// contract again. 16:9 and 4:3 the same arrangement (<dialog>_WIDE the wider), the picture smaller at 4:3.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_campaign.h"
#include "mw1_repairs.h"
#include "mw1_screen.h"

namespace mw1::results {

inline constexpr const char* kResultsFile = "mw1results.zrd";
inline constexpr const char* kResultsDialog = "MW1RESULTSDIALOG";

enum ResultsButton : int32_t { kResultsContinue = 1, kResultsReplay, kResultsDetails, kResultsListUp, kResultsListDown };
inline constexpr int32_t kResultsButtonCount = kResultsListDown;
// The layout's PANELs in order: the dim, the picture's frame, the picture, the status terminal, the message's, the
// crest.
enum ResultsPanelId : int32_t { kResultsDim, kResultsFrame, kResultsPicture, kResultsStatus, kResultsMessage, kResultsCrest };
inline constexpr size_t kPayLines = 4, kCrewLines = 5;  // the status panel's: PAYMENT's lines, CREW STATUS's
inline constexpr int32_t kListRows = 6;                 // the list's rows (the salvage, DETAILS' kills)
inline constexpr size_t kTerminalLines = 8;             // DETAILS' mech table (its header and six locations)
inline constexpr size_t kMessageLines = 6;              // the message's lines along the bottom
// The letters a line holds (mw1_term_line, about 8 pixels a letter; mw1screenfit checks them): under the picture,
// the message's, the status panel's; the wide layout's and 4:3's.
struct ResultsWidths {
  size_t column, message, status;
};
inline constexpr ResultsWidths kResultsWide{29, 84, 62}, kResultsNarrow{23, 58, 43};

// The locations' names, in AfterAction's kMechLocations order.
inline constexpr const char* kLocationNames[6] = {"Head", "Torso", "Left arm", "Right arm", "Left leg", "Right leg"};

// ---- AFTER ACTION's picture and message (FIXES.md items 31 and 38: MW1's beginDebrief, the port's
// MW_MAIN_081B_STARTUP.c, read as reference) ----

// The picture: a win (and an uneventful garrison tour) the commander with a big gun (MW1's 0x4C); a failure with
// nobody lost, a retreat, kicking a rock (0x4E); a failure with someone lost (a lancemate KILLED or MISSING, or the
// commander's mech destroyed) the wrecked cockpit (0x48). Owen's picks (art: end_win, end_retreat, end_lost).
enum class Ending { kWin, kRetreat, kLost };
Ending ending_of(Outcome o, bool anyone_lost);
// Its image: "mw1_end_win", or the large one ("_l") the 16:9 page shows.
std::string ending_image(Ending e, bool large);
// The House's crest (the world's House, as MW1 draws it under the picture): "mw1_icon_house_kurita".
std::string crest_image(int32_t house);

// The line after a win paid short (settlementPayoutPercent below 100): the supply overrun, the "disappointment" or
// the House transports' abandonment (MW1 picks one of the three, MwRandomUpTo(2): `pick` 0..2); no pay at all, the
// breach of contract. None otherwise.
enum class Deduction { kNone, kSupply, kPerformance, kTransport, kNoPay };
Deduction deduction_of(Outcome o, int32_t payout_percent, int32_t pick);
// The message, from the player's MW_MAIN.EXE (the --mw1-game folder; never kept in the repo), each a paragraph (the
// exe's 40-column lines joined): the outcome's (the garrison tour's, else the House's success or failure line;
// a retreat is a failure), then the deduction's. Empty without the exe (or another exe): the screen shows none.
// `garrison`: a deployment resolved without combat (MW1's deploymentResolvedWithoutCombat; MW1: Critical Hit has
// none yet, so it is false today).
std::vector<std::string> outcome_message(int32_t house, Outcome o, bool garrison, Deduction d);
// The Dark Wing's (research/story.md, the story battles): MW1's attack-failed line (DS:1BBC) as the results' message
// of a lost or withdrawn attack; and its victory text (DS:1A37), shown on the event panel after a win. "" without it.
std::string dark_wing_failed();
std::string dark_wing_victory();
// Paragraphs as lines of at most `width` letters, broken at spaces (a word longer than the width alone on its line).
std::vector<std::string> wrap(const std::vector<std::string>& paragraphs, size_t width);

// A list row: its words and what it stands for (a salvage row: its item's place in the mission's salvage, so a keep
// list can take it later, item 38; -1 none).
struct ListRow {
  std::string words;
  int32_t item = -1;
};
// What the screen shows.
struct ResultsPanel {
  std::string planet, employer, outcome, scene;
  Ending ending = Ending::kWin;
  int32_t house = -1;                    // the world's House: its crest
  std::vector<std::string> where;        // under the picture: paragraphs (wrapped to the column)
  std::vector<std::string> pay, crew;    // the status panel's (cut to kPayLines, kCrewLines)
  // PAYMENT's repair line (FIXES.md item 57): its wordings, fullest first (repair_line_choices); the view puts the
  // first that fits the panel's width in at `repairs_at`. None: no line.
  std::vector<std::string> repairs;
  size_t repairs_at = 0;
  std::vector<ListRow> salvage;          // SALVAGE TO STORES, all of it (the list scrolls)
  std::string kills_title = "ENEMY MECHS DESTROYED";
  std::vector<ListRow> kills;            // DETAILS: each wreck, by whom, its value
  std::string mech_title = "YOUR MECH";
  std::vector<std::string> mech;         // DETAILS: by location
  std::vector<std::string> message;      // the exe's paragraphs (outcome_message), wrapped along the bottom
  bool replay = false;                   // a failure: REPLAY offered
};
// The player's way through it: DETAILS shown, the list scrolled to `top`.
struct ResultsChoice {
  bool details = false;
  int32_t top = 0;
};
// The list shown (the salvage, or DETAILS' kills), and `top` kept inside it.
const std::vector<ListRow>& results_list(const ResultsPanel& p, const ResultsChoice& c);
void clamp_choice(const ResultsPanel& p, ResultsChoice& c);
// The screen's texts, pictures and buttons (pure: the tests read it), for the wide layout or 4:3's.
ScreenView results_view(const ResultsPanel& p, const ResultsChoice& c, bool wide);
// PAYMENT's lines as shown: `pay` with the first of `repairs` no wider than `width` (else the last) at `repairs_at`.
std::vector<std::string> pay_lines(const ResultsPanel& p, size_t width);
// Lines cut to `n`: past it, the last line says "... and N more".
std::vector<std::string> fit_lines(std::vector<std::string> lines, size_t n = kTerminalLines);
// A mission's time as the results say it: "12:05" (minutes, seconds); "unknown" below 0.
std::string time_words(int32_t seconds);
// A mech's armour and structure left by location (percent, kMechLocations), from PM's lance bars: the head bar, the
// six torso bars together, the arms, the legs, each weighted by its points; -1 where it has none.
void location_left(const Bars& b, std::vector<int32_t>* armor, std::vector<int32_t>* structure);
// YOUR MECH's lines: each location's armour and structure left; destroyed, a line saying so first. Unknown: one line.
std::vector<std::string> mech_lines(const std::vector<int32_t>& armor, const std::vector<int32_t>& structure, bool destroyed);

// The screen's state: pushed over the hub by the loop when a mission is back. Escape is CONTINUE.
class ResultsState : public Mw1State {
 public:
  ResultsState(zgame::Handle h, std::function<void(int32_t)> pressed);
};

}  // namespace mw1::results

namespace mw1 {
using namespace results;
}  // namespace mw1
