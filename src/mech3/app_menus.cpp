// The ported menu screens and menu objects on the program's engine (our
// addition, not original code; docs/mainmenu.md, The port; docs/menus.md).
// The shell states reach their screens through Engine handles: construct,
// vcall, the Layer calls. A ported screen's handle is a block of the module
// memory the size of the original object, so the offsets the states add (a
// button's +0x111ec) still lead to its parts; a menu object's handle is a block
// of 0x10, its address for the hooks that name it.
#include <cstdio>
#include <vector>

#include "gamez/ztwod/twod.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_states.h"
#include "mech3/controls_menu.h"
#include "mech3/credits.h"
#include "mech3/mainmenu.h"
#include "mech3/menu_states.h"
#include "mech3/options_menu.h"
#include "mech3/pilot.h"
#include "mech3/pilot_menu.h"

namespace mech3 {

MenuScreen* GameEngine::screen_of(Handle h, uint32_t* offset) const {
  if (!h || screens_.empty()) return nullptr;
  auto it = screens_.upper_bound(h);
  if (it == screens_.begin()) return nullptr;
  --it;
  const uint32_t off = h - it->first;
  if (off >= it->second.size) return nullptr;
  if (offset) *offset = off;
  else if (off) return nullptr;
  return it->second.screen;
}

MenuState* GameEngine::menu_state_of(Handle h) const {
  auto it = menu_states_.find(h);
  return it == menu_states_.end() ? nullptr : it->second;
}

MenuState* GameEngine::popup_state_of(Handle h) const {
  uint32_t off = 0;
  auto* c = dynamic_cast<ControlsScreen*>(screen_of(h, &off));
  if (!c) return nullptr;
  if (off == 0x16760) return &c->axis_popup;
  if (off == 0x1677c) return &c->load_save_popup;
  return nullptr;
}

Handle GameEngine::adopt_screen(MenuScreen* screen, uint32_t size) {
  const Handle h = alloc_block(size);
  screens_[h] = {screen, size, true};
  return h;
}

Handle GameEngine::construct(uint32_t ctor, std::initializer_list<Arg> args) {
  const Handle a0 = args.size() ? args.begin()->v : 0;
  auto screen = [&](uint32_t size, MenuScreen* (*make)(GameEngine&, Handle)) {
    const Handle h = alloc_block(size);
    screens_[h] = {make(*this, a0), size};
    return h;
  };
  auto state = [&](MenuState* (*make)(Handle)) {
    const Handle h = alloc_block(0x10);
    menu_states_[h] = make(h);
    return h;
  };
  switch (ctor) {
    case 0x486260: {  // the main menu, new(0x12c1c)
      const Handle h = screen(0x12c1c, [](GameEngine& e, Handle s) -> MenuScreen* { return new MainMenu(e, s); });
      poke32(0x609854, h);  // its constructor's [0x00609854] = this
      return h;
    }
    case 0x4289a0:
      return screen(0x11ae0, [](GameEngine& e, Handle s) -> MenuScreen* { return new CreditsScreen(e, s); });
    case 0x4379f0:
      return screen(0x1407c, [](GameEngine& e, Handle s) -> MenuScreen* { return new GameOptionsScreen(e, s); });
    case 0x438f60:
      return screen(0x13f9c, [](GameEngine& e, Handle s) -> MenuScreen* { return new GfxAudioScreen(e, s); });
    case 0x438be0:
      return state([](Handle h) -> MenuState* { return new OptionsState(OptionsState::kGame, h); });
    case 0x43a640:
      return state([](Handle h) -> MenuState* { return new OptionsState(OptionsState::kGfxAudio, h); });
    case 0x42a590: {  // the controls screen, new(0x16c94): its popups' states sit at its offsets
      const Handle h = alloc_block(0x16c94);
      screens_[h] = {nullptr, 0x16c94};
      screens_[h].screen = new ControlsScreen(*this, a0, h);
      return h;
    }
    case 0x42d9d0:
      return state([](Handle h) -> MenuState* { return new OptionsState(OptionsState::kControls, h); });
    case 0x4295e0:
      return state([](Handle h) -> MenuState* { return new CreditsState(h); });
    case 0x488910: {  // the Pilot State: its screen made with it (adopted)
      const Handle h = alloc_block(0x14);
      menu_states_[h] = new PilotState(*this, h);
      return h;
    }
    case 0x48a6c0:
      return state([](Handle h) -> MenuState* { return new GameMenuState(GameMenuState::kAction, h); });
    case 0x48aef0:
      return state([](Handle h) -> MenuState* { return new GameMenuState(GameMenuState::kCampaign, h); });
    case 0x48b120:
      return state([](Handle h) -> MenuState* { return new GameMenuState(GameMenuState::kTraining, h); });
    case 0x49c900:
      return state([](Handle h) -> MenuState* { return new GameMenuState(GameMenuState::kMultiplayer, h); });
    default:
      return Engine::construct(ctor, args);
  }
}

bool GameEngine::menu_vcall(Handle obj, uint32_t slot, std::initializer_list<Arg> args, uint32_t* out) {
  const uint32_t a0 = args.size() ? args.begin()->v : 0;
  MenuState* st = menu_state_of(obj);
  if (!st) st = popup_state_of(obj);
  if (st) {
    *out = 0;
    switch (slot) {
      case 0x00:  // the deleting destructor (the popups' states are their screen's)
        if (menu_states_.erase(obj)) delete st;
        return true;
      case 0x28: st->escape(); return true;
      default: break;
    }
    char key[48];
    std::snprintf(key, sizeof key, "menu state vt+0x%x", slot);
    unported(key, {});
    return true;
  }
  uint32_t off = 0;
  MenuScreen* s = screen_of(obj, &off);
  if (!s) return false;
  *out = 0;
  if (off == 0) {
    switch (slot) {
      case 0x00: s->update(zgame::from_bits(a0)); return true;
      case 0x04: s->set_active(static_cast<int32_t>(a0)); return true;
      case 0x08:  // the deleting destructor
        if (main_menu() == s) poke32(0x609854, 0);
        screens_.erase(obj);
        delete s;
        return true;
      case 0x10: s->apply(static_cast<int32_t>(a0)); return true;
      case 0x18: *out = s->set_busy(static_cast<uint8_t>(a0)); return true;
      case 0x1c: *out = s->busy(); return true;
      default: break;
    }
  } else if (auto* m = dynamic_cast<MainMenu*>(s)) {
    if (MainMenuButton* b = m->button_at(off)) {
      if (slot == 0x30) {
        b->on_left_up();
        return true;
      }
    }
  }
  char key[48];
  std::snprintf(key, sizeof key, "menu screen +0x%x vt+0x%x", off, slot);
  unported(key, {});
  return true;
}

void GameEngine::delete_screens() {
  // The app may be gone: the screens go below, not through the states' destructors.
  while (!menu_states_.empty()) {
    MenuState* st = menu_states_.begin()->second;
    menu_states_.erase(menu_states_.begin());
    st->screen_ = 0;
    delete st;
  }
  // A screen's own popups go with it (their states delete them).
  std::vector<MenuScreen*> top;
  for (auto& [h, s] : screens_)
    if (!s.owned && s.screen) top.push_back(s.screen);
  for (MenuScreen* s : top) {
    if (main_menu() == s) poke32(0x609854, 0);
    delete s;
  }
  screens_.clear();
}

Handle GameEngine::state_screen(Handle state) {
  if (MenuState* st = menu_state_of(state)) return st->screen_;
  if (MenuState* st = popup_state_of(state)) return st->screen_;
  return Engine::state_screen(state);
}

void GameEngine::credits_restart(Handle screen) {
  if (auto* c = dynamic_cast<CreditsScreen*>(screen_of(screen))) c->clock = 0.0f;
  else Engine::credits_restart(screen);
}

void GameEngine::screen_refresh(Handle h) {
  if (MenuScreen* s = screen_of(h)) s->dirty_all();
  else Engine::screen_refresh(h);
}
void GameEngine::screen_hide(Handle h) {
  if (MenuScreen* s = screen_of(h)) s->restore_behind();
  else Engine::screen_hide(h);
}
void GameEngine::twod_reset(Handle h) {
  if (MenuScreen* s = screen_of(h)) s->unload();
  else Engine::twod_reset(h);
}
void GameEngine::twod_set_cursor(uint32_t v) { ztwod::set_page_flip(v != 0); }

// 0x0048ee00 on the state the address names.
void GameEngine::shell_clear(uint32_t state) {
  States& st = states();
  ShellState* const shells[] = {&st.main, &st.pause, &st.load, &st.win, &st.salvage};
  for (ShellState* s : shells) {
    if (s->addr() == state) {
      s->clear_screen();
      return;
    }
  }
  MenuState* m = menu_state_of(state);
  if (!m) m = popup_state_of(state);
  if (m) {
    m->clear_screen();
    return;
  }
  Engine::shell_clear(state);
}

// App::Push of a menu object: the ported ones are pushed; any other is logged.
void GameEngine::push_state(Handle state, uint32_t arg) {
  MenuState* st = menu_state_of(state);
  if (!st) st = popup_state_of(state);
  if (st) {
    app().push(st, arg);
    return;
  }
  char key[40];
  std::snprintf(key, sizeof key, "0x00590400 (menu state %#x)", state);
  unported(key, {state, arg});
}

// ---- the pilot (mech3/pilot) ----

void GameEngine::pilot_state_set_mode(Handle state, int32_t mode) {
  if (auto* p = dynamic_cast<PilotState*>(menu_state_of(state))) p->set_mode(mode);
  else Engine::pilot_state_set_mode(state, mode);
}

// [0x0060861c] as a handle of the pilot object, for the parts that read it
// through the module memory (the mission's, not yet bridged: they see a
// pilot, not its games).
void GameEngine::pilot_chosen(Pilot* p) { poke32(0x60861c, handle_of(p)); }

// 0x004785f0 at the app's stop: every pilot destroyed, so saved.
void GameEngine::pilots_delete() {
  current_pilot() = nullptr;
  delete_other_pilots();
}
void GameEngine::pilot_delete(const char* name) { delete_pilot(name); }

uint32_t GameEngine::message_box1(int32_t a, int32_t caption, int32_t text, const char* s) {
  return message_box3(a, caption, text, s, "", "");
}

}  // namespace mech3
