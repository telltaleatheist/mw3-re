#include "mech3/app_keys.h"

#include "gamez/zinput/zinput.h"

namespace mech3 {
namespace {
AppKeyCalls g_calls;
void debug_7e(int32_t) {
  if (g_calls.debug_7e) g_calls.debug_7e();
}
}  // namespace

AppKeyCalls& app_key_calls() { return g_calls; }

// Mech3.exe 1.2 @ 0x0040db60
void register_app_keys() {
  zin::set_callback(0x61, app_key);   // CYCLE_CAM (X)
  zin::set_callback(0x70, app_key);   // MESSAGE_TOGGLE (`)
  zin::set_callback(0x7e, debug_7e);  // 0x004abbc0
  zin::set_callback(0x6c, app_key);   // VMODE_TOGGLE (Shift+V)
}

// Mech3.exe 1.2 @ 0x0040dba0
void app_key(int32_t cmd) {
  switch (cmd) {
    case 0x61:
      if (g_calls.cycle_camera) g_calls.cycle_camera();
      break;
    case 0x6c:
      if (g_calls.vmode_cycle) g_calls.vmode_cycle();
      break;
    case 0x70:
      if (g_calls.message_toggle) g_calls.message_toggle();
      break;
    default:
      break;
  }
}

}  // namespace mech3
