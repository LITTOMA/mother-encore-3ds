#include "manual_require.hpp"
#include "platform/ctr/podunk_global_data_host.hpp"
// Real source Item factory and actual Reference ownership are checked by this
// host. This manual case is compiled only; no platform/runtime test is run.
void podunk_god_storage_host_manual() {
  encore::ctr::PodunkGlobalDataHost host;
  std::string e;
  encore::upstream::FieldItemDefinitions absent;
  MANUAL_REQUIRE(!host.initialize_items_cache(absent, e));
  encore::upstream::SourceRandom random(1);
  std::vector<uint32_t> ledger;
  MANUAL_REQUIRE(!host.construct_god_storage("en", random, ledger, {}, e));
  MANUAL_REQUIRE(ledger.empty() && !host.runtime().god_storage_complete());
  encore::upstream::FieldObjectId unchanged = 123;
  MANUAL_REQUIRE(!host.read_reference_member(0, "missing", unchanged, e));
  MANUAL_REQUIRE(unchanged == 123);
}
// Caller supplies actual constructed globalData/source cache owner. A fixture
// ID or alternate source inventory cannot stand in for that ownership.
void podunk_god_storage_actual_owner_manual(
    encore::ctr::PodunkGlobalDataHost &host,
    const encore::upstream::FieldItemDefinitions &defs,
    encore::upstream::FieldGlobalRegistry &registry,
    encore::upstream::SourceRandom &random, std::vector<uint32_t> &ledger,
    encore::upstream::LoadRngClockProvider clock) {
  std::string e;
  MANUAL_REQUIRE(host.items_cache().definitions_loaded());
  MANUAL_REQUIRE(host.construct_god_storage("en", random, ledger, clock, e));
  MANUAL_REQUIRE(host.runtime().god_storage_complete());
  MANUAL_REQUIRE(host.runtime().god_storage_items().size() ==
                 defs.definitions().size());
  encore::upstream::FieldObjectId reference = 0;
  MANUAL_REQUIRE(host.read_reference_member(host.runtime().globaldata_object(),
                                            defs.god_storage_member(),
                                            reference, e));
  MANUAL_REQUIRE(reference && host.object_exists(reference));
  const auto saved_reference = reference;
  MANUAL_REQUIRE(
      !host.read_reference_member(0, defs.god_storage_member(), reference, e));
  MANUAL_REQUIRE(reference == saved_reference);
  MANUAL_REQUIRE(!host.read_reference_member(
      host.runtime().globaldata_object(), defs.god_storage_member() + "unknown",
      reference, e));
  for (const auto &item : host.runtime().god_storage_items()) {
    MANUAL_REQUIRE(item.registry == &registry && item.actual_owner);
    MANUAL_REQUIRE(host.object_exists(item.object));
    MANUAL_REQUIRE(
        host.items_cache().source_id_assigned(item.value.definition));
  }
  MANUAL_REQUIRE(!host.construct_god_storage("en", random, ledger, clock, e));
  // The source saved UID value zero is independent of a nonzero ObjectDB ID.
  std::shared_ptr<encore::ctr::PodunkItemObject> zero;
  MANUAL_REQUIRE(encore::ctr::PodunkInventoryHost::reserve_god_storage_item(
      defs, registry, defs.god_storage_id(), zero, e));
  auto cache = host.items_cache();
  const auto &definition = defs.definitions().front();
  encore::upstream::FieldOwnedItem value{definition.id, 0, definition.doses,
                                         false};
  MANUAL_REQUIRE(encore::ctr::PodunkInventoryHost::initialize_god_storage_item(
      zero, defs, value, cache, e));
  MANUAL_REQUIRE(zero->value.uid == 0 && zero->object != 0);
}
