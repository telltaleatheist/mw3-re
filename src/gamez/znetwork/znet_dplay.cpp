#include "gamez/znetwork/znet_dplay.h"

#include <algorithm>
#include <cstring>

#include "gamez/zutil/zutl_msg.h"

namespace znet {

Lists& lists() {
  static Lists l;
  return l;
}

// Mech3.exe 1.2 @ 0x00589700
PlayerRecord* player(uint32_t id) {
  for (auto& p : lists().players)
    if (p->id == id) return p.get();
  return nullptr;
}

TeamRecord* team(uint32_t id) {
  for (auto& t : lists().teams)
    if (t->id == id) return t.get();
  return nullptr;
}

// Mech3.exe 1.2 @ 0x00588a60
const char* player_name(uint32_t id) {
  const PlayerRecord* p = player(id);
  return p ? p->short_name.c_str() : nullptr;
}

// Mech3.exe 1.2 @ 0x005898b0: the host check against [0x00730760].
void each_player(void (*fn)(bool host, const char* name, void* ctx), void* ctx) {
  for (auto& p : lists().players) fn(session().host_id == p->id, p->short_name.c_str(), ctx);
}

// Mech3.exe 1.2 @ 0x00589910: (0, the record, whose name is first); the count.
int32_t each_team(void (*fn)(bool host, const char* name, void* ctx), void* ctx) {
  for (auto& t : lists().teams) fn(false, t->name.c_str(), ctx);
  return static_cast<int32_t>(lists().teams.size());
}

namespace {
// A local message (type, size, then its words) dispatched from `from`.
void local(uint16_t type, uint32_t from, const std::vector<uint32_t>& words = {}) {
  std::vector<uint8_t> m(4 + 4 * words.size());
  const uint16_t size = static_cast<uint16_t>(m.size());
  std::memcpy(m.data(), &type, 2);
  std::memcpy(m.data() + 2, &size, 2);
  for (size_t i = 0; i < words.size(); ++i) std::memcpy(m.data() + 4 + 4 * i, &words[i], 4);
  dispatch(from, m.data(), m.size());
}
// 0xd, "I am the host" (no payload), to every player, guaranteed.
void say_host() {
  const uint8_t m[4] = {0x0d, 0, 4, 0};
  send(m, 4, true, 0);
}
std::string clipped(const std::string& s, size_t n) { return s.size() > n ? s.substr(0, n) : s; }
}  // namespace

// Mech3.exe 1.2 @ 0x00588a70. The exe's local types 2 and 0xe carried a
// pointer to DirectPlay's message at +4; here their words (ours, section 3):
// 2 the player (8 bytes, as the exe's), 0xe the send's id and its time (12).
void system_message(const Packet& p) {
  Session& s = session();
  Lists& l = lists();
  const SystemEvent& e = p.sys;
  switch (e.kind) {
    case kSysAddPlayerToGroup: {  // 7: the player into the team, type 5
      if (PlayerRecord* r = player(e.id)) r->team = e.group;
      local(5, e.id);
      return;
    }
    case kSysDeletePlayerFromGroup: {  // 0x21: out of its team, type 2
      if (PlayerRecord* r = player(e.id)) r->team = 0;
      local(2, e.id, {e.id});
      return;
    }
    case kSysDestroyPlayerOrGroup:
      if (e.player_type == 1) {
        // Type 7 when it is ourselves (kicked), else 6, before the record goes.
        local(e.id == local_id() ? 7 : 6, e.id);
        // FIX (bug): the exe unlinked the record and never freed it.
        l.players.erase(std::remove_if(l.players.begin(), l.players.end(),
                                       [&](const std::unique_ptr<PlayerRecord>& r) { return r->id == e.id; }),
                        l.players.end());
      } else {
        if (!team(e.id)) return;
        for (auto& r : l.players)
          if (r->team == e.id) r->team = 0;
        l.teams.erase(std::remove_if(l.teams.begin(), l.teams.end(),
                                     [&](const std::unique_ptr<TeamRecord>& t) { return t->id == e.id; }),
                      l.teams.end());
        local(8, e.id);
      }
      return;
    case kSysCreatePlayerOrGroup:
      if (e.player_type == 1) {
        if (!player(e.id)) {
          auto r = std::make_unique<PlayerRecord>();
          r->id = e.id;
          r->short_name = clipped(e.short_name, 0x50);
          r->long_name = clipped(e.long_name, 0x50);
          r->caps_flags = s.transport && s.transport->is_host(e.id) ? 2u : 0u;  // 0x00589870(1)
          r->is_host = (r->caps_flags >> 1) & 1;
          l.players.push_back(std::move(r));
          local(3, e.id);
        }
        if (is_host()) say_host();
      } else if (!team(e.id)) {
        auto t = std::make_unique<TeamRecord>();
        t->id = e.id;
        t->name = clipped(e.short_name, 0x4f);
        l.teams.push_back(std::move(t));
      }
      local(4, e.id);
      return;
    case kSysSessionLost:  // DPERR_SESSIONLOST's name as a network error
      network_error(zutl::localize("MSG_DPERR_SESSIONLOST"));  // 0x0058a8c0's name for 0x88770136
      return;
    case kSysHost: {  // we are the host now: say so, to all and to ourselves
      if (s.local) s.local->is_host = 1;
      say_host();
      const uint8_t m[4] = {0x0d, 0, 4, 0};
      dispatch(local_id(), m, 4);
      return;
    }
    case kSysSetPlayerOrGroupData:
      local(e.player_type == 1 ? 9 : 0xa, e.id);
      return;
    case kSysSetPlayerOrGroupName:
      local(0xb, e.id);
      return;
    case kSysSetSessionDesc:  // 0x00587740: the 0x50 bytes into [0x00730730]
      s.desc = e.desc;
      return;
    case kSysSendComplete:
      local(0xe, local_id(), {e.msg_id, e.send_time});
      return;
    default:
      return;
  }
}

// Mech3.exe 1.2 @ 0x0058ae10: type 0xc, its size the text's with its 0 and
// the header, then the text.
void network_error(const std::string& text) {
  std::vector<uint8_t> m(text.size() + 1 + 4, 0);
  const uint16_t type = 0xc, size = static_cast<uint16_t>(m.size());
  std::memcpy(m.data(), &type, 2);
  std::memcpy(m.data() + 2, &size, 2);
  std::memcpy(m.data() + 4, text.data(), text.size());
  dispatch(local_id(), m.data(), m.size());
}

// Mech3.exe 1.2 @ 0x00586d50
void on_host(uint32_t from, const uint8_t*, size_t) { session().host_id = from; }

void wire_players() {
  session().system_message = &system_message;
  on(0x0d, &on_host);
}

void clear_lists() {
  lists().players.clear();
  lists().teams.clear();
}

}  // namespace znet
