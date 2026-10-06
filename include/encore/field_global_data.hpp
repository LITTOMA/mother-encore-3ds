#pragma once
#include "encore/field_global_registry.hpp"
#include "encore/field_inventory.hpp"
#include "encore/global_item_cache.hpp"
namespace encore::upstream {
struct FieldGlobalDataDefault {
  std::string name, string_value;
  uint32_t kind = 0;
  int64_t integer_value = 0;
};
struct FieldGlobalDataDeclaration {
  uint32_t id = 0, kind = 0, role = 0;
  std::string name, native, script;
  std::vector<FieldGlobalDataDefault> defaults;
};
struct FieldGlobalDataPending {
  std::string cursor, source, argument;
};
struct FieldGlobalDataCharacterPolicy {
  std::string name, nickname;
  int64_t exp = 0;
  std::vector<std::string> skills, skills_order, slots, stat_slots;
  std::vector<int64_t> exp_levels;
  std::map<std::string, double> affinities;
};
class FieldGlobalDataData {
public:
  bool load(const uint8_t *, size_t, const FieldIdentity &, std::string &);
  bool load_file(const char *, const FieldIdentity &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &inventory_sha256() const { return inventory_; }
  const auto &owner_source() const { return owner_; }
  const auto &declarations() const { return declarations_; }
  const auto &inventory_owners() const { return owners_; }
  const auto &load_roles() const { return roles_; }
  const auto &normal_inventory_defaults() const { return normal_; }
  const auto &character_policy() const { return character_; }
  const auto &pending() const { return pending_; }
  uint32_t first_character() const { return first_; }
  uint32_t next_character() const { return next_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;
  bool bind_inventory(const FieldInventoryData &, const FieldItemDefinitions &,
                      std::string &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, inventory_{};
  std::string owner_;
  uint32_t first_ = 0, next_ = 0;
  std::vector<FieldGlobalDataDeclaration> declarations_;
  std::vector<FieldGlobalDataDefault> normal_;
  FieldGlobalDataCharacterPolicy character_;
  std::vector<FieldInventoryOwner> owners_;
  std::array<uint32_t, 3> roles_{};
  std::vector<FieldGlobalDataPending> pending_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
// Source Object identities are process handles. Saved owner/definition IDs are
// resource identities and never double as ObjectDB handles or Item UIDs.
struct FieldGlobalDataObject {
  FieldObjectId object = 0, inventory = 0;
  uint32_t declaration = 0, kind = 0, role = 0;
  // Actual value storage of the admitted native/script declarations. Empty
  // Array/Dictionary defaults are owned separately for every constructed
  // Object; these are not references to an IR descriptor's containers.
  std::vector<FieldGlobalDataDefault> fields;
  std::vector<FieldObjectId> item_objects;
  std::vector<std::string> learned_skills;
  std::map<std::string, double> affinities;
  std::array<int64_t, 7> permanent{};
};
struct FieldGlobalDataItemReference {
  FieldGlobalRegistry *registry = nullptr;
  FieldObjectId object = 0;
  uint32_t owner = 0;
  FieldOwnedItem value;
  std::shared_ptr<void> actual_owner;
};
// The platform factory must be the real Item Reference constructor, not a
// source-admission callback. The provided platform adapter validates its
// private allocator provenance again before every snapshot/activation.
using FieldGlobalDataItemFactory =
    std::function<bool(const FieldInventoryData &, const FieldItemDefinitions &,
                       FieldGlobalRegistry &, uint32_t, const FieldOwnedItem &,
                       FieldGlobalDataItemReference &, std::string &)>;
using FieldGlobalDataStatSignal = std::function<bool(
    FieldObjectId, const std::string &, int64_t, int64_t, std::string &)>;
struct FieldGlobalDataGodItemFactory {
  std::function<bool(const FieldItemDefinitions&,FieldGlobalRegistry&,uint32_t,
                     FieldGlobalDataItemReference&,std::string&)>reserve;
  std::function<bool(const FieldItemDefinitions&,const FieldOwnedItem&,
                     GlobalItemCache&,FieldGlobalDataItemReference&,std::string&)>initialize;
};
class FieldGlobalDataRuntime {
public:
  FieldGlobalDataRuntime() = default;
  FieldGlobalDataRuntime(const FieldGlobalDataRuntime &) = delete;
  FieldGlobalDataRuntime &operator=(const FieldGlobalDataRuntime &) = delete;
  ~FieldGlobalDataRuntime();
  bool construct_members(const FieldGlobalDataData &,
                         const FieldGlobalExternalSpec &, FieldObjectId,
                         FieldGlobalRegistry &, std::string &);
  // Call at global.LOAD's real cursor, only after its preceding constructor,
  // overrides and scalar assignment were actually executed by their owners.
  // This fixed source prefix is save_new_game + save_overrides, not
  // global._ready's earlier save_default preload or arbitrary saved state.
  // It stops BEFORE the next character and never reports full LOAD/Ready.
  bool load_inventory_prefix(const FieldInventoryData &,
                             const FieldItemDefinitions &,
                             const FieldInventoryState *saved, SourceRandom &,
                             std::vector<uint32_t> &, LoadRngClockProvider,
                             FieldGlobalDataItemFactory,
                             FieldGlobalDataStatSignal, std::string &);
  bool check_owners(const FieldInventoryData &,
                    const std::vector<std::pair<uint32_t, FieldObjectId>> &,
                    std::string &) const;
  bool object_exists(FieldObjectId) const;
  // Source setters emit synchronously after writing actual fields. Subscribers
  // may read those fields during LOAD, before its inventory projection commits.
  // This exposes the owned source object as it exists at the current cursor;
  // it does not manufacture a future complete snapshot or lifecycle receipt.
  bool read_constructed_object(FieldObjectId, FieldGlobalDataObject &,
                               std::string &) const;
  // Call only at globaldata._ready's Inventory.new(STORAGE_GOD) cursor after
  // the actual Items-cache insertion owner has finished. The other source
  // caches/Ready owners remain independently required, not auto-admitted here.
  bool construct_god_storage(GlobalItemCache &,const std::string &locale,
                             SourceRandom &,std::vector<uint32_t>&,
                             LoadRngClockProvider,FieldGlobalDataGodItemFactory,
                             std::string &);
  bool god_storage_complete()const{return god_storage_complete_;}
  const auto&god_storage_items()const{return god_items_;}
  bool read_reference_member(FieldObjectId actual_globaldata,
                             const std::string&source_member,
                             FieldObjectId&out,std::string&)const;
  // After actual Inventory/global.item owners accept the same References,
  // this construction cursor releases temporary Ref ownership, as source
  // inv_content does. It keeps only allocation trace, never immortal Items.
  void release_item_references() {
    for (auto &item : items_)
      item.actual_owner.reset();
  }
  const auto &objects() const { return objects_; }
  const auto &items() const { return items_; }
  const auto &prefix_state() const { return state_; }
  const auto *data() const { return data_; }
  FieldObjectId globaldata_object() const { return owner_; }
  bool prefix_complete() const { return prefix_; }
  bool constructor_complete() const { return false; }
  bool ready_complete() const { return false; }
  bool load_complete() const { return false; }

private:
  const FieldGlobalDataData *data_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  std::array<uint8_t, 32> admitted_ir_{};
  FieldObjectId owner_ = 0;
  bool prefix_ = false, poisoned_ = false;
  std::vector<FieldGlobalDataObject> objects_;
  std::vector<FieldGlobalDataItemReference> items_;
  FieldInventoryState state_;
  bool god_storage_complete_=false;
  FieldObjectId god_storage_object_=0;
  std::string god_storage_member_;
  std::vector<FieldGlobalDataItemReference>god_items_;
};
} // namespace encore::upstream
