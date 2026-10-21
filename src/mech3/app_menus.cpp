// The ported menu screens and menu objects on the program's engine (our
// addition, not original code; docs/mainmenu.md, The port; docs/menus.md).
// The shell states reach their screens through Engine handles: construct,
// vcall, the Layer calls. A ported screen's handle is a block of the module
// memory the size of the original object, so the offsets the states add (a
// button's +0x111ec) still lead to its parts; a menu object's handle is a block
// of 0x10, its address for the hooks that name it.
#include <cstdio>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/ztwod/twod.h"
#include "gamez/ztwod/twod_host.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/app_states.h"
#include "mech3/briefing.h"
#include "mech3/debriefing.h"
#include "mech3/controls_menu.h"
#include "mech3/credits.h"
#include "mech3/mainmenu.h"
#include "mech3/menu_states.h"
#include "mech3/mission.h"
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
    case 0x47b430: {  // the debriefing, new(0x13bf4): the Win State, operation, mission, its +0x10
      const Handle st = a0;
      int32_t op = 0, mission = 0;
      if (args.size() >= 3) {
        op = static_cast<int32_t>(args.begin()[1].v);
        mission = static_cast<int32_t>(args.begin()[2].v);
      }
      States& ss = states();
      const uint8_t* won = st == ss.end_win.addr() ? &ss.end_win.campaign_won_ : &ss.win.campaign_won_;
      const Handle h = alloc_block(0x13bf4);
      screens_[h] = {new DebriefScreen(*this, st, op, mission, won), 0x13bf4};
      return h;
    }
    case 0x405230: {  // the briefing, new(0x12e6c): its section's name
      const char* name = args.size() && args.begin()->s ? args.begin()->s : "default";
      const Handle h = alloc_block(0x12e6c);
      auto* b = new BriefingScreen(*this, name);
      screens_[h] = {b, 0x12e6c};
      briefing_screen() = b;
      return h;
    }
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

namespace {
// The ported shell states by address, the end screen's Win State too.
ShellState* shell_state_at(Handle state) {
  States& st = states();
  ShellState* const shells[] = {&st.main, &st.pause, &st.load, &st.win, &st.end_win, &st.salvage};
  for (ShellState* s : shells)
    if (s->addr() == state) return s;
  return nullptr;
}
}  // namespace

Handle GameEngine::state_screen(Handle state) {
  if (ShellState* s = shell_state_at(state)) return s->screen_;
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
  if (ShellState* s = shell_state_at(state)) {
    s->clear_screen();
    return;
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

// 0x00478f10 on the pilot the screen chose (the Win State passes 1, the launch
// 0); with none (a debug start's stand-in), the call is logged.
void GameEngine::pilot_record_mission(Handle p, int32_t a) {
  if (!program_record_mission(a != 0)) Engine::pilot_record_mission(p, a);
}

bool program_record_mission(bool next) {
  Pilot* pl = current_pilot();
  if (pl) pl->record_mission(next);
  return pl != nullptr;
}

namespace {
// The pilot module's calls out in the program: the message box is the
// engine's; the loadouts are the ported Config's; zSave stays logged.
struct ProgramPilotHooks : PilotHooks {
  GameEngine* engine = nullptr;
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return engine ? engine->unported(key, args) : 0;
  }
  int32_t message_box(int32_t type, int32_t caption, int32_t text) override {
    return engine ? static_cast<int32_t>(engine->message_box(type, caption, text)) : 0;
  }
};
ProgramPilotHooks g_pilot_hooks;
}  // namespace

void wire_pilot_hooks(GameEngine* e) {
  g_pilot_hooks.engine = e;
  set_pilot_hooks(e ? &g_pilot_hooks : nullptr);
}

uint32_t GameEngine::message_box1(int32_t a, int32_t caption, int32_t text, const char* s) {
  return message_box3(a, caption, text, s, "", "");
}

// ---- the briefing (mech3/briefing, campaign.md section 7, item 5) ----

bool launch_player();  // 0x00491150 (mech3/mission_launch.h, whose mech headers clash with pilot.h)

namespace {
BriefingScreen* briefing_of(const GameEngine& e, Handle s) { return dynamic_cast<BriefingScreen*>(e.screen_of(s)); }

// 0x00435350 (a campaign's vt+0x54): the mission directory, c4b past Op 4 Mission 3.
int32_t campaign_dir(const CampaignGame& c) {
  switch (c.operation()) {
    case 1: return 0;
    case 2: return 1;
    case 3: return 2;
    case 4: return (3 < c.mission_number()) + 3;
    default: return -1;
  }
}
}  // namespace

// The Load state from the menus: its game is the chosen pilot's (the
// launch's stand-in filled from it; a debug start keeps its own). Only a
// campaign's and training's missions for now.
void GameEngine::load_state_enter() {
  if (launch_.active && launch_.auto_launch) return;
  Pilot* p = current_pilot();
  Game* g = p ? p->current : nullptr;
  Launch l;
  l.auto_launch = false;
  l.keep_pilot = true;
  l.type = g ? g->type() : -1;
  if (auto* c = dynamic_cast<CampaignGame*>(g)) {
    l.number = c->mission_number();
    l.op = c->operation();
    l.index = campaign_dir(*c);
  } else if (auto* t = dynamic_cast<TrainingGame*>(g)) {
    l.number = t->mission;
    l.index = 5;  // t1
  } else {
    l.index = -1;
  }
  if (l.index < 0) {
    // Not ported (Instant Action, multiplayer), or no game: back to the
    // main menu rather than a mission left from before.
    unported("the Load state of this game", {l.type});
    launch_.active = false;
    replace_state(2);
    return;
  }
  l.mech = g->slot.mech;
  l.config = g->slot.config ? handle_of(g->slot.config) : 0;  // the slot's, alive while its game is
  l.skin = g->slot.skin;
  set_launch(l);
}

void GameEngine::briefing_script_start(Handle s) {
  if (BriefingScreen* b = briefing_of(*this, s)) b->script.start();
  else Engine::briefing_script_start(s);
}

// Mech3.exe 1.2 @ 0x00409ec0 / 0x00409eb0: no cursor while built, then the screen's own.
void GameEngine::briefing_init(Handle s) {
  if (BriefingScreen* b = briefing_of(*this, s)) b->set_cursor(nullptr);
  else Engine::briefing_init(s);
}
void GameEngine::briefing_start(Handle s) {
  if (BriefingScreen* b = briefing_of(*this, s)) b->set_cursor(&b->cursor_obj);
  else Engine::briefing_start(s);
}

// 0x00405160 stood in for: the briefing's MAP image (images\maps) as the
// dialog's first image, at the origin behind the rest. The mission's map
// object it also makes (0x004582a0 into [0x006098d0]+0xbc, the waypoints'
// and the radar's) is not ported.
void GameEngine::briefing_map_image(ztwod::Dialog* d, zrdr::Node* section) {
  ztwod::Host& h = ztwod::host();
  h.image_path("..\\data\\common\\images\\maps");
  const char* name = zrdr::get_string(section, "MAP");
  zimg::Texture* t = name ? h.image_get(name) : nullptr;
  if (!d || !t) return;
  ztwod::Image& im = d->images[0];
  d->add(&im);
  im.show_for(-1.0f);
  im.set_pos(0, 0);
  im.set_image(t);
  im.flags = ztwod::kManaged | (im.flags & ztwod::kHidden);  // 0x00409ac0(2)
  im.dirty();
}

void program_briefing_progress(float f) {
  if (BriefingScreen* b = briefing_screen()) b->progress(f);
}
uint32_t program_slot_count() {
  CampaignGame* c = current_pilot() ? current_pilot()->campaign() : nullptr;
  return c ? static_cast<uint32_t>(c->slot_count()) : 0;
}
void program_briefing_line(const char* s) {
  if (BriefingScreen* b = briefing_screen()) b->log(s);
}

// 0x00479440 on the pilot the launch filled: its game's stand-in.
Handle GameEngine::pilot_campaign(Handle p) {
  if (p && p == pilot_ && launch_.type == kCampaign) return game_;
  return Engine::pilot_campaign(p);
}

// Mech3.exe 1.2 @ 0x0047ae10 (CL a, DL b): a failed mission's salvage merged
// back (0x00434cb0); a won one's lance put in the salvage store, the whole
// team with b (zEntity's list, 0x00472f00 each: not ported, item 8).
void GameEngine::mission_results(uint8_t a, uint8_t b) {
  CampaignGame* c = current_pilot() ? current_pilot()->campaign() : nullptr;
  if (!c) return;
  if (a) c->merge_salvage();
  else unported("0x0047ae10 (the lance into the salvage store)", {b});
}

bool GameEngine::missions_loaded() { return missions().finished(); }
bool GameEngine::mission_launch() { return launch_player(); }
void GameEngine::mission_cleanup(int32_t unload) { missions().cleanup(unload != 0); }
void GameEngine::replace_state(int32_t n) { app().replace(app().state(n), 0); }

Handle GameEngine::voice_play(Handle s, float v) {
  auto* o = static_cast<zsnd::Object*>(object_of(s));
  return o ? handle_of(zsnd::play(o, v)) : 0;
}
int32_t GameEngine::voice_status(Handle v) { return zsnd::status(static_cast<zsnd::VoiceBase*>(object_of(v))); }
void GameEngine::sound_cue_briefing(Handle s) {
  if (auto* o = static_cast<zsnd::Object*>(object_of(s)))
    zsnd::set_cue_fn(o, [](uint32_t marker, zsnd::Sound*) { briefing_cue(static_cast<int32_t>(marker)); });
}
uint8_t GameEngine::app_flag_1ba() { return app().byte_at(0x1ba); }
Handle GameEngine::mission_navs() { return missions().stations; }

}  // namespace mech3
