#include "gamez/zclass/cls_zbd.h"
#include "gamez/zutil/zutl_zip.h"
#include "upgrades/json/gamez_json.h"

#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/display.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"

namespace zcls {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zClass\\cls_zbd.c";

int read_error(int line, const char* msg) {
  zerr::report(0x200, kFile, line, msg);
  return -1;
}

template <typename T>
T* read_data(Node* node, std::FILE* f, bool* ok) {
  auto* d = new T{};
  node->data = d;
  *ok = std::fread(d->raw, sizeof d->raw, 1, f) == 1;
  return d;
}

// FIX (A) memory safety (docs/fixes.md): `count` entries of `entry` bytes that the file can hold (zmod::ReadBound);
// false, warned, when a broken count asks for more, and the caller's count is set to 0.
bool list_fits(std::FILE* f, int64_t count, uint64_t entry, const char* what) {
  return count < 1 || zmod::read_bound().fits(f, static_cast<uint64_t>(count) * entry, what);
}

// An index list after a count; the exe ignores the list's result.
Node** read_list(int32_t count, std::FILE* f) {
  if (count < 1) return nullptr;
  Node** out = new Node*[count]();
  read_node_list(out, count, f);
  return out;
}

int read_world(Node* node, std::FILE* f) {
  bool ok;
  World* w = read_data<World>(node, f, &ok);
  if (!ok) return read_error(0x406, "Error reading node world data.");
  if (!list_fits(f, w->get<int32_t>(0xa0), 4, "a world's light list")) w->put<int32_t>(0xa0, 0);  // FIX (A)
  if (!list_fits(f, w->get<int32_t>(0xac), 4, "a world's sound list")) w->put<int32_t>(0xac, 0);
  const int32_t lights = w->get<int32_t>(0xa0), sounds = w->get<int32_t>(0xac);
  if ((w->lights = read_list(lights, f))) w->light_data = new void*[lights]();
  if ((w->sounds = read_list(sounds, f))) w->sound_data = new void*[sounds]();
  if (w->get<int32_t>(0x8c) > 0 && w->get<int32_t>(0x88) > 0 &&
      !list_fits(f, int64_t{w->get<int32_t>(0x8c)} * w->get<int32_t>(0x88), sizeof(Partition::raw), "a world's area grid"))
    w->put<int32_t>(0x8c, 0), w->put<int32_t>(0x88, 0);  // FIX (A)
  const int32_t rows = w->get<int32_t>(0x8c), cols = w->get<int32_t>(0x88);
  w->rows = new Partition*[rows > 0 ? rows : 0]();
  for (int32_t z = 0; z < rows; ++z) w->rows[z] = new Partition[cols > 0 ? cols : 0]();
  for (int32_t z = 0; z < rows; ++z) {
    for (int32_t x = 0; x < cols; ++x) {
      Partition& p = w->rows[z][x];
      if (std::fread(p.raw, sizeof p.raw, 1, f) != 1)
        return read_error(0x455, "Error reading world area partition data.");
      p.nodes = read_list(p.get<int16_t>(0x3a), f);
    }
  }
  w->put<int32_t>(4, 0), w->put<int32_t>(8, 0), w->put<int32_t>(0xc, 0);
  w->area_pending = nullptr;
  push_front(kAllNodes, node), push_front(kWorlds, node);
  world_set_fog_type(node, w->get<uint32_t>(0x10));
  world_set_fog_color(node, w->get<float>(0x14), w->get<float>(0x18), w->get<float>(0x1c));
  world_set_fog_altitude(node, w->get<float>(0x2c), w->get<float>(0x28));
  world_set_fog_range(node, w->get<float>(0x20), w->get<float>(0x24));
  world_set_fog_density(node, w->get<float>(0x30));
  world_push_fog(node);
  return 1;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004fea80. Indices are not bounds-checked.
int read_node_list(Node** out, int32_t count, std::FILE* f) {
  if (count == 0) return 0;
  std::vector<int32_t> index(count > 0 ? count : 0);
  if (std::fread(index.data(), static_cast<size_t>(count) * 4, 1, f) != 1) {
    zerr::report(0x200, kFile, 0x302, "Error reading GameZ Node list.");
    return -1;
  }
  for (int32_t i = 0; i < count; ++i) out[i] = node_at(index[i]);
  return 0;
}

// Mech3.exe 1.2 @ 0x004feaf0. 1 when the node has data or links, 0 when not,
// -1 on a read error or an unknown class (7, 8 and above 10).
int read_node(Node* node, std::FILE* f) {
  int result = 0;
  bool ok;
  switch (node->cls()) {
    case 0:
      break;
    case 1: {
      result = 1;
      Camera* c = read_data<Camera>(node, f, &ok);
      if (!ok) return read_error(0x3a3, "Error reading node camera data.");
      for (int i = 0; i < 4; ++i) c->nodes[i] = node_at(c->get<int32_t>(4 * i));
      push_front(kAllNodes, node), push_front(kCameras, node);
      camera_set_clip(node, c->get<float>(0xb0), c->get<float>(0xb4));
      camera_set_fov(node, c->get<float>(0xd8), c->get<float>(0xdc));
      break;
    }
    case 2:
      result = read_world(node, f);
      if (result < 0) return result;
      break;
    case 3:
      result = 1;
      read_data<Window>(node, f, &ok);
      if (!ok) return read_error(0x3e9, "Error reading node window data.");
      push_front(kAllNodes, node), push_front(kWindows, node);
      break;
    case 4: {
      result = 1;
      Display* d = read_data<Display>(node, f, &ok);
      if (!ok) return read_error(0x3cc, "Error reading node display data.");
      push_front(kAllNodes, node), push_front(kDisplays, node);
      display_set_clear_color(node, d->get<float>(0x10), d->get<float>(0x14), d->get<float>(0x18));
      break;
    }
    case 5:
      read_data<Object3d>(node, f, &ok);
      if (!ok) return read_error(0x355, "Error reading node object3d data.");
      result = 1;
      push_front(kAllNodes, node);
      break;
    case 6:
      read_data<Lod>(node, f, &ok);
      if (!ok) return read_error(0x36a, "Error reading node lod data.");
      result = 1;
      push_front(kAllNodes, node);
      break;
    case 9: {
      result = 1;
      Light* l = read_data<Light>(node, f, &ok);
      if (!ok) return read_error(0x37f, "Error reading node light data.");
      if (!list_fits(f, l->get<int32_t>(0xc8), 4, "a light's node list")) l->put<int32_t>(0xc8, 0);  // FIX (A)
      l->nodes = read_list(l->get<int32_t>(0xc8), f);
      push_front(kAllNodes, node), push_front(kLights, node);
      break;
    }
    case 10: {
      result = 1;
      zerr::report(0x200, kFile, 0x323, "Reading sound node data: Must complete software.");
      Sound* s = read_data<Sound>(node, f, &ok);
      if (!ok) return read_error(0x32e, "Error reading node sound data.");
      s->put<int32_t>(0, 0), s->put<int32_t>(4, 0);
      if (!list_fits(f, s->get<int32_t>(0x8c), 4, "a sound's node list")) s->put<int32_t>(0x8c, 0);  // FIX (A)
      s->nodes = read_list(s->get<int32_t>(0x8c), f);
      push_front(kAllNodes, node), push_front(kSounds, node);
      break;
    }
    default: {
      char msg[160];
      std::snprintf(msg, sizeof msg,
                    "gClsReadNode(): Unrecognized node class type:\n  node = %s class_type = %d\n",
                    node->name(), node->cls());
      zerr::report(0x400, kFile, 0x48f, msg);
      return -1;
    }
  }
  if (!list_fits(f, node->parent_count(), 4, "a node's parent list")) node->put<int32_t>(0x54, 0);  // FIX (A)
  if (!list_fits(f, node->child_count(), 4, "a node's child list")) node->put<int32_t>(0x5c, 0);
  if ((node->parents = read_list(node->parent_count(), f))) result = 1;
  if ((node->children = read_list(node->child_count(), f))) return 1;
  return result;
}

// Mech3.exe 1.2 @ 0x004ff200. All slots are read first, then each node's
// data in slot order. Returns the capacity, 0 for an empty pool, -1 on error.
int read_nodes(int32_t size, std::FILE* f) {
  zmod::read_bound().begin(f);  // FIX (A): each section's read starts its own bound (no file's state carries over)
  NodePool& p = nodes();
  if (size == 0) return 0;
  const int32_t bs = p.block_size;
  int32_t block = 0, done = 0;
  std::vector<uint8_t> buf;
  if (size > 0) {
    do {
      if (block == static_cast<int32_t>(p.blocks.size())) {
        p.blocks.emplace_back(new Node[bs]{});
        p.capacity += bs;
      }
      const int32_t n = size < done + bs ? size - done : bs;
      buf.resize(static_cast<size_t>(n) * 0xd4);
      if (std::fread(buf.data(), buf.size(), 1, f) != 1)
        return read_error(0x4e8, "Error reading GameZ Node buffer.");
      for (int32_t i = 0; i < n; ++i) {
        Node& node = p.blocks[block][i];
        node = Node{};
        std::memcpy(node.raw, buf.data() + i * 0xd4, 0xd0);
        std::memcpy(&node.link, buf.data() + i * 0xd4 + 0xd0, 4);
      }
      done += bs;
      ++block;
    } while (done < size);
  }
  if (size < done) {
    for (int32_t i = size; i < done; ++i) *node_at(i) = Node{};
    for (int32_t i = size; i < done; ++i) free_node(i);
  }
  for (; block < static_cast<int32_t>(p.blocks.size()); ++block) {
    for (int32_t i = 0; i < bs; ++i) p.blocks[block][i] = Node{};
    for (int32_t i = 0; i < bs; ++i) free_node(block * bs + i);
  }
  p.live = 0;
  for (int32_t i = 0; i < size; ++i) {
    Node* node = node_at(i);
    node->model = zmod::model_at(node->get<int32_t>(0x3c));
    node->action = nullptr;
    if (read_node(node, f) < 1) {
      node->link &= ~kNodeLive;
    } else {
      ++p.live;
      node->link |= kNodeLive;
    }
  }
  for (ListItem* item = lists().list[kWorlds].head; item; item = item->next) {
    if (!item->node) continue;
    auto* w = static_cast<World*>(item->node->data);
    for (int32_t i = 0; i < w->get<int32_t>(0xa0); ++i) w->light_data[i] = w->lights[i]->data;
    for (int32_t i = 0; i < w->get<int32_t>(0xac); ++i) w->sound_data[i] = w->sounds[i]->data;
  }
  return p.capacity;
}

OpenHook g_open_hook = nullptr;
void set_open_hook(OpenHook hook) { g_open_hook = hook; }

// Mech3.exe 1.2 @ 0x004ff6a0. Only the signature and version are checked.
std::FILE* open_zbd(const char* path, ZbdHeader* header) {
  std::FILE* f = nullptr;
  {
    // UPGRADE (mods): an enabled mod's unzbd JSON for this world, as the file it stands for (docs/modding.md, gamez
    // JSON); one that does not read is said and the gamez.zbd read. The file stays until the next open: the caller
    // reads on from it, and Windows will not remove an open file.
    static std::unique_ptr<zutl_zip::ScratchFile> scratch;
    static std::set<std::string> said;
    std::vector<uint8_t> bytes;
    std::string what, why;
    const int got = gamez_from_mod(path, &bytes, &what, &why);
    if (got > 0) scratch = std::make_unique<zutl_zip::ScratchFile>(bytes.data(), bytes.size());
    if (got > 0 && scratch->file) {
      f = std::fopen(scratch->path.string().c_str(), "rb");
      if (said.insert(what + "\n").second) std::printf("mw3: mod: %s instead of %s\n", what.c_str(), path);
    } else if (got != 0 && said.insert(what + "\n" + why).second) {
      std::printf("mw3: mod: %s: %s; %s read instead\n", what.c_str(), why.c_str(), path);
    }
  }
  if (!f) f = zutl::open(path, "rb");  // FIX (A): the mounts (zutl_path.h)
  if (f && g_open_hook) f = g_open_hook(path, f);
  if (!f) return nullptr;
  int line;
  const char* msg;
  if (std::fread(header, sizeof *header, 1, f) != 1) {
    line = 0x584, msg = "Error reading GameZ header data.";
  } else if (header->signature != 0x02971222) {
    line = 0x58d, msg = "Error reading GameZ header data; incompatible file type";
  } else if (header->version != 27) {
    line = 0x596, msg = "Error reading GameZ header data; incompatible file version";
  } else {
    return f;
  }
  zerr::report(0x200, kFile, line, msg);
  std::fclose(f);
  return nullptr;
}

// Mech3.exe 1.2 @ 0x004fe100. -1 for a null node; one outside the pool
// returns whatever EAX held in the exe (here -1).
int32_t node_index_of(const Node* node) {
  if (!node) return -1;
  const NodePool& p = nodes();
  for (size_t b = 0; b < p.blocks.size(); ++b) {
    const Node* base = p.blocks[b].get();
    if (node >= base && node <= base + p.block_size - 1)
      return static_cast<int32_t>(node - base) + static_cast<int32_t>(b) * p.block_size;
  }
  return -1;
}

// Mech3.exe 1.2 @ 0x004ff7a0 (ECX file, EDX header, stack node, recurse):
// the node's model as the file has it, read again into a new slot; the
// node's current one is let go (freed when unused), kept when the read
// fails. 1 when the node is not in the file or its record is unreadable.
int reread_node(std::FILE* f, const ZbdHeader& h, Node* node, int32_t recurse) {
  const int32_t index = node_index_of(node);
  if (index < 0 || index >= h.node_array_size) return 1;
  std::fseek(f, h.nodes_offset + index * 0xd4, SEEK_SET);
  uint8_t record[0xd4];
  if (std::fread(record, sizeof record, 1, f) != 1) return read_error(0x624, "Error reading GameZ Node buffer."), 1;
  std::fseek(f, h.models_offset, SEEK_SET);
  zmod::Model* old = node->model;  // 0x004eb420
  node_set_model(node, nullptr);
  int32_t model;
  std::memcpy(&model, record + 0x3c, 4);
  if (zmod::Model* m = zmod::reread_model(f, model)) {
    node_set_model(node, m);
    if (old) zmod::model_release(old);
  } else {
    node_set_model(node, old);
  }
  if (recurse)
    for (int32_t i = 0; i < node->child_count(); ++i) reread_node(f, h, node->children[i], recurse);
  return 0;
}

// Mech3.exe 1.2 @ 0x004ff730 (ECX node, EDX recurse): from the file the last
// read_zbd remembered (the destructible buildings' reset, 0x00500760).
int reread(Node* node, int32_t recurse) {
  if (zbd_path()[0] == 0) return 1;
  ZbdHeader h;
  std::FILE* f = open_zbd(zbd_path(), &h);
  if (!f) return 1;
  zmod::read_bound().begin(f);
  const int r = reread_node(f, h, node, recurse);
  std::fclose(f);
  return r;
}

char* zbd_path() {
  static char path[0x30];
  return path;
}

// Mech3.exe 1.2 @ 0x004ff520. A path too long to remember is still read; the
// old remembered path stays.
int read_zbd(const char* path) {
  const size_t len = std::strlen(path);
  if (len == 0) return -1;
  if (len < 0x2f) {
    std::memcpy(zbd_path(), path, len + 1);
  } else {
    char msg[80];
    std::snprintf(msg, sizeof msg, "zbd_filename length %d exceeds storage string size %d.",
                  static_cast<int>(len), 0x30);
    zerr::report(0x200, kFile, 0x5c0, msg);
  }
  ZbdHeader h;
  std::FILE* f = open_zbd(path, &h);
  if (!f) return -1;
  zmod::read_bound().begin(f);  // FIX (A): this file's read starts its bound
  int line = 0;
  const char* msg = nullptr;
  std::fseek(f, h.textures_offset, SEEK_SET);
  const std::string where = zutl::find(path);
  if (zimg::read_texture_directory(h.texture_count, f, where.empty() ? path : where.c_str()) < 0) {
    line = 0x5d1, msg = "Error reading GameZ texture data.";
  } else if (std::fseek(f, h.materials_offset, SEEK_SET), zmod::read_materials(f) < 0) {
    line = 0x5dd, msg = "Error reading GameZ material data.";
  } else if (std::fseek(f, h.models_offset, SEEK_SET), zmod::read_models(f) < 0) {
    line = 0x5e9, msg = "Error reading GameZ model3d data.";
  } else {
    nodes().free_head = h.node_last_free;
    std::fseek(f, h.nodes_offset, SEEK_SET);
    if (read_nodes(h.node_array_size, f) >= 0) {
      std::fclose(f);
      return 0;
    }
    line = 0x5f6, msg = "Error reading GameZ node data.";
  }
  zerr::report(0x200, kFile, line, msg);
  std::fclose(f);
  return -1;
}

// The world's writers (WriteZBD, a script command; ported for preservation, the engine's entry left null;
// docs/gamez.md, Writing a world back).

// Mech3.exe 1.2 @ 0x004fe210 (ECX nodes, EDX count, stack file): their indices; 0, -1 when not written (reported).
int write_node_list(Node* const* list, int32_t count, std::FILE* f) {
  if (count == 0) return 0;
  std::vector<int32_t> index(count > 0 ? count : 0);
  for (int32_t i = 0; i < count; ++i) index[i] = node_index_of(list[i]);
  if (std::fwrite(index.data(), index.size() * 4, 1, f) != 1) {
    zerr::report(0x200, kFile, 0x102, "Error writing node data.");
    return -1;
  }
  return 0;
}

namespace {
template <typename T>
bool write_raw(const Node* node, std::FILE* f) {
  return std::fwrite(static_cast<const T*>(node->data)->raw, sizeof(T::raw), 1, f) == 1;
}
int write_error(int line, const char* msg) {
  zerr::report(0x200, kFile, line, msg);
  return -1;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004fe2d0 (ECX node, EDX file): its class's data as the reader takes it (a camera's four nodes
// as indices; a world's light, sound and cell lists; a light's and a sound's nodes), then its parents and
// children. 1 when it wrote any, 0 when none, -1 on an error (reported). The data's pointers are the file's
// values here, which the exe writes as its own and the reader replaces.
int write_node(const Node* node, std::FILE* f) {
  int result = 0;
  if (node->action) {
    char msg[96];
    std::snprintf(msg, sizeof msg, "Writing gamez.zbd; node %s has action callback defined.", node->name());
    zerr::report(0x200, kFile, 0x11c, msg);
  }
  switch (node->cls()) {
    case 0:
      break;
    case 1: {
      result = 1;
      const auto* c = static_cast<const Camera*>(node->data);
      uint8_t raw[sizeof c->raw];
      std::memcpy(raw, c->raw, sizeof raw);
      for (int i = 0; i < 4; ++i) {
        const int32_t at = node_index_of(c->nodes[i]);
        std::memcpy(raw + 4 * i, &at, 4);
      }
      if (std::fwrite(raw, sizeof raw, 1, f) != 1) return write_error(0x185, "Error writing node camera data.");
      break;
    }
    case 2: {
      result = 1;
      const auto* w = static_cast<const World*>(node->data);
      if (!write_raw<World>(node, f)) return write_error(0x1b7, "Error writing node world data.");
      if (w->get<int32_t>(0xa0) > 0) write_node_list(w->lights, w->get<int32_t>(0xa0), f);
      if (w->get<int32_t>(0xac) > 0) write_node_list(w->sounds, w->get<int32_t>(0xac), f);
      for (int32_t z = 0; z < w->get<int32_t>(0x8c); ++z)
        for (int32_t x = 0; x < w->get<int32_t>(0x88); ++x) {
          const Partition& p = w->rows[z][x];
          if (std::fwrite(p.raw, sizeof p.raw, 1, f) != 1)
            return write_error(0x1d3, "Error writing world area partition data.");
          if (p.get<int16_t>(0x3a) > 0) write_node_list(p.nodes, p.get<int16_t>(0x3a), f);
        }
      break;
    }
    case 3:
      result = 1;
      if (!write_raw<Window>(node, f)) return write_error(0x1a4, "Error writing node window data.");
      break;
    case 4:
      result = 1;
      if (!write_raw<Display>(node, f)) return write_error(0x195, "Error writing node display data.");
      break;
    case 5:
      result = 1;
      if (!write_raw<Object3d>(node, f)) return write_error(0x144, "Error writing node object3d data.");
      break;
    case 6:
      result = 1;
      if (!write_raw<Lod>(node, f)) return write_error(0x153, "Error writing node lod data.");
      break;
    case 9: {
      result = 1;
      if (!write_raw<Light>(node, f)) return write_error(0x162, "Error writing node light data.");
      const auto* l = static_cast<const Light*>(node->data);
      if (l->get<int32_t>(0xc8) > 0) write_node_list(l->nodes, l->get<int32_t>(0xc8), f);
      break;
    }
    case 10: {
      result = 1;
      zerr::report(0x200, kFile, 0x127, "Writing sound node data: Must complete software.");
      if (!write_raw<Sound>(node, f)) return write_error(0x130, "Error writing node sound data.");
      const auto* s = static_cast<const Sound*>(node->data);
      if (s->get<int32_t>(0x8c) > 0) write_node_list(s->nodes, s->get<int32_t>(0x8c), f);
      break;
    }
    default: {
      char msg[160];
      std::snprintf(msg, sizeof msg, "gClsWriteNode(): Unrecognized node class type:\n  node = %s class_type = %d\n",
                    node->name(), node->cls());
      zerr::report(0x400, kFile, 0x1e8, msg);
      return 0;
    }
  }
  if (node->parent_count() > 0) {
    result = 1;
    write_node_list(node->parents, node->parent_count(), f);
  }
  if (node->child_count() > 0) {
    result = 1;
    write_node_list(node->children, node->child_count(), f);
  }
  return result;
}

// Mech3.exe 1.2 @ 0x004fe6c0 (ECX file): every slot of the pool (an empty list's pointer 0, the model as its slot,
// no action), then each slot's data (0x004fe2d0), the slots given the offsets theirs start at (+0xd0's low 24
// bits, in the file only), and the slots written again. The capacity, 0 after an error (reported).
int write_nodes(std::FILE* f) {
  NodePool& p = nodes();
  const int32_t cap = p.capacity;
  if (cap == 0) return 0;
  std::vector<uint8_t> raw(static_cast<size_t>(cap) * 0xd4);
  for (int32_t i = 0; i < cap; ++i) {
    const Node& n = *node_at(i);
    uint8_t* r = raw.data() + static_cast<size_t>(i) * 0xd4;
    std::memcpy(r, n.raw, 0xd0);
    std::memcpy(r + 0xd0, &n.link, 4);
    if (n.parent_count() == 0) std::memset(r + 0x58, 0, 4);
    if (n.child_count() == 0) std::memset(r + 0x60, 0, 4);
    const int32_t model = zmod::model_index(n.model);
    std::memcpy(r + 0x3c, &model, 4);
    std::memset(r + 0x48, 0, 4);
  }
  int32_t result = cap;
  const long slots_at = std::ftell(f);
  if (std::fwrite(raw.data(), raw.size(), 1, f) != 1) {
    zerr::report(0x200, kFile, 0x24a, "Error writing node data.");
    result = 0;
  }
  for (int32_t i = 0; i < result; ++i) {
    const uint32_t at = static_cast<uint32_t>(std::ftell(f));
    if (write_node(node_at(i), f) != 0) {
      uint8_t* link = raw.data() + static_cast<size_t>(i) * 0xd4 + 0xd0;
      uint32_t v;
      std::memcpy(&v, link, 4);
      v = (v & ~0xffffffu) | (at & 0xffffffu);
      std::memcpy(link, &v, 4);
    }
  }
  const long end = std::ftell(f);
  std::fseek(f, slots_at, SEEK_SET);
  if (std::fwrite(raw.data(), raw.size(), 1, f) != 1) {
    zerr::report(0x200, kFile, 0x26c, "Error writing node data.");
    result = 0;
  }
  std::fseek(f, end, SEEK_SET);
  return result;
}

// Mech3.exe 1.2 @ 0x004fe8e0 (ECX path): the world back to a gamez.zbd (GameZWriteZBDFile): the header, the
// texture directory, the materials, the models, the deferred deletes done, the nodes, then the header again
// with where each starts. 0, -1 when it could not be opened or the header not written (reported). The path is
// remembered as read_zbd's is. FIX (A): written through the write folder (zutl::open).
int write_zbd(const char* path) {
  const size_t len = std::strlen(path);
  if (len == 0) return -1;
  if (len < 0x2f) {
    std::memcpy(zbd_path(), path, len + 1);
  } else {
    char msg[80];
    std::snprintf(msg, sizeof msg, "zbd_filename length %d exceeds storage string size %d.",
                  static_cast<int>(len), 0x30);
    zerr::report(0x200, kFile, 0x2a4, msg);
  }
  std::FILE* f = zutl::open(path, "wb");
  if (!f) return -1;
  ZbdHeader h{};
  h.signature = 0x02971222, h.version = 27;
  if (std::fwrite(&h, sizeof h, 1, f) != 1) {
    zerr::report(0x200, kFile, 0x2b7, "Error writing GameZ header data.");
    std::fclose(f);  // the exe leaves it open
    return -1;
  }
  h.textures_offset = static_cast<int32_t>(std::ftell(f));
  h.texture_count = zimg::write_texture_directory(f);
  h.materials_offset = static_cast<int32_t>(std::ftell(f));
  zmod::write_materials(f);
  h.models_offset = static_cast<int32_t>(std::ftell(f));
  zmod::write_models(f);
  deferred_flush();
  h.node_last_free = nodes().free_head;
  h.nodes_offset = static_cast<int32_t>(std::ftell(f));
  h.node_array_size = write_nodes(f);
  std::fseek(f, 0, SEEK_SET);
  if (std::fwrite(&h, sizeof h, 1, f) != 1) {
    zerr::report(0x200, kFile, 0x2dc, "Error writing GameZ header data.");
    std::fclose(f);  // the exe leaves it open
    return -1;
  }
  std::fclose(f);
  return 0;
}

}  // namespace zcls
