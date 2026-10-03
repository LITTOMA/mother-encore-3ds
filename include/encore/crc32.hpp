#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace encore {
namespace crc32_detail {
// Reflected IEEE CRC-32 polynomial, identical to the checked pack/PCM format.
// Build the 1 KiB table at compile time; no mutable cache or runtime setup.
constexpr std::array<uint32_t, 256> make_table() {
    std::array<uint32_t, 256> table{};
    for (size_t i = 0; i < table.size(); ++i) {
        uint32_t crc = static_cast<uint32_t>(i);
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
        table[i] = crc;
    }
    return table;
}
inline constexpr auto table = make_table();
}

// Incremental raw accumulator: begin with 0xffffffff, update every byte, then
// XOR the final result with 0xffffffff. Chunk boundaries do not affect the CRC.
inline uint32_t crc32_update(uint32_t crc, const uint8_t* bytes, size_t size) {
    for (size_t i = 0; i < size; ++i)
        crc = (crc >> 8) ^ crc32_detail::table[(crc ^ bytes[i]) & 0xffu];
    return crc;
}
inline uint32_t crc32(const uint8_t* bytes, size_t size) {
    return crc32_update(0xffffffffu, bytes, size) ^ 0xffffffffu;
}
}
