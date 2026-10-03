#include "encore/load_rng.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

using namespace encore::upstream;
namespace {
unsigned checks = 0;
void check(bool good, const char* message) {
    ++checks;
    if (!good) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
class Reference {
public:
    explicit Reference(const char* path) {
        std::ifstream input(path, std::ios::binary);
        check(bool(input), "official engine LOAD reference exists");
        data_ = std::vector<uint8_t>(std::istreambuf_iterator<char>(input), {});
    }
    uint64_t integer(unsigned size) {
        check(cursor_ + size <= data_.size(), "LOAD reference is not truncated");
        uint64_t value = 0;
        for (unsigned i = 0; i < size; ++i) value |= uint64_t(data_[cursor_++]) << (i * 8);
        return value;
    }
    std::string string() {
        const auto size = integer(4);
        std::string result;
        for (uint64_t i = 0; i < size; ++i) result.push_back(static_cast<char>(integer(1)));
        return result;
    }
    std::vector<uint32_t> values() {
        const auto count = integer(4);
        std::vector<uint32_t> values;
        for (uint64_t i = 0; i < count; ++i) values.push_back(static_cast<uint32_t>(integer(4)));
        return values;
    }
    bool finished() const { return cursor_ == data_.size(); }
private:
    std::vector<uint8_t> data_;
    size_t cursor_ = 0;
};
}

int main(int argc, char** argv) {
    check(argc == 2, "provide reports/load-rng/reference.bin");
    Reference ref(argv[1]);
    for (char ch : {'E','L','R','N','G','3','6','2'})
        check(ref.integer(1) == static_cast<uint8_t>(ch), "LOAD reference signature");
    const auto case_count = ref.integer(4);
    check(case_count == 8, "all official engine LOAD cases supplied");
    size_t allocations = 0, retries = 0;
    for (uint64_t c = 0; c < case_count; ++c) {
        const auto name = ref.string();
        SourceRandom random(0);
        random.set_state(ref.integer(8));
        auto ledger = ref.values();
        const auto row_count = ref.integer(4);
        std::vector<LoadInventoryAllocation> rows;
        for (uint64_t i = 0; i < row_count; ++i) {
            const auto id = static_cast<uint32_t>(ref.integer(4));
            const auto count = static_cast<uint32_t>(ref.integer(4));
            rows.push_back({id, count});
        }
        check(rows.size() == 10, "source order includes keys storage five members three NPCs");
        for (size_t i = 0; i < rows.size(); ++i)
            check(rows[i].order_id == i, "exact engine source dictionary order");
        for (size_t i = 7; i < rows.size(); ++i)
            check(rows[i].item_count == 0, "PartyNPC rows do not rebuild inventory");
        const auto sample_count = ref.integer(4);
        std::vector<LoadRngClockSample> samples;
        for (uint64_t i = 0; i < sample_count; ++i) {
            const auto seconds = ref.integer(8), ticks = ref.integer(8);
            samples.push_back({seconds, ticks});
        }
        const auto expected_count = ref.integer(4);
        std::vector<LoadUidAllocation> expected;
        for (uint64_t i = 0; i < expected_count; ++i) {
            const auto row = static_cast<uint32_t>(ref.integer(4));
            const auto item = static_cast<uint32_t>(ref.integer(4));
            const auto uid = static_cast<uint32_t>(ref.integer(4));
            const auto draws = ref.integer(8), seed = ref.integer(8);
            expected.push_back({row, item, uid, draws, seed});
        }
        const auto final_state = ref.integer(8);
        const auto final_ledger = ref.values();
        const auto retained = ref.values();
        const auto sentinel = ref.integer(4);
        size_t sampled = 0;
        std::string error;
        const auto clock = [&](LoadRngClockSample& sample, std::string&) {
            check(sampled < samples.size(), "no extra randomize calls");
            sample = samples[sampled++];
            return true;
        };
        std::vector<LoadUidAllocation> actual;
        check(apply_load_uid_allocations(random, ledger, rows, clock, error, &actual), "LOAD UID operation succeeds");
        check(error.empty(), "successful operation clears error");
        check(sampled == samples.size(), "one clock sample per original Item.get_uid");
        check(actual.size() == expected.size(), "one UID append per fallback evaluation");
        for (size_t i = 0; i < expected.size(); ++i) {
            const auto& a = actual[i]; const auto& e = expected[i];
            check(a.order_id == e.order_id && a.item_index == e.item_index, "source row and item order");
            check(a.seed == e.seed, "official randomize arithmetic with wrapping clock fixtures");
            check(a.generated_uid == e.generated_uid, "fallback UID matches official engine");
            check(a.raw_draws == e.raw_draws, "exact randi count including collision retry");
            retries += a.raw_draws - 1;
        }
        check(random.state() == final_state, "same global stream after LOAD");
        check(ledger == final_ledger, "discarded fallback appended while saved UID unregistered");
        check(random.randi() == sentinel, "same subsequent global random draw");
        if (name == "full_saved_native_projection") {
            check(actual.size() == 4, "inactive source character items included in native projection");
            check(retained == std::vector<uint32_t>({801,802,803,804}), "oracle retained all present saved UIDs");
            check(rows[0].item_count == 1 && rows[2].item_count == 1 && rows[4].item_count == 2,
                  "key first then first member then inactive third member items");
        }
        if (name == "missing_uid")
            check(retained.size() == 1 && retained[0] == actual[0].generated_uid,
                  "oracle missing UID receives fallback");
        if (name == "same_process_second_load")
            check(final_ledger.size() > actual.size(), "second load preserves prior generated ledger");
        allocations += actual.size();
    }
    check(ref.finished(), "complete reference consumed");
    check(retries == 4, "both forced two-collision cases exercised");

    SourceRandom random(73);
    random.randi();
    const auto original_state = random.state(), original_draws = random.raw_draw_count();
    std::vector<uint32_t> ledger{16, 19};
    const auto original_ledger = ledger;
    std::vector<LoadUidAllocation> trace{{91, 8, 17, 2, 3}};
    std::string error;
    unsigned calls = 0;
    auto failing_clock = [&](LoadRngClockSample& sample, std::string& failure) {
        ++calls;
        sample = {1, 2};
        if (calls == 2) { failure = "fixture clock failure"; return false; }
        return true;
    };
    check(!apply_load_uid_allocations(random, ledger, {{8,2}}, failing_clock, error, &trace), "clock failure rejects operation");
    check(calls == 2 && error == "fixture clock failure", "provider failure propagated");
    check(random.state() == original_state && random.raw_draw_count() == original_draws,
          "failed apply leaves live RNG and diagnostics untouched");
    check(ledger == original_ledger && trace.size() == 1 && trace[0].generated_uid == 17,
          "failed apply leaves live ledger and trace untouched");
    calls = 0;
    check(!apply_load_uid_allocations(random, ledger, {{8,1},{8,0}}, failing_clock, error), "duplicate row rejects before sampling");
    check(calls == 0 && random.state() == original_state, "structural validation consumes no entropy or RNG");
    check(!apply_load_uid_allocations(random, ledger, {{8,1}}, {}, error), "missing provider rejects nonempty load");
    check(random.state() == original_state && ledger == original_ledger, "missing provider does not mutate");
    check(apply_load_uid_allocations(random, ledger, {{8,0},{2,0}}, {}, error, &trace), "empty ordered rows need no clock");
    check(random.state() == original_state && random.raw_draw_count() == original_draws && ledger == original_ledger && trace.empty(),
          "empty load preserves stream and ledger and emits no allocations");
    check(apply_load_uid_allocations(random, ledger, {}, {}, error), "zero inventory load succeeds");
    std::cout << "LOAD RNG: " << checks << " checks; " << allocations
              << " official-engine allocations, " << retries << " collision retries; transactional failure checks passed\n";
}
