#pragma once
#include <array>
#include <cstdint>
namespace encore::ctr::region_channel_pool {
// NDSP channels 0..3 and 20..23 retain their existing fixed owners. Region
// voices and the optional named run stream share the remaining physical pool.
inline constexpr uint32_t first_channel=4,capacity=16;
inline std::array<const void*,capacity>&owners(){static std::array<const void*,capacity>value{};return value;}
}
