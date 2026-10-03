#pragma once
#include <cstdint>

namespace encore::upstream {
// Mother: Encore 0.4.1.0, PartyMember._level_to_exp / _exp_to_level only.
// This independent rule module is not wired into the M0 fixture's battle rules.
class RoomView;
int32_t progression_level_cap(const RoomView& content);
// Valid game levels start at 1; above-cap values use the upstream cap threshold.
// Invalid levels leave output unchanged. Experience accepts every signed int32.
bool experience_for_level(const RoomView& content,int32_t level,int32_t& output);
int32_t level_for_experience(const RoomView& content,int32_t experience);
}
