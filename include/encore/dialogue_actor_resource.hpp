#pragma once
#include "encore/field_node_recipe.hpp"
#include <map>
#include <memory>
namespace encore::upstream {
// Immutable complete original PackedScene, without Actor.instance approval.
class DialogueActorResourceData {
public:
  bool load(const uint8_t *, size_t, const FieldIdentity &, std::string &);
  bool load_file(const char *, const FieldIdentity &, std::string &);
  bool valid() const { return valid_; }
  FieldIdentity identity() const { return identity_; }
  const std::string &source_scene() const { return scene_; }
  const std::string &resource_name() const { return name_; }
  const std::array<uint8_t, 32> &ir_sha256() const { return ir_; }
  const std::shared_ptr<const FieldNodeRecipeData> &recipe() const {
    return recipe_;
  }
  const std::vector<uint8_t> &native_graph() const { return graph_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::string scene_, name_;
  std::array<uint8_t, 32> ir_{};
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  std::shared_ptr<const FieldNodeRecipeData> recipe_;
  std::vector<uint8_t> graph_;
};
} // namespace encore::upstream
