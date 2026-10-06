#pragma once
#include "encore/native_input.hpp"
#include "encore/player_initialization.hpp"
namespace encore::upstream {
enum class GlobalReadyStep : uint32_t {
  LocalizedInputs = 1,
  Settings,
  PartySpace,
  Transition,
  Player,
  ColdDefault
};
struct GlobalReadyPolicy {
  uint32_t party_space = 0;
  std::string settings_source_path, settings_native_path,
      locale_preference_path, slot_preference_path;
  std::string source_language_default, source_save_slot_member;
  std::string input_path;
  std::array<uint8_t, 32> input_sha{};
  uint32_t input_bytes = 0, input_crc = 0;
  std::vector<std::string> languages;
  std::vector<GlobalReadyStep> steps;
  std::vector<std::string> source_steps;
};
class GlobalReadyData {
public:
  bool load(const uint8_t *, size_t, const FieldGlobalConstructorData &,
            const PlayerInitializationData &, const GlobalLoadData &,
            std::string &);
  bool load_file(const char *, const FieldGlobalConstructorData &,
                 const PlayerInitializationData &, const GlobalLoadData &,
                 std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &constructor_ir_sha256() const { return constructor_; }
  const auto &player_ir_sha256() const { return player_; }
  const auto &load_ir_sha256() const { return load_; }
  const auto &policy() const { return policy_; }
  const auto &settings_file_spec() const { return settings_file_; }

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, constructor_{}, player_{}, load_{};
  GlobalReadyPolicy policy_;
  FieldGlobalExternalSpec settings_file_;
};
} // namespace encore::upstream
