#include "podunk_scene_npc_world.hpp"
#include <algorithm>
#include <cmath>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *m) {
  e = m;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.source_sha256 == b.source_sha256 &&
         a.upstream_commit == b.upstream_commit;
}
bool finite(Vec2 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) <= 1000000 &&
         std::abs(p.y) <= 1000000;
}
bool inverse(const FieldTransform &t, Vec2 p, Vec2 &out) {
  float det = t[0].x * t[1].y - t[0].y * t[1].x;
  if (!det || !std::isfinite(det))
    return false;
  p = {p.x - t[2].x, p.y - t[2].y};
  out = {(t[1].y * p.x - t[1].x * p.y) / det,
         (-t[0].y * p.x + t[0].x * p.y) / det};
  return finite(out);
}
} // namespace
bool PodunkSceneNpcWorld::prepare(
    const FieldNpcWorldData &d, const FieldNodeTreeData &nodes,
    const FieldNpcData &npc, FieldNodeTreeRuntime &t, FieldGlobalRegistry &r,
    FieldGeometrySpace &g, FieldMapSpace &m, FieldNpcRuntime &runtime,
    FieldSpriteRuntime &sprites, PodunkSceneNative &native,
    PodunkSceneTimers &timers, PodunkNpcWorldPorts ports, std::string &e) {
  if (data_ || !d.valid() || !nodes.valid() || !npc.valid() || !g.source() ||
      !m.source() || !same(d.identity(), nodes.identity()) ||
      !same(d.identity(), g.source()->identity()) ||
      !same(d.identity(), m.source()->identity()) ||
      npc.source_pin() != d.identity().upstream_commit ||
      native.canvas_tree() != &t || !ports.map_gates || !ports.player ||
      !ports.global || !ports.characters || !ports.signals ||
      ports.characters->runtime().registry() != &r ||
      ports.signals->registry() != &r)
    return fail(e, "NPC world complete source/actual map/geometry/Canvas "
                   "owners unavailable");
  data_ = &d;
  nodes_ = &nodes;
  npcs_ = &npc;
  tree_ = &t;
  registry_ = &r;
  space_ = &g;
  map_ = &m;
  runtime_ = &runtime;
  sprites_ = &sprites;
  native_ = &native;
  timers_ = &timers;
  ports_ = std::move(ports);
  return true;
}

bool PodunkSceneNpcWorld::activate_geometry(FieldSceneHost &scene,
                                            std::string &e) {
  if (geometry_active_ || !data_ || !scene.scene_ready())
    return fail(
        e, "NPC world complete source Ready required for physics activation");
  std::vector<FieldGeometryNodeUpdate> updates;
  for (auto &entry : instances_) {
    auto &v = entry.second;
    if (v.is_ray)
      continue;
    if (!actual(entry.first, e) || !v.entered || !v.ready)
      return fail(e, "NPC world actual Kinematic Enter/Ready incomplete");
    auto state = tree_->state(entry.first);
    FieldGeometryNodeUpdate update;
    update.stable_id = v.source;
    update.fields = 1;
    update.local = {state->local[0], state->local[1], state->local[2]};
    updates.push_back(update);
  }
  if (!space_->apply_updates(updates, e))
    return false;
  for (const auto &v : runtime_->npcs()) {
    const FieldNpcDescriptor *d = nullptr;
    for (const auto &n : npcs_->npcs())
      if (n.id == v.id)
        d = &n;
    if (!d || !v.ready)
      return fail(e, "NPC world actual source body not Ready");
    if (d->extended_interact &&
        !space_->apply_npc_interaction(*runtime_, v.id, *tree_, *registry_, e))
      return false;
  }
  geometry_active_ = true;
  return true;
}
bool PodunkSceneNpcWorld::source(uint32_t id, FieldObjectId &out,
                                 std::string &e) const {
  out = tree_ ? tree_->source_object(id) : 0;
  auto d = tree_ ? tree_->descriptor(out) : nullptr;
  auto s = tree_ ? tree_->state(out) : nullptr;
  FieldIdentity identity{};
  if (!out || !d || d->id != id || !s || !s->alive ||
      registry_->tree_owner(out).get() != tree_ ||
      !tree_->object_identity(out, identity) ||
      !same(identity, data_->identity()))
    return fail(e, "NPC world actual source node identity unavailable");
  return true;
}
bool PodunkSceneNpcWorld::actual(FieldObjectId id, std::string &e) const {
  auto s = tree_ ? tree_->state(id) : nullptr;
  auto d = tree_ ? tree_->descriptor(id) : nullptr;
  FieldIdentity identity{};
  return data_ && s && s->alive && d &&
                 registry_->tree_owner(id).get() == tree_ &&
                 tree_->object_identity(id, identity) &&
                 same(identity, data_->identity())
             ? true
             : fail(e, "NPC world native actual object/source unavailable");
}
bool PodunkSceneNpcWorld::owns(const FieldNodeDescriptor &d) const {
  return data_ && (data_->body(d.id) || data_->ray(d.id));
}
bool PodunkSceneNpcWorld::owns(FieldObjectId id) const {
  return instances_.count(id) != 0;
}
bool PodunkSceneNpcWorld::native_entered(FieldObjectId id, std::string &e) const {
  const auto v = instances_.find(id);
  const auto *state = tree_ ? tree_->state(id) : nullptr;
  if (v == instances_.end() || v->second.is_ray || !v->second.entered ||
      !state || !state->alive || !state->inside || !state->bound || !actual(id, e))
    return fail(e, "NPC script Ready requires actual entered native body");
  e.clear(); return true;
}
bool PodunkSceneNpcWorld::native_ready(FieldObjectId id, std::string &e) const {
  auto v = instances_.find(id);
  auto s = tree_ ? tree_->state(id) : nullptr;
  return v != instances_.end() && !v->second.is_ray && v->second.entered &&
                 v->second.ready && s && s->inside && s->bound && actual(id, e)
             ? true
             : fail(e, "NPC actual Kinematic source lifecycle not Ready");
}
bool PodunkSceneNpcWorld::construct(FieldObjectId id,
                                    const FieldNodeDescriptor &d,
                                    const FieldIdentity &identity,
                                    std::string &e) {
  if (!data_ || !same(identity, data_->identity()) || !owns(d) ||
      instances_.count(id) || !actual(id, e))
    return fail(e, "NPC native construct identity/duplicate rejected");
  auto original = nodes_->record(d.id);
  if (!original || original->path != d.path ||
      original->script_sha != d.script_sha ||
      original->class_index != d.class_index)
    return fail(e, "NPC native source descriptor differs");
  Native v;
  v.object = id;
  v.source = d.id;
  if (auto ray = data_->ray(d.id)) {
    v.is_ray = true;
    v.ray = *ray;
  } else {
    auto b = data_->body(d.id);
    v.disabled = b->shapes.front().second;
  }
  instances_.emplace(id, std::move(v));
  return true;
}
bool PodunkSceneNpcWorld::bind(FieldObjectId id, FieldNodeBinding &b,
                               std::string &e) {
  if (!owns(id) || !actual(id, e))
    return false;
  auto d = tree_->descriptor(id);
  b = {};
  b.identity = data_->identity();
  b.stable_id = d->id;
  b.class_index = d->class_index;
  b.family = 0x454e006a;
  b.capability = 1;
  b.script_sha = d->script_sha;
  b.native_class = instances_.at(id).is_ray ? "RayCast2D" : "KinematicBody2D";
  return true;
}
bool PodunkSceneNpcWorld::ray_enabled(FieldObjectId id, bool value,
                                      std::string &e) {
  auto i = instances_.find(id);
  if (i == instances_.end() || !i->second.is_ray || !actual(id, e))
    return fail(e, "NPC Ray enabled actual owner missing");
  auto &v = i->second;
  v.ray.enabled = value;
  if (!value)
    v.hit = 0;
  if (v.entered)
    return value ? tree_->add_group(id, "physics_process_internal", e)
                 : tree_->remove_group(id, "physics_process_internal", e);
  return true;
}
bool PodunkSceneNpcWorld::ray_cast(FieldObjectId id, Vec2 cast,
                                   std::string &e) {
  auto i = instances_.find(id);
  if (i == instances_.end() || !i->second.is_ray || !actual(id, e) ||
      !finite(cast))
    return fail(e, "NPC Ray source cast rejected");
  i->second.ray.cast = cast;
  return true;
}
bool PodunkSceneNpcWorld::ray_collider(FieldObjectId id, FieldObjectId &out,
                                       std::string &e) const {
  auto i = instances_.find(id);
  if (i == instances_.end() || !i->second.is_ray || !actual(id, e))
    return fail(e, "NPC Ray cached collider actual owner missing");
  out = i->second.hit;
  if (out && !registry_->object_exists(out))
    out = 0; // Source ObjectDB::get_instance returns null for a retired hit.
  return true;
}
bool PodunkSceneNpcWorld::phase(FieldObjectId id, FieldTreePhase p, float delta,
                                bool paused, bool, std::string &e) {
  auto i = instances_.find(id);
  if (i == instances_.end() || !actual(id, e))
    return false;
  auto &v = i->second;
  switch (p) {
  case FieldTreePhase::EnterNative:
    if (v.entered)
      return fail(e, "NPC native duplicate Enter");
    v.entered = true;
    if (!v.is_ray && !tree_->set_transform_notification(id, false, true, e))
      return false;
    if (v.is_ray && v.ray.enabled)
      return tree_->add_group(id, "physics_process_internal", e);
    return true;
  case FieldTreePhase::ReadyNative: {
    auto s = tree_->state(id);
    if (!v.entered || !s->inside || !s->bound)
      return fail(e, "NPC native Ready actual tree incomplete");
    v.ready = true;
    return true;
  }
  case FieldTreePhase::PhysicsInternal:
    if (!v.is_ray || !v.entered || !v.ready || !std::isfinite(delta) ||
        delta < 0)
      return fail(e, "NPC Ray internal physics lifecycle rejected");
    if (!tree_->can_process(id, paused) || !v.ray.enabled)
      return true;
    {
      Vec2 cast = v.ray.cast;
      if (cast.x == 0 && cast.y == 0)
        cast = data_->zero_cast();
      if (!geometry_active_)
        return fail(
            e, "NPC Ray before complete actual source geometry activation");
      return field_npc_world_ray(v.ray, id, cast, *tree_, *registry_, *map_,
                                 *space_, ports_.map_gates, v.hit, e);
    }
  case FieldTreePhase::ExitNative:
    if (!v.entered)
      return fail(e, "NPC native Exit before Enter");
    if (v.is_ray && v.ray.enabled &&
        !tree_->remove_group(id, "physics_process_internal", e))
      return false;
    v.entered = false;
    return true;
  case FieldTreePhase::Deleting:
    return release(id, e);
  case FieldTreePhase::TransformChanged:
  case FieldTreePhase::LocalTransformChanged: {
    if (v.is_ray || !geometry_active_)
      return true;
    auto state = tree_->state(id);
    FieldGeometryNodeUpdate update;
    update.stable_id = v.source;
    update.fields = 1;
    update.local = {state->local[0], state->local[1], state->local[2]};
    return space_->apply_updates({update}, e);
  }

  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::PathChanged:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::VisibilityChanged:
  case FieldTreePhase::Hide:
    return true; // Native bodies/rays have no callback for these base
                 // notifications.
  default:
    return fail(e, "NPC native unknown source phase");
  }
}
bool PodunkSceneNpcWorld::release(FieldObjectId id, std::string &e) {
  auto i = instances_.find(id);
  if (i == instances_.end())
    return fail(e, "NPC native release owner absent");
  if (i->second.entered)
    return fail(e, "NPC native release before Exit");
  instances_.erase(i);
  return true;
}
bool PodunkSceneNpcWorld::publish_position(uint32_t id, Vec2 p,
                                           std::string &e) {
  FieldObjectId object;
  if (!finite(p) || !source(id, object, e))
    return false;
  auto state = tree_->state(object);
  auto local = state->local;
  if (state->canvas_parent) {
    FieldTransform parent;
    if (!tree_->world_transform(state->canvas_parent, parent, e) ||
        !inverse(parent, p, local[2]))
      return fail(e, "NPC native position parent singular");
  } else
    local[2] = p;
  if (!tree_->set_local(object, local, e))
    return false;
  if (!geometry_active_)
    return true;
  FieldGeometryNodeUpdate update;
  update.stable_id = id;
  update.fields = 1;
  update.local = {local[0], local[1], local[2]};
  return space_->apply_updates({update}, e);
}
bool PodunkSceneNpcWorld::disabled(uint32_t shape, bool value, std::string &e) {
  FieldObjectId object;
  if (!source(shape, object, e) || !native_->set_disabled(object, value, e))
    return false;
  auto desc = tree_->descriptor(object);
  auto i = instances_.find(tree_->source_object(desc->parent));
  if (i != instances_.end() && !i->second.is_ray)
    i->second.disabled = value;
  return true;
}
bool PodunkSceneNpcWorld::move(uint32_t id, Vec2 velocity, float delta,
                               Vec2 &position, Vec2 &out, std::string &e) {
  auto b = data_->body(id);
  auto link = data_->npc(id);
  FieldObjectId object, collider;
  if (!b || !link || !source(id, object, e) ||
      !source(link->collider, collider, e))
    return fail(e, "NPC native motion source binding absent");
  auto i = instances_.find(object);
  if (i == instances_.end() || !i->second.entered || !i->second.ready)
    return fail(e, "NPC native motion lifecycle pending");
  if (!geometry_active_)
    return fail(
        e, "NPC movement before complete actual source geometry activation");
  if (!field_npc_world_move(*b, object, collider, i->second.disabled, velocity,
                            delta, *tree_, *registry_, *map_, *space_,
                            ports_.map_gates, position, out, e))
    return false;
  return publish_position(id, position, e);
}
bool PodunkSceneNpcWorld::timer(uint32_t id, FieldNpcTimer op, double wait,
                                uint64_t receipt, std::string &e) {
  auto link = data_->npc(id);
  FieldObjectId object, timer;
  if (!link || !source(id, object, e) || !source(link->timer, timer, e) ||
      !std::isfinite(wait) || wait < 0 || wait > 60)
    return fail(e, "NPC source Timer binding/value rejected");
  switch (op) {
  case FieldNpcTimer::SetWanderWait:
    return timers_->set_wait(timer, float(wait), e);
  case FieldNpcTimer::StartWander:
    return timers_->start(timer, 0, e);
  case FieldNpcTimer::StopWander:
    return timers_->stop(timer, e);
  case FieldNpcTimer::ReturnDirection:
    if (!receipt || !ports_.return_timer)
      return fail(
          e,
          "NPC source SceneTree.create_timer return-direction owner pending");
    return ports_.return_timer(object, wait, receipt, e);
  }
  return fail(e, "NPC unknown timer opcode");
}
bool PodunkSceneNpcWorld::npc_present(uint32_t id, FieldNpcPresentation op,
                                      const FieldNpcDescriptor &d,
                                      const FieldNpcInstance &pose,
                                      std::string &e) {
  auto link = data_->npc(id);
  FieldObjectId object, sprite;
  if (!link || pose.id != id || d.id != id || runtime_->data() != npcs_ ||
      !source(id, object, e) || !source(link->sprite, sprite, e))
    return fail(e, "NPC presentation actual runtime/source mismatch");
  switch (op) {
  case FieldNpcPresentation::QueueFree:
    return tree_->queue_free(object, e);
  case FieldNpcPresentation::Pose:
    return publish_position(id, pose.position, e);
  case FieldNpcPresentation::Blend:
    if (!sprites_->blend_position(link->sprite, pose.blend)) {
      e = sprites_->error();
      return false;
    }
    return true;
  case FieldNpcPresentation::Motion: {
    auto index = pose.pending_motion != UINT32_MAX ? pose.pending_motion
                                                   : pose.current_motion;
    const auto &name =
        index < d.motions.size() ? d.motions[index].name : d.idle;
    if (!sprites_->travel(link->sprite, name)) {
      e = sprites_->error();
      return false;
    }
    return true;
  }
  case FieldNpcPresentation::Frame:
    if (!native_->sprite_set_frame(sprite, pose.frame, e) ||
        !sprites_->frame(link->sprite, pose.frame)) {
      if (e.empty())
        e = sprites_->error();
      return false;
    }
    return true;
  case FieldNpcPresentation::Visibility:
    if (!tree_->set_visible(object, pose.visible, e) ||
        !disabled(link->collider, !pose.collision_enabled, e) ||
        !disabled(link->interact, !pose.interact_enabled, e) ||
        !tree_->set_process(object, true, pose.physics, e))
      return false;
    return true;
  case FieldNpcPresentation::Create: {
    if (!publish_position(id, pose.position, e) ||
        !sprites_->parent_setup(link->sprite)) {
      if (e.empty())
        e = sprites_->error();
      return false;
    }
    FieldObjectId shadow;
    if (!source(link->shadow, shadow, e))
      return false;
    auto local = tree_->state(shadow)->local;
    local[0] = {pose.geometry[7].scale.x, 0};
    local[1] = {0, pose.geometry[7].scale.y};
    if (!tree_->set_local(shadow, local, e) ||
        !tree_->set_visible(shadow, !d.has(FieldNpcFlag::NoShadow), e))
      return false;
    if (d.has(FieldNpcFlag::NoCollision) && !disabled(link->collider, true, e))
      return false;
    if (d.extended_interact) {
      if (geometry_active_ &&
          !space_->apply_npc_interaction(*runtime_, id, *tree_, *registry_, e))
        return false;
      FieldObjectId interaction;
      if (!source(link->interact, interaction, e))
        return false;
      auto is = tree_->state(interaction);
      auto ilocal = is->local;
      FieldTransform parent;
      if (!tree_->world_transform(is->canvas_parent, parent, e) ||
          !inverse(parent,
                   {pose.position.x + pose.geometry[1].offset.x,
                    pose.position.y + pose.geometry[1].offset.y},
                   ilocal[2]) ||
          !tree_->set_local(interaction, ilocal, e))
        return fail(e, "NPC interaction actual rectangle position rejected");
      FieldGeometryNodeUpdate update;
      update.stable_id = link->interact;
      update.fields = 1;
      update.local = {ilocal[0], ilocal[1], ilocal[2]};
      if (geometry_active_ && !space_->apply_updates({update}, e))
        return false;
    }
    const FieldNpcWorldCallback *visibility = nullptr;
    for (const auto &c : data_->callbacks())
      if (c.op == 12)
        visibility = &c;
    if (!visibility)
      return fail(e, "NPC source visibility callback binding absent");
    if (!ports_.signals->connect(object, data_->visibility_signal(), object,
                                 visibility->method, 0, {}, e))
      return false;
    return true;
  }
  }
  return fail(e, "NPC unknown source presentation opcode");
}
FieldNpcHost PodunkSceneNpcWorld::source_host() {
  auto h = ports_.source;
  h.ready_context=[this](uint32_t stable,FieldNpcReadyContext &out,std::string &e){
    FieldObjectId object=0;
    if(!source(stable,object,e))return false;
    const auto *node=tree_->state(object);
    if(!node||!node->inside||!node->ready_notified)
      return fail(e,"NPC Ready visibility requires actual entered source body");
    out.ancestor_visible=!node->canvas_parent||tree_->visible_in_tree(node->canvas_parent);
    out.debug_build=ports_.actual_debug_build;e.clear();return true;
  };
  h.flag = [this](const std::string &key, bool &value, std::string &e) {
    bool present = false;
    if (!ports_.characters->flags().read(false, key, present, value, e))
      return false;
    value = present && value;
    return true;
  };
  h.seen = [this](uint32_t id, const FieldNpcDialogue &d, bool &value,
                  std::string &e) {
    if (!data_->npc(id))
      return fail(e, "NPC seen-dialogue source identity absent");
    return ports_.characters->flags().seen(d.source, value, e);
  };
  h.mark_seen = [this](uint32_t id, const FieldNpcDialogue &d, std::string &e) {
    if (!data_->npc(id))
      return fail(e, "NPC mark-seen source identity absent");
    return ports_.characters->flags().mark_seen(d.source, e);
  };
  h.begin_talker = [this](uint32_t id, std::string &e) {
    FieldObjectId object;
    return source(id, object, e) &&
           ports_.global->set_object(FieldGlobalMemberRole::Talker, object, e);
  };
  auto source_context = h.context;
  h.context = [this, source_context](uint32_t id, FieldNpcContext &out,
                                     std::string &e) {
    if (!source_context)
      return fail(e, "NPC actual UI/player-paused context owner pending");
    FieldObjectId object;
    if (!source(id, object, e))
      return false;
    std::shared_ptr<const GlobalLoadObjectArray> objects;
    FieldObjectId talker;
    if (!ports_.global->array(FieldGlobalMemberRole::PartyObjects, objects,
                              e) ||
        !objects || objects->values.empty() ||
        objects->values.front() != ports_.player->object() ||
        !ports_.global->object(FieldGlobalMemberRole::Talker, talker, e) ||
        !ports_.player->constructed())
      return fail(e, "NPC actual global player/talker source owner pending");
    if (!source_context(id, out, e))
      return false;
    auto playerTree = registry_->tree_owner(objects->values.front());
    FieldTransform world;
    if (!playerTree ||
        !playerTree->world_transform(objects->values.front(), world, e))
      return false;
    out.player = world[2];
    out.current_talker = talker == object;
    auto state = tree_->state(object);
    out.ancestor_visible =
        !state->canvas_parent || tree_->visible_in_tree(state->canvas_parent);
    return true;
  };
  h.cached_raycast = [this](uint32_t id, Vec2 cast, bool &hit, std::string &e) {
    auto link = data_->npc(id);
    FieldObjectId ray, object;
    if (!link || !source(link->ray, ray, e) || !ray_enabled(ray, true, e) ||
        !ray_cast(ray, cast, e) || !ray_collider(ray, object, e))
      return false;
    hit = object != 0;
    return true;
  };
  h.move_and_slide = [this](uint32_t id, Vec2 v, float d, Vec2 &p, Vec2 &o,
                            std::string &e) { return move(id, v, d, p, o, e); };
  h.present = [this](uint32_t id, FieldNpcPresentation op,
                     const FieldNpcDescriptor &d, const FieldNpcInstance &v,
                     std::string &e) { return npc_present(id, op, d, v, e); };
  h.timer = [this](uint32_t id, FieldNpcTimer op, double time, uint64_t receipt,
                   std::string &e) { return timer(id, op, time, receipt, e); };
  return h;
}
bool PodunkSceneNpcWorld::handles_callback(
    const FieldDeferredMessage &m) const {
  auto d = tree_ ? tree_->descriptor(m.object) : nullptr;
  if (!d || !data_->npc(d->id) || m.kind != FieldDeferredKind::Call)
    return false;
  for (const auto &v : data_->callbacks())
    if (v.method == m.member)
      return true;
  return false;
}
bool PodunkSceneNpcWorld::return_direction_timeout(uint32_t id,
                                                   uint64_t receipt,
                                                   std::string &e) {
  if (!runtime_->return_direction_timeout(id, receipt)) {
    e = runtime_->error();
    return false;
  }
  return true;
}
bool PodunkSceneNpcWorld::callback(uint32_t id, uint32_t op,
                                   const FieldDeferredMessage &m,
                                   std::string &e) {
  bool result = false;
  bool player = false;
  if (op >= 3 && op <= 6) {
    auto object = std::get_if<FieldObjectRef>(&m.args.front());
    if (!object || !registry_->object_exists(object->id))
      return fail(e, "NPC actual source body callback argument rejected");
    std::shared_ptr<const GlobalLoadObjectArray> objects;
    if (!ports_.global->array(FieldGlobalMemberRole::PartyObjects, objects,
                              e) ||
        !objects || objects->values.empty() || !ports_.player->constructed() ||
        objects->values.front() != ports_.player->object())
      return fail(e, "NPC actual global PartyMemberPlayer owner pending");
    player = object->id == objects->values.front();
    if (op >= 5 && !player) {
      auto owner = registry_->tree_owner(object->id);
      auto desc = owner ? owner->descriptor(object->id) : nullptr;
      FieldIdentity identity{};
      const auto *playerData = ports_.player->data();
      if (!desc || !owner->object_identity(object->id, identity) || !playerData)
        return fail(e, "NPC body callback actual source identity absent");
      // Class tests use the checked actual Player source constructor identity.
      // A different live body is not silently assigned to the player class by
      // name.
      auto root = playerData->recipe().records().front();
      player = same(identity, playerData->identity()) && desc->id == root.id &&
               desc->script_sha == root.script_sha;
    }
  }
  switch (op) {
  case 1:
    result = runtime_->wander_timeout(id);
    break;
  case 2:
    return fail(e, "NPC return-direction callback requires actual "
                   "SceneTreeTimer waiter identity");
  case 3:
    result = runtime_->view_entered(id, player);
    break;
  case 4:
    result = runtime_->view_exited(id, player);
    break;
  case 5:
    result = runtime_->near_entered(id, player);
    break;
  case 6:
    result = runtime_->near_exited(id, player);
    break;
  case 7:
    result = runtime_->screen_entered(id);
    break;
  case 8:
    result = runtime_->screen_exited(id);
    break;
  case 9:
    result = runtime_->stop_interaction(id);
    break;
  case 10:
    result = runtime_->interact(id);
    break;
  case 11:
    result = runtime_->telepathy(id);
    break;
  case 12:
    result = runtime_->visibility_changed(id);
    break;
  case 13: {
    FieldObjectId actual;
    bool persistent = false;
    if (!source(id, actual, e) || !ports_.persistent ||
        !ports_.persistent(actual, persistent, e))
      return fail(e, "NPC actual persistent membership owner pending");
    result = runtime_->tree_exiting(id, persistent);
    break;
  }
  default:
    return fail(e, "NPC unsupported source callback");
  }
  if (!result)
    e = runtime_->error();
  return result;
}
bool PodunkSceneNpcWorld::deferred(const FieldDeferredMessage &m,
                                   std::string &e) {
  if (!handles_callback(m) || !actual(m.object, e))
    return fail(e, "NPC world unknown actual callback");
  auto id = tree_->descriptor(m.object)->id;
  for (const auto &v : data_->callbacks())
    if (v.method == m.member) {
      if (v.arity != m.args.size())
        return fail(e, "NPC source callback arity rejected");
      return callback(id, v.op, m, e);
    }
  return false;
}
} // namespace encore::ctr
