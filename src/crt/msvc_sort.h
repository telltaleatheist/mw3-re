#pragma once
#include <iterator>
#include <utility>

// MSVC 5's std::sort, as the exe instantiates it for each comparator (the
// entities 0x004d7360, the contacts by distance 0x004ab500 / 0x004ab620,
// by what they saw 0x004ab1f0, the health_below triggers 0x004d21f0): a
// quicksort on a median of three down to runs of 16, an insertion sort of
// the first 16, then an unguarded one for the rest. Comparators that call
// out (the entity sort's vt calls) see the same calls in the same order.
namespace crt {

namespace detail {
template <class It, class Less>
void msvc_insertion(It f, It l, Less less) {
  if (f == l) return;
  for (It i = f + 1; i != l; ++i) {
    auto v = *i;
    if (less(v, *f)) {
      std::move_backward(f, i, i + 1);
      *f = v;
    } else {
      It j = i;
      for (It k = j - 1; less(v, *k); --k) *j = *k, j = k;
      *j = v;
    }
  }
}
template <class T, class Less>
const T& msvc_median(const T& a, const T& b, const T& c, Less less) {
  if (less(a, b)) return less(b, c) ? b : less(a, c) ? c : a;
  return less(a, c) ? a : less(b, c) ? c : b;
}
template <class It, class T, class Less>
It msvc_partition(It f, It l, T piv, Less less) {
  for (;; ++f) {
    while (less(*f, piv)) ++f;
    for (--l; less(piv, *l); --l) {}
    if (!(f < l)) return f;
    std::iter_swap(f, l);
  }
}
template <class It, class Less>
void msvc_quick(It f, It l, Less less) {
  while (l - f > 16) {
    auto piv = msvc_median(*f, *(f + (l - f) / 2), *(l - 1), less);
    It m = msvc_partition(f, l, piv, less);
    if (m - f < l - m) {
      msvc_quick(f, m, less);
      f = m;
    } else {
      msvc_quick(m, l, less);
      l = m;
    }
  }
}
}  // namespace detail

template <class It, class Less>
void msvc_sort(It f, It l, Less less) {
  if (l - f <= 16) return detail::msvc_insertion(f, l, less);
  detail::msvc_quick(f, l, less);
  detail::msvc_insertion(f, f + 16, less);
  for (It i = f + 16; i != l; ++i) {  // unguarded: the first 16 hold a smaller one
    auto v = *i;
    It j = i;
    for (It k = j - 1; less(v, *k); --k) *j = *k, j = k;
    *j = v;
  }
}

}  // namespace crt
