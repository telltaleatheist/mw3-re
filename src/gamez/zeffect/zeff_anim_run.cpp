#include "gamez/zeffect/zeff_anim_run.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#pragma STDC FP_CONTRACT OFF

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/light.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zeffect/zeff_anim_events.h"
#include "gamez/zeffect/zeff_anim_puffer.h"
#include "gamez/zeffect/zeff_anim_save.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zmodel/gmod_puff.h"
#include "gamez/zutil/ztimer.h"
#include "gamez/zweapon/zweapon.h"

namespace zeff {

using zcls::Node;

namespace {
const char kRunFile[] = "C:\\zipper\\GameZMech3\\zEffect\\zeff_anim_run.c";
AnimRunHooks g_run_hooks;

AnimState& A() { return anim(); }
Node* world() { return static_cast<Node*>(A().world); }
uint32_t flags(const AnimDef* d) { return d->get<uint32_t>(0x94); }
void set_flags(AnimDef* d, uint32_t f) { d->put<uint32_t>(0x94, f); }
uint8_t& status(AnimDef* d) { return d->raw[0x98]; }
float& fld(AnimDef* d, int off) { return *d->floats(off); }
uint8_t& seq_state(SeqDef* s) { return s->raw[0x20]; }
float& seq_f(SeqDef* s, int off) { return *s->floats(off); }
uint8_t*& seq_cur(SeqDef* s) { return s->cur; }
uint8_t* seq_end(SeqDef* s) { return s->events + s->get<int32_t>(0x3c); }

// x87 comparisons of the shape `fcomp; test ah, 0x41`: "not greater" also
// holds for NaN.
bool not_above(float a, float b) { return !(a > b); }
}  // namespace

AnimRunHooks& anim_run_hooks() { return g_run_hooks; }

// Mech3.exe 1.2 @ 0x00506f40. The events from the start, its reset state.
int32_t anim_seq_restart(SeqDef* seq) {
  if (!seq) return -1;
  seq_f(seq, 0x24) = 0, seq_f(seq, 0x28) = 0;
  seq_cur(seq) = seq->events;
  seq_state(seq) = seq->raw[0x21];
  return 0;
}

namespace {
// The switch at 0x00507a6e: the handler of the current event; -1 for an
// unknown type (reported).
int32_t dispatch(AnimDef* def, SeqDef* seq, uint8_t* e, bool* bad) {
  AnimDef& d = *def;
  SeqDef& s = *seq;
  const AnimRunHooks& h = g_run_hooks;
  switch (e[0]) {
    case 1: return ev_sound(d, e);
    case 2: return ev_sound_node(d, e);
    case 3:
      if (h.effect) h.effect(h.ctx, def, e);
      return 2;
    case 4: return ev_light_state(d, e);
    case 5: return ev_light_animation(d, s, e);
    case 6: return ev_object_active(d, e);
    case 7: return ev_object_translate(d, e);
    case 8: return ev_object_scale(d, e);
    case 9: return ev_object_rotate(d, e);
    case 10: return ev_object_motion(d, s, e);
    case 11: return ev_object_motion_from_to(d, s, e);
    case 12: return ev_object_motion_script(d, s, e);
    case 13: return ev_object_opacity(d, e);
    case 14: return ev_object_opacity_from_to(d, s, e);
    case 15: return ev_object_add_child(d, e);
    case 16:
      if (h.remove_child) h.remove_child(h.ctx, def, e);
      return 2;
    case 17: return ev_object_cycle_texture(d, s, e);
    case 18: return ev_object_connector(d, s, e);
    case 19: return ev_call_connector(d, e);
    case 20:
      if (h.camera_state) h.camera_state(h.ctx, def, e);
      return 2;
    case 21: return h.camera_from_to ? h.camera_from_to(h.ctx, def, seq, e) : 2;
    case 22: return ev_call_sequence(d, e);
    case 23: return ev_stop_sequence(d, e);
    case 24: return ev_call_animation(d, s, e);
    case 25: return ev_stop_animation(e);
    case 26: return ev_reset_animation(e);
    case 27: return ev_invalidate_animation(e);
    case 28: return ev_fog_state(e);
    case 30: return ev_loop(d, s, e);
    case 31: return ev_if(d, s, e);
    case 32: case 33: return ev_else(s);
    case 34: case 39: case 40: return 2;
    case 35: return ev_callback(d, e);
    case 36: return h.fbfx_color ? h.fbfx_color(h.ctx, def, seq, e) : 2;
    case 37: return h.fbfx_wave ? h.fbfx_wave(h.ctx, def, seq, e) : 2;
    case 38:
      if (h.message) h.message(h.ctx, e);
      return 2;
    case 41: return ev_detonate_weapon(d, e);
    case 42: return anim_puffer_state(d, e, d.at_node);
    default:
      *bad = true;
      return 0;
  }
}
}  // namespace

// Mech3.exe 1.2 @ 0x005079b0. Runs events while each finishes at once; a
// sequence whose reset state is 3 restarts (and waits) after its last event.
// The time left this frame (block +0x40) is spent by the timed events.
int32_t anim_run_events(AnimDef* def, SeqDef* seq) {
  if (!def || !seq || !seq_cur(seq)) {
    seq_state(seq) = 2;  // the exe writes through a null seq too
    return -1;
  }
  uint8_t state = seq_state(seq);
  float& left = A().time_left();
  if (state == 1 || seq_cur(seq) > seq->events) {
    seq_f(seq, 0x24) = left + seq_f(seq, 0x24);
    seq_f(seq, 0x28) = left + seq_f(seq, 0x28);
  }
  for (;;) {
    if (state == 0) {
      uint8_t* e = seq_cur(seq);
      const float start = ev::get<float>(e, 8);
      bool wait;
      switch (e[1]) {
        case 1: wait = fld(def, 0xa8) < start; break;
        case 2: wait = seq_f(seq, 0x24) < start; break;
        case 3: wait = seq_f(seq, 0x28) < start; break;
        default: {
          char msg[96];
          std::snprintf(msg, sizeof msg, "Invalid Start Time.\n  Animation: %s\n", def->name());
          zerr::report(0x400, kRunFile, 0x165d, msg);
          return -1;
        }
      }
      if (wait) return 0;
      seq_f(seq, 0x28) = left;
      if (seq_cur(seq) == seq->events) seq_f(seq, 0x24) = left;
    }
    bool bad = false;
    uint8_t* e = seq_cur(seq);
    const int32_t r = dispatch(def, seq, e, &bad);
    if (bad) {
      char msg[96];
      std::snprintf(msg, sizeof msg, "Invalid Sequence Event.\n  Animation: %s\n", def->name());
      zerr::report(0x400, kRunFile, 0x1786, msg);
      return -1;
    }
    seq_state(seq) = static_cast<uint8_t>(r);
    if (e[0] == 30 && r == 0) return 0;  // the loop restarted the sequence
    if (seq_state(seq) == 2) {
      seq_f(seq, 0x28) = 0;
      seq_cur(seq) = seq_cur(seq) + ev::get<int32_t>(seq_cur(seq), 4);
      if (seq_cur(seq) < seq_end(seq)) {
        seq_state(seq) = 0;
      } else if (seq->raw[0x21] == 3) {
        anim_seq_restart(seq);
        return 0;
      }
    }
    if (seq_state(seq) != 0) return 0;
    state = 0;
  }
}

// Mech3.exe 1.2 @ 0x00507de0. ECX the def's object. A def that runs only in
// range of the player (flag 2), in a zone (8) or when its root was drawn
// (4) checks once, then waits a few frames (+0x9b, a counter shared round
// robin over 1..10) before checking again. Every running sequence then gets
// the frame's time; when none is left running the anim stops.
void anim_tick(Node* object) {
  if (!object || !object->owner) return;
  AnimDef* def = static_cast<AnimDef*>(object->owner);
  AnimState& a = A();
  const uint32_t f = flags(def);
  const auto later = [&] {
    def->raw[0x9b] = a.stagger;
    if (++a.stagger > 10) a.stagger = 1;
  };
  if ((f & 2) || (f & 0xc)) {
    if (def->raw[0x9b] > 0) {
      --def->raw[0x9b];
      return;
    }
    bool out = false;
    if (f & 2) {
      if (!a.block.get<int32_t>(0x24)) return;
      const float d2 = distance2_to_player(def->root);
      out = d2 < fld(def, 0x9c) || !(d2 < fld(def, 0xa0));
    }
    if (!out && (f & 8)) {
      if (!a.block.get<int32_t>(0x28)) return;
      std::memcpy(zmod::zones().list, a.block.raw + 0x38, 4);
      if (!zmod::zone_visible(def->root->raw[0x30])) out = true;
    }
    if (!out && (f & 4)) {
      Node* root = def->root;
      if (!(root->flags() & 0x80000000u)) {
        later();
        return;
      }
      root->set_flags(root->flags() & 0x7fffffffu);
    }
    if (out) {
      later();
      return;
    }
  }
  fld(def, 0xa8) = ztimer::state().dt + fld(def, 0xa8);
  bool done = true;
  for (int32_t i = 0; i < def->count(0x108); ++i) {
    SeqDef* s = &def->seqs[i];
    a.time_left() = ztimer::state().dt;
    if (seq_state(s) == 0 || seq_state(s) == 1) {
      if (anim_run_events(def, s) != 0) {
        status(def) = 5;
        zerr::report(0x400, kRunFile, 0x187f, "Animation sequence error.");
      }
    }
    if (seq_state(s) == 0 || seq_state(s) == 1) done = false;
  }
  if (done) anim_stop(def);
}

// Mech3.exe 1.2 @ 0x00507fc0. Each object ref from 0 (the first is empty):
// active, and for an object3d its matrix or its translation, rotation and
// scale, whichever it uses.
int32_t anim_save_states(AnimDef* def) {
  if (!def) return -1;
  for (int32_t i = 0; i < def->count(0x109); ++i) {
    ObjectRef& r = def->objects[i];
    Node* n = r.node;
    if (!n) continue;
    r.put<uint32_t>(0x28, (n->flags() >> 2) & 1);
    if (n->cls() != 5) continue;
    const uint32_t use_matrix = (static_cast<zcls::Object3d*>(n->data)->get<uint32_t>(0) >> 4) & 1;
    r.put<uint32_t>(0x2c, use_matrix);
    if (use_matrix) {
      std::memcpy(r.raw + 0x30, zcls::object3d_matrix(n), 0x30);
    } else {
      float* v = r.floats(0x30);
      zcls::object3d_get_translation(n, v, v + 1, v + 2);
      zcls::object3d_get_rotation(n, v + 3, v + 4, v + 5);
      zcls::object3d_get_scale(n, v + 6, v + 7, v + 8);
    }
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00508090. Puts the saved states back (AUTO_RESET only)
// and turns the models' opacity off (+0x08 bit 3, +0x24 = 0).
int32_t anim_restore_states(AnimDef* def) {
  if (!def || !(flags(def) & 0x10000)) return -1;
  for (int32_t i = 0; i < def->count(0x109); ++i) {
    ObjectRef& r = def->objects[i];
    Node* n = r.node;
    if (!n) continue;
    zcls::node_set_active(n, r.get<int32_t>(0x28));
    if (n->cls() == 5) {
      const float* v = r.floats(0x30);
      if (r.get<int32_t>(0x2c)) {
        zcls::object3d_set_matrix(n, v);
      } else {
        zcls::object3d_set_translation(n, v[0], v[1], v[2]);
        zcls::object3d_set_rotation(n, v[3], v[4], v[5]);
        zcls::object3d_set_scale(n, v[6], v[7], v[8]);
      }
    }
    if (zmod::Model* m = n->model) {
      uint32_t mf;
      std::memcpy(&mf, m->header + 8, 4);
      mf &= ~8u;
      std::memcpy(m->header + 8, &mf, 4);
      std::memset(m->header + 0x24, 0, 4);
    }
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x005069b0. Lights off and out of the world.
int32_t anim_lights_off(AnimDef* def) {
  for (int32_t i = 0; i < def->count(0x10b); ++i) {
    CreatedRef& r = def->lights[i];
    Node* n = static_cast<Node*>(r.made);
    if (!n) continue;
    if (n->flags() & 4) zcls::node_set_active(n, 0);
    if (r.get<int32_t>(0x28)) {
      zcls::world_remove_light(world(), n);
      r.put<int32_t>(0x28, 0);
    }
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00506a20
int32_t anim_puffers_off(AnimDef* def) {
  for (int32_t i = 0; i < def->count(0x10c); ++i)
    if (def->puffers[i].made) zmod::puffer_set_active(static_cast<zmod::Puffer*>(def->puffers[i].made), 0);
  return 0;
}

// Mech3.exe 1.2 @ 0x00506a70. Dynamic sounds off and out of the world.
int32_t anim_sounds_off(AnimDef* def) {
  for (int32_t i = 0; i < def->count(0x10d); ++i) {
    CreatedRef& r = def->sounds[i];
    Node* n = static_cast<Node*>(r.made);
    if (!n) continue;
    if (n->flags() & 4) zcls::node_set_active(n, 0);
    if (r.get<int32_t>(0x28)) {
      zcls::world_remove_sound(world(), n);
      r.put<int32_t>(0x28, 0);
    }
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00508170. Back to rest: out of the world if it was added,
// lights, puffers and sounds off, the anims it called stopped, no tick,
// status 1 (4 from 6), health refilled.
int32_t anim_reset_finish(AnimDef* def) {
  if (!def || status(def) == 5) return -1;
  if (flags(def) & 0x100) {
    if (zcls::world_remove_child(world(), def->node) != 0) return -1;
    set_flags(def, flags(def) & ~0x100u);
  }
  anim_lights_off(def);
  anim_puffers_off(def);
  anim_sounds_off(def);
  for (int32_t i = 0; i < def->count(0x112); ++i) {
    AnimRefRec& r = def->anim_refs[i];
    if (r.ref && r.get<int32_t>(0x40)) anim_stop(r.ref);
    r.ref = nullptr;
  }
  if (def->object) zcls::node_set_action(def->object, nullptr);
  const uint8_t s = status(def);
  if (s != 5 && s != 4) status(def) = s == 6 ? 4 : 1;
  def->put<uint32_t>(0xb0, def->get<uint32_t>(0xac));
  return 0;
}

// Mech3.exe 1.2 @ 0x00508260. ECX the def's object: the reset sequence goes
// on; when it is done the def finishes resetting.
void anim_reset_tick(Node* object) {
  if (!object || !object->owner) return;
  AnimDef* def = static_cast<AnimDef*>(object->owner);
  fld(def, 0xa8) = ztimer::state().dt + fld(def, 0xa8);
  bool finish = true;
  if (def->reset_events) {
    A().time_left() = ztimer::state().dt;
    SeqDef* s = reset_sequence(*def);
    if (seq_state(s) == 0 || seq_state(s) == 1) {
      if (anim_run_events(def, s) != 0) {
        status(def) = 5;
        zerr::report(0x400, kRunFile, 0x19e3, "Animation sequence error.");
      }
      if (seq_state(s) == 0 || seq_state(s) == 1) finish = false;
    }
  }
  if (finish) anim_reset_finish(def);
}

// Mech3.exe 1.2 @ 0x00508310. With a node the def (or a free instance of it)
// is rebound to that node first. `keep` restores the saved states (AUTO_RESET).
// The reset sequence runs now; one that takes time finishes from the tick.
int32_t anim_reset(AnimDef* def, Node* node, int32_t keep) {
  if (!def || status(def) == 5) return -1;
  if (node) {
    if (def->node == node) {
      if (status(def) == 2 || status(def) == 6) {
        anim_stop(def);
        if (!anim_bind(def, node)) return -1;
      } else if (!anim_bind(def, node)) {
        return -1;
      }
    } else {
      while (def->instance && status(def) == 2) def = def->instance;
      if (status(def) == 2) {
        AnimDef* copy = anim_instance(def, node);
        def->instance = copy;
        if (!copy) return -1;
        def = copy;
      }
      if (!anim_bind(def, node)) return -1;
    }
    status(def) = 0;
  }
  set_flags(def, keep ? flags(def) | 0x40 : flags(def) & ~0x40u);
  if (status(def) == 1) return 0;
  if (flags(def) & 0x40) anim_restore_states(def);
  if (def->reset_events) {
    SeqDef* s = reset_sequence(*def);
    anim_seq_restart(s);
    anim_run_events(def, s);
    if (seq_state(s) == 0 || seq_state(s) == 1) {
      zcls::node_set_action_back(def->object, anim_reset_tick);
      return 0;
    }
  }
  anim_reset_finish(def);
  return 0;
}

// Mech3.exe 1.2 @ 0x00506ca0. A run ends: with a reset time below 0 the def
// rests at once (status 3, or 4 from 6); else it waits out the reset time
// (+0xa4) from its tick, or resets now. Its callback hears 0.
int32_t anim_stop(AnimDef* def) {
  if (!def) return -1;
  if ((status(def) == 2 || status(def) == 6) && def->object) {
    if (fld(def, 0xa4) < 0.0f) {
      anim_reset_finish(def);
      const uint8_t s = status(def);
      if (s != 5 && s != 4) status(def) = s == 6 ? 4 : 3;
    } else {
      fld(def, 0xa8) = A().stopped_elapsed ? fld(def, 0xa4) - -0.01f : 0.0f;
      if (not_above(fld(def, 0xa4), fld(def, 0xa8))) anim_wait_tick(def->object);
      else zcls::node_set_action_back(def->object, anim_wait_tick);
    }
    if (def->callback) def->callback(def, def->callback_arg, 0);
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x005085b0. ECX the def's object.
void anim_wait_tick(Node* object) {
  AnimDef* def = static_cast<AnimDef*>(object->owner);
  if (!def) return;
  fld(def, 0xa8) = ztimer::state().dt + fld(def, 0xa8);
  if (!not_above(fld(def, 0xa8), fld(def, 0xa4))) anim_reset_logged(def);
}

// Mech3.exe 1.2 @ 0x00508460
int32_t anim_restart(AnimDef* def) {
  if (!def) return -1;
  if (status(def) == 6) status(def) = 2;
  else if (status(def) == 4) status(def) = 3;
  return anim_reset_logged(def);
}

// Mech3.exe 1.2 @ 0x00508490
int32_t anim_reset_logged(AnimDef* def) {
  if (!def) return -1;
  const int32_t r = anim_reset(def, nullptr, 1);
  log_reset(def, nullptr);
  return r;
}

// Mech3.exe 1.2 @ 0x005084c0. Running (2) becomes 6, anything else 4.
void anim_invalidate(AnimDef* def) {
  if (def && status(def) != 5) status(def) = status(def) == 2 ? 6 : 4;
}

// Mech3.exe 1.2 @ 0x005084f0. A def whose node hangs free (its root is not a
// world or a camera) is added to the world for the run (flag 0x100). Then
// the clock, the saved states (unless 0x40000) and every sequence restart.
int32_t anim_prepare(AnimDef* def) {
  if (!def) return -1;
  Node* root = zcls::node_root(def->node);
  if (!root) return -1;
  uint32_t f = flags(def);
  if (root->cls() == 2 || root->cls() == 1) {
    f &= ~0x100u;
  } else {
    if (zcls::world_add_child(world(), def->node) != 0) return -1;
    f = flags(def) | 0x100u;
  }
  set_flags(def, f);
  fld(def, 0xa8) = 0;
  if ((f & 0x40000) == 0) anim_save_states(def);
  for (int32_t i = 0; i < def->count(0x108); ++i) {
    SeqDef* s = &def->seqs[i];
    s->put<uint16_t>(0x30, 0);
    s->put<uint32_t>(0x2c, 0);
    anim_seq_restart(s);
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00508950. Type 1 names a def (looked up over all defs and
// cached), type 2 a node and the active state wanted; type 3 is never
// checked. +0x00 set: the entry counts when met; clear: unmet fails at once.
// At least +0x111 must count.
int32_t anim_prerequisites(AnimDef* def) {
  const uint8_t n = def->count(0x110);
  if (!n) return 1;
  int32_t met = 0;
  for (int32_t k = 0; k < def->count(0x110); ++k) {
    Prerequisite& p = def->prereqs[k];
    const uint32_t counts = p.get<uint32_t>(0);
    if (p.raw[4] == 1) {
      if (!p.def) {
        const char* want = reinterpret_cast<const char*>(p.raw + 8);
        for (int32_t j = 0; j < A().def_count(); ++j)
          if (std::strcmp(A().defs[j].name(), want) == 0) {
            p.def = &A().defs[j];
            break;
          }
      }
      if (!p.def) return 0;
      if (status(p.def) == 1) {
        if (!counts) return 0;
      } else if (counts) {
        ++met;
      }
    } else if (p.raw[4] == 2 && p.node) {
      if (((p.node->flags() >> 2) & 1) == p.get<uint32_t>(8)) {
        if (counts) ++met;
      } else if (!counts) {
        return 0;
      }
    }
  }
  return met < def->count(0x111) ? 0 : 1;
}

// Mech3.exe 1.2 @ 0x00508760. Starts a def. Running already: with a node, a
// free instance (or a new copy) starts at that node unless one runs there;
// a def added to the world takes its last instance or a new copy. A reset
// time of -99 means "never restart while running". The node rebinds the
// def's lookups; prerequisites must be met; RESET_TIME defs reset first.
AnimDef* anim_call(AnimDef* def, Node* node) {
  int32_t fresh = 0;
  if (!def) return nullptr;
  const uint8_t s = status(def);
  if (s == 6 || s == 4 || s == 5) return nullptr;
  if (s == 3 || s == 2) {
    if (std::fabs(fld(def, 0xa4) - -99.0f) < 0.1) return nullptr;
    if (s == 2) {
      if (flags(def) & 0x100) {
        while (def->instance && status(def) == 2) def = def->instance;
        if (status(def) == 2) {
          AnimDef* copy = anim_instance(def, node);
          def->instance = copy;
          if (!copy) return nullptr;
          def = copy;
          fresh = 1;
        }
      } else {
        if (!node) return nullptr;
        AnimDef* last = def;
        AnimDef* free_one = nullptr;
        for (AnimDef* d = def; d; d = d->instance) {
          if (status(d) == 2) {
            if (d->node == node) return nullptr;
          } else {
            free_one = d;
          }
          last = d;
        }
        if (!free_one) {
          free_one = anim_instance(last, node);
          last->instance = free_one;
          if (!free_one) return nullptr;
        }
        def = free_one;
      }
      if (!def) return nullptr;
    }
  }
  if (node && !anim_bind(def, node)) return nullptr;
  if (!anim_prerequisites(def)) return nullptr;
  if (flags(def) & 0x20) anim_reset(def, nullptr, fresh);
  if (anim_prepare(def) != 0) return nullptr;
  if (!def->object) {
    def->object = zcls::object3d_create();
    char name[0x24];
    std::snprintf(name, sizeof name, "_%s", def->name());
    zcls::node_set_name(def->object, name);
    if (!def->object) return nullptr;
    zcls::node_set_priority(def->object, def->raw[0x9a]);
  }
  def->object->owner = def;
  zcls::node_set_action_back(def->object, anim_tick);
  status(def) = 2;
  return def;
}

namespace {
// The start calls' common middle: a def added to the world starts at the
// origin, rotation 0 unless it is a billboard (0x200). For the plain start
// the "at" pose is all zeros.
void place(AnimDef* d, const float pos[3], const float rot[3]) {
  if (!(flags(d) & 0x100)) return;
  Node* n = d->node;
  if (n->cls() == 1) {
    zcls::camera_set_position(n, pos[0], pos[1], pos[2]);
    if (!(flags(d) & 0x200)) zcls::camera_set_rotation(n, rot[0], rot[1], rot[2]);
  } else if (n->cls() == 5) {
    zcls::object3d_set_translation(n, pos[0], pos[1], pos[2]);
    if (!(flags(d) & 0x200)) zcls::object3d_set_rotation(n, rot[0], rot[1], rot[2]);
  }
}

void clear_ends(AnimDef* d) {
  d->at_node = nullptr, d->at_node2 = nullptr;
  for (int off : {0x78, 0x7c, 0x80, 0x88, 0x8c, 0x90}) d->put<uint32_t>(off, 0);
}

void set_vector(AnimDef* d, float x, float y, float z) {
  const bool moving = !(std::fabs(x) <= 0.01) || !(std::fabs(y) <= 0.01) || !(std::fabs(z) <= 0.01);
  set_flags(d, moving ? flags(d) | 0x80 : flags(d) & ~0x80u);
  fld(d, 0xb8) = x, fld(d, 0xc0) = z, fld(d, 0xbc) = y;
}
}  // namespace

// Mech3.exe 1.2 @ 0x005085e0. The anim's start vector (+0xb8, flag 0x80 when
// any part is over 0.01) carries into its motions. Logged under the def given.
AnimDef* anim_start_at(AnimDef* def, Node* node, const float pos[3], const float rot[3], const float vec[3]) {
  AnimDef* d = anim_call(def, node);
  if (!d) return nullptr;
  place(d, pos, rot);
  set_vector(d, vec[0], vec[1], vec[2]);
  clear_ends(d);
  log_start_at(def, node, pos, rot, vec);
  return d;
}

// Mech3.exe 1.2 @ 0x00508ae0
AnimDef* anim_start(AnimDef* def, Node* node, float x, float y, float z) {
  AnimDef* d = anim_call(def, node);
  if (!d) return nullptr;
  static const float kZero[3] = {};
  place(d, kZero, kZero);
  set_vector(d, x, y, z);
  clear_ends(d);
  log_start(def, node, x, y, z);
  return d;
}

// Mech3.exe 1.2 @ 0x00508c30. `from` and its offset become the def's first
// end (+0x74, +0x78); the vector is optional.
AnimDef* anim_start_connector(AnimDef* def, Node* node, Node* from, const float* from_pos, const float* vec) {
  AnimDef* d = anim_call(def, node);
  if (!d) return nullptr;
  static const float kZero[3] = {};
  place(d, kZero, kZero);
  d->at_node = from;
  for (int i = 0; i < 3; ++i) d->put<float>(0x78 + 4 * i, from_pos ? from_pos[i] : 0.0f);
  d->at_node2 = nullptr;
  for (int off : {0x88, 0x8c, 0x90}) d->put<uint32_t>(off, 0);
  if (!vec || (std::fabs(vec[0]) <= 0.01 && std::fabs(vec[1]) <= 0.01 && std::fabs(vec[2]) <= 0.01)) {
    d->put<uint32_t>(0xc0, 0), d->put<uint32_t>(0xbc, 0);
    set_flags(d, flags(d) & ~0x80u);
    d->put<uint32_t>(0xb8, 0);
  } else {
    set_flags(d, flags(d) | 0x80);
    fld(d, 0xb8) = vec[0], fld(d, 0xbc) = vec[1], fld(d, 0xc0) = vec[2];
  }
  log_connector(def, node, from, from_pos, vec);
  return d;
}

// Mech3.exe 1.2 @ 0x00508dc0. Both ends; the start vector is left as it was.
AnimDef* anim_start_connector_at(AnimDef* def, Node* node, Node* from, const float* from_pos, Node* to,
                                 const float* to_pos) {
  AnimDef* d = anim_call(def, node);
  if (!d) return nullptr;
  static const float kZero[3] = {};
  place(d, kZero, kZero);
  d->at_node = from;
  for (int i = 0; i < 3; ++i) d->put<float>(0x78 + 4 * i, from_pos ? from_pos[i] : 0.0f);
  d->at_node2 = to;
  for (int i = 0; i < 3; ++i) d->put<float>(0x88 + 4 * i, to_pos ? to_pos[i] : 0.0f);
  log_connector_at(def, node, from, from_pos, to, to_pos);
  return d;
}

// Mech3.exe 1.2 @ 0x00508f00
void anim_set_callback(AnimDef* def, AnimDef::Callback cb, void* arg) {
  if (def) def->callback = cb, def->callback_arg = arg;
}

// Mech3.exe 1.2 @ 0x00508fb0
void anim_set_level(int32_t level) { A().level = level; }

// Mech3.exe 1.2 @ 0x00502d40
void anim_set_player(const float pos[3]) {
  std::memcpy(A().block.raw + 0x2c, pos, 12);
  A().block.put<int32_t>(0x24, 1);
}

// Mech3.exe 1.2 @ 0x00502d70
void anim_set_zone(const uint8_t zone[4]) {
  const uint8_t n = zone[0];
  if (!n) return;
  for (int i = 0; i < n; ++i)
    if (zone[i + 1] == 0xff) return;
  std::memcpy(A().block.raw + 0x38, zone, 4);
  A().block.put<int32_t>(0x28, 1);
}

// Mech3.exe 1.2 @ 0x00502d20
// Mech3.exe 1.2 @ 0x0050ace0 (ECX the setting for the run): every def but
// the first, and each of its instances, stopped; [0x0060c1d4] put back.
int32_t anim_stop_all(int32_t keep_elapsed) {
  const int32_t saved = A().stopped_elapsed;  // 0x00502d30
  anim_keep_stopped_elapsed(keep_elapsed);
  for (int32_t j = 1; j < A().def_count(); ++j) {
    anim_stop(&A().defs[j]);
    for (AnimDef* c = A().defs[j].instance; c; c = c->instance) anim_stop(c);
  }
  anim_keep_stopped_elapsed(saved);
  return 0;
}

void anim_keep_stopped_elapsed(int32_t on) { A().stopped_elapsed = on; }

// Mech3.exe 1.2 @ 0x00506ae0 (ECX def, EDX node)
bool anim_uses_node(const AnimDef* def, const Node* node) {
  if (def->node == node || def->root == node) return true;
  for (int i = 0; i < def->count(0x109); ++i)
    if (def->objects[i].node == node) return true;
  for (int i = 0; i < def->count(0x10a); ++i)
    if (def->nodes[i].node == node) return true;
  return false;
}

// Mech3.exe 1.2 @ 0x00506b60
void anim_forget_nodes(AnimDef* def) {
  for (int i = 0; i < def->count(0x109); ++i) def->objects[i].node = nullptr;
  for (int i = 0; i < def->count(0x10a); ++i) def->nodes[i].node = nullptr;
}

// Mech3.exe 1.2 @ 0x00506bc0
int32_t anim_stop_using(const Node* node, int32_t keep_elapsed, int32_t forget) {
  if (!node) return -1;
  const int32_t saved = A().stopped_elapsed;  // 0x00502d30
  anim_keep_stopped_elapsed(keep_elapsed);
  for (int32_t j = 1; j < A().def_count(); ++j) {
    for (AnimDef* d = &A().defs[j]; d; d = d->instance) {
      if (!anim_uses_node(d, node)) continue;
      anim_stop(d);
      if (forget) anim_forget_nodes(d);
    }
  }
  anim_keep_stopped_elapsed(saved);
  return 0;
}

// Mech3.exe 1.2 @ 0x00502df0. Damage over the threshold (+0xb4) comes off
// the health; at 0 or below (also with no damage done, so a def with health
// 0 fires on any hit) the anim starts. Returns the health.
float anim_on_collide(zwep::Handle object, float damage) {
  AnimDef* def = static_cast<AnimDef*>(object);
  if (def->raw[0x99] == 1 || def->raw[0x99] == 2) {
    const float d = damage - fld(def, 0xb4);
    if (!not_above(d, 0.0f)) fld(def, 0xb0) = fld(def, 0xb0) - d;
    if (not_above(fld(def, 0xb0), 0.0f)) {
      static const float kZero[3] = {};
      anim_start_at(def, nullptr, kZero, kZero, kZero);
    }
  }
  return fld(def, 0xb0);
}

// Mech3.exe 1.2 @ 0x00502e60. ECX def, EDX the hit (its +0x6c flag 0x400
// ignores it). A kill tells the weapon code (0x00583710); otherwise it gets
// the health left as a fraction (0x00583730).
uint32_t anim_on_weapon_hit(zwep::Handle object, zwep::WeaponDef* weapon, zwep::HitRecord*, float damage) {
  AnimDef* def = static_cast<AnimDef*>(object);
  if (weapon->flags & zwep::kFreeze) return 0;  // +0x6c 0x400: FREEZE weapons leave animations alone
  if (def->raw[0x99] != 0 && def->raw[0x99] != 2) return 0;
  const float d = damage - fld(def, 0xb4);
  if (!not_above(d, 0.0f)) fld(def, 0xb0) = fld(def, 0xb0) - d;
  if (not_above(fld(def, 0xb0), 0.0f)) {
    static const float kZero[3] = {};
    anim_start_at(def, nullptr, kZero, kZero, kZero);
    zwep::note_kill(reinterpret_cast<zwep::Handle>(1), nullptr);  // ECX 1: killed
    return 0;
  }
  zwep::set_health_after(fld(def, 0xb0) / fld(def, 0xac));
  return 0;
}

float fast_sqrt(float v) {
  int32_t b;
  std::memcpy(&b, &v, 4);
  b = (b >> 1) + 0x1fc00000;
  float r;
  std::memcpy(&r, &b, 4);
  return r;
}

}  // namespace zeff
