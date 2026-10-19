#pragma once
#include <SDL3/SDL.h>

#include <map>
#include <vector>

#include "platform/render3d.h"

// platform::Render3D on SDL3's GPU API (docs/render.md section 4): the
// original's D3DTLVERTEX draws collected over a scene, fans and strips turned
// into triangle lists, then drawn in order in one render pass with reversed
// depth (cleared to 0, GREATER_OR_EQUAL). Shaders are MSL for now (Metal);
// SPIR-V and DXIL come with the Windows build.
namespace sdlfe {

class SdlRender3D : public platform::Render3D {
 public:
  SdlRender3D(SDL_GPUDevice* device, SDL_Window* window);
  ~SdlRender3D() override;
  bool ok() const { return ok_; }

  int32_t create_texture(int width, int height, const uint8_t* rgba, uint8_t type) override;
  void release_texture(int32_t handle) override;
  void set_viewport(int x, int y, int width, int height) override;
  void clear(bool color, uint8_t r, uint8_t g, uint8_t b) override;
  void begin_scene() override;
  void end_scene() override;
  void draw(platform::Primitive type, const platform::TLVertex* vertices, int count,
            const platform::DrawState& state) override;
  void set_fog_color(uint8_t r, uint8_t g, uint8_t b) override;
  void present() override;

  // Renders the next scene into an offscreen target as well and saves it.
  void capture_next(const char* bmp_path) { capture_path_ = bmp_path; }

 private:
  struct Run {
    platform::DrawState state;
    uint32_t first, count;
  };
  SDL_GPUGraphicsPipeline* pipeline(const platform::DrawState& s, bool textured);
  SDL_GPUSampler* sampler(uint8_t u, uint8_t v);
  void ensure_depth(uint32_t w, uint32_t h);
  void ensure_buffer(uint32_t bytes);

  SDL_GPUDevice* dev_;
  SDL_Window* window_;
  bool ok_ = false;
  SDL_GPUTextureFormat color_format_ = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
  SDL_GPUShader* vs_ = nullptr;
  SDL_GPUShader* fs_tex_ = nullptr;
  SDL_GPUShader* fs_flat_ = nullptr;
  std::map<uint32_t, SDL_GPUGraphicsPipeline*> pipelines_;
  std::map<uint32_t, SDL_GPUSampler*> samplers_;
  std::vector<SDL_GPUTexture*> textures_;
  SDL_GPUTexture* depth_ = nullptr;
  uint32_t depth_w_ = 0, depth_h_ = 0;
  SDL_GPUBuffer* vbuf_ = nullptr;
  uint32_t vbuf_size_ = 0;
  std::vector<platform::TLVertex> vertices_;
  std::vector<Run> runs_;
  int vx_ = 0, vy_ = 0, vw_ = 640, vh_ = 480;
  bool clear_color_ = true;
  float clear_rgb_[3] = {0, 0, 0};
  float fog_rgb_[3] = {0, 0, 0};
  SDL_GPUCommandBuffer* cmd_ = nullptr;
  const char* capture_path_ = nullptr;
};

}  // namespace sdlfe
