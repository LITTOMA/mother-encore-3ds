#include "manual_require.hpp"
#include "podunk_house_global_bridge.hpp"
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
  return 0;
}
