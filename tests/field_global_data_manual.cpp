#include "encore/field_global_data.hpp"
#include <algorithm>
#include <cassert>
// Manual-only parser cases. Caller provides the genuinely generated pack and
// source identity; these cases are not registered or executed automatically.
void global_data_negative_manual(const std::vector<uint8_t> &binary,
                                 encore::upstream::FieldIdentity identity) {
  using namespace encore::upstream;
  std::string error;
  FieldGlobalDataData source;
  assert(source.load(binary.data(), binary.size(), identity, error));
  assert(!source.pending().empty());
  FieldGlobalDataRuntime owner;
  assert(!owner.constructor_complete() && !owner.ready_complete() &&
         !owner.load_complete());
  FieldGlobalDataObject untouched;
  untouched.object = 321;
  assert(!owner.read_constructed_object(0, untouched, error));
  assert(untouched.object == 321);
  assert(!owner.read_constructed_object(1, untouched, error));
  assert(untouched.object == 321);
  for (size_t size = 0; size < 84; ++size)
    assert(!source.load(binary.data(), size, identity, error));
  for (auto index : {8u, 20u, 24u, 28u}) {
    auto corrupt = binary;
    corrupt.at(index) ^= 128;
    assert(!source.load(corrupt.data(), corrupt.size(), identity, error));
  }
  identity.upstream_commit[0] ^= 1;
  assert(!source.load(binary.data(), binary.size(), identity, error));
}

// The caller constructs this actual source owner through the same registry
// before invoking this manual case. No fake ObjectDB/lifecycle is supplied.
void global_data_synchronous_setter_manual(
    encore::upstream::FieldGlobalDataRuntime &owner,
    const encore::upstream::FieldInventoryData &inventory,
    const encore::upstream::FieldItemDefinitions &definitions,
    encore::upstream::SourceRandom &random, std::vector<uint32_t> &ledger,
    encore::upstream::LoadRngClockProvider clock,
    encore::upstream::FieldGlobalDataItemFactory factory) {
  using namespace encore::upstream;
  std::string error;
  size_t signals = 0;
  auto receive = [&](FieldObjectId id, const std::string &stat, int64_t raw,
                     int64_t maximum, std::string &why) {
    assert(!owner.prefix_complete());
    FieldGlobalDataObject current;
    assert(owner.read_constructed_object(id, current, why));
    assert(current.object == id && current.inventory != 0);
    const auto &policy = owner.data()->character_policy();
    auto found = std::find_if(
        current.fields.begin(), current.fields.end(),
        [&](const auto &value) { return value.name == "_" + stat; });
    assert(found != current.fields.end());
    // Character.set_stat emits the requested raw HP/PP, although its actual
    // stored value has already been clamped. Other stats emit assigned value.
    if (stat == policy.slots[4].substr(1) || stat == policy.slots[5].substr(1))
      assert(found->integer_value == std::clamp<int64_t>(raw, 0, maximum));
    else
      assert(found->integer_value == raw);
    if (!signals)
      assert(current.learned_skills.empty() && current.affinities.empty());
    ++signals;
    return true;
  };
  assert(owner.load_inventory_prefix(inventory, definitions, nullptr, random,
                                     ledger, std::move(clock),
                                     std::move(factory), receive, error));
  assert(signals != 0 && owner.prefix_complete());
  assert(!owner.load_complete() && !owner.ready_complete());
}
