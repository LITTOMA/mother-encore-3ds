#pragma once
#include "encore/field_canvas_art.hpp"
namespace encore::upstream {
struct FieldMaterialUniform {
  std::string name;
  uint32_t type = 0, role = 0;
  std::vector<float> values;
};
struct FieldMaterialRecord {
  uint32_t id = 0;
  FieldCanvasShader kind = FieldCanvasShader::Default;
  bool local = false;
  std::string source, shader, name;
  std::vector<FieldMaterialUniform> uniforms;
  const FieldMaterialUniform *uniform(uint32_t role) const;
};
struct FieldMaterialBinding {
  uint32_t id = 0, material = 0, asset = 0, owner_id = 0, texture = 0;
  FieldCanvasOwner owner = FieldCanvasOwner::Native;
  std::string node;
};
struct FieldMaterialAsset {
  uint32_t id = 0, texture = 0, width = 0, height = 0, bytes = 0, crc = 0;
  std::string source, path;
  std::array<uint8_t, 32> output_sha{};
};
class FieldSceneMaterialsData {
public:
  bool load(const uint8_t *, size_t, const FieldCanvasArtData &, std::string &);
  bool load_file(const char *, const FieldCanvasArtData &, std::string &);
  bool valid() const { return valid_; }
  bool scene_admitted() const { return false; }
  FieldIdentity identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &prompt_ir_sha256() const { return prompt_ir_; }
  const auto &melody_ir_sha256() const { return melody_ir_; }
  const auto &records() const { return records_; }
  const auto &bindings() const { return bindings_; }
  const auto &assets() const { return assets_; }
  const FieldMaterialRecord *record(uint32_t) const;
  const FieldMaterialBinding *binding(uint32_t) const;
  const FieldMaterialAsset *asset(uint32_t) const;
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, prompt_ir_{}, melody_ir_{};
  std::string scene_;
  std::vector<FieldMaterialRecord> records_;
  std::vector<FieldMaterialBinding> bindings_;
  std::vector<FieldMaterialAsset> assets_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
} // namespace encore::upstream
