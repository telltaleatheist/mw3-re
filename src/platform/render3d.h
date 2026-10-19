#pragma once
#include <cstdint>

// What the rebuilt Direct3D layer (zvid_ddd3d.c, docs/render.md section 4)
// needs from the machine: stands in for IDirect3DDevice3 and the DirectDraw
// surfaces behind it. The game keeps its own transform, cull, clip and
// projection, so vertices arrive already in screen pixels, as the original's
// D3DTLVERTEX (FVF 0x1c4) did. FIX (A): Direct3D 6 does not exist on macOS and
// is legacy on Windows; the SDL3 GPU front end implements this, a recorder
// does for tests.
namespace platform {

struct TLVertex {                  // D3DTLVERTEX, 0x20 bytes
  float sx, sy;                    // pixels in the viewport
  float sz;                        // depth: near / z, so larger is nearer (reversed)
  float rhw;                       // 1 / w, for perspective-correct UVs
  uint32_t diffuse;                // ARGB 8888
  uint32_t specular;               // ARGB; alpha = fog factor (255 no fog)
  float tu, tv;
};
static_assert(sizeof(TLVertex) == 0x20, "TLVertex must match D3DTLVERTEX");

enum class Primitive : uint8_t { kPoints = 1, kStrip = 5, kFan = 6 };  // D3DPRIMITIVETYPE values used

// The render and texture-stage states the draw functions set (section 4).
struct DrawState {
  int32_t texture = -1;            // SetTexture(0, ...): a handle from create_texture, -1 none
  uint8_t address_u = 1;           // ADDRESSU: 1 WRAP, 3 CLAMP
  uint8_t address_v = 1;           // ADDRESSV
  bool blend = false;              // ALPHABLENDENABLE with SRCALPHA / INVSRCALPHA
  bool multiply = false;           // the shadow passes: SRCBLEND ZERO, DESTBLEND SRCCOLOR
  bool depth_write = true;         // ZWRITEENABLE
  bool depth_always = false;       // ZFUNC ALWAYS (overlays), else GREATEREQUAL
  bool alpha_modulate = false;     // ALPHAOP MODULATE (transparent), else SELECTARG1
  bool fog = false;                // FOGENABLE
  bool flat = false;               // SHADEMODE FLAT
  bool wireframe = false;          // FILLMODE WIREFRAME (a debug mode)
};

class Render3D {
 public:
  virtual ~Render3D() = default;
  // 0x0057b130's surfaces: RGBA8, width * height * 4 bytes. `type` is the
  // record's 1 opaque, 4 alpha plane, 5 one-bit alpha; returns a handle.
  virtual int32_t create_texture(int width, int height, const uint8_t* rgba, uint8_t type) = 0;
  virtual void release_texture(int32_t handle) = 0;
  virtual void set_viewport(int x, int y, int width, int height) = 0;  // Viewport3::SetViewport2
  // The frame clear (0x005777f0): colour when asked, depth always to 0 (reversed).
  virtual void clear(bool color, uint8_t r, uint8_t g, uint8_t b) = 0;
  virtual void begin_scene() = 0;  // BeginScene (vt+0x24)
  virtual void end_scene() = 0;    // EndScene (vt+0x28)
  // DrawPrimitive (vt+0x70) with flags 0x18.
  virtual void draw(Primitive type, const TLVertex* vertices, int count, const DrawState& state) = 0;
  virtual void set_fog_color(uint8_t r, uint8_t g, uint8_t b) = 0;  // FOGCOLOR (0x0057bcf0)
  virtual void present() = 0;      // Flip, or the windowed Blt (0x0057a850 / 0x00577130)
};

}  // namespace platform
