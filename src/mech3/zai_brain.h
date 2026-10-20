#pragma once
#include <cstdint>
#include <deque>
#include <memory>

#include "gamez/zgame/unported.h"
#include "mech3/zai_params.h"

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
  virtual void* sound_find(const char* name);  // 0x0056f6f0: zsnd::find
  // The player's vehicle ([0x0060861c] +0x14) and its allies (+0xc4, the
  // AiTarget masks); false when there is no pilot or no vehicle. Silent.
  virtual bool player_allies(uint32_t* allies) {
    (void)allies;
    return false;
  }
  // The basic vehicle controls' destructor (0x004ac800): the owner's vt+0xec,
  // then 0x00402510 on its answer (veh_basic.cpp, not ported).
  virtual void vehicle_controls_gone(void* owner) { unported("0x004ac800", {low(owner)}); }
};
BrainHooks& brain_hooks();
void set_brain_hooks(BrainHooks* h);

// What a net builds in an order's +0x30 (its vt+0x5c: 0x004c0070 for a
// compiled net, 0x004c0520 for the built-in one): the order and the net.
// 0x0c bytes over 0x004bffe0, vtable 0x005ca748 (0x005cab50 built-in).
struct OrderNet {
  bool builtin = false;
  Order* order = nullptr;  // +0x04
  AiNet* net = nullptr;    // +0x08
};

// What a behaviour builds in an order's +0x54 (its vt+0x8c): the order's
// state for it. The orders made here run the shared behaviour at 0x0060b460
// (class 0x004cbe10, vtable 0x005cbe18; what it does is the behaviours'),
// whose state (0x004cbe30 -> 0x004cbe50 over 0x004c8660, vtable 0x005cbed0)
// is 8 bytes: the order.
struct OrderState {
  Order* order = nullptr;  // +0x04
};

// One order (0x98 bytes, vtables 0x005cd8d4 / 0x005cd8d8, ctor 0x004e7f80,
// 0x004e8030 for the template at 0x0060b898). The deque's orders are
// 0x004e8070's (the template argument unused); the brain's own at +0x194 is
// 0x004e8180's (vtable 0x005cd8e8).
struct Order {
  Order() = default;
  ~Order();  // 0x004e7ff0: the state and the net's part destroyed in place
  Order(const Order&) = delete;
  Order& operator=(const Order&) = delete;

  uint8_t x04 = 0;                  // +0x04
  Brain* owner = nullptr;           // +0x08  0x004e8090 sets it once
  OrderNet* net = nullptr;          // +0x0c  -> net_part
  OrderState* state = nullptr;      // +0x10  -> state_part
  int32_t x14 = 0;                  // +0x14  0x004ce200: a word and a point
  float x18[3] = {};                // +0x18
  float x24[3] = {};                // +0x24  0x0063d870 (the zero vector)
  OrderNet net_part;                // +0x30  (0x24 bytes of room)
  OrderState state_part;            // +0x54  (0x44 bytes of room)
  bool own_net = false, own_state = false;  // which parts were built (the pointers say it)

  bool set_owner(Brain* b) {  // 0x004e8090
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

// 0x004ce620 (0x48 bytes at the brain's +0x250): steering toward a point
// (likely: 0x004ce6a0 tests a distance against +0x34).
struct Steer {
  float x00[3] = {};   // +0x00  0x0063d870
  uint32_t x0c = 0;    // +0x0c  [0x0060b4b8], never written: 0
  uint32_t x10 = 0;    // +0x10  [0x0060b4bc], the same
  uint32_t x14[6] = {};  // +0x14..+0x28
  uint32_t x2c = 0;    // +0x2c  [0x0060b4b8]
  uint32_t x30 = 0;    // +0x30  [0x0060b4bc]
  float x34 = 20.0f;   // +0x34  0x005e8bb4
  float x38;           // +0x38  [0x0060b4a0]: cos(pi/4), set at start-up (0x004ce5b0)
  uint32_t x3c = 0, x40 = 0, x44 = 0;  // +0x3c..+0x44
  Steer();
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

  AiParams params;          // +0x00  0x004d9bf0 -> 0x004d9940 (vtable 0x005cd360, the brain's 0x005cc698)
  float xdc = 3.4028235e38f;  // +0xdc  FLT_MAX (0x004d9bf0)
  // +0xe0: the virtual base pointer.
  int32_t xe4 = 0;            // +0xe4
  float xe8 = 3.4028235e38f;  // +0xe8
  float xec = 1.0f;           // +0xec
  int32_t xf0 = 0;            // +0xf0
  float xf4 = 3.4028235e38f;  // +0xf4
  uint8_t xf8 = 0;            // +0xf8
  int32_t xfc = 0;            // +0xfc
  int32_t x100 = 0;           // +0x100  the fire timer (ai.md section 4)
  uint8_t x104 = 0;           // +0x104  the order asks to run again
  Firing firing;              // +0x108
  // +0x160: a std::deque<Order> (0x004e82a0: allocator byte, two
  // iterators, map, map size, size; blocks of 26 orders, 0xf70 bytes) and
  // at +0x190 its owner, this brain. The last is the current order (+0x17c).
  std::deque<Order> orders;
  const void* order_block = nullptr;  // the deque's first block and map, for the tests
  const void* order_map = nullptr;
  Order order;                // +0x194  0x004e8180, its owner this brain
  uint8_t x22c = 2;           // +0x22c
  int32_t x230 = 0;           // +0x230
  char* x234 = nullptr;       // +0x234  0x0049d180: a string holder, empty
  void* x238 = nullptr;       // +0x238  an object the dtor deletes (vt+0, 1)
  void* x23c = nullptr;       // +0x23c  the same
  BrainStep step;             // +0x240
  Steer steer;                // +0x250
  float x298[3] = {};         // +0x298  0x0063d870
  uint8_t x2a4 = 0;           // +0x2a4
  Radio* radio = nullptr;     // +0x2a8  0x004e7e20

 private:
  void push_order();     // 0x004e9d80 -> 0x004e9d90 (push_back of the template)
  void pop_order();      // the pop_back of 0x004e9380
  void make_radio();     // 0x004e7e20
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

// The default net for a vehicle without its own: znet's object of the
// record's first item, as a compiled net (vt+0x28, vt+0x34), else the
// built-in one.
AiNet* brain_net_for(const AiParams& rec);

}  // namespace mech3::ai
