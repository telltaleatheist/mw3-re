#include "mech3/zainet.h"

#include <algorithm>
#include <cmath>

#include "mech3/zainetedit.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {

// Mech3.exe 1.2 @ 0x004020d0 at the 53-bit precision of the load: the sums,
// the root and the inverse in double (zmth::normalize is the 24-bit one).
float normalize_load(float v[3]) {
  const double len = std::sqrt((double(v[0]) * v[0] + double(v[2]) * v[2]) + double(v[1]) * v[1]);
  if (float(len) != 0.0f) {
    const double k = 1.0 / len;
    v[0] = float(k * v[0]), v[1] = float(k * v[1]), v[2] = float(k * v[2]);
  }
  return float(len);
}

// Mech3.exe 1.2 @ 0x004d52a0
const char* AiNet::name() { return net_name(get_id()); }

// Mech3.exe 1.2 @ 0x004d53f0: each element's +0x68 (itself for every kind
// here), then each one's link pass.
void AiNet::link_pass() {
  const int32_t n = count();
  for (int32_t i = 0; i < n; ++i) {
    NetNode* nd = at(i) ? at(i)->as_node() : nullptr;
    if (nd) nd->relink();
  }
}

// Mech3.exe 1.2 @ 0x004c6d30 (the record copied by 0x004c6ce0 -> 0x004c6050)
NetNode::NetNode(CZAiNetEdit* o, const NetNodeRec& r) : rec(r), owner(o) {}

// Mech3.exe 1.2 @ 0x004c6da0
NetNode::NetNode(CZAiNetEdit* o) : owner(o), placeholder(true) {}

// Mech3.exe 1.2 @ 0x004c71f0. The exe lets i reach the count, one link past
// the end (FIX (bug): stopped there; no caller asks for it).
NetNode* NetNode::link_node(int32_t i) const {
  if (i < 0 || i >= link_count()) return nullptr;
  return owner->node(rec.links[size_t(i)]);
}

// Mech3.exe 1.2 @ 0x004c7270: the link's number that leads to `to` (by its
// number in the net), -1 for none.
int32_t NetNode::link_index(NetNode* to) const {
  const int32_t j = owner->index_of(to);
  const auto it = std::find(rec.links.begin(), rec.links.end(), j);
  if (j < 0 || it == rec.links.end()) return -1;
  return static_cast<int32_t>(it - rec.links.begin());
}

// Mech3.exe 1.2 @ 0x004c6ec0 (the placeholder's slot is 0x00410510: no): a
// link to `target` (its number in the net) added after the last unless one
// leads there, then stored again through the target (its vt+0x40, 0x004c6fa0
// -> this vt+0x44, set_link). When that refuses, the last link is taken off,
// the new one or, when there was one already, the last there was.
bool NetNode::add_link(NetNode* target) {
  if (placeholder) return false;
  NetNode* t = target ? target->as_node() : nullptr;  // vt+0x60 then +0x88
  const int32_t j = owner->index_of(t);
  if (!t || j < 0) return false;
  auto it = std::find(rec.links.begin(), rec.links.end(), j);
  int32_t i;
  if (it != rec.links.end()) {
    i = static_cast<int32_t>(it - rec.links.begin());
  } else {
    i = link_count();
    rec.links.push_back(j);
  }
  const bool ok = !t->placeholder && set_link(t, i);  // the placeholder's vt+0x40 is 0x00410490
  if (!ok) rec.links.pop_back();
  return ok;
}

// Mech3.exe 1.2 @ 0x004c71a0 (the placeholder's slot is 0x00410510: no),
// then the fold into the file's cache (0x004befc0), which acts only in the
// editor.
bool NetNode::set_pos(const float p[3]) {
  if (placeholder) return false;
  rec.pos[0] = p[0], rec.pos[1] = p[1], rec.pos[2] = p[2];
  owner->fold(this);
  return true;
}

// Mech3.exe 1.2 @ 0x00410590: each link, last first, told which node points at
// it, so that node can store the target's index again.
void NetNode::relink() {
  for (int32_t i = link_count() - 1; i >= 0; --i) {
    NetNode* t = link_node(i);
    if (t) t->link_from(this, i);
  }
}

// Mech3.exe 1.2 @ 0x004c7170 (the placeholder's slot is 0x00410490: nothing)
void NetNode::link_from(NetNode* src, int32_t i) {
  if (!placeholder) src->set_link(this, i);
}

// Mech3.exe 1.2 @ 0x004c6fc0 (the placeholder's slot is 0x00410490: nothing)
bool NetNode::set_link(NetNode* target, int32_t i) {
  if (placeholder) return false;
  rec.links[size_t(i)] = owner->index_of(target ? target->as_node() : nullptr);
  owner->fold(this);  // 0x004befc0
  return true;
}

namespace {

// 0x004c7300's 0x14 bytes: the net, the centre, the count over 2 pi, 2 pi
// over the count, the radius.
struct Arranger {
  CZAiNetEdit* net;
  const float* centre;
  float per_turn;
  float step;
  float radius;
};

// Mech3.exe 1.2 @ 0x004c7340: node `i` moved onto the circle at the nearest
// of the count's even angles, its height kept, each coordinate rounded to a
// tenth.
void arrange_one(const Arranger& a, int32_t i) {
  // FIX (bug): the exe reads the net's slot unchecked; a link past the nodes
  // reads past the vector.
  NetNode* nd = (i >= 0 && i < a.net->count() && a.net->at(i)) ? a.net->at(i)->as_node() : nullptr;
  if (!nd) return;
  const float* p = nd->pos();
  float d[3] = {p[0] - a.centre[0], 0.0f, p[2] - a.centre[2]};
  normalize_load(d);
  const double turn = std::floor(double(std::atan2(double(d[0]), double(d[2])) * double(a.per_turn) + 0.5));
  const double ang = turn * double(a.step);
  float out[3];
  out[0] = float(std::sin(ang) * double(a.radius) + double(a.centre[0]));
  out[2] = float(std::cos(ang) * double(a.radius) + double(a.centre[2]));
  out[1] = p[1];
  for (float& v : out) v = float(std::floor(0.5 - double(v) * -10.0) * 0.1);
  if (nd->set_pos(out)) nd->relink();
}

}  // namespace

// Mech3.exe 1.2 @ 0x004c74a0: arrangenodes (spacing) spreads the nodes this
// one links to evenly round it.
void NetNode::arrange() {
  if (!rec.arrange_set) return;
  const int32_t n = link_count();
  if (n <= 0) return;
  // 0x004c7300: both from the unrounded product.
  const double turns = double(n) * 0.15915494309188485;
  Arranger a{owner, pos(), float(turns), float(1.0 / turns), rec.arrange};
  for (int32_t l : rec.links) arrange_one(a, l);
}

}  // namespace mech3::ai
