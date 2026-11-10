#include "upgrades/editor/ddx.h"

#include "crt/msvc_string.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace mech3 {

namespace {
int lower(char c) { return std::tolower(static_cast<unsigned char>(c)); }

// Win32 sorts a CBS_SORT list without case (CompareString's word sort, which
// also skips hyphens and apostrophes: not modelled, no shipped name has one).
int compare_nocase(const std::string& a, const std::string& b) {
  for (size_t i = 0;; ++i) {
    if (i == a.size() || i == b.size()) return (i == a.size() ? 0 : 1) - (i == b.size() ? 0 : 1);
    const int d = lower(a[i]) - lower(b[i]);
    if (d) return d;
  }
}

[[noreturn]] void fail(DataExchange& dx, std::string text) {
  ddx_message() = {dx.last, std::move(text)};  // AfxMessageBox, then pDX->Fail()
  throw DdxFail{};
}

DlgControl& prepare(DataExchange& dx, int32_t id) {  // PrepareCtrl / PrepareEditCtrl
  dx.last = id;
  if (dx.trace) dx.trace->push_back(id);
  return dx.dlg->at(id);
}

// GetWindowText into MFC's 32-character buffers (szT[32]).
std::string window_text32(const DlgControl& c) { return c.text.substr(0, 31); }

bool space_or_tab(char c) { return c == ' ' || c == '\t'; }
bool c_space(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
}  // namespace

void DlgCombo::reset() { items.clear(), cur = -1; }

int32_t DlgCombo::add(const std::string& s) {
  size_t at = items.size();
  if (sorted) {
    at = 0;
    while (at < items.size() && compare_nocase(items[at].text, s) <= 0) ++at;
  }
  items.insert(items.begin() + static_cast<std::ptrdiff_t>(at), Item{s, 0});
  if (cur >= static_cast<int32_t>(at)) ++cur;
  return static_cast<int32_t>(at);
}

void DlgCombo::set_data(int32_t i, int32_t d) {
  if (i >= 0 && i < static_cast<int32_t>(items.size())) items[size_t(i)].data = d;
}

int32_t DlgCombo::data(int32_t i) const {
  return i >= 0 && i < static_cast<int32_t>(items.size()) ? items[size_t(i)].data : -1;
}

void DlgCombo::set_cur(int32_t i) { cur = i >= 0 && i < static_cast<int32_t>(items.size()) ? i : -1; }

int32_t DlgCombo::select_string(const std::string& s) {
  for (size_t i = 0; i < items.size(); ++i) {
    const std::string& t = items[i].text;
    if (t.size() < s.size()) continue;
    size_t k = 0;
    while (k < s.size() && lower(t[k]) == lower(s[k])) ++k;
    if (k == s.size()) return cur = static_cast<int32_t>(i);
  }
  return -1;
}

std::vector<std::string> DlgCombo::texts() const {
  std::vector<std::string> v;
  for (const Item& i : items) v.push_back(i.text);
  return v;
}

void Dialog::set_check(int32_t id, int32_t v) {
  DlgControl& c = at(id);
  c.check = v > c.max_check ? c.max_check : v;
}

DdxMessage& ddx_message() {
  static DdxMessage m;
  return m;
}

// MSVC's printf("%.6g"): at least three exponent digits; its own text for
// the infinities and NaNs.
std::string ddx_float_text(float v) {
  const double d = v;
  if (std::isinf(d)) return d < 0 ? "-1.#INF" : "1.#INF";
  if (std::isnan(d)) {
    uint32_t b;
    std::memcpy(&b, &v, 4);
    if (!(b & 0x00400000)) return (b >> 31) ? "-1.#SNAN" : "1.#SNAN";
    if (b == 0xffc00000u) return "-1.#IND";
    return (b >> 31) ? "-1.#QNAN" : "1.#QNAN";
  }
  char buf[40];
  std::snprintf(buf, sizeof buf, "%.*g", 6, d);
  std::string s = buf;
  const size_t e = s.find('e');
  if (e != std::string::npos && s.size() - (e + 2) < 3) s.insert(e + 2, 3 - (s.size() - (e + 2)), '0');
  return s;
}

// _AfxSimpleFloatParse: spaces and tabs, MSVC's strtod (crt), spaces and
// tabs, the end; a 0 read from anything but a leading '0' is no number.
bool ddx_parse_float(const std::string& text, double& d) {
  const char* p = text.c_str();
  while (space_or_tab(*p)) ++p;
  const char first = *p;
  const char* end = p;
  d = crt::msvc_strtod(p, &end);
  if (d == 0.0 && first != '0') return false;
  while (space_or_tab(*end)) ++end;
  return *end == '\0';
}

// strtoul(s, end, 10) as MSVCRT reads it: whitespace, a sign (a '-' negates),
// decimal digits; ULONG_MAX past 32 bits.
static uint32_t strtoul32(const char* s, const char** end) {
  const char* const start = s;
  while (c_space(*s)) ++s;
  const char sign = *s;
  if (sign == '+' || sign == '-') ++s;
  if (!std::isdigit(static_cast<unsigned char>(*s))) {
    *end = start;
    return 0;
  }
  uint64_t v = 0;
  bool over = false;
  while (std::isdigit(static_cast<unsigned char>(*s))) {
    v = v * 10 + uint64_t(*s++ - '0');
    if (v > 0xffffffffull) over = true, v = 0xffffffffull;
  }
  *end = s;
  if (over) return 0xffffffffu;
  return sign == '-' ? 0u - static_cast<uint32_t>(v) : static_cast<uint32_t>(v);
}

// _AfxSimpleScanf("%d" / "%u"): spaces and tabs, (for %u no '-'), strtol or
// strtoul, spaces and tabs, the end; a 0 from anything but a leading '0' fails.
bool ddx_parse_int(const std::string& text, bool is_unsigned, int32_t& v) {
  const char* p = text.c_str();
  while (space_or_tab(*p)) ++p;
  const char first = *p;
  if (is_unsigned && *p == '-') return false;
  const char* end = p;
  const int32_t l = is_unsigned ? static_cast<int32_t>(strtoul32(p, &end)) : crt::msvc_strtol(p, &end);
  if (l == 0 && first != '0') return false;
  while (space_or_tab(*end)) ++end;
  if (*end != '\0') return false;
  v = l;
  return true;
}

void ddx_text(DataExchange& dx, int32_t id, int32_t& v) {
  DlgControl& c = prepare(dx, id);
  if (!dx.save) {
    c.text = std::to_string(v);
    return;
  }
  if (!ddx_parse_int(window_text32(c), false, v)) fail(dx, "Please enter an integer.");
}

void ddx_text(DataExchange& dx, int32_t id, uint32_t& v) {
  DlgControl& c = prepare(dx, id);
  if (!dx.save) {
    c.text = std::to_string(v);
    return;
  }
  int32_t r = 0;
  if (!ddx_parse_int(window_text32(c), true, r)) fail(dx, "Please enter a positive integer.");
  v = static_cast<uint32_t>(r);
}

void ddx_text(DataExchange& dx, int32_t id, float& v) {
  DlgControl& c = prepare(dx, id);
  if (!dx.save) {
    c.text = ddx_float_text(v);
    return;
  }
  double d = 0;
  if (!ddx_parse_float(window_text32(c), d)) fail(dx, "Please enter a number.");
  v = static_cast<float>(d);
}

void ddx_text(DataExchange& dx, int32_t id, std::string& v) {
  DlgControl& c = prepare(dx, id);
  if (dx.save)
    v = c.text;
  else
    c.text = v;
}

// MFC 4.2's DDX_Check: a value past 0..2 is shown, and kept, as 0.
void ddx_check(DataExchange& dx, int32_t id, int32_t& v) {
  DlgControl& c = prepare(dx, id);
  if (dx.save) {
    v = c.check;
    return;
  }
  if (v < 0 || v > 2) v = 0;
  dx.dlg->set_check(id, v);
}

void ddx_radio(DataExchange& dx, int32_t id, int32_t& v) {
  DlgControl& c = prepare(dx, id);
  if (dx.save)
    v = c.check >= 0 && c.check < c.radios ? c.check : -1;
  else
    c.check = v >= 0 && v < c.radios ? v : -1;
}

void ddv_max_chars(DataExchange& dx, const std::string& v, int32_t n) {
  if (dx.save && static_cast<int32_t>(v.size()) > n)
    fail(dx, "Please enter no more than " + std::to_string(n) + " characters.");
  dx.dlg->at(dx.last).limit = n;  // EM_LIMITTEXT on the edit just exchanged
}

void ddv_min_max_int(DataExchange& dx, int32_t v, int32_t lo, int32_t hi) {
  if (dx.save && (v < lo || v > hi))
    fail(dx, "Please enter an integer between " + std::to_string(lo) + " and " + std::to_string(hi) + ".");
}

// MFC 4.2's DDV_MinMaxFloat -> _AfxFailMinMaxReal (FLT_DIG): a NaN passes.
void ddv_min_max_float(DataExchange& dx, float v, float lo, float hi) {
  if (dx.save && (v < lo || v > hi))
    fail(dx, "Please enter a number between " + ddx_float_text(lo) + " and " + ddx_float_text(hi) + ".");
}

}  // namespace mech3
