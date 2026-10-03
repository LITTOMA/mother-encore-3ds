#pragma once

#include "encore/source_random.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace encore::upstream {

// Source inventory reconstruction order comes from a checked external resource.
// Rows include empty inventories. order_id identifies a row for diagnostics;
// this operation neither sorts it nor interprets a game/character identity.
struct LoadInventoryAllocation {
    uint32_t order_id = 0;
    uint32_t item_count = 0;
};

// Godot OS::get_unix_time() seconds and get_ticks_usec() since engine start.
// The platform samples these separately for every Item.get_uid call. A native
// clock with a different epoch/origin is an explicit entropy adaptation.
struct LoadRngClockSample {
    uint64_t unix_seconds = 0;
    uint64_t ticks_usec = 0;
};
using LoadRngClockProvider =
    std::function<bool(LoadRngClockSample&, std::string&)>;

struct LoadUidAllocation {
    uint32_t order_id = 0;
    uint32_t item_index = 0;
    uint32_t generated_uid = 0;
    uint64_t raw_draws = 0;
    uint64_t seed = 0;
};

// Exact Godot 3.6.2 RandomPCG::randomize expression, with unsigned wraparound.
uint64_t godot_randomize_seed(uint64_t current_state,
                             LoadRngClockSample sample) noexcept;

// LOAD calls at its accepted commit after decoding/domain/preparation succeeds.
// New Game may call through stage_new_game_startup on isolated copies before
// naming; cancellation discards those copies. Replays Inventory.init_from_serialized's eager Dictionary.get
// fallback: one randomize per item, then randi until unique in the generated UID
// ledger, appending the fallback even when the saved UID is retained instead.
// It never reads or changes saved UIDs. Supplying all four Item constructor
// arguments causes no additional allocation.
//
// The ledger persists for the process and contains generated fallback/default
// UIDs, not saved UIDs. Boot-time UID construction is a separate caller policy;
// native stable initial IDs do not reproduce the source boot allocation history.
// On a provider/structural failure, RNG, ledger and optional trace are unchanged.
// Clock reads cannot be undone. Empty input requires no provider and draws none.
bool apply_load_uid_allocations(SourceRandom& random,
    std::vector<uint32_t>& generated_uid_ledger,
    const std::vector<LoadInventoryAllocation>& ordered_inventories,
    const LoadRngClockProvider& clock,
    std::string& error,
    std::vector<LoadUidAllocation>* trace = nullptr);

} // namespace encore::upstream
