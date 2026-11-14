#include "mech3/zainet105.h"
#include "mech3/veh_basic_controls.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mech3::ai {
namespace {
Net105* g_head = nullptr;  // [0x005f4c4c]
Net105* g_tail = nullptr;  // [0x005f4c50]
}  // namespace

// Mech3.exe 1.2 @ 0x004ae920: [0x005e8654], the null node every "none" answer gives; its static constructor is
// 0x004ae8b0.
Net105Node* Net105Node::none() {
  static Net105Node n;
  return &n;
}

// Mech3.exe 1.2 @ 0x004024a0: each link record freed (the exe's malloc), then the node.
Net105Node::~Net105Node() {
  for (Net105Link*& r : link_rec) {
    std::free(r);
    r = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x004024e0: each node by +0x30, then the net. The net's making is 0x004021c0 with 0x00402190 (its
// fields cleared) and the list's append below.
Net105::~Net105() {
  for (Net105Node* n = first; n;) {
    Net105Node* next_node = n->next;
    delete n;
    n = next_node;
  }
  first = nullptr;
}

// Mech3.exe 1.2 @ 0x00401ab0's making (ours to own): a node of the defaults.
Net105Node* Net105::make_node() {
  made.push_back(std::make_unique<Net105Node>());
  return made.back().get();
}

// The basic controls' 0x004023d0 (mech3/veh_basic_controls.cpp) by default; a test may stub it.
void (*net105_link_fill)(Net105Link& rec, const float from[3], const float to[3], float width) = mech3::basic_link_fill;

namespace {
// 0x004023d0 through the hook (the basic controls' own); none leaves the record as it is.
void fill(Net105Link* rec, const float from[3], const float to[3], float width) {
  if (rec && net105_link_fill) net105_link_fill(*rec, from, to, width);
}
Net105Link* new_link() { return static_cast<Net105Link*>(std::calloc(1, sizeof(Net105Link))); }
}  // namespace

// Mech3.exe 1.2 @ 0x00403cd0 (vt+0x30)
bool Net105Node::add_link(Net105Node* other) {
  if (is_none()) return false;
  int32_t i = 0;
  while (i < 3 && link_rec[i]) ++i;
  return i < 3 && other->link_pair(this, i);
}

// Mech3.exe 1.2 @ 0x00403d10 (vt+0x40): `from` into this's first slot with no record, this into `from`'s slot i.
bool Net105Node::link_pair(Net105Node* from, int32_t i) {
  if (is_none()) return false;
  int32_t j = 0;
  while (j < 3 && link_rec[j]) ++j;
  if (j >= 3) return false;
  set_link(from, j);
  from->set_link(this, i);
  return true;
}

// Mech3.exe 1.2 @ 0x00403d60 (vt+0x44): the node (`to`'s vt+0x60 then vt+0x80: itself) in slot i, a fresh record
// (the exe's malloc, measured at once), measured.
void Net105Node::set_link(Net105Node* to, int32_t i) {
  if (is_none()) return;
  links[i] = to;
  std::free(link_rec[i]);  // ours: the exe overwrites the pointer and leaks the old record
  link_rec[i] = new_link();
  measure(to, i);
}

// Mech3.exe 1.2 @ 0x00403db0 (vt+0x38): from the last slot down, each link (the null node for none, which does
// nothing) remeasures its way back to this.
void Net105Node::remeasure_all() {
  if (is_none()) return;
  for (int32_t i = link_count() - 1; i >= 0; --i)
    if (Net105Node* l = link(i)) l->remeasure(this, i);
}

// Mech3.exe 1.2 @ 0x00403df0 (vt+0x3c): the slot holding `to`, then this's remeasure with itself as the other end.
void Net105Node::remeasure_to(Net105Node* to) {
  if (is_none()) return;
  const int32_t j = link_index(to);
  if (j >= 0) remeasure(this, j);
}

// Mech3.exe 1.2 @ 0x00403e20 (vt+0x50): the slot holding `to` measured again, and `to`'s slot i back to this.
bool Net105Node::remeasure(Net105Node* to, int32_t i) {
  if (is_none()) return false;
  int32_t j = 0;
  while (j < 3 && links[j] != to) ++j;
  if (j >= 3) return false;
  measure(to, j);
  to->measure(this, i);
  return true;
}

// Mech3.exe 1.2 @ 0x00403e70 (vt+0x54): record i zeroed and filled from this place to `to`'s; the width is the 0
// the call to `to`'s place (0x00409fc0, a bare ret) leaves on the stack.
void Net105Node::measure(Net105Node* to, int32_t i) {
  Net105Link* r = link_rec[i];
  if (!r) return;  // FIX (A): the exe zeroes through the pointer; set_link always made one first
  *r = Net105Link{};
  fill(r, place, to->place, 0.0f);
}

namespace {
// Mech3.exe 1.2 @ 0x004022e0 (ECX the number, EDX the first node): the node of that number along +0x30, none null.
Net105Node* node_numbered(Net105Node* first, int32_t number) {
  Net105Node* n = first;
  while (n && n->number != number) n = n->next;
  return n;
}

// Mech3.exe 1.2 @ 0x00402300 (ECX the first node, the path width): each node's three links, a number until now,
// made the node (none for a negative one), each with a fresh record measured to it at that width.
void resolve(Net105Node* first, float width) {
  for (Net105Node* n = first; n; n = n->next) {
    for (int32_t i = 0; i < 3; ++i) {
      const int32_t number = static_cast<int32_t>(reinterpret_cast<intptr_t>(n->links[i]));
      if (number < 0) {
        n->links[i] = nullptr;
        n->link_rec[i] = nullptr;
        continue;
      }
      n->links[i] = node_numbered(first, number);
      n->link_rec[i] = new_link();
      // FIX (A): a number no node has leaves the link null; the exe reads the null's place (a crash).
      if (n->links[i]) fill(n->link_rec[i], n->place, n->links[i]->place, width);
    }
  }
}

const char* str1(zrdr::Node* v) { return v->list[1].s; }
float f1(zrdr::Node* v, int32_t k) { return v->list[k].f; }
}  // namespace

// Mech3.exe 1.2 @ 0x004025b0, 0x00402590 (its thunk) (ECX `x18`, EDX the block, the name on the stack). The values are read as the exe
// does, from each key's list's first (and second) item as it stands, whatever its type.
Net105* net105_read(zrdr::Node* block, const char* default_name, int32_t x18) {
  auto* net = new Net105;  // 0x004021c0: made and put on the list
  nets105_append(net);
  net->x18 = x18;
  const char* name = default_name ? default_name : "";
  if (zrdr::Node* v = zrdr::find_flat(block, "name", 1)) name = str1(v);
  std::snprintf(net->name_, sizeof net->name_, "%s", name);  // FIX (A): the exe's strcpy runs on past 20
  net->kind = 0;
  if (zrdr::Node* v = zrdr::find_flat(block, "type", 1)) {
    char t[20] = {};
    std::snprintf(t, sizeof t, "%s", str1(v));
    for (char& c : t) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (std::strncmp(t, "ST", 2) == 0) net->kind = 0;
    else if (std::strncmp(t, "HI", 2) == 0) net->kind = 1;
    else if (std::strncmp(t, "FI", 2) == 0) net->kind = 2;
  }
  zrdr::Node* v = zrdr::find_flat(block, "path_width", 1);
  net->path_width = v ? f1(v, 1) : 10.0f;
  if ((v = zrdr::find_flat(block, "activate_rad", 1))) net->activate_rad = f1(v, 1);
  if ((v = zrdr::find_flat(block, "attack_rad", 1))) net->attack_rad = f1(v, 1);
  if ((v = zrdr::find_flat(block, "attack_dwell", 1))) net->attack_dwell = f1(v, 1);
  if ((v = zrdr::find_flat(block, "pursuit_range", 1))) {
    const float a = f1(v, 1), b = f1(v, 2);
    net->pursuit_near = a, net->pursuit_far = b;
    if (b < net->pursuit_near) net->pursuit_near = b, net->pursuit_far = a;
  }
  if ((v = zrdr::find_flat(block, "not_pursuit_dwell", 1))) net->not_pursuit_dwell = f1(v, 1);
  if ((v = zrdr::find_flat(block, "return_range", 1))) net->return_range = f1(v, 1);
  Net105Node* last = nullptr;
  for (int32_t k = 0; k < 99; ++k) {
    char key[16];
    std::snprintf(key, sizeof key, "node_%02d", k);
    zrdr::Node* r = zrdr::find_flat(block, key, 1);
    if (!r) continue;
    auto* n = new Net105Node;  // 0x004ae8e0
    (last ? last->next : net->first) = n;
    last = n;
    n->number = k;
    n->x28 = r->list[1].i;
    zrdr::Node* p = &r->list[2];
    n->place[0] = p->list[1].f, n->place[1] = p->list[2].f, n->place[2] = p->list[3].f;
    zrdr::Node* l = &r->list[3];
    const int32_t c = l->list[0].i;
    for (int32_t i = 0; i < 3; ++i)  // the file's numbers, -1 past the list's end; 0x00402300 makes them nodes
      n->links[i] = reinterpret_cast<Net105Node*>(static_cast<intptr_t>(c < i + 2 ? -1 : l->list[i + 1].i));
    if (net->elems.size() < size_t(k) + 1) net->elems.resize(size_t(k) + 1, Net105Node::none());
    net->elems[size_t(k)] = n;
  }
  resolve(net->first, net->path_width);
  return net;
}

Net105* nets105() { return g_head; }

// Mech3.exe 1.2 @ 0x004021c0: the net made (in net105_read) and linked: the first the head, each after it the
// tail's +0x58.
// Mech3.exe 1.2 @ 0x00402190: its fields cleared (the class's member defaults).
void nets105_append(Net105* n) {
  if (g_head) g_tail->next = n;
  else g_head = n;
  g_tail = n;
}

// Mech3.exe 1.2 @ 0x00402560's walk over [0x005f4c4c].
void nets105_free() {
  for (Net105* n = g_head; n;) {
    Net105* next = n->next;
    delete n;
    n = next;
  }
  g_head = g_tail = nullptr;
}

}  // namespace mech3::ai
