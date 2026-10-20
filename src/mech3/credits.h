#pragma once
#include <cstddef>
#include <cstdint>
#include <new>

#include "mech3/menu_widgets.h"

// The credits screen (docs/menus.md, Credits): dlgCredits.zrd's CREDITSDIALOG
// over the main menu's background, its CREDITS_SCREEN scrolling the names of
// SCROLLING_TEXT (docs/credits.md) up through a rectangle. Original file
// unknown: 0x004289a0..0x00429eb0, between mech3\mech_anim.cpp and
// mech3\mech_data.cpp, before the option screens.
namespace mech3 {

// The exe's std::vector<T> by value (HP STL): its copies and its growth kept,
// since every element copy copies a GDI font (0x005a0200).
template <class T>
class HpVector {
 public:
  HpVector() = default;
  // 0x00429df0 for the sections: a block of exactly o's size.
  HpVector(const HpVector& o) {
    const size_t n = o.size();
    first_ = static_cast<T*>(::operator new(n * sizeof(T)));
    T* d = first_;
    for (const T* p = o.first_; p != o.last_; ++p, ++d) new (d) T(*p);
    last_ = end_ = d;
  }
  HpVector& operator=(const HpVector&) = delete;
  ~HpVector() {  // 0x00429690, 0x004295a0: front to back
    for (T* p = first_; p != last_; ++p) p->~T();
    ::operator delete(first_);
  }
  size_t size() const { return static_cast<size_t>(last_ - first_); }
  T* begin() { return first_; }
  T* end() { return last_; }
  // insert(end(), 1, x) (0x00429700, 0x00429a70): no room, a block of size +
  // max(size, 1), the old ones copied over, x copied, the old ones destroyed.
  void push_back(const T& x) {
    if (last_ != end_) {
      new (last_) T(x);
      ++last_;
      return;
    }
    const size_t n = size(), len = n + (n > 1 ? n : 1);
    T* block = static_cast<T*>(::operator new(len * sizeof(T)));
    T* d = block;
    for (T* p = first_; p != last_; ++p, ++d) new (d) T(*p);
    new (d) T(x);
    ++d;
    for (T* p = first_; p != last_; ++p) p->~T();
    ::operator delete(first_);
    first_ = block;
    last_ = d;
    end_ = block + len;
  }
  // erase(begin(), end()) (0x00429d20 then 0x004296d0): the block is kept.
  void clear() {
    for (T* p = first_; p != last_; ++p) p->~T();
    last_ = first_;
  }

 private:
  T* first_ = nullptr;               // +0x04 (+0x00 the allocator's byte)
  T* last_ = nullptr;                // +0x08
  T* end_ = nullptr;                 // +0x0c
};

// A line of the credits, 0x4b8 bytes: its text and its offset (0x00429dc0
// copies it).
struct CreditLine {
  ztwod::FontText text{nullptr, 0, 0};  // +0x000
  int32_t dx = 0, dy = 0;            // +0x4b0  "offset"; dy grows by the sections above
};

// A SCROLLING_TEXT entry: its lines, side by side (0x10 bytes, a vector).
struct CreditSection {
  HpVector<CreditLine> lines;
};

// CREDITS_SCREEN (vtable 0x005bd758), 0x178 bytes: a control whose lines
// scroll from the bottom of RECT to above its top, shown only inside it.
class CreditsScroll : public ztwod::Control {
 public:
  CreditsScroll() = default;         // in 0x004289a0
  ~CreditsScroll() override = default;  // 0x00428da0 (deleting 0x00429010)
  void update(float dt) override;    // +0x24  0x004290c0: and every line's
  bool read_control(zrdr::Node* node, ztwod::Dialog* dialog) override;  // +0x84  0x00429200
  void scroll(float t);              // 0x00429120: t from 0 (all below) to 1 (all above)

  HpVector<CreditSection> sections;  // +0x150 (+0x14c the allocator's byte)
  int32_t left = 0, top = 0;         // +0x15c  RECT, from the control's place
  int32_t right = 0, bottom = 0;     // +0x164
  int32_t total = 0;                 // +0x16c  the sections' heights added up
  float rate = 0.0f;                 // +0x170  SCROLL_RATE: seconds a section
};

// The credits screen (vtable 0x005bd738), 0x11ae0 bytes.
class CreditsScreen : public MenuScreen {
 public:
  CreditsScreen(Engine& e, Handle state);  // 0x004289a0
  ~CreditsScreen() override = default;  // 0x00428e60 (deleting 0x00428c70)
  void update(float dt) override;    // +0x00  0x00429030: the scroll by the clock, then the window

  // CANCEL (vtable 0x005bd7e8): its +0x30 (0x00428c60) only calls the
  // state's +0x28, so no ACTIVATE sound or image (the option screens' BACK too).
  class BackButton : public SlideButton {
   public:
    explicit BackButton(Engine& e) : SlideButton(e, -1) {}
    void on_left_up() override;      // 0x00428c60
  };

  BackButton cancel;                 // +0x111ec  CANCEL
  CreditsScroll names;               // +0x113d8  CREDITS_SCREEN
  ztwod::Text title;                 // +0x11550  CREDIT_TEXT
  ztwod::Image lines{0};             // +0x11a1c  CREDIT_LINES
  float unused_11ad8 = 4.0f;         // +0x11ad8  read by nothing
  float clock = 0.0f;                // +0x11adc  seconds into the scroll
};

}  // namespace mech3
