#include "encore/podunk_player_character.hpp"
#include "manual_require.hpp"
#include "podunk_house_global_bridge.hpp"
int main() {
  using namespace encore::upstream;
  encore::ctr::PodunkHouseGlobalBridge bridge;
  HouseStatusActualData out;
  out.object = 123;
  std::string e;
  MANUAL_REQUIRE(!bridge.status_data(0, 0, out, e) && out.object == 123 &&
                 !e.empty());
  HouseStatusEffectsRuntime effects;
  bool result = true;
  MANUAL_REQUIRE(
      !effects.boolean_effect(0, HouseStatusBoolean::Sweat, result, e) &&
      result);
  MANUAL_REQUIRE(!effects.boolean_effect(0, static_cast<HouseStatusBoolean>(3),
                                         result, e) &&
                 result);
  FieldGlobalDataRuntime absent;
  FieldGlobalRegistry registry;
  HouseStatusEffectsData data;
  MANUAL_REQUIRE(!effects.initialize(data, absent, registry, {}, e));
  PodunkPlayerCharacter character;
  MANUAL_REQUIRE(!character.bind_status_effects(effects, e));
  return 0;
}
