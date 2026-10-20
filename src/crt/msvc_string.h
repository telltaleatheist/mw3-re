#pragma once
#include <cstdint>

// MSVCRT string functions the original links that have no portable equivalent.
namespace crt {

// _stricmp: compares with ASCII A-Z folded to lower case, as MSVCRT does in the
// default "C" locale. <0, 0 or >0 like strcmp.
int msvc_stricmp(const char* a, const char* b);

// _splitpath, file name and extension parts only (each at most 255 + NUL). The
// original splits on '\\', '/' and a drive ':'; the extension starts at the last
// '.' after the last separator.
void msvc_splitpath_name(const char* path, char* fname, char* ext);

// atof as MSVCRT reads a number: leading whitespace, a sign, digits with one
// '.', then an exponent written e, E, d or D; the rest is ignored. No "inf",
// "nan" or hex forms, which a modern strtod would take.
double msvc_atof(const char* s);

// strtod with the grammar of msvc_atof; *end is past the number, or `s`
// when there is none.
double msvc_strtod(const char* s, const char** end);

// strtol(s, end, 10): whitespace, a sign and decimal digits; out of range it
// gives LONG_MAX or LONG_MIN as MSVCRT does. *end is `s` when there are no
// digits.
int32_t msvc_strtol(const char* s, const char** end);

// sscanf(s, "%x") and sscanf(s, "%d") as MSVCRT reads them: whitespace, for
// %x an optional 0x or 0X (its 0 counts as a digit), for %d a sign; the
// value wraps at 32 bits. Returns the conversions (0 or 1).
int msvc_scan_hex(const char* s, uint32_t* out);
int msvc_scan_dec(const char* s, int32_t* out);

// atoi: whitespace, a sign and decimal digits, wrapping on overflow as
// MSVCRT's atol does.
int32_t msvc_atoi(const char* s);

// iswspace((short)c) on a char. The original passes the char sign-extended,
// so bytes 0x80..0xff become U+FF80..U+FFFF, none of which is a space.
bool msvc_iswspace_char(char c);

}  // namespace crt
