#include "../tests/manual_require.hpp"
#include "encore/player_child_scripts.hpp"
// Explicit manual invocation only. No lifecycle, probes, or test execution in
// production conversion/build. NDEBUG still executes every admission action.
void player_child_scripts_format_cases(
    const std::vector<uint8_t> &original,
    const encore::upstream::PlayerInitializationData &player,
    const encore::upstream::PlayerReadyData &ready) {
  using namespace encore::upstream;
  std::string error;
  PlayerChildScriptsData data;
  MANUAL_REQUIRE(
      data.load(original.data(), original.size(), player, ready, error));
  MANUAL_REQUIRE(data.records().size() == 4);
  MANUAL_REQUIRE(data.camera().records().size() == 1);
  MANUAL_REQUIRE(data.arrows().records().size() == 1);
  for (size_t at : {size_t(8), size_t(24), size_t(28), size_t(36), size_t(40),
                    size_t(60), size_t(124)}) {
    auto raw = original;
    raw.at(at) ^= 0x80;
    PlayerChildScriptsData bad;
    MANUAL_REQUIRE(!bad.load(raw.data(), raw.size(), player, ready, error));
  }
  auto trailing = original;
  trailing.push_back(0);
  PlayerChildScriptsData bad;
  MANUAL_REQUIRE(
      !bad.load(trailing.data(), trailing.size(), player, ready, error));
  MANUAL_REQUIRE(!bad.load(original.data(), 127, player, ready, error));
  PlayerChildScriptsRuntime runtime;
  FieldNodeBinding binding;
  MANUAL_REQUIRE(
      !runtime.ready(1, FieldTreePhase::ReadyScript, binding, error));
  MANUAL_REQUIRE(!runtime.animation_started("unknown", error));
  MANUAL_REQUIRE(!runtime.set_bubble_offset(error));
}
