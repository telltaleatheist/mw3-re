#pragma once
#include <cstdint>

// The program's player view (our addition, docs/view.md section 6): the port
// (mech3/player_view) made where the mission load makes the view and called
// where the Core state, the launch and the options call it; its hooks onto
// zClass, the mech's ports and zReader. Fields the app code reads by address
// ([0x005f4c8c] +0x58 the mode, its +4 the id; +0x64 the target) are copied
// into the view's block after each change.
namespace mech3 {

struct Mech;
class PlayerView;

// What the glue asks of the engine (GameEngine, which cannot share a file
// with mech.h's types).
struct ViewProgram {
  void* ctx = nullptr;
  uint32_t (*unported)(void* ctx, const char* key) = nullptr;  // logged once
  int32_t (*hardware)(void* ctx) = nullptr;                    // [0x0072f348]
  void (*sync)(void* ctx) = nullptr;                           // after each change
};
void wire_view(const ViewProgram* p);       // null: unwired, the view dropped
int32_t program_view_mode_id();             // the view's camera id, -1 without one
void* program_view_target();                // its target mech
// For the tests: the camera node's world position and the target's torso
// node's; false without a view, a target or a torso.
bool program_view_positions(float camera[3], float torso[3]);
PlayerView* program_view();
void program_view_make(void* camera_node);  // 0x0040aca0
void program_view_attach(Mech& m);          // the mech's 0x0041ce30
void program_view_set_camera(int32_t id);   // 0x0040ae70
void* program_cameras(int32_t roster);       // 0x00430e70
void program_cameras_free(void* list);      // 0x0040abf0
// The options' camera choice (0x0040ce70 / 0x0040ce10; defined with the options).
int32_t program_camera_option();
const float* program_mission_fog(bool water);  // world.zrd's fog: range 2, altitude 2, colour 3 (app_mission.cpp)
void program_camera_set(int32_t mode);
int32_t program_mouse_mode();
void program_set_mouse_mode(int32_t v);  // [[0x005f4d44]], the options' MouseMode: the controls' view state

}  // namespace mech3
