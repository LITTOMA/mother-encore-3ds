#pragma once
#include "encore/field_native_timer.hpp"
#include "encore/field_node_recipe.hpp"
#include "encore/global_load_runtime.hpp"
namespace encore::upstream {
enum class FieldGlobalMemberRole : uint32_t {
  Party = 1,
  PartyNpcs,
  PartySpace,
  PartyObjects,
  Persistent,
  CurrentScene,
  CurrentCamera,
  SceneTransition,
  Item,
  Talker,
  InCutscene,
  CanPause,
  EnteringDoor,
  PhoneLocation
};
enum class FieldGlobalLiteralKind : uint32_t {
  Null = 0,
  Boolean = 1,
  Integer = 2,
  Number = 3,
  String = 4,
  StringArray = 5,
  Vector = 7,
  SourceNode = 8
};
struct FieldGlobalConstructorField {
  uint32_t role = 0;
  std::string name, type_hint, string_value;
  FieldGlobalLiteralKind kind{};
  bool boolean = false;
  int64_t integer = 0;
  double number = 0;
  std::array<double, 2> vector{};
  std::vector<std::string> strings;
};
struct FieldGlobalChildFields {
  uint32_t id = 0;
  std::string script;
  std::vector<FieldGlobalConstructorField> fields;
};
// Constructor-only source capability. No field or method approves Ready.
class FieldGlobalConstructorData {
public:
  bool load(const uint8_t *, size_t, const FieldIdentity &, std::string &);
  bool load_file(const char *, const FieldIdentity &, std::string &);
  bool valid() const { return valid_; }
  const FieldIdentity &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &owner_source() const { return owner_; }
  const auto &scene_source() const { return scene_; }
  const auto &fields() const { return fields_; }
  const auto &constants() const { return constants_; }
  const auto &signals() const { return signals_; }
  const auto &child_fields() const { return children_; }
  const auto &ready_steps() const { return ready_; }
  const FieldNodeRecipeData &recipe() const { return recipe_; }
  const FieldNativeTimerData &timer_data() const { return timers_; }
  const FieldNodeDescriptor &transition_node() const { return transition_; }
  FieldIdentity transition_identity() const;
  const FieldGlobalConstructorField *member(FieldGlobalMemberRole) const;
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;
  bool bind_registry(const FieldGlobalExternalSpec &, std::string &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{};
  std::string owner_, scene_;
  std::vector<FieldGlobalConstructorField> fields_, constants_;
  std::vector<FieldGlobalChildFields> children_;
  std::vector<std::string> signals_, ready_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  FieldNodeRecipeData recipe_;
  FieldNativeTimerData timers_;
  FieldNodeDescriptor transition_;
};
// PartySpace stores Variant nil/Vector2 entries, never ObjectIDs. Its root
// remains shared across resize/push_front/pop_back, as the source Array does.
struct FieldGlobalPartySpaceValue {
  bool is_vector = false;
  Vec2 vector{};
};
struct FieldGlobalPartySpaceArray {
  std::vector<FieldGlobalPartySpaceValue> values;
};
// Owns the actual source Array roots. Saved IDs, Item UIDs and ObjectIDs are
// separate domains; append never allocates or replaces an Item/Character.
class FieldGlobalConstructorRuntime {
public:
  bool initialize(const FieldGlobalConstructorData &, FieldGlobalRegistry &,
                  FieldObjectId owner, FieldObjectId transition, std::string &);
  bool array(FieldGlobalMemberRole,
             std::shared_ptr<const GlobalLoadObjectArray> &,
             std::string &) const;
  bool clear_array(FieldGlobalMemberRole, std::string &);
  bool append_array(FieldGlobalMemberRole, FieldObjectId, std::string &);
  // Array.erase removes the first occurrence while retaining the same root.
  // A missing value is a source no-op, including an already freed ObjectID.
  bool erase_array_first(FieldGlobalMemberRole, FieldObjectId, std::string &);
  bool assign_array(FieldGlobalMemberRole, std::vector<FieldObjectId>,
                    std::string &);
  bool party_space(std::shared_ptr<const FieldGlobalPartySpaceArray> &,
                   std::string &) const;
  bool resize_party_space(size_t, std::string &);
  bool push_front_party_space(Vec2, std::string &);
  bool pop_back_party_space(FieldGlobalPartySpaceValue &, std::string &);
  bool object(FieldGlobalMemberRole, FieldObjectId &, std::string &) const;
  bool set_object(FieldGlobalMemberRole, FieldObjectId, std::string &);
  bool boolean(FieldGlobalMemberRole, bool &, std::string &) const;
  bool set_boolean(FieldGlobalMemberRole, bool, std::string &);
  bool string(FieldGlobalMemberRole, std::string &, std::string &) const;
  bool set_string(FieldGlobalMemberRole, std::string, std::string &);
  bool bind_characters(const FieldGlobalDataRuntime &, std::string &);
  bool party_array(std::string_view,
                   std::shared_ptr<const GlobalLoadObjectArray> &,
                   std::string &) const;
  bool clear_party(std::string_view, std::string &);
  bool append_party(std::string_view, FieldObjectId, std::string &);
  FieldObjectId owner() const { return owner_; }
  const FieldGlobalRegistry *registry()const{return registry_;}
  const FieldGlobalConstructorData *data() const { return data_; }

private:
  bool live(std::string &) const;
  bool valid_object(FieldGlobalMemberRole, FieldObjectId, std::string &) const;
  bool party_role(std::string_view, FieldGlobalMemberRole &,
                  std::string &) const;
  const FieldGlobalConstructorData *data_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  const FieldGlobalDataRuntime *characters_ = nullptr;
  FieldObjectId owner_ = 0;
  std::map<FieldGlobalMemberRole, std::shared_ptr<GlobalLoadObjectArray>>
      arrays_;
  std::shared_ptr<FieldGlobalPartySpaceArray> party_space_;
  std::map<FieldGlobalMemberRole, FieldObjectId> objects_;
  std::map<FieldGlobalMemberRole, bool> booleans_;
  std::map<FieldGlobalMemberRole, std::string> strings_;
};
} // namespace encore::upstream
