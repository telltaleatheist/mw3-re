#include "mech3/briefing.h"

#include <cstring>

#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zutil/zfix.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
// DIK codes the frame reads (0x00406440).
constexpr int32_t kKeyL = 0x26, kKeyM = 0x32, kKeySpace = 0x39, kKeyS = 0x1f;
}  // namespace

int32_t& briefing_reached() {
  static int32_t v = -1;  // [0x005e043c]
  return v;
}

Handle& briefing_voice() {
  static Handle v = 0;  // [0x005f4c84]
  return v;
}

BriefingScreen*& briefing_screen() {
  static BriefingScreen* s = nullptr;  // [0x005f4c80]
  return s;
}

// ---- the current game, as the screen reads it (app_hooks.h) ----

int32_t Engine::game_kind() {
  Pilot* p = current_pilot();
  return p && p->current ? p->current->type() : -1;
}

bool Engine::game_campaign(int32_t* operation, int32_t* mission) {
  CampaignGame* c = current_pilot() ? current_pilot()->campaign() : nullptr;
  if (!c) return false;
  *operation = c->operation();
  *mission = c->mission_number();
  return true;
}

bool Engine::game_salvage_left() {
  CampaignGame* c = current_pilot() ? current_pilot()->campaign() : nullptr;
  return c && c->store3c.count() != 0;
}

int32_t Engine::game_multiplayer_kind() {
  MultiplayerGame* g = current_pilot() ? current_pilot()->multiplayer() : nullptr;
  return g ? g->x94 : 0;
}

void Engine::campaign_merge_salvage() {
  if (CampaignGame* c = current_pilot() ? current_pilot()->campaign() : nullptr) c->merge_salvage();
}

// ---- the script ----

// Mech3.exe 1.2 @ 0x00405ed0
void briefing_cue(int32_t marker) { briefing_reached() = marker; }

BriefingVoice::BriefingVoice(Engine& en, const char* sound) : e(en) {
  std::strncpy(name, sound, sizeof name);
}

// Mech3.exe 1.2 @ 0x00405e10
bool BriefingVoice::step(float) {
  const Handle sound = e.sound_find(name);
  if (briefing_voice()) {
    e.voice_stop(briefing_voice());
    briefing_voice() = 0;
  }
  if (!cues) {
    if (sound) briefing_voice() = e.voice_play(sound, volume);
  } else if (!sound) {
    briefing_reached() = 999;
  } else {
    e.sound_cue_briefing(sound);
    briefing_voice() = e.voice_play(sound, volume);
  }
  return true;
}

// Mech3.exe 1.2 @ 0x00405db0
bool BriefingMarker::step(float) {
  if (marker == -1) return e.voice_status(briefing_voice()) == 0;
  return briefing_reached() >= marker;
}

// Mech3.exe 1.2 @ 0x00405d50
bool BriefingShow::step(float) {
  obj->show_for(-1.0f);
  obj->dirty();
  return true;
}

// Mech3.exe 1.2 @ 0x00405d90
void BriefingShow::reset() { obj->hide(); }

BriefingScript::BriefingScript(Engine& e) {
  briefing_reached() = -1;
  if (briefing_voice()) e.voice_stop(briefing_voice());
  briefing_voice() = 0;
}

// Mech3.exe 1.2 @ 0x004063e0
void BriefingScript::stop(Engine& e) {
  briefing_reached() = -1;
  if (briefing_voice()) e.voice_stop(briefing_voice());
  briefing_voice() = 0;
  playing = 0;
  at = steps.end();
}

// The head of 0x00406440: the entry playing steps; done, the next one plays
// from the next frame.
void BriefingScript::step(float dt) {
  if (!playing || at == steps.end()) return;
  if ((*at)->step(dt)) ++at;
}

// ---- the screen ----

// Mech3.exe 1.2 @ 0x00405230. The dialog is the briefing's own section of
// briefing.zrd (multiplayer's from mpinfo.zrd), else "default"; the buttons
// are BRIEFINGDIALOG's. Built, it is drawn once at once (the load starts next).
BriefingScreen::BriefingScreen(Engine& e, const char* name)
    : MenuScreen(e, 0),
      abort_btn(e, -1),
      launch_btn(e, -1),
      lab_btn(e, -1),
      replay_btn(e, -1),
      salvage_btn(e, -1),
      loading(e, -1),
      realloc_ok(e, 7),
      realloc_list(e, 0, 5) {
  lines.set_pos(300, 12);
  lines.set_font(ztwod::host().font_find("debug_font"));
  const int32_t fast = e.fast_menu();
  const bool mp_info = e.game_multiplayer_kind() > 2;
  tree = zrdr::load("briefing.zrd");
  zrdr::Node* mp_tree = nullptr;
  zrdr::Node* section;
  if (!mp_info) {
    section = zrdr::find(tree, name);
  } else {
    mp_tree = zrdr::load("mpinfo.zrd");
    section = zrdr::find(mp_tree, "briefing");
  }
  if (!section) section = zrdr::find(tree, "default");
  e.briefing_map_image(section);
  load_node(tree, section, true);
  add(&lines);
  zrdr::Node* dlg = zrdr::find(tree, "BRIEFINGDIALOG");
  if (dlg) {
    struct {
      HelpButton* b;
      int32_t id;
      const char* name;
    } const bind[] = {{&abort_btn, kBtnAbort, "ABORT"},       {&replay_btn, kBtnReplay, "REPLAY"},
                      {&lab_btn, kBtnMechLab, "MECHLAB"},     {&salvage_btn, kBtnSalvage, "SALVAGE"},
                      {&launch_btn, kBtnLaunch, "LAUNCH"},    {&loading, kBtnLoading, "LOADING"}};
    for (const auto& x : bind) {
      x.b->id = x.id;
      ztwod::bind_button(this, dlg, x.b, x.name);
    }
  }
  for (HelpButton* b : {&abort_btn, &replay_btn, &lab_btn, &salvage_btn, &launch_btn, &loading}) b->enable(0);
  version.set_font(ztwod::host().font_find("debug_font"));
  version.set_text("%s %s", e.message(7), e.version_number());
  if (version.needs_render) version.render();  // 0x00409c60: the width
  const float w = static_cast<float>(version.width + 5);
  const float h = static_cast<float>(version.text_height() + 3);
  version.set_pos(static_cast<int32_t>(640.0f - w), static_cast<int32_t>(480.0f - h));
  add(&version);
  if (section) {
    bind_button(&bar, "TRANSPORT_PROGRESS");
    read_script(section);
  }
  if (e.app_flag_1ba()) lines.show_for(-1.0f);
  else lines.hide();
  if (dlg) {
    // REALLOC_LIST (the mech lab's store list, 0x0048d880): a later item.
    ztwod::bind_button(this, dlg, &realloc_ok, "REALLOC_OK");
    e.briefing_realloc(0);
  }
  dirty_all();
  set_flags_all(0);
  e.set_fast_menu(fast);
  set_active(1);
  if (mp_tree) zrdr::free_node(mp_tree);
  unload();
  loaded = 1;
  set_loaded(false);
  e.sound_update(1, 0);
  e.video_lock();
  update(e.frame_dt());
  e.video_unlock();
  e.video_flip(0, 0, 1, 1);
}

// Mech3.exe 1.2 @ 0x00406170: the script stopped and its entries deleted.
BriefingScreen::~BriefingScreen() {
  Engine& e = engine();
  script.stop(e);
  for (BriefingStep*& s : script.steps) {
    delete s;
    s = nullptr;
  }
  script.stop(e);  // the list's own destructor (0x004063e0 again)
  script.steps.clear();
  if (briefing_screen() == this) briefing_screen() = nullptr;
}

// Mech3.exe 1.2 @ 0x00406440. FIX (B): the bar crept up 0.005 a frame; with
// zfix's per_second, 0.15 a second (0.005 at 30 fps).
void BriefingScreen::update(float dt) {
  Engine& e = engine();
  script.step(dt);
  set_loaded(e.missions_loaded());
  float v = bar.value;
  if (v < target) v += 0.005f * zfix::frame_steps(dt);
  if (1.0f < v) v = 1.0f;
  else if (v < 0.0f) v = 0.0f;
  if (loaded) v = 1.0f;
  bar.set_value(v);
  Window::update(dt);
  if (loaded) {
    if (e.key_state(kKeyL) & 3) launch_btn.on_left_up();
    if (e.key_state(kKeyM) & 3) lab_btn.on_left_up();
  }
  if ((e.key_state(kKeySpace) & 3) || (e.key_state(kKeyS) & 3)) {
    e.input_clear();
    script.stop(e);
  }
}

// Mech3.exe 1.2 @ 0x00404c30
void BriefingScreen::on_button(int32_t id, int32_t a, int32_t b) {
  (void)a, (void)b;
  Engine& e = engine();
  switch (id) {
    case kBtnAbort:
      script.stop(e);
      abort();
      break;
    case kBtnLaunch:
      launch();
      break;
    case kBtnMechLab:
      script.stop(e);
      lab_btn.on_leave();
      e.mech_lab_open(e.briefing_lab_store(), 0);
      break;
    case kBtnReplay:  // every entry back to its start, then played from the first
      script.stop(e);
      for (BriefingStep* s : script.steps) s->reset();
      script.step(0.0f);  // (stopped: nothing)
      script.start();
      break;
    case kBtnSalvage:
      script.stop(e);
      salvage_btn.on_leave();
      e.salvage_open();
      salvage_seen = 1;
      break;
    case kBtnReallocOk:
      e.briefing_realloc(0);
      break;
    default: break;
  }
}

// Mech3.exe 1.2 @ 0x00408ef0. LAUNCH follows the load; the others by the
// game: a campaign's MECHLAB and SALVAGE not at Op 1 Mission 1; Instant
// Action has no SALVAGE or REPLAY, training no MECHLAB either, multiplayer
// only LAUNCH, and launches by itself.
void BriefingScreen::set_loaded(bool on) {
  if ((loaded != 0) == on) return;
  Engine& e = engine();
  bool lab = on, abort_on = on, replay = on, salvage = false, auto_launch = false;
  switch (e.game_kind()) {
    case kMechLab:
      salvage = on;
      break;
    case kCampaign: {
      int32_t op = 0, mission = 0;
      e.game_campaign(&op, &mission);
      if (op == 1 && mission == 1) lab = false;
      salvage = lab;
      if (on) salvage_seen = 0;
      break;
    }
    case kInstantAction:
      salvage_btn.hide();
      replay_btn.hide();
      replay = false;
      break;
    case kMultiplayer:
      lab_btn.hide();
      salvage_btn.hide();
      replay_btn.hide();
      abort_btn.hide();
      lab = abort_on = replay = false;
      auto_launch = on;
      break;
    case kTraining:
      salvage_btn.hide();
      lab_btn.hide();
      replay_btn.hide();
      lab = replay = false;
      break;
    default: break;
  }
  if (on) cursor_obj.show_for(-1.0f);  // 0x00409a90
  else cursor_obj.hide();
  abort_btn.enable(abort_on);
  launch_btn.enable(on);
  lab_btn.enable(lab);
  salvage_btn.enable(salvage);
  replay_btn.enable(replay);
  loaded = on ? 1 : 0;
  if (loaded) loading.hide();
  if (loaded) e.briefing_lance_loadouts();
  if (auto_launch) launch();
}

// Mech3.exe 1.2 @ 0x00405050: once loaded, the pilot saved, a campaign's
// salvage merged, the mission unloaded, back to the main menu.
void BriefingScreen::abort() {
  if (!loaded) return;
  Engine& e = engine();
  e.pilot_record_mission(e.pilot(), 0);
  if (e.game_kind() == kCampaign) e.campaign_merge_salvage();
  e.mission_cleanup(1);
  e.replace_state(2);
}

// Mech3.exe 1.2 @ 0x00404f70. A campaign with salvage not looked at asks
// first (messages 0x1e0, 0x1e1); a failed launch goes back to the main menu.
void BriefingScreen::launch() {
  Engine& e = engine();
  const int32_t kind = e.game_kind();
  if (kind == kCampaign) {
    if (!salvage_seen && e.game_salvage_left()) {
      cursor_obj.erase();
      if (e.message_box(1, 0x1e0, 0x1e1) != 1) return;
    }
  } else if (kind == kMultiplayer) {
    e.multiplayer_launch_prep();
  }
  if (!e.mission_launch()) {
    e.mission_cleanup(1);
    e.replace_state(2);
  } else {
    e.replace_state(5);
  }
}

// Inline in 0x00405230: SOUND ("voice" ... MARKERS ((name (..) ...) ...)):
// the voice, then for each marker its step and the objects shown at it
// (0x00406650 reads each), then the end of the voice.
void BriefingScreen::read_script(zrdr::Node* section) {
  zrdr::Node* sound = zrdr::find(section, "SOUND");
  if (!sound) return;
  Engine& e = engine();
  script.steps.push_back(new BriefingVoice(e, sound->list[1].s));
  zrdr::Node* markers = zrdr::find(sound, "MARKERS");
  // FIX (bug): no MARKERS crashed (read unchecked); here, no map steps.
  const int32_t n = markers ? markers->list[0].i - 1 : 0;
  for (int32_t i = 0; i < n; ++i) {
    script.steps.push_back(new BriefingMarker(e, i));
    zrdr::Node* objs = markers->list[i + 1].list;
    const int32_t m = (objs[0].i - 1) / 2;
    for (int32_t j = 0; j < m; ++j) {
      ztwod::Object2D* o = e.briefing_map_object(&objs[1 + 2 * j]);
      if (!o) continue;
      o->set_background(static_bg, nullptr);
      script.steps.push_back(new BriefingShow(o));
      add(o);
    }
  }
  script.steps.push_back(new BriefingMarker(e, -1));
}

// Mech3.exe 1.2 @ 0x00405120. FIX (bug): the line went in as the format
// (a '%' in it read arguments that are not there).
void BriefingScreen::log(const char* s) { lines.set_text("%s", s); }

}  // namespace mech3
