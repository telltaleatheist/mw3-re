#pragma once
#include <cstdint>
#include <map>

#include "gamez/zclass/cls_node.h"

// mech3: the Mech Lab's 3D view (0x0044fa30..0x00450810, between
// mech3\mech_data.cpp and mech3\mech_map.cpp; the file is not named): the
// lab world, its window and camera, and the chassis turning in it
// (docs/mechlab.md section 9).
namespace mech3 {

struct Skin;

// One chassis of the view (new(0x14), 0x0044fcc0): its block holds the mech
// at +0x08 (LabState::model_index reads it); the rest lives here.
struct LabModel {
  zcls::Node* node = nullptr;      // +0x04  its tree, copied on first use (0x0044fd20)
  Skin* skin = nullptr;            // +0x0c  the camo on it (0x00475a80)
  int32_t shown = -1;              // +0x10  the camo's index, -1 none
};

struct LabView {
  zcls::Node* world = nullptr;     // +0x14's +0x04  "labworld" (0x004b5020)
  zcls::Node* window = nullptr;    // +0x18's +0x08 +0x04  "labwindow" (0x004b5440)
  zcls::Node* camera = nullptr;    // +0x18's +0x04  "labcam" (0x004b5150)
  int32_t hardware = 0;            // +0x18's +0x14
  std::map<uint32_t, LabModel> models;  // by the entry's block
};

// 0x00450690's work (the Lab State's checks left to GameEngine): the camera
// from the constants, the node turned by the frame's time, then `draw`
// (render_subtree, 0x004f73f0) with the zone test off.
using LabDraw = int (*)(zcls::Node* camera, zcls::Node* node, int32_t hardware);
void lab_view_frame(const LabView& v, zcls::Node* node, const float pos[3], const float rot[3], float dt,
                    LabDraw draw);

}  // namespace mech3
