#include "mech3/mainmenu.h"

#include "gamez/zimage/zimg_library.h"
#include "gamez/zreader/zreader.h"
#include "mech3/app.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
MainMenu* g_main_menu = nullptr;  // 0x00609854

const char* const kButtonNames[7] = {"INSTANTACTION", "NEWCAMPAIGN", "MULTIPLAYER", "TRAINING",
                                     "OPTIONS",       "CREDITS",     "QUIT"};
constexpr uint32_t kButtons = 0x111ec, kButtonSize = 0x2a8;
}  // namespace

MainMenu* main_menu() { return g_main_menu; }

// The globals 0x0048e980..0x0048eac0 push (QUIT pushes none).
uint32_t menu_global(MainMenuButton::Kind k) {
  switch (k) {
    case MainMenuButton::kInstantAction: return 0x60985c;  // Action State (0x0048a6c0)
    case MainMenuButton::kNewCampaign: return 0x609864;    // Campaign State (0x0048aef0)
    case MainMenuButton::kMultiplayer: return 0x609860;    // Multiplayer State (0x0049c900)
    case MainMenuButton::kTraining: return 0x609874;       // Training State (0x0048b120)
    case MainMenuButton::kOptions: return 0x609868;        // the options (0x00438be0)
    case MainMenuButton::kCredits: return 0x609878;        // the credits (0x004295e0)
    default: return 0;
  }
}

// ---- the buttons ----

MainMenuButton::MainMenuButton(Engine& e, Kind k) : SlideButton(e, -1), kind(k) { line.hide(); }

// Mech3.exe 1.2 @ 0x0048e810
void MainMenuButton::update(float dt) {
  line.update(dt);
  SlideButton::update(dt);
}

// Mech3.exe 1.2 @ 0x0048e980, 0x0048e9c0, 0x0048ea00, 0x0048ea40, 0x0048ea80,
// 0x0048eac0: pressed, then App::Push (0x00590400) of the menu's object
// whether or not the button is enabled; 0x0048eb00 (QUIT) only asks to quit.
void MainMenuButton::on_left_up() {
  if (kind == kQuit) {
    e_.confirm_quit();
    return;
  }
  if (enabled) {
    Control::on_left_up();
    notify(1, 0);
  }
  e_.push_state(e_.peek32(menu_global(kind)), 0);
}

// Mech3.exe 1.2 @ 0x0048e6b0: AUXBITMAP ("name" dx dy) is read raw, the line
// placed by the button's position and shown for good.
bool MainMenuButton::read_control(zrdr::Node* node, ztwod::Dialog* dialog) {
  reading = 1;
  const bool r = Control::read_control(node, dialog);
  reading = 0;
  read_ripple(node);
  if (zrdr::Node* aux = zrdr::find(node, "AUXBITMAP")) {
    line.set_image(aux->list[1].s);
    const int32_t py = get_y() + aux->list[3].i;
    const int32_t px = get_x() + aux->list[2].i;
    line.set_pos(px, py);
    line.show_for(-1.0f);
  }
  return r;
}

// Mech3.exe 1.2 @ 0x0048e780
void MainMenuButton::set_baked(bool on) {
  if (!on) line.show_for(-1.0f);
  else line.hide();
  SlideButton::set_baked(on);
}

// ---- the screen ----

// Mech3.exe 1.2 @ 0x00486260
MainMenu::MainMenu(Engine& e, Handle s)
    : MenuScreen(e, s),
      buttons{{e, MainMenuButton::kInstantAction}, {e, MainMenuButton::kNewCampaign},
              {e, MainMenuButton::kMultiplayer},   {e, MainMenuButton::kTraining},
              {e, MainMenuButton::kOptions},       {e, MainMenuButton::kCredits},
              {e, MainMenuButton::kQuit}},
      title(e) {
  if (!app().no_voices()) {
    e.voice_start("MENU");
    e.music_play(ztwod::host().cd_find("menu"), 2);
  }
  ztwod::host().image_path("zbd");
  {  // the render target from zVideo's buffers (0x00558290)
    const Handle a = e.video_back_buffer();
    const Handle b = e.video_buffer_da0();
    const Handle r = e.video_buffer_d90();
    e.render_set_target2(e.video_target(), r, b, a);
  }
  const bool moving = !e.fast_menu();
  if (load("dialog.zrd", "MAINMENU", moving)) {
    definition = tree;
    bind_primitive(&background, "BACKGROUND");
    for (int i = 0; i < 7; ++i) bind_button(&buttons[i], kButtonNames[i]);
    version.set_font(ztwod::host().font_find("debug_font"));
    const char* number = e.version_number();
    const char* name = e.message(7);
    version.set_text("%s %s", name, number);
    if (version.needs_render) version.render();
    // The bottom right corner of a 640x480 screen, whatever the resolution
    // (docs/mainmenu.md, Resolution).
    const float w = static_cast<float>(version.width + 5);
    const int32_t h = version.text_height() + 3;
    const int32_t py = static_cast<int32_t>(480.0f - static_cast<float>(h));
    const int32_t px = static_cast<int32_t>(640.0f - w);
    version.set_pos(px, py);
    add(&version);
    if (moving) {
      bind_primitive(&title, "MECH3TEXT");
    } else {
      bake();
      remove(&background);
      for (MainMenuButton& b : buttons) b.set_baked(true);
    }
  } else {
    definition = nullptr;
  }
  set_flags_all(moving ? 0 : ztwod::kManaged);
  set_active(1);
  g_main_menu = this;
}

// Mech3.exe 1.2 @ 0x00485780
MainMenu::~MainMenu() {
  engine().voice_release("MENU");
  engine().cd_stop();
  g_main_menu = nullptr;
}

MainMenuButton* MainMenu::button_at(uint32_t offset) {
  if (offset < kButtons || (offset - kButtons) % kButtonSize) return nullptr;
  const uint32_t i = (offset - kButtons) / kButtonSize;
  return i < 7 ? &buttons[i] : nullptr;
}

}  // namespace mech3
