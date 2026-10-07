#pragma once
#include "encore/field_global_registry.hpp"
#include "encore/field_scene_audio.hpp"
#include "encore/global_yaml_caches.hpp"
namespace encore::upstream {
struct PlayerNamedSfxStream {
  FieldSceneAudioStream audio;
  std::array<uint8_t, 32> import_sha{}, payload_sha{};
  std::shared_ptr<const GlobalYamlValue> metadata;
  std::vector<uint8_t> payload;
};
class PlayerNamedSfxData {
public:
  bool load(const uint8_t *, size_t, const FieldGlobalRegistryData &,
            std::string &);
  bool load_file(const char *, const FieldGlobalRegistryData &, std::string &);
  bool valid() const { return valid_; }
  const FieldNodeRecipeData &recipe() const { return recipe_; }
  FieldIdentity identity() const { return recipe_.identity(); }
  const auto &ir_sha256() const { return ir_; }
  const auto &bank_sha256() const { return bank_sha_; }
  const auto &bank_path() const { return bank_; }
  const auto &streams() const { return streams_; }
  const PlayerNamedSfxStream *stream(uint32_t) const;
  const PlayerNamedSfxStream *stream(std::string_view) const;
  const auto &prototype() const { return prototype_; }
  const FieldNodeDescriptor &voice_descriptor() const { return voice_; }
  const auto &constructor() const { return constructor_; }
  const auto &effects() const { return effects_; }
  const auto &native_source() const { return native_; }
  const auto &methods() const { return methods_; }
  const auto &bus() const { return bus_; }
  const auto &music_bus() const { return music_bus_; }
  FieldIdentity voice_identity() const {
    auto v = identity();
    v.scene_id = voice_.id;
    return v;
  }

private:
  bool valid_ = false;
  FieldNodeRecipeData recipe_;
  FieldSceneAudioNode prototype_;
  FieldNodeDescriptor voice_;
  std::array<uint8_t, 32> ir_{}, bank_sha_{};
  std::string bus_, bank_, music_bus_;
  std::vector<PlayerNamedSfxStream> streams_;
  std::shared_ptr<const GlobalYamlValue> native_, constructor_, effects_,
      methods_;
};
} // namespace encore::upstream
