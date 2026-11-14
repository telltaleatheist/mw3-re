// Ours: does this computer have Vulkan for the game (docs/linux.md, Vulkan)? On Linux the game
// draws through SDL_GPU's Vulkan; without it mw3 can only say so in its fatal box. The launcher
// (whose own window needs no Vulkan) checks at its start and says what to install before PLAY.
#pragma once

#include <string>

namespace sdlfe {

enum class Vulkan { kOk, kNoLoader, kNoDriver };

// kNoLoader: libvulkan.so.1 does not load; kNoDriver: it loads, but SDL_GPU finds no Vulkan device
// it can use (no ICD for the card, or only 32-bit ones). Elsewhere than Linux always kOk.
// MW3_VULKAN_CHECK=noloader|nodriver|ok (checks) says what to answer.
Vulkan vulkan_check();

// What to install, in plain words ("" for kOk).
std::string vulkan_help(Vulkan v);

}  // namespace sdlfe
