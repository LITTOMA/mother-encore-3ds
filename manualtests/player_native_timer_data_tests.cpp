#include "../tests/manual_require.hpp"
#include "encore/field_native_timer.hpp"
#include "encore/player_initialization.hpp"
#include <algorithm>
// Manual-only. The caller supplies the real checked production Player resource.
void player_native_timer_source_cases(
    const encore::upstream::PlayerInitializationData &player) {
  using namespace encore::upstream;
  std::string e;
  FieldNativeTimerData timers;
  MANUAL_REQUIRE(timers.load_player(player, e));
  auto count = std::count_if(
      player.recipe().records().begin(), player.recipe().records().end(),
      [](const auto &r) { return r.native_class == "Timer"; });
  MANUAL_REQUIRE(timers.records().size() == size_t(count));
  for (const auto &r : timers.records()) {
    auto *source = player.recipe().record(r.id);
    MANUAL_REQUIRE(source && source->native_class == "Timer");
    MANUAL_REQUIRE(r.identity.scene_id == player.identity().scene_id);
    MANUAL_REQUIRE(r.identity.source_sha256 == player.identity().source_sha256);
    MANUAL_REQUIRE(r.script_sha == source->script_sha);
    auto bad = r.identity;
    bad.scene_id ^= 0x80;
    MANUAL_REQUIRE(!timers.record(bad, r.id));
    bad = r.identity;
    bad.source_sha256[0] ^= 1;
    MANUAL_REQUIRE(!timers.record(bad, r.id));
  }
  PlayerInitializationData absent;
  FieldNativeTimerData rejected;
  MANUAL_REQUIRE(!rejected.load_player(absent, e));
  MANUAL_REQUIRE(!rejected.valid());
}
