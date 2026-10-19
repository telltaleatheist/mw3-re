#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// A development tool, not part of the port: flattens a mission world read by
// the ported loaders (zcls::read_zbd, zimg) into triangle batches for the
// viewer. The real render path (zRender) is ported separately; this only lets
// the loaded data be looked at.
namespace viewer {

struct Vertex {
  float pos[3];
  float uv[2];
  float color[4];
};

struct Image {
  int width = 0, height = 0;
  std::vector<uint8_t> rgba;     // width * height * 4
  bool alpha = false;            // any pixel below full alpha
};

struct Batch {
  int image = -1;                // index into Scene::images, -1 untextured
  std::vector<Vertex> vertices;  // triangle list
  bool marker = false;           // under an in/out-of-bounds node, hidden by default
  bool blend = false;            // alpha below 255: drawn after the opaque ones
  bool two_sided = false;        // polygon flag 0x100: no backface culling
  bool sky = false;              // under `horizon`: positions relative to it, drawn around the camera
};

struct Scene {
  std::vector<Image> images;
  std::vector<Batch> batches;
  float min[3] = {0, 0, 0}, max[3] = {0, 0, 0};
  int nodes = 0, objects = 0, markers = 0, polygons = 0, triangles = 0, missing_textures = 0;
};

// Reads <zbd_dir>/<mission>/gamez.zbd, textures from the mission's
// rtexture.zbd, rmechtex.zbd and rimage.zbd. Empty error string on success.
std::string load_world(const std::string& zbd_dir, const std::string& mission, Scene& out);

// Local transforms by part name (3x3 rows then translation), replacing the
// model's own for the parts a motion moves.
using Pose = std::map<std::string, std::array<float, 12>>;

// One model tree from <zbd_dir>/mechlib.zbd (e.g. "mech_madcat.flt") with the
// shared materials, textures from rmechtex*.zbd. build() can run again with a
// new pose; the batches keep their order and sizes.
class Mech {
 public:
  Mech();
  ~Mech();
  std::string load(const std::string& zbd_dir, const std::string& entry);
  void build(Scene& out, const Pose* pose) const;

 private:
  struct Data;
  std::unique_ptr<Data> d_;
};

// One animation from <zbd_dir>/motion.zbd (e.g. "madcat_walk"), read by the
// ported zSIScript reader. The game's own playback is not ported; this samples
// the keys linearly (rotations normalised).
class Motion {
 public:
  Motion();
  ~Motion();
  Motion(Motion&&) noexcept;
  Motion& operator=(Motion&&) noexcept;
  std::string load(const std::string& zbd_dir, const std::string& name);
  float loop_time() const;
  void pose(float t, Pose& out) const;

 private:
  struct Data;
  std::unique_ptr<Data> d_;
};

// The motion names in <zbd_dir>/motion.zbd that start with `prefix`.
std::vector<std::string> motions(const std::string& zbd_dir, const std::string& prefix);

}  // namespace viewer
