#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// zNetwork's session (gamezmech3\znetwork\znet_session.cpp and znet_dplay.cpp,
// 0x00586c20..0x00589540; docs/multiplayer-port.md). The exe wrapped
// DirectPlay 4; the port keeps zNetwork's entry points and its globals and
// puts a Transport of ours under them (FIX (A): DirectPlay is gone).
namespace znet {

// What a transport hands up: a message from a player, or (from 0) one of
// DirectPlay's system events, which the poll turns into MW3 messages 2..0xe
// (network-messages.md, Local only).
struct Packet {
  uint32_t from = 0;  // the sender's player id; 0 a system event
  uint32_t to = 0;    // 0 every player (DPID_ALLPLAYERS), else one player or a team
  std::vector<uint8_t> data;
};

// A system event's payload (ours; from 0): its kind, then the player or
// team it is about. The kinds are DirectPlay's DPSYS_* numbers.
enum SystemKind : uint32_t {
  kSysCreatePlayerOrGroup = 0x0003,
  kSysDestroyPlayerOrGroup = 0x0005,
  kSysAddPlayerToGroup = 0x0007,
  kSysDeletePlayerFromGroup = 0x0021,
  kSysSessionLost = 0x0031,
  kSysHost = 0x0101,
  kSysSetPlayerOrGroupData = 0x0102,
  kSysSetPlayerOrGroupName = 0x0103,
  kSysSetSessionDesc = 0x0104,
  kSysSendComplete = 0x010d,
};

// The session a host makes and the others join (DPSESSIONDESC2's parts the
// game reads: its name, the player limit and the lobby's words).
struct SessionDesc {
  std::string name;
  uint32_t max_players = 8;
  uint32_t user[4] = {};  // dwUser1..4: the lobby's settings (0x00586ed0, 0x00586f20)
};

// The platform's side: one machine's place in a session.
class Transport {
 public:
  virtual ~Transport() = default;
  // Make a session (the local player its first, and host) or join one by
  // its address; the local player's id, 0 when it failed.
  virtual uint32_t host(const SessionDesc& desc) = 0;
  virtual uint32_t join(const std::string& address) = 0;
  virtual void close() = 0;
  // A message from the local player; guaranteed ones arrive, in order. The
  // send's id (for the send-done event) in *id, 0 when it has none.
  virtual bool send(uint32_t to, const void* data, size_t size, bool guaranteed, uint32_t* id) = 0;
  // The next message waiting, false when none.
  virtual bool receive(Packet* out) = 0;
  // Whether sends may be asynchronous (DPCAPS's, [0x00730765]).
  virtual bool async_sends() const { return true; }
};

// The local player's record, [0x0073075c] (0xec bytes in the exe; its id at
// +4, the rest with the players' lists, multiplayer-port.md piece 3).
struct Player {
  uint32_t id = 0;  // +0x04
};

// zNetwork's globals.
struct Session {
  bool initialised = false;   // [0x007306c8]
  bool open = false;          // [0x00730764]  a session joined or made
  uint32_t lobby = 0;         // [0x0073072c]  a lobby's connection (never: the lobby launch is not ported)
  bool async = false;         // [0x00730765]  sends asynchronous
  uint32_t host_id = 0;       // [0x00730760]  the host's player id
  std::unique_ptr<Player> local;  // [0x0073075c]
  Transport* transport = nullptr;  // [0x00730724]'s place
};
Session& session();

// Ours: zNetwork up on a transport (the exe's 0x00589540 made DirectPlay).
void initialise(Transport* t);
// Ours, the session's start: host, or join an address (0x00586f60,
// 0x00586fb0 with Open; the local player made, the host recorded).
bool host_session(const SessionDesc& desc);
bool join_session(const std::string& address);

bool active();          // 0x00586ce0: up, with a session or a lobby's
uint32_t local_id();    // 0x00586d10: the local player's id, 0 without one
bool is_host();         // 0x00586d20: the local player is the host
// 0x005888a0: a message from the local player to `to` (0 all); the async
// send's id, else 0 (also on a failure, which is reported).
uint32_t send(const void* msg, uint32_t size, bool guaranteed, uint32_t to);
// 0x00586c20: the session left and zNetwork down (the handlers cleared,
// the local player destroyed, the transport closed, the globals cleared).
void shutdown();

// Ours: an in-process transport for the tests, every endpoint of one Hub in
// the same session: the first to host is the host, ids from 1 in order.
class Hub {
 public:
  Hub();
  ~Hub();
  Transport* endpoint();  // a new machine's transport (the hub keeps it)
  struct Impl;
  std::unique_ptr<Impl> impl;
};

}  // namespace znet
