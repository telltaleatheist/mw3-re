#pragma once
#include "gamez/zclass/cls_node.h"

// zClass\Seq.c and Animate.c: the frame steps of the two timed classes, run
// by the frame update over lists 11 and 12 (list.cpp, 0x004f7ec0 and
// 0x004f7f10; docs/action-lists.md). Neither class is in the shipped files.
namespace zcls {

// A sequence's time advances by the frame; past an entry's time the index
// steps on. At either end it wraps (loop) or bounces, and stops unless it
// repeats. Paused or inactive: nothing. 5 for a null node or data.
int seq_update(Node* node);        // 0x004fdf50

// A playing animate's time advances (0x004fda20), its pose is sampled
// between two keys (0x004fdab0), its matrix marked dirty and the node put
// on list 7. Stopped: its playing bit is cleared.
int animate_update(Node* node);    // 0x004fd950
int seq_remove_child(Node* parent, Node* child);  // 0x004fdd90, its entry too
int animate_delete(Node* node);    // 0x004fd890

// The script commands' (ported for preservation; the engine's entries are null): NewSEQ, SEQAddChild,
// SEQSetActive / Repeat / Loop / Pause (data +0x00 / +0x04 / +0x08 / +0x0c).
Node* seq_create();                                                // 0x004fdc70
int seq_add_child(Node* node, Node* child, int32_t at, float time);  // 0x004fdcd0
int seq_set_active(Node* node, int32_t v);                         // 0x004fde50
int seq_set_repeat(Node* node, int32_t v);                         // 0x004fde90
int seq_set_loop(Node* node, int32_t v);                           // 0x004fded0
int seq_set_pause(Node* node, int32_t v);                          // 0x004fdf10

}  // namespace zcls
