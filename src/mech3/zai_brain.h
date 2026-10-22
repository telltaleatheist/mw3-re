#pragma once
#include <cstdint>
#include <deque>
#include <vector>
#include <memory>

#include "gamez/zgame/unported.h"
#include "mech3/zai_params.h"
#include "mech3/zai_path.h"
#include "mech3/zai_steps.h"

namespace znet { class Object; }

// The pilot's brain (docs/ai.md, The brain's data): what an AI vehicle's
// controls are built around. Its constructor 0x004cf330 copies the vehicle's
// aiv.zrd settings, makes the firing state, the order stack with its first
// order on the vehicle's net, a steering block and, for an allied pilot with
// a radio voice, the lancemate radio. What the brain decides each frame
// (0x004e9710), the behaviours and the firing are not ported: only the data
// they start from. Original files unknown: the brain sits in the AI's big
// unnamed range (0x004cf330..0x004cf810), its orders, radio and net hook with
// the AI vehicle controls (0x004e7c40..0x004ea910), the firing state after
// weapon.cpp (0x004b6390..0x004b7060).
namespace mech3::ai {

class AiNet;
class Brain;
struct Order;

class NetPart;
class BehaviourState;
struct Behaviour;
struct AiTarget;
struct Contact;
class Region;

// What the brain calls outside it. The defaults run the ports.
class BrainHooks : public zgame::Unported {
 public:
  static uint32_t low(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  // The original's operator new and delete for blocks the port owns (silent
  // here; the tests give the port's objects the original's addresses).
  virtual void on_new(uint32_t size, const void* p) { (void)size, (void)p; }
  virtual void on_delete(const void* p) { (void)p; }
  virtual uint32_t ref(const void* p) { return low(p); }
  // A net by number: znet's object (0x0058b160), then the AI's casts.
  virtual znet::Object* net_find(int32_t id);
  // The built-in net at 0x00604008 (made at start-up by 0x004c0310, not
  // ported): the pilot's net when its own is missing.
  virtual AiNet* builtin_net() { return nullptr; }
  // The default behaviour at 0x00604068 (zai_steps): an order's without one.
  virtual Behaviour* default_behaviour();
  // The handle the hooks know a vehicle by (the program's object table).
  virtual zgame::Handle vehicle_handle(void* vehicle) { return low(vehicle); }
  // IControls vt+0xa0 (0x004aed40 -> 0x004aec40(1.0)): hit within the last
  // second (IControls +0x04 a time; what sets it is not ported).
  virtual bool hit_lately(zgame::Handle v) { return unported("0x004aed40", {v}) != 0; }
  // The game time [0x0060991c].
  virtual float now();
  // The pilot's vehicle (the exe's IControls +0x10): its health, vt+0x98
  // (the Mech's 0x0041ba40, its parts' product); its position (0x005a85e0);
  // its AiTarget part (+0x68..), whose contacts the brain reads.
  virtual float health(zgame::Handle v);
  virtual void position(zgame::Handle v, float out[3]);
  virtual AiTarget* targetable(zgame::Handle v) { (void)v; return nullptr; }
  // The plain states' questions (docs/ai.md 17.2): a net's nearest step to a
  // point (0x004d51d0 then vt+0x74; the port's nearest_step), a vehicle's
  // AiTarget +0xbc, the player's pilot's lance state ([0x0060861c] +0x6c:
  // vt+0x48, then vt+0x2c; -1 without a pilot), the formation net
  // 0x00603fb8, the player's vehicle (the pilot's +0x14) and a vehicle's
  // own net's follow behaviour (0x004c0de0, item 10).
  virtual Behaviour* nearest(AiNet* n, const float p[3]);
  virtual uint8_t vehicle_flags(zgame::Handle v);
  virtual int32_t player_lance_state() { return static_cast<int32_t>(unported("[0x0060861c]+0x6c", {})) - 1; }
  virtual AiNet* formation_net() {
    unported("0x00603fb8", {});
    return nullptr;
  }
  virtual zgame::Handle player_vehicle() { return unported("[0x0060861c]+0x14", {}); }
  virtual Behaviour* follow_behaviour(AiNet* vehicle_net) {
    unported("0x004c0de0", {low(vehicle_net)});
    return nullptr;
  }
  // The manoeuvres at 0x00604098 (0) and 0x00604088 (1), class 0x004ca440.
  virtual Behaviour* manoeuvre_behaviour(int32_t which);

  // The brain's frame (0x004e9710, docs/ai.md 17.1). The AI switch
  // [0x0060b938] (0x004ea740 on, 0x004ea750 off).
  virtual bool ai_on();
  virtual int32_t powered_down(zgame::Handle v);               // vt+0xa0 == 1 (the Mech's 0x0041cc10)
  virtual Contact* pick(zgame::Handle v, int32_t side);        // 0x004d6b90 on the vehicle
  // A contact's vehicle: 0x004e6fe0 (its +0x04's vt+0x2c), then vt+0x40.
  virtual zgame::Handle contact_vehicle(const Contact* c);
  virtual float contact_range2(zgame::Handle v);               // a vehicle's AiTarget +0xa4
  virtual void count_awake();                                  // [0x006099c0] += 1
  virtual float intensity();                                   // GameIntensity [0x00608e54]
  virtual int32_t rand();                                      // MSVC's rand (0x005bb668)
  // The vehicle's weapon manager's ammo value 0x004b4ed0 (+0xd4); false
  // without a manager.
  virtual bool ammo_value(zgame::Handle v, float* out);
  // Not ported yet, each an item of the porting order (ai.md 17.9).
  // Steering's questions: a vehicle's contacts (+0xac..+0xb0, nearest
  // first); what a contact sees as an entity (0x004e6fe0: its +0x04's
  // vt+0x2c; 0 for none) and an entity's flat forward (+0x24, +0x2c).
  virtual const std::vector<Contact*>* contacts(zgame::Handle v);
  virtual zgame::Handle contact_entity(const Contact* c) { return unported("0x004e6fe0", {low(c)}); }
  virtual void forward(zgame::Handle e, float* x, float* z);
  virtual void radio_line(Brain& b, int32_t event);            // 0x004e6d50(1.0) on the radio's line (item 14)
  // The vlist triggers' questions (0x004d8e70): the vehicle's heat (vt+0x9c,
  // the Mech's +0x554), a clear line to what a contact sees (0x004d5980 on
  // the vehicle, not ported), a region's inside test (0x004c57d0).
  virtual float heat(zgame::Handle v);
  virtual bool clear_line(zgame::Handle v, zgame::Handle seen);
  virtual bool region_inside(Region* r, const float* p);
  virtual void firing_reset(Brain& b);                         // 0x004e9590 -> 0x004b6700 (item 12)
  virtual void firing_run(Brain& b, float dt);                 // 0x004e9fb0 -> 0x004b5b80 (item 12)
  virtual AiNet* vehicle_net(zgame::Handle v);                 // vt+0xd8: its own net (0x004c0810, item 10)
  virtual Behaviour* attack_behaviour(AiNet* vehicle_net);     // 0x004c0e10 (item 10)
  virtual bool attack(Brain& b, AiNet* current_net);           // 0x004e9ca0 (item 10)
  virtual void own_order_run(Brain& b, float dt);              // the brain's own order's vt+0x04, 0x004e81a0 (item 10)
  virtual void* sound_find(const char* name);  // 0x0056f6f0: zsnd::find
  // The player's vehicle ([0x0060861c] +0x14) and its allies (+0xc4, the
  // AiTarget masks); false when there is no pilot or no vehicle. Silent.
  virtual bool player_allies(uint32_t* allies) {
    (void)allies;
    return false;
  }
  // The walking states' questions (docs/ai.md 17.4a): the brain's way
  // finder's next step after `at` (+0x238, 0x004c7cb0); the vehicle's speed at
  // a share of its top speed (vt+0x118, the Mech's 0x0041d1c0) and its
  // stopping distance from a speed (vt+0x120, 0x0041d210); the mission
  // boundary's pull on a point (0x004c3000, not ported: false, the point
  // kept, as when the world has no boundary); what the steps' passable tests
  // see of the pilot (its vehicle's +0x04 name and vt+0x50's +0x3d8 jump jets).
  virtual Behaviour* path_next(Brain& b, Behaviour* at);
  virtual float speed_at(zgame::Handle v, float share);
  virtual float stopping_distance(zgame::Handle v, float speed);
  virtual bool inout_clamp(zgame::Handle v, float p[3]) {
    (void)p;
    return unported("0x004c3000", {v}) & 0xff;
  }
  virtual void pilot_of(zgame::Handle v, Pilot* p) {
    (void)p;
    unported("vehicle.vt+0x50", {v});
  }
  // The basic vehicle controls' destructor (0x004ac800): the owner's vt+0xec,
  // then 0x00402510 on its answer (veh_basic.cpp, not ported).
  virtual void vehicle_controls_gone(void* owner) { unported("0x004ac800", {low(owner)}); }
};
BrainHooks& brain_hooks();
// The AI switch [0x0060b938]: on at the end of the mission start
// (0x004ea740), off in the cleanup (0x004ea750).
bool& ai_switch();
inline void ai_switch_on() { ai_switch() = true; }    // 0x004ea740
inline void ai_switch_off() { ai_switch() = false; }  // 0x004ea750
void set_brain_hooks(BrainHooks* h);

// What a net builds in an order (its vt+0x5c, in place at the order's
// +0x30; docs/ai.md 17.2). Slot 0 destroys it in place (no free).
class NetPart {
 public:
  explicit NetPart(Order* o) : order(o) {}
  virtual ~NetPart() = default;           // +0x00
  virtual void run(float dt) { (void)dt; }  // +0x04: 0x004bf620 for the shipped kinds
  virtual AiNet* net() const = 0;         // +0x08

  Order* order = nullptr;                 // +0x04
};

// The part a compiled net and the built-in one make: 0x0c bytes over
// 0x004bffe0 (0x004c0070 -> 0x004c0010, vtable 0x005ca748; the built-in's
// 0x004c0520 -> 0x004c0540, 0x005cab50): the order and the net.
class NetLink : public NetPart {
 public:
  NetLink(Order* o, AiNet* n, bool built_in) : NetPart(o), target(n), builtin(built_in) {}
  AiNet* net() const override { return target; }  // 0x004c0030

  AiNet* target = nullptr;  // +0x08
  bool builtin = false;     // the vtable
};

// What a behaviour builds in an order (its vt+0x8c, in place at +0x54),
// over 0x004c8660 (vtable 0x005cb238): the order. The slots' defaults are the
// base's; each behaviour's own state class overrides them (ai.md 17.2).
class BehaviourState {
 public:
  BehaviourState(Order* o, Behaviour* b) : order(o), from(b) {}
  virtual ~BehaviourState() = default;                  // +0x00
  virtual bool may_break_in() const { return true; }    // +0x04 0x00499270
  virtual bool may_fire() const { return true; }        // +0x08 0x004d7210
  virtual void suspend() {}                             // +0x0c 0x0049bfd0
  virtual void resume() {}                              // +0x10 0x0049bfd0
  virtual void run(float dt);                           // +0x14 0x004c8690 (not ported)
  virtual Behaviour* behaviour() const { return nullptr; }  // +0x18 0x0040bae0 for the base

  Order* order = nullptr;  // +0x04
  Behaviour* from = nullptr;
};

// The state of the shared behaviour at 0x0060b460 (class 0x004cbe10, vtable
// 0x005cbe18) every first order runs: 0x004cbe30 -> 0x004cbe50 (vtable
// 0x005cbed0), 8 bytes. Its run 0x004cbeb0 is not ported (ai.md 17.2).
class SharedState : public BehaviourState {
 public:
  explicit SharedState(Order* o);
  bool may_break_in() const override { return false; }  // 0x004d72a0
  bool may_fire() const override { return false; }      // 0x004d7290
  void run(float dt) override;                          // 0x004cbeb0
  Behaviour* behaviour() const override;                // 0x004cbea0: 0x0060b460
};

// The two plain behaviours made at start-up: 0x00604058 (0x00410610, vtable
// 0x005bc988; the formation net 0x00603fb8 and a vehicle's own net are made
// of it) and 0x00604028 (0x00410630, 0x005bca40).
Behaviour* stand_behaviour();
Behaviour* hold_behaviour();

// The shared behaviour at 0x0060b460, made at start-up.
Behaviour* shared_behaviour();

// The built-in net at 0x00604008 (0x004c0310 over the seven-behaviour net,
// not ported): what an order without a net goes on. Its part is the
// built-in kind (0x004c0520).
class BuiltinNet : public CompiledNet {
 public:
  using CompiledNet::CompiledNet;
  NetPart* make_part(Order& o) override;
};

// One order (0x98 bytes, vtables 0x005cd8d4 / 0x005cd8d8, ctor 0x004e7f80,
// 0x004e8030 for the template at 0x0060b898). The deque's orders are
// 0x004e8070's (the template argument unused); the brain's own at +0x194 is
// 0x004e8180's (vtable 0x005cd8e8). Its parts live in place in the exe
// (+0x30, +0x54); here they are owned.
struct Order {
  Order() = default;
  ~Order();                                        // 0x004e7ff0: the state, then the net's part, then the owner cleared
  Order(const Order&) = delete;
  Order& operator=(const Order&) = delete;

  void run(float dt);                              // vt+0x04 0x004e8150: the part, then the state
  void pop();                                      // vt+0x08 0x004e8170: its owner's pop
  bool reset(AiNet* n, Behaviour* b);              // 0x004e80b0: both parts remade
  bool set_behaviour(Behaviour* b);                // 0x004e8120: the state remade

  uint8_t x04 = 0;                  // +0x04  marks the orders a stop takes back (0x004e8c60)
  Brain* owner = nullptr;           // +0x08  0x004e8090 sets it once
  std::unique_ptr<NetPart> net;     // +0x0c  (in place at +0x30)
  std::unique_ptr<BehaviourState> state;  // +0x10  (in place at +0x54)
  int32_t x14 = 0;                  // +0x14  0x004ce200: a word and a point
  float x18[3] = {};                // +0x18
  float x24[3] = {};                // +0x24  0x0063d870 (the zero vector)

  bool set_owner(Brain* b) {                       // 0x004e8090
    if (!owner) owner = b;
    return owner == b;
  }
};

// 0x004b6390 (0x1c bytes, vtable 0x005c9458): a fire plan (ai.md section 1).
struct FirePlan {
  int32_t x04 = 0, x08 = 0;  // +0x04, +0x08
  float x0c[3] = {};          // +0x0c
  int32_t x18 = 0;            // +0x18
};

// 0x004b7060 (0x24 bytes, made by the firing state): aim (likely).
struct Aim {
  float x00[3] = {};              // +0x00  0x0063d870
  float x0c[3] = {1.0f, 0, 0};    // +0x0c  0x005eda60
  uint8_t x18 = 0;                // +0x18
  int32_t x1c = 0;                // +0x1c
  float x20 = 1.0f;               // +0x20
};

// 0x004b6570 (0x58 bytes at the brain's +0x108): the firing state the brain
// runs each frame (0x004b5b80, not ported). Its first 11 words are the
// defaults at 0x005e8868 (8, then zeros).
struct Firing {
  Firing();
  ~Firing();  // 0x004b65f0
  Firing(const Firing&) = delete;
  Firing& operator=(const Firing&) = delete;

  uint32_t x00[11];        // +0x00
  Brain* brain = nullptr;  // +0x2c  0x004b6660
  void* x30 = nullptr;     // +0x30
  FirePlan plan;           // +0x34
  void* x50 = nullptr;     // +0x50
  Aim* aim = nullptr;      // +0x54
};

// The brain's steering (+0x250, 0x48 bytes, 0x004ce620; docs/ai.md 17.4):
// a path segment from `start` along `dir` (x, z), and a carrot on it
// `along_at` ahead of the start and `across` to its side, which the
// vehicle's position is measured from (`along`, `side`). The states set the
// segment (0x004cea30) and steer the controls by it; each frame
// 0x004cec80 eases the carrot and keeps it near, and the contacts in the
// way push it aside.
struct Steer {
  float start[3] = {};   // +0x00  0x0063d870
  float dir[2] = {};     // +0x0c  x, z; [0x0060b4b8] / [0x0060b4bc] at first: never written, 0
  float along_min = 0;   // +0x14  how far back the carrot may fall
  float length = 0;      // +0x18  the segment's
  float along_at = 0;    // +0x1c  the carrot's distance along
  float across = 0;      // +0x20  its offset to the side, eased toward
  float across_to = 0;   // +0x24
  float rate = 0;        // +0x28  how fast the carrot falls back, a second
  float along = 0;       // +0x2c  the vehicle from the carrot, along the segment
  float side = 0;        // +0x30  and across
  float near_dist = 20.0f;  // +0x34  0x005e8bb4: near the segment within this
  float x38;             // +0x38  [0x0060b4a0]: cos(pi/4), set at start-up (0x004ce5b0)
  uint32_t x3c = 0, x40 = 0, x44 = 0;  // +0x3c..+0x44  (0x004ce690 clears +0x3c and +0x44)
  Steer();

  // 0x004cea30: a new segment from `from` toward `to`, the carrot falling
  // back at `rate`; the vehicle measured and the contacts let push.
  void set(const float to[3], const float from[3], float rate, zgame::Handle owner);
  void update(float dt, zgame::Handle owner);         // 0x004cec80
  void measure(zgame::Handle owner, float dt);        // 0x004ceb50
  bool near() const;                                   // 0x004ce6a0
  void carrot(float out[3]) const;                    // 0x004ce6f0
  void project(const float p[3], float out[2]) const;  // 0x004ceb00
  void advance(float d) { along_at += d, along -= d; }  // 0x004ce800
  void shift(float da, float ds) { along -= da, side -= ds, along_at += da, across += ds; }  // 0x004ce820

 private:
  float distance_off() const;  // the measure 0x004ce6a0 and 0x004ceb50 compare with near_dist
  void push_by_contacts(zgame::Handle owner, float dt);
};

// 0x004e7c40 (0x40 bytes): the lancemate radio, sixteen zSound lookups of
// "<voice>_<event>" (0x004e7dc0) in this order.
struct Radio {
  static const char* const kEvents[16];
  void load(const char* voice);
  void* sounds[16] = {};
};

// 0x004c9d00 then 0x004cd770 (0x10 bytes at the brain's +0x240, vtable
// 0x005cc290): a step of the brain's own, pointing at the null step.
struct BrainStep {
  uint8_t flags = 0;    // +0x04  bit 0 cleared (the rest is the heap's)
  int32_t x08 = 0;      // +0x08
  const void* next;     // +0x0c  0x00603fa0, the null step
  BrainStep();
};

// What the brain asks of the controls it is a base of (the exe's through
// its virtual base +0xe0): the AI mech controls' 0x004d8750 family.
struct BrainControls {
  virtual ~BrainControls() = default;
  virtual zgame::Handle vehicle() = 0;    // IControls +0x10: the vehicle
  virtual void stop() = 0;                // IControls vt+0x14 (0x00428490)
  virtual void smooth(float dt) = 0;      // the brain's vt+0x04 (the mech's 0x004d89f0 -> 0x00426960)
  virtual bool hit_lately() = 0;          // IControls vt+0xa0 (0x004aed40)
  virtual void fire(bool on) = 0;         // IControls vt+0x10 (0x00428580: +0x34)
  virtual void set_throttle(float v) = 0; // IControls vt+0x20 (0x00426b60)
  virtual void set_turn(float v) = 0;     // IControls vt+0x28 (0x00426b10)
  virtual float turn_goal() = 0;          // IControls vt+0x30 (0x00426b00: +0x48)
};

// The brain (0x004cf330, 0x2ac bytes and, when it is the most derived
// object, an IControls base at +0x2b0; it never is: the AI controls derive
// from it). Offsets in the comments are the brain's.
class Brain {
 public:
  // 0x004cf330(net, record, 0): the net is the vehicle's, else the record's
  // first item's (znet), else the built-in one.
  Brain(AiNet* net, const AiParams& rec);
  ~Brain();  // 0x004cf630
  Brain(const Brain&) = delete;
  Brain& operator=(const Brain&) = delete;

  // IControls vt+0x04 (0x004e9380): drop every order, then one order on the
  // net. False (nothing changed) when the net is not a compiled one.
  bool set_net(AiNet* net);

  // The order stack (docs/ai.md 17.2). A net or behaviour of none is the
  // built-in net (0x00604008) or the default behaviour (0x00604068).
  Order& current() { return orders.back(); }       // 0x004ea760
  bool remake(AiNet* n);                           // 0x004e82e0: the current order on n
  bool push_net(AiNet* n);                         // 0x004e86a0
  bool push(AiNet* n, Behaviour* b);               // 0x004e87c0
  bool push_behaviour(Behaviour* b);               // 0x004e88c0: on the current order's net
  void pop();                                      // 0x004e8380
  bool pop_if(AiNet* n);                           // 0x004e84a0: when the current order is on n
  Order* top_unmarked();                           // 0x004e8c60
  void unwind_to(Order* o);                        // 0x004e8990
  bool stop();                                     // vt+0x40 0x004ea4e0
  bool may_fire();                                 // 0x004e9070
  // The vlist triggers (docs/ai.md 17.5): 0x004e9550 -> 0x004d9290 over the
  // settings' list, the nearest enemy picked once between them and the frame.
  void vlist(BrainControls& c, zgame::Handle owner, bool* picked, Contact** target);
  void pilot_tick(BrainControls& c);               // IControls vt+0xa4 0x004e94f0: the same, nothing picked yet
  // From the IControls side, given a znet object (its vt+0x34 cast).
  bool pop_if_object(znet::Object* o);             // 0x004e8660
  bool push_object(znet::Object* o);               // 0x004e8960
  bool clear_if_object(znet::Object* o);           // 0x004e9100
  void push_order();  // 0x004e9d80 -> 0x004e9d90: a new order without parts (the template's)

  // The controls it is a base of in the exe (set by the frame and by the AI
  // controls): what its orders' states steer.
  BrainControls* controls = nullptr;

  // Mech3.exe 1.2 @ 0x004e9710: the frame (0x004e96f0 answers it): the
  // targets set and the controls smoothed (ai.md 17.1).
  bool frame(BrainControls& c, float dt);

  // The gates (docs/ai.md 17.3), given the pilot's vehicle.
  bool may_engage(zgame::Handle owner, AiNet* net, BehaviourState* state);  // 0x004e8f10
  bool must_break_off(zgame::Handle owner);                                 // 0x004e8dd0
  bool p6_clear() const;            // vt+0x0c 0x004e9640: +0x54 is 0
  bool sees_target(zgame::Handle owner);  // vt+0x10 0x004e9650
  void manoeuvre();                 // 0x004e96b0

  AiParams params;          // +0x00  0x004d9bf0 -> 0x004d9940 (vtable 0x005cd360, the brain's 0x005cc698)
  float xdc = 3.4028235e38f;  // +0xdc  FLT_MAX (0x004d9bf0)
  // +0xe0: the virtual base pointer.
  const float* xe4 = nullptr;  // +0xe4  the home point the leash measures from (0x004e8f10)
  float xe8 = 3.4028235e38f;  // +0xe8
  float xec = 1.0f;           // +0xec
  int32_t xf0 = 0;            // +0xf0
  float xf4 = 3.4028235e38f;  // +0xf4
  uint8_t xf8 = 0;            // +0xf8
  int32_t xfc = 0;            // +0xfc
  float x100 = 0;             // +0x100  when a hit may start the firing again (0x004e9710)
  uint8_t x104 = 0;           // +0x104  the order asks to run again
  Firing firing;              // +0x108
  // +0x160: a std::deque<Order> (0x004e82a0: allocator byte, two
  // iterators, map, map size, size; blocks of 26 orders, 0xf70 bytes) and
  // at +0x190 its owner, this brain. The last is the current order (+0x17c).
  std::deque<Order> orders;
  const void* order_block = nullptr;  // the deque's first block and map, for the tests
  const void* order_map = nullptr;
  Order order;                                     // +0x194  0x004e8180, its owner this brain
  uint8_t x22c = 2;           // +0x22c  hurt: 2 not decided this frame, 0 / 1 (0x004e8f10)
  float x230 = 0;             // +0x230  the health an attack gives up at (0x004e9710)
  char* x234 = nullptr;       // +0x234  0x0049d180: a string holder, empty
  PathHolder path;            // +0x238  the way finder (0x004c7c10), deleted by the dtor
  void* x23c = nullptr;       // +0x23c  the same
  BrainStep step;             // +0x240
  Steer steer;                // +0x250
  float x298[3] = {};         // +0x298  0x0063d870
  uint8_t x2a4 = 0;           // +0x2a4
  Radio* radio = nullptr;     // +0x2a8  0x004e7e20

 private:
  void pop_order();                                // the pop_back of 0x004e9380 (no resume)
  void pop_resume();                               // the pop of 0x004e8380: then the new current state's resume
  void make_radio();                               // 0x004e7e20
};

// 0x004a4a70 over 0x004a4000 (0x14 bytes, vtable 0x005c8340): a turret's
// limits as sines and cosines (0x004a3fc0), made with -pi/2 and pi/2 (the
// other two arguments, -pi and pi, are not kept).
struct TurretLimits {
  explicit TurretLimits(float lo, float hi);
  float lo_sin, lo_cos;  // +0x04
  float hi_sin, hi_cos;  // +0x0c
};

// The AI vehicle controls (0x004e71c0 over 0x004e7090, 0xcd8 bytes from
// 0x004ac860, which zeroes them): the basic vehicle's controls (0x004ac750,
// veh_basic.cpp: their +0x764 block holds at +0x28 a pointer to the turret
// limits at +0x2c), the brain at +0xa04, the IControls virtual base at +0xcc4
// (its owner +0xcd4).
struct AiVehicleControls {
  AiVehicleControls(void* owner, AiNet* net, const AiParams& rec);  // 0x004e71c0(owner, net, rec, 1)
  ~AiVehicleControls();  // IControls vt+0x00: 0x004e7820 -> 0x004e7870
  AiVehicleControls(const AiVehicleControls&) = delete;
  AiVehicleControls& operator=(const AiVehicleControls&) = delete;

  int32_t x38 = 2;          // +0x38
  TurretLimits turret;      // +0x790 (the pointer to it at +0x78c)
  std::unique_ptr<Brain> brain;  // +0xa04 (a base in the exe)
  float xcb0[3] = {};       // +0xcb0  0x0063d870
  uint8_t xcbc = 0;         // +0xcbc
  void* owner = nullptr;    // +0xcd4
};

// Mech3.exe 1.2 @ 0x004d9290: each trigger of a settings' vlist, in order
// (0x004d8e70); the player's controls run their own (0x004d8990, not ported).
void vlist_run(const AiParams& p, BrainControls& c, zgame::Handle owner, bool* picked, Contact** target);

// The default net for a vehicle without its own: znet's object of the
// record's first item, as a compiled net (vt+0x28, vt+0x34), else the
// built-in one.
AiNet* brain_net_for(const AiParams& rec);

}  // namespace mech3::ai
