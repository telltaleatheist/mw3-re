#include "frontend/sdl/vulkan_check.h"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <cstring>

namespace sdlfe {

Vulkan vulkan_check() {
  if (const char* forced = std::getenv("MW3_VULKAN_CHECK")) {  // (the process's own: SDL_getenv keeps a copy)
    if (std::strcmp(forced, "noloader") == 0) return Vulkan::kNoLoader;
    if (std::strcmp(forced, "nodriver") == 0) return Vulkan::kNoDriver;
    return Vulkan::kOk;
  }
#if defined(__linux__)
  SDL_SharedObject* lib = SDL_LoadObject("libvulkan.so.1");
  if (!lib) return Vulkan::kNoLoader;
  SDL_UnloadObject(lib);
  // SDL's own test for its Vulkan backend: an instance and a device with what SDL_GPU needs.
  return SDL_GPUSupportsShaderFormats(SDL_GPU_SHADERFORMAT_SPIRV, "vulkan") ? Vulkan::kOk : Vulkan::kNoDriver;
#else
  return Vulkan::kOk;
#endif
}

std::string vulkan_help(Vulkan v) {
  if (v == Vulkan::kOk) return {};
  const std::string what = v == Vulkan::kNoLoader ? "no Vulkan library (libvulkan)"
                                                  : "no Vulkan driver for its graphics card";
  return "MechWarrior 3 needs Vulkan graphics, and this computer has " + what +
         ". Install your system's 64-bit Vulkan packages (not the lib32 ones), then start again:\n"
         "Arch: vulkan-icd-loader and vulkan-intel, vulkan-radeon or nvidia-utils\n"
         "Debian or Ubuntu: libvulkan1 and mesa-vulkan-drivers\n"
         "Fedora: vulkan-loader and mesa-vulkan-drivers\n"
         "(NVIDIA's own driver brings its Vulkan with it.)";
}

}  // namespace sdlfe
