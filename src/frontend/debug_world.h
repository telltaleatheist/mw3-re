#pragma once
#include <string>

namespace platform { class Render3D; }

// A mission world drawn by the ported render path alone, for the tools and
// mw3's debug start (docs/program.md; first written as mw3view --real): the
// world read by the ported zClass reader, its textures loaded by the ported
// texture groups into Direct3D texture records on platform::Render3D, then
// per frame the camera node set with the ported setters, the world's lights
// updated, and the world render 0x4f7030 (walk, cull, model draw, lighting,
// fog, clips, draw table). Our addition, not original code: it stands in for
// the mission load 0x48faf0 and the Core state's frame until those run. The
// pieces not ported yet (the animations and their puffers, the mechs, the
// frame update) are left out through their null hooks.
namespace frontend {

// Reads zbd/<mission>/gamez.zbd and the textures through the mounts, then the
// mission's world.zrd block `variant` (its fog, sky, sun, clouds; "" = the
// first block, "m1" when there is one) from zbd/<mission>/reader.zbd.
// `width` x `height` is the screen mode the world's windows take.
std::string world_load(const std::string& mission, const std::string& variant, platform::Render3D* render,
                       int width = 640, int height = 480);

// The world's sound nodes (zcls::world_sounds) and the listener (zSound's 3D
// placement) wired to the scene walk's hooks; for a program with zSound up.
void world_sounds_on();

// One frame from a camera at `eye` (yaw 0 looking along +z, pitch up
// positive, radians): cleared to the sky colour, the world rendered and
// presented. `dt` < 0 keeps the clock's step (ztimer), else sets it.
void world_frame(const float eye[3], float yaw, float pitch, platform::Render3D* render, float dt = 1.0f / 60);

}  // namespace frontend
