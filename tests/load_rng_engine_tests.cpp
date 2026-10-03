// Compile with official engine RandomPCG/PCG sources left unchanged and fixture
// OS/typedef headers. This independently proves randomize's exact seed formula;
// the official Godot binary oracle proves GDScript evaluation and global draws.
#include "encore/load_rng.hpp"
#include "core/math/random_pcg.h"
#include "core/os/os.h"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace encore::upstream;
int main() {
    unsigned checks = 0;
    auto check = [&](bool value) {
        ++checks;
        if (!value) { std::cerr << "official RandomPCG formula mismatch\n"; std::exit(1); }
    };
    const uint64_t states[] = {0, 1, 2, UINT64_C(0x8000000000000000), UINT64_MAX};
    const LoadRngClockSample samples[] = {{0,0}, {1770000000, 987654321},
                                         {UINT64_MAX, 0}, {UINT64_MAX, UINT64_MAX},
                                         {UINT64_C(0x8000000000000000), 1}};
    for (auto state : states) for (const auto sample : samples) {
        RandomPCG original;
        original.set_state(state);
        auto* os = OS::get_singleton();
        os->unix_seconds = sample.unix_seconds;
        os->ticks_usec = sample.ticks_usec;
        os->unix_calls = os->tick_calls = 0;
        original.randomize();
        const auto seed = godot_randomize_seed(state, sample);
        check(original.get_seed() == seed);
        check(os->unix_calls == 1 && os->tick_calls == 1);
        SourceRandom native(seed);
        check(native.state() == original.get_state());
        for (unsigned draw = 0; draw < 8; ++draw) check(native.randi() == original.rand());
        check(native.state() == original.get_state());
    }
    std::cout << "Unmodified Godot 3.6.2 RandomPCG: " << checks
              << " checks across 25 wrapping-state/clock cases passed\n";
}
