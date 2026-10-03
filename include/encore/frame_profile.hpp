#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace encore {
// Diagnostic-only clock accumulator. The caller supplies ticks; this class does
// not sample a clock, render, allocate, advance gameplay or write a log.
class FrameProfile {
public:
    static constexpr size_t stage_count = 7;
    static constexpr size_t total_index = stage_count;
    static constexpr size_t value_count = stage_count + 1;
    static constexpr uint32_t window_frames = 60;
    using Context = std::array<uint32_t, 4>;
    struct Snapshot {
        uint64_t sequence = 0, first_tick = 0, last_tick = 0;
        uint32_t frames = 0;
        Context context{};
        std::array<double, value_count> mean_ms{};
    };

    explicit FrameProfile(double ticks_per_ms) : ticks_per_ms_(ticks_per_ms) {}

    void start(uint64_t tick, Context context) {
        discard_frame();
        if (!have_context_ || context != context_) {
            context_ = context;
            have_context_ = true;
            sums_ = {};
            count_ = 0;
        }
        frame_ticks_ = {};
        next_stage_ = 0;
        begin_ = previous_ = tick;
        active_ = std::isfinite(ticks_per_ms_) && ticks_per_ms_ > 0;
    }

    bool mark(size_t stage, uint64_t tick) {
        if (!active_) return false;
        if (stage != next_stage_ || stage >= stage_count || tick < previous_) {
            discard_frame();
            return false;
        }
        frame_ticks_[stage] = tick - previous_;
        previous_ = tick;
        ++next_stage_;
        return true;
    }

    // Commit only a fully marked frame. A failed FrameBegin or an early return
    // therefore cannot pollute the means of subsequent completed frames.
    bool finish(Context context) {
        if (!active_) return false;
        if (next_stage_ != stage_count || context != context_) {
            discard_frame();
            return false;
        }
        frame_ticks_[total_index] = previous_ - begin_;
        for (size_t i = 0; i < value_count; ++i) {
            if (frame_ticks_[i] > std::numeric_limits<uint64_t>::max() - sums_[i]) {
                discard_frame();
                return false;
            }
        }
        if (!count_) window_first_ = begin_;
        for (size_t i = 0; i < value_count; ++i) sums_[i] += frame_ticks_[i];
        active_ = false;
        if (++count_ != window_frames) return false;
        ++snapshot_.sequence;
        snapshot_.frames = count_;
        snapshot_.first_tick = window_first_;
        snapshot_.last_tick = previous_;
        snapshot_.context = context_;
        for (size_t i = 0; i < value_count; ++i)
            snapshot_.mean_ms[i] = double(sums_[i]) / ticks_per_ms_ / count_;
        sums_ = {};
        count_ = 0;
        return true;
    }

    void discard_frame() {
        if (active_) ++discarded_;
        active_ = false;
    }
    const Snapshot& snapshot() const { return snapshot_; }
    uint32_t pending_frames() const { return count_; }
    uint64_t discarded_frames() const { return discarded_; }

private:
    double ticks_per_ms_;
    Context context_{};
    bool have_context_ = false, active_ = false;
    uint64_t begin_ = 0, previous_ = 0, window_first_ = 0, discarded_ = 0;
    size_t next_stage_ = 0;
    uint32_t count_ = 0;
    std::array<uint64_t, value_count> frame_ticks_{}, sums_{};
    Snapshot snapshot_{};
};
}
