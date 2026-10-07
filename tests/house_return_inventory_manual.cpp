// Manual CTR source-owner negative cases. Not registered, compiled or run.
// The integrator supplies real loaded resources/native owners; these cases
// deliberately do not manufacture Ready, source receipts or reference IDs.
#include "../platform/ctr/house_return_inventory.hpp"
#include <cassert>

namespace encore::ctr::manual {
void house_inventory_unbound_rejects() {
  HouseReturnInventoryOwner owner;
  upstream::OpeningHouseInventoryState receipt;
  upstream::FieldObjectId receiver=0;
  std::string error,name;
  assert(!owner.observe_inventory(receipt,error)&&!error.empty());
  error.clear();
  assert(!owner.item_receiver(receiver,name,error)&&!error.empty());
  assert(!owner.inventory_space());
  assert(!owner.grant_item({},"",error));
}
void house_inventory_absent_authority_rejects(HouseReturnInventoryInput actual) {
  actual.inventory=nullptr;
  HouseReturnInventoryOwner owner;
  std::string error;
  assert(!owner.prepare(actual,error)&&!error.empty());
}
// Supply a real source global.party with multiple members, or a source
// GlobalData owner bound to a different retained Inventory host. Preparation
// must reject before allocation, RNG, UID or any inventory commit occurs.
void house_inventory_foreign_or_multi_party_rejects(HouseReturnInventoryInput actual) {
  HouseReturnInventoryOwner owner;
  std::string error;
  assert(!owner.prepare(actual,error)&&!error.empty());
}
void house_inventory_wrong_template_rejects(HouseReturnInventoryOwner &prepared,
                                           upstream::DrawerItemTemplate real,
                                           std::string_view actual_name) {
  ++real.doses;
  std::string error;
  assert(!prepared.validate_item(real,actual_name,error)&&!error.empty());
}
// Supply a bound source fixture whose global.item is absent, foreign, a key
// Item or detached from the actual singleton normal Inventory. Native text
// must reject its ItemReceiver instead of using a constant Ninten identity.
void house_inventory_wrong_receiver_rejects(HouseReturnInventoryOwner &actual) {
  upstream::FieldObjectId receiver=0;
  std::string name,error;
  assert(!actual.item_receiver(receiver,name,error)&&!error.empty());
}
void house_inventory_legacy_effects_reject(HouseReturnInventoryOwner &actual) {
  std::string error;
  assert(!actual.show_text(0,error)&&!error.empty());
  assert(!actual.play_sound("",error)&&!error.empty());
  assert(!actual.set_flag("",true,error)&&!error.empty());
}
void house_inventory_core_absent_source_rejects() {
  upstream::FieldItemDefinitionsRuntime source;
  upstream::FieldItemResult result;
  std::string error;
  assert(!source.source_drawer_grant({}, {}, "", 0, result,error)&&!error.empty());
}
} // namespace encore::ctr::manual
