#pragma once
#include <cstdint>
#include <list>
#include <vector>

#include "gamez/znetwork/znet_object.h"
#include "gamez/ztwod/twod.h"

// The net view (docs/editor.md, The net view; S5 sitting 3b): a numbered
// object's elements drawn over the 3D view as circles, the links between
// them as lines with arrowheads, each a zTwoD object in the view's own
// window so the mouse can pick them. Original file unknown: it sits at
// 0x0058b3c0..0x0058f8a0, after znet_object (0x0058aec0..0x0058b390) and
// before zgameapp, so it is named after its neighbour. Only the AI net
// editor makes one (0x00402f40, its selection base at +0x18).
namespace znet {

// What the view asks of an object's elements: the element class's virtuals
// (the AI net's nodes in the release, vtable 0x005cae60); and the keyboard.
struct ViewHooks {
  virtual ~ViewHooks() = default;
  virtual int32_t link_count(Element* e) = 0;              // vt+0x04
  virtual Element* link(Element* e, int32_t k) = 0;        // vt+0x08
  virtual int32_t link_index(Element* e, Element* to) = 0; // vt+0x0c: -1 when e has no link to `to`
  virtual bool placed(Element* e) = 0;                     // vt+0x14
  virtual const float* pos(Element* e) = 0;                // vt+0x1c
  // vt+0x28 (0x004c71d0 -> vt+0x70, 0x004d5170): the buffer when the world's
  // segment query (0x004eed90) finds the line between the two clear, else null.
  virtual bool sees(Element* e, Element* to) = 0;
  virtual uint32_t key_state(int32_t key);  // 0x0051a9b0 (zin::key_state)
};

struct View;
struct Mark;
struct Link;

// The colours (0x0058bf50 sets them, 0x00579370), each view's own.
struct ViewColors {
  uint32_t normal = 0;   // [0x007307f0] red
  uint32_t pressed = 0;  // [0x007307f4] green
  uint32_t lit = 0;      // [0x007307f8] yellow: hovered or picked
  uint32_t cyan = 0;     // [0x007307fc] set, never read
  uint32_t clear = 0;    // [0x00730800] blue: a link whose ends see each other
};
ViewColors& view_colors();
// [0x00730804]: the line clip's height in pixels (0x0058b530, each frame).
float& view_height();

// A mark's circle (+0x2c in the mark, vtable 0x005cf150): its mouse
// events go to the mark (0x0058f760..0x0058f7c0, its slots +0x00..+0x18).
class MarkObject : public ztwod::Object2D {
 public:
  MarkObject() : Object2D(0, 0) { hide(); }  // 0x0058ca30
  void draw() override {}           // +0x04 0x0049bfd0: the view draws it (0x0058b510)
  void draw_circle();               // 0x0058cf20
  int32_t hit(int32_t px, int32_t py) override;  // +0x5c 0x0058d0e0
  void on_press() override;         // +0x44 -> mark +0x00
  void on_release() override;       // +0x48 -> mark +0x04
  void on_enter() override;         // +0x3c -> mark +0x08
  void on_leave() override;         // +0x40 -> mark +0x0c
  void on_left_up() override;       // +0x30 -> mark +0x10
  void on_right_up() override;      // +0x34 -> mark +0x14
  void on_click() override;         // +0x50 -> mark +0x18
  Mark* mark = nullptr;             // +0x34
  uint32_t color = 0;               // +0x38
};

// An element's place on screen (0x74 bytes, vtable 0x005cf028; the cursor's
// 0x005cf2b8). Its point is worked out lazily, `state` saying how far:
// 1 in view space, 2 within the clip's depth (5: not), 3 projected with its
// size, 4 on screen (6: not).
struct Mark {
  enum Kind { kNode, kCursor };
  Kind kind = kNode;
  uint32_t flags = 0;          // +0x04  1 under the mouse, 2 clicked, 4 right-clicked
  View* view = nullptr;        // +0x08
  Element* elem = nullptr;     // +0x0c  null for the cursor
  float cam[3] = {};           // +0x10  in view space
  float scr[3] = {};           // +0x1c  x, y, 1 / z
  int32_t state = 0;           // +0x28
  MarkObject obj;              // +0x2c
  float size = 0;              // +0x68  1 / z * the clip's height
  float radius = 1.0f;         // +0x6c  size within 8 .. height / 8 (0x0058b3c0: [0x005f2a50])
  float ring = 0.75f;          // +0x70  [0x005f2a60]: the circle's radius over `radius`

  const float* world();        // the element's vt+0x1c; the cursor's point (0x004104d0)
  bool placed();               // vt+0x14; the cursor's 0x004d7210: yes
  void to_view();              // 0x0058b890 (state < 1)
  void depth();                // state < 2: 2 or 5 (0x0059b7b0)
  void project();              // state < 3: the size (0x005234f0(1))
  void screen();               // state < 4: 4 or 6 (0x0059b7e0)
  void through_project() { to_view(), depth(), project(); }  // 0x0058de40
  void refresh();              // 0x0058b410: anew, through to screen, then placed (0x0058ca90)
  void place();                // 0x0058ca90: the circle shown at its point when it is all inside
  bool shown() { return state != 5 && size > 0.0f; }  // [0x005f2a5c] 0

  // The node's events (0x0058f7d0..0x0058f890); the cursor's are 0x004bf620
  // but for left-up (0x0058ea60: the view's +0x30) and right-up.
  void press();
  void release();
  void enter();
  void leave();
  void left_up();
  void right_up();
  void click();
};

// An arrow pair on a link (+0x14, vtable 0x005cf1d0; +0x88, 0x005cf2d8): two
// heads, each three points joined by two lines (0x0058da30).
class ArrowObject : public ztwod::Object2D {
 public:
  explicit ArrowObject(bool tested) : Object2D(0, 0), tested_hit(tested) { hide(); }  // 0x0058d350
  void draw() override {}           // +0x04 0x0049bfd0: the link draws it
  void draw_heads();                // 0x0058da30
  int32_t hit(int32_t px, int32_t py) override;  // 0x005cf2d8's 0x0058d6b0; 0x005cf1d0's the base
  void on_enter() override;         // 0x0058f700 -> 0x0058ea10
  void on_leave() override;         // 0x0058f720 -> 0x0058ea20
  void on_press() override;         // 0x0058f6c0 -> 0x0058e920
  void on_release() override;       // 0x0058f6e0 -> 0x0058e990
  void forward();                   // 0x0058da70: the heads along the link
  void back();                      // 0x0058dfd0: the heads against it
  Link* link = nullptr;             // +0x34
  uint32_t color = 0;               // +0x38
  int32_t on_from = 0, on_to = 0;   // +0x3c, +0x40
  int32_t head_from[6] = {};        // +0x44
  int32_t head_to[6] = {};          // +0x5c
  bool tested_hit;
  bool live();                      // the link set and in a view (0x0058f6c0's test)
};

// A link between two marks (0x120 bytes, vtable 0x005cf048): kind 1 from ->
// to, 3 both ways (only the lower-numbered end makes it), 2 the cursor's line
// to a picked mark.
struct Link {
  View* view = nullptr;        // +0x04
  Mark* from = nullptr;        // +0x08
  Mark* to = nullptr;          // +0x0c
  int32_t kind = 0;            // +0x10
  ArrowObject ahead{false};    // +0x14  along: drawn for kind & 1
  ArrowObject back{true};      // +0x88  against: drawn for kind & 2
  float dir[2] = {};           // +0xfc  from -> to on screen, unit
  float perp[2] = {};          // +0x104
  int32_t line[4] = {};        // +0x10c x0, y0, x1, y1
  int32_t shown = 0;           // +0x11c
  void update();               // 0x0058ba40
  void set_dir(const float* a, const float* b);  // 0x0058b990: its length back
  float dir_length = 0;        // ours: 0x0058b990's answer left on the FPU
  void draw();                 // 0x0058b550
};

// The selection base (0x0058bf50, 0x110 bytes, vtable 0x005cf0d0): a zTwoD
// object holding the marks, the links and the window that routes the mouse
// to them.
struct View : ztwod::Object2D {
  View(Object* net, ViewHooks* hooks);  // 0x0058bf50 (the camera, +0x34, unused here)
  ~View() override;                     // 0x0058c640
  View(const View&) = delete;
  View& operator=(const View&) = delete;
  void on_left_up() override;           // +0x30 0x0058ea80: the picks into `to_link`, shift up

  void refresh(const float* cursor);    // 0x0058c830
  void frame(const float* cursor);      // 0x0058c950: update, then draw
  void pick(Mark* m);                   // 0x0058e620
  void unpick(Mark* m);                 // 0x0058e6e0
  void cursor_line(Mark* m);            // 0x0058e4c0: the cursor's line to m
  void cursor_unline(Mark* m);          // 0x0058e5a0
  Mark* first_pick() { return picks.empty() ? nullptr : picks.front(); }

  Object* net = nullptr;               // +0x38
  ViewHooks* hooks = nullptr;          // ours: the elements' virtuals
  const float* cursor_at = nullptr;    // +0x44  the cursor element's point (null: none)
  Mark cursor;                         // +0x54 (owned, 0x0058ea40)
  std::list<Link> cursor_lines;        // +0x5c
  std::vector<Link> links;             // +0x68
  std::vector<Mark> marks;             // +0x78
  ztwod::Window window{4, 4};          // +0x84 (0x005a2b50(4, 4))
  MarkObject* last = nullptr;          // +0xdc  the last pressed
  std::list<Mark*> picks;              // +0xe4
  std::list<Link*> picked_links;       // +0xf0
  std::list<Mark*> to_link;            // +0xfc
  std::list<Mark*> hovered;            // +0x108
};

}  // namespace znet
