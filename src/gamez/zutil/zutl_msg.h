#pragma once
#include <cstdint>

// Localized messages from Mech3Msg.dll (docs/mechdef-skins.md). Original file
// unknown: between zsnd_create.cpp and zutl_zar.cpp.
namespace zutl {

// FIX (A): the DLL's ZLocGetID (0x0072c670) and the FormatMessage lookup of
// 0x00576040, set by the front end from the importer's message table.
struct Messages {
  int32_t (*get_id)(const char* name) = nullptr;
  const char* (*get_text)(int32_t id) = nullptr;
};
Messages& messages();

int32_t msg_id(const char* name);         // 0x00575f90, 0 when unknown
const char* localize(const char* name);   // 0x00575fb0, the name itself when unknown

}  // namespace zutl
