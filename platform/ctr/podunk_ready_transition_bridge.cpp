#include "podunk_ready_transition_bridge.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
} // namespace
bool PodunkReadyTransitionBridge::prepare(PodunkReadyTransitionInput i,
                                          std::string &e) {
  if (input_.sources || !i.sources || !i.continuation ||
      !i.continuation->initialized() || !i.player || !i.player_sources ||
      !i.player_sources->motion || !i.player_sources->children || !i.tree ||
      !i.geometry || !i.map || !i.native || !i.source ||
      !i.sources->transitions().valid() ||
      i.sources->transitions().capability() != 2 ||
      i.sources->transitions().source_pin() !=
          i.sources->tree().identity().upstream_commit ||
      i.sources->transitions().source_scene() !=
          i.sources->tree().source_scene())
    return fail(e, "Transition normal bridge actual source owners rejected");
  input_ = std::move(i);
  e.clear();
  return true;
}
bool PodunkReadyTransitionBridge::actual(uint32_t stable, FieldObjectId &id,
                                         std::string &e) const {
  if (!input_.sources || !input_.source(stable, id, e))
    return false;
  const auto *d = input_.tree->descriptor(id);
  const auto *n = input_.tree->state(id);
  FieldIdentity identity;
  if (!d || d->id != stable || !n || !n->alive ||
      input_.continuation->registry()->tree_owner(id).get() != input_.tree ||
      !input_.tree->object_identity(id, identity) ||
      !same(identity, input_.sources->tree().identity()))
    return fail(e,
                "Transition source resolver differs from actual Tree/ObjectDB");
  return true;
}
bool PodunkReadyTransitionBridge::flag(PlayerMotionField field, bool &out,
                                       std::string &e) const {
  PlayerInitializationMember v;
  if (!input_.player->body().member(input_.player_sources->motion->field(field),
                                    v, e))
    return false;
  if (v.kind != 1 || !v.value || v.value->kind != 1)
    return fail(e, "Transition actual Player boolean field rejected");
  out = v.value->boolean;
  return true;
}
bool PodunkReadyTransitionBridge::vector(PlayerMotionField field, Vec2 &out,
                                         std::string &e) const {
  PlayerInitializationMember v;
  if (!input_.player->body().member(input_.player_sources->motion->field(field),
                                    v, e))
    return false;
  if (v.kind != 7 || !std::isfinite(v.vector[0]) || !std::isfinite(v.vector[1]))
    return fail(e, "Transition actual Player vector field rejected");
  out = {float(v.vector[0]), float(v.vector[1])};
  return true;
}
bool PodunkReadyTransitionBridge::context(FieldTransitionContext &out,
                                          std::string &e) {
  auto &player = input_.player->body();
  auto *registry = input_.continuation->registry();
  std::shared_ptr<const GlobalLoadObjectArray> party;
  if (!player.constructed() || player.tree() != input_.tree ||
      !input_.continuation->global()->core().array(
          FieldGlobalMemberRole::PartyObjects, party, e) ||
      !party || party->values != std::vector<FieldObjectId>{player.object()} ||
      registry->tree_owner(player.object()).get() != input_.tree ||
      player.object() > std::numeric_limits<uint32_t>::max())
    return fail(e, "Transition requires the actual singleton continuation "
                   "Player, not a follower proxy");
  FieldTransitionContext c;
  c.player = uint32_t(player.object());
  PlayerInitializationMember state;
  if (!player.member(
          input_.player_sources->motion->field(PlayerMotionField::State), state,
          e) ||
      state.kind != 2 || !state.value || state.value->kind != 2)
    return fail(e, "Transition actual Player state field rejected");
  c.player_move = state.value->integer ==
                  input_.player_sources->motion->state(PlayerMotionState::Move);
  c.player_jumping =
      state.value->integer ==
      input_.player_sources->motion->state(PlayerMotionState::Jumping);
  if (!flag(PlayerMotionField::Paused, c.player_paused, e) ||
      !flag(PlayerMotionField::Running, c.running, e) ||
      !flag(PlayerMotionField::Walking, c.walking, e) ||
      !flag(PlayerMotionField::Substantial, c.substantial_movement, e) ||
      !vector(PlayerMotionField::Velocity, c.velocity, e) ||
      !input_.continuation->ui()->source_stack_empty(
          input_.continuation->ui()->binding().object, c.stack_empty, e))
    return false;
  FieldGlobalDataMemberState prompt;
  if (!input_.continuation->characters()->runtime().read_global_member(
          input_.sources->transitions().bindings().prompt_member, prompt, e) ||
      prompt.kind != 4 || !prompt.value || prompt.value->kind != 4)
    return fail(e, "Transition actual source prompt preference unavailable");
  c.prompt_mode = prompt.value->string;
  FieldTransitionActor a;
  a.id = c.player;
  a.kind = 1;
  FieldTransform world;
  FieldObjectId camera, shadow;
  const auto *cameraRow = input_.player_sources->children->record(3);
  if (!cameraRow || !input_.tree->world_transform(player.object(), world, e) ||
      !input_.tree->get_node(player.object(), cameraRow->path, camera, e) ||
      camera > std::numeric_limits<uint32_t>::max() ||
      !input_.tree->get_node(
          player.object(), input_.sources->transitions().bindings().shadow_path,
          shadow, e) ||
      !input_.tree->state(shadow) ||
      !vector(PlayerMotionField::Direction, a.direction, e))
    return false;
  a.position = world[2];
  a.shadow_position = input_.tree->state(shadow)->local[2];
  a.camera_id = uint32_t(camera);
  const auto *node = input_.tree->state(player.object());
  a.physics = std::find(node->groups.begin(), node->groups.end(),
                        "physics_process") != node->groups.end();
  // Child shapes have real native ownership only once Player Ready completed.
  // Source Jump Ready itself never queries these shapes. A disabled shape is
  // observed from its true native body and is excluded, not re-enabled.
  if (input_.player->ready_complete()) {
    auto collect = [&](FieldObjectId owner, bool area) -> bool {
      std::vector<FieldGeometryContact> contacts;
      if (!input_.geometry->player_shapes(owner, contacts, e))
        return false;
      for (const auto &contact : contacts) {
        FieldGeometryActor geometry;
        FieldGeometryOwner physicsOwner;
        FieldGeometryShape shape;
        bool disabled;
        if (!input_.geometry->player_shape_snapshot(
                contact, geometry, physicsOwner, shape, disabled, e))
          return false;
        if (geometry.area != area || contact.actual_owner != owner)
          return fail(
              e, "Transition actual actor collider/Area native type differs");
        if (!disabled) {
          // Transition actor identities are actual ObjectDB handles. The
          // native contact retains the independent source stable ID above.
          if (!area)
            geometry.stable_id = a.id;
          (area ? a.area_geometry : a.body_geometry)
              .push_back(std::move(geometry));
        }
      }
      return true;
    };
    if (!collect(player.object(), false))
      return false;
    for (const auto &row :
         input_.player_sources->initialization->recipe().records())
      if (row.native_class == "Area2D") {
        FieldObjectId area;
        if (!input_.tree->get_node(player.object(), row.path, area, e) ||
            !collect(area, true))
          return false;
      }
  }
  c.party.push_back(std::move(a));
  out = std::move(c);
  e.clear();
  return true;
}
bool PodunkReadyTransitionBridge::apply(FieldPlayerTransitionsHost &host,
                                        std::string &e) {
  if (!input_.sources || applied_)
    return fail(e, "Transition bridge unprepared/already applied");
  host.context = [this](auto &v, auto &error) { return context(v, error); };
  host.cached_ray = [this](const auto &r, const auto &a, auto &hit,
                           auto &error) { return cached(r, a, hit, error); };
  host.apply = [this](const auto &c, auto &error) { return command(c, error); };
  applied_ = true;
  e.clear();
  return true;
}
bool PodunkReadyTransitionBridge::cached(const FieldTransitionDescriptor &r,
                                         const FieldTransitionActor &actor,
                                         uint32_t &out, std::string &e) {
  FieldObjectId id, ground;
  if (!actual(r.ray_id, id, e) || !rays_.count(id) ||
      actor.id != input_.player->body().object() ||
      !input_.tree->get_node(
          actor.id, input_.sources->transitions().bindings().ground_path,
          ground, e))
    return false;
  const auto *shape = input_.tree->state(ground);
  const auto *node = input_.tree->state(id);
  FieldTransform world;
  if (!shape || !node || !input_.tree->world_transform(id, world, e))
    return false;
  const float factor =
      input_.sources->transitions().bindings().ground_multiplier;
  const Vec2 target{actor.position.x + shape->local[2].x * factor,
                    actor.position.y + shape->local[2].y * factor};
  const float rotation =
      std::atan2(target.y - world[2].y, target.x - world[2].x) -
      std::atan2(world[0].y, world[0].x);
  auto local = node->local;
  const float cs = std::cos(rotation), sn = std::sin(rotation);
  for (unsigned i = 0; i < 2; ++i) {
    const auto v = local[i];
    local[i] = {cs * v.x - sn * v.y, sn * v.x + cs * v.y};
  }
  if (!input_.tree->set_local(id, local, e))
    return false;
  const auto hit = rays_.at(id).hit;
  // The native cache is updated solely in PhysicsInternal. ObjectDB expiry
  // returns null, exactly as RayCast2D.get_collider; no synchronous new query.
  if (hit && input_.continuation->registry()->object_exists(hit)) {
    if (hit > std::numeric_limits<uint32_t>::max())
      return fail(e, "Transition cached actual collider identity overflow");
    out = uint32_t(hit);
  } else
    out = 0;
  e.clear();
  return true;
}
bool PodunkReadyTransitionBridge::command(const FieldTransitionCommand &c,
                                          std::string &e) {
  const auto &data = input_.sources->transitions();
  const auto *r = data.record(c.transition);
  if (!r)
    return fail(e, "Transition mutation source row absent");
  if (c.kind == FieldTransitionCommandKind::CanInteract) {
    if (c.actor != input_.player->body().object() || c.value > 1)
      return fail(e, "Transition can_interact actual Player differs");
    PlayerInitializationMember value;
    value.kind = 1;
    auto boolean = std::make_shared<GlobalYamlValue>();
    boolean->kind = 1;
    boolean->boolean = c.value != 0;
    value.value = boolean;
    return input_.player->body().assign_member(
        input_.player_sources->motion->field(PlayerMotionField::CanInteract),
        value, e);
  }
  FieldObjectId object;
  if (c.kind == FieldTransitionCommandKind::ArrowPose ||
      c.kind == FieldTransitionCommandKind::ArrowAnimation) {
    if (r->kind != 1 || c.actor != r->sprite_id ||
        !actual(r->sprite_id, object, e) || !input_.native->owns(object))
      return fail(e, "Transition source arrow is not the actual native Sprite");
    if (c.kind == FieldTransitionCommandKind::ArrowPose) {
      auto local = input_.tree->state(object)->local;
      const float cs = std::cos(c.scalar), sn = std::sin(c.scalar);
      local[0] = {cs * c.second.x, sn * c.second.x};
      local[1] = {-sn * c.second.y, cs * c.second.y};
      local[2] = c.vector;
      return input_.tree->set_local(object, local, e);
    }
    FieldCanvasAppearance pose;
    if (!input_.native->sprite_snapshot(object, pose, e))
      return false;
    pose.offset = c.vector;
    auto local = input_.tree->state(object)->local;
    const float angle = std::atan2(local[0].y, local[0].x);
    const float rotate = c.scalar - angle, cs = std::cos(rotate),
                sn = std::sin(rotate);
    for (unsigned i = 0; i < 2; ++i) {
      const auto v = local[i];
      local[i] = {cs * v.x - sn * v.y, sn * v.x + cs * v.y};
    }
    return input_.native->sprite_publish(object, pose, e) &&
           input_.tree->set_local(object, local, e);
  }
  if (c.kind == FieldTransitionCommandKind::StateChanged) {
    if (c.actor != r->id || c.value > 1 ||
        (c.vector.x != 0 && c.vector.x != 1) || !actual(r->id, object, e))
      return fail(e, "Transition source controlled-state emission rejected");
    return input_.continuation->signals()->emit(
        object, data.bindings().state_signal, {bool(c.vector.x), bool(c.value)},
        e);
  }
  return fail(e, "Transition invoked future jump/stair mutation lacks its "
                 "actual native consumer");
}
bool PodunkReadyTransitionBridge::owns(const FieldNodeDescriptor &d) const {
  if (!input_.sources || d.native_class != "RayCast2D" || !d.script.empty())
    return false;
  for (const auto &r : input_.sources->transitions().records())
    if (r.kind == 1 && r.ray_id == d.id)
      return true;
  return false;
}
bool PodunkReadyTransitionBridge::owns(FieldObjectId id) const {
  return rays_.count(id) != 0;
}
bool PodunkReadyTransitionBridge::construct(FieldObjectId id,
                                            const FieldNodeDescriptor &d,
                                            const FieldIdentity &identity,
                                            std::string &e) {
  if (!owns(d) || rays_.count(id) ||
      !same(identity, input_.sources->tree().identity()))
    return fail(e, "Transition native Ray constructor identity rejected");
  FieldObjectId actualId;
  if (!actual(d.id, actualId, e) || actualId != id)
    return false;
  const auto *original = input_.sources->tree().record(d.id);
  if (!original || original->path != d.path ||
      original->class_index != d.class_index)
    return fail(e, "Transition native Ray source descriptor differs");
  for (unsigned i = 0; i < 3; ++i)
    if (original->local[i].x != d.local[i].x ||
        original->local[i].y != d.local[i].y)
      return fail(e, "Transition native Ray constructor transform differs");
  for (const auto &r : input_.sources->transitions().records())
    if (r.kind == 1 && r.ray_id == d.id) {
      Ray ray;
      ray.source = {r.ray_id,
                    d.parent,
                    r.ray_mask,
                    true,
                    bool(r.ray_flags & 4),
                    bool(r.ray_flags & 1),
                    bool(r.ray_flags & 2),
                    r.ray_cast,
                    d.path};
      if (r.ray_cast.x == 0 && r.ray_cast.y == 0)
        return fail(e, "Transition zero native ray cast requires reviewed "
                       "engine fallback");
      rays_.emplace(id, std::move(ray));
      e.clear();
      return true;
    }
  return fail(e, "Transition native Ray checked binding absent");
}
bool PodunkReadyTransitionBridge::bind(FieldObjectId id, FieldNodeBinding &b,
                                       std::string &e) {
  if (!owns(id))
    return fail(e, "Transition native Ray bind before construction");
  auto *d = input_.tree->descriptor(id);
  b = {};
  b.identity = input_.sources->tree().identity();
  b.stable_id = d->id;
  b.class_index = d->class_index;
  b.script_sha = d->script_sha;
  b.native_class = d->native_class;
  b.family = 0x454e003c;
  b.capability = 3;
  e.clear();
  return true;
}
bool PodunkReadyTransitionBridge::phase(FieldObjectId id, FieldTreePhase p,
                                        float delta, bool paused, bool,
                                        std::string &e) {
  auto found = rays_.find(id);
  const auto *node = input_.tree->state(id);
  if (found == rays_.end() || !node || !node->alive)
    return fail(e, "Transition native Ray phase owner absent");
  auto &ray = found->second;
  switch (p) {
  case FieldTreePhase::EnterNative:
    if (ray.entered)
      return fail(e, "Transition native Ray duplicate Enter");
    ray.entered = true;
    return input_.tree->add_group(id, "physics_process_internal", e);
  case FieldTreePhase::ReadyNative:
    if (!ray.entered || !node->inside || !node->bound)
      return fail(e, "Transition native Ray Ready outside actual tree");
    ray.ready = true;
    e.clear();
    return true;
  case FieldTreePhase::PhysicsInternal:
    if (!ray.entered || !ray.ready || !std::isfinite(delta) || delta < 0)
      return fail(e, "Transition native Ray internal lifecycle rejected");
    if (!input_.tree->can_process(id, paused)) {
      e.clear();
      return true;
    }
    return field_npc_world_ray(ray.source, id, ray.source.cast, *input_.tree,
                               *input_.continuation->registry(), *input_.map,
                               *input_.geometry, input_.map_gates, ray.hit, e);
  case FieldTreePhase::ExitNative:
    if (!ray.entered)
      return fail(e, "Transition native Ray Exit before Enter");
    if (!input_.tree->remove_group(id, "physics_process_internal", e))
      return false;
    ray.entered = false;
    ray.hit = 0;
    return true;
  case FieldTreePhase::Deleting:
    return release(id, e);
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::TransformChanged:
  case FieldTreePhase::LocalTransformChanged:
  case FieldTreePhase::VisibilityChanged:
  case FieldTreePhase::Hide:
  case FieldTreePhase::PathChanged:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
    e.clear();
    return true; // These native RayCast2D notifications have no body.
  default:
    return fail(e, "Transition native Ray unknown phase");
  }
}
bool PodunkReadyTransitionBridge::deferred(const FieldDeferredMessage &,
                                           std::string &e) {
  return fail(e, "Transition Ray has no source deferred method implementation");
}
bool PodunkReadyTransitionBridge::release(FieldObjectId id, std::string &e) {
  if (!rays_.erase(id))
    return fail(e, "Transition native Ray release absent");
  e.clear();
  return true;
}
bool PodunkReadyTransitionBridge::declaration(FieldObjectId id,
                                              std::string_view signal,
                                              uint32_t &arity,
                                              std::string &e) const {
  const auto *d = input_.tree->descriptor(id);
  const auto *r = d ? input_.sources->transitions().record(d->id) : nullptr;
  if (!r || r->kind != 1 ||
      signal != input_.sources->transitions().bindings().state_signal)
    return fail(e, "Transition source signal declaration absent");
  arity = input_.sources->transitions().bindings().state_arguments;
  e.clear();
  return true;
}
bool PodunkReadyTransitionBridge::emission(FieldObjectId id,
                                           std::string_view signal,
                                           size_t count, std::string &e) const {
  uint32_t arity;
  return declaration(id, signal, arity, e) &&
         (count == arity ||
          fail(e, "Transition source state_changed emission arity differs"));
}
} // namespace encore::ctr
