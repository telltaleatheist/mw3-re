#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

#include "gamez/zgame/unported.h"

namespace zrdr {
struct Node;
}

// The player's view and its cameras (docs/view.md). File unknown: the view
// (0x0040aca0..0x0040bb00) sits between mech3\briefing.cpp and mech3\mech.cpp,
// the mech cameras (0x00421530..0x004222d0) between mech_anim.cpp and
// mech_data.cpp, their set-up 0x00430e70 in mech_data.cpp; named after the
// view. The view ([0x005f4c8c]) owns the scene's camera node (camera1); each
// frame its camera mode, one of the target mech's six cameras, places that
// node: in the cockpit, behind, above.
namespace mech3 {

using Node3 = void*;  // a zClass node (zcls::Node)
struct Mech;
class PlayerView;

// A camera mode (base vtable 0x005bc2c0; 0x78 bytes as the mech's are).
class CameraMode {
 public:
  explicit CameraMode(int32_t id);                      // 0x0040a950
  virtual ~CameraMode() = default;                      // +0x00
  virtual void activate(PlayerView& v) { (void)v; }     // +0x04 (0x004bf620)
  void deactivate(PlayerView& v) { on_switch(v, 0); }   // +0x08 (0x0040a9f0)
  virtual bool update(float dt, PlayerView& v);         // +0x0c 0x0040b8d0
  virtual void set_look(int32_t look) { look_ = look; }  // +0x10 0x0040bac0
  virtual void reset(PlayerView& v);                    // +0x14 0x0040b820
  virtual bool usable(PlayerView& v) { (void)v; return true; }  // +0x18 0x0040bad0
  virtual void on_switch(PlayerView& v, bool on) { (void)v, (void)on; }  // +0x20 0x0040baf0
  virtual bool read(zrdr::Node* node) { (void)node; return false; }  // +0x24

  int32_t id;                      // +0x04  0 tether, 1 attach (cockpit), 2 zippercam, 3 godseye, 4 mcbtether, 5 area
  float delta_rot[4];              // +0x08  the offset's rotation (w, x, y, z)
  float desired_pos[3];            // +0x18
  float dist_vary_factor;          // +0x24
  float dist;                      // +0x28
  float dist_factor;               // +0x2c  per unit of speed
  float dist_vary;                 // +0x30
  float dist_catch_up;             // +0x34  the speed's smoothing rate
  float turn_rate;                 // +0x38  the offset's rotation follows at dt x this
  float look_rate;                 // +0x3c  the look follows at dt x this
  int32_t look_ = 2;               // +0x40
};

// The mech cameras (0x00421530, vtable 0x005bd2f8): tether, zippercam, godseye, mcbtether.
class MechCamera : public CameraMode {
 public:
  explicit MechCamera(int32_t id);                      // 0x00421530
  void activate(PlayerView& v) override;                // 0x00421ad0
  bool update(float dt, PlayerView& v) override;        // 0x00421760
  void set_look(int32_t look) override;                 // 0x00421c00
  void reset(PlayerView& v) override;                   // 0x00421900
  bool usable(PlayerView& v) override;                  // 0x00421a30
  bool read(zrdr::Node* node) override;                 // 0x004215d0

  float look_q[4];                 // +0x44  the look's rotation
  uint8_t torso_look = 0;          // +0x54  look along the torso
  float dist_now;                  // +0x58
  float speed = 0;                 // +0x5c  smoothed forward speed
  PlayerView* view = nullptr;      // +0x60
  uint8_t fixed = 0;               // +0x64  the cockpit camera's: no mech rotation
  float look_left = 1.5533430f;    // +0x68  look limits, radians
  float look_right = -1.5533430f;  // +0x6c
  float look_down = -0.2617994f;   // +0x70
  float look_up = 0.7853982f;      // +0x74
};

// "attach" (vtable 0x005be9f0): the cockpit camera.
class CockpitCamera : public MechCamera {
 public:
  CockpitCamera() : MechCamera(1) {}
  bool update(float dt, PlayerView& v) override;        // 0x00421e40
  void reset(PlayerView& v) override;                   // 0x00421d40
  void on_switch(PlayerView& v, bool on) override;      // 0x00421d50
};

// "area" (vtable 0x005be9c8): a fixed camera that keeps the mech in view.
class AreaCamera : public MechCamera {
 public:
  AreaCamera() : MechCamera(5) {}
  bool update(float dt, PlayerView& v) override;        // 0x004221d0
  void set_look(int32_t look) override;                 // 0x004220c0
};

// A mech type's cameras (+0x24 of its data), and the current one's id (+0x10).
struct CameraList {
  std::vector<CameraMode*> modes;  // +0x04..+0x0c
  int32_t current = 0;             // +0x10
  CameraMode* find(int32_t id) const;  // 0x0040aa00
  ~CameraList();                       // 0x0040abf0
};
// Mech3.exe 1.2 @ 0x00430e70: the six cameras with their defaults, then each
// one's block of the mech's `camera` reader node; `roster` picks tether's
// height, `fov_x` is the view camera's (area's from it).
CameraList* make_cameras(zrdr::Node* mech_reader, int32_t roster, float fov_x);

// What the view calls outside it: zClass's camera and nodes, the world's
// queries and fog, the mech's torso and HUD. One per original function.
class ViewHooks : public zgame::Unported {
 public:
  // How a node or object is named in the hooks' records (the probe's addresses).
  virtual uint32_t id(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  virtual void camera_set_position(Node3 cam, float x, float y, float z) {   // 0x004f3870
    unported("0x004f3870", {id(cam), zgame::bits(x), zgame::bits(y), zgame::bits(z)});
  }
  virtual void camera_set_rotation(Node3 cam, float x, float y, float z) {   // 0x004f36b0
    unported("0x004f36b0", {id(cam), zgame::bits(x), zgame::bits(y), zgame::bits(z)});
  }
  virtual void camera_add_position(Node3 cam, float x, float y, float z) {   // 0x004f3920
    unported("0x004f3920", {id(cam), zgame::bits(x), zgame::bits(y), zgame::bits(z)});
  }
  // A node's data: the flags (+0x00, 0x20: its world matrix is stale) and the
  // world position (+0x84). Read, not called.
  virtual uint32_t node_flags(Node3 node) { return unported("@node_flags", {id(node)}); }
  virtual const float* node_position(Node3 node) = 0;
  // 0x0051e9d0, 0x0051fde0, 0x004ed280(node, 1), 0x0051ea00: the stale world matrix rebuilt.
  virtual void node_refresh(Node3 node) { unported("0x004ed280", {id(node), 1}); }
  virtual void node_world_position(Node3 node, float out[3]) = 0;  // 0x004ed530
  virtual const float* torso_aim(Mech& m) = 0;                       // 0x0041ce00: the torso's rotation
  virtual void torso_cockpit(Mech& m, bool on) = 0;                  // 0x0041ce10
  virtual void mech_attached(Mech& m) = 0;                           // vt+0x100
  virtual void mech_detached(Mech& m) = 0;                           // vt+0x104
  virtual void hud_set_camera(Mech& m, int32_t id) = 0;              // the activations' HUD calls
};
ViewHooks& view_hooks();
void set_view_hooks(ViewHooks* h);

// The player's view (0x0040aca0, vtable 0x005bc2e8; base zView 0x004b5060).
class PlayerView {
 public:
  PlayerView(Node3 camera, Node3 window, Node3 x, int32_t hardware);  // 0x0040aca0
  ~PlayerView();                                        // 0x0040bab0
  void update(float dt);                                // +0x08 0x0040ada0
  bool attach(Mech* target, CameraMode* mode);          // 0x0040adf0
  void set_camera_mode(int32_t id);                     // 0x0040ae70
  void reset();                                         // 0x0040ad90
  // 0x0040b7e0: the camera at `pos` (+ the offset turned by `rot` times
  // `dist`), looking along `look` then the mode's delta.
  void place(const float pos[3], float dist, const float rot[4], const float offset[3], const float look[4],
             const float delta[4]);
  void place_position(const float pos[3], float dist, const float rot[4], const float offset[3]);  // 0x0040b600
  void place_look(const float look[4], const float delta[4]);   // 0x0040b750
  void place_now(const float pos[3], float dist, const float rot[4], const float offset[3], const float look[4],
                 const float delta[4]);                         // 0x0040b210: no smoothing

  Node3 camera = nullptr;         // +0x04  the camera node (camera1)
  Node3 window = nullptr;          // +0x08
  Node3 x0c = nullptr;             // +0x0c
  int32_t hardware = 0;            // +0x14  [0x0072f348] when made
  float speed = 0;                 // +0x18  the target's smoothed speed
  float rot_q[4] = {1, 0, 0, 0};   // +0x1c  the offset's rotation, smoothed
  float look_q[4] = {1, 0, 0, 0};  // +0x2c  the look, smoothed
  float final_q[4] = {1, 0, 0, 0};  // +0x3c  look x delta
  float pos[3] = {};               // +0x4c  where the camera is put
  CameraMode* mode = nullptr;      // +0x58
  int32_t last_mode = 1;           // +0x5c
  float dist = 0;                  // +0x60
  Mech* target = nullptr;          // +0x64
  uint8_t handled = 0;             // +0x68
  uint8_t x69 = 0;                 // +0x69
  uint8_t underwater = 0;          // +0x6a
  float dt = 0;                    // +0x78
};

}  // namespace mech3
