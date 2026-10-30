#pragma once
// strcasecmp / strncasecmp on every platform: POSIX's <strings.h>, or the
// Windows CRT's _stricmp / _strnicmp (the exe's own, lower-casing as POSIX does).
#if defined(_WIN32)
#include <string.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif
