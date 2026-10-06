#include "encore/global_load_runtime.hpp"
#include <cassert>

// Manual negative cases only. This file is not registered as an automatic
// test and does not instantiate source scenes or grant a bootstrap receipt.
void global_load_missing_actual_owners_manual() {
  using namespace encore::upstream;
  GlobalLoadRuntime load;
  FieldGlobalDataRuntime core;
  GlobalLoadData data;
  FieldGlobalRegistry registry;
  GlobalItemCache items;
  SourceRandom random(0);
  std::vector<uint32_t> uids;
  std::string error;
  assert(!load.initialize(data, core, registry, items, random, uids, {}, {},
                          error));
  assert(!error.empty());
  assert(!load.complete() && !core.load_complete());
  assert(!load.load_cold_default(error));
  assert(!load.poisoned() && load.cursor() == 0 && uids.empty());
  assert(!core.initialize_global_load(data, error));
  FieldObjectId object = 0;
  assert(!core.global_load_inventory(0, object, error) && object == 0);
  uint32_t declaration = 0;
  assert(!core.load_inventory_owner(1, declaration, error) && declaration == 0);
  assert(!core.global_load_normal_flag({}, true, error));
  auto dictionary = std::make_shared<GlobalYamlValue>();
  dictionary->kind = 6;
  assert(!core.assign_global_load_member({}, dictionary, nullptr, error));
}
