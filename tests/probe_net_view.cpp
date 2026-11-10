// The net view (gamez/znetwork/znet_view) for test_net_view.py: two marks and
// a link between them on one camera, the exe's fields after each step as words.
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "gamez/zmath/zmth_matrix.h"
#include "gamez/znetwork/znet_view.h"
#include "gamez/ztwod/twod.h"

namespace {

struct Elem : znet::Element {
  float pos[3] = {};
  bool placed = true;
};
struct Hooks : znet::ViewHooks {
  int32_t link_count(znet::Element*) override { return 0; }
  znet::Element* link(znet::Element*, int32_t) override { return nullptr; }
  int32_t link_index(znet::Element*, znet::Element*) override { return -1; }
  bool placed(znet::Element* e) override { return static_cast<Elem*>(e)->placed; }
  const float* pos(znet::Element* e) override { return static_cast<Elem*>(e)->pos; }
  bool sees(znet::Element*, znet::Element*) override { return false; }
  uint32_t key_state(int32_t) override { return 0; }
};

uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
float f_of(const std::string& s) {
  const uint32_t u = static_cast<uint32_t>(std::stoul(s, nullptr, 16));
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

void mark_words(std::vector<uint32_t>& v, const znet::Mark& m) {
  for (float f : m.cam) v.push_back(bits(f));
  for (float f : m.scr) v.push_back(bits(f));
  v.push_back(static_cast<uint32_t>(m.state));
  v.push_back(bits(m.size)), v.push_back(bits(m.radius));
}
void arrow_words(std::vector<uint32_t>& v, const znet::ArrowObject& a) {
  v.push_back(a.flags & ztwod::kHidden);
  v.push_back(static_cast<uint32_t>(a.on_from)), v.push_back(static_cast<uint32_t>(a.on_to));
  for (int32_t i : a.head_from) v.push_back(static_cast<uint32_t>(i));
  for (int32_t i : a.head_to) v.push_back(static_cast<uint32_t>(i));
}
}  // namespace

// Lines, floats as hex words: "C m0..m11 ident fx fy cx cy mn0 mn1 mn2 mx0 mx1 mx2 height" the camera,
// "N a|b x y z placed ring" an end, "L kind" the link; then "U" (the link's update), "H x y" (each
// circle's hit), "A x y" (the back arrow's hit), "P" (each circle placed). Each adds a line of words.
extern "C" int probe_net_view(const char* plan, char* out, int cap) {
  static zmth::Mat34 m;
  zmth::reset_stack();
  zmth::push(&m);
  Hooks hooks;
  znet::Object net(9999);
  znet::View view(&net, &hooks);
  Elem ea, eb;
  znet::Mark a, b;
  a.view = b.view = &view;
  a.elem = &ea, b.elem = &eb;
  a.obj.mark = &a, b.obj.mark = &b;
  znet::Link l;
  l.view = &view;
  l.from = &a, l.to = &b;
  l.ahead.link = &l, l.back.link = &l;
  std::string o;
  std::istringstream lines(plan);
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream in(line);
    std::string op;
    in >> op;
    std::vector<std::string> w;
    for (std::string t; in >> t;) w.push_back(t);
    std::vector<uint32_t> v;
    if (op == "C") {
      for (int i = 0; i < 12; ++i) m.m[i] = f_of(w[size_t(i)]);
      zmth::matrix_stack().is_identity() = std::stoi(w[12]);
      zmth::Projection& p = zmth::projection();
      p.fx = f_of(w[13]), p.fy = f_of(w[14]), p.cx = f_of(w[15]), p.cy = f_of(w[16]);
      const float mn[3] = {f_of(w[17]), f_of(w[18]), f_of(w[19])};
      const float mx[3] = {f_of(w[20]), f_of(w[21]), f_of(w[22])};
      ztwod::set_line_clip(mn, mx);
      znet::view_height() = f_of(w[23]);
      continue;
    }
    if (op == "N") {
      znet::Mark& k = w[0] == "a" ? a : b;
      Elem& e = w[0] == "a" ? ea : eb;
      for (int i = 0; i < 3; ++i) e.pos[i] = f_of(w[size_t(1 + i)]);
      e.placed = w[4] == "1";
      k.ring = f_of(w[5]);
      k.state = 0;
      continue;
    }
    if (op == "L") {
      l.kind = std::stoi(w[0]);
      continue;
    }
    if (op == "U") {
      l.update();
      mark_words(v, a), mark_words(v, b);
      for (float f : l.dir) v.push_back(bits(f));
      for (float f : l.perp) v.push_back(bits(f));
      for (int32_t i : l.line) v.push_back(static_cast<uint32_t>(i));
      v.push_back(static_cast<uint32_t>(l.shown));
      arrow_words(v, l.ahead), arrow_words(v, l.back);
    } else if (op == "H") {
      const int32_t x = std::stoi(w[0]), y = std::stoi(w[1]);
      v.push_back(static_cast<uint32_t>(a.obj.hit(x, y)));
      v.push_back(static_cast<uint32_t>(b.obj.hit(x, y)));
    } else if (op == "A") {
      v.push_back(static_cast<uint32_t>(l.back.hit(std::stoi(w[0]), std::stoi(w[1]))));
    } else if (op == "P") {
      for (znet::Mark* k : {&a, &b}) {
        k->place();
        v.push_back(k->obj.flags & ztwod::kHidden);
        v.push_back(static_cast<uint32_t>(k->obj.x)), v.push_back(static_cast<uint32_t>(k->obj.y));
      }
    } else {
      continue;
    }
    for (size_t i = 0; i < v.size(); ++i) o += (i ? " " : "") + std::to_string(v[i]);
    o += "\n";
  }
  zmth::pop();
  if (static_cast<int>(o.size()) >= cap) return -1;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}
