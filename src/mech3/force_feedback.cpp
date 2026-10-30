#include "mech3/force_feedback.h"

#include <cmath>

#include "crt/msvc_math.h"

namespace mech3 {

// Mech3.exe 1.2 @ 0x004339a0 (IFCreateEffects on the project, by name).
FfEffect::FfEffect(const char* name, platform::ForceDevice* d) : dev(d) {
  if (dev) parts = dev->create(name);
}

// Mech3.exe 1.2 @ 0x00433b70
bool FfEffect::start() {
  if (!made()) return false;
  for (int32_t p : parts) dev->start(p);
  return true;
}

// Mech3.exe 1.2 @ 0x00433bc0. Quirk kept: the loop stops the first part each
// time (the array's [0], not [i]), so a compound's other parts run out by
// themselves.
bool FfEffect::stop() {
  if (!made()) return false;
  for (size_t i = 0; i < parts.size(); ++i) dev->stop(parts[0]);
  return true;
}

// Mech3.exe 1.2 @ 0x00433c00: degrees 0..360 as hundredths (DIEP_DIRECTION),
// on the first part's parameters.
bool FfEffect::set_direction(int32_t degrees) {
  if (!made()) return false;
  if (degrees > 360) degrees = 360;
  else if (degrees < 0) degrees = 0;
  dev->set_direction(parts[0], degrees * 100);
  return true;
}

// Mech3.exe 1.2 @ 0x00433c50: 0..100 as 0..10000 (DIEP_GAIN), on the first part.
bool FfEffect::set_gain(int32_t gain) {
  if (!made()) return false;
  if (gain > 100) gain = 100;
  else if (gain < 0) gain = 0;
  dev->set_gain(parts[0], gain * 100);
  return true;
}

namespace {
std::unique_ptr<FfEffect> make(const char* name, platform::ForceDevice* dev) {
  auto e = std::make_unique<FfEffect>(name, dev);
  if (!e->made()) e.reset();  // 0x004339a0 leaves +0x04 0: the slots' tests fail
  return e;
}
}  // namespace

// Mech3.exe 1.2 @ 0x00433090: force_eff.ifr (0x00433960 on zInput's device,
// 0x0051dda0) and, when it read, the twelve effects by their names. The
// file has no "i_fall" ("impact fall"), so that slot stays empty.
ForceFeedback::ForceFeedback(platform::ForceDevice* dev) {
  project = dev && dev->load("force_eff.ifr");
  if (!project) return;
  recoil = make("Recoil", dev);
  spin_move = make("Spin_Move", dev);
  laser_buzz = make("laser_buzz", dev);
  impact_force = make("impact_force", dev);
  footfall = make("footfall", dev);
  jets1 = make("jets1", dev);
  spring_fx = make("Spring", dev);
  dead_spring_fx = make("dead_spring", dev);
  dead_dance_fx = make("dead_dance", dev);
  water_fx = make("water", dev);
  impact_land = make("impact_land", dev);
  i_fall = make("i_fall", dev);
}

// Mech3.exe 1.2 @ 0x004333f0 (IFReleaseEffects each, the project's release)
ForceFeedback::~ForceFeedback() = default;

// Mech3.exe 1.2 @ 0x00433530
void ForceFeedback::stop_all() {
  for (FfEffect* e : {recoil.get(), spin_move.get(), laser_buzz.get(), impact_force.get(), footfall.get(),
                      jets1.get(), spring_fx.get(), dead_spring_fx.get(), dead_dance_fx.get(), water_fx.get(),
                      impact_land.get(), i_fall.get()})
    if (e) e->stop();
}

// Mech3.exe 1.2 @ 0x004335d0 (n unread): the amount clamped to 0..1, x 80.
void ForceFeedback::jolt(const float*, float amount) {
  if (!spin_move) return;
  spin_move->stop();
  float a = amount;
  if (a > 1.0f) a = 1.0f;       // [0x005bec18]
  else if (a < 0.0f) a = 0.0f;  // [0x005bec1c]
  spin_move->set_gain(crt::msvc_ftol(a * 80.0f));  // [0x005bec20]
  spin_move->start();
}

// Mech3.exe 1.2 @ 0x00433630: the weapon's kind (its id): the ballistics and
// missiles Recoil, the lasers laser_buzz, by size.
void ForceFeedback::weapon(int32_t kind) {
  FfEffect* e = nullptr;
  int32_t gain = 0;
  switch (kind) {
    case 0: case 1: case 2: case 3: case 0xd: case 0xe: case 0xf: case 0x1a: case 0x1b: case 0x1c:
      e = recoil.get(), gain = 0x3c;
      break;
    case 4: case 10: case 0x17:
      e = laser_buzz.get(), gain = 0x32;
      break;
    case 5: case 0xb: case 0x18:
      e = laser_buzz.get(), gain = 0x50;
      break;
    case 6: case 0xc: case 0x19:
      e = laser_buzz.get(), gain = 100;
      break;
    case 7: case 9: case 0x11: case 0x12: case 0x13: case 0x14: case 0x21: case 0x22: case 0x23:
      e = recoil.get(), gain = 100;
      break;
    case 0x1d: case 0x1e: case 0x1f:
      e = recoil.get(), gain = 0x28;
      break;
    default:
      break;
  }
  if (!e) return;
  e->stop();
  e->set_gain(gain);
  e->start();
}

// Mech3.exe 1.2 @ 0x00433720 (the weapon unread): the force x 100, from the
// side the hit came: pi less atan2(-x, -z) of its direction, wrapped into
// 0..2 pi ([0x005bec24], [0x005bec28]), as degrees.
void ForceFeedback::hit(int32_t, const float dir[3], float force) {
  if (!impact_force) return;
  impact_force->stop();
  float a = 3.1415927f - static_cast<float>(std::atan2(-dir[0], -dir[2]));
  if (a > 6.2831855f) a = 6.2831855f;  // 0x40c90fdb
  else if (a < 0.0f) a = 0.0f;
  impact_force->set_gain(crt::msvc_ftol(force * 100.0f));      // [0x005bec2c]
  impact_force->set_direction(crt::msvc_ftol(static_cast<double>(a) * 57.29577951308));  // [0x005bec30], a double: degrees
  impact_force->start();
}

// Mech3.exe 1.2 @ 0x004337c0
void ForceFeedback::footstep(float speed) {
  if (!footfall) return;
  footfall->stop();
  footfall->set_gain(crt::msvc_ftol(speed * 100.0f));
  footfall->start();
}

// Mech3.exe 1.2 @ 0x00433800
void ForceFeedback::landing(float j) {
  if (!impact_land) return;
  impact_land->stop();
  impact_land->set_gain(crt::msvc_ftol(j * 100.0f));
  impact_land->start();
}

// Mech3.exe 1.2 @ 0x00433840
void ForceFeedback::fall() {
  if (!i_fall) return;
  i_fall->set_gain(100);
  i_fall->set_direction(0);
  i_fall->start();
}

// Mech3.exe 1.2 @ 0x00433870, 0x004338a0, 0x004338d0, 0x00433900, 0x00433930:
// stopped, then started when on.
namespace {
void toggle(FfEffect* e, bool on) {
  if (!e) return;
  e->stop();
  if (on) e->start();
}
}  // namespace
void ForceFeedback::jets(bool on) { toggle(jets1.get(), on); }
void ForceFeedback::spring(bool on) { toggle(spring_fx.get(), on); }
void ForceFeedback::dead_spring(bool on) { toggle(dead_spring_fx.get(), on); }
void ForceFeedback::dead_dance(bool on) { toggle(dead_dance_fx.get(), on); }
void ForceFeedback::water(bool on) { toggle(water_fx.get(), on); }

}  // namespace mech3
