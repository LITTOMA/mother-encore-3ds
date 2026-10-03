#pragma once

#include <cstdint>

namespace encore::upstream {

// Godot 3.6.2 GDScript GLOBAL seed/randi/randf/rand_range stream.
// This is not the RandomNumberGenerator object's floating-point API.
// Share one instance between battle logic and presentation. The caller chooses
// a seed or imports a state at an explicitly documented continuation boundary.
class SourceRandom {
public:
    explicit SourceRandom(uint64_t initial_seed) noexcept { seed(initial_seed); }
    void seed(uint64_t value) noexcept;
    uint32_t randi() noexcept;
    // Global randf performs float32 conversion/division, then Variant promotes
    // the result to double. It consumes exactly one raw draw, including at 0/1.
    double randf() noexcept;
    // Global rand_range performs double arithmetic. Normally three raw draws;
    // only one when its first draw is zero. Equal/reversed bounds still draw.
    double rand_range(double from, double to) noexcept;

    uint64_t state() const noexcept { return state_; }
    // Restore PCG state directly, not as a seed. Both setters reset diagnostics.
    void set_state(uint64_t value) noexcept { state_ = value; raw_draw_count_ = 0; }
    uint64_t raw_draw_count() const noexcept { return raw_draw_count_; }

private:
    uint64_t state_ = 0;
    uint64_t raw_draw_count_ = 0;
    double rand_double() noexcept;
};

} // namespace encore::upstream
