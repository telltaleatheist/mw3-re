#pragma once

// MSVCRT string functions the original links that have no portable equivalent.
namespace crt {

// _stricmp: compares with ASCII A-Z folded to lower case, as MSVCRT does in the
// default "C" locale. <0, 0 or >0 like strcmp.
int msvc_stricmp(const char* a, const char* b);

// _splitpath, file name and extension parts only (each at most 255 + NUL). The
// original splits on '\\', '/' and a drive ':'; the extension starts at the last
// '.' after the last separator.
void msvc_splitpath_name(const char* path, char* fname, char* ext);

}  // namespace crt
