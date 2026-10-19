#pragma once
#include <cstdint>
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
};

struct Scene {
  std::vector<Image> images;
  std::vector<Batch> batches;
  float min[3] = {0, 0, 0}, max[3] = {0, 0, 0};
  int nodes = 0, objects = 0, polygons = 0, triangles = 0, missing_textures = 0;
};

// Reads <zbd_dir>/<mission>/gamez.zbd and the world textures from
// <zbd_dir>/rimage.zbd. Empty error string on success.
std::string load_world(const std::string& zbd_dir, const std::string& mission, Scene& out);

}  // namespace viewer
