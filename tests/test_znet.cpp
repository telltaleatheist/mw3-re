// zNetwork's session over the in-process hub (docs/multiplayer-port.md,
// piece 1): hosting and joining, the checks, sends to all and to one, the
// system events, the shutdown.
#include <cstring>
#include <vector>

#include "check.h"
#include "gamez/znetwork/znet_session.h"

namespace {
uint32_t word(const std::vector<uint8_t>& d, size_t at) {
  uint32_t v = 0;
  if (d.size() >= at + 4) std::memcpy(&v, d.data() + at, 4);
  return v;
}
}  // namespace

TEST(znet_session_checks_without_a_session) {
  znet::session() = znet::Session{};
  EXPECT(!znet::active() && znet::local_id() == 0 && !znet::is_host());
  const uint8_t m[4] = {0x1f, 0, 4, 0};
  EXPECT(znet::send(m, 4, true, 0) == 0);  // no local player: nothing sent (the exe reads through null)
  znet::shutdown();                         // not initialised: nothing to do
  EXPECT(!znet::session().initialised);
}

TEST(znet_session_host_join_send_and_leave) {
  znet::Hub hub;
  znet::Transport* a = hub.endpoint();
  znet::Transport* b = hub.endpoint();
  // The host, through zNetwork's globals.
  znet::session() = znet::Session{};
  znet::initialise(a);
  EXPECT(znet::session().initialised && !znet::active());  // up, no session yet
  znet::SessionDesc desc;
  desc.name = "test";
  EXPECT(znet::host_session(desc));
  EXPECT(znet::active() && znet::local_id() == 1 && znet::is_host());
  EXPECT(!znet::host_session(desc));  // one session at a time
  // A second machine joins through its own transport (the hub, not zNetwork:
  // one process holds one zNetwork).
  EXPECT(b->join("loopback") == 2);
  znet::Packet p;
  EXPECT(a->receive(&p) && p.from == 0 && word(p.data, 0) == znet::kSysCreatePlayerOrGroup && word(p.data, 4) == 2);
  EXPECT(!a->receive(&p));
  // To all from the host (async: its send's id, and the send-done event).
  const uint8_t m[8] = {0x14, 0, 8, 0, 9, 9, 9, 9};
  const uint32_t id = znet::send(m, 8, true, 0);
  EXPECT(id != 0);
  EXPECT(a->receive(&p) && p.from == 0 && word(p.data, 0) == znet::kSysSendComplete && word(p.data, 4) == id);
  EXPECT(b->receive(&p) && p.from == 1 && p.to == 0 && p.data.size() == 8 && p.data[0] == 0x14);
  // To one: only that one gets it; the sender never hears its own.
  EXPECT(b->send(1, m, 4, false, nullptr));
  EXPECT(a->receive(&p) && p.from == 2 && p.data.size() == 4);
  EXPECT(!b->receive(&p));
  // b leaves: the host hears it.
  b->close();
  EXPECT(a->receive(&p) && p.from == 0 && word(p.data, 0) == znet::kSysDestroyPlayerOrGroup && word(p.data, 4) == 2);
  // The host's shutdown clears zNetwork.
  znet::shutdown();
  const znet::Session& s = znet::session();
  EXPECT(!s.initialised && !s.open && !s.local && s.host_id == 0 && s.transport == nullptr);
  EXPECT(!znet::active() && znet::local_id() == 0);
}

TEST(znet_session_join_needs_a_host) {
  znet::Hub hub;
  znet::session() = znet::Session{};
  znet::initialise(hub.endpoint());
  EXPECT(!znet::join_session("loopback") && !znet::active());  // nobody hosts yet
  znet::Transport* h = hub.endpoint();
  znet::SessionDesc d;
  EXPECT(h->host(d) == 1);
  EXPECT(znet::join_session("loopback") && znet::active() && znet::local_id() == 2 && !znet::is_host());
  znet::shutdown();
}

// Piece 2: the dispatch's own cases the exe comparison leaves out (a handler
// removed while a dispatch runs, which read a deleted node in the exe; one
// added meanwhile, which the exe's walk reaches), and the poll over the hub.
namespace {
std::vector<int> g_calls;
void h_a(uint32_t, const uint8_t*, size_t) { g_calls.push_back(1); }
void h_b(uint32_t, const uint8_t*, size_t) { g_calls.push_back(2); }
void h_removes_a(uint32_t, const uint8_t*, size_t) {
  g_calls.push_back(3);
  znet::off(0x1f, &h_a);
}
void h_adds_b(uint32_t, const uint8_t*, size_t) {
  g_calls.push_back(4);
  znet::on(0x1f, &h_b);
}
uint32_t g_from = 0;
size_t g_size = 0;
void h_keep(uint32_t from, const uint8_t*, size_t size) { g_from = from, g_size = size; }
int g_sys = 0;
}  // namespace

TEST(znet_dispatch_while_handlers_change) {
  znet::session() = znet::Session{};
  const uint8_t m[4] = {0x1f, 0, 4, 0};
  g_calls.clear();
  znet::on(0x1f, &h_removes_a);
  znet::on(0x1f, &h_a);  // removed by the first before its turn: not called
  znet::on(0x1f, &h_adds_b);
  znet::dispatch(9, m, 4);
  EXPECT((g_calls == std::vector<int>{3, 4, 2}));  // b, added meanwhile at the end, is reached
  EXPECT(znet::session().handlers.size() == 3);  // a erased after the dispatch
  g_calls.clear();
  znet::dispatch(9, m, 4);
  EXPECT((g_calls == std::vector<int>{3, 4, 2, 2}));  // a second b added this time
  znet::clear_handlers();
  EXPECT(znet::session().handlers.empty());
}

TEST(znet_poll_over_the_hub) {
  znet::Hub hub;
  znet::session() = znet::Session{};
  znet::initialise(hub.endpoint());
  znet::SessionDesc d;
  EXPECT(znet::host_session(d));
  EXPECT(znet::poll(-1) == 0);  // nothing waiting
  znet::Transport* b = hub.endpoint();
  EXPECT(b->join("loopback") == 2);
  znet::session().system_message = [](const znet::Packet&) { ++g_sys; };
  znet::on(0x1f, &h_keep);
  const uint8_t chat[12] = {0x1f, 0, 12, 0, 0, 0, 0, 0, 'h', 'i', 0, 0};
  const uint8_t tiny[3] = {0x1f, 0, 3};
  EXPECT(b->send(0, chat, 12, true, nullptr) && b->send(0, tiny, 3, true, nullptr) && b->send(1, chat, 8, false, nullptr));
  g_sys = 0, g_from = 0, g_size = 0;
  EXPECT(znet::poll(2) == 2);  // the join's event, then the chat; the rest left for the next poll
  EXPECT(g_sys == 1 && g_from == 2 && g_size == 12);
  EXPECT(znet::poll(-1) == 1);  // the 3-byte one dropped, the 8-byte one handled
  EXPECT(g_size == 8);
  znet::shutdown();  // the handlers cleared with it
  EXPECT(znet::session().handlers.empty() && !znet::active());
  EXPECT(znet::poll(-1) == 0);
}
