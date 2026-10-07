#include "podunk_player_preload_scenes.hpp"
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool PodunkPlayerPreloadSceneResource::state(FieldGlobalExternalState &s,
                                             std::string &e) const {
  if (!scene_ || !registry_)
    return fail(e, "Player preload actual PackedScene owner missing");
  s = {};
  s.name = binding_.source.name;
  e.clear();
  return true;
}
bool PodunkPlayerPreloadSceneResource::deferred(const FieldDeferredMessage &,
                                                std::string &e) {
  return fail(e, "Player preload PackedScene instance requires its concrete "
                 "native/script factory");
}
bool PodunkPlayerPreloadSceneResource::persist_append(FieldObjectId,
                                                      std::string &e) {
  return fail(e, "Player preload PackedScene is not a persistent Node");
}
bool PodunkPlayerPreloadSceneResource::assign_stable_canvas(FieldObjectId,
                                                            std::string &e) {
  return fail(e, "Player preload PackedScene is not UiManager");
}
bool PodunkPlayerPreloadScenes::prepare(const PlayerPreloadScenesData &d,
                                        const PlayerInitializationData &p,
                                        FieldGlobalRegistry &r,
                                        std::string &e) {
  if (data_ || !d.valid() || !p.valid() ||
      d.player_ir_sha256() != p.ir_sha256() || !r.data() ||
      r.data()->identity().upstream_commit != p.identity().upstream_commit)
    return fail(e, "Player preload same source/ObjectDB owners rejected");
  data_ = &d;
  source_ir_ = d.ir_sha256();
  player_ = &p;
  registry_ = &r;
  e.clear();
  return true;
}
bool PodunkPlayerPreloadScenes::resolve(const GlobalYamlValue &row,
                                        FieldObjectId &out, std::string &e) {
  if (!data_ || !player_ || !registry_ || !data_->valid() ||
      data_->ir_sha256() != source_ir_ ||
      data_->player_ir_sha256() != player_->ir_sha256())
    return fail(e,
                "Player preload actual source owner was changed/uninitialized");
  auto *scene = data_->entry(row);
  if (!scene)
    return fail(e, "Player preload original onready resource row unknown");
  auto found = owners_.find(scene->recipe.source_scene());
  if (found != owners_.end()) {
    auto *resource = found->second;
    if (!resource || resource->registry() != registry_ ||
        !registry_->object_exists(resource->binding().object) ||
        registry_->source_resource(resource->binding().object) != resource)
      return fail(e,
                  "Player preload real Resource cache owner retired/foreign");
    out = resource->binding().object;
    e.clear();
    return true;
  }
  std::unique_ptr<PodunkPlayerPreloadSceneResource> owner(
      new PodunkPlayerPreloadSceneResource);
  FieldObjectId id = 0;
  if (!registry_->allocate_object(id, e))
    return false;
  owner->scene_ = std::make_shared<const PlayerPreloadScene>(*scene);
  owner->registry_ = registry_;
  auto &b = owner->binding_;
  b.object = id;
  b.family = 0x454e0072;
  b.capability = 1;
  b.source.identity = scene->recipe.identity();
  b.source.stable_id = b.source.identity.scene_id;
  b.source.role = 4;
  b.source.name = b.source.source = scene->recipe.source_scene();
  b.source.source_sha = b.source.identity.source_sha256;
  b.source.native_class = "PackedScene";
  auto *actual = owner.get();
  if (!registry_->publish_source_resource(b.source, id, std::move(owner), e))
    return false;
  owners_.emplace(scene->recipe.source_scene(), actual);
  out = id;
  e.clear();
  return true;
}
} // namespace encore::ctr
