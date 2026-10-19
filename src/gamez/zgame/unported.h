#pragma once
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <type_traits>

// The calls ported code makes into parts of the exe that are not ported yet.
// Each hook class (zgame::AppHooks, mech3::Engine, ...) names one virtual method
// per original function, with its address; by default every method reports
// itself here and does nothing (returns 0). A front end overrides the methods it
// implements; the differential tests override only unported() and record every
// call in order, to compare with the original x86 code under the same stubs
// (docs/game-loop.md, Testing).
namespace zgame {

// One argument as the original passed it: 32 bits (floats as their bits, objects
// as opaque handles) or a C string.
struct Arg {
  uint32_t v = 0;
  const char* s = nullptr;
  template <typename T, typename = std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>>>
  Arg(T x) : v(static_cast<uint32_t>(x)) {}  // NOLINT: implicit on purpose
  Arg(const char* x) : s(x) {}               // NOLINT
};

class Unported {
 public:
  virtual ~Unported() = default;
  // `key` is the original address ("0x0051b360") or, for a virtual call on an
  // object the port does not model, "<object>.vt+<slot>"; the arguments are the
  // original's.
  virtual uint32_t unported(const char* key, std::initializer_list<Arg> args) {
    (void)key;
    (void)args;
    return 0;
  }
};

// Unported objects are opaque 32-bit handles that only the hooks interpret.
using Handle = uint32_t;

inline uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
inline float from_bits(uint32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

}  // namespace zgame
