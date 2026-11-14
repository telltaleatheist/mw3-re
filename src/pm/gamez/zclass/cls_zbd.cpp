#include "pm/gamez/zclass/cls_zbd.h"

#include <ctime>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/display.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zutil/zutl_path.h"
#include "pm/gamez/zimage/zimg_texture.h"
#include "pm/gamez/zmodel/gmod_const.h"
#include "pm/gamez/zmodel/gmod_matl.h"

namespace pm::zcls {

namespace {
const char kFile[] = "C:\\zipper\\branchep1\\gamez\\zClass\\cls_zbd.c";
ZbdState g_state;
}  // namespace

ZbdState& zbd_state() { return g_state; }

// Pirate's Moon 1.0 @ 0x00516ab0
::zimg::TextureEntry* texture_for(int32_t index) {
  if (g_state.own_textures) return ::zimg::texture_at(index);  // 0x00531840
  return file_texture(index);
}

// Pirate's Moon 1.0 @ 0x00516ae0. The name is the entry's +0xc on, as the exe
// passes it (a 20-letter name runs on into the state's bytes).
::zimg::TextureEntry* file_texture(int32_t index) {
  if (!g_state.file || g_state.current < 0 || index < 0) return nullptr;
  const ZbdFile& z = g_state.files[static_cast<size_t>(g_state.current)];
  if (index >= z.get(0xc)) return nullptr;
  const long at = std::ftell(g_state.file);
  std::fseek(g_state.file, z.get(0x10) + index * 0x2c, SEEK_SET);
  uint8_t raw[0x2c + 1] = {};
  const bool ok = std::fread(raw, 0x2c, 1, g_state.file) == 1;
  std::fseek(g_state.file, at, SEEK_SET);
  if (!ok) {
    zerr::report(0x200, kFile, 0x3b8, "Error reading GameZ Texture directory data.");
    return nullptr;
  }
  return ::zimg::add_texture(reinterpret_cast<const char*>(raw + 0xc));
}

namespace {
using ::zcls::Node;

int read_error(int line, const char* msg) {
  zerr::report(0x200, kFile, line, msg);
  return -1;
}

template <typename T>
uint32_t i32(const uint8_t* p, int off) {
  T v;
  std::memcpy(&v, p + off, sizeof v);
  return static_cast<uint32_t>(v);
}

}  // namespace

// PM's node (its slot's first 0xd0 bytes; the link is at +0xd0) into the
// shared Node (MW3's layout; docs/pm-map.md). Its +0x70 box index is
// returned: the callers make it a pointer (box_pointer).
int32_t translate_node(Node& n, const uint8_t* pm) {
  std::memcpy(n.raw, pm, 0x4c);                     // name, flags, class, data, model, action: as MW3
  const int32_t ax = static_cast<int16_t>(i32<int16_t>(pm, 0x4c)), az = static_cast<int16_t>(i32<int16_t>(pm, 0x4e));
  n.put<int32_t>(0x4c, ax), n.put<int32_t>(0x50, az);  // the area partition: PM int16 x, z at +0x4c, +0x4e
  n.pm_virtual[0] = static_cast<int16_t>(i32<int16_t>(pm, 0x50)), n.pm_virtual[1] = static_cast<int16_t>(i32<int16_t>(pm, 0x52));
  n.put<int32_t>(0x54, static_cast<int16_t>(i32<int16_t>(pm, 0x54)));  // the parents: PM int16 +0x54
  n.put<uint32_t>(0x58, i32<uint32_t>(pm, 0x58));
  n.put<int32_t>(0x5c, static_cast<int16_t>(i32<int16_t>(pm, 0x56)));  // the children: PM int16 +0x56
  n.put<uint32_t>(0x60, i32<uint32_t>(pm, 0x5c));
  std::memcpy(n.raw + 0x64, pm + 0x60, 0x10);       // the box centre and diagonal: PM +0x60..+0x6f
  std::memcpy(n.raw + 0x74, pm + 0x74, 0xd0 - 0x74);  // the three boxes on: as MW3
  return static_cast<int32_t>(i32<int32_t>(pm, 0x70));
}

// PM's LoD (0x5c bytes) into MW3's layout: three words at +0x34..+0x3f are PM's
// own, MW3's +0x34..+0x4f follow them at +0x40 (the fades, the uniform flag,
// the morph: 0x00508680 against MW3's 0x004f5090; docs/pm-map.md).
void translate_lod(::zcls::Lod& l, const uint8_t* pm) {
  std::memcpy(l.raw, pm, 0x34);
  std::memcpy(l.pm_tail, pm + 0x34, 0xc);
  std::memcpy(l.raw + 0x34, pm + 0x40, 0x1c);
}

// PM's +0x70: which box (0 the node's, 1 the model's, 2 the children's) made
// a pointer to it; any other value is left (the shared Node has none).
const float* box_pointer(const Node& n, int32_t box) {
  return box == 0 ? reinterpret_cast<const float*>(n.raw + 0x74)
         : box == 1 ? reinterpret_cast<const float*>(n.raw + 0x8c)
         : box == 2 ? reinterpret_cast<const float*>(n.raw + 0xa4) : nullptr;
}

namespace {

// 0x00517960: `count` 12-byte entries (a node index, a y range).
void read_partition_nodes(::zcls::Partition& p, int32_t count, std::FILE* f, int32_t base) {
  std::vector<uint8_t> raw(static_cast<size_t>(count) * 12);
  if (std::fread(raw.data(), raw.size(), 1, f) != 1)
    zerr::report(0x200, kFile, 0x40d, "Error reading GameZ Node list.");
  p.nodes = new Node*[static_cast<size_t>(count)]();
  p.pm_y = new float[static_cast<size_t>(count)][2]();
  for (int32_t i = 0; i < count; ++i) {
    int32_t index = static_cast<int32_t>(i32<int32_t>(raw.data(), i * 12));
    if (index >= 0) index += base;
    p.nodes[i] = ::zcls::node_at(index);
    std::memcpy(p.pm_y[i], raw.data() + i * 12 + 4, 8);
  }
}

// FIX (A) memory safety (docs/fixes.md): `count` entries of `entry` bytes that the file can hold (::zmod::ReadBound);
// false, warned, when a broken count asks for more, and the caller's count is set to 0.
bool list_fits(std::FILE* f, int64_t count, uint64_t entry, const char* what) {
  return count < 1 || ::zmod::read_bound().fits(f, static_cast<uint64_t>(count) * entry, what);
}

Node** read_list(int32_t count, std::FILE* f, int32_t base) {
  if (count < 1) return nullptr;
  Node** out = new Node*[static_cast<size_t>(count)]();
  read_node_list(out, count, f, base);
  return out;
}

// The world (PM's 0xcc: MW3's 0xbc with a second area at +0x4c).
int read_world(Node* node, std::FILE* f, int32_t base) {
  auto* w = new ::zcls::World{};
  node->data = w;
  uint8_t pm[0xcc] = {};
  const bool ok = std::fread(pm, sizeof pm, 1, f) == 1;
  std::memcpy(w->raw, pm, 0x4c);
  std::memcpy(w->pm_area2, pm + 0x4c, 0x10);
  std::memcpy(w->raw + 0x4c, pm + 0x5c, 0xbc - 0x4c);
  if (!ok) return read_error(0x51d, "Error reading node world data.");
  if (!list_fits(f, w->get<int32_t>(0xa0), 4, "a world's light list")) w->put<int32_t>(0xa0, 0);  // FIX (A)
  if (!list_fits(f, w->get<int32_t>(0xac), 4, "a world's sound list")) w->put<int32_t>(0xac, 0);
  if (w->get<int32_t>(0x8c) > 0 && w->get<int32_t>(0x88) > 0 &&
      !list_fits(f, int64_t{w->get<int32_t>(0x8c)} * w->get<int32_t>(0x88), 0x58, "a world's area grid"))
    w->put<int32_t>(0x8c, 0), w->put<int32_t>(0x88, 0);
  const int32_t lights = w->get<int32_t>(0xa0), sounds = w->get<int32_t>(0xac);  // PM +0xb0, +0xbc
  if ((w->lights = read_list(lights, f, base))) w->light_data = new void*[static_cast<size_t>(lights)]();
  if ((w->sounds = read_list(sounds, f, base))) w->sound_data = new void*[static_cast<size_t>(sounds)]();
  const int32_t rows = w->get<int32_t>(0x8c), cols = w->get<int32_t>(0x88);  // PM +0x9c, +0x98
  w->rows = new ::zcls::Partition*[rows > 0 ? rows : 0]();
  for (int32_t z = 0; z < rows; ++z) w->rows[z] = new ::zcls::Partition[cols > 0 ? cols : 0]();
  for (int32_t z = 0; z < rows; ++z) {
    for (int32_t x = 0; x < cols; ++x) {
      ::zcls::Partition& p = w->rows[z][x];
      uint8_t pr[0x58];
      if (std::fread(pr, sizeof pr, 1, f) != 1) return read_error(0x56c, "Error reading world area partition data.");
      std::memcpy(p.raw, pr, 0x48);
      std::memcpy(p.pm_tail, pr + 0x48, 0x10);
      if (const int16_t n = p.get<int16_t>(0x3a); n > 0) read_partition_nodes(p, n, f, base);
    }
  }
  w->put<int32_t>(4, 0), w->put<int32_t>(8, 0), w->put<int32_t>(0xc, 0);
  w->area_pending = nullptr;
  ::zcls::push_front(::zcls::kAllNodes, node), ::zcls::push_front(::zcls::kWorlds, node);
  ::zcls::world_set_fog_type(node, w->get<uint32_t>(0x10));
  ::zcls::world_set_fog_color(node, w->get<float>(0x14), w->get<float>(0x18), w->get<float>(0x1c));
  ::zcls::world_set_fog_altitude(node, w->get<float>(0x2c), w->get<float>(0x28));
  ::zcls::world_set_fog_range(node, w->get<float>(0x20), w->get<float>(0x24));
  ::zcls::world_set_fog_density(node, w->get<float>(0x30));
  ::zcls::world_push_fog(node);
  return 1;
}

template <typename T>
T* read_data(Node* node, std::FILE* f, bool* ok) {
  auto* d = new T{};
  node->data = d;
  *ok = std::fread(d->raw, sizeof d->raw, 1, f) == 1;
  return d;
}
}  // namespace

// Pirate's Moon 1.0 @ 0x005178e0
void read_node_list(Node** out, int32_t count, std::FILE* f, int32_t base) {
  if (count == 0) return;
  std::vector<int32_t> index(static_cast<size_t>(count > 0 ? count : 0));
  if (std::fread(index.data(), static_cast<size_t>(count) * 4, 1, f) != 1)
    zerr::report(0x200, kFile, 0x3e3, "Error reading GameZ Node list.");
  for (int32_t i = 0; i < count; ++i) out[i] = ::zcls::node_at(index[static_cast<size_t>(i)] >= 0 ? index[static_cast<size_t>(i)] + base : -1);
}

// Pirate's Moon 1.0 @ 0x005171a0, MW3's 0x004feaf0 with the node base and
// PM's sizes (docs/pm-map.md, the jump table at 0x005178ac).
int read_node(Node* node, std::FILE* f, int32_t base) {
  using namespace ::zcls;
  int result = 0;
  bool ok;
  switch (node->cls()) {
    case 0:
      break;
    case 1: {
      result = 1;
      Camera* c = read_data<Camera>(node, f, &ok);
      if (!ok) return read_error(0x4b2, "Error reading node camera data.");
      for (int i = 0; i < 4; ++i) {
        const int32_t k = c->get<int32_t>(4 * i);
        c->nodes[i] = node_at(k >= 0 ? k + base : -1);
      }
      push_front(kAllNodes, node), push_front(kCameras, node);
      camera_set_clip(node, c->get<float>(0xb0), c->get<float>(0xb4));
      camera_set_fov(node, c->get<float>(0xd8), c->get<float>(0xdc));
      break;
    }
    case 2:
      result = read_world(node, f, base);
      if (result < 0) return result;
      break;
    case 3:
      result = 1;
      read_data<Window>(node, f, &ok);
      if (!ok) return read_error(0x500, "Error reading node window data.");
      push_front(kAllNodes, node), push_front(kWindows, node);
      break;
    case 4: {
      result = 1;
      Display* d = read_data<Display>(node, f, &ok);
      if (!ok) return read_error(0x4e3, "Error reading node display data.");
      push_front(kAllNodes, node), push_front(kDisplays, node);
      display_set_clear_color(node, d->get<float>(0x10), d->get<float>(0x14), d->get<float>(0x18));
      break;
    }
    case 5:
      result = 1;
      read_data<Object3d>(node, f, &ok);
      if (!ok) return read_error(0x464, "Error reading node object3d data.");
      push_front(kAllNodes, node);
      break;
    case 6: {
      result = 1;
      auto* l = new Lod{};
      node->data = l;
      uint8_t pm[0x5c] = {};
      ok = std::fread(pm, sizeof pm, 1, f) == 1;
      translate_lod(*l, pm);
      if (!ok) return read_error(0x479, "Error reading node lod data.");
      push_front(kAllNodes, node);
      break;
    }
    case 9: {
      result = 1;
      auto* l = new Light{};
      node->data = l;
      uint8_t pm[0x100] = {};
      ok = std::fread(pm, sizeof pm, 1, f) == 1;
      std::memcpy(l->raw, pm, 0xb0);
      std::memcpy(l->pm_colors, pm + 0xb0, 0x30);
      std::memcpy(l->raw + 0xb0, pm + 0xe0, 0x20);
      if (!ok) return read_error(0x48e, "Error reading node light data.");
      if (!list_fits(f, l->get<int32_t>(0xc8), 4, "a light's node list")) l->put<int32_t>(0xc8, 0);  // FIX (A)
      l->nodes = read_list(l->get<int32_t>(0xc8), f, base);  // PM +0xf8
      push_front(kAllNodes, node), push_front(kLights, node);
      break;
    }
    case 10: {
      result = 1;
      zerr::report(0x200, kFile, 0x432, "Reading sound node data: Must complete software.");
      Sound* s = read_data<Sound>(node, f, &ok);
      if (!ok) return read_error(0x43d, "Error reading node sound data.");
      s->put<int32_t>(0, 0), s->put<int32_t>(4, 0);
      if (!list_fits(f, s->get<int32_t>(0x8c), 4, "a sound's node list")) s->put<int32_t>(0x8c, 0);  // FIX (A)
      s->nodes = read_list(s->get<int32_t>(0x8c), f, base);
      push_front(kAllNodes, node), push_front(kSounds, node);
      break;
    }
    default: {
      char msg[160];
      std::snprintf(msg, sizeof msg,
                    "gClsReadNode(): Unrecognized node class type:\n  node = %s class_type = %d\n",
                    node->name(), node->cls());
      zerr::report(0x400, kFile, 0x5a6, msg);
      return -1;
    }
  }
  if ((node->parents = read_list(node->parent_count(), f, base))) result = 1;  // PM's int16 +0x54
  if ((node->children = read_list(node->child_count(), f, base))) return 1;  // PM's int16 +0x56
  return result;
}

// Pirate's Moon 1.0 @ 0x00516e90, MW3's 0x004ff200 appending.
int read_nodes(int32_t size, std::FILE* f, int32_t model_base) {
  ::zmod::read_bound().begin(f);  // FIX (A): each section's read starts its own bound (no file's state carries over)
  ::zcls::NodePool& p = ::zcls::nodes();
  const int32_t bs = p.block_size;
  const int32_t base = static_cast<int32_t>(p.blocks.size()) * bs;
  std::vector<int32_t> boxes;  // each node's +0x70 box index
  for (int32_t done = 0; done < size; done += bs) {
    const int32_t b = static_cast<int32_t>(p.blocks.size());
    p.blocks.emplace_back(new Node[static_cast<size_t>(bs)]{});
    p.capacity += bs;
    const int32_t n = size < done + bs ? size - done : bs;
    std::vector<uint8_t> raw(static_cast<size_t>(n) * 0xd4);  // a short read keeps what it read, as the exe's
    const bool ok = std::fread(raw.data(), 1, raw.size(), f) == raw.size();
    for (int32_t i = 0; i < n; ++i) {
      Node& slot = p.blocks[static_cast<size_t>(b)][i];
      boxes.push_back(translate_node(slot, raw.data() + i * 0xd4));
      std::memcpy(&slot.link, raw.data() + i * 0xd4 + 0xd0, 4);
    }
    if (!ok) return read_error(0x607, "Error reading GameZ Node buffer.");
  }
  for (int32_t i = base + size; i < p.capacity; ++i) {  // the last block's rest: zeroed and freed (0x0050c9d0)
    *::zcls::node_at(i) = Node{};
    ::zcls::free_node(i);
  }
  p.live = 0;  // [0x006393cc] = 0 on every file (kept)
  for (int32_t i = base; i < base + size; ++i) {
    Node* node = ::zcls::node_at(i);
    node->pm_box = box_pointer(*node, boxes[static_cast<size_t>(i - base)]);
    int32_t model = node->get<int32_t>(0x3c);
    if (model >= 0) model += model_base;
    node->pm_model = pm::zmod::model_at(model);
    node->model = nullptr;
    node->action = nullptr;
    // PM sets and clears bits 24 and 25 (MW3: 24, kNodeLive); 25's use is unknown.
    if (read_node(node, f, base) < 1) {
      node->link &= ~0x3000000u;
    } else {
      ++p.live;
      node->link |= 0x3000000u;
    }
  }
  for (::zcls::ListItem* item = ::zcls::lists().list[::zcls::kWorlds].head; item; item = item->next) {
    if (!item->node) continue;
    auto* w = static_cast<::zcls::World*>(item->node->data);
    for (int32_t i = 0; i < w->get<int32_t>(0xa0); ++i) w->light_data[i] = w->lights[i]->data;
    for (int32_t i = 0; i < w->get<int32_t>(0xac); ++i) w->sound_data[i] = w->sounds[i]->data;
  }
  return p.capacity;
}

// Pirate's Moon 1.0 @ 0x005179e0. FIX (A): the mounts (zutl::open), as MW3's.
std::FILE* open_zbd(const char* path, uint8_t header[0x28]) {
  std::FILE* f = zutl::open(path, "rb");
  if (!f) return nullptr;
  int line;
  const char* msg;
  int32_t sig, version;
  if (std::fread(header, 0x28, 1, f) != 1) {
    line = 0x69e, msg = "Error reading GameZ header data.";
  } else if (std::memcpy(&sig, header, 4), sig != 0x02971222) {
    line = 0x6a7, msg = "Error reading GameZ header data; incompatible file type";
  } else if (std::memcpy(&version, header + 4, 4), version != 0x29) {
    line = 0x6d0, msg = "Error reading GameZ header data; incompatible file version";
  } else {
    return f;
  }
  zerr::report(0x200, kFile, line, msg);
  std::fclose(f);
  return nullptr;
}

namespace {
// Pirate's Moon 1.0 @ 0x00517a70: the entry pushed for the file dropped.
void drop_file(std::FILE* f) {
  g_state.files.pop_back();
  std::fclose(f);
  g_state.file = nullptr, g_state.current = -1, g_state.own_textures = false;
}

int load_error(std::FILE* f, int line, const char* msg) {
  zerr::report(0x200, kFile, line, msg);
  drop_file(f);
  return -1;
}
}  // namespace

int reread(::zcls::Node* node, int32_t recurse) {
  ZbdState& z = zbd_state();
  if (z.files.size() != 1) return 1;
  uint8_t header[0x28];
  std::FILE* f = open_zbd(z.files[0].path.c_str(), header);
  if (!f) return 1;
  ::zmod::read_bound().begin(f);
  const int r = reread_node(f, header, node, recurse);
  std::fclose(f);
  return r;
}

int reread_node(std::FILE* f, const uint8_t header[0x28], ::zcls::Node* node, int32_t recurse) {
  auto get = [&](int off) { int32_t v; std::memcpy(&v, header + off, 4); return v; };
  const int32_t index = ::zcls::node_index_of(node);  // 0x00515ea0
  if (index < 0 || index >= get(0x1c)) return 1;
  std::fseek(f, get(0x24) + index * 0xd4, SEEK_SET);
  uint8_t record[0xd4];
  if (std::fread(record, sizeof record, 1, f) != 1) return read_error(0x775, "Error reading GameZ Node buffer."), 1;
  std::fseek(f, get(0x18), SEEK_SET);
  pm::zmod::Model* old = node->pm_model;  // 0x004fcd10
  ::zcls::node_set_pm_model(node, nullptr);
  int32_t model;
  std::memcpy(&model, record + 0x3c, 4);
  if (pm::zmod::Model* m = pm::zmod::reread_model(f, model, 0)) {
    ::zcls::node_set_pm_model(node, m);
    if (old) pm::zmod::model_release(old);  // 0x005590f0
  } else {
    ::zcls::node_set_pm_model(node, old);
  }
  if (recurse)
    for (int32_t i = 0; i < node->child_count(); ++i) reread_node(f, header, node->children[i], recurse);
  return 0;
}

// Pirate's Moon 1.0 @ 0x00516bc0
int read_zbd(const char* path) {
  if (!path[0]) return -1;
  ZbdFile entry;
  std::FILE* f = open_zbd(path, entry.header);
  g_state.file = f;
  if (!f) return -1;
  ::zmod::read_bound().begin(f);  // FIX (A): this file's read starts its bound
  entry.path = path;
  g_state.current = static_cast<int32_t>(g_state.files.size());
  g_state.files.push_back(entry);
  const ZbdFile& z = g_state.files.back();
  if (::zimg::textures().count == 0) {  // 0x00531860: only the first file's directory
    std::fseek(f, z.get(0x10), SEEK_SET);
    if (pm::zimg::read_texture_directory(z.get(0xc), f) < 0) return load_error(f, 0x720, "Error reading GameZ texture data.");
    g_state.own_textures = true;
  }
  std::fseek(f, z.get(0x14), SEEK_SET);
  const int32_t materials = pm::zmod::read_materials(f);
  if (materials < 0) return load_error(f, 0x72e, "Error reading GameZ material data.");
  std::fseek(f, z.get(0x18), SEEK_SET);
  const int32_t models = pm::zmod::read_models(f, materials);
  if (models < 0) return load_error(f, 0x73a, "Error reading GameZ model3d data.");
  std::fseek(f, z.get(0x24), SEEK_SET);
  if (read_nodes(z.get(0x1c), f, models) < 0) return load_error(f, 0x746, "Error reading GameZ node data.");
  std::fclose(f);
  g_state.file = nullptr, g_state.current = -1, g_state.own_textures = false;
  return 0;
}

// The world's writers (WriteZBD's; ported for preservation, docs/gamez.md, Writing a world back).

namespace {
int32_t written_index(const Node* node, const int32_t* remap) {
  const int32_t i = ::zcls::node_index_of(node);  // 0x00515ea0
  return i >= 0 && remap ? remap[i] : i;
}

int write_error(int line, const char* msg) {
  zerr::report(0x200, kFile, line, msg);
  return -1;
}

// The shared Node back into PM's slot: translate_node turned the other way. The exe copies its slot whole; the
// pointers it holds (+0x38 data, +0x58 parents, +0x5c children) are the file's values here.
void pm_record(const Node& n, uint8_t* pm) {
  std::memset(pm, 0, 0xd4);
  std::memcpy(pm, n.raw, 0x4c);
  const int16_t area[2] = {static_cast<int16_t>(n.get<int32_t>(0x4c)), static_cast<int16_t>(n.get<int32_t>(0x50))};
  std::memcpy(pm + 0x4c, area, 4);
  std::memcpy(pm + 0x50, n.pm_virtual, 4);
  const int16_t links[2] = {static_cast<int16_t>(n.parent_count()), static_cast<int16_t>(n.child_count())};
  std::memcpy(pm + 0x54, links, 4);
  std::memcpy(pm + 0x58, n.raw + 0x58, 4);
  std::memcpy(pm + 0x5c, n.raw + 0x60, 4);
  std::memcpy(pm + 0x60, n.raw + 0x64, 0x10);
  std::memcpy(pm + 0x74, n.raw + 0x74, 0xd0 - 0x74);
  std::memcpy(pm + 0xd0, &n.link, 4);
}
}  // namespace

// Pirate's Moon 1.0 @ 0x00516720. The exe's buffer is kept between calls ([0x00639c38], freed by 0x00515fb0).
int write_node_list(Node* const* list, int32_t count, std::FILE* f, const int32_t* remap) {
  if (count == 0) return 0;
  std::vector<int32_t> index(static_cast<size_t>(count > 0 ? count : 0));
  for (int32_t i = 0; i < count; ++i) index[static_cast<size_t>(i)] = written_index(list[i], remap);
  if (std::fwrite(index.data(), index.size() * 4, 1, f) != 1) return write_error(0x146, "Error writing node data.");
  return 0;
}

// Pirate's Moon 1.0 @ 0x00516800 ([0x00639c40] its buffer)
int write_cell_list(const ::zcls::Partition& p, int32_t count, std::FILE* f, const int32_t* remap) {
  if (count == 0) return 0;
  std::vector<uint8_t> raw(static_cast<size_t>(count > 0 ? count : 0) * 12);
  for (int32_t i = 0; i < count; ++i) {
    const int32_t at = written_index(p.nodes[i], remap);
    std::memcpy(raw.data() + static_cast<size_t>(i) * 12, &at, 4);
    if (p.pm_y) std::memcpy(raw.data() + static_cast<size_t>(i) * 12 + 4, p.pm_y[i], 8);
  }
  if (std::fwrite(raw.data(), raw.size(), 1, f) != 1)
    return write_error(0x17d, "Error writing area partition child data.");
  return 0;
}

// Pirate's Moon 1.0 @ 0x00516260
int write_node(const Node* node, uint8_t record[0xd4], std::FILE* f, const int32_t* remap) {
  using namespace ::zcls;
  int result = 0;
  if (node->action) {
    char msg[96];
    std::snprintf(msg, sizeof msg, "Writing gamez.zbd; node %s has action callback defined.", node->name());
    zerr::report(0x200, kFile, 0x198, msg);
  }
  const uint32_t at = static_cast<uint32_t>(std::ftell(f));
  const auto data = [&](const void* d, size_t n) { return std::fwrite(d, n, 1, f) == 1; };
  switch (node->cls()) {
    case 0:
      break;
    case 1: {
      result = 1;
      const auto* c = static_cast<const Camera*>(node->data);
      uint8_t raw[sizeof c->raw];
      std::memcpy(raw, c->raw, sizeof raw);
      for (int i = 0; i < 4; ++i) {
        const int32_t k = written_index(c->nodes[i], remap);
        std::memcpy(raw + 4 * i, &k, 4);
      }
      if (!data(raw, sizeof raw)) return write_error(0x212, "Error writing node camera data.");
      break;
    }
    case 2: {
      result = 1;
      const auto* w = static_cast<const World*>(node->data);
      uint8_t pm[0xcc];
      std::memcpy(pm, w->raw, 0x4c);
      std::memcpy(pm + 0x4c, w->pm_area2, 0x10);
      std::memcpy(pm + 0x5c, w->raw + 0x4c, 0xbc - 0x4c);
      if (!data(pm, sizeof pm)) return write_error(0x244, "Error writing node world data.");
      if (w->get<int32_t>(0xa0) > 0) write_node_list(w->lights, w->get<int32_t>(0xa0), f, remap);  // PM +0xb0
      if (w->get<int32_t>(0xac) > 0) write_node_list(w->sounds, w->get<int32_t>(0xac), f, remap);  // PM +0xbc
      for (int32_t z = 0; z < w->get<int32_t>(0x8c); ++z)
        for (int32_t x = 0; x < w->get<int32_t>(0x88); ++x) {
          const Partition& p = w->rows[z][x];
          uint8_t pr[0x58];
          std::memcpy(pr, p.raw, 0x48);
          std::memcpy(pr + 0x48, p.pm_tail, 0x10);
          if (!data(pr, sizeof pr)) return write_error(0x260, "Error writing world area partition data.");
          if (p.get<int16_t>(0x3a) > 0) write_cell_list(p, p.get<int16_t>(0x3a), f, remap);
        }
      break;
    }
    case 3:
      result = 1;
      if (!data(static_cast<const Window*>(node->data)->raw, 0xf8)) return write_error(0x231, "Error writing node window data.");
      break;
    case 4:
      result = 1;
      if (!data(static_cast<const Display*>(node->data)->raw, 0x1c)) return write_error(0x222, "Error writing node display data.");
      break;
    case 5:
      result = 1;
      if (!data(static_cast<const Object3d*>(node->data)->raw, 0x90)) return write_error(0x1c4, "Error writing node object3d data.");
      break;
    case 6: {
      result = 1;
      const auto* l = static_cast<const Lod*>(node->data);
      uint8_t pm[0x5c];
      std::memcpy(pm, l->raw, 0x34);
      std::memcpy(pm + 0x34, l->pm_tail, 0xc);
      std::memcpy(pm + 0x40, l->raw + 0x34, 0x1c);
      if (!data(pm, sizeof pm)) return write_error(0x1d3, "Error writing node lod data.");
      break;
    }
    case 9: {
      result = 1;
      const auto* l = static_cast<const Light*>(node->data);
      uint8_t pm[0x100];
      std::memcpy(pm, l->raw, 0xb0);
      std::memcpy(pm + 0xb0, l->pm_colors, 0x30);
      std::memcpy(pm + 0xe0, l->raw + 0xb0, 0x20);
      if (!data(pm, sizeof pm)) return write_error(0x1e2, "Error writing node light data.");
      if (l->get<int32_t>(0xc8) > 0) write_node_list(l->nodes, l->get<int32_t>(0xc8), f, remap);  // PM +0xf8
      break;
    }
    case 10: {
      result = 1;
      zerr::report(0x200, kFile, 0x1a6, "Writing sound node data: Must complete software.");
      const auto* s = static_cast<const Sound*>(node->data);
      if (!data(s->raw, 0x94)) return write_error(0x1af, "Error writing node sound data.");
      if (s->get<int32_t>(0x8c) > 0) write_node_list(s->nodes, s->get<int32_t>(0x8c), f, remap);
      break;
    }
    default: {
      char msg[160];
      std::snprintf(msg, sizeof msg, "gClsWriteNode(): Unrecognized node class type:\n  node = %s class_type = %d\n",
                    node->name(), node->cls());
      zerr::report(0x400, kFile, 0x275, msg);
      return 0;
    }
  }
  if (node->parent_count() > 0) {
    result = 1;
    write_node_list(node->parents, node->parent_count(), f, remap);
  }
  if (node->child_count() > 0) {
    result = 1;
    write_node_list(node->children, node->child_count(), f, remap);
  }
  std::memcpy(record + 0x38, &at, 4);
  return result;
}

// Pirate's Moon 1.0 @ 0x00515fb0. The box pointer at +0x70 becomes which box it is (0 the node's, 1 the model's, 2
// the children's), as read_nodes takes it; a node with none writes 0, as the exe's null pointer.
// FIX (bug) (docs/fixes.md): the exe's list of where each node went is set only for the live ones, so a list naming
// one that is not written takes whatever that memory held; here -1, none.
int write_nodes(std::FILE* f) {
  ::zcls::NodePool& p = ::zcls::nodes();
  const int32_t cap = p.capacity;
  if (cap == 0) return 0;
  std::vector<int32_t> remap(static_cast<size_t>(cap), -1);
  std::vector<const Node*> live;
  std::vector<uint8_t> raw;
  for (int32_t i = 0; i < cap; ++i) {
    const Node* n = ::zcls::node_at(i);
    if (!(n->link & 0x2000000u)) continue;
    remap[static_cast<size_t>(i)] = static_cast<int32_t>(live.size());
    live.push_back(n);
    raw.resize(raw.size() + 0xd4);
    uint8_t* r = raw.data() + raw.size() - 0xd4;
    pm_record(*n, r);
    const float* box = n->pm_box;
    const int32_t which = box == box_pointer(*n, 0) ? 0 : box == box_pointer(*n, 1) ? 1 : box == box_pointer(*n, 2) ? 2 : 0;
    std::memcpy(r + 0x70, &which, 4);
  }
  int32_t result = static_cast<int32_t>(live.size());
  for (int32_t i = 0; i < result; ++i) {
    uint8_t* r = raw.data() + static_cast<size_t>(i) * 0xd4;
    int16_t parents, children;
    std::memcpy(&parents, r + 0x54, 2), std::memcpy(&children, r + 0x56, 2);
    if (parents == 0) std::memset(r + 0x58, 0, 4);
    if (children == 0) std::memset(r + 0x5c, 0, 4);
    const int32_t model = pm::zmod::model_index(live[static_cast<size_t>(i)]->pm_model);  // 0x00557b30
    std::memcpy(r + 0x3c, &model, 4);
    std::memset(r + 0x48, 0, 4);
  }
  const long slots_at = std::ftell(f);
  if (!raw.empty() && std::fwrite(raw.data(), raw.size(), 1, f) != 1) {
    zerr::report(0x200, kFile, 0x2f6, "Error writing node data.");
    result = 0;
  }
  for (int32_t i = 0; i < result; ++i)
    write_node(live[static_cast<size_t>(i)], raw.data() + static_cast<size_t>(i) * 0xd4, f, remap.data());
  const long end = std::ftell(f);
  std::fseek(f, slots_at, SEEK_SET);
  if (std::fwrite(raw.data(), static_cast<size_t>(result) * 0xd4, 1, f) != 1) {
    zerr::report(0x200, kFile, 0x316, "Error writing node data.");
    result = 0;
  }
  std::fseek(f, end, SEEK_SET);
  return result;
}

// Pirate's Moon 1.0 @ 0x005168e0
int write_zbd(const char* path) {
  if (!path[0]) return -1;
  std::FILE* f = zutl::open(path, "wb");
  if (!f) return -1;
  uint8_t h[0x28] = {};
  const int32_t sig = 0x02971222, version = 0x29;
  const uint32_t stamp = static_cast<uint32_t>(std::time(nullptr));  // time(): its low word stays at +0x08
  std::memcpy(h, &sig, 4), std::memcpy(h + 4, &version, 4), std::memcpy(h + 8, &stamp, 4);
  const auto put = [&](int off, int32_t v) { std::memcpy(h + off, &v, 4); };
  if (std::fwrite(h, sizeof h, 1, f) != 1) {
    zerr::report(0x200, kFile, 0x35f, "Error writing GameZ header data.");
    std::fclose(f);  // the exe leaves it open
    return -1;
  }
  put(0x10, static_cast<int32_t>(std::ftell(f)));
  put(0xc, pm::zimg::write_texture_directory(f));
  put(0x14, static_cast<int32_t>(std::ftell(f)));
  pm::zmod::write_materials(f);
  put(0x18, static_cast<int32_t>(std::ftell(f)));
  pm::zmod::write_models(f);
  ::zcls::deferred_flush();  // 0x0050bb20
  put(0x20, ::zcls::nodes().free_head);
  put(0x24, static_cast<int32_t>(std::ftell(f)));
  put(0x1c, write_nodes(f));
  std::fseek(f, 0, SEEK_SET);
  if (std::fwrite(h, sizeof h, 1, f) != 1) {
    zerr::report(0x200, kFile, 0x384, "Error writing GameZ header data.");
    std::fclose(f);  // the exe leaves it open
    return -1;
  }
  std::fclose(f);
  ZbdFile entry;
  entry.path = path;
  std::memcpy(entry.header, h, sizeof h);
  g_state.files.push_back(entry);
  return 0;
}

}  // namespace pm::zcls
