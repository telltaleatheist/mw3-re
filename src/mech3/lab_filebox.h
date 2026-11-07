// The lab's loadout file box on Pirate's Moon: Ctrl+X and Ctrl+I, in the
// game's own dialog (MW3 opens the Windows file dialogs there). Original file
// name unknown; it sits between the mech lab and the pilot screens
// (docs/menus.md, The loadout file box).
#pragma once

#include <string>

#include "mech3/menu_widgets.h"

namespace mech3 {

// LOADSAVE_DIALOG (0x00490270, vtable 0x005e7bb4), 0x12664 bytes.
class LabFileBox : public MenuScreen {
 public:
  // `name` is the default on saving and the chosen file's name after run().
  LabFileBox(Engine& e, const char* dir, const char* filter, std::string* name, bool load);
  void update(float dt) override { Window::update(dt); }  // +0x00
  bool run();  // 0x004907c0: its own frames until a button; true for Accept

  void answer(bool accept);  // 0x00490710, 0x00490750

  class Button : public SlideButton {
   public:
    Button(Engine& e, bool accept) : SlideButton(e, -1), accept_(accept) {}
    void on_left_up() override;  // the +0x18 call of 0x00487d80
   private:
    bool accept_;
  };
  // EDITBAR (0x0048fe00, vtable 0x005e7b1c), Enter as Accept and Escape as Cancel.
  class NameBar : public ShellEditBar {
   public:
    explicit NameBar(Engine& e);
    void on_return() override;  // +0x90  0x0048feb0
    void on_cancel() override;  // +0x94  0x0048fed0
  };
  // FILELIST (vtable 0x005e7c88): a choice shows in the bar.
  class Files : public FileList {
   public:
    explicit Files(Engine& e) : FileList(e, -1) {}
    void select(int32_t index) override;  // +0xa0  0x00490760
  };

 private:
  void prepare();  // 0x00490480
  void fill();     // 0x00490570
  void finish();   // 0x004904f0

  Button accept_;            // BTN_ACCEPT
  Button cancel_;            // BTN_CANCEL
  NameBar bar_;              // EDITBAR
  Files files_;              // FILELIST
  ztwod::Image box_{0};      // LOAD_BOX
  ztwod::Image backdrop_{0}; // the screen behind (the list's first object)
  zimg::Texture* grab_ = nullptr;  // +0x12640
  bool result_ = false;      // +0x12644
  bool done_ = false;        // +0x12645
  bool load_;                // +0x1264c
  std::string* name_;        // +0x12654
  std::string pattern_;      // the listing: "%s\%s" of the folder and the filter
};

}  // namespace mech3
