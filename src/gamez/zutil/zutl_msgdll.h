#pragma once
#include <cstdint>
#include <map>
#include <string>

// Mech3Msg.dll read as data (docs/program.md). The original loads the DLL
// (0x00575f30), asks its one export ZLocGetID for a message key's id (a
// binary search of a sorted {name, id} table in its .data, sorted by its
// DllMain) and FormatMessageA for an id's text (its RT_MESSAGETABLE resource,
// 0x00576040). FIX (A): a Windows DLL cannot be loaded on macOS, and both are
// plain data, so the front end reads the table and the resource from the file
// and installs them as zutl::messages().
namespace zutl {

struct MessageDll {
  std::map<int32_t, std::string> text;  // the message table, trailing "\r\n" cut
  std::map<std::string, int32_t> ids;   // ZLocGetID's table
};

// Reads the DLL at a host path; false when it is not a PE with a message table.
bool read_message_dll(const std::string& host_path, MessageDll& out);

// zutl::messages() answered from `dll`, which must outlive the use; texts are
// cut to the original's 0x200-byte buffers.
void install_messages(const MessageDll* dll);

// 0x00575fd0: message `id` with FormatMessage's %1 replaced by `arg`.
std::string format_message(int32_t id, const char* arg);

}  // namespace zutl
