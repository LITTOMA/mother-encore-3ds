#pragma once
#include "encore/room_data.hpp"
namespace encore::upstream {
// Strict first-entry convex overlap. Exit-contact hysteresis remains separate.
bool first_trigger_overlap(const RoomView& content,uint32_t trigger_index,Vec2 player_position);
}
