#pragma once
#include "encore/field_global_registry.hpp"
#include "encore/field_item_definitions.hpp"
#include <set>
namespace encore::upstream {
// The actual Items cache owns insertion order. insert_loaded_yaml is called at
// Directory.get_next/_load_data's real insertion cursor, never sorted by name.
// This admits the Items cache only, not the five other globaldata caches.
class GlobalItemCache final {
  const FieldItemDefinitions *definitions_ = nullptr;
  FieldObjectId owner_ = 0;
  bool directory_closed_ = false;
  std::array<uint8_t, 32> directory_proof_{};
  std::vector<uint32_t> insertion_order_;
  std::set<uint32_t> inserted_;
  // Source Dictionary["id"] is an item-name string; stable numeric definition
  // identity belongs to a separate binary/save domain.
  std::map<uint32_t, std::string> source_ids_;

public:
  bool initialize(const FieldItemDefinitions &, FieldObjectId, std::string &);
  bool insert_loaded_yaml(const std::string &source,
                          const std::array<uint8_t, 32> &source_sha,
                          std::string &);
  // Package coverage only; source Directory closure is admitted by its owning
  // initialization consumer, independently of this pack.
  bool definitions_loaded() const;
  // Called at the actual Directory end cursor with the independent checked
  // initialization resource's full source closure, never inferred from this
  // Items package. Does not approve other caches or globalData Ready.
  bool observe_directory_complete(
      const std::vector<std::pair<std::string, std::array<uint8_t, 32>>> &,
      const std::array<uint8_t, 32> &closure_proof, std::string &);
  bool directory_admitted() const { return directory_closed_; }
  const auto &directory_proof() const { return directory_proof_; }
  // Mirrors get_item_data's id=item_name mutation on the same cache record.
  const FieldItemDefinition *get_item_data(uint32_t);
  bool source_id_assigned(uint32_t id) const {
    return source_ids_.count(id) != 0;
  }
  const std::string *source_id(uint32_t id) const {
    auto found = source_ids_.find(id);
    return found == source_ids_.end() ? nullptr : &found->second;
  }
  FieldObjectId owner() const { return owner_; }
  const auto &insertion_order() const { return insertion_order_; }
  const auto *definitions() const { return definitions_; }
};
} // namespace encore::upstream
