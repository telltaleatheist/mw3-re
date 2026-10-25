// Probes for test_camera_zone.py (docs/view.md section 1): zClass's camera
// zone set 0x004f6c80 (zcls::camera_set_zone) on a node the test describes.
#include <cstdint>
#include <cstring>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zerror/zerr.h"

extern "C" {

// kind 0 no node, 1 no data, 2 the node's class `cls`; `cam` the camera's
// +0x1e0 and +0x1e4 in and out; `zone` 8 bytes (the count, then ids and
// what follows). Returns the answer.
int32_t probe_cz_set(int32_t kind, int32_t cls, uint32_t* cam, const uint8_t* zone) {
  zerr::set_sink([](int, const char*, int, const char*) {});
  static zcls::Node node;
  static zcls::Camera data;
  node.put<int32_t>(0x34, cls);
  node.data = kind == 1 ? nullptr : &data;
  data.put<uint32_t>(0x1e0, cam[0]);
  data.put<uint32_t>(0x1e4, cam[1]);
  const int32_t r = zcls::camera_set_zone(kind == 0 ? nullptr : &node, zone);
  cam[0] = data.get<uint32_t>(0x1e0), cam[1] = data.get<uint32_t>(0x1e4);
  zerr::set_sink(nullptr);
  return r;
}

}  // extern "C"
