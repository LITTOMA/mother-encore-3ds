#include "manual_require.hpp"
#include "podunk_house_global_bridge.hpp"
#include "podunk_global_data_singleton.hpp"
#include "podunk_global_host.hpp"
#include "podunk_global_native.hpp"
int main() {
  encore::ctr::PodunkHouseGlobalBridge owner;
  encore::upstream::FieldInventoryData data;
  encore::ctr::PodunkInventorySnapshot out;
  std::string error;
  MANUAL_REQUIRE(!owner.snapshot(data, out, error));
  MANUAL_REQUIRE(!error.empty());
  MANUAL_REQUIRE(!owner.core().complete());
  std::map<std::string, std::shared_ptr<encore::upstream::GlobalYamlValue>>
      fields;
  MANUAL_REQUIRE(!owner.status_fields(0, fields, error));
  encore::upstream::HouseGlobalBridgeData bridge;
  encore::upstream::FieldGlobalDataRuntime actual_data;
  encore::upstream::FieldGlobalConstructorRuntime actual_global;
  encore::upstream::FieldGlobalRegistry registry;
  encore::ctr::PodunkGlobalDataHost characters;
  encore::upstream::NativeSessionData session;
  encore::upstream::SourceRandom played(17);
  const std::vector<uint32_t> ledger;
  const auto original_state=played.state(),original_draws=played.raw_draw_count();
  MANUAL_REQUIRE(!owner.construct_continuation_autoload(0,bridge,session,{},{},{},{},{},registry,played,ledger,error));
  MANUAL_REQUIRE(played.state()==original_state&&played.raw_draw_count()==original_draws);
  MANUAL_REQUIRE(!owner.prepare_continuation(bridge, characters, actual_global, registry, error));
  MANUAL_REQUIRE(!actual_data.house_continuation_prepared());
  MANUAL_REQUIRE(!characters.runtime().house_continuation_complete());
  MANUAL_REQUIRE(!characters.runtime().ready_complete());
  MANUAL_REQUIRE(!characters.runtime().load_complete());
  encore::ctr::PodunkGlobalDataSingleton singleton;
  encore::ctr::PodunkGlobalHost global_host;
  encore::ctr::PodunkGlobalNative native;
  MANUAL_REQUIRE(!owner.core().binds_source_owners(actual_data,actual_global,registry));
  MANUAL_REQUIRE(!singleton.bind_continuation(owner.core(),actual_global,error));
  MANUAL_REQUIRE(!singleton.adopt_continuation_ready(error));
  MANUAL_REQUIRE(!singleton.continuation_native_ready());
  MANUAL_REQUIRE(!global_host.bind_continuation(owner.core(),actual_data,error));
  MANUAL_REQUIRE(!actual_data.ready_complete());
  MANUAL_REQUIRE(!actual_data.load_complete());
  return 0;
}
