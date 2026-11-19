#pragma once
// MW1's Mech Complex (the hub's MECH COMPLEX; mw1_market.h's rules) on MW1's screen template (mw1_screen.h), its
// layout mw1market.zrd in MW1's data folder. Owen's shape (FIXES.md items 8 and 13): one page, MW1's buttons down
// the left (BUY, SELL, REPAIR, RELOAD, MECH LAB, BACK) and two lists, FOR SALE (the world's stock, the buying
// prices) over YOUR MECHS (the company's, who pilots each, the selling prices). BUY takes FOR SALE's chosen mech;
// SELL, REPAIR, RELOAD and MECH LAB act on YOUR MECHS' chosen one. MECH LAB is the lab's way in: Pirate's Moon's lab
// on that mech, its pilot chosen there (mw1_lab_pilot.h), back here after. REPAIR and RELOAD are greyed (every
// mission starts repaired and armed, VISION.md; research/repairs.md). The lists' rows take the mouse, their arrows
// scroll them. Escape is BACK.
#include <cstdint>
#include <functional>
#include <string>

#include "mw1_campaign.h"
#include "mw1_screen.h"

namespace mw1::market_ui {

inline constexpr const char* kMarketFile = "mw1market.zrd";
inline constexpr const char* kMarketDialog = "MW1MARKETDIALOG";

enum MarketButton : int32_t {
  kMarketBuy = 1, kMarketSell, kMarketRepair, kMarketReload, kMarketLab, kMarketBack,  // down the left
  kMarketStockUp, kMarketStockDown, kMarketOwnUp, kMarketOwnDown                       // the lists' arrows
};
inline constexpr int32_t kMarketButtonCount = kMarketOwnDown;
const char* market_button_name(int32_t id);

// The layout's PANELs in order: the two plaques, the status bar, the two lists' terminals.
enum MarketPanel : int32_t { kMarketPanelPlaque, kMarketPanelPlaque2, kMarketPanelStatusBar, kMarketPanelStock, kMarketPanelOwned,
                             kMarketPanelMechFrame, kMarketPanelMech, kMarketPanelCount };  // item 14: the chosen mech

// The lists: each shows kMarketRows rows from its first shown (`*_top`), scrolled to keep the chosen one in view.
inline constexpr int32_t kMarketRows = 8;

// What the player has chosen in each list, how far each is scrolled, and the line said after the last button (a
// refusal's reason, or what was done).
struct MarketChoice {
  int32_t stock = 0, owned = 0;
  int32_t stock_top = 0, owned_top = 0;
  std::string note;
  bool own_shown = false;  // the picture's (item 14): YOUR MECHS' chosen one, else FOR SALE's
};
// The choices kept inside the lists as they change, each list scrolled to show its chosen row.
void clamp_choice(const Company& c, MarketChoice& m);

// The lists' rows by the mouse (a click chooses one): the shown row at (x, y) in the layout's places (the wide
// layout's when `wide`), of YOUR MECHS (`own`) or FOR SALE; false off both (their titles, their arrows). The part of
// the screen that hears the mouse is kMarketListArea (both layouts' lists inside it).
inline constexpr int32_t kMarketListArea[4] = {188, 76, 837, 428};  // left, top, right, bottom
inline constexpr int32_t kMarketListTop[2] = {76, 256};              // FOR SALE's terminal, YOUR MECHS'
inline constexpr int32_t kMarketRowsFrom = 52, kMarketRowGap = 15;   // a terminal's first row, from its top; the gap
bool market_row_at(int32_t x, int32_t y, bool wide, bool* own, int32_t* row);

// The screen's texts and buttons for the company's Mech Complex (pure: the tests read it).
ScreenView market_view(const Company& c, const MarketChoice& m);
// The mech the panel shows (the chosen row's), and whether it is drawn live (the spike: MW1_LIVE_MECH=1).
int32_t market_shown_mech(const Company& c, const MarketChoice& m);
bool live_mech_wanted();

// The Mech Complex's state: pushed in the hub's place by the loop. `resumed` hears it come back to the top (the
// mech lab closed over it).
class MarketState : public Mw1State {
 public:
  MarketState(zgame::Handle h, std::function<void(int32_t)> pressed);
  void resume(uint32_t arg) override;
  void set_resumed(std::function<void()> f) { resumed_ = std::move(f); }

 private:
  std::function<void()> resumed_;
};

}  // namespace mw1::market_ui

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace market_ui;
}  // namespace mw1
