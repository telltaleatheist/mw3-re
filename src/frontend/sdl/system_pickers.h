#pragma once
// Ours: the system's file and folder pickers, the one way the programs open them (the launcher's FIND THE GAME and
// GAME FILES, the MODS screen's ADD MOD and EXPORT, the install search). They live in the shared setup kit since
// 2026-11-21 (extern/re-setup-kit, rekit/sdl/pickers.h). In a run no player is at (pointer_hands_off.h: hidden,
// --shot, --frames, scripted, every test program and what a test starts) no picker ever opens: the kit asks
// pointer_hands_off through its unattended() hook, set here once.
#include "platform/pointer_hands_off.h"
#include "rekit/sdl/pickers.h"

namespace sdlfe {

using rekit::sdl::open_picker;
using rekit::sdl::picker_opener;
using rekit::sdl::PickerAsk;
using rekit::sdl::PickerKind;

namespace pickers_detail {
inline const bool hooked = (rekit::sdl::unattended() = [] { return platform::pointer_hands_off(); }, true);
}  // namespace pickers_detail

}  // namespace sdlfe
