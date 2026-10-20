#include "mech3/mech_data.h"

#include <cstdio>

#include "gamez/zerror/zerr.h"
#include "gamez/zreader/zreader.h"
#include "mech3/mech_config.h"
#include "mech3/mech_names.h"
#include "mech3/mech_reader.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
MechDataCalls g_calls;
const char kFile[] = "C:\\zipper\\Mech3\\mech_data.cpp";
constexpr uint32_t kGaitFrames = 60;    // 0x3c, every gait resampled to 60 keys
constexpr uint32_t kActionFrames = 20;  // 0x14, passed and unused (0x00421120)

const MechDfn* dfn_of(Pilot& p) { return g_calls.dfn ? g_calls.dfn(p) : mech_dfn(p.roster); }

const Config* first_config(Pilot& p) {
  const MechDfn* d = dfn_of(p);
  if (!d || !d->configs || d->configs->configs.empty()) return nullptr;  // 0x00432950(0): 0, then a crash
  return d->configs->configs.front();
}
}  // namespace

MechDataCalls& mech_data_calls() { return g_calls; }

// Mech3.exe 1.2 @ 0x00431560
const char* gait_name(int i) {
  switch (i) {
    case 0: case 1: return "limp";
    case 2: case 12: return "stand";
    case 3: return "step";
    case 4: return "walk";
    case 5: return "trot";
    case 6: return "run";
    default: return "unknown";
  }
}

// Mech3.exe 1.2 @ 0x004316e0
const char* action_name(int i) {
  static const char* const names[] = {"unknown", "fallf", "fallb", "landf", "landb",
                                      "getupf", "getupb", "collapsel", "collapser"};
  return i >= 1 && i <= 8 ? names[i] : names[0];
}

// Mech3.exe 1.2 @ 0x00433030 (0x00433050 with its flag set)
float engine_speed(int32_t engine, float mass) {
  const float rating = static_cast<float>(engine_rating(Engine{engine, 0}));
  return rating / mass * 1.5f * 10.8f * 0.2778f;  // 0x005bec10, 0x005bec04
}

float config_speed(Pilot& p, bool roster_mass) {
  const Config* c = first_config(p);
  if (!c) return 0.0f;
  return engine_speed(c->engine, max_mass(roster_mass ? p.roster : c->mech));
}

// Mech3.exe 1.2 @ 0x00431350. The motion is <name>_<gait> unless the
// gait's block names one ("anim"); "trans" replaces the throttle bounds.
// Stand and step keep no hip velocity; the limps run at their own stride
// speed, the right one the left mirrored.
bool load_gait(Pilot& p, zrdr::Node* motion, int i) {
  float pair[2] = {p.trans[i][0], p.trans[i][1]};
  const char* g = gait_name(i);
  zrdr::Node* n = zrdr::find(motion, g);
  char name[128];
  if (const char* s = zrdr::get_string(n, "anim")) std::snprintf(name, sizeof name, "%s", s);
  else std::snprintf(name, sizeof name, "%s_%s", p.name, g);
  if (zrdr::get_vec2(n, "trans", pair)) p.trans[i][0] = pair[0], p.trans[i][1] = pair[1];
  if (i == 2 || i == 3 || i == 12) {
    p.anims[i] = load_anim(name, 0.0f, kGaitFrames);
    // FIX (bug): the exe frees the hip's velocity keys of a missing stand or
    // step without a check (a crash); never with the shipped motions.
    if (p.anims[i]) zsis::free_velocities(p.anims[i]->hip_part);
  } else {
    const float speed = i == 0 || i == 1 ? p.limp_speed : config_speed(p, false);
    p.anims[i] = load_anim(name, speed, kGaitFrames);
  }
  if (!p.anims[i]) return false;
  if (i == 1) p.anims[1]->mirror_init(p.limp_speed, kGaitFrames);
  return true;
}

// Mech3.exe 1.2 @ 0x004315d0. The one-shots run at 20 frames a second; the
// collapses at 15, the left one the right mirrored, and a mech without a
// collapse falls backwards (fallb at 20) instead.
bool load_action_motion(Pilot& p, int i) {
  float fps = 20.0f;
  char name[128];
  std::snprintf(name, sizeof name, "%s_%s", p.name, action_name(i == 7 ? 8 : i));
  p.actions[i] = load_action(name, kActionFrames);
  if (i == 7 || i == 8) {
    if (!p.actions[i]) {
      std::snprintf(name, sizeof name, "%s_fallb", p.name);
      p.actions[i] = load_action(name, kActionFrames);
    } else {
      fps = 15.0f;
    }
  }
  if (p.actions[i]) {
    p.actions[i]->set_fps(fps);
    if (i == 7) p.actions[7]->mirror_init(0.0f, p.actions[7]->frame_count);
  }
  return p.x20 != 0;
}

// Mech3.exe 1.2 @ 0x00431a70. The gaits, their bounds with stand.hi, step
// and walk.lo forced, a report where a gait's bounds do not overlap the
// next's, the footfalls, and the step turned so its left footfall falls on
// the walk's key; then the one-shots.
void load_animations(Pilot& p) {
  const MechDfn* d = dfn_of(p);
  zrdr::Node* anims = zrdr::find(d ? d->reader : nullptr, "animations");
  zrdr::Node* canned = zrdr::find(anims, "canned");
  for (int i = 0; i < 3; ++i)
    if (g_calls.canned) g_calls.canned(p, canned, i);
  zrdr::Node* motion = zrdr::find(anims, "motion");
  for (int i : {2, 3, 4, 5, 6, 0, 1}) load_gait(p, motion, i);
  p.trans[2][1] = 0.01f;
  p.trans[3][0] = 0.0f;
  p.trans[3][1] = 0.05f;
  p.trans[4][0] = 0.02f;
  char msg[256];
  for (int i = 2; i <= 6; ++i) {
    const bool rising = p.trans[i][0] < p.trans[i][1];
    const bool overlaps = i >= 6 || p.trans[i][1] > p.trans[i + 1][0];
    if (!rising) {
      std::snprintf(msg, sizeof msg, "AnimTransition ( %s ): [%s]  min = %.3f > max = %.3f", p.name, gait_name(i),
                    double{p.trans[i][0]}, double{p.trans[i][1]});
      zerr::report(0x200, kFile, 0x31b, msg);
    }
    if (!overlaps) {
      std::snprintf(msg, sizeof msg, "AnimTransition ( %s ): [%s max = %.3f] < [%s max = %.3f]", p.name,
                    gait_name(i), double{p.trans[i][1]}, gait_name(i + 1), double{p.trans[i + 1][0]});
      zerr::report(0x200, kFile, 0x320, msg);
    }
  }
  MechAnim* step = p.anims[3];
  MechAnim* walk = p.anims[4];
  step->footfalls();
  walk->footfalls();
  walk->set_phase(walk->events[0], true);
  const int32_t wk = walk->key0;
  step->set_phase(step->events[0], true);
  const int32_t sk = step->key0;
  const int32_t k = wk - sk;
  step->rotate_keys(k > 0 ? step->frame_count - static_cast<uint32_t>(k) : static_cast<uint32_t>(-k));
  step->footfalls();
  for (int i : {2, 1, 6, 5, 4, 3, 7, 8}) load_action_motion(p, i);
}

// Mech3.exe 1.2 @ 0x00431fb0
MechAnim* gait_copy(const Pilot& p, int i) {
  if (!p.anims[i]) return nullptr;
  auto* a = new MechAnim;
  a->copy_from(*p.anims[i]);
  return a;
}

// Mech3.exe 1.2 @ 0x00432030 (0x00420e60)
MechAction* action_copy(const Pilot& p, int i) {
  if (!p.actions[i]) return nullptr;
  auto* a = new MechAction;
  a->copy_from(*p.actions[i]);
  a->bind_leg_parts();
  return a;
}

// Mech3.exe 1.2 @ 0x004329c0
void scale_gaits(Pilot& p, float gait[13][2], float max_speed) {
  const float k = config_speed(p, true) / max_speed;
  for (int i = 0; i < 13; ++i) {
    gait[i][0] = k * p.trans[i][0];
    gait[i][1] = k * p.trans[i][1];
  }
}

}  // namespace mech3
