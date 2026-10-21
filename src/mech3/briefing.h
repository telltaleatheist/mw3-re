#pragma once
#include <cstdint>
#include <list>

#include "mech3/menu_widgets.h"
#include "mech3/pilot.h"

// The briefing screen (docs/campaign.md section 2): C:\zipper\Mech3\
// briefing.cpp (0x00404ae0..0x00406440) and its helpers after it
// (0x00408b40..0x00409440). The screen the Load state builds while the
// mission loads: the buttons and their enabling, the progress bar, the
// frame, ABORT and LAUNCH. The briefing's map and its script of voices and
// map steps (0x00406650, the SOUND key), the reallocation list
// (REALLOC_LIST, 0x00404ae0) and the loadouts (0x00408b40..0x00408c30) are
// later items (campaign.md section 7).
namespace mech3 {

// An entry of the briefing's script (vtable 0x005bbc60): played one after
// another, each until its step says done.
class BriefingStep {
 public:
  virtual ~BriefingStep() = default;
  virtual bool step(float dt) = 0;   // +0x00: true when done
  virtual void x04() {}              // +0x04
  virtual void reset() {}            // +0x08: REPLAY
};

// The script (+0x11220): a list, the entry playing and whether it plays.
struct BriefingScript {
  explicit BriefingScript(Engine& e);  // inline in 0x00405230: the voice stopped
  std::list<BriefingStep*> steps;    // +0x00 (+0x04 its head, +0x08 its size)
  std::list<BriefingStep*>::iterator at = steps.end();  // +0x0c
  uint8_t playing = 0;               // +0x10

  void stop(Engine& e);              // 0x004063e0 (and inline): the voice stopped, at the end
  void start() {                     // the Load state's (0x00409440)
    playing = 1;
    at = steps.begin();
  }
  void step(float dt);               // the head of 0x00406440
};

// [0x005e043c] the map step the script reached (-1 none) and [0x005f4c84]
// the voice playing.
int32_t& briefing_reached();
Handle& briefing_voice();

// The screen (vtable 0x005bbc70), 0x12e6c bytes.
class BriefingScreen : public MenuScreen {
 public:
  enum Button : int32_t { kBtnAbort = 0, kBtnLaunch = 1, kBtnMechLab = 2, kBtnReplay = 3, kBtnSalvage = 4,
                          kBtnLoading = 5, kBtnReallocOk = 7 };

  BriefingScreen(Engine& e, const char* name);  // 0x00405230: the briefing's section of briefing.zrd
  ~BriefingScreen() override;        // 0x00406170 (deleting 0x00405fd0)
  void update(float dt) override;    // +0x00  0x00406440
  void on_button(int32_t id, int32_t a, int32_t b) override;  // +0x14  0x00404c30

  void set_loaded(bool loaded);      // 0x00408ef0: the buttons for the game, once the load ends
  void abort();                      // 0x00405050
  void launch();                     // 0x00404f70
  void progress(float f) { target = f; }  // 0x004050d0 on [0x005f4c80]
  void log(const char* s);           // 0x00405120 on [0x005f4c80]: a line in the debug lines

  Store realloc_store;               // +0x111ec  what the mech lab left over (0x00408bd0)
  Store lab_store;                   // +0x11200  the mech lab's (0x00408b40)
  uint8_t salvage_seen = 0;          // +0x11214  SALVAGE pressed since the load ended
  float target = 0.0f;               // +0x11218  the load's fraction (0x004050d0)
  uint8_t loaded = 1;                // +0x1121c  the buttons' state (set_loaded)
  BriefingScript script{engine()};   // +0x11220
  ShellSlider bar;                   // +0x11234  TRANSPORT_PROGRESS (a click moves it)
  ztwod::TextBox lines{0x19};        // +0x113bc  the load's lines, shown with the app's +0x1ba
  HelpButton abort_btn, launch_btn, lab_btn, replay_btn, salvage_btn, loading;  // +0x11884, 0x17c each
  ztwod::Text version;               // +0x1216c  "<message 7> <version>", at the bottom right
  ShellButton realloc_ok;            // +0x12638  REALLOC_OK (7)
  StoreList realloc_list;            // +0x12790  REALLOC_LIST (not bound yet: a later item)
  // +0x12e60 a list the map uses: a later item.
};

// [0x005f4c80] the briefing screen, while the Load state has one.
BriefingScreen*& briefing_screen();

}  // namespace mech3
