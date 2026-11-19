#pragma once
// Ours (MW1: Critical Hit, FIXES.md item 14's change, Owen 2026-11-18): a mech's own model drawn live in a rectangle
// of any screen, as the Mech Lab draws its chassis (lab_view.h) but held by no state: its own world, window and camera,
// the chassis' tree copied once and skinned by a camo's name, drawn alone from the front with the zone test off.
// Opt-in: only a program that makes one draws one; MW3's and Pirate's Moon's states never do.
#include <cstdint>
#include <map>
#include <string>

namespace zcls { struct Node; }

namespace mech3 {

class GameEngine;
struct Skin;

class ModelView {
 public:
  ~ModelView();
  // The view made, the node pool up first when none is (the lab's mission world 6, as lab_world_load). False when
  // the engine draws no 3D (the software renderer).
  bool open(GameEngine& en);
  void close(GameEngine& en);
  bool is_open() const { return world_ != nullptr; }
  // `mech` made ready in the camo named `skin` (any case; the first when it has none of that name): its tree copied
  // and skinned once, kept till close. False when the mech has no model. One view draws any number of its mechs a
  // frame (the Company page's slots).
  bool prepare(GameEngine& en, int32_t mech, const std::string& skin);
  // The frame's render target set and cleared black (lab_view_begin and _end's work): before the 2D is drawn.
  void begin_frame(GameEngine& en);
  // `mech` (prepared) in the render-pixel rect (x, y, w, h), turned `yaw` degrees from the front: after the 2D.
  bool draw(GameEngine& en, int32_t mech, int32_t x, int32_t y, int32_t w, int32_t h, float yaw);
  // The camera's place (the lab's, plus `offset`).
  void set_camera_offset(const float offset[3]);
  // How long the last open's node pool load and the last prepare took, in milliseconds (the spike's timing).
  double open_ms() const { return open_ms_; }
  double show_ms() const { return show_ms_; }

 private:
  struct Model {
    zcls::Node* node = nullptr;
    Skin* skin = nullptr;
    int32_t shown = -1;
  };
  zcls::Node* world_ = nullptr;
  zcls::Node* window_ = nullptr;
  zcls::Node* camera_ = nullptr;
  int32_t hardware_ = 0;
  bool own_world_ = false;
  std::map<int32_t, Model> models_;
  float offset_[3] = {0, 0, 0};
  double open_ms_ = 0, show_ms_ = 0;
};

}  // namespace mech3
