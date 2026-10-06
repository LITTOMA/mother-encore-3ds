#include "../tests/manual_require.hpp"
#include "encore/crc32.hpp"
#include "encore/player_effects.hpp"
namespace encore::manual {
using namespace upstream;
namespace {
void put(std::vector<uint8_t> &b, size_t i, uint32_t value) {
  for (size_t j = 0; j < 4; ++j)
    b.at(i + j) = uint8_t(value >> (8 * j));
}
} // namespace
// Run only in an explicitly requested manual full check. The caller supplies
// the independently loaded actual Player data and real generated effect bytes.
void player_effects_format_manual(const PlayerInitializationData &player,
                                  const std::vector<uint8_t> &bytes) {
  MANUAL_REQUIRE(player.valid());
  MANUAL_REQUIRE(bytes.size() >= 128);
  std::string e;
  PlayerEffectsData d;
  MANUAL_REQUIRE(d.load(bytes.data(), bytes.size(), player, e));
  MANUAL_REQUIRE(d.creators().size() == 2);
  MANUAL_REQUIRE(d.recipe(0) && d.recipe(1));
  MANUAL_REQUIRE(d.native(0) && d.native(1));
  for (size_t offset : {size_t(8), size_t(24), size_t(28), size_t(32)}) {
    auto damaged = bytes;
    put(damaged, offset, 0xffffffffu);
    PlayerEffectsData bad;
    MANUAL_REQUIRE(!bad.load(damaged.data(), damaged.size(), player, e));
    MANUAL_REQUIRE(!bad.valid());
  }
  auto corrupt = bytes;
  corrupt.at(20) ^= 1;
  MANUAL_REQUIRE(!d.load(corrupt.data(), corrupt.size(), player, e));
  auto wrong = bytes;
  wrong[128] ^= 1;
  put(wrong, 20, crc32(wrong.data() + 128, wrong.size() - 128));
  MANUAL_REQUIRE(!d.load(wrong.data(), wrong.size(), player, e));
  auto foreign = bytes;
  foreign[40] ^= 1;
  MANUAL_REQUIRE(!d.load(foreign.data(), foreign.size(), player, e));
  auto trailing = bytes;
  trailing.push_back(0);
  put(trailing, 16, uint32_t(trailing.size()));
  put(trailing, 20, crc32(trailing.data() + 128, trailing.size() - 128));
  MANUAL_REQUIRE(!d.load(trailing.data(), trailing.size(), player, e));
  MANUAL_REQUIRE(!d.load(bytes.data(), 127, player, e));
}
// A real caller can run this before the actual source Ready cursor. This
// proves neither an unknown ObjectID nor callback existence creates a Node,
// consumes RNG, or marks the source object Ready.
void player_effects_unready_manual(PlayerEffectsRuntime &actual,
                                   SourceRandom &random,
                                   FieldObjectId missing) {
  MANUAL_REQUIRE(!actual.state(missing));
  auto before = random.state();
  auto draws = random.raw_draw_count();
  std::string e;
  MANUAL_REQUIRE(!actual.create_dust(missing, e));
  MANUAL_REQUIRE(!actual.create_after_image(missing, e));
  MANUAL_REQUIRE(!actual.timeout(missing, e));
  MANUAL_REQUIRE(random.state() == before);
  MANUAL_REQUIRE(random.raw_draw_count() == draws);
}
} // namespace encore::manual
