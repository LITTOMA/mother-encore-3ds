#pragma once
#include "encore/field_global_constructor.hpp"
#include "encore/native_session.hpp"
namespace encore::upstream {
struct HouseGlobalAssignment {
  uint32_t role = 0, index = 0, kind = 0, member_kind = 0, adapter = 0;
  std::string member, key;
};
struct HouseGlobalStatusPolicy {
  std::string id, source;
  std::array<uint8_t, 32> sha{};
  bool passive = false;
  int32_t priority = 0, probability = 0;
};
struct HouseGlobalStatusBinding {
  uint32_t id = 0, times = 0, turns = 0;
  std::string script, native, getter, ailment, turns_field, times_field,
      probability, healing_key, passive_key, probability_key;
  std::vector<HouseGlobalStatusPolicy> policies;
};
struct HouseGlobalAutoloadBinding {
  uint32_t role = 0;
  FieldGlobalAutoload source;
};
class HouseGlobalBridgeData {
public:
  bool load(const uint8_t *, size_t, const FieldCharacterLoadData &,
            const GlobalLoadData &, const NativeSessionData &, std::string &);
  bool load_file(const char *, const FieldCharacterLoadData &,
                 const GlobalLoadData &, const NativeSessionData &,
                 std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &leader() const { return leader_; }
  bool constructor_continuation() const { return constructor_continuation_; }
  const auto &continuation_autoloads() const { return continuation_autoloads_; }
  const auto &continuation_scene() const { return continuation_scene_; }
  const auto &namespace_source() const { return namespace_source_; }
  uint32_t declaration() const { return declaration_; }
  const auto &assignments() const { return assignments_; }
  const auto &status() const { return status_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;
  const FieldCharacterLoadData *characters() const { return chars_; }
  const GlobalLoadData *cold() const { return cold_; }

private:
  bool valid_ = false, constructor_continuation_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, character_ir_{}, load_ir_{}, definition_ir_{},
      session_ir_{};
  std::string leader_;
  uint32_t declaration_ = 0;
  std::vector<HouseGlobalAssignment> assignments_;
  HouseGlobalStatusBinding status_;
  std::string continuation_scene_,namespace_source_;
  std::vector<HouseGlobalAutoloadBinding> continuation_autoloads_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  const FieldCharacterLoadData *chars_ = nullptr;
  const GlobalLoadData *cold_ = nullptr;
};
struct HouseGlobalStatusObject {
  FieldObjectId object = 0;
  std::shared_ptr<FieldNodeTreeRuntime> tree;
  std::string ailment;
  int64_t turns = 0, times = 0, probability = 0;
};
struct HouseGlobalBridgeHost {
  std::function<bool(uint32_t, const FieldOwnedItem &,
                     FieldGlobalDataItemReference &, std::string &)>
      adopt_item;
  std::function<bool(const HouseGlobalStatusBinding &,
                     const HouseGlobalStatusPolicy &, const SessionStatus &,
                     HouseGlobalStatusObject &, std::string &)>
      new_status;
};
// A one-time native continuation transfer into actual constructed source owners.
// This does not execute SAVE/LOAD, complete Ready, or advance the RNG stream.
class HouseGlobalBridgeRuntime {
public:
  // Existing House session permits only checked continuation constructors.
  // Cold project order and all native/script lifecycle gates remain separate.
  bool construct_continuation_autoload(uint32_t, const HouseGlobalBridgeData &,
      const NativeSessionData &, RoomView, HouseView, RoundView, ItemView,
      const SessionSnapshot &, FieldGlobalRegistry &, SourceRandom &,
      const std::vector<uint32_t> &, std::string &);
  // Explicit native continuation capability. Executes a real NORMAL Inventory
  // constructor and binds source fields; never calls cold LOAD or grants Ready.
  bool prepare_continuation(const HouseGlobalBridgeData &,
                            FieldGlobalDataRuntime &,
                            FieldGlobalConstructorRuntime &,
                            FieldGlobalRegistry &, std::string &);
  bool adopt(const HouseGlobalBridgeData &, const NativeSessionData &, RoomView,
             HouseView, RoundView, ItemView, const SessionSnapshot &,
             const FieldInventoryData &, const FieldItemDefinitions &,
             FieldGlobalDataRuntime &, FieldGlobalConstructorRuntime &,
             FieldGlobalRegistry &, SourceRandom &,
             const std::vector<uint32_t> &, HouseGlobalBridgeHost,
             std::string &);
  bool binds_source_owners(const FieldGlobalDataRuntime &,
                          const FieldGlobalConstructorRuntime &,
                          const FieldGlobalRegistry &) const;
  bool complete() const { return complete_; }
  const FieldInventoryState &inventory_state() const { return inventory_; }
  const auto &items() const { return items_; }
  const auto &statuses() const { return statuses_; }
  double playtime_remainder() const { return playtime_remainder_; }
  bool actual_inventory_owner(uint32_t, FieldObjectId &, std::string &) const;

private:
  bool complete_ = false;
  const HouseGlobalBridgeData *prepared_data_ = nullptr;
  FieldGlobalDataRuntime *prepared_core_ = nullptr;
  FieldGlobalConstructorRuntime *prepared_global_ = nullptr;
  FieldGlobalRegistry *prepared_registry_ = nullptr;
  FieldGlobalDataRuntime *owner_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  FieldInventoryState inventory_;
  std::vector<FieldGlobalDataItemReference> items_;
  std::vector<HouseGlobalStatusObject> statuses_;
  std::map<uint32_t, FieldObjectId> owners_;
  double playtime_remainder_ = 0;
};
} // namespace encore::upstream
