#pragma once
#include "encore/field_character_load.hpp"
#include "encore/global_data_constructor.hpp"
#include "encore/global_yaml_file.hpp"
namespace encore::upstream {
struct GlobalLoadDocument {
  uint32_t kind = 0;
  std::string caller_source, caller_method, getter_source, getter_method;
  std::string parser_source, parser_method;
  GlobalYamlFileRecord file;
};
struct GlobalLoadAssignment {
  // 1 Variant assignment, 2 Vector2 construction; coercion 1 source int().
  uint32_t kind = 0, coercion = 0, fallback_kind = 0, constant_index = 0;
  std::string member, key, key_y, fallback_member, constant;
  std::shared_ptr<const GlobalYamlValue> fallback;
};
struct GlobalLoadInventory {
  uint32_t id = 0, role = 0;
  std::string member, source_key, method;
  std::vector<FieldCharacterSavedItem> items;
};
struct GlobalLoadCharacter {
  uint32_t id = 0, role = 0;
  std::string name;
  std::shared_ptr<const GlobalYamlValue> saved;
};
struct GlobalLoadPartyReference {
  uint32_t id = 0;
  bool playable = false;
  std::string name;
};
struct GlobalLoadStep {
  // 1 override; 2 scalar; 3 inventory; 4 characters; 5/6 party clears;
  // 7 party loop; 8 UI flavor; 9 flags loop; 10 goto_game boundary (false).
  uint32_t kind = 0, index = 0;
  std::string owner, method, member;
};
struct GlobalLoadPrecondition {
  uint32_t kind = 0;
  std::string source, method, member;
};
class GlobalLoadData {
public:
  bool load(const uint8_t *, size_t, const GlobalDataConstructorData &,
            const FieldCharacterLoadData &, const FieldGlobalFlagsData &,
            const FieldItemDefinitions &, std::string &);
  bool load_file(const char *, const GlobalDataConstructorData &,
                 const FieldCharacterLoadData &, const FieldGlobalFlagsData &,
                 const FieldItemDefinitions &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &constructor_ir_sha256() const { return constructor_; }
  const auto &characters_ir_sha256() const { return character_; }
  const auto &flags_ir_sha256() const { return flag_; }
  const auto &file_ir_sha256() const { return file_; }
  const auto &documents() const { return documents_; }
  const auto &owner_source() const { return owner_; }
  const auto &cold_source() const { return cold_source_; }
  const auto &new_game_source() const { return new_source_; }
  const auto &overrides_source() const { return override_source_; }
  const auto &cold_original() const { return cold_; }
  const auto &new_game_original() const { return new_; }
  const auto &overrides() const { return overrides_; }
  // Reviewed merge witness, not proof that the runtime performed mutation.
  const auto &cold_merged() const { return merged_; }
  const auto &assignments() const { return assignments_; }
  const auto &inventories() const { return inventories_; }
  const auto &saved_item_keys() const { return saved_item_keys_; }
  uint32_t saved_item_default_doses() const { return saved_doses_; }
  bool saved_item_default_equipped() const { return saved_equipped_; }
  const auto &characters() const { return characters_; }
  const auto &party() const { return party_; }
  const auto &playable_names() const { return playable_; }
  const auto &party_source_key() const { return party_key_; }
  const auto &flags() const { return flags_; }
  const auto &steps() const { return steps_; }
  const auto &preconditions() const { return preconditions_; }
  const auto &normal_flags_member() const { return normal_flags_member_; }
  const auto &normal_flags_source_key() const { return normal_flags_key_; }
  const auto &menu_flavor_member() const { return menu_member_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;
  bool goto_game_admitted() const { return false; }
  bool new_game_admitted() const { return false; }

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, constructor_{}, character_{}, flag_{}, file_{};
  std::string owner_, cold_source_, new_source_, override_source_;
  std::string normal_flags_member_, normal_flags_key_, menu_member_, party_key_;
  std::shared_ptr<const GlobalYamlValue> cold_, new_, overrides_, merged_;
  std::vector<GlobalLoadAssignment> assignments_;
  std::vector<GlobalLoadInventory> inventories_;
  std::vector<std::string> saved_item_keys_;
  uint32_t saved_doses_ = 0;
  bool saved_equipped_ = false;
  std::vector<GlobalLoadCharacter> characters_;
  std::vector<GlobalLoadPartyReference> party_;
  std::vector<std::string> playable_;
  FieldFlagDictionary flags_;
  std::vector<GlobalLoadStep> steps_;
  std::vector<GlobalLoadPrecondition> preconditions_;
  std::vector<GlobalLoadDocument> documents_;
  std::map<std::string, std::array<uint8_t, 32>> sources_, definitions_;
};
} // namespace encore::upstream
