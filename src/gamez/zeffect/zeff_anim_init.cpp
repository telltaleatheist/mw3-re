#include "gamez/zeffect/zeff_anim_init.h"

#include <cstring>
#include <vector>

#include "gamez/zclass/light.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/sound.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zutil/zutl_msg.h"

namespace zeff {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zEffect\\zeff_anim_init.c";
AnimState g_anim;
AnimHooks g_hooks{};

// `count` records of T read with one fread, as the exe does.
template <typename T>
T* read_records(std::FILE* f, uint32_t count, bool* ok) {
  T* out = new T[count]{};
  std::vector<uint8_t> buf(count * sizeof out->raw);
  *ok = std::fread(buf.data(), sizeof out->raw, count, f) == count;
  for (uint32_t i = 0; i < count; ++i) std::memcpy(out[i].raw, buf.data() + i * sizeof out->raw, sizeof out->raw);
  return out;
}

bool read_blob(std::FILE* f, int32_t size, uint8_t** out) {
  if (size <= 0) return true;
  *out = new uint8_t[size];
  return std::fread(*out, size, 1, f) == 1;
}

// One def and its lists, blobs and sequences, in file order.
bool read_def(std::FILE* f, AnimDef& d) {
  if (std::fread(d.raw, sizeof d.raw, 1, f) != 1) return false;
  std::memset(d.raw + 0x114, 0, 0x24);
  std::memset(d.raw + 0xc4, 0, 4), std::memset(d.raw + 0x100, 0, 4);
  bool ok = true;
  if (const uint8_t n = d.count(0x109); n && (d.objects = read_records<ObjectRef>(f, n, &ok), !ok)) return false;
  if (const uint8_t n = d.count(0x10a); n && (d.nodes = read_records<NodeRef40>(f, n, &ok), !ok)) return false;
  if (const uint8_t n = d.count(0x10b); n && (d.lights = read_records<CreatedRef>(f, n, &ok), !ok)) return false;
  if (const uint8_t n = d.count(0x10c); n && (d.puffers = read_records<CreatedRef>(f, n, &ok), !ok)) return false;
  if (const uint8_t n = d.count(0x10d); n && (d.sounds = read_records<CreatedRef>(f, n, &ok), !ok)) return false;
  if (const uint8_t n = d.count(0x10e); n && (d.static_sounds = read_records<StaticSoundRef>(f, n, &ok), !ok))
    return false;
  if (const uint8_t n = d.count(0x10f); n && (d.effects = read_records<EffectRef>(f, n, &ok), !ok)) return false;
  if (const uint8_t n = d.count(0x110); n && (d.prereqs = read_records<Prerequisite>(f, n, &ok), !ok)) return false;
  if (const uint8_t n = d.count(0x112); n) {
    if ((d.anim_refs = read_records<AnimRefRec>(f, n, &ok), !ok)) return false;
    for (uint8_t i = 0; i < n; ++i) d.anim_refs[i].put<uint32_t>(0x44, 0);
  }
  const uint8_t seqs = d.count(0x108);
  if (seqs) d.seqs = new SeqDef[seqs]{};
  // The reset sequence again, over the copy in the record.
  if (std::fread(d.raw + 0xc8, 0x40, 1, f) != 1) return false;
  if (!read_blob(f, d.get<int32_t>(0x104), &d.reset_events)) return false;
  for (uint8_t i = 0; i < seqs; ++i) {
    SeqDef& s = d.seqs[i];
    if (std::fread(s.raw, sizeof s.raw, 1, f) != 1) return false;
    if (!read_blob(f, s.get<int32_t>(0x3c), &s.events)) return false;
  }
  return true;
}

// Binds one def to the scene: false on any lookup failure. Def 0 and corrupt
// (status 5) defs are skipped with their light, puffer and sound counts left 0.
bool fix_def(int32_t index, AnimDef& d, const char** prev_name) {
  const AnimHooks& h = g_hooks;
  const uint8_t lights = d.count(0x10b), puffers = d.count(0x10c), sounds = d.count(0x10d);
  d.raw[0x10b] = d.raw[0x10c] = d.raw[0x10d] = 0;
  if (index == 0 || d.raw[0x98] == 5) return true;
  const char* node_name = reinterpret_cast<const char*>(d.raw + 0x20);
  // A run of defs with one node name takes that name's nodes in turn.
  d.node = nullptr;
  if (*prev_name && std::strcmp(*prev_name, node_name) == 0) d.node = zcls::list_walk_by_name(nullptr, 0);
  if (!d.node) {
    *prev_name = node_name;
    zcls::list_walk_by_name(node_name, zcls::kAllNodes);
    d.node = zcls::list_walk_by_name(nullptr, 0);
  }
  if (!d.node) return false;
  Node* root = zcls::node_root(d.node);
  if (g_anim.copy_trees && root && root->cls() != 2 && root->cls() != 1) {
    if (!(d.node = h.copy_tree(d.node))) return false;
    d.put<uint32_t>(0x94, d.get<uint32_t>(0x94) | 0x20000);
  }
  d.root = nullptr;
  if (!(d.root = anim_find_node(&d, reinterpret_cast<const char*>(d.raw + 0x44)))) return false;
  const auto name = [](const auto& rec) { return reinterpret_cast<const char*>(rec.raw); };
  d.raw[0x10b] = lights;
  for (int k = 1; k < d.count(0x10b); ++k) {
    Node* n = zcls::light_create();
    if (!(d.lights[k].made = n)) return false;
    zcls::node_set_name(n, name(d.lights[k]));
  }
  d.raw[0x10c] = puffers;
  for (int k = 1; k < d.count(0x10c); ++k)
    if (!(d.puffers[k].made = h.create_puffer())) return false;
  d.raw[0x10d] = sounds;
  for (int k = 1; k < d.count(0x10d); ++k) {
    Node* n = zcls::sound_create();
    if (!(d.sounds[k].made = n)) return false;
    zcls::node_set_name(n, name(d.sounds[k]));
    zcls::sound_set_name(n, name(d.sounds[k]));
  }
  for (int k = 1; k < d.count(0x109); ++k)
    if (!(d.objects[k].node = anim_find_node(&d, name(d.objects[k])))) return false;
  for (int k = 1; k < d.count(0x10a); ++k)
    if (!(d.nodes[k].node = anim_find_node(&d, name(d.nodes[k])))) return false;
  for (int k = 1; k < d.count(0x10e); ++k) d.static_sounds[k].sound = h.find_sound(name(d.static_sounds[k]));
  for (int k = 1; k < d.count(0x10f); ++k) {
    const int32_t e = h.effect_index(name(d.effects[k]));
    d.effects[k].put(0x20, e);
    if (e == -1) return false;
  }
  // Prerequisites: type 1 names a def; types 2/3 name a node, 3 under the
  // node found before it. FIX (bug): the exe's def search stops one short and
  // never finds the last def; this searches them all (docs/fixes.md).
  Node* last = nullptr;
  for (int k = 0; k < d.count(0x110); ++k) {
    Prerequisite& p = d.prereqs[k];
    const uint8_t type = p.raw[4];
    if (type == 1) {
      p.def = nullptr;
      const char* want = reinterpret_cast<const char*>(p.raw + 8);
      for (int32_t j = 0; j < g_anim.def_count(); ++j) {
        if (std::strcmp(g_anim.defs[j].name(), want) == 0) {
          p.def = &g_anim.defs[j];
          break;
        }
      }
    } else if (type == 2 || type == 3) {
      const char* want = reinterpret_cast<const char*>(p.raw + 0xc);
      Node* n = last ? zcls::node_find_in_tree(last, want) : anim_find_node(&d, want);
      if (!n) {
        char msg[200];
        std::snprintf(msg, sizeof msg,
                      "ACTIVATION_PREREQUISITE error; couldn't find node.\n  Animation: %s; node: %s\n",
                      d.name(), want);
        zerr::report(0x400, kFile, 0x33a0, msg);
        d.raw[0x110] = 0;  // the def loses every prerequisite
        break;
      }
      p.node = n;
      last = type == 2 ? nullptr : n;
    }
  }
  return true;
}
}  // namespace

AnimState& anim() { return g_anim; }
AnimHooks& anim_hooks() { return g_hooks; }

// Mech3.exe 1.2 @ 0x005090e0
void anim_set_world(void* world) { g_anim.world = world; }

// Mech3.exe 1.2 @ 0x005090f0. A path of 0x80 characters or more is refused.
void anim_set_zbd_path(const char* path) {
  if (std::strlen(path) > 0x7f) {
    char msg[200];
    std::snprintf(msg, sizeof msg, "Animation ZBD filename too long: %s\n", path);
    zerr::report(0x400, kFile, 0x100, msg);
    return;
  }
  std::strcpy(g_anim.path, path);
}

// Mech3.exe 1.2 @ 0x00509150
void anim_allow_loading(int32_t on) { g_anim.loading = on; }

// Mech3.exe 1.2 @ 0x0050a0e0. A failure part way leaves what was read in
// place; the caller (0x50ae60) ignores the result.
int anim_read_zbd() {
  AnimState& a = g_anim;
  if (!a.loading || a.path[0] == 0) return -1;
  std::FILE* f = std::fopen(a.path, "rb");
  if (!f) return -1;
  const auto fail = [f] {
    std::fclose(f);
    return -1;
  };
  uint32_t header[3];
  if (std::fread(header, sizeof header, 1, f) != 1 || header[0] != 0x08170616 || header[1] != 39) return fail();
  // The source file list (0x0060c5f8 / 0x0060c5fc) is read and dropped.
  std::vector<uint8_t> sources(static_cast<size_t>(header[2]) * 0x54);
  if (std::fread(sources.data(), 0x54, header[2], f) != header[2]) return fail();
  if (std::fread(a.block.raw, sizeof a.block.raw, 1, f) != 1) return fail();
  a.block.put<uint32_t>(0x14, 0);
  a.messages = nullptr;
  const int16_t count = a.def_count();
  a.defs = new AnimDef[count > 0 ? count : 0]{};
  for (int32_t i = 0; i < count; ++i)
    if (!read_def(f, a.defs[i])) return fail();
  const char* prev_name = nullptr;
  for (int32_t i = 0; i < count; ++i)
    if (!fix_def(i, a.defs[i], &prev_name)) return fail();
  if (const int32_t n = a.message_count(); n > 0) {
    bool ok;
    a.messages = read_records<AnimMessage>(f, n, &ok);
    if (!ok) return fail();
    for (int32_t i = 0; i < n; ++i)
      a.messages[i].put(0x20, zutl::msg_id(reinterpret_cast<const char*>(a.messages[i].raw)));
  }
  std::fclose(f);
  return 0;
}

// Mech3.exe 1.2 @ 0x005096d0. The node or a descendant by name, children first to last.
Node* anim_find_in_tree(Node* node, const char* name) {
  if (!node) return nullptr;
  if (std::strcmp(node->name(), name) == 0) return node;
  for (int32_t i = 0; i < node->child_count(); ++i)
    if (Node* hit = anim_find_in_tree(node->children[i], name)) return hit;
  return nullptr;
}

namespace {
int32_t created_index(const CreatedRef* refs, uint8_t count, const char* name) {
  for (int32_t k = 0; k < count; ++k) {
    const auto* n = static_cast<const Node*>(refs[k].made);
    if (n && std::strcmp(n->name(), name) == 0) return k;
  }
  return -1;
}
}  // namespace

// Mech3.exe 1.2 @ 0x005091e0. By the created light node's name.
int32_t anim_light_index(AnimDef* def, const char* name) {
  return def->count(0x10b) ? created_index(def->lights, def->count(0x10b), name) : -1;
}

// Mech3.exe 1.2 @ 0x00509160
int32_t anim_sound_index(AnimDef* def, const char* name) {
  return def->count(0x10d) ? created_index(def->sounds, def->count(0x10d), name) : -1;
}

// Mech3.exe 1.2 @ 0x00509640. Under the def's root, under its node, its own
// lights and sounds (entry 0 never matches), then, unless flag 0x80000 keeps
// it local, any node in the world.
Node* anim_find_node(AnimDef* def, const char* name) {
  Node* hit = anim_find_in_tree(def->root, name);
  if (!hit) hit = anim_find_in_tree(def->node, name);
  if (hit) return hit;
  if (const int32_t i = anim_light_index(def, name); i > 0) hit = static_cast<Node*>(def->lights[i].made);
  if (hit) return hit;
  if (const int32_t i = anim_sound_index(def, name); i > 0) hit = static_cast<Node*>(def->sounds[i].made);
  if (!hit && (def->get<uint32_t>(0x94) & 0x80000) == 0) hit = zcls::list_find_by_name(zcls::kAllNodes, name);
  return hit;
}

}  // namespace zeff
