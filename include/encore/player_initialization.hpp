#pragma once
#include "encore/field_global_constructor.hpp"
#include "encore/global_data_constructor.hpp"
namespace encore::upstream {
struct PlayerInitializationField {
  std::string source, name, hint, resource, native, member, key;
  uint32_t adapter = 0, declaration = 0, kind = 0;
  std::shared_ptr<const GlobalYamlValue> value;
  std::array<double, 2> vector{};
};
struct PlayerInitializationPolicy {
  std::string current_scene, party, party_objects, player_name;
  std::string leader_member, leader_key, ready_signal, pause_method;
  std::string followers_method, respawn_method;
  uint32_t leader_declaration = 0, connect_flags = 0;
  Vec2 position{};
  std::vector<std::string> parent_candidates, respawn_fields;
  bool followers_emit = false;
};
class PlayerInitializationData {
public:
  bool load(const uint8_t *, size_t, const GlobalDataConstructorData &,
            std::string &);
  bool load_file(const char *, const GlobalDataConstructorData &,
                 std::string &);
  bool valid() const { return valid_; }
  bool ready_admitted() const { return false; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &recipe() const { return recipe_; }
  const auto &fields() const { return fields_; }
  const auto &policy() const { return policy_; }
  const auto &onready_source() const { return onready_; }
  const auto &native_source() const { return native_; }
  const auto &owner_source() const { return owner_; }
  const auto &player_source() const { return player_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{};
  std::string owner_, player_, base_;
  FieldNodeRecipeData recipe_;
  std::vector<PlayerInitializationField> fields_;
  PlayerInitializationPolicy policy_;
  std::shared_ptr<const GlobalYamlValue> onready_, native_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
struct PlayerInitializationMember {
  uint32_t kind = 0;
  std::shared_ptr<const GlobalYamlValue> value;
  std::array<double, 2> vector{};
  FieldObjectId object = 0;
};
// The actual Player script body is attached to the same native Tree node,
// before its first child is allocated. This grants no onready/Ready behavior.
class PlayerInitializationBody {
public:
  using ResourceLoader = std::function<bool(const PlayerInitializationField &,
                                            FieldObjectId &, std::string &)>;
  bool construct(const PlayerInitializationData &, FieldNodeTreeRuntime &,
                 FieldObjectId, const FieldGlobalDataRuntime &,
                 FieldGlobalRegistry &, ResourceLoader, std::string &);
  bool member(std::string_view, PlayerInitializationMember &,
              std::string &) const;
  FieldObjectId object() const { return object_; }
  bool constructed() const { return complete_ && !poisoned_; }

private:
  const PlayerInitializationData *data_ = nullptr;
  FieldNodeTreeRuntime *tree_ = nullptr;
  FieldObjectId object_ = 0;
  bool complete_ = false, poisoned_ = false;
  std::map<std::string, PlayerInitializationMember> members_;
};
struct PlayerInitializationHost {
  // Load the checked PackedScene using this same ObjectDB and source script
  // construction cursor. All unknown native/script owners must reject.
  std::function<bool(const PlayerInitializationData &,
                     std::shared_ptr<FieldNodeTreeRuntime> &, FieldObjectId &,
                     std::string &)>
      instantiate;
  // These are actual Player/global source methods, not completion receipts.
  std::function<bool(FieldObjectId, const std::string &, const std::string &,
                     uint32_t, std::string &)>
      connect_ready_pause;
  std::function<bool(const std::string &, bool, std::string &)> followers;
  std::function<bool(const PlayerInitializationPolicy &, FieldObjectId,
                     FieldObjectId, std::string &)>
      set_respawn;
};
class PlayerInitializationRuntime {
public:
  bool initialize(const PlayerInitializationData &,
                  FieldGlobalConstructorRuntime &,
                  const FieldGlobalDataRuntime &, FieldGlobalRegistry &,
                  PlayerInitializationHost, std::string &);
  bool run(std::string &);
  bool complete() const { return complete_ && !poisoned_; }
  FieldObjectId player() const { return player_; }

private:
  const PlayerInitializationData *data_ = nullptr;
  FieldGlobalConstructorRuntime *global_ = nullptr;
  const FieldGlobalDataRuntime *characters_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  PlayerInitializationHost host_;
  FieldObjectId player_ = 0;
  bool started_ = false, complete_ = false, poisoned_ = false;
};
} // namespace encore::upstream
