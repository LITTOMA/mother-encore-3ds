#include "encore/global_item_cache.hpp"
namespace encore::upstream {
bool GlobalItemCache::initialize(const FieldItemDefinitions &d,
                                 FieldObjectId owner, std::string &e) {
  if (definitions_ || !owner || !d.valid() || !d.global_constructor_scope()) {
    e = "Global Items cache actual owner/full constructor data required";
    return false;
  }
  definitions_ = &d;
  owner_ = owner;
  e.clear();
  return true;
}
bool GlobalItemCache::insert_loaded_yaml(const std::string &source,
                                         const std::array<uint8_t, 32> &hash,
                                         std::string &e) {
  if (directory_closed_ || !definitions_ || !definitions_->valid() ||
      !definitions_->global_constructor_scope()) {
    e = "Global Items cache uninitialized";
    return false;
  }
  const FieldItemDefinition *item = nullptr;
  for (const auto &r : definitions_->definitions())
    if (r.source == source)
      item = &r;
  std::array<uint8_t, 32> actual{};
  if (!item || !definitions_->source_hash(source, actual) || actual != hash ||
      inserted_.count(item->id)) {
    e = "Global Items source YAML identity/duplicate rejected";
    return false;
  }
  insertion_order_.push_back(item->id);
  inserted_.insert(item->id);
  e.clear();
  return true;
}
bool GlobalItemCache::definitions_loaded() const {
  return definitions_ && definitions_->valid() &&
         definitions_->global_constructor_scope() &&
         inserted_.size() == definitions_->definitions().size();
}
bool GlobalItemCache::observe_directory_complete(
    const std::vector<std::pair<std::string, std::array<uint8_t, 32>>> &expected,
    const std::array<uint8_t, 32> &proof, std::string &e) {
  bool has_proof = false;
  for (auto byte : proof) has_proof |= byte != 0;
  if (directory_closed_ || !definitions_loaded() || !has_proof ||
      expected.size() != insertion_order_.size()) {
    e = "Items actual Directory end/source closure unavailable";
    return false;
  }
  std::set<uint32_t> seen;
  for (const auto &source : expected) {
    const FieldItemDefinition *definition = nullptr;
    for (const auto &row : definitions_->definitions())
      if (row.source == source.first) definition = &row;
    std::array<uint8_t, 32> actual{};
    if (!definition || !inserted_.count(definition->id) ||
        !seen.insert(definition->id).second ||
        !definitions_->source_hash(source.first, actual) ||
        actual != source.second) {
      e = "Items independent Directory source closure differs";
      return false;
    }
  }
  directory_proof_ = proof;
  directory_closed_ = true;
  e.clear();
  return true;
}
const FieldItemDefinition *GlobalItemCache::get_item_data(uint32_t id) {
  if (!definitions_ || !inserted_.count(id))
    return nullptr;
  const auto *definition = definitions_->definition(id);
  if (!definition)
    return nullptr;
  source_ids_[id] = definition->item_name;
  return definition;
}
} // namespace encore::upstream
