#pragma once
#include <cstdint>

// zVideo's surfaces and frame calls around Direct3D. The surface lock and
// unlock (0x00577590 / 0x00577600) are zvid_dd.c's; the scene nesting
// (0x00579af0 / 0x00579b10) sits between zvid_dd.c and zvid_init.c, the 3D
// frame's lock (0x00576d00 / 0x00576d60) between zutl_zar.cpp and zvid_buff.c:
// file names unknown, kept here with zvid_dd. docs/render-frame.md.
namespace zvid {

// A DirectDraw surface's record (0x24 bytes), its fields filled by a lock.
struct Surface {
  int32_t width = 0;               // +0x00
  int32_t height = 0;              // +0x04
  int32_t pitch = 0;               // +0x08  bytes
  int32_t valid = 0;               // +0x0c  1 once locked
  uint16_t* pixels = nullptr;      // +0x10
  int32_t locked = 0;              // +0x14
};
struct Surfaces {
  Surface back;                    // 0x008027a4  the back buffer
  Surface copy;                    // 0x008027c8  the software renderer's surface
  Surface primary;                 // 0x008027ec
  int32_t primary_lockable = 0;    // 0x008026e4
};
Surfaces& surfaces();

// FIX (A): DirectDraw's Lock (0x00577630, retried after a restore) and Unlock
// (0x00577690). lock fills width, height, pitch and pixels; 0 is success.
struct SurfaceHooks {
  int32_t (*lock)(void* ctx, Surface* s) = nullptr;
  int32_t (*unlock)(void* ctx, Surface* s) = nullptr;
  void* ctx = nullptr;
};
SurfaceHooks& surface_hooks();

int32_t lock_surface(Surface* s);    // 0x00577590 ([0x00809340])
int32_t unlock_surface(Surface* s);  // 0x00577600 ([0x0080933c])

// Mech3.exe 1.2 @ 0x00579af0 / 0x00579b10: BeginScene and EndScene
// (0x0057a740 / 0x0057a7e0) on the outermost of nested pairs.
int32_t& scene_depth();              // 0x0080270c
int32_t scene_begin();
int32_t scene_end();

// The 2D drawing target of the windows-text object at 0x00736328 (+0x18..),
// set by 0x005a67a0 (between twod_wintext.cpp and zent_init.cpp).
struct TextTarget {
  uint16_t* pixels = nullptr;      // +0x18
  int32_t width = 0;               // +0x1c
  int32_t height = 0;              // +0x20
  int32_t pitch = 0;               // +0x24
};
TextTarget& text_target();
void set_text_target(uint16_t* pixels, int32_t width, int32_t height, int32_t pitch);  // 0x005a67a0

// Mech3.exe 1.2 @ 0x00576d00: the back buffer locked in Direct3D so the 2D
// drawing goes on top of the 3D frame (the HUD, the fade's still); every 2D
// target set to it. 0x00576d60 unlocks it.
int32_t lock_3d();
void unlock_3d();

}  // namespace zvid
