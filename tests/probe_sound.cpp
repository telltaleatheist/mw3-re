// Probes for test_sound.py (docs/sound.md): zSound on a recording
// platform::SoundOutput that models DirectSound as the test's stub of the
// original does, so the two logs can be compared call for call.
//
// Log entries are int32 words: op, word count, words. Buffers are numbered
// in creation order from 1. Objects (find) and voices (play) live in tables
// the test indexes; their descriptions are compared, not their addresses.
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <list>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "crt/msvc_rand.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zsound/zsnd_cd.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/ztimer.h"

namespace {
using platform::SoundBuffer;
using SO = platform::SoundOutput;

std::vector<int32_t> g_log;
void log(std::initializer_list<int32_t> w) {
  g_log.push_back(*w.begin());
  g_log.push_back(static_cast<int32_t>(w.size() - 1));
  g_log.insert(g_log.end(), w.begin() + 1, w.end());
}
int32_t fbits(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
uint32_t fnv(const void* p, uint32_t n, uint32_t h) {
  const auto* b = static_cast<const uint8_t*>(p);
  for (uint32_t i = 0; i < n; ++i) h = (h ^ b[i]) * 16777619u;
  return h;
}

// The DirectSound model; tests/differential/test_sound.py has the same.
struct Buf {
  uint32_t flags = 0;
  std::vector<uint8_t> data;
  bool playing = false, looping = false;
  uint32_t pos = 0;
  int32_t volume = 0, pan = 0;
  uint32_t freq = 0;
  platform::WaveFormat fmt;
};
const platform::WaveFormat kPrimaryFormat = {1, 2, 22050, 88200, 4, 16};

struct Recorder : SO {
  std::vector<Buf> bufs{Buf{}};  // [0] unused
  uint32_t caps = 0;
  uint32_t extra = 0;  // controls every buffer has as well (the test's "lenient" runs)
  bool ok(SoundBuffer b) const { return b > 0 && static_cast<size_t>(b) < bufs.size(); }

  int32_t create() override { return log({1}), 0; }
  void release_device() override { log({2}); }
  int32_t set_cooperative_level(uint32_t window, uint32_t level) override {
    log({3, static_cast<int32_t>(window), static_cast<int32_t>(level)});
    return 0;
  }
  int32_t get_caps(uint32_t* flags) override {
    log({4});
    *flags = caps;
    return 0;
  }
  int32_t create_buffer(const platform::BufferDesc& d, SoundBuffer* out) override {
    Buf b;
    b.flags = d.flags | extra;
    b.data.assign(d.bytes, 0);
    const platform::WaveFormat f = d.format ? *d.format : platform::WaveFormat{0, 0, 0, 0, 0, 0};
    b.fmt = (d.flags & kPrimary) ? kPrimaryFormat : f;
    b.freq = b.fmt.samples_per_sec;
    bufs.push_back(b);
    *out = static_cast<SoundBuffer>(bufs.size() - 1);
    log({5, *out, static_cast<int32_t>(d.flags), static_cast<int32_t>(d.bytes), d.format ? 1 : 0, f.format_tag,
         f.channels, static_cast<int32_t>(f.samples_per_sec), static_cast<int32_t>(f.avg_bytes_per_sec),
         f.block_align, f.bits_per_sample});
    return 0;
  }
  int32_t duplicate_buffer(SoundBuffer src, SoundBuffer* out) override {
    Buf b = bufs[src];
    b.playing = false, b.pos = 0;
    bufs.push_back(b);
    *out = static_cast<SoundBuffer>(bufs.size() - 1);
    log({6, src, *out});
    return 0;
  }
  void release(SoundBuffer b) override { log({7, b}); }
  int32_t get_format(SoundBuffer b, platform::WaveFormat* f) override {
    log({8, b});
    *f = bufs[b].fmt;
    return 0;
  }
  int32_t set_format(SoundBuffer b, const platform::WaveFormat& f) override {
    log({9, b, f.format_tag, f.channels, static_cast<int32_t>(f.samples_per_sec),
         static_cast<int32_t>(f.avg_bytes_per_sec), f.block_align, f.bits_per_sample});
    return 0;
  }
  int32_t get_status(SoundBuffer b, uint32_t* s) override {
    const Buf& x = bufs[b];
    *s = (x.playing ? 1u : 0u) | (x.playing && x.looping ? 4u : 0u);
    log({10, b, static_cast<int32_t>(*s)});
    return 0;
  }
  int32_t restore(SoundBuffer b) override { return log({11, b}), 0; }
  int32_t lock(SoundBuffer b, uint32_t off, uint32_t bytes, void** p1, uint32_t* n1, void** p2,
               uint32_t* n2) override {
    log({12, b, static_cast<int32_t>(off), static_cast<int32_t>(bytes)});
    Buf& x = bufs[b];
    const uint32_t size = static_cast<uint32_t>(x.data.size());
    if (off > size || bytes > size) return kInvalidParam;
    const uint32_t first = std::min(bytes, size - off);
    *p1 = x.data.data() + off, *n1 = first;
    *p2 = bytes > first ? x.data.data() : nullptr, *n2 = bytes - first;
    return 0;
  }
  int32_t unlock(SoundBuffer b, void* p1, uint32_t n1, void* p2, uint32_t n2) override {
    uint32_t h = fnv(p1, n1, 2166136261u);
    if (p2) h = fnv(p2, n2, h);
    log({13, b, static_cast<int32_t>(h), static_cast<int32_t>(n1), static_cast<int32_t>(n2)});
    return 0;
  }
  int32_t play(SoundBuffer b, uint32_t flags) override {
    log({14, b, static_cast<int32_t>(flags)});
    bufs[b].playing = true, bufs[b].looping = flags & 1;
    return 0;
  }
  int32_t stop(SoundBuffer b) override {
    log({15, b});
    bufs[b].playing = false;
    return 0;
  }
  int32_t set_current_position(SoundBuffer b, uint32_t p) override {
    log({16, b, static_cast<int32_t>(p)});
    bufs[b].pos = p;
    return 0;
  }
  int32_t get_current_position(SoundBuffer b, uint32_t* play, uint32_t* write) override {
    log({17, b});
    *play = bufs[b].pos, *write = bufs[b].pos;
    return 0;
  }
  int32_t get_volume(SoundBuffer b, int32_t* v) override {
    log({18, b});
    *v = bufs[b].volume;
    return 0;
  }
  int32_t set_volume(SoundBuffer b, int32_t v) override {
    log({19, b, v});
    if (!(bufs[b].flags & kCtrlVolume)) return kControlUnavailable;
    bufs[b].volume = v;
    return 0;
  }
  int32_t set_pan(SoundBuffer b, int32_t v) override {
    const bool can = bufs[b].flags & kCtrlPan;
    log({20, b, v, can ? 1 : 0});
    if (!can) return kControlUnavailable;
    bufs[b].pan = v;
    return 0;
  }
  int32_t get_frequency(SoundBuffer b, uint32_t* hz) override {
    log({21, b});
    if (!(bufs[b].flags & kCtrlFrequency)) return kControlUnavailable;
    *hz = bufs[b].freq;
    return 0;
  }
  int32_t set_frequency(SoundBuffer b, uint32_t hz) override {
    const bool can = bufs[b].flags & kCtrlFrequency;
    log({22, b, can ? static_cast<int32_t>(hz) : -1});
    if (!can) return kControlUnavailable;
    bufs[b].freq = hz;
    return 0;
  }
};
Recorder g_out;

// Engine variables (0x00597d10).
int32_t g_lod = 0, g_mute = 0;
float g_volume = 1.0f;
bool g_vars = false;
void* find_var(const char* name) {
  if (!g_vars) return nullptr;
  if (!std::strcmp(name, "SoundLOD")) return &g_lod;
  if (!std::strcmp(name, "MuteSound")) return &g_mute;
  if (!std::strcmp(name, "SoundVolume")) return &g_volume;
  return nullptr;
}

int32_t g_cb_count = 0;
void* load_cb(zsnd::Object* obj, zrdr::Node*) {
  const char* n = zsnd::name_of(obj);
  log({50, obj->kind, static_cast<int32_t>(n ? std::strlen(n) : 0)});
  return reinterpret_cast<void*>(static_cast<uintptr_t>(0x7000 + g_cb_count++));
}
void free_cb(zsnd::Object* obj, void* user) {
  log({51, obj->kind, static_cast<int32_t>(reinterpret_cast<uintptr_t>(user))});
}
void cue_cb(uint32_t index, zsnd::Sound* s) {
  log({52, static_cast<int32_t>(index), static_cast<int32_t>(s->name ? std::strlen(s->name) : 0)});
}

void zerr_sink(int level, const char*, int line, const char* message) {
  std::vector<int32_t> w = {60, 0, level, line};
  for (const char* p = message; *p; ++p) w.push_back(static_cast<uint8_t>(*p));
  w[1] = static_cast<int32_t>(w.size() - 2);
  g_log.insert(g_log.end(), w.begin(), w.end());
}

std::vector<zsnd::Object*> g_objs{nullptr};
std::vector<zsnd::VoiceBase*> g_voices{nullptr};
std::vector<zsnd::Snapshot*> g_snaps{nullptr};
std::vector<std::list<zsnd::GroupVoice*>*> g_lists{nullptr};
std::map<const void*, int> g_gids;  // group voices by first sight; forgotten when freed
int g_next_gid = 1;
int gid(const void* p) {
  auto it = g_gids.find(p);
  if (it != g_gids.end()) return it->second;
  g_gids[p] = g_next_gid;
  return g_next_gid++;
}

int32_t add_voice(zsnd::VoiceBase* v) {
  if (!v) return 0;
  g_voices.push_back(v);
  return static_cast<int32_t>(g_voices.size() - 1);
}

// The voice as "s<set>.<sound>.<slot>" or "g<id>".
std::string voice_desc(const zsnd::VoiceBase* v) {
  if (!v) return "-";
  if (v->kind == 1) return "g" + std::to_string(gid(v));
  const auto* vc = static_cast<const zsnd::Voice*>(v);
  const zsnd::State& st = zsnd::state();
  for (size_t i = 0; i < st.sets.size(); ++i) {
    zsnd::Set* set = st.sets[i];
    for (size_t j = 0; j < set->sounds.size(); ++j) {
      const zsnd::Sound& s = set->sounds[j];
      int slot = -1;
      if (&s.voice == vc) slot = 0;
      for (size_t k = 0; k < s.dups.size(); ++k)
        if (s.dups[k] == vc) slot = static_cast<int>(k) + 1;
      if (slot >= 0) return "s" + std::to_string(i) + "." + std::to_string(j) + "." + std::to_string(slot);
    }
  }
  return "?";
}

std::string obj_desc(const zsnd::Object* o) {
  if (!o) return "-";
  const char* n = zsnd::name_of(const_cast<zsnd::Object*>(o));
  return std::to_string(o->kind) + ":" + (n ? n : "");
}

std::string g_dump;
void out(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void out(const char* fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  g_dump += buf;
}
uint32_t ub(float f) { return static_cast<uint32_t>(fbits(f)); }

void dump_voice(const zsnd::Voice& v) {
  out(" [b%d k%d l%d v%d p%d %08x %08x %08x %08x %08x %08x]", v.buffer, v.kind, v.locked, v.volume, v.has_pos,
      ub(v.pos[0]), ub(v.pos[1]), ub(v.pos[2]), ub(v.vel[0]), ub(v.vel[1]), ub(v.vel[2]));
}

void dump_entry(const zsnd::GroupEntry* e) {
  for (; e; e = e->next)
    out(" (%d %d %08x %08x %d %s %s)", e->initial, e->count, ub(e->delay), ub(e->weight), e->solo,
        e->name ? e->name : "-", obj_desc(e->sound).c_str());
}

// Where a group voice's entry is: index in the group, then depth in its chain.
std::string entry_desc(const zsnd::GroupVoice* g) {
  if (!g->entry) return "-";
  for (size_t i = 0; i < g->group->entries.size(); ++i) {
    int d = 0;
    for (const zsnd::GroupEntry* e = &g->group->entries[i]; e; e = e->next, ++d)
      if (e == g->entry) return std::to_string(i) + "." + std::to_string(d);
  }
  return "?";
}

void dump_gv(const zsnd::GroupVoice* g) {
  out("  %s %s st%d t%08x r%d e%s w%d v%08x p%d %08x %08x %08x l%d:", voice_desc(g).c_str(), g->group->name,
      g->state, ub(g->timer), g->repeats, entry_desc(g).c_str(), g->wait, ub(g->volume), g->has_pos,
      ub(g->pos[0]), ub(g->pos[1]), ub(g->pos[2]), g->locked);
  for (const zsnd::VoiceBase* v : g->voices) out(" %s", voice_desc(v).c_str());
  out("\n");
}
}  // namespace

extern "C" {

// Mounts the install and opens reader.zbd, once.
int probe_snd_setup(const char* game_dir) {
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(game_dir);
  zerr::set_sink(zerr_sink);
  zrdr::close_archives(true);
  zrdr::init();
  return zrdr::open_archive("zbd\\reader.zbd", true) ? 1 : 0;
}

// A fresh zSound (the old one is dropped, not freed).
void probe_snd_begin(int lod, int mute, float volume, int vars, int archives_on, uint32_t caps, uint32_t seed) {
  zsnd::state() = zsnd::State{};
  zsnd::state().archives_on = archives_on;
  zsnd_cd::state() = zsnd_cd::State{};
  g_out = Recorder{};
  g_out.caps = caps;
  g_lod = lod, g_mute = mute, g_volume = volume, g_vars = vars != 0;
  g_cb_count = 0;
  g_objs.assign(1, nullptr), g_voices.assign(1, nullptr), g_snaps.assign(1, nullptr), g_lists.assign(1, nullptr);
  g_gids.clear(), g_next_gid = 1;
  zsnd::Hooks& h = zsnd::hooks();
  h.output = &g_out;
  h.find_var = find_var;
  h.cd_open = [](zrdr::Node*) { log({40}); };
  h.cd_close = []() { log({41}); };
  crt::msvc_srand(seed);
  g_log.clear();
}

int probe_snd_log(int32_t* out_, int max) {
  const int n = static_cast<int>(g_log.size());
  if (n > max) return -n;
  std::memcpy(out_, g_log.data(), sizeof(int32_t) * n);
  g_log.clear();
  return n;
}

void probe_snd_time(float dt, float total) {
  ztimer::state().dt = dt;
  ztimer::state().total_raw = total;
}

int probe_snd_init(uint32_t window) { return zsnd::init(window); }
int probe_snd_load(uint32_t window, const char* name, int cb, int demo) {
  return zsnd::load(window, name, cb ? load_cb : nullptr, demo);
}
void probe_snd_set_archives(int on) { zsnd::set_archives(on); }
void probe_snd_load_set(const char* name) { zsnd::load_set(name); }
void probe_snd_unload_set(const char* name) { zsnd::unload_set(name); }
int probe_snd_close(int cb) { return zsnd::close_device(cb ? free_cb : nullptr); }
int probe_snd_shutdown() { return zsnd::shutdown(); }

int probe_snd_find(const char* name) {
  zsnd::Object* o = zsnd::find(name);
  if (!o) return 0;
  g_objs.push_back(o);
  return static_cast<int>(g_objs.size() - 1);
}
int probe_snd_obj(int i, char* buf, int max) {
  std::snprintf(buf, max, "%s", obj_desc(g_objs[i]).c_str());
  return 0;
}
int probe_snd_voice(int i, char* buf, int max) {
  std::snprintf(buf, max, "%s", voice_desc(g_voices[i]).c_str());
  return 0;
}

int probe_snd_play(int o, float vol) { return add_voice(zsnd::play(g_objs[o], vol)); }
int probe_snd_play_at(int o, const float* pos, float vol, const float* vel) {
  return add_voice(zsnd::play_at(g_objs[o], pos, vol, vel));
}
int probe_snd_play_fading(int o, const float* pos, float vol, const float* vel, float to, int stop) {
  return add_voice(zsnd::play_fading(g_objs[o], pos, vol, vel, to, stop));
}
int probe_snd_status(int v) { return zsnd::status(g_voices[v]); }
int probe_snd_stop(int v) { return zsnd::stop(g_voices[v]); }
int probe_snd_freq(int v, float t) { return zsnd::set_frequency(g_voices[v], t); }
int probe_snd_vvol(int v, float x) { return zsnd::set_voice_volume(g_voices[v], x); }
int probe_snd_lock(int v, int on) { return on ? zsnd::lock(g_voices[v]) : zsnd::unlock(g_voices[v]); }
int probe_snd_fade(int v, int stop, float level) { return add_voice(zsnd::fade(g_voices[v], stop, level)); }
int probe_snd_update3d(int v, const float* pos, const float* vel, int doppler) {
  return zsnd::update_3d(static_cast<zsnd::Voice*>(g_voices[v]), pos, vel, doppler);
}
void probe_snd_frame(int a, int b) { zsnd::frame(a, b); }
int probe_snd_listener(const float* m, const float* vel) { return zsnd::set_listener(m, vel); }
void probe_snd_volume(float v) { zsnd::set_volume(v); }
void probe_snd_cue(int o) { zsnd::set_cue_fn(g_objs[o], cue_cb); }
int probe_snd_snapshot() {
  g_snaps.push_back(zsnd::snapshot());
  return static_cast<int>(g_snaps.size() - 1);
}
int probe_snd_snap_op(int s, int op) {
  if (op == 0) return zsnd::stop_snapshot(g_snaps[s]);
  if (op == 1) return zsnd::resume_snapshot(g_snaps[s]);
  const int r = zsnd::free_snapshot(g_snaps[s]);
  g_snaps[s] = nullptr;
  return r;
}
int probe_snd_take_groups() {
  g_lists.push_back(zsnd::take_playing());
  return static_cast<int>(g_lists.size() - 1);
}
int probe_snd_groups_op(int l, int op) {
  std::list<zsnd::GroupVoice*>* p = g_lists[l];
  g_lists[l] = nullptr;
  if (op != 0 && p)
    for (zsnd::GroupVoice* g : *p) g_gids.erase(g);
  return op == 0 ? zsnd::restore_playing(p) : zsnd::free_playing(p);
}
void probe_snd_reset() { zsnd::reset(); }
void probe_snd_stop_queue(int f) { zsnd::stop_queue(f); }
void probe_snd_finish(int b) {
  if (b > 0 && static_cast<size_t>(b) < g_out.bufs.size()) {
    Buf& x = g_out.bufs[b];
    if (x.playing && !x.looping) x.playing = false, x.pos = 0;
  }
}
void probe_snd_extra_controls(uint32_t f) { g_out.extra = f; }
int probe_snd_buffers() { return static_cast<int>(g_out.bufs.size() - 1); }

// Everything the test compares besides the calls.
const char* probe_snd_dump() {
  const zsnd::State& st = zsnd::state();
  g_dump.clear();
  out("state %d %d caps%08x pf%d", st.initialized, st.open, st.caps, st.playback_current);
  for (int i = 0; i < 3; ++i)
    if (st.has_playback[i]) out(" [%d %d %d]", st.playback[i].rate, st.playback[i].bits, st.playback[i].channels);
    else out(" [-]");
  const platform::WaveFormat f = st.has_primary_format ? st.primary_format : platform::WaveFormat{0, 0, 0, 0, 0, 0};
  out(" prim%d %d %d %d %d %d", st.primary, f.channels, f.samples_per_sec, f.avg_bytes_per_sec, f.block_align,
      f.bits_per_sample);
  out(" sos%08x %08x lis%d search %s muted %d vol%08x\n", ub(st.speed_of_sound), ub(st.inv_speed_of_sound),
      st.listener_set, st.has_search ? st.search.c_str() : "-", st.muted, ub(st.master()));
  for (size_t i = 0; i < st.sets.size(); ++i) {
    const zsnd::Set* set = st.sets[i];
    out("set %zu %s %d %zu subs", i, set->name.c_str(), set->loaded, set->sounds.size());
    for (const zsnd::Set* s : set->subsets) out(" %s", s ? s->name.c_str() : "-");
    out("\n");
    for (const zsnd::Sound& s : set->sounds) {
      out(" %s %s f%x q%08x v%08x r%08x %08x a%08x c%u u%d", s.name ? s.name : "-", s.file ? s.file : "-",
          s.flags, ub(s.queue_life), ub(s.volume), ub(s.range_min), ub(s.range_max), ub(s.a3d_dist), s.cue_count,
          static_cast<int>(reinterpret_cast<uintptr_t>(s.user)));
      for (size_t k = 0; k < s.cue_times.size(); ++k)
        out(" %08x/%u/%08x", ub(s.cue_times[k]), s.cue_bytes[k], ub(s.cue_at[k]));
      for (const zsnd::Format& fm : s.formats)
        out(fm.has_file ? " {%s}" : " {%s%d %d %d}", fm.has_file ? fm.file.c_str() : "", fm.rate, fm.bits,
            fm.channels);
      dump_voice(s.voice);
      for (const zsnd::Voice* d : s.dups) dump_voice(*d);
      out("\n");
    }
  }
  if (st.groups)
    for (const zsnd::Group* g : *st.groups) {
      out("group %s d%d s%d f%08x r%d %08x %08x x%d:", g->name ? g->name : "-", g->dynamic, g->solo,
          ub(g->factor), g->repeat, ub(g->delay_repeat), ub(g->delay_term),
          static_cast<int>(reinterpret_cast<uintptr_t>(g->extra)));
      for (const zsnd::GroupEntry& e : g->entries) dump_entry(&e);
      out("\n");
    }
  out("playing %d\n", st.playing ? static_cast<int>(st.playing->size()) : -1);
  if (st.playing)
    for (const zsnd::GroupVoice* g : *st.playing) dump_gv(g);
  out("spare %d:", st.spare ? static_cast<int>(st.spare->size()) : -1);
  if (st.spare)
    for (const zsnd::GroupVoice* g : *st.spare) out(" %s", voice_desc(g).c_str());
  out(" fin %s %d ready %d\n", st.finished ? voice_desc(st.finished).c_str() : "-", st.finished_count,
      st.groups_ready);
  out("queue %08x %08x %s:", ub(st.queue_clock), ub(st.queue_next), voice_desc(st.queued).c_str());
  if (st.queue)
    for (const zsnd::Queued* q : *st.queue)
      out(" (%s %08x %d %d %08x %08x %08x %08x)", q->sound->name, ub(q->volume), q->has_pos, q->has_vel,
          ub(q->pos[0]), ub(q->vel[0]), q->start, ub(q->expires));
  out("\ncue %s %s %u %u\n", st.cue_sound ? st.cue_sound->name : "-", voice_desc(st.cue_voice).c_str(),
      st.cue_index, st.cue_limit);
  out("fades %zu:", st.free_fades.size());
  for (const zsnd::Fade* fd : st.fades)
    out(" (%s %08x %08x %d)", voice_desc(fd->voice).c_str(), ub(fd->target), ub(fd->current), fd->stop);
  out("\n");
  return g_dump.c_str();
}

}  // extern "C"
