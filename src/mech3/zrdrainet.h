#pragma once
#include <cstdint>
#include <map>
#include <string>

#include "mech3/zainet_node.h"
#include "mech3/zrdraiv.h"

// mech3\ZRdrAiNet.cpp: the AI's net and region files, net_NN.zrd and
// reg_*.zrd in each mission's reader archive (docs/ai.md, Net files). The
// net file classes' small methods and the STL code they use sit after it, up
// to zrdraiv.cpp (0x004dc150..0x004de980); the cache by name (0x004da570)
// before it.
namespace mech3::ai {

// version ( n ["mech3"] ) name ( "..." ) netparams ( pos ( x y z ) ), at the
// net file's +0xf4: 0x20 bytes, the defaults at 0x005e8910 (ctor 0x004bcb50).
struct NetHeader {
  int32_t net = 0;          // +0x00  the number it was asked for (net_NN)
  int32_t version = 0x6a;   // +0x04  105 / 106 nets, 108 regions; 105 + "mech3" reads as 106
  int32_t has_pos = 0;      // +0x08
  float pos[3] = {};        // +0x0c
  char* name = nullptr;     // +0x18  strdup'd
  int32_t first = 0;        // +0x1c  the slot before the first node_ key

  NetHeader() = default;
  NetHeader(const NetHeader&) = delete;
  NetHeader& operator=(const NetHeader& o);  // 0x004bca40
  ~NetHeader();                              // 0x004bca20
  bool operator==(const NetHeader& o) const; // 0x004bca90
  // 0x004da900: `node` is the file's top list, `base` its name without ".zrd".
  void parse(zrdr::Node* node, const char* base);
};

// The file object (0x118 bytes; the leaf vtable 0x005cd3a0 over 0x005cd3c0,
// 0x005cd3e0 and 0x005cd400, then the base 0x005cd418). Records are parsed on
// demand into one shared record, or taken from the cache the editor fills.
class NetFile : public RdrFile {
 public:
  NetFile(int32_t net, const char* name);   // inline in 0x004da570
  ~NetFile() override;                      // 0x004da840
  int32_t unload() override;                // +0x08 0x004de090
  void parse() override;                    // +0x0c 0x004da0d0

  const char* base_name();                  // 0x004da060: the name up to its '.', cached
  // +0x18 0x004da800 -> 0x004db850: one node_XX's value into `rec`.
  NetNodeRec* read_node(NetNodeRec* rec, zrdr::Node* value, const char* key);
  NetNodeRec* node(int32_t i, const char** key);   // 0x004de730: cache first
  NetNodeRec* node_raw(int32_t i, const char** key);  // 0x004dc690: always parsed
  NetNodeRec* node_named(const char* key);  // 0x004de830
  void clear_cache();                       // 0x004de010 -> 0x004da320

  zrdr::Node* value = nullptr;              // +0x24  the last record's value
  const char* key = nullptr;                // +0x28  and its key
  NetNodeRec scratch;                       // +0x2c
  NetNodeRec* current = nullptr;            // +0xe0
  std::map<std::string, NetNodeRec*> cache; // +0xe4 (size +0xf0)
  NetHeader header;                         // +0xf4
  char* base = nullptr;                     // +0x114
};

// The files by name, a std::map at 0x0060b5a8 that lives for the whole game.
std::map<std::string, NetFile*>& net_files();
void net_files_clear();                     // not original: the tests start empty
const char* net_name(int32_t net);          // 0x004d5280: "net_%02d", a static buffer

// What the AI holds a net file by (4 bytes): 0x004da570 finds or makes the
// file of that name (net_NN when none) and adds a reference; 0x004da8e0
// releases it.
class NetRef {
 public:
  NetRef(int32_t net, const char* name);    // 0x004da570
  ~NetRef();                                // 0x004da8e0
  NetRef(const NetRef&) = delete;
  NetRef& operator=(const NetRef&) = delete;

  NetFile* file = nullptr;
  NetHeader* header() const { return file ? &file->header : nullptr; }  // 0x004dc260
  int32_t count() const { return file ? file->record_count() : 0; }      // 0x004dc670
  NetNodeRec* node(int32_t i, const char** key) const {                  // 0x004dc640
    return file ? file->node(i, key) : nullptr;
  }
};

}  // namespace mech3::ai
