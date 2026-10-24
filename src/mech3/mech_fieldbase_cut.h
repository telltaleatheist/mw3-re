#pragma once
#include <cstdint>

#include "gamez/zgame/unported.h"

// A field base lost in a campaign: the bases one fewer and what the player
// carries cut (0x004590b0 after the basic vehicle's death; docs/vehicles.md,
// A mobile field base lost). Apart from mech_fieldbase.cpp: the stores are
// pilot.h's, whose Handle is zgame's.
namespace mech3 {

class CampaignGame;

class FieldCutHooks : public zgame::Unported {
 public:
  virtual int32_t rand();                                   // the CRT's (0x005bb668)
  virtual const char* mech_name(int32_t mech);              // 0x00484650
  virtual int32_t lab_rounds(int32_t weapon, int32_t n);    // 0x004b4900: the rounds that go as one
  // 0x0040fe70: the message panel for `seconds`, Mech3Msg's `id` with `text`
  // as %1 (null: none).
  virtual void message(float seconds, int32_t id, const char* text) {
    unported("0x0040fe70", {zgame::bits(seconds), id, text});
  }
};
void set_field_cut_hooks(FieldCutHooks* h);  // null: the defaults
FieldCutHooks& field_cut_hooks();

// 0x004590b0's part after the death, on the current campaign.
void field_base_cut(CampaignGame& g);

}  // namespace mech3
