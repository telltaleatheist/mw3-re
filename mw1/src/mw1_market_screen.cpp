#include "mw1_market_screen.h"

#include <cstdlib>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

#include "mech3/mech_names.h"
#include "mw1_market.h"
#include "mw1_mech_renders.h"
#include "mw1_pilots.h"

namespace mw1::market_ui {
namespace {
const std::vector<ScreenButton> kMarketButtons = {
    {"BUY", "BUY"},       {"SELL", "SELL"},           {"REPAIR", "REPAIR"}, {"RELOAD", "RELOAD"},
    {"LAB", "MECH LAB"},  {"BACK", "BACK"},           {"STOCK_UP", "", true},     {"STOCK_DOWN", "", true},
    {"OWN_UP", "", true},       {"OWN_DOWN", "", true}};
constexpr size_t kNameWidth = 14;  // the lists' columns (the line font is fixed-width)
constexpr size_t kWhoWidth = 11;
// The lists' terminals (the layouts' PANEL4 and PANEL5): left and right, the arrows' column inside the right edge.
constexpr int32_t kListLeft[2] = {188, 200}, kListRight[2] = {188 + 436, 200 + 637}, kArrowColumn = 40;

std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}
std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
std::string padded(std::string s, size_t w) {
  if (s.size() > w) s.resize(w);
  s.append(w - s.size(), ' ');
  return s;
}
std::string tons(int32_t mech) {
  char t[8];
  std::snprintf(t, sizeof t, "%3dt ", static_cast<int>(tier0_price(mech) + kPriceOffset) / kPricePerTon);
  return t;
}
// A Clan mech starred.
std::string listed(int32_t mech) { return clan_mech(mech) ? mech_title(mech) + "*" : mech_title(mech); }
std::string price_column(int64_t price) {
  std::string p = thousands(price);
  if (p.size() < 10) p.insert(0, 10 - p.size(), ' ');
  return p;
}
// Who pilots owned mech i: you (0), the lancemate assigned to it, or nobody (a spare).
std::string pilot_of(const Company& c, int32_t i) {
  if (i == 0) return "YOU";
  const OwnedMech& o = c.hangar[static_cast<size_t>(i - 1)];
  return o.pilot ? pilot_title(o.pilot) : "spare";
}
// The first row shown, moved only as far as keeps `chosen` in view.
int32_t scrolled(int32_t top, int32_t chosen, int32_t count) {
  top = std::min(top, chosen);
  top = std::max(top, chosen - (kMarketRows - 1));
  return std::clamp(top, 0, std::max(0, count - kMarketRows));
}
}  // namespace

const char* market_button_name(int32_t id) { return id >= 1 && id <= kMarketButtonCount ? kMarketButtons[id - 1].name : ""; }

void clamp_choice(const Company& c, MarketChoice& m) {
  const int32_t stock = static_cast<int32_t>(c.market.stock.size()), owned = owned_count(c);
  m.stock = stock == 0 ? 0 : std::clamp(m.stock, 0, stock - 1);
  m.owned = std::clamp(m.owned, 0, owned - 1);
  m.stock_top = scrolled(m.stock_top, m.stock, stock);
  m.owned_top = scrolled(m.owned_top, m.owned, owned);
}

bool market_row_at(int32_t x, int32_t y, bool wide, bool* own, int32_t* row) {
  const int32_t l = wide ? 1 : 0;
  if (x < kListLeft[l] || x >= kListRight[l] - kArrowColumn) return false;
  for (int32_t list = 0; list < 2; ++list) {
    const int32_t first = kMarketListTop[list] + kMarketRowsFrom - 3;
    if (y < first || y >= first + kMarketRows * kMarketRowGap) continue;
    if (own) *own = list == 1;
    if (row) *row = (y - first) / kMarketRowGap;
    return true;
  }
  return false;
}

bool live_mech_wanted() {
  const char* s = std::getenv("MW1_LIVE_MECH");
  return s && *s == '1';
}

int32_t market_shown_mech(const Company& c, const MarketChoice& m) {
  if (const char* s = std::getenv("MW1_SPIKE_MECH")) return std::atoi(s);
  return m.own_shown || c.market.stock.empty() ? owned_mech(c, m.owned) : c.market.stock[static_cast<size_t>(m.stock)];
}

ScreenView market_view(const Company& c, const MarketChoice& m) {
  ScreenView v;
  v.blocks = {at_place("PLANET", {upper(c.planet)}),
              at_place("HOUSE", {"HOUSE " + upper(kHouses[c.planet_house].name) + " \xB7 THE MECH COMPLEX"}),
              at_place("PLACE", {thousands(c.funds) + " C-BILLS"}),
              at_place("PLACE_SUB", {"TIER " + std::to_string(c.tier) + " WORLD \xB7 SELLING AT " +
                                     std::to_string(sell_percent(c.tier)) + "%"}),
              at_place("STATUS_WORLD", {upper(c.planet)}),
              at_place("STATUS", {m.note.empty() ? "BUY takes from FOR SALE; the rest act on YOUR MECHS." : m.note})};
  v.panels.assign(kMarketPanelCount, true);
  // FIXES.md item 14: the chosen mech's render under the buttons (none till the renders are made).
  const int32_t shown = market_shown_mech(c, m);
  v.pictures.assign(kMarketPanelCount, "");
  // The spike (mech3/model_view.h): the mech drawn live in the panel, its picture none.
  const bool live = live_mech_wanted();
  if (!live && renders_ready() && shown >= 0) v.pictures[kMarketPanelMech] = render_image(shown, RenderUse::kPanel);
  v.panels[kMarketPanelMechFrame] = v.panels[kMarketPanelMech] = (live || renders_ready()) && shown >= 0;

  // FOR SALE: the stock at the buying price; a price over the funds dimmed (FIXES.md item 41, Owen: what he can
  // afford at a glance): its row in S_ROWS_DIM's darker green, a blank line in its place in S_ROWS (and the other way).
  std::vector<std::string> stock, stock_dim;
  bool any_clan = false;
  for (int32_t i = m.stock_top; i < static_cast<int32_t>(c.market.stock.size()) && i < m.stock_top + kMarketRows; ++i) {
    const int32_t mech = c.market.stock[static_cast<size_t>(i)];
    const std::string row = std::string(i == m.stock ? "> " : "  ") + padded(listed(mech), kNameWidth) + tons(mech) +
                            price_column(buy_price(mech, c.tier));
    const bool afford = buy_price(mech, c.tier) <= c.funds;
    stock.push_back(afford ? row : ""), stock_dim.push_back(afford ? "" : row);
  }
  for (int32_t mech : c.market.stock) any_clan = any_clan || clan_mech(mech);
  if (stock.empty()) stock.push_back("  Nothing for sale here.");
  // The dealer's limit on the YOUR MECHS title (the column header has no room for it: past the 4:3 canvas, the
  // mw1screenfit flake of FIXES 3+5, only when the market rolled a limit).
  const std::string limit =
      c.market.limit < 0 ? "" : " \xB7 DEALER BUYS " + std::to_string(c.market.limit - c.market.sold) + " MORE";
  v.blocks.push_back(at_place("S_TITLE", {"FOR SALE"}));
  v.blocks.push_back(at_place("S_HEAD", {"  " + padded("MECH", kNameWidth) + "TONS      PRICE" + (any_clan ? "   * Clan: twice" : "")}));
  v.blocks.push_back(at_place("S_ROWS", stock));
  if (std::any_of(stock_dim.begin(), stock_dim.end(), [](const std::string& l) { return !l.empty(); }))
    v.blocks.push_back(at_place("S_ROWS_DIM", stock_dim));

  // YOUR MECHS: who pilots each, its value.
  std::vector<std::string> owned;
  for (int32_t i = m.owned_top; i < owned_count(c) && i < m.owned_top + kMarketRows; ++i) {
    const int32_t mech = owned_mech(c, i);
    owned.push_back(std::string(i == m.owned ? "> " : "  ") + padded(listed(mech), kNameWidth) + tons(mech) +
                    padded(pilot_of(c, i), kWhoWidth) + price_column(sell_price(mech, c.tier)));
  }
  v.blocks.push_back(at_place("O_TITLE", {"YOUR MECHS" + limit}));
  v.blocks.push_back(at_place("O_HEAD", {"  " + padded("MECH", kNameWidth) + "TONS " + padded("PILOT", kWhoWidth) + "     VALUE"}));
  v.blocks.push_back(at_place("O_ROWS", owned));

  v.buttons.resize(kMarketButtonCount);
  auto on = [&](int32_t id, bool b) { v.buttons[static_cast<size_t>(id - 1)].on = b; };
  on(kMarketBuy, !c.market.stock.empty());
  on(kMarketSell, owned_count(c) > 1);
  on(kMarketRepair, false), on(kMarketReload, false);  // VISION.md: nothing to mend between missions
  on(kMarketStockUp, m.stock > 0);
  on(kMarketStockDown, m.stock + 1 < static_cast<int32_t>(c.market.stock.size()));
  on(kMarketOwnUp, m.owned > 0);
  on(kMarketOwnDown, m.owned + 1 < owned_count(c));
  return v;
}

MarketState::MarketState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Market State", kMarketFile, kMarketDialog, kMarketButtons, kMarketBack, std::move(pressed)) {}

void MarketState::resume(uint32_t arg) {
  if (resumed_) resumed_();
  Mw1State::resume(arg);
}

}  // namespace mw1::market_ui
