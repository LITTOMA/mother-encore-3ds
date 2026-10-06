// Manual platform-owner negatives; not automatically registered/executed.
#include "podunk_global_data_host.hpp"
#include <cassert>
void global_data_host_snapshot_manual(
    const encore::ctr::PodunkGlobalDataHost &host,
    const encore::upstream::FieldInventoryState &actual) {
  using namespace encore::ctr;
  std::string error;
  PodunkInventorySnapshot exported;
  assert(host.make_snapshot(actual, exported, error));
  PodunkInventorySnapshot unchanged;
  unchanged.state.cash = 321;
  auto invalid = actual;
  ++invalid.revision;
  assert(!host.make_snapshot(invalid, unchanged, error));
  assert(unchanged.state.cash == 321);
  invalid = actual;
  // Cash has no Item/UID difference, so an item-only check cannot catch this
  // unsupported attempt to substitute a saved projection for actual LOAD.
  ++invalid.cash;
  assert(!host.make_snapshot(invalid, unchanged, error));
  assert(unchanged.state.cash == 321);
}
void global_data_host_missing_owner_manual() {
  encore::ctr::PodunkGlobalDataHost host;
  encore::ctr::PodunkInventoryHost inventory;
  std::string error;
  assert(!host.bind_live_inventory(inventory, error));
  encore::upstream::FieldGlobalDataObject unchanged;
  unchanged.object = 321;
  assert(!host.read_object(0, unchanged, error));
  assert(unchanged.object == 321);
}
