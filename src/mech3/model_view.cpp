#include "mech3/model_view.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <strings.h>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zclass/window.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "mech3/app_engine.h"
#include "mech3/mech_reader.h"
#include "mech3/mech_skin.h"
#include "mech3/mission.h"
#include "mech3/options.h"

namespace mech3 {

zcls::Node* mech_model(int32_t roster, int32_t variant);  // 0x00484760 (mech_lib.h, as lab_view.cpp declares it)

namespace {
// The Mech Lab's camera (lab_view.cpp's kCameraPos and kCameraRot, [0x005e4648] and [0x005e4678]) and its vertical
// field of view ([0x005e476c]); the horizontal one follows the rect's shape.
constexpr float kLabCameraPos[3] = {0.0f, 5.0f, -11.0f};
constexpr float kLabCameraRot[3] = {0.02f, 3.1415927f, 0.0f};
constexpr double kFovV = 75.0;
constexpr double kRad = 0.017453292519943295;

double ms_since(std::chrono::steady_clock::time_point t) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count();
}
}  // namespace

ModelView::~ModelView() = default;  // close() frees the engine's objects; past the engine's life they are its

bool ModelView::open(GameEngine& en) {
  if (world_) return true;
  if (en.video_mode() == 0) return false;
  const auto t0 = std::chrono::steady_clock::now();
  if (zcls::nodes().blocks.empty()) {  // as lab_world_load: no pool up (no mission loaded), the lab's world 6
    own_world_ = true;
    missions().load_world(6);
  }
  world_ = zcls::world_create();
  zcls::node_set_name(world_, "mw1viewworld");
  window_ = zcls::window_create();
  zcls::node_set_name(window_, "mw1viewwindow");
  zcls::window_set_size(window_, 0, 0);
  zcls::window_set_origin(window_, 0, 0);
  camera_ = zcls::camera_create();
  zcls::node_set_name(camera_, "mw1viewcam");
  zcls::camera_set_window(camera_, window_);
  zcls::camera_set_world(camera_, world_);
  hardware_ = en.video_mode();
  const uint8_t zone[4] = {0x01, 0xff, 0xff, 0xff};
  zcls::camera_set_zone(camera_, zone);
  zcls::camera_set_clip(camera_, 1.0f, 500.0f);
  zcls::world_set_origin(world_, -1024.0f, 1024.0f);
  zcls::world_set_size(world_, 2048.0f, -2048.0f);
  zcls::world_make_grid(world_, 2048.0f, -2048.0f);
  zcls::node_add_child(world_, camera_);
  zcls::frame_update();
  zcls::purge_lists();
  open_ms_ = ms_since(t0);
  std::printf("mw3: model view: node pool %s\n", own_world_ ? "loaded (world 6)" : "already up");
  return true;
}

void ModelView::close(GameEngine& en) {
  for (auto& [mech, m] : models_) {
    if (m.node) zcls::tree_destroy(m.node);
    delete m.skin;
  }
  models_.clear();
  world_ = window_ = camera_ = nullptr;  // their nodes stay in the pool, as the lab's do
  own_world_ = false;
  en.model_purge(1, 1);
}

bool ModelView::prepare(GameEngine& en, int32_t mech, const std::string& skin) {
  if (!world_) return false;
  Model& m = models_[mech];
  if (m.node) return true;
  const auto t0 = std::chrono::steady_clock::now();
  zcls::Node* model = mech_model(mech, 0);
  if (model) m.node = zcls::tree_copy(model, 1, 1);
  if (!m.node) return false;
  MechDfn* d = mech_dfn(mech);
  if (d && d->skins) {  // as lab_model_node, then lab_model_skin's change to the named camo
    d->skins->apply_first(model);
    m.skin = d->skins->mech_skin(0, m.node);
    m.shown = 0;
    const std::vector<Skin*>& skins = d->skins->skins;
    int32_t want = 0;
    for (size_t i = 0; i < skins.size(); ++i)
      if (skins[i] && skins[i]->name && strcasecmp(skins[i]->name, skin.c_str()) == 0) want = static_cast<int32_t>(i);
    if (want != 0 && m.skin) {
      en.video_begin_3d();
      en.video_end_3d();
      Skin* copy = skins[static_cast<size_t>(want)]->clone(d->skins->link_damage());
      if (copy) {
        m.skin->apply(m.node, copy);
        delete m.skin;
        m.skin = copy, m.shown = want;
      }
    }
  }
  show_ms_ = ms_since(t0);
  return true;
}

void ModelView::begin_frame(GameEngine& en) {
  en.lab_view_begin(0);
  en.lab_clear();
}

bool ModelView::draw(GameEngine& en, int32_t mech, int32_t x, int32_t y, int32_t w, int32_t h, float yaw) {
  auto it = models_.find(mech);
  if (!world_ || it == models_.end() || !it->second.node || w <= 0 || h <= 0) return false;
  zcls::Node* node = it->second.node;
  zcls::window_set_size(window_, w, h);
  zcls::window_set_origin(window_, x, y);
  const double v = kFovV * kRad;
  const double hz = 2.0 * std::atan(std::tan(v / 2.0) * w / h);
  zcls::camera_set_fov_angles(camera_, static_cast<float>(hz), static_cast<float>(v));
  zcls::camera_set_position(camera_, kLabCameraPos[0] + offset_[0], kLabCameraPos[1] + offset_[1], kLabCameraPos[2] + offset_[2]);
  zcls::camera_set_rotation(camera_, kLabCameraRot[0], kLabCameraRot[1], kLabCameraRot[2]);
  zcls::object3d_set_rotation(node, 0.0f, yaw * static_cast<float>(kRad), 0.0f);
  zcls::frame_update();  // the changed nodes' matrices (the Lab State's update does it each frame)
  zmod::Zones& zones = zmod::zones();
  const int32_t mode = zones.mode;
  zones.mode = 0;
  zcls::render_subtree(camera_, node, hardware_);
  zones.mode = mode;
  return true;
}

void ModelView::set_camera_offset(const float offset[3]) {
  for (int i = 0; i < 3; ++i) offset_[i] = offset[i];
}

}  // namespace mech3
