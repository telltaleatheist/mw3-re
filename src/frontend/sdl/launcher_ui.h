// Ours: the launcher's screens as RmlUi draws them (third_party/rmlui; docs/launcher.md, How it is drawn): the
// document and its styles in assets/launcher/ui (launcher.rml, launcher.rcss), what they show in a UiModel the
// launcher fills, drawn by an SDL_Renderer. The launcher keeps its logic: the buttons by number, what is hot.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

struct SDL_Renderer;

namespace sdlfe {

// A button: `i` its number in the launcher's list.
struct UiButton {
  int i = 0;
  std::string label, sub;
  std::string kind = "plate";  // plate, amber, flat, seg (a split button's UPDATE), gear: its class
  bool on = true;
  int keep = 0;  // the sub's start kept whole when it is cut in the middle to fit
  bool operator==(const UiButton& o) const { return std::tie(i, label, sub, kind, on, keep) == std::tie(o.i, o.label, o.sub, o.kind, o.on, o.keep); }
};
struct UiLine {
  std::string text, kind;  // kind: its class (amber, dim, soft, ...)
  int keep = 0;
  bool operator==(const UiLine& o) const { return std::tie(text, kind, keep) == std::tie(o.text, o.kind, o.keep); }
};
struct UiPanel {
  std::string badge, title, buttons;  // the badge's picture ("" none), the group its buttons are
  std::string section;                // a heading above it ("": none)
  bool on = true;                     // false: not out yet
  std::vector<UiLine> lines;
  bool operator==(const UiPanel& o) const { return std::tie(badge, title, buttons, section, on, lines) == std::tie(o.badge, o.title, o.buttons, o.section, o.on, o.lines); }
};
// What the document shows. Every string is a data variable of its name (launcher.rml).
struct UiModel {
  std::string screen;                     // main, find, confirm, work, notes, data, update, copies, game
  std::string head;                       // the header: main, launcher, game (a game's own start)
  std::string heading, heading_kind, game, badge;  // its name (kind: own, own long, with), the game under it, the game's badge
  std::string title, words, text, path;   // a screen's words, as its part of the document places them
  std::string status, status_kind;        // the status line; good, bad or use (the files in use)
  std::string package, stage, message, percent, picker, variant;
  float fraction = 0;                     // the work's bar, 0 to 1
  int notes_size = 14, notes_width = 470, notes_gap = 14, notes_line = 20;  // BEFORE YOU PLAY's measure
  std::vector<UiLine> lines;
  std::vector<std::string> notes;         // paragraphs
  std::vector<UiPanel> panels;
  std::string empty;  // said in the list's place when there is no panel ("": nothing)
  std::vector<UiPanel> rows;              // a game's page: its game files, its copies
  std::map<std::string, std::vector<UiButton>> buttons;  // by group: <x buttons="group"> in the document
  bool operator==(const UiModel& o) const { return std::tie(screen, head, heading, heading_kind, game, badge, title, words, text, path, status, status_kind, package, stage, message, percent, picker, variant, fraction, notes_size, notes_width, notes_gap, notes_line, lines, notes, panels, rows, buttons) == std::tie(o.screen, o.head, o.heading, o.heading_kind, o.game, o.badge, o.title, o.words, o.text, o.path, o.status, o.status_kind, o.package, o.stage, o.message, o.percent, o.picker, o.variant, o.fraction, o.notes_size, o.notes_width, o.notes_gap, o.notes_line, o.lines, o.notes, o.panels, o.rows, o.buttons); }
};

class LauncherUi {
 public:
  LauncherUi();
  ~LauncherUi();
  // The fonts, for measuring too (before there is a window); `dir` assets/launcher.
  bool init(const std::string& dir, const std::string& font, const std::string& body_font);
  void art_from(const std::string& dir);  // a second folder for pictures not in `dir` (a fetched catalog's cache)
  bool attach(SDL_Renderer* ren);
  void detach();  // before the renderer goes
  bool ready() const;

  // In the layout's units (dp: pixels on the 854x480 canvas).
  int width(const std::string& s, int size, bool body = false);
  int line_skip(int size, bool body = false);

  void show(const UiModel& m, int hot);  // hot: the button lit, -1 none
  bool update();                         // the renderer's size followed, the layout done; true: show() again (a new size)
  void render();

  void mouse_move(float x, float y);  // in the renderer's pixels
  void mouse_button(bool down);
  void mouse_wheel(float notches);
  void mouse_leave();
  int over() const;              // the button under the mouse, -1 none
  void into_view(int button);    // its panel scrolled into the list
  void scroll_list(float y);     // the panels' list, in dp (checks)
  bool reload();                 // the document and its styles read again

 private:
  struct Impl;
  std::unique_ptr<Impl> p_;
};

}  // namespace sdlfe
