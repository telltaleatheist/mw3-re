#include "mech3/mech3_interp.h"

#include <cstring>

#include "crt/msvc_math.h"

namespace mech3 {

namespace {
InterpGlobals g_globals;
zint::Interp* g_interp = nullptr;
const zint::Vtable kVtable = {interp_pre, zint::base_vtable().post, interp_on_error};

// 0x004d6dc0, reached through identical-code folding: always false.
bool console_command_known(const char*) { return false; }
}  // namespace

InterpGlobals& interp_globals() { return g_globals; }
zint::Interp*& interp() { return g_interp; }
const zint::Vtable& interp_vtable() { return kVtable; }

// Mech3.exe 1.2 @ 0x00410700
void interp_create(zint::Interp& in) {
  zint::init(in, "interp.zbd", ".;zbd");
  in.vt = &kVtable;
  in.mech = nullptr;
  zint::var_register(in, "DamageScale", 1, &g_globals.damage_scale);
}

// Mech3.exe 1.2 @ 0x004107e0. WorldSetGravity is MW3's own command (no
// shipped script uses it); everything else goes on to the dispatcher. A word
// starting with 'W' that is not it counts a miss here as well, so an unknown
// W command reaches three and is logged as BadCommand.
bool interp_pre(zint::Interp& in, const char* argv0) {
  if (argv0[0] == 'W') {
    if (zint::argv0_is(in, "WorldSetGravity")) {
      const double g = zint::arg_float(in, 0.0f);
      g_globals.gravity = static_cast<float>(g);
      g_globals.inv_gravity = crt::x87_div_f24(1.0, g);  // 1/g from atof's double
      return false;
    }
    zint::miss(in);
  } else if (in.console && !console_command_known(argv0)) {
    zint::miss(in);
  }
  return true;
}

// Mech3.exe 1.2 @ 0x00410770. If gamez.zbd would not load, USEZBD goes off
// and the .gs falls into its build-from-sources branch (docs/interp.md).
// It looks at argv[0] as it is now, not at the line's word.
bool interp_on_error(zint::Interp& in, const char*) {
  if (std::strcmp(in.argc ? in.argv[0] : "", "GameZReadZBDFile") == 0) {
    zint::macro_set_false(in, "USEZBD");
    return true;
  }
  return false;
}

void interp_register_world_vars(zint::Interp& in, float* ceiling) {
  zint::var_register(in, "gravity", 1, &g_globals.gravity);
  zint::var_register(in, "ceiling", 1, ceiling);
}

// Mech3.exe 1.2 @ 0x004108a0
bool interp_set_mech(zint::Interp& in, void* mech, zcls::Node* node) {
  in.mech = mech;
  if (!mech) return false;
  in.current = node;
  zint::log(in, "current mech = %s", node ? node->name() : nullptr);
  return true;
}

}  // namespace mech3
