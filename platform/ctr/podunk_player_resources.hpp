#pragma once
#include "encore/player_resources.hpp"
#include "loading_texture.hpp"
#include "podunk_player_visual_native.hpp"
namespace encore::ctr {
class PodunkPlayerResources final : public PodunkPlayerVisualTextures {
public:
  struct Image;
  struct State;
  PodunkPlayerResources() = default;
  PodunkPlayerResources(const PodunkPlayerResources &) = delete;
  PodunkPlayerResources &operator=(const PodunkPlayerResources &) = delete;
  // Caller retains checked data and Registry until shutdown. Actual GPU assets
  // share the existing loading cache; local material clones retain player IDs.
  bool load(const upstream::PlayerResourcesData &, const char *,
            upstream::FieldGlobalRegistry &, std::string &);
  bool construct_resource(uint32_t source_id,
                          upstream::FieldObjectId actual_player,
                          upstream::FieldObjectId &, std::string &);
  bool construct_audio(uint32_t source_id, upstream::FieldObjectId &,
                       std::string &);
  bool resolve(const upstream::PlayerInitializationField &,
               upstream::FieldObjectId &, std::string &);
  bool load_texture(std::string_view, upstream::FieldObjectId &, std::string &);
  bool resource_exists(std::string_view, bool &, std::string &) const;
  bool image(std::string_view, const std::array<uint8_t, 32> &, C2D_Image &,
             std::string &) const override;
  bool texture(upstream::FieldObjectId, C2D_Image &, std::string &) const;
  bool height(upstream::FieldObjectId, uint32_t &, std::string &) const;
  bool source_path(upstream::FieldObjectId, std::string &, std::string &) const;
  bool set_shader_parameter(upstream::FieldObjectId, std::string_view, uint32_t,
                            const std::array<float, 4> &, std::string &);
  bool shader_parameters(upstream::FieldObjectId,
                         std::vector<upstream::PlayerResourceParameter> &,
                         std::string &) const;
  bool neutral_material(upstream::FieldObjectId, std::string &) const;
  bool pcm(upstream::FieldObjectId, const std::vector<uint8_t> *&,
           upstream::PlayerResourceAudio &, std::string &) const;
  bool release_player(upstream::FieldObjectId, std::string &);
  bool shutdown(std::string &);
  const upstream::FieldGlobalRegistry *registry() const { return registry_; }

private:
  bool live(std::string &) const;
  const State *owned(upstream::FieldObjectId, std::string &) const;
  bool publish(std::shared_ptr<State>, const char *, std::string &,
               upstream::FieldObjectId &, std::string &);
  const upstream::PlayerResourcesData *data_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  std::array<uint8_t, 32> ir_{};
  std::map<uint32_t, std::shared_ptr<Image>> images_;
  std::map<uint32_t, std::shared_ptr<const std::vector<uint8_t>>> audios_;
  std::map<std::pair<uint32_t, upstream::FieldObjectId>, std::shared_ptr<State>>
      resources_;
  std::map<uint32_t, std::shared_ptr<State>> audio_resources_;
};
} // namespace encore::ctr
