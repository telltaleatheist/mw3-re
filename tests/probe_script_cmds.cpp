// test_script_cmds.py: the script engine's commands the port leaves null (zinterp.h), ported for preservation:
// the world's partition settings, the camera's getters and settings, the sequences.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/display.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zclass/seq.h"
#include "gamez/zclass/window.h"
#include "gamez/zeffect/zeff_anim_events.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_init.h"
#include "gamez/zmodel/gmod_hit.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zrender/zrndr_blit.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zerror/zerr.h"

namespace {
// The reports, as (level, line) pairs.
std::vector<int32_t> g_reports;
void sink(int level, const char*, int line, const char*) { g_reports.push_back(level), g_reports.push_back(line); }
int32_t take_reports(int32_t* out, int32_t cap) {
  const int32_t n = static_cast<int32_t>(g_reports.size());
  for (int32_t i = 0; i < n && i < cap; ++i) out[i] = g_reports[i];
  g_reports.clear();
  zerr::set_sink(nullptr);
  return n;
}
}  // namespace

extern "C" {

// One world command on a world whose data is `raw` (0xbc bytes, written back). With `grid`, a grid of cols x rows
// partitions; partition i's count is counts[i] (-1: no node list), its nodes' flags in `flags` in order (written
// back). which: 0 virtual map size (a, b), 1 virtual partition (a), 2 partition tolerance (fa, fb), 3 max dec (a).
int32_t probe_world_cmd(int32_t which, uint8_t* raw, int32_t a, int32_t b, float fa, float fb, int32_t grid,
                        const int16_t* counts, uint32_t* flags, int32_t* reports, int32_t cap) {
  zerr::set_sink(sink);
  zcls::Node world{};
  auto* w = new zcls::World{};
  std::memcpy(w->raw, raw, sizeof w->raw);
  world.data = w;
  const int32_t cols = w->get<int32_t>(0x88), rows = w->get<int32_t>(0x8c);
  std::vector<zcls::Node> nodes;
  if (grid) {
    int32_t total = 0, k = 0;
    for (int32_t i = 0; i < cols * rows; ++i) total += counts[i] > 0 ? counts[i] : 0;
    nodes.resize(total);
    w->rows = new zcls::Partition*[rows];
    for (int32_t z = 0; z < rows; ++z) {
      w->rows[z] = new zcls::Partition[cols]();
      for (int32_t x = 0; x < cols; ++x) {
        zcls::Partition& p = w->rows[z][x];
        const int16_t n = counts[z * cols + x];
        p.put<int16_t>(0x3a, n < 0 ? static_cast<int16_t>(3) : n);
        if (n < 0) continue;
        p.nodes = new zcls::Node*[n > 0 ? n : 1];
        for (int16_t i = 0; i < n; ++i, ++k) {
          nodes[k].set_flags(flags[k]);
          p.nodes[i] = &nodes[k];
        }
      }
    }
  }
  int32_t r = 0;
  switch (which) {
    case 0: r = zcls::world_set_virtual_map_size(&world, a, b); break;
    case 1: r = zcls::world_set_virtual_partition(&world, a); break;
    case 2: r = zcls::world_set_partition_tolerance(&world, fa, fb); break;
    default: r = zcls::world_set_partition_max_dec(&world, a); break;
  }
  for (size_t i = 0; i < nodes.size(); ++i) flags[i] = nodes[i].flags();
  std::memcpy(raw, w->raw, sizeof w->raw);
  if (w->rows) {
    for (int32_t z = 0; z < rows; ++z) {
      for (int32_t x = 0; x < cols; ++x) delete[] w->rows[z][x].nodes, w->rows[z][x].nodes = nullptr;
      delete[] w->rows[z];
    }
    delete[] w->rows;
    w->rows = nullptr;
  }
  delete w;
  reports[0] = r;
  return take_reports(reports + 1, cap - 1);
}

// One camera command. which: 0 dynamic LoD (on, f0), 1 hidden-surface test (on), 2 get translate, 3 get clip,
// 4 get fov, 5 get LoD scale. A camera node of class `cls` whose data is `raw` (0x1e8 bytes; none with
// `null_data`), or no node. out: the result, then the getter's three floats, then the walk's +0x51c, +0x520,
// +0x518; the reports after.
int32_t probe_camera_cmd(int32_t which, const uint8_t* raw, int32_t cls, int32_t null_node, int32_t null_data,
                         int32_t on, float f0, int32_t walk_dyn, float walk_target, int32_t walk_hse, uint32_t* out,
                         int32_t* reports, int32_t cap) {
  zerr::set_sink(sink);
  zcls::SceneWalk& wk = zcls::scene_walk();
  const zcls::SceneWalk saved = wk;
  wk.lod_dynamic = walk_dyn, wk.lod_target = walk_target, wk.hidden_surface = walk_hse;
  zcls::Node node{};
  node.put<int32_t>(0x34, cls);
  zcls::Camera cam{};
  std::memcpy(cam.raw, raw, sizeof cam.raw);
  if (!null_data) node.data = &cam;
  zcls::Node* n = null_node ? nullptr : &node;
  float v[3] = {};
  int32_t r = 0;
  switch (which) {
    case 0: zcls::camera_set_dynamic_lod(on, f0); break;
    case 1: r = zcls::camera_set_object_hse_test(on); break;
    case 2: r = zcls::camera_get_translate(n, &v[0], &v[1], &v[2]); break;
    case 3: r = zcls::camera_get_clip(n, &v[0], &v[1]); break;
    case 4: r = zcls::camera_get_fov(n, &v[0], &v[1]); break;
    default: r = zcls::camera_get_lod_scale(n, &v[0]); break;
  }
  out[0] = static_cast<uint32_t>(r);
  std::memcpy(out + 1, v, sizeof v);
  out[4] = static_cast<uint32_t>(wk.lod_dynamic);
  std::memcpy(out + 5, &wk.lod_target, 4);
  out[6] = static_cast<uint32_t>(wk.hidden_surface);
  wk = saved;
  return take_reports(reports, cap);
}

// One sequence command on a sequence whose header is `header` (0x20 bytes, written back) and entries {node index,
// time} (`entries`, count at header +0x1c; written back, one more after an add). Node indices: 0 the sequence,
// 1.. the children (`kids` of them). which: 0 create (out: class, list 11's head is it), 1 add child (child index
// `a`, at `b`, time `t`), 2..5 set active, repeat, loop, pause (a). The node null with null_node, its data with
// null_data; the child null with a 0.
int32_t probe_seq_cmd(int32_t which, uint8_t* header, int32_t* entry_nodes, float* entry_times, int32_t kids,
                      int32_t null_node, int32_t null_data, int32_t a, int32_t b, float t, int32_t* out,
                      int32_t* reports, int32_t cap) {
  zerr::set_sink(sink);
  zcls::nodes() = zcls::NodePool{};
  zcls::reset_lists();
  if (which == 0) {
    zcls::Node* n = zcls::seq_create();
    auto* s = static_cast<zcls::Seq*>(n->data);
    std::memcpy(header, s->raw, sizeof s->raw);
    out[0] = n->cls();
    out[1] = zcls::list_head(zcls::kSeqs) && zcls::list_head(zcls::kSeqs)->node == n;
    out[2] = s->entries != nullptr;
    return take_reports(reports, cap);
  }
  std::vector<zcls::Node*> all(kids + 1);
  for (auto& p : all) p = zcls::node_alloc();
  auto* s = new zcls::Seq{};
  std::memcpy(s->raw, header, sizeof s->raw);
  const int32_t count = s->get<int32_t>(0x1c);
  if (count > 0) {
    s->entries = static_cast<zcls::Seq::Entry*>(std::calloc(count, sizeof(zcls::Seq::Entry)));
    for (int32_t i = 0; i < count; ++i) s->entries[i] = {all[entry_nodes[i]], entry_times[i]};
  }
  all[0]->put<int32_t>(0x34, 7);
  if (!null_data) all[0]->data = s;
  zcls::Node* n = null_node ? nullptr : all[0];
  int32_t r = 0;
  switch (which) {
    case 1: r = zcls::seq_add_child(n, a ? all[a] : nullptr, b, t); break;
    case 2: r = zcls::seq_set_active(n, a); break;
    case 3: r = zcls::seq_set_repeat(n, a); break;
    case 4: r = zcls::seq_set_loop(n, a); break;
    default: r = zcls::seq_set_pause(n, a); break;
  }
  std::memcpy(header, s->raw, sizeof s->raw);
  const int32_t after = s->get<int32_t>(0x1c);
  for (int32_t i = 0; i < after && s->entries; ++i) {
    int32_t k = -1;
    for (int32_t j = 0; j <= kids; ++j)
      if (all[j] == s->entries[i].node) k = j;
    entry_nodes[i] = k, entry_times[i] = s->entries[i].time;
  }
  out[0] = r;
  // The child linked under the sequence (0x004eba90): the sequence's child count.
  out[1] = all[0]->child_count();
  all[0]->data = nullptr;
  delete s;
  zcls::nodes() = zcls::NodePool{};
  zcls::reset_lists();
  return take_reports(reports, cap);
}

// One window command on a window of class `cls` whose data is `raw` (0xf8 bytes, written back); none with null_data,
// no node with null_node. which: 0 set buffer (a), 1 set clear polygon (a), 2 add clear vertex (xyz), 3 close.
int32_t probe_window_cmd(int32_t which, uint8_t* raw, int32_t cls, int32_t null_node, int32_t null_data, int32_t a,
                         const float* xyz, int32_t* reports, int32_t cap) {
  zerr::set_sink(sink);
  zcls::Node node{};
  node.put<int32_t>(0x34, cls);
  zcls::Window win{};
  std::memcpy(win.raw, raw, sizeof win.raw);
  if (!null_data) node.data = &win;
  zcls::Node* n = null_node ? nullptr : &node;
  int32_t r = 0;
  switch (which) {
    case 0: r = zcls::window_set_buffer(n, a); break;
    case 1: r = zcls::window_set_clear_polygon(n, a); break;
    case 2: r = zcls::window_add_clear_vertex(n, xyz); break;
    default: r = zcls::window_close_clear_polygon(n); break;
  }
  std::memcpy(raw, win.raw, sizeof win.raw);
  reports[0] = r;
  return take_reports(reports + 1, cap - 1);
}

// NewDisplay: the data (0x1c bytes) and out = {class, list 15's head is it}.
void probe_display_create(uint8_t* raw, int32_t* out) {
  zcls::nodes() = zcls::NodePool{};
  zcls::reset_lists();
  zcls::Node* n = zcls::display_create();
  std::memcpy(raw, static_cast<zcls::Display*>(n->data)->raw, 0x1c);
  out[0] = n->cls();
  out[1] = zcls::list_head(zcls::kDisplays) && zcls::list_head(zcls::kDisplays)->node == n;
  zcls::nodes() = zcls::NodePool{};
  zcls::reset_lists();
}

namespace {
std::string g_printed;
}  // namespace

// A list of `count` items on list `list`: item i's node named names[i] (0x24-byte slots), or removed with
// present[i] 0. which 0: ListPrint, the text into `text`. which 1: the prefix walk, `calls` calls, each starting
// with keys[k] (0x24-byte slots) or stepping on (has_key[k] 0); each found node's item index (-1 none) in found.
int32_t probe_list_cmd(int32_t which, int32_t list, const char* names, const int32_t* present, int32_t count,
                       const char* keys, const int32_t* has_key, int32_t calls, int32_t* found, char* text,
                       int32_t cap) {
  zcls::nodes() = zcls::NodePool{};
  zcls::reset_lists();
  std::vector<zcls::Node*> made(count);
  for (int32_t i = count; i-- > 0;) {
    made[i] = zcls::node_alloc();
    std::memset(made[i]->raw, 0, 0x24);
    std::memcpy(made[i]->raw, names + 0x24 * i, 0x23);
    zcls::push_front(list, made[i]);
  }
  int32_t i = 0;
  for (zcls::ListItem* it = zcls::list_head(list); it; it = it->next, ++i)
    if (!present[i]) it->node = nullptr;
  int32_t n = 0;
  if (which == 0) {
    g_printed.clear();
    zcls::list_print(list, [](const char* line) { g_printed += line; });
    std::strncpy(text, g_printed.c_str(), cap - 1);
    text[cap - 1] = 0;
    n = static_cast<int32_t>(g_printed.size());
  } else {
    zcls::list_walk() = zcls::ListWalk{};
    for (int32_t k = 0; k < calls; ++k) {
      zcls::Node* hit = zcls::list_walk_prefix(has_key[k] ? keys + 0x24 * k : nullptr, list);
      found[k] = -1;
      for (int32_t j = 0; j < count; ++j)
        if (made[j] == hit) found[k] = j;
    }
  }
  zcls::nodes() = zcls::NodePool{};
  zcls::reset_lists();
  return n;
}

// NodeSetLighting on a tree: node i's parent parent[i] (-1 the root, node 0), its model's flags model_flags[i]
// (written back; has_model[i] 0: none).
int32_t probe_lighting(const int32_t* parent, const int32_t* has_model, uint32_t* model_flags, int32_t count,
                       int32_t on) {
  std::vector<zcls::Node> nodes(count);
  std::vector<zmod::Model> models(count);
  std::vector<std::vector<zcls::Node*>> kids(count);
  for (int32_t i = 1; i < count; ++i) kids[parent[i]].push_back(&nodes[i]);
  for (int32_t i = 0; i < count; ++i) {
    std::memcpy(models[i].header + 8, &model_flags[i], 4);
    if (has_model[i]) nodes[i].model = &models[i];
    nodes[i].put<int32_t>(0x5c, static_cast<int32_t>(kids[i].size()));
    nodes[i].children = kids[i].data();
  }
  const int32_t r = zcls::node_set_lighting(&nodes[0], on);
  for (int32_t i = 0; i < count; ++i) std::memcpy(&model_flags[i], models[i].header + 8, 4);
  for (auto& n : nodes) n.children = nullptr;
  return r;
}

// One model command on a model whose header is `header` (0x5c bytes, written back) with npoly polygons: polygon i's
// flags poly_flags[i] (written back), its vertex indices from `indices` (flags & 0xff of them, in order), its
// material mats[poly_mat[i]] (0x20 bytes each of the exe's layout up to +0x20, written back). morphs: the vertex
// count's three floats each, written. which: 0 facade (a), 1 show backface (a), 2 colour (a), 3 morph (fixed,
// a = their count, f = v, b = min uses).
void probe_model_cmd(int32_t which, uint8_t* header, uint32_t* poly_flags, const uint32_t* indices,
                     const int32_t* poly_mat, uint8_t* mats, int32_t nmat, const int32_t* fixed, int32_t a, int32_t b,
                     float f, float* morphs) {
  zmod::Model m{};
  std::memcpy(m.header, header, sizeof m.header);
  const int32_t npoly = static_cast<int32_t>(m.count(0x10));
  std::vector<zmod::Polygon> polys(npoly > 0 ? npoly : 0);
  std::vector<zmod::Material> mt(nmat);
  std::vector<std::vector<uint32_t>> vi(polys.size());
  for (int32_t i = 0; i < nmat; ++i) {
    const uint8_t* r = mats + 0x20 * i;
    mt[i].alpha = r[0], mt[i].flags = r[1];
    std::memcpy(&mt[i].rgb, r + 2, 2);
    std::memcpy(&mt[i].r, r + 4, 12);
    std::memcpy(mt[i].unk_14, r + 0x14, 12);
  }
  size_t at = 0;
  for (int32_t i = 0; i < npoly; ++i) {
    polys[i].flags = poly_flags[i];
    vi[i].assign(indices + at, indices + at + (poly_flags[i] & 0xff));
    at += poly_flags[i] & 0xff;
    polys[i].vertex_indices = vi[i].data();
    polys[i].material = &mt[poly_mat[i]];
  }
  m.polygons = polys.data();
  switch (which) {
    case 0: zmod::model_set_facade(&m, static_cast<uint32_t>(a)); break;
    case 1: zmod::model_set_show_backface(&m, static_cast<uint32_t>(a)); break;
    case 2: zmod::model_set_color(&m, a); break;
    default: zmod::model_set_morph(&m, fixed, f, a, b); break;
  }
  std::memcpy(header, m.header, sizeof m.header);
  for (int32_t i = 0; i < npoly; ++i) poly_flags[i] = polys[i].flags;
  for (int32_t i = 0; i < nmat; ++i) {
    uint8_t* r = mats + 0x20 * i;
    r[0] = mt[i].alpha, r[1] = mt[i].flags;
    std::memcpy(r + 2, &mt[i].rgb, 2);
    std::memcpy(r + 4, &mt[i].r, 12);
    std::memcpy(r + 0x14, mt[i].unk_14, 12);
  }
  if (which == 3 && m.morphs) std::memcpy(morphs, m.morphs, 12 * m.count(0x14));
  std::free(m.morphs);
  m.morphs = nullptr;
  m.polygons = nullptr;
}

// The texture world setters and AnimDebugFrame: out = base u, v, per meter u, v, the debug frame.
void probe_misc_cmd(int32_t which, float u, float v, uint32_t frame, uint32_t* out) {
  const zmod::TextureWorld saved = zmod::texture_world();
  const uint32_t saved_frame = zcls::frames().frame;
  zcls::frames().frame = frame;
  if (which == 0) zmod::texture_world_base(u, v);
  else if (which == 1) zmod::texture_world_per_meter(u, v);
  else if (which == 2) zeff::anim_debug_frame();  // 3: only read
  std::memcpy(out, zmod::texture_world().base, 8);
  std::memcpy(out + 2, zmod::texture_world().per_meter, 8);
  out[4] = static_cast<uint32_t>(zeff::anim_debug_frame_at());
  zmod::texture_world() = saved;
  zcls::frames().frame = saved_frame;
}

// VideoSetWireFrame: the fill mode after, the display's dirty bits unchanged.
void probe_wireframe(int32_t mode, int32_t* out) {
  zvid::Display& d = zvid::display();
  const zvid::Display saved = d;
  d.dirty = 0x54;
  out[0] = zvid::set_wireframe(mode);
  out[1] = d.fill_mode, out[2] = static_cast<int32_t>(d.dirty);
  d = saved;
}

namespace {
int32_t g_reloads = 0;
}  // namespace

// The texture marks' painter (0x00542060): a material of `mflags` whose texture's image is tw x th (palette count
// tpal) with pixels `tpix` (written back), the mark image mw x mh (mpal) with `mpix` and `malpha` (null: none) as
// the impact type's map; the capture on (`on`) at uv (written back); the display format; a surface or not.
// Returns the reload hook's calls.
int32_t probe_mark_paint(int32_t mflags, int32_t tw, int32_t th, int32_t tpal, uint16_t* tpix, int32_t mw,
                         int32_t mh, int32_t mpal, uint16_t* mpix, uint8_t* malpha, float* uv, int32_t on,
                         int32_t format, int32_t surface) {
  zmod::UvCapture& cap = zmod::uv_capture();
  const zmod::UvCapture saved = cap;
  const int32_t saved_format = zrndr::target2d().format;
  auto saved_hooks = zimg::texture_hooks();
  zimg::Texture timg{}, mimg{};
  timg.width = static_cast<int16_t>(tw), timg.height = static_cast<int16_t>(th);
  timg.palette_count = static_cast<int16_t>(tpal), timg.pixels = tpix;
  mimg.width = static_cast<int16_t>(mw), mimg.height = static_cast<int16_t>(mh);
  mimg.palette_count = static_cast<int16_t>(mpal), mimg.pixels = mpix, mimg.alpha = malpha;
  zimg::TextureEntry target{}, map{};
  target.image = &timg;
  target.surface = surface ? &timg : nullptr;
  map.image = &mimg;
  zmod::Material m{};
  m.flags = static_cast<uint8_t>(mflags);
  m.texture = &target;
  cap.on = on, cap.type = 1, cap.maps[1] = &map, cap.uv[0] = uv[0], cap.uv[1] = uv[1];
  zrndr::target2d().format = format;
  g_reloads = 0;
  zimg::texture_hooks().reload = [](void*, zimg::Texture*) { ++g_reloads; };
  zmod::mark_paint(&m);
  uv[0] = cap.uv[0], uv[1] = cap.uv[1];
  cap = saved;
  zrndr::target2d().format = saved_format;
  zimg::texture_hooks() = saved_hooks;
  return g_reloads;
}

// Anim events 20 and 21 (which) on a def whose node ref 1 is a camera of class `cls` with data `raw` (0x1e8 bytes,
// written back; none with `null_node`), the event `ev` (0x6c bytes), a sequence in state `state` at time `time`,
// the time left `left` (written back). Returns the result.
int32_t probe_anim_camera(int32_t which, uint8_t* raw, int32_t cls, int32_t null_node, uint8_t* ev, int32_t state,
                          float time, float* left) {
  zcls::Node cam{};
  cam.put<int32_t>(0x34, cls);
  zcls::Camera data{};
  std::memcpy(data.raw, raw, sizeof data.raw);
  cam.data = &data;
  zeff::NodeRef40 refs[3]{};
  refs[1].node = null_node ? nullptr : &cam;
  zeff::AnimDef def{};
  def.nodes = refs;
  zeff::SeqDef seq{};
  seq.raw[0x20] = static_cast<uint8_t>(state);
  std::memcpy(seq.raw + 0x28, &time, 4);
  const float saved = zeff::anim().time_left();
  zeff::anim().time_left() = *left;
  const int32_t r = which == 20 ? zeff::ev_camera_state(def, &seq, ev) : zeff::ev_camera_from_to(def, &seq, ev);
  *left = zeff::anim().time_left();
  zeff::anim().time_left() = saved;
  std::memcpy(raw, data.raw, sizeof data.raw);
  return r;
}

namespace {
std::string g_shown;
float g_shown_for = 0;
}  // namespace

// Anim event 38 on a table of `count` message records (0x24 bytes each); out: the text shown and for how long
// (an id's text is "text <id>"). Returns the result.
int32_t probe_anim_message(uint8_t* ev, uint8_t* records, int32_t count, char* shown, float* seconds) {
  zeff::AnimState& a = zeff::anim();
  zeff::AnimMessage* saved = a.messages;
  std::vector<zeff::AnimMessage> msgs(count > 0 ? count : 1);
  for (int32_t i = 0; i < count; ++i) std::memcpy(msgs[i].raw, records + 0x24 * i, 0x24);
  a.messages = msgs.data();
  g_shown = "(none)", g_shown_for = 0;
  const int32_t r = zeff::ev_message(
      ev,
      [](int32_t id) -> const char* {
        static std::string t;
        t = "text " + std::to_string(id);
        return t.c_str();
      },
      [](const char* t, float s) { g_shown = t ? t : "(null)", g_shown_for = s; });
  a.messages = saved;
  std::strncpy(shown, g_shown.c_str(), 63);
  shown[63] = 0;
  *seconds = g_shown_for;
  return r;
}

// Anim event 16 on a def whose refs 1..3 are nodes; ref `child` linked under ref `parent` first when `linked`.
// Returns the parent's child count after (-1 with a null parent).
int32_t probe_anim_remove_child(int16_t parent, int16_t child, int32_t linked) {
  zcls::nodes() = zcls::NodePool{};
  zcls::reset_lists();
  zeff::NodeRef40 refs[4]{};
  for (int i = 1; i < 4; ++i) {
    refs[i].node = zcls::object3d_create();
  }
  if (linked && parent > 0 && child > 0 && parent != child) zcls::node_add_child(refs[parent].node, refs[child].node);
  zeff::AnimDef def{};
  def.nodes = refs;
  uint8_t ev[0x10] = {};
  std::memcpy(ev + 0xc, &parent, 2), std::memcpy(ev + 0xe, &child, 2);
  zerr::set_sink([](int, const char*, int, const char*) {});
  zeff::ev_object_remove_child(def, ev);
  zerr::set_sink(nullptr);
  const int32_t n = refs[parent].node ? refs[parent].node->child_count() : -1;
  zcls::nodes() = zcls::NodePool{};
  zcls::reset_lists();
  return n;
}

}  // extern "C"
