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

// atoi: whitespace, a sign and decimal digits, wrapping on overflow as
// MSVCRT's atol does.
int32_t msvc_atoi(const char* s);

// iswspace((short)c) on a char. The original passes the char sign-extended,
// so bytes 0x80..0xff become U+FF80..U+FFFF, none of which is a space.
bool msvc_iswspace_char(char c);

}  // namespace crt
