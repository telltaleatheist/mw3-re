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
