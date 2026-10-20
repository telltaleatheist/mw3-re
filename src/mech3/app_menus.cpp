// The ported menu screens on the program's engine (our addition, not original
// code; docs/mainmenu.md, The port). The shell states reach their screens
// through Engine handles: construct, vcall, the Layer calls. A ported screen's
// handle is a block of the module memory the size of the original object, so
// the offsets the states add (a button's +0x111ec) still lead to its parts.
#include <cstdio>

#include "gamez/ztwod/twod.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_states.h"
#include "mech3/mainmenu.h"

namespace mech3 {

MenuScreen* GameEngine::screen_of(Handle h, uint32_t* offset) const {
  if (!h || screens_.empty()) return nullptr;
  auto it = screens_.upper_bound(h);
  if (it == screens_.begin()) return nullptr;
  --it;
  const uint32_t off = h - it->first;
  if (off >= 0x12c1c) return nullptr;
  if (offset) *offset = off;
  else if (off) return nullptr;
  return it->second;
}

Handle GameEngine::construct(uint32_t ctor, std::initializer_list<Arg> args) {
  if (ctor == 0x486260) {  // the main menu, new(0x12c1c)
    const Handle h = alloc_block(0x12c1c);
    screens_[h] = new MainMenu(*this, args.size() ? args.begin()->v : 0);
    poke32(0x609854, h);  // its constructor's [0x00609854] = this
    return h;
  }
  return Engine::construct(ctor, args);
}

bool GameEngine::menu_vcall(Handle obj, uint32_t slot, std::initializer_list<Arg> args, uint32_t* out) {
  uint32_t off = 0;
  MenuScreen* s = screen_of(obj, &off);
  if (!s) return false;
  const uint32_t a0 = args.size() ? args.begin()->v : 0;
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
  for (auto& [h, s] : screens_) {
    if (main_menu() == s) poke32(0x609854, 0);
    delete s;
  }
  screens_.clear();
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
  Engine::shell_clear(state);
}

// The menus' states (Action, Campaign, Training, Multiplayer, the options and
// the credits) are not ported: their objects are 0 and a press is logged.
void GameEngine::push_state(Handle state, uint32_t arg) {
  char key[40];
  std::snprintf(key, sizeof key, "0x00590400 (menu state %#x)", state);
  unported(key, {state, arg});
}

}  // namespace mech3
