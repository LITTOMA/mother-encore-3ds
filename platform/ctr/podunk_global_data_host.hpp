#pragma once
#include "encore/field_global_data.hpp"
#include "podunk_inventory_host.hpp"
namespace encore::ctr {
// Source globaldata's actual Character Objects / Inventory References. This
// owns the selected declarations, LOAD prefix, and Items/GodStorage constructor
// consumers. Remaining caches/characters and the full Ready traversal still
// require their actual source owners; legacy Session data is not substituted.
class PodunkGlobalDataHost final {
  upstream::FieldGlobalDataRuntime owner_;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  const upstream::FieldInventoryData *inventory_ = nullptr;
  const upstream::FieldItemDefinitions *definitions_ = nullptr;
  const PodunkInventoryHost *live_inventory_ = nullptr;
  upstream::GlobalItemCache item_cache_;
  std::map<upstream::FieldObjectId, std::weak_ptr<PodunkItemObject>> items_;
  static bool fail(std::string &e, const char *s) {
    e = s;
    return false;
  }

public:
  bool construct_members(const upstream::FieldGlobalDataData &data,
                         const upstream::FieldGlobalExternalSpec &spec,
                         upstream::FieldObjectId object,
                         upstream::FieldGlobalRegistry &registry,
                         std::string &error) {
    if (registry_)
      return fail(error, "globalData owning host reinitialized");
    if (!owner_.construct_members(data, spec, object, registry, error))
      return false;
    registry_ = &registry;
    return true;
  }
  bool initialize_items_cache(const upstream::FieldItemDefinitions &defs,
                              std::string &e) {
    if (!registry_ || !owner_.data())
      return fail(e, "Actual globalData owner required for Items cache");
    if (defs.source_pin() != owner_.data()->identity().upstream_commit)
      return fail(e, "Global Items foreign upstream source");
    return item_cache_.initialize(defs, owner_.globaldata_object(), e);
  }
  bool insert_loaded_item_yaml(const std::string &source,
                               const std::array<uint8_t, 32> &hash,
                               std::string &e) {
    return item_cache_.insert_loaded_yaml(source, hash, e);
  }
  const auto &items_cache() const { return item_cache_; }
  bool observe_items_directory_complete(
      const std::vector<std::pair<std::string, std::array<uint8_t, 32>>> &paths,
      const std::array<uint8_t, 32> &closure_proof, std::string &e) {
    return item_cache_.observe_directory_complete(paths, closure_proof, e);
  }
  bool read_reference_member(upstream::FieldObjectId actual_globaldata,
                             const std::string &source_member,
                             upstream::FieldObjectId &out,
                             std::string &e) const {
    return owner_.read_reference_member(actual_globaldata, source_member, out,
                                        e);
  }
  bool construct_god_storage(const std::string &locale,
                             upstream::SourceRandom &random,
                             std::vector<uint32_t> &ledger,
                             upstream::LoadRngClockProvider clock,
                             std::string &e) {
    upstream::FieldGlobalDataGodItemFactory factory;
    factory.reserve = [this](const auto &defs, auto &registry,
                             uint32_t stable_owner,
                             upstream::FieldGlobalDataItemReference &out,
                             std::string &error) {
      std::shared_ptr<PodunkItemObject> item;
      if (&registry != registry_ ||
          !PodunkInventoryHost::reserve_god_storage_item(
              defs, registry, stable_owner, item, error))
        return false;
      out = {&registry, item->object, stable_owner, {}, item};
      items_[item->object] = item;
      return true;
    };
    factory.initialize = [this](const auto &defs, const auto &value,
                                auto &cache,
                                upstream::FieldGlobalDataItemReference &ref,
                                std::string &error) {
      auto found = items_.find(ref.object);
      auto item = found == items_.end() ? nullptr : found->second.lock();
      if (!item || ref.registry != registry_ ||
          ref.actual_owner.get() != item.get() ||
          !PodunkInventoryHost::initialize_god_storage_item(item, defs, value,
                                                            cache, error))
        return false;
      ref.value = item->value;
      return true;
    };
    return owner_.construct_god_storage(item_cache_, locale, random, ledger,
                                        std::move(clock), factory, e);
  }
  bool load_inventory_prefix(const upstream::FieldInventoryData &data,
                             const upstream::FieldItemDefinitions &definitions,
                             const upstream::FieldInventoryState *saved,
                             upstream::SourceRandom &random,
                             std::vector<uint32_t> &ledger,
                             upstream::LoadRngClockProvider clock,
                             upstream::FieldGlobalDataStatSignal stat_signal,
                             PodunkInventorySnapshot &snapshot,
                             std::string &error) {
    if (!registry_ || inventory_)
      return fail(error, "globalData LOAD cursor missing/repeated");
    auto factory = [this](const auto &inventory, const auto &defs,
                          auto &registry, auto stable_owner, const auto &value,
                          upstream::FieldGlobalDataItemReference &out,
                          std::string &e) {
      std::shared_ptr<PodunkItemObject> actual;
      if (&registry != registry_ ||
          !PodunkInventoryHost::construct_source_item(
              inventory, defs, registry, stable_owner, value, actual, e) ||
          !PodunkInventoryHost::check_source_item(actual, registry))
        return false;
      if (!items_.emplace(actual->object, actual).second)
        return fail(e, "globalData actual Item Reference reused");
      out = {&registry, actual->object, stable_owner, value, actual};
      return true;
    };
    if (!owner_.load_inventory_prefix(data, definitions, saved, random, ledger,
                                      std::move(clock), factory,
                                      std::move(stat_signal), error)) {
      items_.clear();
      return false;
    }
    inventory_ = &data;
    definitions_ = &definitions;
    if (!make_snapshot(owner_.prefix_state(), snapshot, error))
      return false;
    owner_.release_item_references();
    return true;
  }
  bool object_exists(upstream::FieldObjectId object) const {
    if (live_inventory_ && live_inventory_->object_exists(object))
      return true;
    if (owner_.object_exists(object))
      return true;
    auto i = items_.find(object);
    return registry_ && i != items_.end() &&
           PodunkInventoryHost::check_source_item(i->second.lock(), *registry_);
  }
  bool owners(const upstream::FieldInventoryData &data,
              const std::vector<PodunkInventoryOwnerObject> &owners,
              const upstream::FieldInventoryState &state,
              std::string &error) const {
    std::vector<std::pair<uint32_t, upstream::FieldObjectId>> actual;
    for (const auto &o : owners)
      actual.emplace_back(o.owner, o.object);
    upstream::FieldInventoryRuntime check;
    if (!inventory_ || !definitions_ ||
        data.identity() != inventory_->identity() ||
        !owner_.check_owners(data, actual, error) ||
        !check.initialize(data, *definitions_, state, error))
      return false;
    // New items bought/granted after prefix construction belong to the actual
    // PodunkInventoryHost's own ObjectDB table; this owner proves Inventory
    // References and source domain, never creates items merely to match a save.
    error.clear();
    return true;
  }
  bool make_snapshot(const upstream::FieldInventoryState &state,
                     PodunkInventorySnapshot &out, std::string &error) const {
    if (!inventory_ || !definitions_ || !owner_.prefix_complete())
      return fail(error, "globalData source prefix is incomplete");
    // Until activation transfers these References, this host can export only
    // the state actually produced by the completed LOAD prefix. A validated
    // altered projection is not proof that its character setters ran.
    const auto &initial = owner_.prefix_state();
    upstream::FieldInventoryRuntime original, requested;
    std::vector<uint8_t> initial_bytes, requested_bytes;
    if (!original.initialize(*inventory_, *definitions_, initial, error) ||
        !requested.initialize(*inventory_, *definitions_, state, error) ||
        !original.encode_save(initial_bytes, error) ||
        !requested.encode_save(requested_bytes, error))
      return false;
    if (initial_bytes != requested_bytes || initial.revision != state.revision)
      return fail(
          error,
          "globalData cold snapshot differs from actual source LOAD state");
    PodunkInventorySnapshot next;
    next.source_pin = inventory_->source_pin();
    next.inventory_identity = inventory_->identity();
    next.state = state;
    for (const auto &source : inventory_->owners()) {
      const auto &rows = owner_.data()->declarations();
      auto definition =
          std::find_if(rows.begin(), rows.end(), [&](const auto &r) {
            return r.name == source.source_name;
          });
      if (definition == rows.end())
        return fail(error, "globalData source owner missing");
      const auto &objects = owner_.objects();
      auto object =
          std::find_if(objects.begin(), objects.end(), [&](const auto &r) {
            return r.declaration == definition->id &&
                   r.kind == definition->kind;
          });
      if (object == objects.end())
        return fail(error, "globalData actual source owner missing");
      next.owners.push_back(
          {source.id, source.role ? object->object : object->inventory});
    }
    if (!owners(*inventory_, next.owners, state, error))
      return false;
    std::set<uint32_t> uids;
    for (const auto &row : state.items.inventories)
      for (const auto &value : row.items) {
        std::shared_ptr<PodunkItemObject> found;
        for (const auto &item : items_) {
          auto actual = item.second.lock();
          if (actual && actual->value.uid == value.uid) {
            found = actual;
            break;
          }
        }
        if (!uids.insert(value.uid).second ||
            !PodunkInventoryHost::check_source_item(found, *registry_) ||
            found->owner != row.owner ||
            found->value.definition != value.definition ||
            found->value.doses != value.doses ||
            found->value.equipped != value.equipped)
          return fail(
              error,
              "globalData snapshot lacks the actual source Item Reference");
        next.items.push_back(std::move(found));
      }
    out = std::move(next);
    error.clear();
    return true;
  }
  const auto &runtime() const { return owner_; }
  bool bind_live_inventory(const PodunkInventoryHost &inventory,
                           std::string &error) {
    if (live_inventory_ || !registry_ || !inventory_ || !definitions_ ||
        !owner_.prefix_complete())
      return fail(error,
                  "globalData actual inventory owner missing/already bound");
    PodunkInventorySnapshot actual;
    if (!inventory.snapshot(actual, error) ||
        !owners(*inventory_, actual.owners, actual.state, error))
      return false;
    live_inventory_ = &inventory;
    error.clear();
    return true;
  }
  // Read source Character/Inventory fields from their SAME actual owning
  // state after Goods/grants/shop commits. No post-action clone is installed
  // as a second gameplay inventory; the current source Item References are
  // obtained from the live host's concrete private-provenance checked table.
  bool read_object(upstream::FieldObjectId object,
                   upstream::FieldGlobalDataObject &out,
                   std::string &error) const {
    upstream::FieldGlobalDataObject current;
    if (!owner_.read_constructed_object(object, current, error))
      return false;
    if (!owner_.prefix_complete() || (item_cache_.definitions() && current.declaration==item_cache_.definitions()->god_storage_id() && owner_.god_storage_complete())) {
      // stat_changed is synchronous. HP/MAXHP etc. have already been written;
      // nickname/skills/affinities still have their real constructor defaults
      // until execution reaches their later assignments. Do not substitute the
      // final FieldInventoryState's future values into this source observation.
      out = std::move(current);
      error.clear();
      return true;
    }
    PodunkInventorySnapshot actual;
    if (live_inventory_) {
      if (!live_inventory_->snapshot(actual, error))
        return false;
    } else if (!make_snapshot(owner_.prefix_state(), actual, error))
      return false;
    if (!owners(*inventory_, actual.owners, actual.state, error))
      return false;
    auto next = std::move(current);
    if (next.kind == 2) {
      next.item_objects.clear();
      const auto *stable = inventory_->role(next.role);
      for (const auto &item : actual.items)
        if (item->owner == stable->id)
          next.item_objects.push_back(item->object);
    } else if (next.declaration == owner_.data()->first_character()) {
      const auto &slots = owner_.data()->character_policy().slots;
      for (auto &field : next.fields) {
        if (field.name == slots[3] && field.integer_value != actual.state.level)
          return fail(error, "AwaitingCharacterFields: actual source EXP/level "
                             "owner update required");
        if (field.name == slots[4])
          field.integer_value = actual.state.hp;
        if (field.name == slots[5])
          field.integer_value = actual.state.pp;
      }
      // New-game status list is empty. Nonempty Status Reference ownership
      // requires its separate actual constructor and remains fail closed.
      if (!actual.state.statuses.empty())
        return fail(
            error,
            "AwaitingCharacterFields: actual Status Reference owner required");
      next.permanent = actual.state.permanent;
    }
    out = std::move(next);
    error.clear();
    return true;
  }
};
} // namespace encore::ctr
