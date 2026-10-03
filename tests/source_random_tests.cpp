#include "encore/source_random.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <vector>

using encore::upstream::SourceRandom;
namespace {
unsigned checks = 0;
void check(bool good, const char* message) {
    ++checks;
    if (!good) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
uint64_t bits(double value) {
    uint64_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
class Reference {
public:
    explicit Reference(const char* path) {
        std::ifstream input(path, std::ios::binary);
        check(bool(input), "official engine reference exists");
        data_ = std::vector<uint8_t>(std::istreambuf_iterator<char>(input), {});
    }
    uint64_t integer(unsigned size) {
        check(cursor_ + size <= data_.size(), "reference is not truncated");
        uint64_t value = 0;
        for (unsigned i = 0; i < size; ++i) value |= uint64_t(data_[cursor_++]) << (i * 8);
        return value;
    }
    double real() { const auto raw = integer(8); double value; std::memcpy(&value, &raw, 8); return value; }
    bool finished() const { return cursor_ == data_.size(); }
private:
    std::vector<uint8_t> data_;
    size_t cursor_ = 0;
};
} // namespace

int main(int argc, char** argv) {
    check(argc == 2, "provide reports/battle-round-random/reference.bin");
    Reference ref(argv[1]);
    for (char ch : {'E', 'R', 'N', 'G', '3', '6', '2', '\n'})
        check(ref.integer(1) == uint8_t(ch), "reference format identity");
    const auto cases = ref.integer(4);
    check(cases == 40, "complete official-engine case set");
    uint64_t operations = 0;
    bool saw_zero_range = false, saw_one_float = false;
    for (uint64_t c = 0; c < cases; ++c) {
        const auto seed = ref.integer(8);
        const auto state = ref.integer(8);
        const auto count = ref.integer(4);
        SourceRandom rng(seed);
        check(rng.state() == state, "PCG seed expansion matches engine object state");
        check(rng.raw_draw_count() == 0, "seed warmup not counted as gameplay draws");
        for (uint64_t i = 0; i < count; ++i) {
            const auto kind = ref.integer(4);
            const double from = ref.real(), to = ref.real();
            const auto expected = ref.integer(8), after = ref.integer(8), draws = ref.integer(8);
            const auto raw_n = ref.integer(4);
            const uint64_t raw[3] = {ref.integer(4), ref.integer(4), ref.integer(4)};
            check(kind < 3, "known reference operation");
            check(raw_n >= 1 && raw_n <= 3, "known primitive consumption range");
            SourceRandom raw_replay = rng;
            for (uint64_t j = 0; j < raw_n; ++j)
                check(raw_replay.randi() == raw[j], "raw PCG outputs match engine object trace");
            const uint64_t before_count = rng.raw_draw_count();
            uint64_t actual;
            if (kind == 0) actual = rng.randi();
            else if (kind == 1) { const double f = rng.randf(); actual = bits(f); saw_one_float |= f == 1.0; }
            else { const double f = rng.rand_range(from, to); actual = bits(f); saw_zero_range |= from == 0.0 && to == 1.0 && f == 0.0 && rng.raw_draw_count() == before_count + 1; }
            if (actual != expected) std::cerr << "case=" << c << " op=" << i << " kind=" << kind << " actual=" << actual << " expected=" << expected << '\n';
            check(actual == expected, "bit-exact global value from actual Godot3.6.2");
            check(rng.state() == after, "same engine continuation state");
            check(rng.raw_draw_count() == draws, "same engine raw consumption");
            ++operations;
        }
        // Restore is an exact continuation; it must not expand as a new seed.
        SourceRandom restored(0);
        restored.set_state(rng.state());
        check(restored.raw_draw_count() == 0, "restore resets diagnostics");
        for (unsigned i = 0; i < 16; ++i) check(restored.randi() == rng.randi(), "imported state continues unchanged");
        rng.seed(seed);
        check(rng.state() == state && rng.raw_draw_count() == 0, "reseed repeats initial state");
    }
    check(ref.finished(), "no unexamined reference tail");
    check(operations == 1888, "all global reference operations covered");
    check(saw_zero_range, "actual engine rare exponent-zero range branch covered");
    check(saw_one_float, "actual engine randf upper endpoint covered");
    SourceRandom zero(0);
    zero.set_state(0);
    check(zero.rand_range(-7, 11) == -7 && zero.raw_draw_count() == 1, "zero state takes one-draw exponent branch");
    zero.set_state(0);
    check(zero.randi() == 0, "zero-output rotation does not shift by32");
    zero.set_state(std::numeric_limits<uint64_t>::max());
    check(zero.randi() == 0xfff00001U, "rotation31 boundary is defined");
    std::cout << "Source RNG: " << checks << " checks across " << operations
              << " official-engine operations; binary32 global randf and binary64 ranges verified\n";
}
