#pragma once
// For the tests: a Mech3Msg.dll made here (nothing of the game's), which zutl::read_message_dll reads.
#include <cstdint>
#include <map>
#include <string>

// A message dll as the game's is laid out, made here: a PE32 with one section holding a resource directory
// (type 11, RT_MESSAGETABLE) and its table, one block a message, each text ending \r\n as the game's do.
inline std::string message_dll(const std::map<int32_t, std::string>& texts) {
  auto u16 = [](std::string& b, size_t at, uint32_t v) { b[at] = static_cast<char>(v), b[at + 1] = static_cast<char>(v >> 8); };
  auto u32 = [&](std::string& b, size_t at, uint32_t v) { u16(b, at, v & 0xffff), u16(b, at + 2, v >> 16); };
  std::string data(4 + 12 * texts.size(), '\0');
  u32(data, 0, static_cast<uint32_t>(texts.size()));
  size_t k = 0;
  for (const auto& [id, text] : texts) {
    std::string e = std::string(4, '\0') + text + "\r\n" + std::string(1, '\0');
    while (e.size() % 4) e += '\0';
    u16(e, 0, static_cast<uint32_t>(e.size()));
    u32(data, 4 + 12 * k, static_cast<uint32_t>(id)), u32(data, 8 + 12 * k, static_cast<uint32_t>(id));
    u32(data, 12 + 12 * k, static_cast<uint32_t>(data.size()));
    data += e;
    ++k;
  }
  std::string sec(0x58, '\0');
  u16(sec, 0x0e, 1), u32(sec, 0x10, 11), u32(sec, 0x14, 0x80000018u);     // the types: 11
  u16(sec, 0x26, 1), u32(sec, 0x28, 1), u32(sec, 0x2c, 0x80000030u);      // its one name
  u16(sec, 0x3e, 1), u32(sec, 0x40, 0x409), u32(sec, 0x44, 0x48);         // its one language
  u32(sec, 0x48, 0x1058), u32(sec, 0x4c, static_cast<uint32_t>(data.size()));
  sec += data;
  std::string b(0x200, '\0');
  b[0] = 'M', b[1] = 'Z';
  u32(b, 0x3c, 0x40);
  b[0x40] = 'P', b[0x41] = 'E';
  u16(b, 0x46, 1);                    // one section
  u16(b, 0x54, 0xe0);                 // the optional header's size
  const size_t opt = 0x58;
  u16(b, opt, 0x10b);
  u32(b, opt + 28, 0x10000000);
  u32(b, opt + 96 + 8 * 2, 0x1000);   // the resources
  const size_t sh = opt + 0xe0;
  u32(b, sh + 8, static_cast<uint32_t>(sec.size())), u32(b, sh + 12, 0x1000);
  u32(b, sh + 16, static_cast<uint32_t>(sec.size())), u32(b, sh + 20, 0x200);
  return b + sec;
}
