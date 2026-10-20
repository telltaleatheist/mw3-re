// C:\zipper\GameZMech3\zSound\zsnd_grp.cpp: sound groups, read from
// SOUND_GROUPS and played as little scripts (docs/sound.md section 6). The
// readers 0x00573560 and 0x005739f0 name the file; the list helpers before
// them (0x00573270..0x00573420) and the player after (0x00573cc0..0x00574610)
// have no strings and are kept here.
#include <cstdio>
#include <cstring>

#include "crt/msvc_rand.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/ztimer.h"

namespace zsnd {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zSound\\zsnd_grp.cpp";

float raw_float(const zrdr::Node& n) {
  float f;
  std::memcpy(&f, &n.i, 4);
  return f;
}
int32_t float_bits(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}

void error(int line, const char* what, const char* group) {
  char msg[160];
  std::snprintf(msg, sizeof msg, "Error loading %s for sound group (%s)", what, group ? group : "(null)");
  zerr::report(0x200, kFile, line, msg);
}

bool in_playing(GroupVoice* g) {
  const State& st = state();
  if (!st.playing) return false;
  for (GroupVoice* p : *st.playing)
    if (p == g) return true;
  return false;
}

// 0x00574230: stops the group's voices still playing.
int32_t stop_voices(GroupVoice* g) {
  for (VoiceBase* v : g->voices)
    if (status(v) != 0) stop(v);
  g->state = 4;
  return 1;
}

// 0x005739f0 (ECX the entry's list, EDX the group's name, the entry): a
// sound name with DELAY_PLAY, PLAY_COUNT, PLAY_SOLO, WEIGHT. A first list
// item reads into the same entry; later ones chain new entries after it.
void read_entry(zrdr::Node* node, const char* const* group, GroupEntry* e) {
  if (e->count == 0) e->count = -1;
  e->initial = e->count;
  zrdr::Node* items = node->list;
  for (int32_t i = 1; i < items[0].i; ++i) {
    zrdr::Node& it = items[i];
    if (it.type == zrdr::List) {
      if (i == 1) {
        read_entry(&it, group, e);
      } else {
        auto* next = new GroupEntry{};
        e->next = next;
        read_entry(&it, group, next);
        e = next;
      }
    } else if (it.type == zrdr::String) {
      const char* s = it.s;
      zrdr::Node& v = items[i + 1];
      if (std::strcmp(s, "DELAY_PLAY") == 0) {
        if (v.type == zrdr::Float) e->delay = raw_float(v);
        else if (v.type == zrdr::Int) e->delay = static_cast<float>(v.i);
        else error(0x116, "DELAY_PLAY", *group);
        ++i;
      } else if (std::strcmp(s, "PLAY_COUNT") == 0) {
        if (v.type == zrdr::Float) e->count = static_cast<int16_t>(static_cast<int32_t>(raw_float(v) - 0.5f));
        else if (v.type == zrdr::Int) e->count = static_cast<int16_t>(v.i);
        else error(0x124, "PLAY_COUNT", *group);
        e->initial = e->count;
        ++i;
      } else if (std::strcmp(s, "PLAY_SOLO") == 0) {
        e->solo = 1;
      } else if (std::strcmp(s, "WEIGHT") == 0) {
        if (v.type == zrdr::Float) e->weight = raw_float(v);
        else if (v.type == zrdr::Int) e->weight = static_cast<float>(v.i);
        else error(0x136, "WEIGHT", *group);
        ++i;
      } else {
        e->name = s;
      }
    }
  }
}

// 0x00573e00 (ECX group): a weighted random entry among those with plays
// left. DYNAMIC_WEIGHTS: the pick keeps FACTOR of its weight, the others
// stay above 0.001, and the weights are scaled back to 100.
GroupEntry* pick(Group* g) {
  const int32_t n = static_cast<int32_t>(g->entries.size());
  if (n == 1) return g->entries[0].count != 0 ? &g->entries[0] : nullptr;
  float total = 0.0f;
  for (GroupEntry& e : g->entries)
    if (e.count != 0) total += e.weight;
  const float slack = total * 0.001f;
  const int r = crt::msvc_rand();
  const float target = static_cast<float>(r) * 3.0518509e-05f * total;
  GroupEntry* result = nullptr;
  int32_t picked = float_bits(slack);  // nothing picked: the slack's bits stand in (0 when total is 0)
  float acc = 0.0f;
  for (int32_t i = 0; i < n; ++i) {
    GroupEntry& e = g->entries[i];
    if (e.count == 0) continue;
    acc += e.weight;
    if (!(slack + acc < target)) {
      result = &e;
      picked = i;
      if (g->dynamic) e.weight = g->factor * e.weight;
      break;
    }
  }
  if (g->dynamic) {
    float sum = 0.0f;
    for (int32_t i = 0; i < n; ++i) {
      GroupEntry& e = g->entries[i];
      if (e.count == 0) continue;
      if (i != picked && e.weight < 0.001f) e.weight = 0.001f;
      sum += e.weight;
    }
    const float scale = 100.0f / sum;
    for (GroupEntry& e : g->entries)
      if (e.count != 0) e.weight = scale * e.weight;
  }
  return result;
}

// 0x00573da0: from the start.
int32_t group_start(GroupVoice* gv) {
  gv->timer = 0.0f;
  gv->repeats = 0;
  gv->entry = nullptr;
  gv->voices.clear();
  if (gv->group->entries.empty()) {
    gv->entry = nullptr;
    gv->state = 4;
    return 1;
  }
  gv->entry = pick(gv->group);
  if (gv->entry) {
    gv->state = 1;
    gv->wait = gv->entry->solo;
  } else {
    gv->state = 4;
  }
  return 1;
}

// 0x00574120: a PLAY_SOLO entry still sounding holds the group.
int32_t waiting(GroupVoice* gv) {
  if (gv->wait || gv->entry->solo)
    for (VoiceBase* v : gv->voices)
      if (status(v) != 0) return 1;
  return 0;
}

// 0x00573f90: plays the entries whose delay has passed, in a chain.
void group_step(GroupVoice* gv) {
  if (waiting(gv)) return;
  gv->timer = ztimer::state().dt + gv->timer;
  while (gv->entry && !(gv->timer < gv->entry->delay)) {
    GroupEntry* e = gv->entry;
    if (e->count != 0) {
      if (!e->sound && e->name && std::strcmp(e->name, "NULL") != 0) e->sound = find(e->name);
      if (e->sound) {
        VoiceBase* v = gv->has_pos ? play_at(e->sound, gv->pos, gv->volume, gv->vel) : play(e->sound, gv->volume);
        if (v) gv->voices.push_back(v);
      }
      if (e->count > 0) e->count = static_cast<int16_t>(e->count - 1);
    }
    gv->timer = 0.0f;
    gv->wait = e->solo;
    gv->entry = e->next;
    if (gv->entry && waiting(gv)) return;
  }
  if (gv->entry) return;
  Group* g = gv->group;
  if (gv->repeats == g->repeat) {
    if (g->delay_term > 0.0f) gv->timer = 0.0f, gv->state = 3;
    else gv->state = 4;
  } else {
    if (g->repeat != -1) ++gv->repeats;
    gv->state = 2;
  }
}

// 0x00573cc0: one group's frame. Done groups leave the playing list one per
// frame, once their voices have ended.
int32_t group_tick(GroupVoice* gv) {
  State& st = state();
  for (size_t i = 0; i < gv->voices.size();) {  // 0x005741f0
    if (status(gv->voices[i]) == 0) {
      if (i + 1 < gv->voices.size()) gv->voices[i] = gv->voices.back();
      gv->voices.pop_back();
    } else {
      ++i;
    }
  }
  const float dt = ztimer::state().dt;
  switch (gv->state) {
    case 0: group_start(gv); break;
    case 1: group_step(gv); break;
    case 2:  // 0x00574170
      gv->timer = dt + gv->timer;
      if (!(gv->timer < gv->group->delay_repeat)) {
        gv->timer = 0.0f;
        gv->entry = pick(gv->group);
        gv->state = gv->entry ? 1 : 4;
      }
      break;
    case 3:  // 0x005741c0
      gv->timer = dt + gv->timer;
      if (!(gv->timer < gv->group->delay_term)) gv->timer = 0.0f, gv->state = 4;
      break;
    case 4: {
      bool all = true;
      for (VoiceBase* v : gv->voices)
        if (status(v) != 0) {
          all = false;
          break;
        }
      if (!st.finished && all) {
        gv->voices.clear();
        st.finished = gv;
        ++st.finished_count;
      }
      break;
    }
    default: break;
  }
  return 1;
}

// 0x00574590
void ensure_lists() {
  State& st = state();
  if (!st.groups) st.groups = new std::list<Group*>;
  if (!st.playing) st.playing = new std::list<GroupVoice*>;
  if (!st.spare) st.spare = new std::list<GroupVoice*>;
  st.groups_ready = 1;
}
}  // namespace

// Mech3.exe 1.2 @ 0x00573270 (with 0x00574420 and 0x00573290): stops every
// group and gives every entry its plays back.
void groups_reset() {
  State& st = state();
  if (st.playing)
    for (GroupVoice* g : *st.playing) stop_voices(g);
  if (!st.groups) return;
  for (Group* g : *st.groups)
    for (GroupEntry& e : g->entries)
      for (GroupEntry* p = &e; p; p = p->next) p->sound = nullptr, p->count = p->initial;
}

// Mech3.exe 1.2 @ 0x005732f0: pause takes the playing groups away.
std::list<GroupVoice*>* take_playing() {
  std::list<GroupVoice*>* l = state().playing;
  state().playing = nullptr;
  return l;
}

// Mech3.exe 1.2 @ 0x00573300: and gives them back, ahead of any started since.
int32_t restore_playing(std::list<GroupVoice*>* l) {
  State& st = state();
  if (!l) return 1;
  if (!st.playing) {
    st.playing = l;
    return 1;
  }
  while (!l->empty()) {
    st.playing->push_front(l->front());
    l->pop_front();
  }
  delete l;
  return 1;
}

// Mech3.exe 1.2 @ 0x00573350
int32_t free_playing(std::list<GroupVoice*>* l) {
  if (!l) return 1;
  for (GroupVoice* g : *l) delete g;
  delete l;
  return 1;
}

// Mech3.exe 1.2 @ 0x005733b0 (0x005733d0 the match)
Group* find_group(const char* name) {
  const State& st = state();
  if (!st.groups) return nullptr;
  for (Group* g : *st.groups)
    if (g->name && std::strcmp(g->name, name) == 0) return g;
  return nullptr;
}

// Mech3.exe 1.2 @ 0x00573420 (ECX SOUND_GROUPS, EDX DEMO_GROUPS or null, the
// load callback).
int32_t read_groups(zrdr::Node* node, zrdr::Node* demo, LoadFn fn) {
  State& st = state();
  if (node->type != zrdr::List) return 0;
  if (!st.groups) st.groups = new std::list<Group*>;
  const int32_t demo_count = demo ? demo->list[0].i - 1 : 0;
  zrdr::Node* items = node->list;
  for (int32_t i = 1; i < items[0].i; ++i) {
    bool include = true;
    if (demo) {
      include = false;
      for (int32_t j = 1; j <= demo_count; ++j)
        if (std::strcmp(demo->list[j].s, items[i].list[1].s) == 0) {
          include = true;
          break;
        }
    }
    if (!include) continue;
    if (Group* g = read_group(&items[i], fn)) st.groups->push_front(g);
  }
  return 1;
}

// Mech3.exe 1.2 @ 0x00573560 (ECX the group's list, EDX the load callback):
// its name, DELAY_REPEAT, DELAY_TERMINATION, DYNAMIC_WEIGHTS (factor),
// PLAY_SOLO, REPEAT, EXTRA (the callback's) and the entries. Without weights
// on every entry they all weigh the same.
Group* read_group(zrdr::Node* node, LoadFn fn) {
  if (node->type != zrdr::List) return nullptr;
  auto* g = new Group;
  g->kind = 1;
  zrdr::Node* items = node->list;
  int16_t group_count = 0;
  for (int32_t i = 1; i < items[0].i; ++i) {
    zrdr::Node& it = items[i];
    if (it.type == zrdr::List) {
      g->entries.emplace_back();
      read_entry(&it, &g->name, &g->entries.back());
    } else if (it.type == zrdr::String) {
      const char* s = it.s;
      zrdr::Node& v = items[i + 1];
      if (std::strcmp(s, "DELAY_REPEAT") == 0) {
        if (v.type == zrdr::Float) g->delay_repeat = raw_float(v);
        else if (v.type == zrdr::Int) g->delay_repeat = static_cast<float>(v.i);
        else error(0x1a8, "DELAY_REPEAT", g->name);
        ++i;
      } else if (std::strcmp(s, "DELAY_TERMINATION") == 0) {
        if (v.type == zrdr::Float) g->delay_term = raw_float(v);
        else if (v.type == zrdr::Int) g->delay_term = static_cast<float>(v.i);
        else error(0x1b6, "DELAY_TERMINATION", g->name);
        ++i;
      } else if (std::strcmp(s, "DYNAMIC_WEIGHTS") == 0) {
        g->dynamic = 1;
        if (v.type == zrdr::Float) g->factor = raw_float(v);
        else if (v.type == zrdr::Int) g->factor = static_cast<float>(v.i);
        else error(0x1c6, "DYNAMIC_WEIGHTS", g->name);
        if (!(g->factor > 0.0f)) g->factor = 0.0f;
        if (g->factor > 1.0f) g->factor = 1.0f;
        ++i;
      } else if (std::strcmp(s, "PLAY_SOLO") == 0) {
        g->solo = 1;
      } else if (std::strcmp(s, "REPEAT") == 0) {
        if (v.type == zrdr::Float) g->repeat = static_cast<int16_t>(static_cast<int32_t>(raw_float(v)));
        else if (v.type == zrdr::Int) g->repeat = static_cast<int16_t>(v.i);
        else error(0x1db, "REPEAT", g->name);
        ++i;
      } else if (std::strcmp(s, "EXTRA") == 0) {
        ++i;
        if (fn) g->extra = fn(g, &items[i]);
      } else if (zfix::options().bugs && std::strcmp(s, "PLAY_COUNT") == 0) {
        // FIX (bug): six ammo groups put this entry keyword at group level and
        // the exe takes it as the group's name; here it applies to each entry.
        if (v.type == zrdr::Int) group_count = static_cast<int16_t>(v.i);
        else if (v.type == zrdr::Float) group_count = static_cast<int16_t>(static_cast<int32_t>(raw_float(v) - 0.5f));
        ++i;
      } else {
        g->name = s;
      }
    }
  }
  if (group_count != 0)
    for (GroupEntry& e : g->entries)
      if (e.count == -1) e.count = e.initial = group_count;
  bool unweighted = false;
  for (GroupEntry& e : g->entries)
    if (!(e.weight > 0.0001f)) {
      unweighted = true;
      break;
    }
  if (unweighted) {
    const float w = 100.0f / static_cast<float>(g->entries.size());
    for (GroupEntry& e : g->entries) e.weight = w;
  }
  return g;
}

// Mech3.exe 1.2 @ 0x005742c0 -> 0x00574270
void groups_update() {
  State& st = state();
  if (st.playing) {
    for (auto it = st.playing->begin(); it != st.playing->end(); ++it) group_tick(*it);
  }
  if (st.finished) {
    if (st.playing) {
      for (auto it = st.playing->begin(); it != st.playing->end(); ++it)
        if (*it == st.finished) {
          st.playing->erase(it);
          break;
        }
    }
    if (st.spare) st.spare->push_front(st.finished);
    st.finished = nullptr;
    --st.finished_count;
  }
}

// Mech3.exe 1.2 @ 0x005742d0 (ECX the EXTRA free function). The branch on
// [0x0072c51c] (a world node) is dead: nothing sets it.
int32_t free_groups(FreeFn fn) {
  State& st = state();
  free_playing(st.playing);
  st.playing = nullptr;
  free_playing(st.spare);
  st.spare = nullptr;
  if (st.groups) {
    for (Group* g : *st.groups) {
      if (g->kind == 1) {
        if (fn && g->extra) fn(g, g->extra), g->extra = nullptr;
        for (GroupEntry& e : g->entries) {
          for (GroupEntry* p = e.next; p;) {
            GroupEntry* n = p->next;
            delete p;
            p = n;
          }
        }
        delete g;
      }
    }
    delete st.groups;
    st.groups = nullptr;
  }
  st.groups_ready = 0;
  return 1;
}

// Mech3.exe 1.2 @ 0x00574610: 1 while the group is in the playing list. Its
// voices are asked too, up to the first playing one, and the answer dropped.
int32_t group_status(GroupVoice* g) {
  if (!in_playing(g)) return 0;
  for (VoiceBase* v : g->voices)
    if (status(v) != 0) break;
  return 1;
}

// Mech3.exe 1.2 @ 0x00574470 (ECX group, EDX has a position, the volume,
// position, velocity). PLAY_SOLO groups do not start twice.
VoiceBase* play_group(Group* g, int32_t has_pos, float volume, const float* pos, const float* vel) {
  State& st = state();
  if (!st.groups_ready) ensure_lists();
  if (g->solo && st.playing)
    for (GroupVoice* p : *st.playing)
      if (p->group == g) return nullptr;
  GroupVoice* gv;
  if (st.spare && !st.spare->empty()) {
    gv = st.spare->front();
    st.spare->pop_front();
  } else {
    gv = new GroupVoice;
  }
  *gv = GroupVoice{};
  if (!st.playing) st.playing = new std::list<GroupVoice*>;
  st.playing->push_front(gv);
  gv->volume = volume;
  gv->kind = 1;
  gv->group = g;
  gv->state = 0;
  if (has_pos && pos) {
    gv->has_pos = 1;
    std::memcpy(gv->pos, pos, 12);
    if (vel) std::memcpy(gv->vel, vel, 12);
  }
  group_start(gv);
  return gv;
}

}  // namespace zsnd
