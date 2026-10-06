#include "podunk_player_effects.hpp"
#include <cmath>
#include <limits>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool PodunkPlayerEffectsHost::initialize(
    std::shared_ptr<const PlayerEffectsData> d,
    std::shared_ptr<const PlayerInitializationData> p, FieldGlobalRegistry &r,
    SourceRandom &rng, PodunkPlayerEffectOwners &o, std::string &e) {
  if (data_ || !d || !p || !d->valid() || !p->valid() || r.poisoned())
    return fail(e, "Actual Player effects platform prerequisites missing");
  if (!core_.initialize(*d, *p, r, rng, *this, e))
    return false;
  data_ = std::move(d);
  player_ = std::move(p);
  registry_ = &r;
  timers_ = &timers_owner_;
  owners_ = &o;
  if (!timers_owner_.initialize(
          data_->timer_data(),
          [this](FieldObjectId id, std::string &error) {
            return timeout(id, error);
          },
          [this](FieldObjectId id) -> FieldNodeTreeRuntime * {
            auto owner = registry_->tree_owner(id);
            return owner.get();
          },
          e))
    return false;
  e.clear();
  return true;
}
bool PodunkPlayerEffectsHost::load_preloads(std::string &e) {
  if (!data_ || resources_[0] || resources_[1])
    return fail(e, "Player effect ResourceLoader phase absent/replayed");
  for (uint32_t i = 0; i < resources_.size(); ++i) {
    auto recipe = data_->recipe(i);
    auto native = data_->native(i);
    if (!owners_->load_packed_scene(*recipe, *native, resources_[i], e))
      return false;
    auto resource = registry_->source_resource(resources_[i]);
    auto binding =
        resource ? resource->binding() : FieldGlobalExternalBinding{};
    if (!resource || std::string(resource->resource_class()) != "PackedScene" ||
        binding.source.source != recipe->source_scene() ||
        binding.source.source_sha != recipe->identity().source_sha256 ||
        binding.source.identity.upstream_commit !=
            recipe->identity().upstream_commit)
      return fail(e, "Player effect actual full PackedScene owner differs");
  }
  e.clear();
  return true;
}
bool PodunkPlayerEffectsHost::preload(uint32_t kind,
                                      const FieldNodeRecipeData &recipe,
                                      const GlobalYamlValue &native,
                                      FieldObjectId &out, std::string &e) {
  if (!data_ || kind >= resources_.size() || data_->recipe(kind) != &recipe ||
      data_->native(kind).get() != &native || !resources_[kind] ||
      !registry_->source_resource(resources_[kind]))
    return fail(e, "Player effect source preload owner not loaded");
  out = resources_[kind];
  e.clear();
  return true;
}
bool PodunkPlayerEffectsHost::instance(uint32_t kind,
                                       FieldNodeTreeRuntime &tree,
                                       FieldObjectId &out, std::string &e) {
  if (!data_ || kind >= resources_.size() || !resources_[kind] ||
      !registry_->source_resource(resources_[kind]) ||
      tree.object_domain() != registry_->kernel())
    return fail(e, "Player effect same-domain factory unavailable");
  auto owner = registry_->tree_owner(tree.root());
  if (!owner || owner.get() != &tree)
    return fail(e, "Player effect factory has no actual Tree owner");
  auto recipe = data_->recipe(kind);
  auto native = data_->native(kind);
  if (!owners_->admit_factory(*recipe, *native, e))
    return false;
  if (!tree.instantiate_recipe(*recipe, out, e))
    return false;
  if (!registry_->publish_branch(
          owner, out,
          [this](const FieldDeferredMessage &m, std::string &error) {
            if (!registry_->tree_owner(m.object))
              return fail(error,
                          "Player effect deferred ObjectDB owner absent");
            return owners_->deferred(m, error);
          },
          e))
    return false;
  if (kind == 0) {
    FieldObjectId animation = 0;
    const auto &policy = data_->policy();
    if (!tree.get_node(out, policy.after_animation_path, animation, e))
      return false;
    auto root = out;
    if (!owners_->connect_finished(
            animation, policy.after_finished_signal, root,
            policy.after_finished_method, 0, false,
            [this, root](std::string_view name, FieldObjectId,
                         std::string &error) {
              return core_.after_image_finished(root, name, error);
            },
            e))
      return false;
  }
  e.clear();
  return true;
}
bool PodunkPlayerEffectsHost::ready(FieldObjectId p, FieldObjectId c,
                                    std::string &e) {
  return core_.ready(p, c, e);
}
bool PodunkPlayerEffectsHost::dynamic_script_ready(FieldObjectId id,
                                                   std::string &e) {
  if (!data_)
    return fail(e, "Player dynamic effect owner absent");
  auto tree = registry_->tree_owner(id);
  auto d = tree ? tree->descriptor(id) : nullptr;
  if (!d)
    return fail(e, "Player dynamic effect actual node absent");
  const auto &after = data_->recipe(0)->records().front();
  if (d->script == after.script && d->script_sha == after.script_sha)
    return core_.after_image_ready(id, e);
  if (d->script == data_->policy().tint_script)
    return core_.tint_ready(id, e);
  return fail(e, "Player dynamic effect unknown script Ready");
}
bool PodunkPlayerEffectsHost::timeout(FieldObjectId id, std::string &e) {
  return core_.timeout(id, e);
}
bool PodunkPlayerEffectsHost::bind_timer(FieldNodeTreeRuntime &tree,
                                         FieldObjectId id,
                                         FieldNodeBinding &binding,
                                         std::string &e) {
  return timers_owner_.attach(tree, id, binding, e);
}
bool PodunkPlayerEffectsHost::native_timer_ready(FieldObjectId id,
                                                 std::string &e) {
  return timers_owner_.ready(id, e);
}
bool PodunkPlayerEffectsHost::timer_process(FieldObjectId id,
                                            FieldTreePhase phase, float dt,
                                            bool paused, std::string &e) {
  return timers_owner_.process(id, phase, dt, paused, e);
}
bool PodunkPlayerEffectsHost::release_timer(FieldObjectId id, std::string &e) {
  return timers_owner_.release(id, e);
}
bool PodunkPlayerEffectsHost::duplicate_sprite(FieldObjectId s,
                                               FieldObjectId &out,
                                               std::string &e) {
  return owners_ && owners_->duplicate_sprite(s, out, e);
}
bool PodunkPlayerEffectsHost::copy_sprite(
    FieldObjectId d, FieldObjectId s, const std::vector<std::string> &fields,
    std::string &e) {
  return owners_ && owners_->copy_sprite(d, s, fields, e);
}
bool PodunkPlayerEffectsHost::global_position(FieldObjectId id, Vec2 &out,
                                              std::string &e) const {
  auto tree = registry_ ? registry_->tree_owner(id) : nullptr;
  FieldTransform t{};
  if (!tree || !tree->world_transform(id, t, e))
    return false;
  out = t[2];
  return std::isfinite(out.x) && std::isfinite(out.y);
}
bool PodunkPlayerEffectsHost::set_global_position(FieldObjectId id,
                                                  Vec2 position,
                                                  std::string &e) {
  if (!std::isfinite(position.x) || !std::isfinite(position.y))
    return fail(e, "Player effect global position nonfinite");
  auto tree = registry_ ? registry_->tree_owner(id) : nullptr;
  auto n = tree ? tree->state(id) : nullptr;
  if (!n)
    return fail(e, "Player effect actual world transform owner absent");
  auto local = n->local;
  if (!(n->flags & 4) && n->canvas_parent) {
    FieldTransform parent{};
    auto owner = registry_->tree_owner(n->canvas_parent);
    if (!owner || !owner->world_transform(n->canvas_parent, parent, e))
      return false;
    double determinant =
        double(parent[0].x) * parent[1].y - double(parent[0].y) * parent[1].x;
    if (!std::isfinite(determinant) || determinant == 0)
      return fail(e, "Player effect source parent transform singular");
    double x = double(position.x) - parent[2].x,
           y = double(position.y) - parent[2].y;
    local[2] = {float((parent[1].y * x - parent[1].x * y) / determinant),
                float((-parent[0].y * x + parent[0].x * y) / determinant)};
  } else
    local[2] = position;
  if (!std::isfinite(local[2].x) || !std::isfinite(local[2].y))
    return fail(e, "Player effect local transform overflow");
  return tree->set_local(id, local, e);
}
bool PodunkPlayerEffectsHost::current_scene(FieldObjectId &out,
                                            std::string &e) const {
  if (!registry_)
    return fail(e, "Player effect actual global owner absent");
  out = registry_->current_scene();
  auto tree = registry_->tree_owner(out);
  auto n = tree ? tree->state(out) : nullptr;
  if (!n || !n->inside)
    return fail(e, "Player dust source currentScene unavailable");
  e.clear();
  return true;
}
bool PodunkPlayerEffectsHost::party_size(size_t &out, std::string &e) const {
  return owners_ && owners_->party_size(out, e);
}
bool PodunkPlayerEffectsHost::timer_wait(FieldObjectId id, double seconds,
                                         std::string &e) {
  if (!timers_ || !std::isfinite(seconds) || !std::isfinite(float(seconds)))
    return fail(e, "AfterImage source interval invalid");
  return timers_->set_wait(id, float(seconds), e);
}
bool PodunkPlayerEffectsHost::timer_start(FieldObjectId id, std::string &e) {
  return timers_ && timers_->start(id, 0, e);
}
bool PodunkPlayerEffectsHost::timer_stop(FieldObjectId id, std::string &e) {
  return timers_ && timers_->stop(id, e);
}
bool PodunkPlayerEffectsHost::animation_play(FieldObjectId id,
                                             std::string_view clip,
                                             std::string &e) {
  return owners_ && owners_->play(id, clip, e);
}
bool PodunkPlayerEffectsHost::connect_finished(
    FieldObjectId a, std::string_view signal, FieldObjectId target,
    std::string_view method, FieldObjectId bind, bool once,
    std::function<bool(std::string_view, FieldObjectId, std::string &)> fn,
    std::string &e) {
  return owners_ && owners_->connect_finished(a, signal, target, method, bind,
                                              once, std::move(fn), e);
}
} // namespace encore::ctr
