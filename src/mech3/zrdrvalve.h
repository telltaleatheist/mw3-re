#pragma once
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <vector>

#include "gamez/zgame/unported.h"
#include "mech3/zai_valve.h"
#include "mech3/zrdraiv.h"

// mech3\ZRdrValve.cpp: valves.zrd, the mission's triggers (docs/ai.md,
// Valves at run time). Each record names a valve and what it does: an
// initial value, a condition over other valves (any/all zero/nonzero of a
// namelist) and actions run when it is set (sounds, animations, a vehicle's
// scan or leave switches, a power-down or up, destroying or re-teaming an
// entity, an attack strategy, a teleport, the mission's end, updates of other
// valves now or after a delay). The file is read as the mission starts
// (0x004e2120); the actions reach other modules through ValveHooks.
namespace mech3::ai {

using zgame::Handle;

// The calls out of the valves' actions, by the original's address. Silent
// ones read or write fields and are not calls in the original.
class ValveHooks : public zgame::Unported {
 public:
  virtual void report(int32_t level, int32_t line, const char* text) {  // 0x0049bfd0 (empty in the release build)
    unported("0x0049bfd0", {level, "C:\\zipper\\Mech3\\ZRdrValve.cpp", line, text});
  }
  virtual Handle sound_find(const char* name);   // 0x0056f6f0: zsnd::find
  virtual void sound_play(Handle sound) { unported("0x0056e7e0", {sound, 0, zgame::bits(1.0f), 0}); }
  virtual Handle anim_find(const char* name) { return unported("0x0050b450", {name}); }
  virtual void anim_run(Handle anim) { unported("0x00508c10", {anim, 0, 0, 0, 0}); }
  // 0x004d7330: zEntity's object of that name ([0x00736558], 0x005a8210),
  // else the targets' (0x0060b580, 0x004d8420), through its vt+0x2c.
  virtual Handle target_find(const char* name) { return unported("0x004d7330", {name}); }
  virtual Handle entity_find(const char* name) { return unported("0x005a8210", {0x736558, name}); }
  virtual Handle as_vehicle(Handle o) { return unported("obj.vt+0x2c", {o}); }
  virtual Handle vehicle_of(Handle o) { return unported("obj.vt+0x40", {o}); }
  virtual Handle mech_of(Handle o) { return unported("obj.vt+0x50", {o}); }
  virtual Handle controls_of(Handle v) { return unported("veh.vt+0x140", {v}); }
  virtual Handle brain_of(Handle c) { return unported("ictl.vt+0x54", {c}); }  // the AI's brain (0x004d1e60)
  virtual void destroy(Handle o) { unported("obj.vt+0x70", {o}); }
  virtual void mech_destroy(Handle m) { unported("0x0041cfa0", {m, 1}); }
  virtual void mech_power(Handle m, int32_t down) { unported("0x0041cc30", {m, down}); }
  virtual void vehicle_place(Handle v, const float pos[3], float yaw) {
    unported("veh.vt+0xdc", {v, zgame::bits(pos[0]), zgame::bits(pos[1]), zgame::bits(pos[2]), zgame::bits(yaw)});
  }
  // The brain's current order's net (its part vt+0x08, 0x004c0030); the
  // controls' set-net (IControls vt+0x04, 0x004e9380).
  virtual Handle brain_order_net(Handle b) { return unported("order.net.vt+0x8", {b}); }
  virtual void controls_set_net(Handle c, Handle net) { unported("ictl.vt+0x4", {c, net}); }
  // Silent: a word of the brain's settings; the AiTarget masks (+0xc0,
  // +0xc4, +0xc8) with [0x0060b56c] cleared.
  virtual void brain_write(Handle b, uint32_t off, uint32_t v) { (void)b, (void)off, (void)v; }
  virtual void target_masks(Handle o, uint32_t team, uint32_t allies, uint32_t enemies) {
    (void)o, (void)team, (void)allies, (void)enemies;
  }
  virtual void mission_abort() { unported("0x0048f730", {0x6098d0}); }     // -> 0x0048f9e0 on the missions
  virtual void mission_complete() { unported("0x0048f710", {0x6098d0}); }  // empty: a report only
  virtual float game_time() { return 0; }  // [0x0060991c], silent
};
ValveHooks& valve_hooks();
void set_valve_hooks(ValveHooks* h);

// A namelist (0x004e5080 copies one): its names, strdup'd (an item that is
// no string gives none).
struct NameList {
  uint8_t x00 = 0;                               // +0x00  the std::list's allocator byte
  std::list<std::optional<std::string>> names;   // +0x04 head, +0x08 size
};

// One record of valves.zrd (0xc8 bytes, 0x004e2190, dtor 0x004e1d70).
struct ValveDef {
  struct Named {           // a name and the value it fires at, and a flag
    char* name = nullptr;
    int32_t value = 0;
    uint8_t flag = 0;
  };
  struct Flag {            // a tag with a value
    uint8_t on = 0;
    int32_t value = 0;
  };
  ValveDef() = default;
  ~ValveDef();
  ValveDef(const ValveDef&) = delete;
  ValveDef& operator=(const ValveDef&) = delete;
  void clear();

  Flag init;               // +0x00  "value" (n)
  Named sound;             // +0x08  "sound" / "sound_once" (name n): flag the once
  Named anim;              // +0x14  "animation" / "animation_once"
  Named scan[4];           // +0x20 scan_aim, +0x2c scan_hit, +0x38 scan_time, +0x44 canleave (vehicle on n)
  Named shutdown;          // +0x50  (vehicle down n): flag the power-down
  Flag complete;           // +0x5c  "missioncomplete" (n)
  Flag abort;              // +0x64  "missionabort"
  Flag reset;              // +0x6c  "missionreset"
  Named destroy;           // +0x74  (entity n)
  char* update = nullptr;  // +0x7c  (valve)
  char* delay_valve = nullptr;  // +0x80  "delayupdate" (valve seconds)
  uint32_t delay = 0;      // +0x84  the item's word (a float)
  char* team_entity = nullptr;  // +0x88  "setteam" (entity team n)
  int32_t team_value = 0;  // +0x8c
  uint32_t team = 0;       // +0x90
  char* attack_entity = nullptr;  // +0x94  "attack_strategy" (entity strategy n)
  int32_t attack_value = 0;  // +0x98
  uint32_t strategy = 0;   // +0x9c
  char* teleport = nullptr;  // +0xa0  "teleport" (entity (x y z) yaw n)
  int32_t teleport_value = 0;  // +0xa4
  float pos[3] = {};       // +0xa8
  float yaw = 0;           // +0xb4
  NameList names;          // +0xb8  "namelist" (name ...)
  uint8_t flags = 0;       // +0xc4  bit 0 any_nonzero, 1 any_zero, 2 all_nonzero, 3 all_zero

  ValveDef* parse(zrdr::Node* value, const char* name);  // 0x004e2330
};

// valves.zrd's file (0x0060b668, vtable 0x005cd5a8, 0x004e5290): the reader
// file base, the record being read and its key, the parse target. The
// editor's cache of records (+0xfc) stays empty in play.
class ValveFile : public RdrFile {
 public:
  ValveFile() : RdrFile("valves", 0x80, 100, 0, 1) {}  // 0x004e1cb0 -> 0x004e5290
  ValveDef* read(int32_t i, const char** name);       // 0x004e5e30
  ValveDef def;                                        // +0x2c
};
ValveFile& valve_file();
void valve_file_reset();  // not original: the tests start from a fresh object

// The actions (vtables 0x005cd458..0x005cd528). Each fires when its valve is
// set to its value (an update and a delayed update whatever the value).
struct ValveAction : Reaction {
  enum Kind : uint32_t {
    Sound = 0x5cd458, SoundOnce = 0x5cd468, Anim = 0x5cd478, AnimOnce = 0x5cd488, Scan = 0x5cd498,
    Mission = 0x5cd4a8, Destroy = 0x5cd4b8, Shutdown = 0x5cd4c8, SetTeam = 0x5cd4d8, Attack = 0x5cd4e8,
    Teleport = 0x5cd4f8, Update = 0x5cd508, Timer = 0x5cd518, DelayUpdate = 0x5cd528,
  };
  explicit ValveAction(Kind k) : kind(k) {}
  Kind kind;
};

struct SoundAction : ValveAction {  // 0x004e2ff0 / 0x004e3070, 0x0c bytes
  SoundAction(const char* name, int32_t v, bool once);
  void fire(Valve& v) override;     // 0x004e3040 / 0x004e30b0
  Handle sound = 0;                 // +0x04  dropped once played (once)
  int32_t value;                    // +0x08
};
struct AnimAction : ValveAction {   // 0x004e30f0 / 0x004e3150
  AnimAction(const char* name, int32_t v, bool once);
  void fire(Valve& v) override;     // 0x004e3120 / 0x004e3180
  Handle anim = 0;                  // +0x04
  int32_t value;                    // +0x08
};
// 0x004e31c0 (0x18 bytes): one of the brain's switches set (scan_aim +0x98,
// scan_hit +0x9c, scan_time +0xa0, canleave +0x3c, 0x004d9c20..0x004d9ca0).
struct ScanAction : ValveAction {
  ScanAction(Handle brain, uint32_t test, int32_t arg, int32_t v);
  void fire(Valve& v) override;     // 0x004e3200
  Handle brain;                     // +0x04
  uint32_t test;                    // +0x08  the function (ScanTest)
  int32_t arg;                      // +0x0c  added to the brain (always 0)
  int32_t value;                    // +0x10
};
struct MissionAction : ValveAction {  // 0x004e3310, 0x0c bytes
  MissionAction(uint32_t fn, int32_t v) : ValveAction(Mission), fn(fn), value(v) {}
  void fire(Valve& v) override;     // 0x004e3330
  uint32_t fn;                      // +0x04  0x0048f730 abort, 0x0048f710 complete
  int32_t value;                    // +0x08
};
// The entity actions: a name strdup'd, the value, an argument.
struct EntityAction : ValveAction {
  EntityAction(Kind k, const char* n, int32_t v) : ValveAction(k), name(n ? n : ""), has_name(n != nullptr), value(v) {}
  std::string name;                 // +0x04
  bool has_name;
  int32_t value;                    // +0x08
};
struct DestroyAction : EntityAction {  // 0x004e33c0
  using EntityAction::EntityAction;
  void fire(Valve& v) override;     // 0x004e34b0
};
struct ShutdownAction : EntityAction {  // 0x004e3560, 0x10 bytes
  ShutdownAction(const char* n, int32_t v, uint8_t down) : EntityAction(Shutdown, n, v), down(down) {}
  void fire(Valve& v) override;     // 0x004e3650
  uint8_t down;                     // +0x0c
};
struct SetTeamAction : EntityAction {  // 0x004e3700
  SetTeamAction(const char* n, int32_t v, uint32_t t) : EntityAction(SetTeam, n, v), team(t) {}
  void fire(Valve& v) override;     // 0x004e37f0
  uint32_t team;                    // +0x0c
};
struct AttackAction : EntityAction {  // 0x004e38d0
  AttackAction(const char* n, int32_t v, uint32_t s) : EntityAction(Attack, n, v), strategy(s) {}
  void fire(Valve& v) override;     // 0x004e39c0
  uint32_t strategy;                // +0x0c  into the brain's +0x40
};
struct TeleportAction : EntityAction {  // 0x004e3a90, 0x1c bytes
  TeleportAction(const char* n, int32_t v, const float p[3], float y);
  void fire(Valve& v) override;     // 0x004e3ba0
  float pos[3];                     // +0x0c
  float yaw;                        // +0x18
};
struct UpdateAction : ValveAction {  // 0x004e3d20 / 0x004e3d70, 8 bytes
  explicit UpdateAction(Valve* t) : ValveAction(Update), target(t) {}
  void fire(Valve& v) override;     // 0x004e3d90: the target's reactions fire
  Valve* target;                    // +0x04
};
struct DelayUpdateAction : ValveAction {  // 0x004e3f50, 0x0c bytes
  DelayUpdateAction(Valve* t, float d) : ValveAction(DelayUpdate), target(t), delay(d) {}
  void fire(Valve& v) override;     // 0x004e3fb0: a timer
  Valve* target;                    // +0x04
  float delay;                      // +0x08
};
struct DelayTimer : ValveAction {   // 0x004e3e60, 0x10 bytes
  DelayTimer(const DelayUpdateAction& a, int32_t v);
  bool tick(float now) override;    // 0x004e3f10
  Valve* target;                    // +0x04
  float time = 0;                   // +0x08  the delay after the game time it was made at
  int32_t value;                    // +0x0c  the trigger's value then
};

// The conditions (0x14 bytes over 0x004e40e0, vtables 0x005cd568..0x005cd598):
// the valve's value computed from the namelist's valves; they cannot be set.
// Each member valve gets an update of the condition's valve (0x004e4080).
struct Condition : ValveValue {
  enum Kind : uint32_t { AnyNonzero = 0x5cd568, AnyZero = 0x5cd578, AllNonzero = 0x5cd588, AllZero = 0x5cd598 };
  Condition(Kind k, const NameList& names, Valve& owner);
  int32_t get() const override;            // 0x004e4360 / 0x004e43b0 / 0x004e4400 / 0x004e4430
  void set(int32_t) override {}            // 0x004a5bc0
  bool replaceable() const override { return false; }  // 0x004e42d0
  Kind kind;
  uint8_t x04;                             // +0x04  the namelist's first byte
  std::vector<Valve*> members;             // +0x08
};

// 0x004e44b0 (from 0x004e2fb0): the valve named `name` made or found, its
// condition, value and actions from the record. Null when the valve cannot be
// made or its condition conflicts with one it has.
Valve* build_valve(const char* name, const ValveDef& def);
// 0x004e2120, from the mission setup (0x00490ab0): every record of valves.zrd.
void valves_load();

}  // namespace mech3::ai
