#pragma once
#include "encore/field_global_registry.hpp"
#include "encore/field_inventory.hpp"
#include "encore/global_item_cache.hpp"
namespace encore::upstream {
class GlobalDataConstructorData;
class GlobalYamlCachesData;
class GlobalYamlCachesRuntime;
class GlobalPackedDirectoryHost;
class FieldGlobalFlagsData;
class FieldGlobalFlagsRuntime;
struct GlobalYamlValue;
struct FieldGlobalDataReferenceArray {
  // The source Array owns its References. Borrowers keep this exact Array
  // root alive across replacement/reset, rather than keeping bare ObjectIDs.
  std::vector<std::shared_ptr<const FieldGlobalNativeReference>> values;
};
class FieldCharacterLoadData;
class GlobalLoadData;
class GlobalLoadRuntime;
class HouseGlobalBridgeRuntime;
struct FieldGlobalDataNodeArray {
  std::vector<FieldObjectId> values;
};
struct FieldCharacterLoadState;
struct FieldCharacterOwnedReference;
class FieldCharacterEnemySkillReference;
struct FieldGlobalDataMemberState {
  uint32_t kind = 0, adapter = 0;
  std::shared_ptr<const GlobalYamlValue> value;
  std::array<double, 2> vector{};
  std::vector<std::pair<std::string, FieldObjectId>> references;
  std::shared_ptr<const FieldGlobalDataReferenceArray> reference_array;
  std::shared_ptr<const FieldGlobalDataNodeArray> node_array;
  uint32_t owner_role = 0;
  const FieldGlobalFlagsRuntime *flags = nullptr;
  GlobalYamlCachesRuntime *caches = nullptr;
  GlobalItemCache *items = nullptr;
};
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
  std::map<std::string, std::shared_ptr<FieldGlobalDataNodeArray>> node_arrays{};
  std::vector<std::string> learned_skills;
  std::map<std::string, double> affinities;
  std::array<int64_t, 7> permanent{};
  std::map<std::string, std::shared_ptr<GlobalYamlValue>> collections{};
  std::map<std::string, std::shared_ptr<FieldGlobalDataReferenceArray>>
      reference_arrays{};
};
class FieldGlobalDataItemSourceReference : public FieldGlobalNativeReference {
public:
  virtual bool read_item(FieldOwnedItem &, std::string &) const = 0;
};
struct FieldGlobalDataItemReference {
  FieldGlobalRegistry *registry = nullptr;
  FieldObjectId object = 0;
  uint32_t owner = 0;
  FieldOwnedItem value;
  std::shared_ptr<void> actual_owner;
  std::shared_ptr<const FieldGlobalDataItemSourceReference> source_owner{};
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
  std::function<bool(const FieldItemDefinitions &, FieldGlobalRegistry &,
                     uint32_t, FieldGlobalDataItemReference &, std::string &)>
      reserve;
  std::function<bool(const FieldItemDefinitions &, const FieldOwnedItem &,
                     GlobalItemCache &, FieldGlobalDataItemReference &,
                     std::string &)>
      initialize;
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
  bool construct_god_storage(GlobalItemCache &, const std::string &locale,
                             SourceRandom &, std::vector<uint32_t> &,
                             LoadRngClockProvider,
                             FieldGlobalDataGodItemFactory, std::string &);
  bool god_storage_complete() const { return god_storage_complete_; }
  const auto &god_storage_items() const { return god_items_; }
  bool read_reference_member(FieldObjectId actual_globaldata,
                             const std::string &source_member,
                             FieldObjectId &out, std::string &) const;
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
  const FieldGlobalRegistry *registry() const { return registry_; }
  FieldObjectId globaldata_object() const { return owner_; }
  bool prefix_complete() const { return prefix_; }
  bool constructor_complete() const;
  bool ready_complete() const;
  bool load_complete() const;
  bool initialize_global_load(const GlobalLoadData &, std::string &);
  bool global_load_bound_to(const GlobalLoadData &) const;
  bool assign_global_load_member(std::string_view,
                                 const std::shared_ptr<GlobalYamlValue> &,
                                 const std::array<double, 2> *, std::string &);
  bool global_load_inventory(size_t, FieldObjectId &, std::string &) const;
  bool load_inventory_owner(FieldObjectId, uint32_t &, std::string &) const;
  bool publish_global_load_inventory(
      size_t, const std::vector<FieldGlobalDataItemReference> &, std::string &);
  bool global_load_normal_flag(std::string_view, bool, std::string &);
  bool initialize_constructor(const GlobalDataConstructorData &, std::string &);
  bool complete_constructor(const FieldGlobalFlagsData &,
                            FieldGlobalFlagsRuntime &,
                            const GlobalYamlCachesData &,
                            GlobalYamlCachesRuntime &,
                            const GlobalPackedDirectoryHost &,
                            GlobalItemCache &, std::string &);
  bool source_stage_parent(const FieldGlobalExternalBinding &, FieldObjectId,
                           std::string &);
  bool source_enter(const FieldGlobalExternalBinding &, FieldObjectId,
                    std::string &);
  bool source_begin_ready(const FieldGlobalExternalBinding &, FieldObjectId,
                          std::string &);
  bool source_finish_ready(const FieldGlobalExternalBinding &, FieldObjectId,
                           std::string &);
  bool source_exit(const FieldGlobalExternalBinding &, FieldObjectId,
                   std::string &);
  bool source_state(FieldGlobalExternalState &, std::string &) const;
  bool read_global_member(std::string_view, FieldGlobalDataMemberState &,
                          std::string &) const;
  bool read_constructed_member(FieldObjectId, std::string_view,
                               FieldGlobalDataMemberState &,
                               std::string &) const;
  bool write_global_scalar(std::string_view, const GlobalYamlValue &,
                           std::string &);
  bool menu_flavor(std::string &, std::string &) const;
  bool constructed_body_alive(FieldObjectId) const;
  bool constructor_source_hash(std::string_view,
                               std::array<uint8_t, 32> &) const;
  bool initialize_character_load(const FieldCharacterLoadData &, std::string &);
  bool character_load_bound_to(const FieldCharacterLoadData &) const;
  bool character_load_source_hash(std::string_view,
                                  std::array<uint8_t, 32> &) const;
  bool read_character_load(uint32_t declaration, FieldCharacterLoadState &,
                           std::string &) const;
  bool character_nickname(uint32_t declaration, std::string &,
                          std::string &) const;
  bool publish_character_load(const FieldCharacterLoadState &, std::string &);
  bool new_character_inventory(uint32_t declaration,
                               FieldCharacterOwnedReference &, std::string &);
  bool character_inventory_owner(FieldObjectId, uint32_t &,
                                 std::string &) const;
  bool
  read_character_inventory_items(FieldObjectId,
                                 std::vector<FieldGlobalDataItemReference> &,
                                 std::string &) const;

private:
  friend class GlobalLoadRuntime;
  friend class HouseGlobalBridgeRuntime;
  const GlobalLoadData *global_load_data_ = nullptr;
  std::array<uint8_t, 32> global_load_ir_{};
  bool global_load_complete_ = false;
  bool global_load_started_ = false, global_load_poisoned_ = false;
  FieldObjectId global_load_global_ = 0, global_load_ui_ = 0;
  const GlobalDataConstructorData *constructor_data_ = nullptr;
  std::array<uint8_t, 32> constructor_ir_{}, cache_ir_{}, flags_ir_{};
  FieldGlobalExternalSpec source_spec_;
  FieldObjectId source_parent_ = 0;
  bool constructor_closed_ = false, source_inside_ = false,
       source_ready_ = false;
  bool source_ready_started_ = false, source_ready_first_ = true;
  std::vector<FieldGlobalDataMemberState> members_;
  std::vector<std::shared_ptr<FieldGlobalNativeReference>>
      inventory_references_;
  const FieldGlobalFlagsData *constructor_flags_data_ = nullptr;
  FieldGlobalFlagsRuntime *constructor_flags_ = nullptr;
  const GlobalYamlCachesData *constructor_cache_data_ = nullptr;
  GlobalYamlCachesRuntime *constructor_caches_ = nullptr;
  const GlobalPackedDirectoryHost *constructor_directories_ = nullptr;
  GlobalItemCache *constructor_items_ = nullptr;
  bool constructor_available(std::string &) const;
  bool source_binding(const FieldGlobalExternalBinding &, std::string &) const;
  bool publish_inventory_body(FieldObjectId, std::string &);
  const FieldGlobalDataData *data_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  std::array<uint8_t, 32> admitted_ir_{};
  FieldObjectId owner_ = 0;
  bool prefix_ = false, poisoned_ = false;
  std::vector<FieldGlobalDataObject> objects_;
  std::vector<FieldGlobalDataItemReference> items_;
  FieldInventoryState state_;
  bool god_storage_complete_ = false;
  FieldObjectId god_storage_object_ = 0;
  std::string god_storage_member_;
  std::vector<FieldGlobalDataItemReference> god_items_;
  const FieldCharacterLoadData *character_load_data_ = nullptr;
  std::array<uint8_t, 32> character_load_ir_{};
  std::map<FieldObjectId, std::vector<FieldGlobalDataItemReference>>
      character_items_;
  std::map<FieldObjectId,
           std::shared_ptr<const FieldCharacterEnemySkillReference>>
      character_enemy_skills_;
  std::map<uint32_t, FieldObjectId> character_inventory_cursors_;
  bool character_load_available(std::string &) const;
};
} // namespace encore::upstream
