#ifndef MECH3_CLOUD_BAND_H
#define MECH3_CLOUD_BAND_H

namespace mech3 {

// Pirate's Moon: cloud_cover's "death" band (world.zrd): a vehicle inside it is dead (docs/vehicles.md, The cloud's
// death band). `on` the block was read ([0x0062da58]), `death_on` it has the band ([0x0062da7c]).
struct CloudBand {
  bool on = false, death_on = false;
  float low = 0, high = 0;  // [0x0062da80], [0x0062da84]
  bool holds(float y) const { return on && death_on && y > low && y < high; }
};

CloudBand program_cloud_band();  // the mission's (app_mission.cpp)

}  // namespace mech3

#endif
