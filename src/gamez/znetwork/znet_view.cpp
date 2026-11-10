#include "gamez/znetwork/znet_view.h"

#include <algorithm>
#include <cmath>

#include "crt/msvc_math.h"
#include "gamez/zinput/zinput.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zvideo/zvid_format.h"
#include "gamez/ztwod/twod_host.h"

#pragma STDC FP_CONTRACT OFF

namespace znet {

namespace {
ViewColors g_colors;
float g_height = 0.0f;  // [0x00730804]

constexpr float kMinRadius = 8.0f;      // [0x005f2a54]
constexpr float kMaxRadius = 0.125f;    // [0x005f2a58], of the height
constexpr float kHeadBack = -0.5f;      // [0x005cf020]: a head's hit depth, of the radius
constexpr float kShortest = 2.0f;       // [0x005cf000]: a link shorter on screen is not drawn
constexpr int32_t kShift = 0x2a;        // DIK_LSHIFT

// A head's three points (0x005f2a68..0x005f2a80): along, across, in radii.
constexpr float kHead[3][2] = {{0.5f, 0.5f}, {1.0f, 0.0f}, {0.5f, -0.5f}};

int32_t round_px(float v) { return crt::msvc_ftol(static_cast<double>(v) - -0.5); }  // fsub [0x005cf018]

// 0x0058d9a0 (ECX the arrow, its link's direction at +0xfc): a head's points
// around `at`, `along` radii out from it, `size` pixels a radius.
void head(const Link& l, int32_t out[6], const float* at, float size, float along) {
  for (int i = 0; i < 3; ++i) {
    const float a = along + kHead[i][0], b = kHead[i][1];
    out[2 * i] = round_px((a * l.dir[0] + b * l.perp[0]) * size + at[0]);
    out[2 * i + 1] = round_px((a * l.dir[1] + b * l.perp[1]) * size + at[1]);
  }
}

template <class T>
void remove_all(std::list<T*>& l, T* v) {
  l.remove(v);
}
template <class T>
bool holds(const std::list<T*>& l, T* v) {
  return std::find(l.begin(), l.end(), v) != l.end();
}
}  // namespace

ViewColors& view_colors() { return g_colors; }
float& view_height() { return g_height; }

uint32_t ViewHooks::key_state(int32_t key) { return zin::key_state(key); }

// ---- Mark ----

// The cursor's element (0x0058e3b0, vtable 0x005cf250): vt+0x1c (0x004104d0)
// gives its point (+0x04, the view's +0x44) or, without one, a zero point
// (0x0063d870).
const float* Mark::world() {
  static const float none[3] = {};
  if (kind == kCursor) return view->cursor_at ? view->cursor_at : none;
  return view->hooks->pos(elem);
}
bool Mark::placed() { return kind == kCursor || view->hooks->placed(elem); }

// Mech3.exe 1.2 @ 0x0058b890 (0x0058ce10 and 0x0058de40 inline it, 0x0058b410
// calls 0x004c3520: the same): the point through the top of the matrix stack.
void Mark::to_view() {
  if (state >= 1) return;
  const float* p = world();
  zmth::MatrixStack& st = zmth::matrix_stack();
  if (!st.is_identity()) {
    const float* t = st.matrix().m;
    cam[0] = ((p[0] * t[0] + p[1] * t[3]) + p[2] * t[6]) + t[9];
    cam[1] = ((p[0] * t[1] + p[1] * t[4]) + p[2] * t[7]) + t[10];
    cam[2] = ((p[0] * t[2] + p[1] * t[5]) + p[2] * t[8]) + t[11];
  } else {
    cam[0] = p[0], cam[1] = p[1], cam[2] = p[2];
  }
  state = 1;
  scr[0] = cam[0], scr[1] = cam[1], scr[2] = cam[2];
}

void Mark::depth() {
  if (state < 2) state = ztwod::in_depth(scr) ? 2 : 5;
}

void Mark::project() {
  if (state >= 3) return;
  zmth::Vec3* v = reinterpret_cast<zmth::Vec3*>(scr);
  zmth::project(v, v, 1);  // 0x005234f0
  const float h = g_height;
  size = scr[2] * h * 1.0f;  // [0x005f2a50]
  radius = size;
  if (size < kMinRadius) radius = kMinRadius;
  else if (h * kMaxRadius < size) radius = h * kMaxRadius;
  state = 3;
}

void Mark::screen() {
  if (state < 4) state = ztwod::in_box(scr) ? 4 : 6;
}

// Mech3.exe 1.2 @ 0x0058b410
void Mark::refresh() {
  state = 0;
  to_view();
  state = ztwod::in_depth(scr) ? 2 : 5;
  project();
  screen();
  place();
}

// Mech3.exe 1.2 @ 0x0058ca90 (ECX the circle): shown at the point, rounded,
// only when its whole circle is inside the clip.
void Mark::place() {
  to_view(), depth(), project();
  if (!shown()) {
    obj.hide();
    return;
  }
  to_view(), depth(), project();
  const float r = static_cast<float>(crt::msvc_ftol(radius * ring));
  const ztwod::LineClip& c = ztwod::line_clip();
  if (static_cast<float>(c.ints[0]) <= scr[0] - r && static_cast<float>(c.ints[1]) <= scr[1] - r &&
      scr[0] + r <= static_cast<float>(c.ints[2]) && r + scr[1] <= static_cast<float>(c.ints[3])) {
    obj.show_for(-1.0f);
    const int32_t y = round_px(scr[1]);
    obj.set_pos(round_px(scr[0]), y);
    return;
  }
  obj.hide();
}

// Mech3.exe 1.2 @ 0x0058f7d0 -> 0x0058e620
void Mark::press() {
  if (kind == kNode && view) view->pick(this);
}
// Mech3.exe 1.2 @ 0x0058f7f0 -> 0x0058e770: shift keeps the pick (and the
// circle pressed).
void Mark::release() {
  if (kind != kNode || !view) return;
  if (view->hooks->key_state(kShift) & 3) {
    obj.mouse |= ztwod::kPressed;
    return;
  }
  view->unpick(this);
}
// Mech3.exe 1.2 @ 0x0058f810 -> 0x0058e7b0
void Mark::enter() {
  if (kind != kNode) return;
  flags |= 1;
  if (!view) return;
  obj.color = g_colors.pressed;
  if (!holds(view->hovered, this)) view->hovered.push_back(this);
}
// Mech3.exe 1.2 @ 0x0058f830 -> 0x0058e850
void Mark::leave() {
  if (kind != kNode) return;
  flags &= ~1u;
  if (!view) return;
  obj.color = (obj.mouse & ztwod::kPressed) ? g_colors.lit : g_colors.normal;
  remove_all(view->hovered, this);
}
// Mech3.exe 1.2 @ 0x0058f850; the cursor's 0x0058ea60: the view's +0x30.
void Mark::left_up() {
  if (kind == kCursor) {
    view->on_left_up();
    return;
  }
  flags |= 2;
}
// Mech3.exe 1.2 @ 0x0058f870; the cursor's 0x0058ea70: the view's +0x34
// (0x005ac5c0, nothing).
void Mark::right_up() {
  if (kind == kNode) flags |= 4;
}
// Mech3.exe 1.2 @ 0x0058f890 -> 0x0058e8e0: shift on a circle other than the
// last pressed drops its pick.
void Mark::click() {
  if (kind != kNode) return;
  flags |= 2;
  if (!view) return;
  if (view->last != &obj && (view->hooks->key_state(kShift) & 3)) {
    obj.mouse &= static_cast<uint16_t>(~ztwod::kPressed);
    view->unpick(this);
  }
  view->last = &obj;
}

// ---- MarkObject ----

// Mech3.exe 1.2 @ 0x0058cf20: the circle, radius times its ring, at its place.
void MarkObject::draw_circle() {
  erase();
  mark->through_project();
  mark->to_view(), mark->depth(), mark->project();
  const float ring = mark->ring;
  ztwod::host().circle(x, y, crt::msvc_ftol(mark->radius * ring), color, nullptr);
}

// Mech3.exe 1.2 @ 0x0058d0e0: within the circle (radius times its ring),
// whole pixels.
int32_t MarkObject::hit(int32_t px, int32_t py) {
  if (flags & ztwod::kHidden) return 0;
  mark->through_project();
  const int32_t r = crt::msvc_ftol(mark->radius * mark->ring);
  const int32_t dx = px - round_px(mark->scr[0]);
  const int32_t dy = py - round_px(mark->scr[1]);
  const uint32_t d2 = static_cast<uint32_t>(dx * dx + dy * dy);
  return static_cast<uint32_t>(r * r) >= d2 ? 1 : 0;
}

void MarkObject::on_press() { if (mark) mark->press(); }
void MarkObject::on_release() { if (mark) mark->release(); }
void MarkObject::on_enter() { if (mark) mark->enter(); }
void MarkObject::on_leave() { if (mark) mark->leave(); }
void MarkObject::on_left_up() { if (mark) mark->left_up(); }
void MarkObject::on_right_up() { if (mark) mark->right_up(); }
void MarkObject::on_click() { if (mark) mark->click(); }

// ---- ArrowObject ----

bool ArrowObject::live() { return link && link->view; }

// Mech3.exe 1.2 @ 0x0058da30: each head shown, as two lines.
void ArrowObject::draw_heads() {
  const ztwod::LineClip& c = ztwod::line_clip();
  const ztwod::Rect clip{c.ints[0], c.ints[1], c.ints[2], c.ints[3]};
  if (on_from) ztwod::host().line_list(head_from, 2, &clip, color);
  if (on_to) ztwod::host().line_list(head_to, 2, &clip, color);
}

// Mech3.exe 1.2 @ 0x0058d6b0 (the 0x005cf2d8 arrow's; the 0x005cf1d0 one keeps
// the base 0x0059fd50): within a head's triangle behind its tip, radius / 2
// deep, the far end's radius for the first.
int32_t ArrowObject::hit(int32_t px, int32_t py) {
  if (!tested_hit) return Object2D::hit(px, py);
  if (flags & ztwod::kHidden) return 0;
  const auto inside = [&](const int32_t* tip, Mark* m) {
    const float dx = static_cast<float>(px - tip[2]), dy = static_cast<float>(py - tip[3]);
    const float u = -(link->dir[0] * dx + dy * link->dir[1]);
    const float v = -(dy * link->perp[1] + dx * link->perp[0]);
    m->through_project();
    return m->radius * kHeadBack < u && u < v && u < -v;
  };
  if (inside(head_from, link->to)) return 1;
  return inside(head_to, link->from) ? 1 : 0;
}

// Mech3.exe 1.2 @ 0x0058f700 -> 0x0058ea10
void ArrowObject::on_enter() {
  if (live()) color = g_colors.pressed;
}
// Mech3.exe 1.2 @ 0x0058f720 -> 0x0058ea20
void ArrowObject::on_leave() {
  if (live()) color = (mouse & ztwod::kPressed) ? g_colors.lit : g_colors.normal;
}
// Mech3.exe 1.2 @ 0x0058f6c0 -> 0x0058e920
void ArrowObject::on_press() {
  if (!live()) return;
  color = g_colors.lit;
  link->view->picked_links.push_back(link);
}
// Mech3.exe 1.2 @ 0x0058f6e0 -> 0x0058e990
void ArrowObject::on_release() {
  if (!live()) return;
  color = g_colors.normal;
  remove_all(link->view->picked_links, link);
}

// Mech3.exe 1.2 @ 0x0058da70: hidden with the link; else a head by each end
// that shows, pointing along: at the start 0.75 radii out ([0x005f2a60]), at
// the end 1.75 back.
void ArrowObject::forward() {
  if (!link->shown) {
    on_to = on_from = 0;
    hide();
    return;
  }
  show_for(-1.0f);
  Mark* a = link->from;
  a->through_project();
  on_from = a->shown() ? 1 : 0;
  if (on_from) head(*link, head_from, a->scr, a->radius, a->ring);
  Mark* b = link->to;
  b->through_project();
  on_to = b->shown() ? 1 : 0;
  if (on_to) head(*link, head_to, b->scr, b->radius, -(b->ring - -1.0f));
}

// Mech3.exe 1.2 @ 0x0058dfd0: the same pointing back, from the end.
void ArrowObject::back() {
  if (!link->shown) {
    on_to = on_from = 0;
    hide();
    return;
  }
  show_for(-1.0f);
  Mark* b = link->to;
  b->through_project();
  on_from = b->shown() ? 1 : 0;
  if (on_from) head(*link, head_from, b->scr, -b->radius, b->ring);
  Mark* a = link->from;
  a->through_project();
  on_to = a->shown() ? 1 : 0;
  if (on_to) head(*link, head_to, a->scr, -a->radius, -(a->ring - -1.0f));
}

// ---- Link ----

// Mech3.exe 1.2 @ 0x0058b990: the unit direction a -> b on screen (x across,
// when they meet) and its normal; the length is left on the FPU.
void Link::set_dir(const float* a, const float* b) {
  dir[0] = b[0] - a[0];
  dir[1] = b[1] - a[1];
  const float l2 = dir[1] * dir[1] + dir[0] * dir[0];
  if (l2 <= 0.0f) {
    dir[0] = 1.0f, dir[1] = 0.0f;
    dir_length = l2;
  } else {
    const float len = std::sqrt(l2);
    const float inv = 1.0f / len;  // [0x005cf010]
    dir[0] = inv * dir[0];
    dir[1] = inv * dir[1];
    dir_length = len;
  }
  perp[0] = -dir[1];
  perp[1] = dir[0];
}

// Mech3.exe 1.2 @ 0x0058ba40: both ends to the screen, then the line by
// where each one is: within the depth and the box (4), within the depth only
// (6) or outside the depth (5, cut there in view space first, 0x0059b370).
void Link::update() {
  shown = 1;
  from->through_project(), from->screen();
  to->through_project(), to->screen();
  float a[3], b[3];
  const auto copy = [](float* d, const float* s) { d[0] = s[0], d[1] = s[1], d[2] = s[2]; };
  switch (from->state << 8 | to->state) {
    case 0x404:
      copy(a, from->scr), copy(b, to->scr);
      set_dir(from->scr, to->scr);
      break;
    case 0x406:
    case 0x604:
    case 0x606:
      copy(a, from->scr), copy(b, to->scr);
      shown = ztwod::clip_line(a, b);
      set_dir(from->scr, to->scr);
      break;
    case 0x505:
      copy(a, from->cam), copy(b, to->cam);
      shown = ztwod::clip_depth(a, b);
      if (shown) {
        zmth::Vec3 v[2] = {{a[0], a[1], a[2]}, {b[0], b[1], b[2]}};
        zmth::project(v, v, 2);
        a[0] = v[0].x, a[1] = v[0].y, a[2] = v[0].z;
        b[0] = v[1].x, b[1] = v[1].y, b[2] = v[1].z;
        shown = ztwod::clip_line(a, b);
      }
      set_dir(a, b);
      break;
    case 0x504:
    case 0x506:
      copy(a, from->cam), copy(b, to->cam);
      ztwod::clip_depth(a, b);
      copy(b, to->scr);
      zmth::project(reinterpret_cast<zmth::Vec3*>(a), reinterpret_cast<zmth::Vec3*>(a), 1);
      shown = ztwod::clip_line(a, b);
      set_dir(a, b);
      break;
    case 0x405:
    case 0x605:
      copy(a, from->cam), copy(b, to->cam);
      ztwod::clip_depth(a, b);
      copy(a, from->scr);
      zmth::project(reinterpret_cast<zmth::Vec3*>(b), reinterpret_cast<zmth::Vec3*>(b), 1);
      shown = ztwod::clip_line(a, b);
      set_dir(a, b);
      break;
    default:
      return;  // unreachable: every end is 4, 5 or 6 by now
  }
  line[0] = round_px(a[0]), line[1] = round_px(a[1]);
  line[2] = round_px(b[0]), line[3] = round_px(b[1]);
  if (dir_length < kShortest) shown = 0;
  if (!from->placed() || !to->placed()) shown = 0;
  ahead.forward();
  back.back();
}

// Mech3.exe 1.2 @ 0x0058b550: the heads (each pair a closed triangle), the
// line green under the mouse, yellow pressed, else blue when its ends see
// each other, red when not.
void Link::draw() {
  if (shown != 1) return;
  if ((kind & 1) && !(ahead.flags & ztwod::kHidden)) ahead.draw_heads();
  if ((kind & 2) && !(back.flags & ztwod::kHidden)) back.draw_heads();
  uint32_t c = g_colors.lit;
  // FIX (bug): the exe reads the first byte of its uninitialised 0x500-byte
  // buffer when the ends see each other (vt+0x28 gives it back unwritten);
  // here the answer alone picks blue (docs/fixes.md).
  // The cursor's element answers null (0x00410490): its lines are red.
  if (!(ahead.mouse & ztwod::kPressed) && !(back.mouse & ztwod::kPressed))
    c = from->kind == Mark::kNode && view->hooks->sees(from->elem, to->elem) ? g_colors.clear : g_colors.normal;
  ztwod::host().line(line[0], line[1], line[2], line[3], c);
  if ((ahead.mouse & ztwod::kHover) || (back.mouse & ztwod::kHover)) c = g_colors.pressed;
  const ArrowObject& o = (kind & 1) ? ahead : back;
  int32_t first[4], second[4];
  if (kind & 1) {
    first[0] = o.head_from[0], first[1] = o.head_from[1], first[2] = o.head_from[4], first[3] = o.head_from[5];
    second[0] = o.head_to[0], second[1] = o.head_to[1], second[2] = o.head_to[4], second[3] = o.head_to[5];
  } else {
    first[0] = o.head_to[0], first[1] = o.head_to[1], first[2] = o.head_to[4], first[3] = o.head_to[5];
    second[0] = o.head_from[0], second[1] = o.head_from[1], second[2] = o.head_from[4], second[3] = o.head_from[5];
  }
  const ztwod::LineClip& lc = ztwod::line_clip();
  const ztwod::Rect clip{lc.ints[0], lc.ints[1], lc.ints[2], lc.ints[3]};
  from->through_project();
  if (from->shown()) ztwod::host().line_list(first, 1, &clip, c);
  to->through_project();
  if (to->shown()) ztwod::host().line_list(second, 1, &clip, c);
}

// ---- View ----

// Mech3.exe 1.2 @ 0x0058bf50: a mark for every element (its circle in the
// window when the element is placed), a link for every pair, the cursor's
// mark (hidden), the colours.
View::View(Object* n, ViewHooks* h) : Object2D(0, 0), net(n), hooks(h) {
  cursor.kind = Mark::kCursor;
  cursor.view = this;
  ViewColors& c = g_colors;
  c.normal = zvid::pack_bytes(0xff, 0, 0);
  c.pressed = zvid::pack_bytes(0, 0xff, 0);
  c.lit = zvid::pack_bytes(0xff, 0xff, 0);
  c.cyan = zvid::pack_bytes(0, 0xff, 0xff);
  c.clear = zvid::pack_bytes(0, 0, 0xff);
  const int32_t count = net->count();
  marks.resize(static_cast<size_t>(std::max(count, 0)));
  // A link for each element's link to a later one, and to an earlier one
  // that has none back (both ways is made once, by the earlier end).
  struct Pair { int32_t a, b, kind; };
  std::vector<Pair> pairs;
  for (int32_t i = 0; i < count; ++i) {
    Element* e = net->elem(i);
    for (int32_t k = 0; k < hooks->link_count(e); ++k) {
      Element* o = hooks->link(e, k);
      const int32_t j = net->index_of(o);
      const bool one_way = hooks->link_index(o, e) < 0;
      if (i < j || (j >= 0 && one_way)) pairs.push_back({i, j, one_way ? 1 : 3});
    }
  }
  links.resize(pairs.size());
  window.set_active(1);
  cursor.obj.mark = &cursor;
  cursor.obj.color = c.normal;
  cursor.obj.hide();
  window.add(&cursor.obj);
  size_t p = 0;
  for (int32_t i = 0; i < count; ++i) {
    Mark& m = marks[size_t(i)];
    m.elem = net->elem(i);
    m.state = 0;
    m.view = this;
    m.obj.mark = &m;
    m.obj.color = c.normal;
    if (hooks->placed(m.elem)) window.add(&m.obj);
    for (; p < pairs.size() && pairs[p].a == i; ++p) {
      Link& l = links[p];
      l.view = this;
      l.from = &m;
      l.to = &marks[size_t(pairs[p].b)];
      l.ahead.link = &l, l.ahead.color = c.normal;
      l.back.link = &l, l.back.color = c.normal;
      window.add(&l.ahead);
      l.kind = pairs[p].kind;
      if (l.kind == 3) window.add(&l.back);
    }
  }
}

// Mech3.exe 1.2 @ 0x0058c640: the lists, the links and the marks freed (each
// object leaves the window as it goes).
View::~View() {
  for (Link& l : cursor_lines) window.remove(&l.ahead), window.remove(&l.back);
  for (Link& l : links) window.remove(&l.ahead), window.remove(&l.back);
  for (Mark& m : marks) window.remove(&m.obj);
  window.remove(&cursor.obj);
}

// Mech3.exe 1.2 @ 0x0058ea80: unless shift is held, the picks become the
// marks to link.
void View::on_left_up() {
  if ((hooks->key_state(kShift) & 3) == 0 && !picks.empty()) to_link = picks;
}

// Mech3.exe 1.2 @ 0x0058c830: every mark and link anew, the cursor at its
// point (hidden without one), the mouse through the window, the cursor's
// lines.
void View::refresh(const float* at) {
  const ztwod::LineClip& c = ztwod::line_clip();
  g_height = static_cast<float>((c.ints[3] - c.ints[1]) + 1);  // 0x0058b530
  for (Mark& m : marks) m.refresh();
  for (Link& l : links) l.update();
  cursor_at = at;
  if (!at) {
    cursor.obj.hide();
  } else {
    cursor.obj.show_for(-1.0f);
    cursor.refresh();
  }
  (void)hooks->key_state(kShift);  // read and dropped
  last = nullptr;
  window.update(0.1f);
  if (cursor_at)
    for (Link& l : cursor_lines) l.update();
}

// Mech3.exe 1.2 @ 0x0058c950
void View::frame(const float* at) {
  refresh(at);
  for (Mark& m : marks)
    if (!(m.obj.flags & ztwod::kHidden)) m.obj.draw_circle();  // 0x0058b510
  for (Link& l : links) l.draw();
  if (cursor_at && !cursor_lines.empty()) {
    if (!(cursor.obj.flags & ztwod::kHidden)) cursor.obj.draw_circle();
    for (Link& l : cursor_lines) l.draw();
  }
}

// Mech3.exe 1.2 @ 0x0058ebc0
bool View::press(Element* e) {
  for (Mark& m : marks)
    if (m.elem == e) {
      if (!(m.obj.mouse & ztwod::kPressed)) {
        m.obj.mouse |= ztwod::kPressed;
        m.obj.on_press();
      }
      return true;
    }
  return false;
}

// Mech3.exe 1.2 @ 0x0058e620 (from the circle's press): the last pressed,
// lit, picked once, and the cursor's line to it (each press adds one).
void View::pick(Mark* m) {
  last = &m->obj;
  m->obj.color = g_colors.lit;
  if (!holds(picks, m)) picks.push_back(m);
  cursor_line(m);
}

// Mech3.exe 1.2 @ 0x0058e6e0
void View::unpick(Mark* m) {
  m->obj.color = g_colors.normal;
  remove_all(picks, m);
  cursor_unline(m);
}

// Mech3.exe 1.2 @ 0x0058e4c0: a kind-2 link from the cursor, at the front.
// Its arrows are not in the window.
void View::cursor_line(Mark* m) {
  cursor_lines.emplace_front();
  Link& l = cursor_lines.front();
  l.view = this;
  l.from = &cursor;
  l.to = m;
  l.ahead.link = &l, l.ahead.color = g_colors.normal;
  l.back.link = &l, l.back.color = g_colors.normal;
  l.kind = 2;
}

// Mech3.exe 1.2 @ 0x0058e5a0: the first line to m.
void View::cursor_unline(Mark* m) {
  for (auto it = cursor_lines.begin(); it != cursor_lines.end(); ++it)
    if (it->to == m) {
      cursor_lines.erase(it);
      return;
    }
}

}  // namespace znet
