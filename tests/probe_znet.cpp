// zNetwork's session checks and its send (multiplayer-port.md piece 1) for
// tests/differential/test_znet_session.py.
#include <cstdint>
#include <cstring>

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
