#include "mech3/zainetedit.h"

#include <cstdarg>
#include <cstdio>

#include "gamez/zerror/zerr.h"
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
      case 0x6c: net = build_region(); break;
      // 105 (0x004bed90) and 107 (0x004be690, the multiplayer waynets) are
      // not ported: no shipped single-player file has them.
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

// Mech3.exe 1.2 @ 0x004be670: a version-106 net compiled into the steps the
// pilots follow. Not ported yet (ai.md, porting order step 2): none.
AiNet* CZAiNetEdit::build_compiled() { return nullptr; }

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

}  // namespace mech3::ai
