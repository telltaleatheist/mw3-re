#pragma once
#include <cstdint>
#include <vector>

#include "mech3/hud.h"
#include "mech3/menu_widgets.h"

// The mech lab's item lists' base (docs/mechlab.md, The item lists): rows of
// items (a name, a count and the item behind it) under UP and DOWN arrows,
// with the pilot screen's FileList's ways (the rollover sound and colour, the
// selected row's font over a BAR) and its own row object with a panel and
// three more texts. UNASSIGNED_LIST and ASSIGNED_LIST (steps 5b and 6) are
// made from it. Original file unknown: 0x00451410..0x00453540 and the arrows'
// 0x004567c0..0x00456900, in the lab's range (lab_screen.h).
namespace mech3 {

class LabItemList;

// An item (0x10 bytes, new'd by 0x00453210 and freed by 0x00453420).
struct LabItem {
  char* text = nullptr;  // +0x00  _strdup'd; null: an empty row
  Handle object = 0;     // +0x04  the store item, or 0
  int32_t count = 0;     // +0x08  shown "%02d"
  Handle font = 0;       // +0x0c  set by the lists that colour their rows
};

// A row (0x00451410, vtable 0x005c1ac0), 0x1490 bytes: the item's name, with
// a dark panel behind it and its count, 0xac to the right.
class LabItemRow : public ztwod::Text {
 public:
  LabItemRow(LabItemList* list, Handle font);  // 0x00451410
  void set_pos(int32_t px, int32_t py) override;  // +0x0c  0x004519a0
  void update(float dt) override;    // +0x24  0x004515e0: only for an item with a name
  ztwod::Rect* get_rect() override { return &fixed_rect; }  // +0x2c  0x0048e540
  void on_left_up() override;        // +0x30  0x00456820: the list's +0xac with its index
  void on_enter() override {}        // +0x3c  0x0049bfd0
  void on_leave() override {}        // +0x40  0x0049bfd0
  int32_t hit(int32_t px, int32_t py) override;  // +0x5c  0x00451aa0: its text, 0xac wide
  void fill(int32_t i, const LabItem* item);  // 0x00451a50 with 0x004516a0
  int32_t panel_width() const;       // 0x00451b20

  LabItemList* list;                 // +0x4cc
  int32_t index = 0;                 // +0x4d0  (not set by the constructor)
  HudPanel panel;                    // +0x4d4  0xac by the height, (25, 25, 25)
  ztwod::Text count;                 // +0x5dc  "%02d", right-aligned, 0xac right
  ztwod::Text mass;                  // +0xaa8  0x93 right (the subclasses')
  ztwod::Text qty;                   // +0xf74  0x7a right: the store's count (the subclasses')
  ztwod::Line2D line{0, 0, 0, 0, 0}; // +0x1440 green, under the text (the subclasses')
  int32_t height = 0;                // +0x148c  the panel's, 13
};

// UP and DOWN (vtables 0x005c1cc0, 0x005c1c30), 0x15c bytes: a press scrolls
// the list a row (its +0x9c).
class LabListArrow : public ShellButton {
 public:
  LabListArrow(Engine& e, LabItemList* l, int32_t step) : ShellButton(e, -1), list(l), by(step) {}
  void on_left_up() override;        // +0x30  0x004567c0 (UP), 0x004567e0 (DOWN)
  void after_bind() override;        // +0x88  0x00456800: only the hidden flag kept
  LabItemList* list;                 // +0x158
  int32_t by;
};

// The list (0x00451b40, vtable 0x005c1b70), 0x22c bytes.
class LabItemList : public ShellButton {
 public:
  explicit LabItemList(Engine& e, int32_t id = -1);  // 0x00451b40
  ~LabItemList() override;           // 0x00451ce0
  void set_pos(int32_t px, int32_t py) override;  // +0x0c  0x00452b90
  void update(float dt) override;    // +0x24  0x004523f0
  ztwod::Rect* get_rect() override { return nullptr; }  // +0x2c  0x004d72a0
  void on_left_up() override;        // +0x30  0x00452f40
  void on_enter() override;          // +0x3c  0x00452d50
  void on_leave() override;          // +0x40  0x00452d70
  void on_drag(int32_t px, int32_t py) override;  // +0x4c  0x00452cb0: the row under the mouse lit
  int32_t hit(int32_t px, int32_t py) override;   // +0x5c  0x00452af0
  void show_for(float seconds) override;  // +0x60  0x00452360
  void hide() override;              // +0x64  0x004523a0
  bool read_control(zrdr::Node* node, ztwod::Dialog* dialog) override;  // +0x84  0x00451e60
  void after_bind() override;        // +0x88  0x004522d0
  virtual int32_t add_text(const char* text, int32_t n);  // +0x90  0x004531f0
  virtual int32_t add_object(Handle item, int32_t n);     // +0x94  0x00453160: -1, its slots
  virtual int32_t add_part(int32_t part);                 // +0x98  0x00453140: a part's name
  virtual void scroll(int32_t by);   // +0x9c  0x00452a90: then refilled
  virtual bool can_light(int32_t) { return true; }        // +0xa4  0x0040bad0
  virtual bool can_xa8(int32_t) { return true; }          // +0xa8  0x0040bad0 (asked by nothing here)
  virtual void select(int32_t index);  // +0xac  0x00452a40
  virtual bool can_roll(int32_t) { return true; }         // +0xb0  0x0040bad0: the rollover image
  virtual void layout();             // +0xb4  0x00452780
  virtual void refill();             // +0xb8  0x00453540: the store's items the filter takes

  int32_t add(const char* text, int32_t n, Handle object);  // 0x00453210: the old count
  void clear();                      // 0x00453420: then laid out
  void place_at(ztwod::Image* o);    // 0x00452bc0
  void set_rows(const int32_t size[2]);  // 0x00453020: SIZE's height in rows
  LabItemRow* row_at(int32_t px, int32_t py);  // 0x00452ea0
  LabItemRow* row_under_mouse();     // 0x00452da0
  void stop_sound(int32_t i);        // 0x00452c50
  void play_sound(int32_t i, float volume);  // 0x00452c80
  int32_t item_count() const { return static_cast<int32_t>(items.size()); }
  Handle object_at(int32_t i) const;  // 0x004534b0: 0 out of range

  int32_t top = 0;                   // +0x158  the first item shown
  int32_t end = 0;                   // +0x15c  one past the last shown
  int32_t selected = -1;             // +0x160
  std::vector<LabItem*> items;       // +0x164 (allocator byte), +0x168
  std::vector<LabItemRow*> rows;     // +0x174 (allocator byte), +0x178
  LabItemRow* selected_row = nullptr;  // +0x184  SELECTED_FONT (shown by nothing here)
  int32_t row_height = 12;           // +0x188  HEIGHT
  int32_t size[2] = {};              // +0x18c  SIZE: the rows' width and height
  ztwod::Rect rect{};                // +0x194  where the rows are (hit)
  int32_t spacing[2] = {0, 12};      // +0x1a4  SPACING: x for every row, y + 13 a row
  int32_t offset[2] = {};            // +0x1ac  OFFSET
  Handle font_normal = 0;            // +0x1b4  NORMAL_FONT
  Handle font_selected = 0;          // +0x1b8  SELECTED_FONT, else NORMAL_FONT
  Handle font_rollover = 0;          // +0x1bc  FONT (with ITEMROLLOVER)
  Handle font_large = 0;             // +0x1c0  LARGE_FONT
  Handle font_static = 0;            // +0x1c4  STATIC_FONT
  Handle font_count = 0;             // +0x1c8  COUNT_FONT
  Handle font_mass = 0;              // +0x1cc  MASS_FONT
  Handle font_grey = 0;              // +0x1d0  GREY_FONT
  int32_t bar_dx = 0, bar_dy = 0;    // +0x1d4  BAR's OFFSET from the selected row
  int32_t roll_dx = 0, roll_dy = 0;  // +0x1dc  ITEMROLLOVER's
  ztwod::Image* roll_image = nullptr;  // +0x1e4
  ztwod::Image* bar_image = nullptr;   // +0x1e8
  LabListArrow* up = nullptr;        // +0x1ec
  LabListArrow* down = nullptr;      // +0x1f0
  uint8_t inside = 0;                // +0x1f4
  LabItemRow* lit = nullptr;         // +0x1f8  the row under the mouse
  int32_t sounds[2] = {};            // +0x1fc  ROLLOVER_SOUND, SELECT_SOUND
  int32_t playing[2] = {};           // +0x204
  int32_t arrow_lit = 0;             // +0x20c  1 UP, 2 DOWN
  float repeat_timer = 0.5f;         // +0x210
  float repeat_delay = 0.5f;         // +0x214
  float repeat_rate = 0.15f;         // +0x218
  uint8_t repeating = 0;             // +0x21c
  Handle store = 0;                  // +0x220  what refill lists
  Handle x224 = 0;                   // +0x224
  bool (*filter)(Engine& e, Handle item) = nullptr;  // +0x228  (thiscall on the item) none: all
};

// 0x0048c0b0 on any flashing text (FileRow::flash's body).
void flash_text(ztwod::Text& t, bool on, Handle font);

}  // namespace mech3
