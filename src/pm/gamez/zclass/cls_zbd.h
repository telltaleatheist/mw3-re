#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace zimg { struct TextureEntry; }
namespace zcls { struct Node; struct Lod; struct Partition; }

// C:\zipper\branchep1\gamez\zClass\cls_zbd.c: Pirate's Moon's gamez.zbd
// (version 41) stacks: each file read appends to the material, model and
// node pools, and the loader keeps a list of the files read (docs/pm-map.md,
// gamez.zbd 41).
namespace pm::zcls {

struct ZbdFile {             // 0x2c bytes at 0x00639c28
  std::string path;          // +0x00
  uint8_t header[0x28] = {}; // +0x04  signature, version, time stamp, the texture count +0xc, offsets
  int32_t get(int off) const {
    int32_t v;
    std::memcpy(&v, header + off, 4);
    return v;
  }
};

struct ZbdState {
  std::vector<ZbdFile> files;  // 0x00639c28, count 0x00639c2c
  int32_t current = -1;        // 0x0060ea00  the file being read
  std::FILE* file = nullptr;   // 0x00639c30  open while it is
  bool own_textures = false;   // 0x00639c34  its texture directory is the table's (the first file)
};
ZbdState& zbd_state();

// Pirate's Moon 1.0 @ 0x00516ab0: a texture index of the file being read.
// The first file's are the table's (0x00531840); a later file's directory was
// never read, so its entry's name is looked up (0x00516ae0).
zimg::TextureEntry* texture_for(int32_t index);
// Pirate's Moon 1.0 @ 0x00516ae0: entry `index` of the current file's own
// directory, read from the file, found or added by name (0x00532450). Null
// outside the directory or with no file open.
zimg::TextureEntry* file_texture(int32_t index);

// Pirate's Moon 1.0 @ 0x00516bc0: a gamez.zbd stacked onto what is loaded:
// the file's list entry, its textures (only while none are loaded), its
// materials, models and nodes, each offset by the pools' bases. 0, or -1 (the
// entry dropped again, 0x00517a70).
int read_zbd(const char* path);
// Pirate's Moon 1.0 @ 0x005179e0: the file open past its 0x28-byte header
// (signature 0x02971222, version 41), null on an error.
std::FILE* open_zbd(const char* path, uint8_t header[0x28]);
// Pirate's Moon 1.0 @ 0x00517ac0 (MW3's 0x004ff730): `node`'s model read again from the world's file, only when
// exactly one gamez.zbd was read (its first in the list); 1 when nothing was read.
int reread(::zcls::Node* node, int32_t recurse);
// Pirate's Moon 1.0 @ 0x00517b30 (MW3's 0x004ff7a0): the node's model as the file has it into a new slot, the old one
// let go; with `recurse` its children's too. 1 when the node is not in the file or its record is unreadable.
int reread_node(std::FILE* f, const uint8_t header[0x28], ::zcls::Node* node, int32_t recurse);

// Pirate's Moon 1.0 @ 0x00516e90: `size` node slots appended to zcls's pool
// in new blocks, then each one's data (0x005171a0), its model index offset
// by `model_base`, its node indices by the pool's size before. PM's layout
// is translated into the shared Node's (docs/pm-map.md, Shared types).
// The pool's capacity, -1 on an error.
int read_nodes(int32_t size, std::FILE* f, int32_t model_base);
// Ours: PM's node (the first 0xd0 bytes, as gamez.zbd and mechlib.zbd store
// it) into the shared Node's raw block; its +0x70 box index returned, which
// box_pointer makes the node's pm_box.
int32_t translate_node(::zcls::Node& n, const uint8_t* pm);
const float* box_pointer(const ::zcls::Node& n, int32_t box);
void translate_lod(::zcls::Lod& l, const uint8_t* pm);
// Pirate's Moon 1.0 @ 0x005171a0: one node's class data and links. 1 when
// it has data or links, 0 when not, -1 on an error or an unknown class.
int read_node(::zcls::Node* node, std::FILE* f, int32_t node_base);
// Pirate's Moon 1.0 @ 0x005178e0: `count` node indices into `out`, each
// offset by `base`; a short read is reported and the nodes taken anyway.
void read_node_list(::zcls::Node** out, int32_t count, std::FILE* f, int32_t base);

// The world's writers (GameZWriteZBDFile, a script command: ported for preservation, the engine's entry left null;
// docs/gamez.md, Writing a world back). Each node index written is the node's place among those written (`remap`,
// the live nodes in pool order).
// Pirate's Moon 1.0 @ 0x00516720: `count` nodes as indices. 0, -1 when not written (reported).
int write_node_list(::zcls::Node* const* list, int32_t count, std::FILE* f, const int32_t* remap);
// Pirate's Moon 1.0 @ 0x00516800: a partition's `count` 12-byte entries (the node's index, its y range).
int write_cell_list(const ::zcls::Partition& p, int32_t count, std::FILE* f, const int32_t* remap);
// Pirate's Moon 1.0 @ 0x00516260: a node's class data in PM's layout, then its parents and children; `record` (its
// PM slot as written) given the offset its data starts at (+0x38). 1 when it wrote any, 0 when none or the class
// is unknown (reported, nothing written), -1 on an error (reported).
int write_node(const ::zcls::Node* node, uint8_t record[0xd4], std::FILE* f, const int32_t* remap);
// Pirate's Moon 1.0 @ 0x00515fb0: the live nodes' slots (link bit 0x2000000) in PM's layout, then each one's data,
// then the slots again. How many were written, 0 after an error (reported).
int write_nodes(std::FILE* f);
// Pirate's Moon 1.0 @ 0x005168e0: the world back to a version-41 gamez.zbd: the header (its time stamp at +0x08,
// under the texture count at +0x0c), the texture directory, the materials, the models, the deferred deletes done,
// the nodes, the header again; then the file is added to the list of those read. 0, -1 when it could not be opened
// or the header not written (reported). FIX (A): written through the write folder (zutl::open), as MW3's.
int write_zbd(const char* path);

}  // namespace pm::zcls
