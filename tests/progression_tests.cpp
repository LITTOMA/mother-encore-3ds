#include "room_fixture.hpp"
#include "encore/progression.hpp"
#include "fixtures/progression_v0410.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

static unsigned checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while (0)
int main() {
    using namespace encore::upstream;
    for (const auto& row : progression_reference::levels) {
        int32_t value = -123;
        CHECK(experience_for_level(encore_test::room(),row.input, value));
        CHECK(value == row.expected);
    }
    for (unsigned xp = 0; xp < sizeof(progression_reference::xp_levels); ++xp) {
        CHECK(level_for_experience(encore_test::room(),static_cast<int32_t>(xp)) == progression_reference::xp_levels[xp]);
    }
    for (const auto& row : progression_reference::extra_xp) {
        CHECK(level_for_experience(encore_test::room(),row.input) == row.expected);
    }
    for (int32_t level : {int32_t(0), int32_t(-1), std::numeric_limits<int32_t>::min()}) {
        int32_t value = 123;
        CHECK(!experience_for_level(encore_test::room(),level, value));
        CHECK(value == 123);
    }
    std::printf("Upstream 0.4.1.0 progression: %u checks against Godot 3.6.2 reference\n", checks);
}
