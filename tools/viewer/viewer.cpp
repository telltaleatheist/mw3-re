// mw3view: a development viewer for mission worlds, on SDL3's GPU API. Not
// part of the port (see scene.h).
//
//   mw3view [--assets <dir>]... [--mission t1 | --mech madcat [--motion walk [--time s]]] [--tl | --real]
//           [--info] [--shot out.bmp] [--pos x y z] [--look yaw pitch]
//
// --real draws the mission through the ported render path itself
// (src/frontend/debug_world.h): walk, cull, model draw, lighting, fog, clips, draw table;
// --variant picks the mission's world.zrd block (fog, sky, sun).
// --tl draws through the ported Direct3D stand-in (platform::Render3D on SDL3)
// with the original's camera-space projection done on the CPU (render.md
// section 0), instead of the GPU transform.
//
// --assets mounts a folder laid out like the install (zbd/...); give several
// and the first that has a file wins, so a folder of replacements can sit in
// front of the original. Default: game/mw3-1.2.
//
// Controls: right mouse drag to look, WASD to move, Q/E down/up, Shift fast,
// C toggles vertex colours, B the in/out-of-bounds areas, P prints the
// camera, Esc quits. With a mech: M cycles its motions, Space pauses.
#include <SDL3/SDL.h>

#include <cmath>
#include <memory>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "frontend/sdl/sdl_render3d.h"
#include "gamez/zutil/zutl_path.h"
#include "frontend/debug_world.h"
#include "scene.h"

namespace {

const char* kShaders = R"(
#include <metal_stdlib>
using namespace metal;
struct In { float3 pos [[attribute(0)]]; float2 uv [[attribute(1)]]; float4 color [[attribute(2)]]; };
struct Out { float4 pos [[position]]; float2 uv; float4 color; float fog; };
struct U { float4x4 mvp; float4 opts; };
vertex Out vs(In in [[stage_in]], constant U& u [[buffer(0)]]) {
  Out o;
  o.pos = u.mvp * float4(in.pos, 1.0);
  o.uv = in.uv;
  o.color = u.opts.x > 0.5 ? in.color : float4(1.0, 1.0, 1.0, in.color.a);
  o.fog = clamp(o.pos.w / u.opts.y, 0.0, 1.0);
  return o;
}
struct F { float4 opts; float4 fog; };
fragment float4 fs_tex(Out in [[stage_in]], texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]],
                       constant F& f [[buffer(0)]]) {
  float4 c = t.sample(s, in.uv) * in.color;
  return float4(mix(c.rgb, f.fog.rgb, in.fog * f.opts.x), c.a);
}
fragment float4 fs_flat(Out in [[stage_in]], constant F& f [[buffer(0)]]) {
  return float4(mix(in.color.rgb, f.fog.rgb, in.fog * f.opts.x), in.color.a);
}
)";

struct Mat4 {
  float m[16];  // column-major, as MSL's float4x4
};

Mat4 mul(const Mat4& a, const Mat4& b) {
  Mat4 r{};
  for (int c = 0; c < 4; ++c)
    for (int rr = 0; rr < 4; ++rr) {
      float s = 0;
      for (int k = 0; k < 4; ++k) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
      r.m[c * 4 + rr] = s;
    }
  return r;
}

// The world is right-handed with y up (docs/drafts/render.md): right = forward
// x up. View space here is x right, y up, z forward, for the projection below.
Mat4 view_matrix(const float* eye, float yaw, float pitch) {
  const float f[3] = {std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
  float r[3] = {-f[2], 0, f[0]};
  const float rl = std::sqrt(r[0] * r[0] + r[2] * r[2]);
  r[0] /= rl, r[2] /= rl;
  const float u[3] = {r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]};
  Mat4 v{};
  // rows: r, u, f
  v.m[0] = r[0], v.m[4] = r[1], v.m[8] = r[2];
  v.m[1] = u[0], v.m[5] = u[1], v.m[9] = u[2];
  v.m[2] = f[0], v.m[6] = f[1], v.m[10] = f[2];
  v.m[12] = -(r[0] * eye[0] + r[1] * eye[1] + r[2] * eye[2]);
  v.m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
  v.m[14] = -(f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2]);
  v.m[15] = 1;
  return v;
}

// Left-handed perspective, depth 0..1.
Mat4 projection(float fovy, float aspect, float zn, float zf) {
  const float y = 1.0f / std::tan(fovy / 2);
  Mat4 p{};
  p.m[0] = y / aspect, p.m[5] = y, p.m[10] = zf / (zf - zn), p.m[11] = 1, p.m[14] = -zn * zf / (zf - zn);
  return p;
}

struct GpuBatch {
  SDL_GPUBuffer* buffer = nullptr;
  uint32_t count = 0;
  int image = -1;
  bool marker = false;
  bool blend = false, two_sided = false, sky = false;
};

struct App {
  SDL_Window* window = nullptr;
  SDL_GPUDevice* dev = nullptr;
  SDL_GPUGraphicsPipeline* tex_pipe[3] = {};   // by Pass
  SDL_GPUGraphicsPipeline* flat_pipe[3] = {};
  SDL_GPUSampler* sampler = nullptr;
  std::vector<SDL_GPUTexture*> textures;
  std::vector<GpuBatch> batches;
  SDL_GPUTexture* depth = nullptr;
  uint32_t dw = 0, dh = 0;
  SDL_GPUTextureFormat color_format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
};

bool fail(const char* what) {
  std::fprintf(stderr, "%s: %s\n", what, SDL_GetError());
  return false;
}

SDL_GPUShader* shader(App& a, const char* entry, SDL_GPUShaderStage stage, int samplers, int uniforms) {
  SDL_GPUShaderCreateInfo info{};
  info.code = reinterpret_cast<const Uint8*>(kShaders);
  info.code_size = std::strlen(kShaders);
  info.entrypoint = entry;
  info.format = SDL_GPU_SHADERFORMAT_MSL;
  info.stage = stage;
  info.num_samplers = samplers;
  info.num_uniform_buffers = uniforms;
  return SDL_CreateGPUShader(a.dev, &info);
}

enum class Pass { kOpaque, kTwoSided, kBlend };

// Front faces are counter-clockwise in file order (render.md, section 3); the
// blended pass is two-sided and writes no depth, as the original's queue.
SDL_GPUGraphicsPipeline* pipeline(App& a, SDL_GPUShader* vs, SDL_GPUShader* fs, Pass pass) {
  SDL_GPUVertexBufferDescription vb{};
  vb.slot = 0, vb.pitch = sizeof(viewer::Vertex), vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  SDL_GPUVertexAttribute attrs[3] = {
      {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(viewer::Vertex, pos)},
      {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(viewer::Vertex, uv)},
      {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(viewer::Vertex, color)}};
  SDL_GPUColorTargetDescription ct{};
  ct.format = a.color_format;
  if (pass == Pass::kBlend) {
    ct.blend_state.enable_blend = true;
    ct.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    ct.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    ct.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    ct.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    ct.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    ct.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  }
  SDL_GPUGraphicsPipelineCreateInfo info{};
  info.vertex_shader = vs, info.fragment_shader = fs;
  info.vertex_input_state.vertex_buffer_descriptions = &vb;
  info.vertex_input_state.num_vertex_buffers = 1;
  info.vertex_input_state.vertex_attributes = attrs;
  info.vertex_input_state.num_vertex_attributes = 3;
  info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  info.rasterizer_state.cull_mode = pass == Pass::kOpaque ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_NONE;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  info.depth_stencil_state.enable_depth_test = true;
  info.depth_stencil_state.enable_depth_write = pass != Pass::kBlend;
  info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
  info.target_info.color_target_descriptions = &ct;
  info.target_info.num_color_targets = 1;
  info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  info.target_info.has_depth_stencil_target = true;
  return SDL_CreateGPUGraphicsPipeline(a.dev, &info);
}

bool upload(App& a, const viewer::Scene& s) {
  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(a.dev);
  SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
  std::vector<SDL_GPUTransferBuffer*> staging;
  auto stage = [&](const void* data, uint32_t size) {
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, size, 0};
    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(a.dev, &ti);
    std::memcpy(SDL_MapGPUTransferBuffer(a.dev, tb, false), data, size);
    SDL_UnmapGPUTransferBuffer(a.dev, tb);
    staging.push_back(tb);
    return tb;
  };
  for (const viewer::Image& img : s.images) {
    SDL_GPUTextureCreateInfo ti{};
    ti.type = SDL_GPU_TEXTURETYPE_2D, ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width = img.width, ti.height = img.height, ti.layer_count_or_depth = 1, ti.num_levels = 1;
    SDL_GPUTexture* t = SDL_CreateGPUTexture(a.dev, &ti);
    SDL_GPUTextureTransferInfo src{stage(img.rgba.data(), static_cast<uint32_t>(img.rgba.size())), 0, 0, 0};
    SDL_GPUTextureRegion dst{};
    dst.texture = t, dst.w = img.width, dst.h = img.height, dst.d = 1;
    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    a.textures.push_back(t);
  }
  for (const viewer::Batch& b : s.batches) {
    if (b.vertices.empty()) continue;
    const auto size = static_cast<uint32_t>(b.vertices.size() * sizeof(viewer::Vertex));
    SDL_GPUBufferCreateInfo bi{SDL_GPU_BUFFERUSAGE_VERTEX, size, 0};
    GpuBatch g{SDL_CreateGPUBuffer(a.dev, &bi), static_cast<uint32_t>(b.vertices.size()), b.image, b.marker,
               b.blend, b.two_sided, b.sky};
    SDL_GPUTransferBufferLocation src{stage(b.vertices.data(), size), 0};
    SDL_GPUBufferRegion dst{g.buffer, 0, size};
    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    a.batches.push_back(g);
  }
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_WaitForGPUIdle(a.dev);
  for (SDL_GPUTransferBuffer* tb : staging) SDL_ReleaseGPUTransferBuffer(a.dev, tb);
  SDL_GPUSamplerCreateInfo si{};
  si.min_filter = si.mag_filter = SDL_GPU_FILTER_LINEAR;
  si.address_mode_u = si.address_mode_v = si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  a.sampler = SDL_CreateGPUSampler(a.dev, &si);
  return true;
}

// Writes a rebuilt (posed) scene's vertices into the existing buffers.
void reupload(App& a, const viewer::Scene& s) {
  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(a.dev);
  SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
  std::vector<SDL_GPUTransferBuffer*> staging;
  size_t k = 0;
  for (const viewer::Batch& b : s.batches) {
    if (b.vertices.empty()) continue;
    if (k >= a.batches.size() || a.batches[k].count != b.vertices.size()) break;
    const auto size = static_cast<uint32_t>(b.vertices.size() * sizeof(viewer::Vertex));
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, size, 0};
    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(a.dev, &ti);
    std::memcpy(SDL_MapGPUTransferBuffer(a.dev, tb, false), b.vertices.data(), size);
    SDL_UnmapGPUTransferBuffer(a.dev, tb);
    SDL_GPUTransferBufferLocation src{tb, 0};
    SDL_GPUBufferRegion dst{a.batches[k].buffer, 0, size};
    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    staging.push_back(tb);
    ++k;
  }
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(cmd);
  for (SDL_GPUTransferBuffer* tb : staging) SDL_ReleaseGPUTransferBuffer(a.dev, tb);
}

struct Camera {
  float eye[3] = {0, 0, 0};
  float yaw = 0, pitch = 0;
  bool colors = true;
  bool markers = false;
};

void ensure_depth(App& a, uint32_t w, uint32_t h) {
  if (a.depth && a.dw == w && a.dh == h) return;
  if (a.depth) SDL_ReleaseGPUTexture(a.dev, a.depth);
  SDL_GPUTextureCreateInfo ti{};
  ti.type = SDL_GPU_TEXTURETYPE_2D, ti.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  ti.width = w, ti.height = h, ti.layer_count_or_depth = 1, ti.num_levels = 1;
  a.depth = SDL_CreateGPUTexture(a.dev, &ti);
  a.dw = w, a.dh = h;
}

void draw(App& a, SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target, uint32_t w, uint32_t h, const Camera& cam,
          float far_plane) {
  ensure_depth(a, w, h);
  SDL_GPUColorTargetInfo ct{};
  ct.texture = target;
  ct.clear_color = {0.35f, 0.45f, 0.55f, 1};
  ct.load_op = SDL_GPU_LOADOP_CLEAR, ct.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPUDepthStencilTargetInfo dt{};
  dt.texture = a.depth, dt.clear_depth = 1, dt.load_op = SDL_GPU_LOADOP_CLEAR, dt.store_op = SDL_GPU_STOREOP_DONT_CARE;
  SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &ct, 1, &dt);
  struct { Mat4 mvp; float opts[4]; } vu{}, sky{};
  const Mat4 vp = mul(projection(1.0f, static_cast<float>(w) / h, 1.0f, far_plane), view_matrix(cam.eye, cam.yaw, cam.pitch));
  vu.mvp = vp;
  Mat4 at_eye{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, cam.eye[0], cam.eye[1], cam.eye[2], 1}};
  sky.mvp = mul(vp, at_eye);  // the horizon follows the camera's position
  vu.opts[0] = sky.opts[0] = cam.colors ? 1.0f : 0.0f;
  vu.opts[1] = sky.opts[1] = far_plane;
  const float fu[8] = {0.6f, 0, 0, 0, 0.35f, 0.45f, 0.55f, 1};
  for (int order = 0; order < 2; ++order)  // opaque first, then blended
  for (const GpuBatch& b : a.batches) {
    if (b.marker && !cam.markers) continue;
    if (b.blend != (order == 1)) continue;
    const bool tex = b.image >= 0;
    const int p = b.blend ? 2 : b.two_sided ? 1 : 0;
    SDL_BindGPUGraphicsPipeline(pass, tex ? a.tex_pipe[p] : a.flat_pipe[p]);
    if (b.sky) SDL_PushGPUVertexUniformData(cmd, 0, &sky, sizeof sky);
    else SDL_PushGPUVertexUniformData(cmd, 0, &vu, sizeof vu);
    SDL_PushGPUFragmentUniformData(cmd, 0, fu, sizeof fu);
    SDL_GPUBufferBinding vb{b.buffer, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
    if (tex) {
      SDL_GPUTextureSamplerBinding tsb{a.textures[b.image], a.sampler};
      SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    }
    SDL_DrawGPUPrimitives(pass, b.count, 1, 0, 0);
  }
  SDL_EndGPURenderPass(pass);
}

bool screenshot(App& a, const Camera& cam, float far_plane, const char* path) {
  const uint32_t w = 1280, h = 720;
  SDL_GPUTextureCreateInfo ti{};
  ti.type = SDL_GPU_TEXTURETYPE_2D, ti.format = a.color_format;
  ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
  ti.width = w, ti.height = h, ti.layer_count_or_depth = 1, ti.num_levels = 1;
  SDL_GPUTexture* target = SDL_CreateGPUTexture(a.dev, &ti);
  SDL_GPUTransferBufferCreateInfo tbi{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, w * h * 4, 0};
  SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(a.dev, &tbi);
  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(a.dev);
  draw(a, cmd, target, w, h, cam, far_plane);
  SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
  SDL_GPUTextureRegion src{};
  src.texture = target, src.w = w, src.h = h, src.d = 1;
  SDL_GPUTextureTransferInfo dst{tb, 0, 0, 0};
  SDL_DownloadFromGPUTexture(copy, &src, &dst);
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_WaitForGPUIdle(a.dev);
  auto* px = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(a.dev, tb, false));
  SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ARGB8888, px, w * 4);
  const bool ok = SDL_SaveBMP(s, path);
  SDL_DestroySurface(s);
  SDL_UnmapGPUTransferBuffer(a.dev, tb);
  SDL_ReleaseGPUTransferBuffer(a.dev, tb);
  SDL_ReleaseGPUTexture(a.dev, target);
  return ok;
}

// The original's projection (render.md section 0): camera space x right, y
// down, z forward; sx = cx + x fx / z, sy = cy + y fy / z, sz = rhw = near / z.
// Triangles with a vertex in front of the near plane are dropped (the real
// clippers are being ported).
void draw_tl(sdlfe::SdlRender3D& r, const viewer::Scene& s, const std::vector<int32_t>& handles, const Camera& cam,
             int w, int h) {
  const float f[3] = {std::cos(cam.pitch) * std::sin(cam.yaw), std::sin(cam.pitch), std::cos(cam.pitch) * std::cos(cam.yaw)};
  float rt[3] = {-f[2], 0, f[0]};
  const float rl = std::sqrt(rt[0] * rt[0] + rt[2] * rt[2]);
  rt[0] /= rl, rt[2] /= rl;
  const float up[3] = {rt[1] * f[2] - rt[2] * f[1], rt[2] * f[0] - rt[0] * f[2], rt[0] * f[1] - rt[1] * f[0]};
  const float near_z = 1.0f, fx = w / 2 / std::tan(0.5f * 1.0f * w / h), fy = h / 2 / std::tan(0.5f);
  r.set_viewport(0, 0, w, h);
  r.clear(true, 89, 115, 140);
  r.begin_scene();
  std::vector<platform::TLVertex> tri(3);
  for (int order = 0; order < 2; ++order)
    for (const viewer::Batch& b : s.batches) {
      if (b.marker && !cam.markers) continue;
      if (b.blend != (order == 1)) continue;
      platform::DrawState st;
      st.texture = b.image >= 0 ? handles[b.image] : -1;
      st.blend = b.blend, st.depth_write = !b.blend, st.alpha_modulate = b.blend;
      for (size_t i = 0; i + 2 < b.vertices.size(); i += 3) {
        bool keep = true;
        for (int k = 0; k < 3 && keep; ++k) {
          const viewer::Vertex& v = b.vertices[i + k];
          float d[3];
          for (int a = 0; a < 3; ++a) d[a] = v.pos[a] - (b.sky ? 0 : cam.eye[a]);
          const float x = d[0] * rt[0] + d[1] * rt[1] + d[2] * rt[2];
          const float y = -(d[0] * up[0] + d[1] * up[1] + d[2] * up[2]);
          const float z = d[0] * f[0] + d[1] * f[1] + d[2] * f[2];
          if (z < near_z) keep = false;
          platform::TLVertex& t = tri[k];
          t.sx = w / 2.0f + x * fx / z, t.sy = h / 2.0f + y * fy / z;
          t.sz = t.rhw = near_z / z;
          auto c8 = [](float c) { return static_cast<uint32_t>(std::fmin(std::fmax(c, 0.0f), 1.0f) * 255 + 0.5f); };
          t.diffuse = c8(v.color[3]) << 24 | c8(v.color[0]) << 16 | c8(v.color[1]) << 8 | c8(v.color[2]);
          t.specular = 0xff000000u;
          t.tu = v.uv[0], t.tv = v.uv[1];
        }
        if (keep) r.draw(platform::Primitive::kFan, tri.data(), 3, st);
      }
    }
  r.end_scene();
  r.present();
}

}  // namespace

int main(int argc, char** argv) {
  const std::string zbd = "zbd";  // a game path, found through the mounts
  std::string mission = "t1", mech, motion_name, shot, variant;
  std::vector<std::string> assets;
  float anim_time = 0;
  bool info = false, have_pos = false, have_look = false, tl = false, real = false;
  Camera cam;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--assets" && i + 1 < argc) assets.emplace_back(argv[++i]);
    else if (arg == "--mission" && i + 1 < argc) mission = argv[++i];
    else if (arg == "--mech" && i + 1 < argc) mech = argv[++i];
    else if (arg == "--motion" && i + 1 < argc) motion_name = argv[++i];
    else if (arg == "--time" && i + 1 < argc) anim_time = std::stof(argv[++i]);
    else if (arg == "--info") info = true;
    else if (arg == "--tl") tl = true;
    else if (arg == "--real") tl = real = true;
    else if (arg == "--variant" && i + 1 < argc) variant = argv[++i];
    else if (arg == "--shot" && i + 1 < argc) shot = argv[++i];
    else if (arg == "--pos" && i + 3 < argc) {
      for (float& v : cam.eye) v = std::stof(argv[++i]);
      have_pos = true;
    } else if (arg == "--look" && i + 2 < argc) {
      cam.yaw = std::stof(argv[++i]) * 0.0174533f, cam.pitch = std::stof(argv[++i]) * 0.0174533f;
      have_look = true;
    }
  }
  if (assets.empty()) assets.emplace_back(MW3_GAME_DIR);
  for (const std::string& dir : assets) zutl::mount(dir);
  viewer::Scene scene;
  viewer::Mech mech_model;
  viewer::Motion motion;
  viewer::Pose pose;
  std::vector<std::string> motion_list;
  size_t motion_at = 0;
  std::string err;
  if (mech.empty()) {
    err = viewer::load_world(zbd, mission, scene);
  } else {
    const std::string base = mech;
    if (mech.find(".flt") == std::string::npos) mech = "mech_" + mech + ".flt";
    err = mech_model.load(zbd, mech);
    motion_list = viewer::motions(zbd, base + "_");
    if (err.empty() && !motion_name.empty()) {
      if (motion_name.find('_') == std::string::npos) motion_name = base + "_" + motion_name;
      err = motion.load(zbd, motion_name);
      for (size_t i = 0; i < motion_list.size(); ++i)
        if (motion_list[i] == motion_name) motion_at = i;
      if (err.empty()) motion.pose(anim_time, pose);
    }
    if (err.empty()) mech_model.build(scene, motion_name.empty() ? nullptr : &pose);
    mission = motion_name.empty() ? mech : motion_name;
  }
  if (!err.empty()) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  std::printf("%s: %d live nodes, %d objects with models (%d bounds markers), %d polygons, %d triangles, %zu textures "
              "(%d missing)\n", mission.c_str(), scene.nodes, scene.objects, scene.markers, scene.polygons,
              scene.triangles, scene.images.size(), scene.missing_textures);
  std::printf("bounds (%.0f %.0f %.0f) .. (%.0f %.0f %.0f)\n", scene.min[0], scene.min[1], scene.min[2],
              scene.max[0], scene.max[1], scene.max[2]);
  if (info) return 0;
  float size = 0;
  for (int k = 0; k < 3; ++k) size = std::fmax(size, scene.max[k] - scene.min[k]);
  const float far_plane = std::fmax(size * 1.5f, 1000.0f);
  // Start inside the sky dome (centred on the origin in every world), a little
  // above the ground, as a pilot would see it.
  if (!have_pos) cam.eye[0] = 0, cam.eye[1] = 60, cam.eye[2] = 0;
  if (!have_look) cam.pitch = -0.05f;
  if (!mech.empty()) {  // a mech: stand back from its bounds, looking at its middle
    const float c[3] = {(scene.min[0] + scene.max[0]) / 2, (scene.min[1] + scene.max[1]) / 2,
                        (scene.min[2] + scene.max[2]) / 2};
    if (!have_pos) cam.eye[0] = c[0] + size * 0.9f, cam.eye[1] = c[1] + size * 0.3f, cam.eye[2] = c[2] + size * 0.9f;
    if (!have_look) {
      const float d[3] = {c[0] - cam.eye[0], c[1] - cam.eye[1], c[2] - cam.eye[2]};
      cam.yaw = std::atan2(d[0], d[2]);
      cam.pitch = std::atan2(d[1], std::sqrt(d[0] * d[0] + d[2] * d[2]));
    }
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) return fail("SDL_Init"), 1;
  App a;
  a.dev = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
  if (!a.dev) return fail("SDL_CreateGPUDevice"), 1;
  a.window = SDL_CreateWindow("mw3view", 1280, 720, SDL_WINDOW_RESIZABLE | (shot.empty() ? 0 : SDL_WINDOW_HIDDEN));
  if (!a.window || !SDL_ClaimWindowForGPUDevice(a.dev, a.window)) return fail("window"), 1;
  a.color_format = SDL_GetGPUSwapchainTextureFormat(a.dev, a.window);
  SDL_GPUShader* vs = shader(a, "vs", SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
  SDL_GPUShader* fs_tex = shader(a, "fs_tex", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
  SDL_GPUShader* fs_flat = shader(a, "fs_flat", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1);
  if (!vs || !fs_tex || !fs_flat) return fail("shaders"), 1;
  for (int p = 0; p < 3; ++p) {
    a.tex_pipe[p] = pipeline(a, vs, fs_tex, static_cast<Pass>(p));
    a.flat_pipe[p] = pipeline(a, vs, fs_flat, static_cast<Pass>(p));
    if (!a.tex_pipe[p] || !a.flat_pipe[p]) return fail("pipeline"), 1;
  }
  upload(a, scene);
  std::unique_ptr<sdlfe::SdlRender3D> r3d;
  std::vector<int32_t> handles;
  if (tl) {
    r3d = std::make_unique<sdlfe::SdlRender3D>(a.dev, a.window);
    if (!r3d->ok()) return 1;
    for (const viewer::Image& img : scene.images)
      handles.push_back(r3d->create_texture(img.width, img.height, img.rgba.data(), img.alpha ? 4 : 1));
  }
  if (real) {
    const std::string e = frontend::world_load(mission, variant, r3d.get());
    if (!e.empty()) {
      std::fprintf(stderr, "%s\n", e.c_str());
      return 1;
    }
  }

  if (!shot.empty() && r3d) {
    r3d->capture_next(shot.c_str());
    if (real) frontend::world_frame(cam.eye, cam.yaw, cam.pitch, r3d.get());
    else draw_tl(*r3d, scene, handles, cam, 1280, 720);
    std::printf("wrote %s\n", shot.c_str());
    return 0;
  }
  if (!shot.empty()) {
    const bool ok = screenshot(a, cam, far_plane, shot.c_str());
    std::printf("%s %s\n", ok ? "wrote" : "failed", shot.c_str());
    return ok ? 0 : 1;
  }

  bool running = true, looking = false, paused = false;
  Uint64 last = SDL_GetTicks();
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) running = false;
      if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) running = false;
      if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_C) cam.colors = !cam.colors;
      if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_B) cam.markers = !cam.markers;
      if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_SPACE) paused = !paused;
      if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_M && !motion_list.empty()) {
        motion_at = motion_name.empty() ? 0 : (motion_at + 1) % motion_list.size();
        motion_name = motion_list[motion_at];
        motion = viewer::Motion{};
        if (motion.load(zbd, motion_name).empty()) {
          anim_time = 0;
          SDL_SetWindowTitle(a.window, ("mw3view: " + motion_name).c_str());
        } else {
          motion_name.clear();
        }
      }
      if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_P)
        std::printf("--pos %.1f %.1f %.1f --look %.1f %.1f\n", cam.eye[0], cam.eye[1], cam.eye[2],
                    cam.yaw / 0.0174533f, cam.pitch / 0.0174533f);
      if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT)
        looking = true, SDL_SetWindowRelativeMouseMode(a.window, true);
      if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_RIGHT)
        looking = false, SDL_SetWindowRelativeMouseMode(a.window, false);
      if (e.type == SDL_EVENT_MOUSE_MOTION && looking) {
        cam.yaw += e.motion.xrel * 0.004f;
        cam.pitch = std::fmax(-1.5f, std::fmin(1.5f, cam.pitch - e.motion.yrel * 0.004f));
      }
    }
    const Uint64 now = SDL_GetTicks();
    const float dt = (now - last) / 1000.0f;
    last = now;
    const bool* k = SDL_GetKeyboardState(nullptr);
    const float speed = (k[SDL_SCANCODE_LSHIFT] ? 2000.0f : 300.0f) * dt * (mech.empty() ? 1.0f : 0.05f);
    const float fwd[3] = {std::cos(cam.pitch) * std::sin(cam.yaw), std::sin(cam.pitch),
                          std::cos(cam.pitch) * std::cos(cam.yaw)};
    const float right[3] = {-std::cos(cam.yaw), 0, std::sin(cam.yaw)};
    for (int i = 0; i < 3; ++i) {
      cam.eye[i] += (k[SDL_SCANCODE_W] - k[SDL_SCANCODE_S]) * fwd[i] * speed;
      cam.eye[i] += (k[SDL_SCANCODE_D] - k[SDL_SCANCODE_A]) * right[i] * speed;
    }
    cam.eye[1] += (k[SDL_SCANCODE_E] - k[SDL_SCANCODE_Q]) * speed;
    if (!motion_name.empty()) {
      if (!paused) anim_time += dt;
      pose.clear();
      motion.pose(anim_time, pose);
      mech_model.build(scene, &pose);
      reupload(a, scene);
    }

    if (r3d) {
      int ww = 0, wh = 0;
      SDL_GetWindowSizeInPixels(a.window, &ww, &wh);
      if (real) frontend::world_frame(cam.eye, cam.yaw, cam.pitch, r3d.get());
      else draw_tl(*r3d, scene, handles, cam, ww, wh);
      continue;
    }
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(a.dev);
    SDL_GPUTexture* swap = nullptr;
    Uint32 w = 0, h = 0;
    if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, a.window, &swap, &w, &h) && swap) draw(a, cmd, swap, w, h, cam, far_plane);
    SDL_SubmitGPUCommandBuffer(cmd);
  }
  SDL_Quit();
  return 0;
}
