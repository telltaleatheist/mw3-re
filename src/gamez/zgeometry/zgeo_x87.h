#pragma once
#include <cmath>

#include "gamez/zutil/zfix.h"

// The crater clip's arithmetic as the x87 did it (docs/craters.md, Precision).
// The exe keeps values on the FPU stack between stores, so at the reference
// runtime's 53 bits they are doubles until stored to a float; at 24 every step
// rounds to one. X is such a value: float where the code stores, X in between.
namespace zgeo {

struct X {
  double v;
  X(float f) : v(f) {}
  X(double d) : v(d) {}
  float f() const { return static_cast<float>(v); }
  static X step(double r) { return zfix::options().fpu53 ? X(r) : X(static_cast<float>(r)); }
};
inline X operator+(X a, X b) { return X::step(a.v + b.v); }
inline X operator-(X a, X b) { return X::step(a.v - b.v); }
inline X operator*(X a, X b) { return X::step(a.v * b.v); }
inline X operator/(X a, X b) { return X::step(a.v / b.v); }
inline X operator-(X a) { return X(-a.v); }
inline X abs(X a) { return X(std::fabs(a.v)); }
inline X sqrt(X a) { return X::step(std::sqrt(a.v)); }
inline bool operator<(X a, X b) { return a.v < b.v; }
inline bool operator>(X a, X b) { return a.v > b.v; }
inline bool operator<=(X a, X b) { return a.v <= b.v; }
inline bool operator>=(X a, X b) { return a.v >= b.v; }
inline bool operator==(X a, X b) { return a.v == b.v; }
inline bool operator!=(X a, X b) { return a.v != b.v; }

}  // namespace zgeo
