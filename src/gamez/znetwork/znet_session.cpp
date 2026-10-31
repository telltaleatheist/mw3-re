#include "gamez/znetwork/znet_session.h"

#include <cstdio>
#include <cstring>
#include <deque>

namespace znet {

Session& session() {
  static Session s;
  return s;
}

void initialise(Transport* t) {
  Session& s = session();
  s.transport = t;
  s.initialised = t != nullptr;
  s.async = t && t->async_sends();
}

namespace {
bool start(uint32_t id) {
  Session& s = session();
  if (!id) return false;
  s.local = std::make_unique<Player>();
  s.local->id = id;
  s.open = true;
  return true;
}
}  // namespace

bool host_session(const SessionDesc& desc) {
  Session& s = session();
  if (!s.initialised || s.open) return false;
  const uint32_t id = s.transport->host(desc);
  if (!start(id)) return false;
  s.host_id = id;  // the host's own 0xd (0x0058886c) says so to the others
  return true;
}

bool join_session(const std::string& address) {
  Session& s = session();
  if (!s.initialised || s.open) return false;
  return start(s.transport->join(address));  // the host's 0xd names the host
}

// Mech3.exe 1.2 @ 0x00586ce0
bool active() {
  const Session& s = session();
  return s.initialised && (s.open || s.lobby != 0);
}

// Mech3.exe 1.2 @ 0x00586d10
uint32_t local_id() {
  const Session& s = session();
  return s.local ? s.local->id : 0;
}

// Mech3.exe 1.2 @ 0x00586d20
bool is_host() {
  const Session& s = session();
  return s.local && s.host_id == local_id();
}

// Mech3.exe 1.2 @ 0x005888a0: SendEx from the local player, DPSEND_GUARANTEED
// when asked, DPSEND_ASYNC when the provider allows ([0x00730765]); a failure
// other than DPERR_PENDING goes to the error report (0x0058a880, line 0x1d9).
// FIX (bug): without a local player the exe reads [0x0073075c]+4 through
// null; nothing is sent.
uint32_t send(const void* msg, uint32_t size, bool guaranteed, uint32_t to) {
  Session& s = session();
  if (!s.transport || !s.local) return 0;
  uint32_t id = 0;
  if (!s.transport->send(to, msg, size, guaranteed, s.async ? &id : nullptr)) {
    std::fprintf(stderr, "mw3: network: a message (type 0x%x, %u bytes) was not sent\n",
                 size >= 2 ? static_cast<const uint8_t*>(msg)[0] | static_cast<const uint8_t*>(msg)[1] << 8 : 0, size);
    return 0;
  }
  return s.async ? id : 0;
}

// Mech3.exe 1.2 @ 0x00586c20. The poll first and the handlers cleared come
// with the dispatcher (piece 2); the players' and teams' lists with piece 3.
void shutdown() {
  Session& s = session();
  if (!s.initialised) return;
  if (s.transport) s.transport->close();  // DestroyPlayer(local) and Close
  s.initialised = false;
  s.open = false;
  s.lobby = 0;
  s.async = false;
  s.host_id = 0;
  s.local.reset();
  s.transport = nullptr;
}

// ---------------------------------------------------------------------------
// The in-process hub

struct Hub::Impl {
  struct End;
  std::vector<std::unique_ptr<End>> ends;
  uint32_t next_id = 1, next_msg = 1;
  bool hosted = false;
  void system(End* to, uint32_t kind, uint32_t about);
};

struct Hub::Impl::End : Transport {
  Hub::Impl* hub = nullptr;
  uint32_t id = 0;
  std::deque<Packet> inbox;
  uint32_t host(const SessionDesc&) override {
    if (hub->hosted || id) return 0;
    hub->hosted = true;
    return id = hub->next_id++;
  }
  uint32_t join(const std::string&) override {
    if (!hub->hosted || id) return 0;
    id = hub->next_id++;
    for (auto& e : hub->ends)
      if (e.get() != this && e->id) hub->system(e.get(), kSysCreatePlayerOrGroup, id);
    return id;
  }
  void close() override {
    if (!id) return;
    const uint32_t gone = id;
    id = 0;
    inbox.clear();
    for (auto& e : hub->ends)
      if (e->id) hub->system(e.get(), kSysDestroyPlayerOrGroup, gone);
  }
  bool send(uint32_t to, const void* data, size_t size, bool, uint32_t* msg_id) override {
    if (!id) return false;
    Packet p;
    p.from = id, p.to = to;
    p.data.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
    for (auto& e : hub->ends)
      if (e.get() != this && e->id && (to == 0 || to == e->id)) e->inbox.push_back(p);
    if (msg_id) {
      *msg_id = hub->next_msg++;
      hub->system(this, kSysSendComplete, *msg_id);
    }
    return true;
  }
  bool receive(Packet* out) override {
    if (inbox.empty()) return false;
    *out = std::move(inbox.front());
    inbox.pop_front();
    return true;
  }
};

void Hub::Impl::system(End* to, uint32_t kind, uint32_t about) {
  Packet p;
  p.data.resize(8);
  std::memcpy(p.data.data(), &kind, 4);
  std::memcpy(p.data.data() + 4, &about, 4);
  to->inbox.push_back(std::move(p));
}

Hub::Hub() : impl(std::make_unique<Impl>()) {}
Hub::~Hub() = default;

Transport* Hub::endpoint() {
  impl->ends.push_back(std::make_unique<Impl::End>());
  impl->ends.back()->hub = impl.get();
  return impl->ends.back().get();
}

}  // namespace znet
