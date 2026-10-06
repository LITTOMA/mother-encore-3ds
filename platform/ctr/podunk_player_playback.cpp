#include "podunk_player_playback.hpp"
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) { e = s; return false; }
bool playback_source(const PlayerInitializationData &p,
                     const PlayerReadyData &r, std::string &e) {
  auto native = p.native_source();
  auto resources = native ? native->get("resources") : nullptr;
  if (!resources || resources->kind != 5)
    return fail(e, "Player Playback original native resources absent");
  size_t matches = 0;
  for (const auto &row : resources->array) {
    auto id = row ? row->get("id") : nullptr;
    if (!id || id->kind != 2 || id->integer != r.playback_resource()) continue;
    auto type = row->get("class"), path = row->get("path"),
         properties = row->get("properties");
    auto local = properties ? properties->get("resource_local_to_scene") : nullptr;
    auto name = properties ? properties->get("resource_name") : nullptr;
    auto script = properties ? properties->get("script") : nullptr;
    if (!type || type->kind != 4 ||
        type->string != "AnimationNodeStateMachinePlayback" ||
        !path || path->kind != 4 || !path->string.empty() ||
        !properties || properties->kind != 6 ||
        !local || local->kind != 1 || !local->boolean ||
        !name || name->kind != 4 || !name->string.empty() ||
        !script || script->kind != 0)
      return fail(e, "Player Playback original native constructor differs");
    ++matches;
  }
  return matches == 1 ? true : fail(e, "Player Playback source identity ambiguous");
}
}
bool PodunkPlayerPlayback::construct(
    std::shared_ptr<const PlayerInitializationData> p,
    std::shared_ptr<const PlayerReadyData> r, FieldGlobalRegistry &registry,
    FieldObjectId &out, PodunkPlayerPlayback *&owner, std::string &e) {
  if (!p || !r || !p->valid() || !r->valid() || !registry.root() ||
      p->identity().upstream_commit != r->identity().upstream_commit ||
      p->identity().scene_id != r->identity().scene_id ||
      p->identity().source_sha256 != r->identity().source_sha256 ||
      p->ir_sha256() != r->initialization_ir_sha256() ||
      !playback_source(*p, *r, e))
    return fail(e, "Player Playback original scene/graph constructor rejected");
  auto actual = std::unique_ptr<PodunkPlayerPlayback>(new PodunkPlayerPlayback);
  actual->player_ = std::move(p);
  actual->ready_ = std::move(r);
  actual->registry_ = &registry;
  FieldGlobalExternalSpec source;
  source.identity = actual->player_->identity();
  source.stable_id = actual->ready_->playback_resource();
  source.role = 4;
  source.native_class = actual->resource_class();
  source.source = actual->player_->recipe().source_scene();
  source.source_sha = source.identity.source_sha256;
  FieldObjectId id = 0;
  if (!registry.allocate_object(id, e)) return false;
  actual->binding_ = {id, source, 0x454e005a, 1};
  auto borrowed = actual.get();
  if (!registry.publish_source_resource(source, id, std::move(actual), e)) {
    std::string cleanup;
    registry.retire_object(id, cleanup);
    return false;
  }
  out = id;
  owner = borrowed;
  return true;
}
bool PodunkPlayerPlayback::bind_tracks(PlayerGraphHost h, std::string &e) {
  FieldGlobalExternalState s;
  if (!state(s, e) || tracks_bound())
    return fail(e, "Player Playback actual track owner already bound or expired");
  return graph_.initialize(*ready_, std::move(h), e);
}
bool PodunkPlayerPlayback::state(FieldGlobalExternalState &s, std::string &e) const {
  if (!player_ || !ready_ || !registry_ || !binding_.object ||
      !registry_->object_exists(binding_.object))
    return fail(e, "Player Playback actual Resource lifetime expired");
  s = {};
  s.name = binding_.source.name;
  return true;
}
bool PodunkPlayerPlayback::deferred(const FieldDeferredMessage &, std::string &e) {
  return fail(e, "Player Playback deferred method capability unsupported");
}
bool PodunkPlayerPlayback::persist_append(FieldObjectId, std::string &e) {
  return fail(e, "Player Playback is a Resource, not global.persist");
}
bool PodunkPlayerPlayback::assign_stable_canvas(FieldObjectId, std::string &e) {
  return fail(e, "Player Playback is a Resource, not a Canvas owner");
}
} // namespace encore::ctr
