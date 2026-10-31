// zNetwork's session checks and its send (multiplayer-port.md piece 1) for
// tests/differential/test_znet_session.py.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gamez/znetwork/znet_session.h"

namespace {
// A transport that keeps what the send was asked; `result` 0 sent, 1 sent
// pending (DPERR_PENDING), 2 failed.
struct Recorder : znet::Transport {
  uint32_t to = 0, size = 0, asked_id = 0, guaranteed = 0, first = 0, calls = 0;
  int result = 0;
  uint32_t host(const znet::SessionDesc&) override { return 0; }
  uint32_t join(const std::string&) override { return 0; }
  void close() override {}
  bool send(uint32_t t, const void* d, size_t n, bool g, uint32_t* id) override {
    ++calls, to = t, size = static_cast<uint32_t>(n), guaranteed = g, asked_id = id != nullptr;
    first = 0;
    std::memcpy(&first, d, n < 4 ? n : 4);
    if (id) *id = 77;
    return result != 2;
  }
  bool receive(znet::Packet*) override { return false; }
};
}  // namespace

extern "C" void probe_znet_state(int init, int open, uint32_t lobby, int has_local, uint32_t local_id, uint32_t host_id,
                                 int out[3]) {
  znet::Session& s = znet::session();
  s.initialised = init != 0, s.open = open != 0, s.lobby = lobby, s.host_id = host_id;
  s.local.reset();
  if (has_local) s.local = std::make_unique<znet::Player>(), s.local->id = local_id;
  out[0] = znet::active(), out[1] = static_cast<int>(znet::local_id()), out[2] = znet::is_host();
  s = znet::Session{};
}

// out: the answer, then the transport's calls, to, size, guaranteed, the id asked for, the first word.
extern "C" void probe_znet_send(int has_local, uint32_t local_id, int async, int guaranteed, uint32_t to, uint32_t size,
                                int result, uint32_t out[7]) {
  Recorder r;
  r.result = result;
  znet::Session& s = znet::session();
  s = znet::Session{};
  s.transport = &r, s.initialised = true, s.async = async != 0;
  if (has_local) s.local = std::make_unique<znet::Player>(), s.local->id = local_id;
  const uint8_t msg[8] = {0x0f, 0x00, static_cast<uint8_t>(size), 0x00, 1, 2, 3, 4};
  out[0] = znet::send(msg, size, guaranteed != 0, to);
  out[1] = r.calls, out[2] = r.to, out[3] = r.size, out[4] = r.guaranteed, out[5] = r.asked_id, out[6] = r.first;
  s = znet::Session{};
}

// The handlers and the dispatch (piece 2): ops of three words, op 0 on(type,
// handler), 1 off(type, handler), 2 clear, 3 dispatch(type, sender); each
// handler call logged as (handler, sender, type). Handler 7 removes handler
// 0 of type 5 while it runs, to show a removal during a dispatch.
namespace {
int32_t* g_log = nullptr;
int32_t g_logged = 0, g_cap = 0;
template <int N>
void logged(uint32_t from, const uint8_t* msg, size_t) {
  if (g_logged + 3 > g_cap) return;
  uint16_t type;
  std::memcpy(&type, msg, 2);
  g_log[g_logged++] = N, g_log[g_logged++] = static_cast<int32_t>(from), g_log[g_logged++] = type;
  if (N == 7) znet::off(5, &logged<0>);
}
const znet::Handler kHandlers[8] = {&logged<0>, &logged<1>, &logged<2>, &logged<3>,
                                    &logged<4>, &logged<5>, &logged<6>, &logged<7>};
}  // namespace

extern "C" int32_t probe_znet_handlers(const int32_t* ops, int32_t n, int32_t* log, int32_t cap) {
  znet::session() = znet::Session{};
  g_log = log, g_logged = 0, g_cap = cap;
  for (int32_t i = 0; i < n; ++i) {
    const int32_t op = ops[3 * i], a = ops[3 * i + 1], b = ops[3 * i + 2];
    if (op == 0) znet::on(static_cast<uint16_t>(a), kHandlers[b & 7]);
    else if (op == 1 && std::getenv("MW3_PROBE_BREAK") && std::string(std::getenv("MW3_PROBE_BREAK")) == "1") {
      auto& hs = znet::session().handlers;  // the break: only the first match removed
      for (auto it = hs.begin(); it != hs.end(); ++it)
        if (it->type == a && it->fn == kHandlers[b & 7]) {
          hs.erase(it);
          break;
        }
    } else if (op == 1) znet::off(static_cast<uint16_t>(a), kHandlers[b & 7]);
    else if (op == 2) znet::clear_handlers();
    else {
      const uint8_t msg[4] = {static_cast<uint8_t>(a), static_cast<uint8_t>(a >> 8), 4, 0};
      znet::dispatch(static_cast<uint32_t>(b), msg, 4);
    }
  }
  znet::session() = znet::Session{};
  return g_logged;
}

// The poll (piece 2): a queue of messages (sender, size) through a scripted
// transport; handler 1 on type 0x1f, system ones counted. out: the answer,
// the system ones seen, then the log as above.
namespace {
struct Script : znet::Transport {
  const int32_t* q = nullptr;
  int32_t n = 0, at = 0;
  uint32_t host(const znet::SessionDesc&) override { return 0; }
  uint32_t join(const std::string&) override { return 0; }
  void close() override {}
  bool send(uint32_t, const void*, size_t, bool, uint32_t*) override { return true; }
  bool receive(znet::Packet* p) override {
    if (at >= n) return false;
    p->from = static_cast<uint32_t>(q[2 * at]);
    p->data.assign(static_cast<size_t>(q[2 * at + 1]), 0);
    if (p->data.size() >= 2) p->data[0] = 0x1f;
    ++at;
    return true;
  }
};
int g_system = 0;
}  // namespace

extern "C" int32_t probe_znet_poll(int32_t max, const int32_t* queue, int32_t n, int32_t* out, int32_t cap) {
  Script t;
  t.q = queue, t.n = n;
  znet::session() = znet::Session{};
  znet::Session& s = znet::session();
  s.transport = &t, s.initialised = true, s.open = true;
  s.system_message = [](const znet::Packet&) { ++g_system; };
  g_system = 0, g_log = out + 2, g_logged = 0, g_cap = cap - 2;
  znet::on(0x1f, kHandlers[1]);
  out[0] = znet::poll(max);
  out[1] = g_system;
  const int32_t logged_n = g_logged;
  znet::session() = znet::Session{};
  return logged_n + 2;
}

// The system events' translation (piece 3): events of six words (kind,
// player type, id, team, name number, the id's DPCAPS_ISHOST) applied in
// order with the local player `local_id`; the log: (1, type, size, sender)
// for each local message, (2, to, guaranteed, size, type) for each send,
// then (3, count) and each player (id, is_host, team, short and long names'
// numbers), (4, count) and each team (id, name number).
#include "gamez/znetwork/znet_dplay.h"
namespace {
struct Caps : znet::Transport {
  std::vector<uint32_t> hosts;
  std::vector<int32_t>* log = nullptr;
  uint32_t host(const znet::SessionDesc&) override { return 0; }
  uint32_t join(const std::string&) override { return 0; }
  void close() override {}
  bool send(uint32_t to, const void* d, size_t n, bool g, uint32_t*) override {
    uint16_t type = 0;
    std::memcpy(&type, d, 2);
    log->insert(log->end(), {2, static_cast<int32_t>(to), g ? 1 : 0, static_cast<int32_t>(n), type});
    return true;
  }
  bool receive(znet::Packet*) override { return false; }
  bool is_host(uint32_t id) const override {
    for (uint32_t h : hosts)
      if (h == id) return true;
    return false;
  }
};
std::vector<int32_t>* g_events_log = nullptr;
void any(uint32_t from, const uint8_t* msg, size_t size) {
  uint16_t type, sz;
  std::memcpy(&type, msg, 2);
  std::memcpy(&sz, msg + 2, 2);
  (void)size;
  g_events_log->insert(g_events_log->end(), {1, type, sz, static_cast<int32_t>(from)});
}
int32_t number(const std::string& s) { return std::atoi(s.c_str() + (s.rfind("Player ", 0) == 0 ? 7 : 1)); }
}  // namespace

extern "C" int32_t probe_znet_events(const int32_t* ev, int32_t n, uint32_t local_id, uint32_t host_id, int32_t* out,
                                     int32_t cap) {
  std::vector<int32_t> log;
  Caps t;
  t.log = &log;
  for (int32_t i = 0; i < n; ++i)
    if (ev[6 * i + 5]) t.hosts.push_back(static_cast<uint32_t>(ev[6 * i + 2]));
  znet::session() = znet::Session{};
  znet::clear_lists();
  znet::Session& s = znet::session();
  s.transport = &t, s.initialised = true, s.open = true, s.host_id = host_id;
  s.local = std::make_unique<znet::Player>();
  s.local->id = local_id;
  g_events_log = &log;
  for (uint16_t ty = 2; ty <= 0xe; ++ty) znet::on(ty, &any);
  for (int32_t i = 0; i < n; ++i) {
    znet::Packet p;
    znet::SystemEvent& e = p.sys;
    e.kind = static_cast<uint32_t>(ev[6 * i]), e.player_type = static_cast<uint32_t>(ev[6 * i + 1]);
    e.id = static_cast<uint32_t>(ev[6 * i + 2]), e.group = static_cast<uint32_t>(ev[6 * i + 3]);
    e.short_name = "P" + std::to_string(ev[6 * i + 4]);
    e.long_name = "Player " + std::to_string(ev[6 * i + 4]);
    znet::system_message(p);
  }
  log.insert(log.end(), {3, static_cast<int32_t>(znet::lists().players.size())});
  for (auto& r : znet::lists().players)
    log.insert(log.end(), {static_cast<int32_t>(r->id), r->is_host, static_cast<int32_t>(r->team), number(r->short_name),
                           number(r->long_name)});
  log.insert(log.end(), {4, static_cast<int32_t>(znet::lists().teams.size())});
  for (auto& tm : znet::lists().teams) log.insert(log.end(), {static_cast<int32_t>(tm->id), number(tm->name)});
  log.push_back(s.local ? s.local->is_host : -1);
  znet::clear_lists();
  znet::session() = znet::Session{};
  const int32_t m = static_cast<int32_t>(std::min<size_t>(log.size(), static_cast<size_t>(cap)));
  std::copy(log.begin(), log.begin() + m, out);
  return m;
}
