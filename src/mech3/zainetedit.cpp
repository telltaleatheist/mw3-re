#include "mech3/zainetedit.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "gamez/zerror/zerr.h"
#include "mech3/zai_steps.h"
#include "mech3/mech_waynet.h"
#include "mech3/zainet_region.h"

namespace mech3::ai {
namespace {

constexpr const char* kFile = "C:\\zipper\\Mech3\\ZAiNetEdit.cpp";

void report(int level, int line, const char* fmt, ...) {
  char msg[512];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  zerr::report(level, kFile, line, msg);
}

}  // namespace

// Mech3.exe 1.2 @ 0x004bcba0. The builder's own id comes from 400..499 (one
// building a region asks for 400 too); a net already registered under `net`
// (built earlier) is taken as it is and nothing is rebuilt.
CZAiNetEdit::CZAiNetEdit(int32_t net_no, const char* name_in, int32_t build)
    : AiNet(znet::alloc_id(build ? 400 : net_no, 400, 500)) {
  if (znet::Object* o = znet::find(net_no)) {
    AiNet* n = static_cast<AiNet*>(o)->as_net();
    net = n == this ? nullptr : n;
  }
  header.net = (!net && !build) ? get_id() : net_no;
  const char* fname = net ? net->name() : nullptr;
  if (!fname) fname = name_in;
  placeholder = new NetNode(this);
  ref = new NetRef(header.net, fname);
  int32_t n = 0;
  if (ref->file) {
    header = *ref->header();
    n = ref->count();
    if (n > 0) {
      elems.assign(size_t(n), placeholder);
      for (int32_t i = 0; i < n; ++i) {
        const char* key = nullptr;
        NetNodeRec* r = ref->node(i, &key);
        if (!r || r->index < 0) continue;
        const size_t at = size_t(r->index);
        if (at >= elems.size()) elems.resize(at + 1, placeholder);
        if (elems[at] == placeholder)
          elems[at] = new NetNode(this, *r);
        else
          report(0x2000, 0xa1, "net %d: ignoring duplicate node %d", net_no, r->index);
      }
      link_pass();
    }
    for (znet::Element* e : elems)
      if (NetNode* nd = e ? static_cast<AiElem*>(e)->as_node() : nullptr) nd->arrange();
  }
  dirty = 0;
  if (!net && build == 1 && n > 0) {
    switch (header.version) {
      case 0x6a: net = build_compiled(); break;
      case 0x6b: net = build_waynet(); break;
      case 0x6c: net = build_region(); break;
      // 105 (0x004bed90) is not ported: no shipped file is one.
      default: break;
    }
  }
  editing = build == 0;
}

// Mech3.exe 1.2 @ 0x004bd020. The save on close (0x004bd100) is the editor's.
CZAiNetEdit::~CZAiNetEdit() {
  for (znet::Element*& e : elems) {
    if (e != placeholder) delete e;
    e = nullptr;
  }
  delete placeholder;
  delete ref;
}

// Mech3.exe 1.2 @ 0x004bef40
NetNode* CZAiNetEdit::node(int32_t i) const {
  if (i >= 0 && uint32_t(i) < elems.size()) {
    znet::Element* e = elems[size_t(i)];
    return e ? static_cast<AiElem*>(e)->as_node() : nullptr;
  }
  return placeholder;
}

// Mech3.exe 1.2 @ 0x004bf4b0
void CZAiNetEdit::drop_net() {
  delete net;
  net = nullptr;
}

// Mech3.exe 1.2 @ 0x004bf000. A region's net id is from 10000 and its file
// version 108. The walk skips placeholders at the start (with a budget that
// shrinks as it goes, so the first real node must sit in the first half),
// then follows each node's first link until it comes back.
bool CZAiNetEdit::loop(NetLoop* out) {
  int32_t budget = count();
  if (header.net < znet::kKeptIds || budget <= 1 || header.version != 0x6c) return false;
  int32_t link = -1;
  out->count = 0, out->first = 0;
  NetNode* nd = nullptr;
  for (;;) {
    znet::Element* e = elems[size_t(out->first)];
    nd = e ? static_cast<AiElem*>(e)->as_node() : nullptr;
    if (nd != placeholder) break;
    ++out->first, --budget;
    if (out->first >= budget) break;
  }
  int32_t prev = out->first;
  if (out->first < budget) {
    if (budget > 0) {
      while (nd && nd->link_count() > 0) {
        link = nd->rec.links[0];
        if (link == out->first || link < 0) break;
        nd = uint32_t(link) < elems.size() && elems[size_t(link)]
                 ? static_cast<AiElem*>(elems[size_t(link)])->as_node()
                 : nullptr;
        if (!nd) report(0x800, 0x55d, "region \"%s\" node %d points to nonexistant node %d", name(), prev, link);
        ++out->count;
        prev = link;
        if (--budget <= 0) break;
      }
    }
    ++out->count;
  }
  return nd && out->count > 1 && link == out->first;
}

// Mech3.exe 1.2 @ 0x004bf1b0, with the region given (the only caller,
// 0x004bf3f0, gives one): one edge from each node of the loop to the next.
// The walk is unchecked; loop() has seen every node it reaches. The exe's
// failure path ("unable to create region", line 0x5a2) is an allocation
// failing.
Region* CZAiNetEdit::make_region(Region* r, const NetLoop& l) {
  if (net) drop_net();
  r->elems.resize(size_t(l.count), nullptr);
  r->cursor = 0;  // 0x004c57c0
  auto at = [this](int32_t i) { return static_cast<AiElem*>(elems[size_t(i)])->as_node(); };
  NetNode* nd = at(l.first);
  size_t i = 0;
  int32_t link;
  do {
    link = nd->rec.links[0];
    NetNode* next = at(link);
    r->elems[i++] = new RegionEdge(nd->pos(), next->pos());
    nd = next;
  } while (link != l.first);
  net = r;
  return r;
}

NetHooks& net_hooks() {
  static NetHooks h;
  return h;
}

namespace {

uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

// 0x004bd440 (vtable 0x005ca480): what compiling a net keeps between nodes.
// The slot counts (0x004bd670, 0x004bd4e0, 0x004bd5b0) come first, the
// steps (0x004be230, 0x004bde20, 0x004bd790, 0x004bdd90) after.
struct Compiler {
  CZAiNetEdit* edit;
  int32_t slots = 0;            // +0x08
  int32_t max_links = 0;        // +0x0c
  std::vector<int32_t> slot_of; // +0x14  a node's number -> its step's slot, -1 for none
  std::vector<int32_t> targets; // +0x24  the current node's links as slots
  CompiledNet* net = nullptr;   // +0x30
  NetNode* cur = nullptr;       // +0x34
  int32_t target = 0;           // +0x38  the node the current link goes to
  int32_t slot = 0;             // +0x40  the next slot to fill
  int32_t link = 0;             // +0x44
  int32_t links = 0;            // +0x48
  int32_t* way = nullptr;       // +0x4c  the word the next step's slot goes into
  int32_t lost = -1;            // FIX (bug): stands in for a way past the node's links
  std::vector<std::unique_ptr<Step>> lost_steps;

  explicit Compiler(CZAiNetEdit* e) : edit(e), slot_of(size_t(e->count()), -1) {}

  bool positioned(NetNode* n) const { return n && n != edit->placeholder && n->rec.kind == 0; }
  bool on_edges(NetNode* n) const { return n && n != edit->placeholder && n->rec.kind != 0; }
  int32_t slot_for(int32_t node) const {  // FIX (bug): the exe indexes unchecked
    return node >= 0 && size_t(node) < slot_of.size() ? slot_of[size_t(node)] : -1;
  }

  // Mech3.exe 1.2 @ 0x004bd5b0: one step per property the node has.
  static int32_t properties(const NetNodeRec& r) {
    int32_t n = (r.speed_on != 0) + (r.wait_interval_on != 0) + (r.wait_time_on != 0) + (r.valve_on != 0);
    n += (r.canleave[0] != 0) + (r.jump_on != 0) + (r.sound != nullptr);
    n += (r.scan_aim[0] != 0 || r.scan_hit[0] != 0 || r.scan_time[0] != 0);
    n += (r.gate != nullptr) + (r.shunt != 0) + (r.valve_union != nullptr) + (r.valve_assign != nullptr);
    return n;
  }

  // Mech3.exe 1.2 @ 0x004bd4e0: the steps an edge node `m` (one without a
  // position: links from, to...) adds to the current node, once for each of
  // its ends the current node links to.
  int32_t edge_count(NetNode* m) const {
    if (!on_edges(m)) return 0;
    const std::vector<int32_t>& l = m->rec.links;
    if (l.size() < 2 || l[0] != cur->rec.index) return 0;
    int32_t hits = 0;
    for (size_t k = 1; k < l.size(); ++k)
      for (int32_t t : cur->rec.links)
        if (t == l[k]) {
          ++hits;
          break;
        }
    return hits > 0 ? properties(m->rec) * hits : 0;
  }

  // Mech3.exe 1.2 @ 0x004bd670: a positioned node's slot, then room for its
  // properties on each link and the edge nodes' on theirs.
  void count(NetNode* n) {
    cur = n;
    if (!positioned(n)) return;
    slot_of[size_t(n->rec.index)] = slots;
    const int32_t nl = n->link_count();
    if (max_links < nl) max_links = nl;
    slots += 1 + properties(n->rec) * nl;
    int32_t extra = 0;
    for (znet::Element* e : edit->elems) extra += edge_count(e ? static_cast<AiElem*>(e)->as_node() : nullptr);
    slots += extra;
  }

  // One step into the next slot, threaded into the current link's chain.
  void add(Step* s) {
    s->next_slot = *way, s->slot_set = true;
    *way = slot;
    way = &s->next_slot;
    // FIX (bug): a step past the counted slots (a node linking to one node
    // twice with an edge node on that link) is written past the vector in
    // the exe. Here it is kept aside and dropped: the net is what the exe's
    // ends up as, the chain through it broken (attach finds no step there).
    if (size_t(slot) < net->elems.size())
      net->elems[size_t(slot)] = s;
    else
      lost_steps.emplace_back(s);
    ++slot;
  }

  // Mech3.exe 1.2 @ 0x004bd790: node `n`'s properties as steps on the current
  // link, in this order.
  void emit(NetNode* n) {
    const NetNodeRec& r = n->rec;
    if (r.speed_on) add(new ValueStep(StepKind::Speed, bits(r.speed)));
    if (r.wait_interval_on) add(new ValueStep(StepKind::WaitInterval, bits(r.wait_interval)));
    if (r.wait_time_on) add(new ValueStep(StepKind::WaitTime, bits(r.wait_time)));
    // A valve without a value is set to 1 on the node's last link, 0 on the others.
    if (r.valve_on) add(new ValveStep(r.valve, r.valve_has_value ? r.valve_value : int32_t(link == links - 1)));
    if (r.canleave[0]) {
      const bool on = r.canleave[1] != 0;
      auto* s = new ScanStep(on ? StepKind::CanLeave : StepKind::CanLeaveNot, 1);  // 0x004cbcf0 / 0x004cbd40
      s->tests.emplace_back(on ? ScanTest::CanLeave : ScanTest::CanLeaveNot, 0);
      add(s);
    }
    if (r.scan_aim[0] || r.scan_hit[0] || r.scan_time[0]) {
      auto* s = new ScanStep(StepKind::Scan, (r.scan_aim[0] != 0) + (r.scan_hit[0] != 0) + (r.scan_time[0] != 0));
      if (r.scan_aim[0]) s->tests.emplace_back(r.scan_aim[1] ? ScanTest::AimOn : ScanTest::AimOff, 0);
      if (r.scan_hit[0]) s->tests.emplace_back(r.scan_hit[1] ? ScanTest::HitOn : ScanTest::HitOff, 0);
      if (r.scan_time[0]) s->tests.emplace_back(r.scan_time[1] ? ScanTest::TimeOn : ScanTest::TimeOff, 0);
      add(s);
    }
    if (r.jump_on) add(new ValueStep(StepKind::Jump, bits(r.jump)));
    if (r.sound) add(new SoundStep(r.sound));
    if (r.gate) add(new GateStep(r.gate));
    if (r.shunt) add(new Step(StepKind::Shunt));
    if (r.valve_union) add(new UnionStep(*r.valve_union));
    if (r.valve_assign) add(new AssignStep(*r.valve_assign));
  }

  // Mech3.exe 1.2 @ 0x004bdd90 (-> 0x004bdd70): an edge node from the current
  // node puts its properties on each of its ends the current link goes to.
  void edge(NetNode* m) {
    if (!on_edges(m)) return;
    const std::vector<int32_t>& l = m->rec.links;
    if (l.size() < 2 || l[0] != cur->rec.index) return;
    for (size_t k = 1; k < l.size(); ++k)
      if (l[k] == target) emit(m);
  }

  // Mech3.exe 1.2 @ 0x004bde20: a positioned node's step, then each link's
  // chain of property steps leading on to the node it links to.
  void compile(NetNode* n) {
    cur = n;
    if (!positioned(n)) return;
    const NetNodeRec& r = n->rec;
    slot = slot_of[size_t(r.index)];
    links = n->link_count();
    float p[3] = {r.pos[0], r.pos[1], r.pos[2]};
    if (net_hooks().ground) net_hooks().ground(p);
    for (int32_t k = 0; k < links; ++k) targets[size_t(k)] = slot_for(r.links[size_t(k)]);
    NodeStep* s;
    if (r.deploy_set)
      s = new DeployNode(p, targets.data(), links, r.auxname, r.deploy_valve_a, r.deploy_a, r.deploy_valve_b,
                         r.deploy_b, slot_for(int32_t(r.deploy_c)));
    else if (r.mcbwait_set)
      s = new McbWaitNode(p, targets.data(), links, r.auxname, r.mcbwait_valve, r.mcbwait_b, r.mcbwait_time);
    else if (r.backup)
      s = new NodeStep(StepKind::BackupNode, p, targets.data(), links);
    else {
      s = new NodeStep(StepKind::Node, p, targets.data(), links);
      s->set_name(r.auxname);
    }
    if (r.arrange_set && net_hooks().waynet_point) net_hooks().waynet_point(r, p);
    net->elems[size_t(slot)] = s;
    ++slot;
    for (link = 0; link < links; ++link) {
      way = size_t(link) < s->slots.size() ? &s->slots[size_t(link)] : &(lost = -1);
      target = r.links[size_t(link)];
      emit(n);
      for (znet::Element* e : edit->elems) edge(e ? static_cast<AiElem*>(e)->as_node() : nullptr);
    }
  }
};

}  // namespace

// Mech3.exe 1.2 @ 0x004be670 -> 0x004be300 (no net given): a version-106 net
// compiled into steps (ai.md section 9). A net built before is dropped first;
// the vehicles following it are handed the new one (NetHooks::rebuilt).
AiNet* CZAiNetEdit::build_compiled() {
  if (header.version != 0x6a) return nullptr;
  AiNet* old = net;
  if (net) {
    if (net_hooks().rebuilt) net_hooks().rebuilt(old, nullptr);
    drop_net();
  }
  Compiler c(this);
  for (znet::Element* e : elems) c.count(e ? static_cast<AiElem*>(e)->as_node() : nullptr);  // 0x004bd740
  auto* cn = new CompiledNet(header.net, c.slots);
  cn->elems.assign(size_t(c.slots), null_step());
  net = cn;
  // 0x004be230: the net checked to be a compiled one (0x004bd640).
  c.net = cn;
  c.targets.assign(size_t(c.max_links), 0);
  for (znet::Element* e : elems) c.compile(e ? static_cast<AiElem*>(e)->as_node() : nullptr);
  for (size_t i = 0; i < cn->elems.size(); ++i) static_cast<AiElem*>(cn->elems[i])->as_behaviour()->attach(cn);
  if (net_hooks().rebuilt) net_hooks().rebuilt(old, cn);
  return cn;
}

// Mech3.exe 1.2 @ 0x004bf3f0
Region* CZAiNetEdit::build_region() {
  NetLoop l;
  if (!loop(&l)) return nullptr;
  if (net) drop_net();
  auto* r = new Region(name(), header.net, l.count);
  Region* made = make_region(r, l);
  Region* got = made ? made->as_region() : nullptr;
  if (got != r) delete r;
  return got;
}

// Mech3.exe 1.2 @ 0x004be690: a version-107 file, a mission's nav points
// (waynet_N.zrd), into a waynet (mech_waynet). Its slots counted as a
// compiled net's are (0x004beb00 / 0x004bec10), but none per link (the
// counter's vtable 0x005ca48c answers 0 where 0x004beb90 counts properties):
// a positioned node one, an edge node starting at it its links but the
// first; all null steps, then a point for
// each node in order from slot 0 (the placeholders left out), its links as
// slots. FIX (bug): a link past the nodes, an edge node's links past the
// buffer, and a point past the counted slots (more nodes than slots: edge
// nodes) are read or written past the exe's vectors; here the link is -1,
// the buffer holds them, the point is dropped.
AiNet* CZAiNetEdit::build_waynet() {
  auto* w = new Waynet(header.net, header.name);  // 0x00479640 (new 0x30)
  AiNet* old = net;
  if (old) {
    if (net_hooks().rebuilt) net_hooks().rebuilt(old, nullptr);  // 0x004be5a0 over the vehicles
    drop_net();
  }
  auto node_of = [this](znet::Element* e) -> NetNode* {
    NetNode* n = e ? static_cast<AiElem*>(e)->as_node() : nullptr;  // vt+0x60, vt+0x88
    return n == placeholder ? nullptr : n;
  };
  std::vector<int32_t> slot_of(elems.size(), -1);
  int32_t slots = 0, max_links = 0;
  for (size_t i = 0; i < elems.size(); ++i) {  // 0x004bec10
    NetNode* n = node_of(elems[i]);
    if (!n || n->rec.kind != 0) continue;  // vt+0x14: positioned
    slot_of[i] = slots;
    const int32_t nl = n->link_count();
    if (max_links < nl) max_links = nl;
    slots += 1;  // the counter's per-link steps: its vtable's 0x00410510, none
    int32_t extra = 0;
    for (znet::Element* e : elems) {
      NetNode* m = node_of(e);
      if (!m || m->rec.kind == 0) continue;
      const int32_t k = int32_t(m->rec.links.size());
      if (k - 1 > 0 && m->rec.links[0] == n->rec.index) extra += k - 1;
    }
    slots += extra;
  }
  net = w;
  w->elems.assign(size_t(slots), null_step());
  // The links' buffer: the exe's holds the positioned nodes' most; an edge
  // node with more is written past it (and read back): here it holds any.
  size_t most = size_t(max_links > 0 ? max_links : 0);
  for (znet::Element* e : elems)
    if (NetNode* n = node_of(e)) most = std::max(most, n->rec.links.size());
  std::vector<int32_t> links(most + 1, 0);
  size_t at = 0;
  for (znet::Element* e : elems) {
    NetNode* n = node_of(e);
    if (!n) continue;
    const NetNodeRec& r = n->rec;
    for (size_t k = 0; k < r.links.size(); ++k) {
      const int32_t to = r.links[k];
      links[k] = to >= 0 && size_t(to) < slot_of.size() ? slot_of[size_t(to)] : -1;
    }
    float p[3] = {r.pos[0], r.pos[1], r.pos[2]};  // vt+0x18
    if (net_hooks().ground) net_hooks().ground(p);  // 0x004edab0 / 0x004abd90 while a world is loaded
    auto* pt = new WaynetPoint(r.name, r.auxname, p, links.data(), n->link_count(), 0, r.visible, r.fixed,
                               r.autonext, r.hidden);  // 0x0047a320 (new 0x38)
    if (at < w->elems.size()) w->elems[at] = pt;
    else delete pt;
    ++at;
  }
  // Each element's vt+0x6c with the waynet: nothing for points and null steps (0x004a5bc0).
  if (net_hooks().rebuilt) net_hooks().rebuilt(old, w);
  return w;
}

// Mech3.exe 1.2 @ 0x00402240 (in the exe's first, unnamed module)
AiNet* net_build(int32_t n) {
  CZAiNetEdit edit(n, nullptr, 1);
  return edit.as_compiled();
}

// Mech3.exe 1.2 @ 0x00402170
void nets_build_all() {
  for (int32_t n = 1; n < 100; ++n) net_build(n);
}

}  // namespace mech3::ai
