#pragma once
#include "encore/field_ui_manager.hpp"
namespace encore::upstream {
struct FieldUiPreloadRecipe {
  FieldUiPreload preload;
  bool borrowed = false;
  uint32_t scene_id = 0, node_count = 0;
  std::array<uint8_t, 32> ir_sha{};
  std::shared_ptr<const FieldNodeRecipeData> recipe;
  // Immutable checked TLV for every original native
  // property/resource/SceneState. On-demand native adapters inspect this source
  // graph; no JSON or VM at runtime.
  std::vector<uint8_t> native_graph;
};
class FieldUiPreloadsData {
public:
  bool load(const uint8_t *, size_t, const FieldIdentity &, std::string &);
  bool load_file(const char *, const FieldIdentity &, std::string &);
  bool valid() const { return valid_; }
  FieldIdentity identity() const { return identity_; }
  const std::string &source_script() const { return script_; }
  const std::vector<FieldUiPreloadRecipe> &entries() const { return entries_; }
  const FieldUiPreloadRecipe *entry(uint32_t) const;
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::string script_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  std::vector<FieldUiPreloadRecipe> entries_;
};
// Actual shared source owners, including the existing Dialogue47 owner.
// UiManager's Registry PackedScene object uses these same checked owners.
class FieldUiPreloadRecipes {
public:
  bool initialize(std::shared_ptr<const FieldUiPreloadsData>,
                  const FieldUiManagerData &,
                  std::shared_ptr<const FieldNodeRecipeData> actual_dialogue,
                  std::string &);
  bool load_recipe(const FieldUiPreload &,
                   std::shared_ptr<const FieldNodeRecipeData> &,
                   std::string &) const;
  const FieldUiPreloadRecipe *entry(uint32_t) const;
  bool complete() const {
    return data_ && recipes_.size() == data_->entries().size();
  }

private:
  std::shared_ptr<const FieldUiPreloadsData> data_;
  std::map<uint32_t, std::shared_ptr<const FieldNodeRecipeData>> recipes_;
};
} // namespace encore::upstream
