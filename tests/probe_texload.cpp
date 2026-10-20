// Probes for test_texload.py (docs/drafts/bake-textures.md): a world's
// texture table loaded by group (0x519080) through the ported libraries, onto
// a recording platform::Render3D.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"

namespace {

struct Upload {
  int32_t width, height, type, handle;
  std::vector<uint8_t> rgba;
};
std::vector<Upload> g_uploads;
std::vector<int32_t> g_reports;

class Recorder : public platform::Render3D {
 public:
  int32_t create_texture(int width, int height, const uint8_t* rgba, uint8_t type) override {
    const int32_t h = static_cast<int32_t>(g_uploads.size());
    g_uploads.push_back({width, height, type, h,
                         std::vector<uint8_t>(rgba, rgba + static_cast<size_t>(width) * height * 4)});
    return h;
  }
  void release_texture(int32_t) override {}
  void update_texture(int32_t, int, int, const uint8_t*) override {}
  void set_viewport(int, int, int, int) override {}
  void clear(bool, uint8_t, uint8_t, uint8_t) override {}
  void begin_scene() override {}
  void end_scene() override {}
  void draw(platform::Primitive, const platform::TLVertex*, int, const platform::DrawState&) override {}
  void set_fog_color(uint8_t, uint8_t, uint8_t) override {}
  void present() override {}
};
Recorder g_recorder;
zvid::D3DTexture g_fallback{0x55, 1, 1, 1};

void sink(int level, const char*, int line, const char*) {
  g_reports.push_back(level);
  g_reports.push_back(line);
}

template <typename T> void put(std::vector<uint8_t>& b, T v) {
  const auto* p = reinterpret_cast<const uint8_t*>(&v);
  b.insert(b.end(), p, p + sizeof v);
}

}  // namespace

extern "C" {

// Reads the texture directory of `gamez` (group 0), registers the libraries
// as mission load does ("texture" with `variant`, "mechtex" or "mechtexS",
// then "image"; r prefix = `hardware`), and runs load_group(group) with
// Direct3D `device`. Libraries are found in world_dir, then zbd_dir. display:
// max width, max height, caps, 4444 support. Serialised per entry: name[20],
// state, group, mip index (-1 none), image kind (-1 none, -2 default, else the
// first entry index with that image), surface (-1 none, -2 default record,
// else the upload index); then the image fields (see the test) and the record.
// Returns the length, or < 0.
int32_t probe_texload(const char* gamez, const char* world_dir, const char* zbd_dir, int32_t variant,
                      int32_t low_detail, int32_t hardware, int32_t group, int32_t device,
                      const int32_t* display, uint8_t* out, int32_t cap) {
  g_uploads.clear(), g_reports.clear();
  static const zimg::Texture pristine = zimg::default_image();  // software runs prepare it in place
  zimg::default_image() = pristine;
  zimg::TextureTable& T = zimg::textures();
  std::memset(T.entries, 0, sizeof T.entries);
  T.count = 0, T.group = 0;
  std::FILE* f = std::fopen(gamez, "rb");
  if (!f) return -1;
  uint32_t hdr[9];
  if (std::fread(hdr, 4, 9, f) != 9) return -2;
  std::fseek(f, static_cast<long>(hdr[3]), SEEK_SET);
  zerr::set_sink(sink);
  const int n = zimg::read_texture_directory(static_cast<int>(hdr[2]), f);
  std::fclose(f);
  if (n < 0) return -3;

  zimg::state() = zimg::State{};
  zimg::state().display_format = 6;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(std::string(world_dir) + "/");
  zutl::mount(std::string(zbd_dir) + "/");
  for (int l = 0; l < 2; ++l) zimg::library_list(l).clear();
  zimg::new_library("texture", variant, hardware != 0);
  zimg::new_library(low_detail ? "mechtexS" : "mechtex", 0, hardware != 0);
  zimg::register_image_library(1);
  zimg::texture_load() = zimg::TextureLoad{};
  zimg::texture_hooks() = zimg::TextureHooks{};

  zrndr::device_fog().active = device;
  zvid::set_render3d(&g_recorder);
  zvid::default_texture() = &g_fallback;
  zvid::Display& d = zvid::display();
  d.max_width = static_cast<uint32_t>(display[0]), d.max_height = static_cast<uint32_t>(display[1]);
  d.caps = static_cast<uint32_t>(display[2]), d.alpha_4444 = display[3];
  zvid::set_pixel_format(5, 6, 5);
  zvid::channel_masks() = {0xf800, 0x07e0, 0x001f};

  zimg::load_group(group);
  zerr::set_sink(nullptr);

  std::vector<uint8_t> b;
  put<int32_t>(b, T.count), put<int32_t>(b, zimg::texture_load().list), put<int32_t>(b, zimg::texture_load().memory);
  put<int32_t>(b, static_cast<int32_t>(g_reports.size() / 2));
  for (int32_t r : g_reports) put<int32_t>(b, r);
  for (int32_t i = 0; i < T.count; ++i) {
    const zimg::TextureEntry& e = T.entries[i];
    b.insert(b.end(), e.name, e.name + 20);
    put<int32_t>(b, e.state), put<int32_t>(b, e.group);
    put<int32_t>(b, e.mip ? static_cast<int32_t>(e.mip - T.entries) : -1);
    int32_t kind = -1;
    if (e.image == &zimg::default_image()) kind = -2;
    else if (e.image)
      for (int32_t k = 0; k <= i; ++k)
        if (T.entries[k].image == e.image) { kind = k; break; }
    put<int32_t>(b, kind);
    const auto* rec = static_cast<const zvid::D3DTexture*>(e.surface);
    put<int32_t>(b, !rec ? -1 : rec == &g_fallback ? -2 : rec->handle);
    if (e.image) {
      const auto& t = *static_cast<const zimg::Texture*>(e.image);
      put<int32_t>(b, t.pixel_count), put<int16_t>(b, t.width), put<int16_t>(b, t.height);
      put<uint8_t>(b, t.unk_08), put<uint8_t>(b, t.flags), put<uint8_t>(b, t.log2_width), put<uint8_t>(b, t.log2_height);
      put<int16_t>(b, t.stretch), put<int16_t>(b, t.palette_count);
      put<float>(b, t.mip_scale), put<int32_t>(b, t.span_shift), put<int32_t>(b, t.span_mask_u);
      put<int32_t>(b, t.span_mask_v), put<int32_t>(b, t.width_again);
      put<uint8_t>(b, t.pixels != nullptr), put<uint8_t>(b, t.alpha != nullptr), put<uint8_t>(b, t.palette != nullptr);
    }
    if (rec && rec != &g_fallback) put<int32_t>(b, rec->type), put<int32_t>(b, rec->address_u), put<int32_t>(b, rec->address_v);
  }
  if (static_cast<int32_t>(b.size()) > cap) return -4;
  std::memcpy(out, b.data(), b.size());
  return static_cast<int32_t>(b.size());
}

// Upload i of the last run: width, height, type in info; its RGBA to rgba.
int32_t probe_texload_upload(int32_t i, int32_t* info, uint8_t* rgba, int32_t cap) {
  if (i < 0 || i >= static_cast<int32_t>(g_uploads.size())) return -1;
  const Upload& u = g_uploads[i];
  info[0] = u.width, info[1] = u.height, info[2] = u.type;
  const int32_t n = static_cast<int32_t>(u.rgba.size());
  if (n > cap) return -2;
  std::memcpy(rgba, u.rgba.data(), n);
  return n;
}

int32_t probe_texload_uploads() { return static_cast<int32_t>(g_uploads.size()); }

}  // extern "C"
