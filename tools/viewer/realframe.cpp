#include "realframe.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zmodel/gmod_init.h"
#include "gamez/zmodel/gmod_light.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/ztimer.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"
#include "platform/render3d.h"

namespace viewer {
namespace {

zcls::Node* camera_node() {
  zcls::ListItem* item = zcls::list_head(zcls::kCameras);
  return item ? item->node : nullptr;
}

zvid::D3DTexture g_fallback{};  // the default record (0x0080931c) for textures that fail

}  // namespace

std::string real_load(const std::string& mission, platform::Render3D* render) {
  // Fresh pools, as a mission load starts (0x4ff520's callers).
  zimg::TextureTable& t = zimg::textures();
  std::memset(t.entries, 0, sizeof t.entries);
  t.count = 0, t.group = 0;
  zmod::materials() = zmod::MaterialTable{};
  zmod::models() = zmod::ModelPool{};
  zcls::nodes() = zcls::NodePool{};
  zcls::nodes().free_head = -1;
  zcls::lists() = zcls::Lists{};
  zmod::fog() = zmod::Fog{};
  zrndr::fog_color() = zrndr::FogColor{};
  zvid::set_pixel_format(5, 6, 5);
  zvid::channel_masks() = {0xf800, 0x07e0, 0x001f};
  zrndr::device_fog().active = 1;  // the Direct3D path
  zerr::set_sink(nullptr);
  const std::string path = "zbd\\" + mission + "\\gamez.zbd";
  if (zcls::read_zbd(path.c_str()) != 0) return "could not read " + path;
  if (!camera_node()) return "no camera in " + path;
  // The world files keep a 320x200 window; the game resizes it to the display
  // mode at startup (the window setter is not ported yet). A 640x480 mode:
  for (zcls::ListItem* i = zcls::list_head(zcls::kWindows); i; i = i->next)
    if (i->node && i->node->data) {
      auto* w = static_cast<zcls::Window*>(i->node->data);
      w->put<int32_t>(0x0, 0), w->put<int32_t>(0x4, 0), w->put<int32_t>(0x8, 640), w->put<int32_t>(0xc, 480);
    }
  render->set_viewport(0, 0, 640, 480);

  // The libraries as mission load registers them (0x48fcfe): the mission's
  // "texture", "mechtex", then "image", r-prefixed on hardware, found through
  // the mounts: each asset root's zbd/<mission>/ and zbd/ are added.
  zimg::state() = zimg::State{};
  zimg::state().display_format = 6;
  const std::vector<std::string> roots = zutl::mounts().read;
  for (const std::string& r : roots) zutl::mount(r + "/zbd/" + mission);
  for (const std::string& r : roots) zutl::mount(r + "/zbd");
  for (int l = 0; l < 2; ++l) zimg::library_list(l).clear();
  zimg::new_library("texture", 0, true);
  zimg::new_library("mechtex", 0, true);
  zimg::register_image_library(1);
  zimg::texture_load() = zimg::TextureLoad{};
  zimg::texture_hooks() = zimg::TextureHooks{};
  zvid::set_render3d(render);
  zvid::default_texture() = &g_fallback;
  zimg::load_group(-1);

  // The draw table and the walk (0x579f50; [0x74a2e0] = 0x53f2e0).
  zvid::install_model_draw_table();
  zcls::scene_walk() = zcls::SceneWalk{};
  zmod::walk_state() = zmod::WalkState{};
  zmod::walk_state().draw = zmod::draw_model;
  zmod::zones() = zmod::Zones{};
  zmod::shadow_casters() = zmod::ShadowCasters{};
  zmod::draw_state() = zmod::DrawState{};
  zmod::cull_hooks() = zmod::CullHooks{};
  zcls::update_hooks() = zcls::UpdateHooks{};
  zcls::WorldRenderHooks& h = zcls::world_render_hooks();
  h = zcls::WorldRenderHooks{};
  h.begin_scene = [](void*) { zvid::begin_scene(); };
  h.end_scene = [](void*) { zvid::end_scene(); };
  h.flush_transparent = [](void*) { zvid::flush_transparent(); };
  h.flush_overlay = [](void*) { zvid::flush_overlay(); };
  zmth::reset_stack();
  return {};
}

void real_frame(const float eye[3], float yaw, float pitch, platform::Render3D* render) {
  zcls::Node* cam = camera_node();
  if (!cam) return;
  // The camera looks along -z at zero rotation (render.md section 0); the
  // viewer's yaw 0 looks along +z, hence the half turn. Pitch about x, up
  // positive for the viewer, so negated.
  zcls::camera_set_rotation(cam, -pitch, yaw + 3.14159265f, 0.0f);
  zcls::camera_set_position(cam, eye[0], eye[1], eye[2]);
  ++zcls::frames().frame;
  ztimer::state().dt = 1.0f / 60;
  render->clear(true, 89, 115, 140);
  zcls::render_world(cam, 0);
  render->present();
}

}  // namespace viewer
