#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Ours (Owen, 2026-11-09; docs/editor.md, Add veh to node): the net editor's
// Add veh to node as it was meant to work. The release fills template 141
// (the vehicle type, x, y, z and yaw from the first pick, a team, Friend,
// Foe or Other; 0x00403820 -> 0x0049d350) and on OK hands the record to
// 0x004e0540 -> 0x004671d0, which returns 0, so nothing is made or written.
// Here OK makes the vehicle at the node on the edited net (create_vehicle,
// 0x0049fce0, as a mission's start does from aiv.zrd) and writes its aiv.zrd
// record into the editor's mod, so it is there from the next start too.
// The dialog is a form (form.h), drawn by ImGui.
namespace mech3 {

struct EditorForm;

void add_vehicle_open(int32_t y);  // the Add veh to node row
bool add_vehicle_open_now();
EditorForm& add_vehicle_form();    // the form (form.h; the tests set its fields and press its buttons)
// What OK did last: the record's name, "" when nothing.
const std::string& add_vehicle_last();

// The vehicle types the release lists (0x0049d350): the mechs' names
// (0x00484650), then vehicle.zrd's records.
std::vector<std::string> add_vehicle_types();

// aiv.zrd's binary record, (net (x y z) yaw team) and then `rest`: the tags
// (params, p2, p8, ...) of a record copied whole, as the shipped ones are.
std::vector<uint8_t> aiv_record(int32_t net, const float pos[3], float yaw, int32_t team,
                                const std::vector<uint8_t>& rest, int32_t rest_items);
// The tags past a record's first four items (`items` of them), of the first
// record in an aiv.zrd entry named `type_...`; empty when none is.
std::vector<uint8_t> aiv_tags_of_type(const uint8_t* entry, size_t size, const std::string& type, int32_t* items);
// An aiv.zrd entry's bytes with record `name` = `value` after the last;
// empty when the bytes are not a reader file's tree.
std::vector<uint8_t> aiv_with_record(const uint8_t* entry, size_t size, const std::string& name,
                                     const std::vector<uint8_t>& value);
// The first record name `type_NN` (NN from 01) the entry does not have.
std::string aiv_free_name(const uint8_t* entry, size_t size, const std::string& type);

}  // namespace mech3
