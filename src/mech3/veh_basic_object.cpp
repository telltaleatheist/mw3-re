#include "mech3/veh_basic_object.h"

#include <cstring>

#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "mech3/mech_build.h"
#include "mech3/weapon.h"

namespace mech3 {

namespace {
VehBasicHooks g_default_hooks;
VehBasicHooks* g_hooks = &g_default_hooks;
VehicleMarkers g_markers;
EngineSoundList g_engine_sounds;  // 0x0060b110
zsnd::Object* snd(Handle h) { return static_cast<zsnd::Object*>(h); }
zsnd::VoiceBase* voice(Handle h) { return static_cast<zsnd::VoiceBase*>(h); }
}  // namespace

void set_veh_basic_hooks(VehBasicHooks* h) { g_hooks = h ? h : &g_default_hooks; }

void VehBasicHooks::entity_set_node(Vehicle& v, Handle node) {
  unported("0x004d6c90", {ref(&v), low(node), 0, 0, 0});
  vehicle_set_node(v, node);
}

void VehBasicHooks::weapons_free(void* w) { delete static_cast<WeaponManager*>(w); }

Handle VehBasicHooks::sound_find(const char* name) { return zsnd::find(name); }
const char* VehBasicHooks::sound_name(Handle sound) { return zsnd::name_of(snd(sound)); }
void VehBasicHooks::voice_unlock(Handle v) { zsnd::unlock(voice(v)); }
void VehBasicHooks::voice_stop(Handle v) { zsnd::stop(voice(v)); }
Handle VehBasicHooks::anim_find(const char* name) { return zeff::anim_find(name); }
void VehBasicHooks::report(int32_t level, const char* file, int32_t line, const char* message) {
  zerr::report(level, file, line, message);
}
zrdr::Node* VehBasicHooks::reader_find(zrdr::Node* node, const char* key) { return zrdr::find(node, key); }
int32_t VehBasicHooks::reader_type(zrdr::Node* node) { return node->type; }
const char* VehBasicHooks::reader_string(zrdr::Node* node, const char* key) { return zrdr::get_string(node, key); }
bool VehBasicHooks::reader_string_value(zrdr::Node* node, const char* key, const char** out) {
  return zrdr::get_string_value(zrdr::find(node, key), out);
}

EngineSoundList& engine_sounds() { return g_engine_sounds; }

// Mech3.exe 1.2 @ 0x004ad930: the entry whose sound is named `name`, else a
// new one if zSound has the sound and the list has room.
EngineSoundEntry* engine_sound_find(const char* name) {
  VehBasicHooks& h = *g_hooks;
  std::vector<EngineSoundEntry>& list = g_engine_sounds.entries;
  for (EngineSoundEntry& e : list) {
    const char* n = h.sound_name(e.sound);
    if (name && n && std::strcmp(name, n) == 0) return &e;
  }
  EngineSoundEntry made;  // 0x004adb90 -> 0x004adbb0
  made.sound = name ? h.sound_find(name) : nullptr;
  if (!made.sound) return nullptr;
  if (list.size() >= EngineSoundList::kCapacity) {
    h.report(0x200, "C:\\zipper\\Mech3\\veh_basic.cpp", 0x358, "CZEngineSoundList is too small");
    return nullptr;
  }
  list.push_back(made);  // within the reserve: the entries never move
  return &list.back();
}

// Mech3.exe 1.2 @ 0x004add10: each entry's voice stopped (0x004adbf0), then
// the list emptied.
void engine_sounds_clear() {
  VehBasicHooks& h = *g_hooks;
  for (EngineSoundEntry& e : g_engine_sounds.entries) {
    if (e.voice) {
      h.voice_unlock(e.voice);
      h.voice_stop(e.voice);
    }
    e = EngineSoundEntry{};
  }
  g_engine_sounds.entries.clear();
}

// Mech3.exe 1.2 @ 0x004adb20.
void EngineSound::set(const char* name) { entry = engine_sound_find(name); }

// Mech3.exe 1.2 @ 0x004ade40, over 0x004ade70 (the running one stopped).
void MotionAnim::set(const char* name) {
  VehBasicHooks& h = *g_hooks;
  if (anim) {
    h.anim_stop(anim);
    anim = nullptr;
  }
  def = nullptr;
  if (name) def = h.anim_find(name);
}

// Mech3.exe 1.2 @ 0x004ac320 (ECX the controls, EDX the type).
BasicVehicle* vehicle_create(Handle ctl, const char* type, const char* name) {
  VehBasicHooks& h = *g_hooks;
  Handle node = h.node_named(name);
  if (!node) {
    const Handle model = h.node_named(type);
    if (!model) return nullptr;
    node = h.node_clone(model);
    if (!node) return nullptr;
    h.node_set_name(node, name);
    h.world_add(node);
  }
  auto* v = new BasicVehicle;
  h.on_new(0xfc, v);
  vehicle_construct(*v, ctl, node);
  return v;
}

// Mech3.exe 1.2 @ 0x004ac530 (ctl, node).
void vehicle_construct(BasicVehicle& v, Handle ctl, Handle node) {
  VehBasicHooks& h = *g_hooks;
  vehicle_base(v);
  v.controls = ctl;
  v.engine = EngineSound{};  // 0x004adaf0
  v.motion = MotionAnim{};  // 0x004aec00
  v.node = nullptr;
  if (!v.controls) {
    const Handle mem = h.controls_alloc(0xa70);
    v.controls = mem ? h.controls_make(mem, v) : nullptr;
  } else {
    h.controls_set_owner(v.controls, v);
  }
  v.xe0 = 0;
  h.cameras_once();
  if (node) {
    h.entity_set_node(v, node);
    h.entity_register(v);
  }
}

namespace {

// Mech3.exe 1.2 @ 0x004aeb50, the Vehicle base, with AiTarget's 0x004d58a0
// and ZEntity3d's 0x005a7f90.
void vehicle_base_destroy(Vehicle& v) {
  VehBasicHooks& h = *g_hooks;
  if (v.node) {
    h.entity_set_node(v, nullptr);
    h.entity_unregister(v);
  }
  if (v.ammo) {
    h.weapons_free(v.ammo);
    v.ammo = nullptr;
  }
  if (v.net_collision) {
    h.net_collision_free(v.net_collision);
    v.net_collision = nullptr;
  }
  v.clear_contacts();  // 0x004d58a0: 0x004a7be0
  if (v.node) {
    h.entity_set_node(v, nullptr);
    h.entity_unregister(v);
  }
  v.net = 0;
  v.release_shadow();
  if (v.node) h.zentity_set_node(v, nullptr);  // ZEntity3d's 0x005a7f90
  h.entity_unregister(v);                      // there whether listed or not
  h.zentity_set_node(v, nullptr);              // ZEntity's 0x005a8370
}

}  // namespace

// Mech3.exe 1.2 @ 0x0049ec00.
void vehicle_setup(BasicVehicle& v, zrdr::Node* def, const char* name, int32_t kind) {
  VehBasicHooks& h = *g_hooks;
  zrdr::Node* sounds = h.reader_find(def, "sounds");
  if (sounds && h.reader_type(sounds) == zrdr::List) {
    const char* engine = h.reader_string(sounds, "engine");
    if (engine) v.engine.set(engine);
  }
  const char* motion = nullptr;
  if (h.reader_string_value(def, "motion_anim", &motion) && motion) v.motion.set(motion);
  h.controls_setup(v.controls, def, name, kind);
}

// Mech3.exe 1.2 @ 0x004ac650.
void vehicle_destroy(BasicVehicle& v) {
  VehBasicHooks& h = *g_hooks;
  if (v.engine.entry) v.engine.entry = nullptr;  // 0x004adb40
  if (v.controls) h.controls_delete(v.controls);
  const Handle node = v.node;
  v.controls = nullptr;
  if (node) {
    h.node_detach(node);
    h.world_remove(node);
    h.node_free(node);
    h.entity_set_node(v, nullptr);
    h.entity_unregister(v);
  }
  if (v.motion.anim) {  // 0x004addb0 -> 0x004ade70
    h.anim_stop(v.motion.anim);
    v.motion.anim = nullptr;
  }
  vehicle_base_destroy(v);
}

// Mech3.exe 1.2 @ 0x004ac630(1).
void vehicle_delete(BasicVehicle* v) {
  if (!v) return;
  vehicle_destroy(*v);
  delete v;
}

VehicleMarkers& vehicle_markers() { return g_markers; }

// Mech3.exe 1.2 @ 0x004ac3d0.
void vehicle_markers_create() {
  VehBasicHooks& h = *g_hooks;
  g_markers.ally = h.marker_create("hud_veh_ally_sm");
  g_markers.enemy = h.marker_create("hud_veh_enemy_sm");
  g_markers.other = h.marker_create("hud_veh_other_sm");
}

}  // namespace mech3
