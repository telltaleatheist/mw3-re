#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "mech3/net_save.h"

namespace mech3 {
namespace ai {
class AiParams;
}

// Ours (docs/editor.md, The vehicle editor's save): the exe keeps the vehicle
// editor's Apply in aiv.zrd's record cache only and has no aiv writer. Here
// each record Applied is noted; the Mech AI form's Save writes them into the
// mission's aiv.zrd, in the editor's mod (or, from MODS EDIT, the mod itself)
// as the nets' save does.

// What a record holds as aiv.zrd's binary value, in play units (as the cache
// and the brain hold it), with the "mech3" tag's two words (AivRecord).
struct AivEdit {
  std::string name;
  std::vector<uint8_t> value;  // the record's value as it is to be written
};

// A record's value written over `was` (the value as the file has it; null and
// 0 for a new record): (kind (x y z) yaw team tags...). Each group a tag holds
// (the header's items one by one, then params, p2, p6, p8, p9, p10, p11, p12,
// p13, pilot with can_configure, skin, lancemate, sound_tag, vlist, mech3)
// keeps its bytes where the record reads back to the same values as `was`;
// one that changed is written anew from `rec` in the file's units, ints and
// floats as the reader takes them (the old p3, p4, p5 and p7 give way to p8,
// p4's order to vlist). Tags it does not know are kept. Empty when `was`
// does not read as a record.
std::vector<uint8_t> aiv_record_written(const uint8_t* was, size_t was_size, const ai::AiParams& rec, int32_t mech3_set,
                                        int32_t mech3, const char* name);

// An aiv.zrd entry's bytes with each named record's value replaced (a name
// the entry lacks: added after the last), every other byte as it was; empty
// when the bytes are not a reader file's tree.
std::vector<uint8_t> aiv_with_edits(const uint8_t* entry, size_t size, const std::vector<AivEdit>& edits);

// The records the vehicle editor stored this mission, the last of each name,
// each written at once (aiv_record_written) over the mission's aiv.zrd as it
// stands, as bytes: nothing of the mission is kept.
void aiv_note_edit(const std::string& name, const ai::AiParams& rec, int32_t mech3_set, int32_t mech3);
size_t aiv_noted();
void aiv_notes_clear();  // the mission's end, the tests
// The noted records into the mission's aiv.zrd (aiv_entry_of_mission) and
// saved (save_archive_edit); the notes kept, so a later Save writes them again.
ai::NetSave aiv_save_edits();

}  // namespace mech3
