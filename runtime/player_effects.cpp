#include "encore/player_effects.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool same_source(FieldNodeTreeRuntime &tree, FieldObjectId id,
                 const FieldNodeRecipeData &recipe,
                 const FieldNodeDescriptor &expected) {
  FieldIdentity actual{};
  auto descriptor = tree.descriptor(id);
  auto identity = recipe.identity();
  return descriptor && tree.object_identity(id, actual) &&
         actual.scene_id == identity.scene_id &&
         actual.upstream_commit == identity.upstream_commit &&
         actual.source_sha256 == identity.source_sha256 &&
         descriptor->id == expected.id &&
         descriptor->native_class == expected.native_class &&
         descriptor->script == expected.script &&
         descriptor->script_sha == expected.script_sha;
}
} // namespace
bool PlayerEffectsRuntime::initialize(const PlayerEffectsData &d,
                                      const PlayerInitializationData &p,
                                      FieldGlobalRegistry &r,
                                      SourceRandom &random,
                                      PlayerEffectsNative &native,
                                      std::string &e) {
  if (data_ || !d.valid() || !p.valid() || r.poisoned() ||
      d.player_ir_sha256() != p.ir_sha256())
    return fail(e,
                "Player effects source/actual Registry prerequisite rejected");
  data_ = &d;
  player_ = &p;
  registry_ = &r;
  random_ = &random;
  native_ = &native;
  e.clear();
  return true;
}
const PlayerEffectCreatorState *
PlayerEffectsRuntime::state(FieldObjectId id) const {
  auto i = states_.find(id);
  return i == states_.end() ? nullptr : &i->second;
}
bool PlayerEffectsRuntime::poison(PlayerEffectCreatorState &s, std::string &e) {
  s.failed = true;
  if (e.empty())
    e = "Player effect source operation failed; actual partial instance "
        "retained";
  return false;
}
PlayerEffectCreatorState *
PlayerEffectsRuntime::creator(FieldObjectId id, uint32_t kind, std::string &e) {
  auto i = states_.find(id);
  if (!data_ || i == states_.end() || !i->second.ready || i->second.failed ||
      i->second.kind != kind || registry_->poisoned() ||
      !registry_->object_exists(id)) {
    fail(e, "Player effect actual ready owner unavailable");
    return nullptr;
  }
  auto tree = registry_->tree_owner(id);
  auto d = tree ? tree->descriptor(id) : nullptr;
  const auto &source = data_->creators()[kind];
  if (!d || d->id != source.id || d->script != source.script ||
      d->script_sha != source.script_sha) {
    fail(e, "Player effect live source attachment differs");
    return nullptr;
  }
  return &i->second;
}
bool PlayerEffectsRuntime::ready(FieldObjectId player, FieldObjectId id,
                                 std::string &e) {
  if (!data_ || states_.count(id) || registry_->poisoned())
    return fail(e, "Player effect onready duplicate/unavailable");
  auto tree = registry_->tree_owner(id);
  auto ptree = registry_->tree_owner(player);
  auto descriptor = tree ? tree->descriptor(id) : nullptr;
  auto node = tree ? tree->state(id) : nullptr;
  FieldIdentity identity{};
  if (!tree || tree != ptree || !descriptor || !node || !node->inside ||
      !node->ready_notified || !tree->object_identity(player, identity) ||
      identity.scene_id != player_->identity().scene_id ||
      identity.upstream_commit != player_->identity().upstream_commit ||
      identity.source_sha256 != player_->identity().source_sha256)
    return fail(
        e, "Player effect onready requires actual Player subtree Ready cursor");
  auto source =
      std::find_if(data_->creators().begin(), data_->creators().end(),
                   [&](const auto &r) { return r.id == descriptor->id; });
  FieldObjectId actual = 0;
  if (source == data_->creators().end() ||
      source->script != descriptor->script ||
      source->script_sha != descriptor->script_sha ||
      !tree->get_node(player, source->path, actual, e) || actual != id)
    return fail(e, "Player effect onready actual source node binding rejected");
  PlayerEffectCreatorState s;
  s.object = id;
  s.player = player;
  s.kind = source->kind;
  states_.emplace(id, s);
  auto &live = states_.at(id);
  // The real ResourceLoader returns a checked, strong, complete PackedScene.
  if (!native_->preload(s.kind, *data_->recipe(s.kind), *data_->native(s.kind),
                        live.resource, e) ||
      !registry_->source_resource(live.resource))
    return poison(live, e);
  if (s.kind == 0) {
    if (!tree->get_node(id, source->timer_path, live.timer, e))
      return poison(live, e);
    auto timer = tree->descriptor(live.timer);
    if (!timer || timer->native_class != "Timer")
      return poison(live, e);
    std::string ignored;
    if (!tree->get_node(id, source->sprite_path, live.sprite, ignored))
      live.sprite = 0;
  }
  live.ready = true;
  e.clear();
  return true;
}
bool PlayerEffectsRuntime::create_after_image(FieldObjectId id,
                                              std::string &e) {
  auto s = creator(id, 0, e);
  if (!s)
    return false;
  auto tree = registry_->tree_owner(id);
  const auto &c = data_->creators()[0];
  FieldObjectId after = 0, copy = 0;
  if (!native_->instance(0, *tree, after, e))
    return poison(*s, e);
  // Source duplicate() retains all children/signals/groups/scripts. It is not
  // a property snapshot and is intentionally not free()'d by this source body.
  if (!s->sprite || !native_->duplicate_sprite(s->sprite, copy, e) || !copy ||
      !registry_->object_exists(copy) ||
      !native_->copy_sprite(after, copy, data_->policy().copied_members, e))
    return poison(*s, e);
  Vec2 position{};
  if (!native_->global_position(s->sprite, position, e) ||
      !native_->set_global_position(after, position, e))
    return poison(*s, e);
  auto parent = id;
  for (uint32_t i = 0; i < c.parent_hops; ++i) {
    auto n = tree->state(parent);
    if (!n || !n->parent)
      return poison(*s, e);
    parent = n->parent;
  }
  if (!tree->add_child(parent, after, e))
    return poison(*s, e);
  e.clear();
  return true;
}
bool PlayerEffectsRuntime::set_interval(FieldObjectId id, double time,
                                        std::string &e) {
  auto s = creator(id, 0, e);
  return s && native_->timer_wait(s->timer, time, e);
}
bool PlayerEffectsRuntime::start_creating(FieldObjectId id, std::string &e) {
  auto s = creator(id, 0, e);
  return s && native_->timer_start(s->timer, e);
}
bool PlayerEffectsRuntime::stop_creating(FieldObjectId id, std::string &e) {
  auto s = creator(id, 0, e);
  return s && native_->timer_stop(s->timer, e);
}
bool PlayerEffectsRuntime::timeout(FieldObjectId timer, std::string &e) {
  auto i = std::find_if(states_.begin(), states_.end(), [&](const auto &v) {
    return v.second.kind == 0 && v.second.timer == timer;
  });
  if (i == states_.end())
    return fail(e, "Player effects unknown Timer timeout");
  if (!create_after_image(i->first, e))
    return false;
  return native_->timer_start(timer, e);
}
bool PlayerEffectsRuntime::create_dust(FieldObjectId id, std::string &e) {
  auto s = creator(id, 1, e);
  if (!s)
    return false;
  const auto &c = data_->creators()[1];
  FieldObjectId scene = 0, objects = 0, dust = 0, anim = 0;
  if (!native_->current_scene(scene, e))
    return poison(*s, e);
  auto tree = registry_->tree_owner(scene);
  if (!tree || !tree->get_node(scene, c.objects_path, objects, e) ||
      !native_->instance(1, *tree, dust, e))
    return poison(*s, e);
  // add_child synchronously runs all original child native/script Ready first.
  if (!tree->add_child(objects, dust, e))
    return poison(*s, e);
  Vec2 position{}, dust_position{};
  if (!native_->global_position(dust, dust_position, e) ||
      !native_->global_position(id, position, e))
    return poison(*s, e);
  dust_position.x =
      float(double(position.x) +
            std::round(random_->rand_range(c.rng_bounds[0], c.rng_bounds[1])));
  if (!native_->set_global_position(dust, dust_position, e) ||
      !native_->global_position(id, position, e))
    return poison(*s, e);
  if (!native_->global_position(dust, dust_position, e))
    return poison(*s, e);
  dust_position.y =
      float(double(position.y) +
            std::round(random_->rand_range(c.rng_bounds[0], c.rng_bounds[1])));
  if (!native_->set_global_position(dust, dust_position, e))
    return poison(*s, e);
  size_t party = 0;
  if (!native_->party_size(party, e))
    return poison(*s, e);
  if (!native_->global_position(dust, dust_position, e))
    return poison(*s, e);
  dust_position.y =
      float(double(dust_position.y) - c.party_depth * double(party));
  if (!std::isfinite(dust_position.y) ||
      !native_->set_global_position(dust, dust_position, e))
    return poison(*s, e);
  s->created_dusts.push_back(dust);
  if (!tree->get_node(dust, c.animation_path, anim, e) ||
      !native_->animation_play(anim, c.animation, e))
    return poison(*s, e);
  if (!native_->connect_finished(
          anim, c.finished_signal, id, c.finished_method, dust, true,
          [this, id](std::string_view name, FieldObjectId d,
                     std::string &error) {
            return destroy_dust(id, name, d, error);
          },
          e))
    return poison(*s, e);
  e.clear();
  return true;
}
bool PlayerEffectsRuntime::destroy_dust(FieldObjectId id, std::string_view,
                                        FieldObjectId dust, std::string &e) {
  auto s = creator(id, 1, e);
  if (!s)
    return false;
  if (!dust || !registry_->object_exists(dust)) {
    e.clear();
    return true;
  }
  auto tree = registry_->tree_owner(dust);
  if (!tree)
    return fail(e, "Dust real live Node owner absent");
  auto i = std::find(s->created_dusts.begin(), s->created_dusts.end(), dust);
  if (i != s->created_dusts.end())
    s->created_dusts.erase(i);
  return tree->queue_free(dust, e);
}
bool PlayerEffectsRuntime::after_image_ready(FieldObjectId id, std::string &e) {
  if (!data_)
    return fail(e, "AfterImage actual source owner absent");
  auto tree = registry_->tree_owner(id);
  auto node = tree ? tree->descriptor(id) : nullptr;
  auto expected =
      data_->recipe(0)->record(data_->recipe(0)->identity().scene_id);
  if (!tree || !node || !expected ||
      !same_source(*tree, id, *data_->recipe(0), *expected))
    return fail(e, "AfterImage actual root source differs");
  FieldObjectId anim = 0;
  if (!tree->get_node(id, data_->policy().after_animation_path, anim, e))
    return false;
  return native_->animation_play(anim, data_->policy().after_animation, e);
}
bool PlayerEffectsRuntime::after_image_finished(FieldObjectId id,
                                                std::string_view,
                                                std::string &e) {
  auto tree = registry_ ? registry_->tree_owner(id) : nullptr;
  if (!tree || !data_ ||
      !same_source(*tree, id, *data_->recipe(0),
                   data_->recipe(0)->records().front()))
    return fail(e, "AfterImage real Node owner absent");
  return tree->queue_free(id, e);
}
bool PlayerEffectsRuntime::tint_ready(FieldObjectId id, std::string &e) {
  if (!data_ || tint_targets_.count(id))
    return fail(e, "Dust CharacterTint onready duplicate/unavailable");
  auto tree = registry_->tree_owner(id);
  auto d = tree ? tree->descriptor(id) : nullptr;
  auto s = tree ? tree->state(id) : nullptr;
  std::array<uint8_t, 32> h{};
  auto expected = d ? data_->recipe(1)->record(d->id) : nullptr;
  if (!d || !s || !s->inside || !s->ready_notified || !expected ||
      !same_source(*tree, id, *data_->recipe(1), *expected) ||
      d->script != data_->policy().tint_script ||
      !data_->source_hash(d->script, h) || h != d->script_sha)
    return fail(e, "Dust CharacterTint actual source Ready rejected");
  std::vector<FieldObjectId> targets;
  for (auto &path : data_->policy().tint_paths) {
    FieldObjectId target = 0;
    std::string ignored;
    if (tree->get_node(id, path, target, ignored))
      targets.push_back(target);
  }
  tint_targets_.emplace(id, std::move(targets));
  e.clear();
  return true;
}
bool PlayerEffectsRuntime::release(FieldObjectId id, std::string &e) {
  states_.erase(id);
  tint_targets_.erase(id);
  e.clear();
  return true;
}
} // namespace encore::upstream
