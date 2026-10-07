#pragma once
#include "encore/dialogue_actor_resource.hpp"
#include "encore/field_global_registry.hpp"
namespace encore::ctr {
// One actual ObjectDB PackedScene shared by source preload holders. Its full
// immutable graph is retained in the Registry resource owner, not a marker ID.
class PodunkDialogueActorResource {
public:
  bool prepare(std::shared_ptr<const upstream::DialogueActorResourceData>,
               upstream::FieldGlobalRegistry &, std::string &);
  bool preload(std::string_view, const std::array<uint8_t, 32> &,
               upstream::FieldObjectId &, std::string &);
  bool release(upstream::FieldObjectId, std::string &);
  const upstream::DialogueActorResourceData *data() const {
    return data_.get();
  }

private:
  std::shared_ptr<const upstream::DialogueActorResourceData> data_;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldObjectId object_ = 0;
  size_t holders_ = 0;
};
} // namespace encore::ctr
