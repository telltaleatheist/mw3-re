#pragma once
#include <string>

namespace platform { class Render3D; }

// The viewer's --real mode: a mission frame drawn entirely by the ported
// render path, in the original's order (docs/render.md): the world read by
// the ported zClass reader, its textures loaded by the ported texture groups
// into Direct3D texture records on platform::Render3D, then per frame the
// camera node set with the ported setters, the world's lights updated, and
// the world render 0x4f7030 (walk, cull, model draw, lighting, fog, clips,
// draw table). The pieces not ported yet (puffers, shadows' casters, the
// frame update, sounds) are left out through their null hooks.
namespace viewer {

// Reads zbd/<mission>/gamez.zbd and the textures through the mounts.
std::string real_load(const std::string& mission, platform::Render3D* render);

// One frame from a viewer camera (position, yaw and pitch in radians,
// yaw 0 looking along +z as the viewer's own camera).
void real_frame(const float eye[3], float yaw, float pitch, platform::Render3D* render);

}  // namespace viewer
