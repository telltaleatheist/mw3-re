#pragma once

namespace zcls { struct Node; }

// C:\zipper\branchep1\gamez\zClass\Light.c (MW3's: gamez/zclass/light.cpp).
namespace pm::zcls {

// Pirate's Moon 1.0 @ 0x0050e470: PM's 0x100-byte light (the shared Light,
// its colours in pm_colors) with PM's defaults, active, on list 9. Shared
// code reaches it as zcls::light_create.
::zcls::Node* light_create();

// Pirate's Moon's light setters: each also keeps the mixed colours (+0xbc..+0xdc, pm_colors),
// and the static bit is 0x200 (MW3's 0x100). 1 flags the light for its update.
int light_set_orientation(::zcls::Node* node, float x, float y, float z);  // 0x0050efc0
int light_set_ambient(::zcls::Node* node, float v);                        // 0x0050e6f0
int light_set_diffuse(::zcls::Node* node, float v);                        // 0x0050e7c0
int light_set_diffuse_color(::zcls::Node* node, float r, float g, float b);  // 0x0050f600
int light_set_ambient_color(::zcls::Node* node, float r, float g, float b);  // 0x0050f760
int light_set_own_colors(::zcls::Node* node, int on);                      // 0x0050e890  bit 0x400
int light_set_bit2(::zcls::Node* node, int on);                            // 0x0050e900
int light_set_bit3(::zcls::Node* node);                                    // 0x0050e980  and clears bit 4
int light_set_bit4(::zcls::Node* node);                                    // 0x0050ea00  and clears bit 3
int light_set_ambient_only(::zcls::Node* node, int on);                    // 0x0050ea80
int light_set_diffuse_only(::zcls::Node* node, int on);                    // 0x0050eb60
int light_set_light_map(::zcls::Node* node, int on);                       // 0x0050ec30  bit 0x80
int light_set_directional(::zcls::Node* node, int on);                     // 0x0050ed00  bit 0x100
int light_set_range(::zcls::Node* node, float a, float b);                 // 0x0050edd0
int light_set_position(::zcls::Node* node, float x, float y, float z);     // 0x0050ef50
int light_set_color(::zcls::Node* node, float r, float g, float b);        // 0x0050f460

}  // namespace pm::zcls
