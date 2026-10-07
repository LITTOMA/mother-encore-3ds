#pragma once
#include "encore/field_node_tree.hpp"
#include <map>
namespace encore::upstream {
struct FieldSceneAudioNode {
  uint32_t id = 0, kind = 0, stream = 0, mix_target = 0, area_mask = 0;
  std::string path, bus;
  float volume_db = 0, pitch = 1, max_distance = 0, attenuation = 0,
        panning = 0;
  bool autoplay = false, paused = false;
};
struct FieldSceneAudioStream {
  uint32_t id = 0, asset_id = 0, bank = 0;
  std::string source, native_class;
  std::array<uint8_t, 32> source_sha{};
};
// Pure checked original native properties. Loading this resource neither
// creates a stream Resource/ObjectID nor grants a Node lifecycle phase.
class FieldSceneAudioData {
public:
  bool load(const uint8_t *, size_t, const FieldNodeTreeData &, std::string &);
  bool load_file(const char *, const FieldNodeTreeData &, std::string &);
  bool valid() const { return valid_; }
  FieldIdentity identity() const { return identity_; }
  const std::vector<FieldSceneAudioNode> &nodes() const { return nodes_; }
  const std::vector<FieldSceneAudioStream> &streams() const { return streams_; }
  const FieldSceneAudioNode *node(uint32_t) const;
  const FieldSceneAudioStream *stream(uint32_t) const;
  const std::string &scene_bank() const { return scene_bank_; }
  const std::array<uint8_t, 32> &bank_sha() const { return bank_sha_; }
  const std::array<uint8_t, 32> &ir_sha() const { return ir_; }
  float global_panning() const { return global_panning_; }

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::vector<FieldSceneAudioNode> nodes_;
  std::vector<FieldSceneAudioStream> streams_;
  std::string scene_bank_;
  std::array<uint8_t, 32> bank_sha_{}, ir_{};
  float global_panning_ = 0;
};
} // namespace encore::upstream
