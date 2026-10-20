#include "gamez/zeffect/zeff_anim_save.h"

#include <cstring>

#include "gamez/zeffect/zeff_anim_init.h"

namespace zeff {

namespace {
AnimLog g_log;

uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

// Whether a call is kept (savegame) and sent (network): a def's own setting
// (flags 0x1000 / 0x400, with 0x2000 / 0x800 the value) wins over the
// defaults; otherwise a def added to the world when started (0x100), a
// nameless one, or one at a node outside the pool is never logged.
struct Want {
  uint32_t save, net;
};
Want want(const AnimDef* def, bool node_ok, bool check_defaults) {
  const uint32_t f = def->get<uint32_t>(0x94);
  const bool base = (f & 0x100) == 0 && def->raw[0] != 0 && node_ok;
  Want w;
  w.save = (f & 0x1000) ? (f >> 13) & 1 : (base && (!check_defaults || g_log.save_on)) ? 1 : 0;
  w.net = (f & 0x400) ? (f >> 11) & 1 : (base && (!check_defaults || g_log.net_on)) ? 1 : 0;
  return w;
}

AnimLogRecord* begin(const AnimDef* def, int32_t type, int32_t node) {
  AnimLogRecord* r = log_new_record();
  r->type = type;
  std::strncpy(r->name, def->name(), 0x20);
  r->node = node;
  return r;
}

AnimLogRecord* finish(AnimLogRecord* r, Want w) {
  AnimLogRecord* sent = nullptr;
  if (w.net) {
    sent = r;
    if (g_log.net) g_log.net(r);
  }
  if (!w.save) log_drop_record();
  return sent;
}

void put_vec(uint32_t* out, const float* v) {
  for (int i = 0; i < 3; ++i) out[i] = v ? bits(v[i]) : 0;
}
}  // namespace

AnimLog& anim_log() { return g_log; }

// Mech3.exe 1.2 @ 0x004fe100
int32_t node_index(const zcls::Node* node) {
  if (!node) return -1;
  const zcls::NodePool& p = zcls::nodes();
  for (size_t b = 0; b < p.blocks.size(); ++b) {
    const zcls::Node* base = p.blocks[b].get();
    if (node >= base && node <= base + p.block_size - 1)
      return static_cast<int32_t>(b) * p.block_size + static_cast<int32_t>(node - base);
  }
  return -1;  // the exe returns a stray value here; every caller passes pool nodes
}

// Mech3.exe 1.2 @ 0x004fe180
int32_t live_node_index(const zcls::Node* node) {
  const int32_t i = node_index(node);
  if (i >= 0 && (zcls::node_at(i)->link & zcls::kNodeLive)) return i;
  return -1;
}

// Mech3.exe 1.2 @ 0x0050bd50. 1000 records at first, doubled when full.
AnimLogRecord* log_new_record() {
  AnimLog& l = g_log;
  if (l.records.empty()) {
    l.records.resize(1000);
    l.capacity = 1000;
    l.count = 0;
  }
  if (l.capacity <= l.count) {
    l.records.resize(static_cast<size_t>(l.capacity) * 2);
    l.capacity *= 2;
  }
  AnimLogRecord& r = l.records[l.count];
  r.serial = l.mask & static_cast<uint32_t>(l.count) & 0xffffff;
  ++l.count;
  return &r;
}

// Mech3.exe 1.2 @ 0x0050ccd0
void log_drop_record() { --g_log.count; }

// Mech3.exe 1.2 @ 0x0050cbb0
AnimLogRecord* log_start_at(AnimDef* def, zcls::Node* node, const float pos[3], const float rot[3],
                            const float vec[3]) {
  const int32_t n = live_node_index(node);
  const Want w = want(def, !node || n >= 1, true);
  if (!w.save && !w.net) return nullptr;
  AnimLogRecord* r = begin(def, 1, n);
  put_vec(r->args, pos), put_vec(r->args + 3, rot), put_vec(r->args + 6, vec);
  return finish(r, w);
}

// Mech3.exe 1.2 @ 0x0050cce0
AnimLogRecord* log_start(AnimDef* def, zcls::Node* node, float x, float y, float z) {
  const int32_t n = live_node_index(node);
  const Want w = want(def, !node || n >= 1, true);
  if (!w.save && !w.net) return nullptr;
  AnimLogRecord* r = begin(def, 2, n);
  r->args[0] = bits(x), r->args[1] = bits(y), r->args[2] = bits(z);
  return finish(r, w);
}

// Mech3.exe 1.2 @ 0x0050cde0. The only logger that ignores the defaults.
AnimLogRecord* log_connector(AnimDef* def, zcls::Node* node, zcls::Node* from, const float* from_pos,
                             const float* vec) {
  const int32_t n = live_node_index(node), m = live_node_index(from);
  const Want w = want(def, (!node || n >= 1) && (!from || m >= 1), false);
  if (!w.save && !w.net) return nullptr;
  AnimLogRecord* r = begin(def, 3, n);
  r->args[0] = static_cast<uint32_t>(m);
  put_vec(r->args + 1, from_pos), put_vec(r->args + 4, vec);
  return finish(r, w);
}

// Mech3.exe 1.2 @ 0x0050cf40
AnimLogRecord* log_connector_at(AnimDef* def, zcls::Node* node, zcls::Node* from, const float* from_pos,
                                zcls::Node* to, const float* to_pos) {
  const int32_t n = live_node_index(node), m = live_node_index(from), k = live_node_index(to);
  const Want w = want(def, (!node || n >= 1) && (!from || m >= 1) && (!to || k >= 1), true);
  if (!w.save && !w.net) return nullptr;
  AnimLogRecord* r = begin(def, 4, n);
  r->args[0] = static_cast<uint32_t>(m);
  put_vec(r->args + 1, from_pos);
  r->args[4] = static_cast<uint32_t>(k);
  put_vec(r->args + 5, to_pos);
  return finish(r, w);
}

// Mech3.exe 1.2 @ 0x0050d0f0
AnimLogRecord* log_reset(AnimDef* def, zcls::Node* node) {
  const int32_t n = live_node_index(node);
  const Want w = want(def, !node || n >= 1, true);
  if (!w.save && !w.net) return nullptr;
  return finish(begin(def, 5, n), w);
}

}  // namespace zeff
