#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "mech3/zrdrainet.h"

// Ours: the AI net editor's Save Changes (docs/editor.md, The AI net editor,
// the map). The release's save never reaches the disk and its writers are
// text (zrdrainet.cpp), which rounds positions and which the exe would not
// read over a shipped net, so a save writes the mission's whole archive as a
// mod's copy, the net in zrd's binary form: an unedited node's bytes as they
// were, an edited record with its exact values.
namespace mech3::ai {

// zrd's binary form (docs/zreader.md): a type, then the value; a list its
// count (itself included), then the items. Strings without their terminator.
void put_node(std::vector<uint8_t>& out, const zrdr::Node& n);

// A record as a node_XX's value: (12 (x y z) (links) props...) or ((links)
// props...), each property that differs from the defaults as the text writer
// writes it (0x004db8d0) but exact, and the sound the text writer drops.
// `was`, the node's value as read, gives what the record no longer holds:
// the name as written (the record keeps it localized) and the sound's name.
void put_record(std::vector<uint8_t>& out, const NetNodeRec& r, const zrdr::Node* was);

// A net entry's bytes with each edited key's value replaced (and keys it does
// not have added at the end; a null record drops its key), every other byte as it was; `tree` is the
// entry as read (for put_record's `was`). Empty when the bytes are not a net
// file's tree (a list holding the list of keys and values).
std::vector<uint8_t> net_with_edits(const uint8_t* entry, size_t size, const zrdr::Node* tree,
                                    const std::vector<std::pair<std::string, const NetNodeRec*>>& edits);

// An archive's bytes with entry `name` (matched case-insensitively) holding
// `data`; every other entry's bytes, name, comment and file time as they
// were, in their order. Empty when `archive` is not a zar archive MW3 reads
// (table, version 1, count) or has no such entry, unless `add`: then the
// entry goes after the last (no comment, no file time).
std::vector<uint8_t> archive_with_entry(const std::vector<uint8_t>& archive, const char* name,
                                        const std::vector<uint8_t>& data, bool add = false);

// A new net file's bytes, as the shipped ones begin: version ( 106 ) and
// name ( `name` ), no nodes (net_with_edits adds them, with a null tree).
std::vector<uint8_t> net_new_entry(const char* name);

// The editor's own mod for the edits of an archive found in mod `source`
// (its folder; "" the install): `source` itself when it is one (its marker,
// kEditorMarker), else "<source's mod.txt name> edits" ("editor edits" for the
// install), made with the marker and a mod.txt (name, the source's for=);
// one that exists without the marker is someone else's, so "<base> (2)" and
// on. An ADD MOD folder is the author's: never ours. "" with `error` when no
// folder can be made.
inline constexpr const char* kEditorMarker = "mw3-editor.txt";
std::string editor_mod(const std::string& write_dir, const std::string& source, std::string* error);

struct NetSave {
  bool ok = false;
  std::string folder;   // the editor mod's folder under mods
  std::string path;     // the file written (the host's path)
  std::string message;  // for the screen: where, and from when
  std::string error;    // why not
};
// The archive `rel` as the game finds it ("zbd\c1\readerm1.zbd": a mod's copy
// first) with entry `name` holding `data`, written to the editor mod's
// <rel> (editor_mod), the previous save kept as <rel>.bak (one), through a
// temporary file renamed over it; the mod enabled just before its source in
// mw3.cfg (the install's: last), so it wins from the next start. Never the
// install's file or a folder the editor did not make.
NetSave save_archive_edit(const char* rel, const char* name, const std::vector<uint8_t>& data, bool add = false);

}  // namespace mech3::ai
