#include "encore/load_rng.hpp"

#include <algorithm>
#include <limits>

namespace encore::upstream {

uint64_t godot_randomize_seed(uint64_t current_state,
                             LoadRngClockSample sample) noexcept {
    // Godot 3.6.2 core/math/random_pcg.cpp, RandomPCG::randomize;
    // PCG_DEFAULT_INC_64 is the sequence constant, not the derived increment.
    return (sample.unix_seconds + sample.ticks_usec) * current_state +
           UINT64_C(1442695040888963407);
}

bool apply_load_uid_allocations(SourceRandom& random,
    std::vector<uint32_t>& generated_uid_ledger,
    const std::vector<LoadInventoryAllocation>& ordered_inventories,
    const LoadRngClockProvider& clock,
    std::string& error,
    std::vector<LoadUidAllocation>* trace) {
    size_t total = 0;
    for (size_t row = 0; row < ordered_inventories.size(); ++row) {
        const auto& inventory = ordered_inventories[row];
        for (size_t previous = 0; previous < row; ++previous) {
            if (ordered_inventories[previous].order_id == inventory.order_id) {
                error = "LOAD RNG inventory order contains a duplicate row identity";
                return false;
            }
        }
        if (inventory.item_count > std::numeric_limits<size_t>::max() - total) {
            error = "LOAD RNG allocation count overflows";
            return false;
        }
        total += inventory.item_count;
    }
    if (total > generated_uid_ledger.max_size() - generated_uid_ledger.size() ||
        (trace && total > trace->max_size())) {
        error = "LOAD RNG allocation count exceeds container capacity";
        return false;
    }
    if (total && !clock) {
        error = "LOAD RNG clock provider is missing";
        return false;
    }
    // This also preserves SourceRandom's draw diagnostics for an empty load.
    SourceRandom next_random = random;
    auto next_ledger = generated_uid_ledger;
    next_ledger.reserve(next_ledger.size() + total);
    std::vector<LoadUidAllocation> next_trace;
    if (trace) next_trace.reserve(total);
    for (const auto& inventory : ordered_inventories) {
        for (uint32_t item = 0; item < inventory.item_count; ++item) {
            LoadRngClockSample sample;
            std::string clock_error;
            if (!clock(sample, clock_error)) {
                error = clock_error.empty() ? "LOAD RNG clock provider failed" : clock_error;
                return false;
            }
            const auto seed = godot_randomize_seed(next_random.state(), sample);
            next_random.seed(seed);
            auto generated = next_random.randi();
            while (std::find(next_ledger.begin(), next_ledger.end(), generated) !=
                   next_ledger.end()) {
                generated = next_random.randi();
            }
            next_ledger.push_back(generated);
            if (trace) next_trace.push_back({inventory.order_id, item, generated,
                                             next_random.raw_draw_count(), seed});
        }
    }
    random = next_random;
    generated_uid_ledger.swap(next_ledger);
    if (trace) trace->swap(next_trace);
    error.clear();
    return true;
}

} // namespace encore::upstream
