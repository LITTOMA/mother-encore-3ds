// PCG32 XSH-RR / seeding adapted from the minimal PCG32 implementation:
// Copyright (c) 2014 M. E. O'Neill, https://www.pcg-random.org/
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy at https://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// Changes: explicit evaluation order, portable leading-zero count, source-global
// floating API, diagnostic draw count and state-injection interface.
// Godot API semantics: docs/licenses/Godot-3.6.2-LICENSE.txt.
#include "encore/source_random.hpp"

#include <cmath>
#include <limits>

namespace encore::upstream {
namespace {
// Algorithm constants from Godot 3.6.2's RandomPCG / PCG XSH-RR 64/32.
constexpr uint64_t multiplier = UINT64_C(6364136223846793005);
constexpr uint64_t default_sequence = UINT64_C(1442695040888963407);
constexpr uint64_t increment = (default_sequence << 1U) | UINT64_C(1);
static_assert(std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<float>::digits == 24 &&
              std::numeric_limits<double>::is_iec559 &&
              std::numeric_limits<double>::digits == 53,
              "Source RNG requires IEEE binary32/binary64");

unsigned leading_zeroes(uint32_t value) noexcept {
    // The nonzero precondition is checked in rand_double. Keep this portable
    // across host and ARM rather than using an undefined clz(0) intrinsic.
    unsigned count = 0;
    while ((value & UINT32_C(0x80000000)) == 0) {
        ++count;
        value <<= 1U;
    }
    return count;
}
} // namespace

void SourceRandom::seed(uint64_t value) noexcept {
    state_ = 0;
    randi();
    state_ += value;
    randi();
    // PCG seeding advances twice; they are not user-observable random calls.
    raw_draw_count_ = 0;
}

uint32_t SourceRandom::randi() noexcept {
    const uint64_t old_state = state_;
    state_ = old_state * multiplier + increment;
    const uint32_t mixed = static_cast<uint32_t>(((old_state >> 18U) ^ old_state) >> 27U);
    const uint32_t rotation = static_cast<uint32_t>(old_state >> 59U);
    ++raw_draw_count_;
    return (mixed >> rotation) | (mixed << ((0U - rotation) & 31U));
}

double SourceRandom::randf() noexcept {
    const float numerator = static_cast<float>(randi());
    // (float)UINT32_MAX rounds to 2^32 in the official binary32 Math::randf.
    const float result = numerator / static_cast<float>(UINT32_MAX);
    return static_cast<double>(result);
}

double SourceRandom::rand_double() noexcept {
    const uint32_t exponent_bits = randi();
    if (exponent_bits == 0) return 0.0;
    // Explicitly preserve high-word-before-low-word evaluation established by
    // the official 3.6.2 engine reference; do not combine side effects in |.
    const uint64_t high = static_cast<uint64_t>(randi()) << 32U;
    const uint64_t low = randi();
    const uint64_t significand = high | low | UINT64_C(0x8000000000000001);
    return std::ldexp(static_cast<double>(significand),
                      -64 - static_cast<int>(leading_zeroes(exponent_bits)));
}

double SourceRandom::rand_range(double from, double to) noexcept {
    const double fraction = rand_double();
    // Godot's official x86_64 binary evaluates multiply then add. Volatile
    // prevents a target compiler from contracting this into a different FMA.
    const volatile double scaled = fraction * (to - from);
    return scaled + from;
}

} // namespace encore::upstream
