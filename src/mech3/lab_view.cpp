#include "mech3/lab_view.h"

#include <cstring>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zclass/window.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"
#include "mech3/app_engine.h"
#include "mech3/app_states.h"
#include "mech3/mech_reader.h"
#include "mech3/mech_skin.h"
#include "mech3/mission.h"
#include "mech3/options.h"

#pragma STDC FP_CONTRACT OFF

// The Mech Lab's 3D view on the Lab State (docs/mechlab.md section 9).
namespace mech3 {

// mech_lib.h's (its header brings the mech's own Handle with it).
zcls::Node* mech_model(int32_t roster, int32_t variant);  // 0x00484760

namespace {
constexpr int32_t kMechs = 0x13;                          // the 19 chassis
constexpr int32_t kViewRect[4] = {0xd2, 0x34, 0x1ae, 0x120};  // [0x005e4688]: 210, 52 to 430, 288
constexpr float kCameraPos[3] = {0.0f, 5.0f, -11.0f};    // [0x005e4648]
constexpr float kCameraRot[3] = {0.02f, 3.1415927f, 0.0f};  // [0x005e4678]
constexpr float kFovH = 74.0f, kFovV = 75.0f;             // [0x005e4768], [0x005e476c] degrees
constexpr float kTurn = -0.8f;                            // [0x005c1ab0] per second, subtracted
uint8_t g_frozen = 0;                                     // [0x00605aa4] never set: the view always turns
uint8_t g_doubled = 0;                                    // [0x00605aa8] never set

float from_bits(uint32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
}  // namespace

// Mech3.exe 1.2 @ 0x0044ffc0 (ECX the Lab State): the 3D view made. With no
// node pool up (the lab opened with no mission loaded) the mission world 6
// is loaded first. The frame clear's flag and the cursor are kept, the clear
// colour black, then the world ("labworld", 0x004b5020), its window
// ("labwindow", 0x004b5440) and camera ("labcam", 0x004b5150: the window,
// the world, the default zone), the camera's clip and field of view, the
// world's area and a single 2048-unit cell, the camera put under the world
// (the world's vt+0, 0x0040bab0), the window placed (0x00450460), the 19
// chassis entries (0x0044fcc0), the camera's object test off and the
// camera's place from the constants.
bool GameEngine::lab_world_load(Handle) {
  LabState& ls = states().lab;
  if (zcls::nodes().blocks.empty()) {  // 0x004f8f50
    ls.own_world_ = 1;
    missions().load_world(6);
  }
  ls.world_ = static_cast<uint32_t>(video_mode());
  ls.clear_ = zvid::display().clear_on;  // 0x0057a3c0(0): the old flag back
  zvid::display().clear_on = 0;
  ls.cursor_ = video_show_cursor(0);
  twod_set_cursor(0);                     // 0x0059fb80
  zvid::set_clear_color(0);               // 0x00579250
  lab_clear();                            // 0x00576cf0
  video_clear(0);                         // 0x00576dd0
  lab_view_ = std::make_unique<LabView>();
  LabView& v = *lab_view_;
  v.world = zcls::world_create();
  zcls::node_set_name(v.world, "labworld");
  v.window = zcls::window_create();
  zcls::node_set_name(v.window, "labwindow");
  zcls::window_set_size(v.window, 0, 0);  // its area empty until placed
  zcls::window_set_origin(v.window, 0, 0);
  v.camera = zcls::camera_create();
  zcls::node_set_name(v.camera, "labcam");
  zcls::camera_set_window(v.camera, v.window);
  zcls::camera_set_world(v.camera, v.world);
  v.hardware = video_mode();
  const uint8_t zone[4] = {0x01, 0xff, 0xff, 0xff};      // [0x005e8854]
  zcls::camera_set_zone(v.camera, zone);
  zcls::camera_set_clip(v.camera, 1.0f, 500.0f);        // 0x3f800000, 0x43fa0000
  zcls::camera_set_fov_angles(v.camera, from_bits(0x3fa5514d), from_bits(0x3fa78d36));  // 74, 75 degrees
  zcls::world_set_origin(v.world, -1024.0f, 1024.0f);
  zcls::world_set_size(v.world, 2048.0f, -2048.0f);
  zcls::world_make_grid(v.world, 2048.0f, -2048.0f);
  zcls::node_add_child(v.world, v.camera);
  lab_place_window();
  ls.x40_ = 1;
  zcls::frame_update();                   // 0x004f7f70
  zcls::purge_lists();                    // 0x004f7bd0
  for (int32_t i = 0; i < kMechs; ++i) {  // 0x0044fcc0(i) into +0x20
    const Handle h = alloc_block(0x14);
    set_field32(h, 0x08, static_cast<uint32_t>(i));
    set_field32(h, 0x10, 0xffffffffu);
    ls.models_.push_back(h);
    v.models[h] = LabModel{};
  }
  // 0x004f3540 (with HWCardFlag) writes [0x005ea53c], which nothing reads.
  zcls::scene_walk().hidden_surface = 0;  // 0x004f3550(0)
  std::memcpy(ls.cam_pos_, kCameraPos, sizeof kCameraPos);
  std::memcpy(ls.cam_rot_, kCameraRot, sizeof kCameraRot);
  return true;
}

// 0x004f7f70 (the Lab State's update): the changed nodes' frame update.
void GameEngine::node_frame_update() { zcls::frame_update(); }

// Mech3.exe 1.2 @ 0x00450460 with 0x0044fea0: the window over the lab
// dialog's hole, its area its target (no scaling), the node sized and placed.
void GameEngine::lab_place_window() {
  if (!lab_view_) return;
  zcls::window_set_size(lab_view_->window, kViewRect[2] - kViewRect[0], kViewRect[3] - kViewRect[1]);
  zcls::window_set_origin(lab_view_->window, kViewRect[0], kViewRect[1]);
}

// Mech3.exe 1.2 @ 0x00450390: the entries freed (0x0044fce0: the tree, the
// camo), the world and the camera's objects deleted (their nodes stay in the
// pool, as in the exe), the shown one forgotten, the cursor back, the model
// cache purged.
void GameEngine::lab_world_unload(Handle) {
  LabState& ls = states().lab;
  if (lab_view_) {
    for (auto& [h, m] : lab_view_->models) {
      if (m.node) zcls::tree_destroy(m.node);
      delete m.skin;
      m = LabModel{};
    }
  }
  ls.models_.clear();
  lab_view_.reset();
  ls.model_ = 0;
  video_show_cursor(ls.cursor_);
  twod_set_cursor(ls.cursor_);
  if (ls.own_world_) ls.own_world_ = 0;
  model_purge(1, 1);  // 0x005394c0(1, 1)
}

// Mech3.exe 1.2 @ 0x00450810: the render target the 3D view draws to.
void GameEngine::lab_view_begin(Handle) {
  const Handle back = video_back_buffer();
  const Handle depth = display_depth();
  render_set_target(video_target(), video_screen_rect(), depth, back);
}

// Mech3.exe 1.2 @ 0x00450800: with the world up the frame cleared (0x00576cf0).
void GameEngine::lab_view_end(Handle) {
  if (states().lab.world_) lab_clear();
}

// 0x00576cf0 with the colour 0x00579250 set (the lab's black).
void GameEngine::lab_clear() {
  const uint32_t c = zvid::clear_color();
  if (p_.render) p_.render->clear(true, static_cast<uint8_t>(c >> 16), static_cast<uint8_t>(c >> 8),
                                  static_cast<uint8_t>(c));
}

// Mech3.exe 1.2 @ 0x0044fd20 (ECX the entry): the chassis' tree, copied from
// its model (0x00484760, variant 0) the first time, with the definition's
// first camo on the model and its copy on the tree (shown 0).
zcls::Node* GameEngine::lab_model_node(Handle entry) {
  if (!lab_view_ || !entry) return nullptr;
  LabModel& m = lab_view_->models[entry];
  if (!m.node) {
    const int32_t mech = static_cast<int32_t>(field32(entry, 0x08));
    zcls::Node* model = mech_model(mech, 0);
    if (model) m.node = zcls::tree_copy(model, 1, 1);
    if (m.node) {
      MechDfn* d = mech_dfn(mech);
      if (d && d->skins) {
        d->skins->apply_first(model);
        m.skin = d->skins->mech_skin(0, m.node);
        m.shown = 0;
        set_field32(entry, 0x10, 0);
      }
    }
  }
  return m.node;
}

// Mech3.exe 1.2 @ 0x0044fd90 (ECX the entry): camo `index` on the shown tree
// (a copy for this tree, with the damage skin's holes; the old copy freed).
void GameEngine::lab_model_skin(Handle entry, int32_t index) {
  zcls::Node* node = lab_model_node(entry);
  if (!lab_view_ || !node) return;
  LabModel& m = lab_view_->models[entry];
  if (!m.skin || index == m.shown) return;
  MechDfn* d = mech_dfn(static_cast<int32_t>(field32(entry, 0x08)));
  if (!d || !d->skins) return;
  if (video_mode() != 0) {  // [0x005e4640] is 1
    video_begin_3d();
    video_end_3d();
  }
  const std::vector<Skin*>& skins = d->skins->skins;
  Skin* from = index >= 0 && index < static_cast<int32_t>(skins.size()) ? skins[static_cast<size_t>(index)] : nullptr;
  Skin* damage = d->skins->link_damage();
  // The exe clones from a null skin for an index out of range; none is asked.
  Skin* copy = from ? from->clone(damage) : nullptr;
  if (!copy) return;
  m.skin->apply(node, copy);  // 0x004749e0: true
  delete m.skin;
  m.skin = copy;
  m.shown = index;
  set_field32(entry, 0x10, static_cast<uint32_t>(index));
}

// Mech3.exe 1.2 @ 0x00450690 (ECX the Lab State; the frame's time): the shown
// chassis turned about y by 0.8 radians a second (back by 2 pi past pi), the
// camera's view, place and rotation set, and the chassis drawn alone from
// the camera (0x004f73f0) with the zone test off.
void GameEngine::lab_world_frame(Handle, float dt) {
  LabState& ls = states().lab;
  zcls::Node* node = ls.model_ ? lab_model_node(ls.model_) : nullptr;
  if (!node || g_frozen || !lab_view_) return;
  // The exe doubles the pixels after the turn, before the draw; the flag
  // ([0x00605aa8]) is never set.
  if (g_doubled && pixel_double()) video_double(0, 0);
  lab_view_frame(*lab_view_, node, ls.cam_pos_, ls.cam_rot_, dt, zcls::render_subtree);
}

// 0x00450690's work on the view (its tests drive it with their own draw).
void lab_view_frame(const LabView& v, zcls::Node* node, const float pos[3], const float rot[3], float dt,
                    LabDraw draw) {
  const double rad = 0.017453292519943295;  // [0x005c1aa8]
  zcls::camera_set_fov_angles(v.camera, static_cast<float>(kFovH * rad), static_cast<float>(kFovV * rad));
  zcls::camera_set_position(v.camera, pos[0], pos[1], pos[2]);
  zcls::camera_set_rotation(v.camera, rot[0], rot[1], rot[2]);
  float x = 0.0f, y = 0.0f, z = 0.0f;
  zcls::object3d_get_rotation(node, &x, &y, &z);
  y = y - dt * kTurn;
  if (y > 3.1415927f) y = y - 6.2831855f;  // [0x005c1ab4], [0x005c1ab8]
  zcls::object3d_set_rotation(node, x, y, z);
  zmod::Zones& zones = zmod::zones();
  const int32_t mode = zones.mode;  // [0x005ea1d0]
  zones.mode = 0;
  draw(v.camera, node, v.hardware);
  zones.mode = mode;
}

// Mech3.exe 1.2 @ 0x00450470
void lab_highlight_part(zcls::Node* tree, const char* part, float weight, bool on) {
  static const float kGreen[3] = {0.0f, 1.0f, 0.0f};  // [0x005e4758]
  static const float kNone[3] = {0.0f, 0.0f, 0.0f};   // [0x00605a98]
  if (!tree) return;
  zcls::Node* n = zcls::node_find_in_tree(tree, part);
  if (!n) return;
  zcls::object3d_use_blend(n, on ? 1 : 0);
  if (on) zcls::object3d_set_blend(n, kGreen, weight);
  else zcls::object3d_set_blend(n, kNone, 0.0f);
}

// Mech3.exe 1.2 @ 0x004504d0
void lab_highlight(zcls::Node* tree, int32_t section) {
  constexpr float kOn = 0.3f;  // [0x005e4764]
  static const char* const kParts[8] = {"torso", "larm", "rarm", "lthigh", "lfoot", "rthigh", "rfoot", "head"};
  for (const char* p : kParts) lab_highlight_part(tree, p, 0.0f, false);
  switch (section) {
    case 1: case 7: case 8:
      lab_highlight_part(tree, "torso", kOn, true);
      lab_highlight_part(tree, "larm", 0.0f, true);
      lab_highlight_part(tree, "rarm", 0.0f, true);
      lab_highlight_part(tree, "head", 0.0f, true);
      break;
    case 2: lab_highlight_part(tree, "larm", kOn, true); break;
    case 3: lab_highlight_part(tree, "rarm", kOn, true); break;
    case 4:
      lab_highlight_part(tree, "lthigh", kOn, true);
      lab_highlight_part(tree, "lfoot", kOn, true);
      break;
    case 5:
      lab_highlight_part(tree, "rthigh", kOn, true);
      lab_highlight_part(tree, "rfoot", kOn, true);
      break;
    case 6: lab_highlight_part(tree, "head", kOn, true); break;
    default: break;
  }
}

// 0x004504d0 on the Lab State: its shown chassis' tree (0x0044fd20).
void GameEngine::lab_section_highlight(int32_t section) {
  LabState& ls = states().lab;
  lab_highlight(ls.model_ ? lab_model_node(ls.model_) : nullptr, section);
}

}  // namespace mech3
