// The salvage screen (docs/campaign.md section 10). Original file unknown
// (0x0047d2f0..0x0047e520, after the debriefing's); see salvage_screen.h.
#include "mech3/salvage_screen.h"

#include <cmath>

#include "crt/msvc_math.h"
#include "mech3/briefing.h"
#include "mech3/mech_config.h"
#include "mech3/pilot.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

int32_t lab_rounds(int32_t weapon, int32_t n);  // weapon.h (whose Handle is mech.h's)

namespace {
constexpr float kTonsPerBase = 300.0f;  // 0x00470cc0
constexpr int32_t kMechsPerBase = 2;    // 0x00470cd0
// The cards' layout from ICON_PILOT ([0x005e5b08..0x005e5b18]).
constexpr int32_t kBayDx = 120, kBayDy = 80, kLanceDx = 60, kBayTop = -50, kLanceDy = -70;
constexpr int32_t kTonsText = 0x244;    // the message after the tons

ztwod::Text* label(ztwod::Control& c, size_t i) { return i < c.labels.size() ? c.labels[i] : nullptr; }

// A card bound to ICON_PILOT, then it and its labels moved by (dx, dy) (the
// functors 0x0047f1c0, 0x0047f140, 0x0047f1e0).
void place(LanceCard& c, int32_t dx, int32_t dy) {
  c.set_pos(c.get_x() + dx, c.get_y() + dy);
  for (ztwod::Text* t : c.labels) t->set_pos(t->get_x() + dx, t->get_y() + dy);
}

// An equipment item counted in shares (+0x18 1 or 2).
bool shared(const ConfigItem* item) {
  const int32_t k = static_cast<const EquipmentItem*>(item)->equipment;
  return k == 1 || k == 2;
}
}  // namespace

// Mech3.exe 1.2 @ 0x0047da70: no selection, only the press.
void SalvageList::select(int32_t index) {
  if (auto* s = dynamic_cast<MenuScreen*>(owner)) s->on_button(id, 0, index);
}

// Mech3.exe 1.2 @ 0x0047d3b0
SalvageScreen::SalvageScreen(Engine& e, Handle st)
    : MenuScreen(e, st),
      accept(e, -1),
      bays{LanceCard(e), LanceCard(e), LanceCard(e), LanceCard(e), LanceCard(e), LanceCard(e)},
      lance{LanceCard(e), LanceCard(e), LanceCard(e), LanceCard(e)} {
  const int32_t fast = e.exchange_fast_menu(0);
  if (load("dlgSalvage.zrd", "SALVAGEDIALOG", true)) {
    bind_primitive(&back, "BACKGROUND");
    bind_button(&mission, "MISSION_SALVAGE");
    bind_button(&player, "PLAYER_SALVAGE");
    bind_button(&text, "SALVAGE_TEXT");
    accept.id = kAccept;
    bind_button(&accept, "ICON_ACCEPT");
    for (int32_t i = 0; i < 4; ++i) {  // in a row above
      lance[i].id = 4 + i;
      bind_button(&lance[i], "ICON_PILOT");
      place(lance[i], kLanceDx * i, kLanceDy);
    }
    // Two columns of three below, bound left column first (ids 4..6, 7..9).
    for (int32_t col = 0; col < 2; ++col)
      for (int32_t row = 0; row < 3; ++row) {
        LanceCard& c = bays[row * 2 + col];
        c.id = 4 + col * 3 + row;
        bind_button(&c, "ICON_PILOT");
        place(c, kBayDx * col, kBayTop + kBayDy * (row + 1));
      }
    unload();
  }
  set_active(1);
  set_flags_all(0);
  player.x20d = 0;  // +0x1590d
  player.store = e.store_handle(e.game_store());  // 0x00479430
  player.x218 = 0;
  player.refill();
  mission.store = e.game_kind() == kCampaign ? e.store_handle(e.campaign_salvage()) : e.mission_salvage_store();
  mission.x218 = 0;
  mission.refill();
  refill();
  e.set_fast_menu(fast);
}

// Mech3.exe 1.2 @ 0x0047e490: ACCEPT pops the state; a row (not the first)
// moves its item across, and a move refills.
void SalvageScreen::on_button(int32_t id, int32_t a, int32_t row) {
  (void)a;
  Engine& e = engine();
  auto item_at = [&](SalvageList& l) {  // 0x0048d960
    if (row < 0 || row >= l.item_count()) return static_cast<ConfigItem*>(nullptr);
    // The store's own item (the engine hands it out const).
    return const_cast<ConfigItem*>(e.item_of(static_cast<Handle>(l.items[static_cast<size_t>(row)]->value)));
  };
  bool moved = false;
  if (id == kAccept) {
    e.pop_state(0);
  } else if (id == kPlayer) {
    if (row > 0) moved = salvage_to_mission(e.store_of(mission.store), e.store_of(player.store), item_at(player));
  } else if (id == kMission && row > 0) {
    moved = salvage_to_player(e.store_of(player.store), e.store_of(mission.store), item_at(mission),
                              e.campaign_units());
  }
  if (moved) refill();
}

// Mech3.exe 1.2 @ 0x0047dcf0
void SalvageScreen::refill() {
  Engine& e = engine();
  for (int32_t i = 0; i < 4; ++i) {  // the lance: green while in the mission's count
    LanceCard& c = lance[i];
    if (GameSlot* s = e.game_slot(i)) {
      c.set_mech_image(s->mech, i < e.game_slot_count() ? 1 : 3);
      c.show_name(true);
    } else {
      c.set_mech_image(-1, 3);
    }
    if (ztwod::Text* t = label(c, 0)) t->hide();
  }
  const int32_t bases = e.campaign_units();  // 0 without a campaign
  const int32_t room = kMechsPerBase * bases;
  const float capacity = kTonsPerBase * static_cast<float>(bases);
  Store* store = e.store_of(player.store);
  const float used = store ? store_tons(*store) : 0.0f;  // 0x0045b900
  int32_t bay = 0;
  if (Store* own = e.game_store())  // the player's mechs in the bays (0x00479430 again)
    for (const ConfigItem* it : own->items) {
      if (bay >= 6) break;
      if (it->kind() != 0) continue;
      const int32_t mech = static_cast<const BaseItem*>(it)->value;
      for (int32_t n = 0; n < it->count && bay < 6; ++n, ++bay) {
        bays[bay].set_mech_image(mech, 1);
        if (ztwod::Text* t = label(bays[bay], 0)) t->set_text_plain(e.mech_name_of(mech));  // 0x00484650
        bays[bay].show_name(false);
      }
    }
  for (; bay < 6; ++bay) {  // the empty bays the bases give, the rest hidden
    if (bay < room) {
      bays[bay].set_mech_image(-1, 3);
      if (ztwod::Text* t = label(bays[bay], 0)) t->set_text_plain("");
      bays[bay].show_name(false);
    } else {
      bays[bay].hide();
    }
  }
  for (SalvageList* l : {&player, &mission}) {
    l->on_leave();
    l->on_enter();
  }
  player.refill();
  mission.refill();
  if (ztwod::Text* t = label(text, 4)) t->set_text("%.1f %s", static_cast<double>(capacity), e.message(kTonsText));
  if (ztwod::Text* t = label(text, 3)) t->set_text("%.1f %s", static_cast<double>(used), e.message(kTonsText));
  if (ztwod::Text* t = label(text, 5)) {
    if (bases == 1) t->set_text_id(0x164, 1);
    else t->set_text_id(0x163, bases);
  }
}

// Mech3.exe 1.2 @ 0x0047e110. True once tried, moved or not.
bool salvage_to_mission(Store* to, Store* from, ConfigItem* item) {
  if (!to || !from || !item) return false;
  int32_t n = 1;
  const int k = item->kind();
  if (k == 2) {
    n = lab_rounds(static_cast<AmmoItem*>(item)->weapon, item->count);  // 0x004b4900
  } else if (k == 3 && shared(item)) {
    const float half = static_cast<float>(item->count) * 0.5f;  // [0x005c3ae0..0x005c3af4]
    n = half < 10.0f ? 1 : half < 20.0f ? 2 : half < 50.0f ? 10 : half < 200.0f ? 20 : half < 500.0f ? 40 : 100;
  }
  if (to->add_copy(item, n)) from->remove(item, n);
  return true;
}

// Mech3.exe 1.2 @ 0x0047e270. A mech takes a free bay (none free: the
// negative room, refused); shares up to two a ton of the room left.
bool salvage_to_player(Store* to, Store* from, ConfigItem* item, int32_t bases) {
  if (!to || !from || !item) return false;
  const float capacity = kTonsPerBase * static_cast<float>(bases);
  const int32_t room = kMechsPerBase * bases;
  const float used = store_tons(*to);
  const float left = capacity - used;
  int32_t n = 1;
  const int k = item->kind();
  if (k == 0) {
    const int32_t free = room - store_mechs(*to);
    n = free < 1 ? free : 1;
  } else if (k == 2) {
    n = lab_rounds(static_cast<AmmoItem*>(item)->weapon, item->count);
  } else if (k == 3 && shared(item)) {
    const int32_t fit = crt::msvc_ftol(std::floor(static_cast<double>(left)) * 2.0);
    n = item->count < fit ? item->count : fit;
  }
  if (n <= 0) return false;
  const float w = static_cast<float>(item->weight(n));
  if (!(used + w <= capacity)) return false;
  if (to->add_copy(item, n)) from->remove(item, n);
  return true;
}

}  // namespace mech3
