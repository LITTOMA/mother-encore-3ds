#include "podunk_dialogue_actor_resource.hpp"
#include <limits>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
class ActorPackedScene final : public FieldGlobalSourceResource {
public:
  ActorPackedScene(FieldGlobalExternalBinding b,
                   std::shared_ptr<const DialogueActorResourceData> d)
      : binding_(std::move(b)), data_(std::move(d)) {}
  FieldGlobalExternalBinding binding() const override { return binding_; }
  const char *resource_class() const override { return "PackedScene"; }
  bool state(FieldGlobalExternalState &s, std::string &e) const override {
    if (!data_ || !data_->valid() || !data_->recipe() ||
        !data_->recipe()->valid() || data_->native_graph().empty() ||
        binding_.source.identity.source_sha256 !=
            data_->identity().source_sha256 ||
        binding_.source.source != data_->source_scene())
      return fail(e, "Actor actual immutable complete source owner rejected");
    s = {};
    s.name = binding_.source.name;
    e.clear();
    return true;
  }
  bool deferred(const FieldDeferredMessage &, std::string &e) override {
    return fail(e, "Actor PackedScene is not a Node; instance/native script "
                   "consumers not admitted");
  }
  bool persist_append(FieldObjectId, std::string &e) override {
    return fail(e, "Actor PackedScene is not a Node");
  }
  bool assign_stable_canvas(FieldObjectId, std::string &e) override {
    return fail(e, "Actor PackedScene is not a CanvasLayer");
  }

private:
  FieldGlobalExternalBinding binding_{};
  std::shared_ptr<const DialogueActorResourceData> data_;
};
} // namespace
bool PodunkDialogueActorResource::prepare(
    std::shared_ptr<const DialogueActorResourceData> d, FieldGlobalRegistry &r,
    std::string &e) {
  if (data_ || !d || !d->valid() || !d->recipe() || !d->recipe()->valid() ||
      !r.data() ||
      d->identity().upstream_commit != r.data()->identity().upstream_commit)
    return fail(e,
                "Actor preload checked source/Registry preparation rejected");
  data_ = std::move(d);
  registry_ = &r;
  e.clear();
  return true;
}
bool PodunkDialogueActorResource::preload(std::string_view path,
                                          const std::array<uint8_t, 32> &sha,
                                          FieldObjectId &out, std::string &e) {
  if (!data_ || !registry_ || path != data_->source_scene() ||
      sha != data_->identity().source_sha256 ||
      holders_ == std::numeric_limits<size_t>::max())
    return fail(e, "Actor source preload declaration rejected");
  if (object_) {
    auto *owner = registry_->source_resource(object_);
    if (!owner || owner->binding().family != 0x454e0074 ||
        owner->binding().source.identity.scene_id != data_->identity().scene_id)
      return fail(e, "Actor actual cached PackedScene owner disappeared");
  } else {
    FieldObjectId id = 0;
    if (!registry_->allocate_object(id, e))
      return false;
    FieldGlobalExternalSpec spec;
    spec.identity = data_->identity();
    spec.stable_id = spec.identity.scene_id;
    spec.role = 4;
    spec.name = data_->resource_name();
    spec.native_class = "PackedScene";
    spec.source = data_->source_scene();
    spec.source_sha = spec.identity.source_sha256;
    FieldGlobalExternalBinding b;
    b.object = id;
    b.source = spec;
    b.family = 0x454e0074;
    b.capability = 1;
    if (!registry_->publish_source_resource(
            spec, id, std::make_unique<ActorPackedScene>(b, data_), e))
      return false;
    object_ = id;
  }
  ++holders_;
  out = object_;
  e.clear();
  return true;
}
bool PodunkDialogueActorResource::release(FieldObjectId id, std::string &e) {
  if (!registry_ || !object_ || id != object_ || !holders_ ||
      !registry_->source_resource(id))
    return fail(e, "Actor preload release holder/actual ObjectDB rejected");
  --holders_;
  e.clear();
  return true;
  // Registry retains its real shared source cache owner until the session ends.
}
} // namespace encore::ctr
