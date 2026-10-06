#pragma once
#include "encore/player_graphics.hpp"
#include "encore/player_initialization.hpp"
#include "encore/player_effects.hpp"
namespace encore::upstream {
struct PlayerResourceImage {
  uint32_t id = 0, width = 0, height = 0, bytes = 0;
  std::string source, path;
  std::array<uint8_t, 32> source_sha{}, import_sha{}, output_sha{};
};
struct PlayerResourceAudio {
  uint32_t id = 0, kind = 0, rate = 0, channels = 0, frames = 0, bytes = 0;
  bool loop = false;
  std::string source, path;
  std::array<uint8_t, 32> source_sha{}, import_sha{}, output_sha{};
};
struct PlayerResourceParameter {
  uint32_t role = 0, kind = 0;
  std::string name;
  std::array<float, 4> value{};
};
struct PlayerResource {
  uint32_t id = 0, kind = 0, texture = 0, shader = 0;
  bool local = false, instanced = false;
  std::string source;
  std::array<float, 4> rect{};
  std::vector<PlayerResourceParameter> parameters;
};
struct PlayerResourceNode {
  std::string path;
  uint32_t texture = 0xffffffff, material = 0xffffffff, columns = 0, rows = 0,
           frame = 0;
};
struct PlayerEffectResource { uint32_t effect=0,source_id=0,resource_id=0; };
class PlayerResourcesData {
public:
  bool load(const uint8_t *, size_t, const PlayerInitializationData &,
            const PlayerGraphicsData &, std::string &);
  bool load_file(const char *, const PlayerInitializationData &,
                 const PlayerGraphicsData &, std::string &);
  bool valid() const { return valid_; }
  uint32_t capability() const { return capability_; }
  bool bind_effects(const PlayerEffectsData&,std::string&);
  bool effects_bound() const { return effects_bound_; }
  const auto& effect_resources() const { return effect_resources_; }
  const PlayerResource* effect_resource(uint32_t effect,uint32_t source) const;
  const FieldIdentity* effect_identity(uint32_t effect) const;
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &initialization_ir_sha256() const { return init_ir_; }
  const auto &graphics_ir_sha256() const { return gfx_ir_; }
  const auto &images() const { return images_; }
  const auto &audios() const { return audios_; }
  const auto &resources() const { return resources_; }
  const auto &nodes() const { return nodes_; }
  const PlayerResourceImage *image(uint32_t) const;
  const PlayerResource *resource(uint32_t) const;
  const PlayerResourceAudio *audio(uint32_t) const;
  const PlayerResourceNode *node(std::string_view) const;
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false, effects_bound_ = false;
  uint32_t capability_ = 1;
  std::array<uint8_t,32> effects_ir_{};
  std::vector<PlayerEffectResource> effect_resources_;
  std::map<uint32_t,FieldIdentity> effect_identities_;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, init_ir_{}, gfx_ir_{};
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  std::vector<PlayerResourceImage> images_;
  std::vector<PlayerResourceAudio> audios_;
  std::vector<PlayerResource> resources_;
  std::vector<PlayerResourceNode> nodes_;
};
} // namespace encore::upstream
