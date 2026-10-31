#include "gamez/znetwork/znet_session.h"

#include "gamez/znetwork/znet_dplay.h"

#include <algorithm>
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
  if (t) wire_players();  // 0x00586bfc's registration of 0xd, the translation
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

// Mech3.exe 1.2 @ 0x00587aa0
bool on(uint16_t type, Handler fn, int32_t arg) {
  session().handlers.push_back(HandlerEntry{type, fn, arg});
  return true;
}

// Mech3.exe 1.2 @ 0x00587b20 (std::remove then erase). FIX (bug): while a
// dispatch runs the entries are marked and erased after it; the exe's walk
// (0x00587c40) read the deleted node.
bool off(uint16_t type, Handler fn) {
  Session& s = session();
  for (HandlerEntry& e : s.handlers)
    if (e.type == type && e.fn == fn) e.fn = nullptr;
  if (!s.dispatching)
    s.handlers.erase(std::remove_if(s.handlers.begin(), s.handlers.end(), [](const HandlerEntry& e) { return !e.fn; }),
                     s.handlers.end());
  return true;
}

// Mech3.exe 1.2 @ 0x00587a00
void clear_handlers() {
  Session& s = session();
  if (s.dispatching) {
    for (HandlerEntry& e : s.handlers) e.fn = nullptr;
    return;
  }
  s.handlers.clear();
}

// Mech3.exe 1.2 @ 0x00587c40. A handler added meanwhile is reached too (the
// list's end moves on), as in the exe.
void dispatch(uint32_t from, const uint8_t* msg, size_t size) {
  Session& s = session();
  if (size < 2) return;
  uint16_t type;
  std::memcpy(&type, msg, 2);
  ++s.dispatching;
  for (size_t i = 0; i < s.handlers.size(); ++i) {
    const HandlerEntry e = s.handlers[i];
    if (e.fn && e.type == type) e.fn(from, msg, size);
  }
  if (--s.dispatching == 0)
    s.handlers.erase(std::remove_if(s.handlers.begin(), s.handlers.end(), [](const HandlerEntry& e) { return !e.fn; }),
                     s.handlers.end());
}

// Mech3.exe 1.2 @ 0x00588930. DirectPlay's Receive into the growing buffer
// [0x007307d8] (its size [0x007307dc]) is the transport's; `free_buffer`
// freed it, so here it does nothing.
int32_t poll(int32_t max, bool free_buffer) {
  (void)free_buffer;
  if (!active()) return 0;
  Session& s = session();
  int32_t handled = 0;
  Packet p;
  while (s.transport && s.transport->receive(&p)) {
    if (p.data.size() > 3) {
      --max, ++handled;
      if (p.from == 0) {
        if (s.system_message) s.system_message(p);
      } else {
        dispatch(p.from, p.data.data(), p.data.size());
      }
    }
    if (max == 0) break;
    if (!active()) break;  // ours: a handler ended the session
  }
  return handled;
}

// Mech3.exe 1.2 @ 0x00586c20. The players' and teams' lists with piece 3.
void shutdown() {
  Session& s = session();
  if (!s.initialised) return;
  poll(-1);
  clear_handlers();
  if (s.transport) s.transport->close();  // DestroyPlayer(local) and Close
  s.initialised = false;
  s.open = false;
  s.lobby = 0;
  s.async = false;
  s.host_id = 0;
  s.local.reset();
  s.transport = nullptr;
  clear_lists();  // 0x00587db0, 0x00587e30, 0x00587ed0
}

// ---------------------------------------------------------------------------
// The in-process hub

struct Hub::Impl {
  struct End;
  std::vector<std::unique_ptr<End>> ends;
  uint32_t next_id = 1, next_msg = 1, host_id = 0;
  bool hosted = false;
  void system(End* to, const SystemEvent& e);
};

struct Hub::Impl::End : Transport {
  Hub::Impl* hub = nullptr;
  uint32_t id = 0;
  std::string short_name, long_name;
  std::deque<Packet> inbox;
  void set_name(const std::string& s, const std::string& l) override { short_name = s, long_name = l; }
  bool is_host(uint32_t who) const override { return who && who == hub->host_id; }
  uint32_t host(const SessionDesc&) override {
    if (hub->hosted || id) return 0;
    hub->hosted = true;
    return hub->host_id = id = hub->next_id++;
  }
  uint32_t join(const std::string&) override {
    if (!hub->hosted || id) return 0;
    id = hub->next_id++;
    SystemEvent e;
    e.kind = kSysCreatePlayerOrGroup, e.player_type = 1, e.id = id, e.short_name = short_name, e.long_name = long_name;
    for (auto& o : hub->ends)
      if (o.get() != this && o->id) hub->system(o.get(), e);
    // The joiner hears of everyone already there, as DirectPlay's join does.
    for (auto& o : hub->ends)
      if (o.get() != this && o->id) {
        SystemEvent k;
        k.kind = kSysCreatePlayerOrGroup, k.player_type = 1, k.id = o->id, k.short_name = o->short_name,
        k.long_name = o->long_name;
        hub->system(this, k);
      }
    return id;
  }
  void close() override {
    if (!id) return;
    SystemEvent e;
    e.kind = kSysDestroyPlayerOrGroup, e.player_type = 1, e.id = id;
    id = 0;
    inbox.clear();
    for (auto& o : hub->ends)
      if (o->id) hub->system(o.get(), e);
  }
  bool send(uint32_t to, const void* data, size_t size, bool, uint32_t* msg_id) override {
    if (!id) return false;
    Packet p;
    p.from = id, p.to = to;
    p.data.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
    for (auto& o : hub->ends)
      if (o.get() != this && o->id && (to == 0 || to == o->id)) o->inbox.push_back(p);
    if (msg_id) {
      *msg_id = hub->next_msg++;
      SystemEvent e;
      e.kind = kSysSendComplete, e.id = id, e.msg_id = *msg_id;
      hub->system(this, e);
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

void Hub::Impl::system(End* to, const SystemEvent& e) {
  Packet p;
  p.sys = e;
  const uint32_t about = e.kind == kSysSendComplete ? e.msg_id : e.id;
  p.data.resize(8);
  std::memcpy(p.data.data(), &e.kind, 4);
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
