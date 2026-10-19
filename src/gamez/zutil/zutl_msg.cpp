#include "gamez/zutil/zutl_msg.h"

namespace zutl {

namespace {
Messages g_messages;
}  // namespace

Messages& messages() { return g_messages; }

// Mech3.exe 1.2 @ 0x00575f90
int32_t msg_id(const char* name) {
  if (!name || !g_messages.get_id) return 0;
  return g_messages.get_id(name);
}

// Mech3.exe 1.2 @ 0x00575fb0
const char* localize(const char* name) {
  const int32_t id = msg_id(name);
  if (id == 0) return name;
  return g_messages.get_text ? g_messages.get_text(id) : name;
}

}  // namespace zutl
