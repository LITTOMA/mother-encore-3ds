#include "podunk_scene_grass.hpp"
#include <algorithm>
#include <cmath>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.source_sha256 == b.source_sha256 &&
         a.upstream_commit == b.upstream_commit;
}
} // namespace
class PodunkConcreteSceneGrassFactory::Reference final
    : public FieldGlobalNativeReference {
public:
  Reference(PodunkConcreteSceneGrassFactory &f, FieldGlobalExternalBinding b)
      : factory(f), source(std::move(b)) {}
  ~Reference() {
    if (factory.registry_ && factory.registry_->object_exists(source.object)) {
      std::string e;
      factory.registry_->retire_object(source.object, e);
    }
  }
  FieldGlobalExternalBinding binding() const override { return source; }
  const char *native_class() const override {
    return source.source.native_class.c_str();
  }
  const FieldGlobalRegistry *registry() const override {
    return factory.registry_;
  }
  bool checked_source_hash(std::string_view p,
                           std::array<uint8_t, 32> &h) const override {
    return factory.data_->source_hash(p, h);
  }
  bool dispatch(const FieldDeferredMessage &m, std::string &e) override {
    if (m.kind != FieldDeferredKind::Call || m.object != source.object ||
        m.member != "_signal_callback" || m.args.size() != 1 ||
        !std::holds_alternative<FieldObjectRef>(m.args[0]) ||
        std::get<FieldObjectRef>(m.args[0]).id != source.object ||
        source.source.native_class != "GDScriptFunctionState")
      return fail(e, "Grass actual coroutine method rejected");
    return factory.finish_waiter(source.object, e);
  }
  PodunkConcreteSceneGrassFactory &factory;
  FieldGlobalExternalBinding source;
};
PodunkConcreteSceneGrassFactory::~PodunkConcreteSceneGrassFactory() {
  tweens_.clear();
  renderer_.free();
}
bool PodunkConcreteSceneGrassFactory::create(
    const GrassNativeData &d, const FieldData &field, FieldRuntime &core,
    std::shared_ptr<FieldNodeTreeRuntime> tree, FieldGlobalRegistry &r,
    FieldObjectSignals &signals, FieldGeometrySpace &space,
    PodunkPlayerPhysicsWorld &world, const char *assets,
    PodunkConcreteSceneGrassFactory *&out, std::string &e) {
  if (!d.valid() || !field.valid() || core.data() != &field || !tree ||
      !r.data() || signals.registry() != &r || !space.source() ||
      space.source()->identity().upstream_commit !=
          d.identity().upstream_commit ||
      field.identity().upstream_commit != d.identity().upstream_commit ||
      !assets)
    return fail(e, "Grass actual factory owners/source prerequisites rejected");
  std::unique_ptr<PodunkConcreteSceneGrassFactory> owner(
      new PodunkConcreteSceneGrassFactory);
  owner->data_ = &d;
  owner->field_ = &field;
  owner->core_ = &core;
  owner->tree_ = std::move(tree);
  owner->registry_ = &r;
  owner->signals_ = &signals;
  owner->space_ = &space;
  owner->world_ = &world;
  if (!owner->renderer_.load(field, assets, e))
    return false;
  FieldObjectId id = 0;
  if (!r.allocate_object(id, e))
    return false;
  auto &b = owner->binding_;
  b.object = id;
  b.family = 0x454e0071;
  b.capability = 1;
  b.source.identity = d.identity();
  b.source.stable_id = d.identity().scene_id;
  b.source.role = 4;
  b.source.name = d.recipe().source_scene();
  b.source.source = d.recipe().source_scene();
  b.source.native_class = "PackedScene";
  b.source.source_sha = d.identity().source_sha256;
  auto *pointer = owner.get();
  if (!r.publish_source_resource(b.source, id, std::move(owner), e))
    return false;
  out = pointer;
  return true;
}
bool PodunkConcreteSceneGrassFactory::state(FieldGlobalExternalState &out,
                                            std::string &e) const {
  if (!data_ || !registry_)
    return fail(e, "Grass PackedScene actual owner absent");
  out = {};
  out.name = binding_.source.name;
  e.clear();
  return true;
}
bool PodunkConcreteSceneGrassFactory::persist_append(FieldObjectId,
                                                     std::string &e) {
  return fail(e, "Grass PackedScene is not a persistent Node");
}
bool PodunkConcreteSceneGrassFactory::assign_stable_canvas(FieldObjectId,
                                                           std::string &e) {
  return fail(e, "Grass PackedScene is not UiManager");
}
bool PodunkConcreteSceneGrassFactory::owns(const FieldNodeDescriptor &n) const {
  return data_ && data_->native_matches(n);
}
bool PodunkConcreteSceneGrassFactory::owns(FieldObjectId id) const {
  auto ref = references_.find(id);
  return nodes_.count(id) ||
         (ref != references_.end() && !ref->second.expired());
}
bool PodunkConcreteSceneGrassFactory::actual(FieldObjectId id, Node *&out,
                                             std::string &e) {
  auto i = nodes_.find(id);
  const auto *n = tree_ ? tree_->descriptor(id) : nullptr;
  const auto *s = tree_ ? tree_->state(id) : nullptr;
  FieldIdentity identity;
  if (!data_ || poisoned_ || i == nodes_.end() || i->second.released || !n ||
      !s || !s->alive || !owns(*n) || !registry_->object_exists(id) ||
      registry_->tree_owner(id) != tree_ ||
      !tree_->object_identity(id, identity) ||
      !same(identity, data_->identity()))
    return fail(e, "Grass actual native/source node identity rejected");
  out = &i->second;
  return true;
}
bool PodunkConcreteSceneGrassFactory::native_construct(
    FieldObjectId id, const FieldNodeDescriptor &n,
    const FieldIdentity &identity, std::string &e) {
  if (!building_factory_ || !data_ || poisoned_ || nodes_.count(id) ||
      !owns(n) || !same(identity, data_->identity()) ||
      registry_->tree_owner(id) != tree_ || !registry_->object_exists(id))
    return fail(e, "Grass actual native allocation/factory cursor rejected");
  size_t index = 0;
  while (index < 6 && data_->node(GrassNativeRole(index + 1)) != n.id)
    ++index;
  if (index == 6)
    return fail(e, "Grass source native role missing");
  if (index == 0) {
    if (building_)
      return fail(e, "Grass source root constructed twice");
    building_ = id;
    instances_.emplace(id, Instance{});
  } else if (!building_)
    return fail(e, "Grass native child precedes source root");
  auto &instance = instances_.at(building_);
  if (instance.nodes[index])
    return fail(e, "Grass actual child role constructed twice");
  instance.nodes[index] = id;
  Node node;
  node.root = building_;
  node.role = GrassNativeRole(index + 1);
  node.binding = {identity, n.id,         n.class_index, 0x454e0071,
                  1,        n.script_sha, n.native_class};
  nodes_.emplace(id, std::move(node));
  if (index == 0 &&
      !space_->reserve_grass_owner(*data_, *tree_, *registry_, id, e))
    return false;
  return true;
}
bool PodunkConcreteSceneGrassFactory::construct_source(
    FieldObjectId id, const FieldNodeDescriptor &n,
    const FieldIdentity &identity, std::string &e) {
  Node *node = nullptr;
  if (!actual(id, node, e) || !same(identity, data_->identity()) ||
      node->role != GrassNativeRole::Area || n.script != data_->script() ||
      node->source_ready)
    return fail(e, "Grass source constructor attachment rejected");
  auto &body = instances_.at(node->root);
  if (body.source_constructed)
    return fail(e, "Grass source constructor body repeated");
  // The only source declaration is its fresh objects Array. Onready child
  // references belong to the later ScriptReady boundary, never allocation.
  body.bodies.clear();
  body.source_constructed = true;
  return true;
}
bool PodunkConcreteSceneGrassFactory::bind(FieldObjectId id,
                                           FieldNodeBinding &out,
                                           std::string &e) {
  Node *node = nullptr;
  if (!actual(id, node, e))
    return false;
  out = node->binding;
  return true;
}
bool PodunkConcreteSceneGrassFactory::source_spawner(FieldObjectId actual,
                                                     uint32_t source,
                                                     FieldGrass &out,
                                                     std::string &e) const {
  const auto *s = tree_ ? tree_->state(actual) : nullptr;
  const auto *n = tree_ ? tree_->descriptor(actual) : nullptr;
  FieldIdentity identity;
  bool found = false;
  for (uint32_t i = 0; i < field_->grass_count(); ++i)
    if (field_->grass(i).stable_id == source) {
      out = field_->grass(i);
      found = true;
      break;
    }
  if (!found || !s || !n || !s->inside || !s->bound || !s->ready_notified ||
      n->id != source || n->path != field_->string(out.node_string) ||
      n->name != field_->string(out.name_string) ||
      n->ready != out.ready_ordinal || registry_->tree_owner(actual) != tree_ ||
      !tree_->object_identity(actual, identity) ||
      !same(identity, field_->identity()))
    return fail(e, "Grass source spawner actual Ready/body rejected");
  return true;
}
bool PodunkConcreteSceneGrassFactory::screen_entered(FieldObjectId spawner,
                                                     uint32_t stable,
                                                     std::string &e) {
  FieldGrass source;
  if (!source_spawner(spawner, stable, source, e))
    return false;
  if (core_->grass_instance(stable))
    return true;
  if (building_factory_ || poisoned_)
    return fail(e, "Grass nested source factory rejected");
  building_factory_ = true;
  building_ = 0;
  FieldObjectId root = 0;
  const bool made = tree_->instantiate_recipe(data_->recipe(), root, e);
  building_factory_ = false;
  building_ = 0;
  if (!made) {
    poisoned_ = true;
    return false;
  }
  auto i = instances_.find(root);
  const auto *state = tree_->state(spawner);
  if (i == instances_.end() || !state || !state->parent)
    return fail(e, "Grass real source parent missing");
  i->second.spawner = stable;
  for (size_t k = 0; k < 6; ++k) {
    FieldObjectId id = 0;
    auto *record = data_->recipe().record(data_->node(GrassNativeRole(k + 1)));
    if (!record || !tree_->get_node(root, record->path, id, e) ||
        id != i->second.nodes[k])
      return fail(e, "Grass complete dynamic subtree differs");
  }
  for (const auto &c : data_->connections()) {
    size_t a = 0, b = 0;
    while (data_->node(GrassNativeRole(a + 1)) != c.emitter)
      ++a;
    while (data_->node(GrassNativeRole(b + 1)) != c.receiver)
      ++b;
    if (!signals_->connect(i->second.nodes[a], c.signal, i->second.nodes[b],
                           c.method, FieldSignalPersist, {}, e))
      return false;
  }
  // Original assigns global_position while detached, from the spawner's local
  // position; the parent's transform is applied only by immediate add_child.
  auto local = tree_->state(root)->local;
  local[2] = state->local[2];
  if (!tree_->set_local(root, local, e) ||
      !tree_->add_child(state->parent, root, e))
    return false;
  for (auto id : i->second.nodes)
    if (!nodes_.at(id).ready)
      return fail(e,
                  "Grass original immediate add_child native Ready incomplete");
  if (!i->second.onready)
    return fail(e, "Grass original onready refs not established");
  FieldTransform world;
  if (!tree_->world_transform(root, world, e) ||
      !core_->bind_native_instance(stable, root, world[2], e))
    return false;
  i->second.published = true;
  if (!synchronize(root, e))
    return false;
  FieldGrassDraw selected;
  bool physics, graph, timer;
  double left;
  if (!core_->native_pose(root, selected, physics, graph, timer, left, e))
    return false;
  if (selected.texture_index != data_->profile().texture_first &&
      !signals_->emit(i->second.nodes[2], "texture_changed", {}, e))
    return false;
  // set_grass writes the actual Sprite texture and flip pose before the source
  // currentGrass reference. No random draw occurs in this factory.
  if (!core_->publish_native_instance(stable, root, e) ||
      !space_->register_grass_shape(*data_, *tree_, *registry_, root,
                                    i->second.nodes[1], e))
    return false;
  i->second.geometry = true;
  std::vector<FieldGeometryContact> contacts;
  if (!space_->player_shapes(root, contacts, e) || contacts.size() != 1 ||
      !world_->admit_grass_monitor(*data_, root, contacts.front(), e))
    return false;
  i->second.monitor = true;
  return true;
}
bool PodunkConcreteSceneGrassFactory::screen_exited(FieldObjectId spawner,
                                                    uint32_t stable,
                                                    std::string &e) {
  FieldGrass source;
  if (!source_spawner(spawner, stable, source, e))
    return false;
  const auto id = core_->grass_instance(stable);
  if (!id)
    return true;
  auto i = instances_.find(id);
  if (i == instances_.end() || i->second.spawner != stable)
    return fail(e, "Grass currentGrass owner differs");
  if (!tree_->queue_free(id, e))
    return false;
  return core_->screen_exited(stable, e);
}
bool PodunkConcreteSceneGrassFactory::synchronize(FieldObjectId root,
                                                  std::string &e) {
  auto i = instances_.find(root);
  if (i == instances_.end() || !i->second.published)
    return fail(e, "Grass source body not published");
  FieldGrassDraw pose;
  bool physics, graph, timer;
  double left;
  if (!core_->native_pose(root, pose, physics, graph, timer, left, e))
    return false;
  if (!tree_->set_process(root, true, physics, e))
    return false;
  const char *group = "_process_internal";
  std::vector<FieldObjectId> current;
  if (!tree_->group(group, current, e))
    return false;
  for (const auto &entry : std::array<std::pair<FieldObjectId, bool>, 2>{
           {{i->second.nodes[4], graph},
            {i->second.nodes[5], timer && i->second.timer_alive}}}) {
    bool member =
        std::find(current.begin(), current.end(), entry.first) != current.end();
    if (member != entry.second &&
        (entry.second ? !tree_->add_group(entry.first, group, e)
                      : !tree_->remove_group(entry.first, group, e)))
      return false;
  }
  auto local = tree_->state(i->second.nodes[2])->local;
  local[0] = {1, 0};
  local[1] = {0, pose.scale_y};
  return tree_->set_local(i->second.nodes[2], local, e);
}
bool PodunkConcreteSceneGrassFactory::update_positions(FieldObjectId root,
                                                       std::string &e) {
  auto &instance = instances_.at(root);
  FieldTransform position;
  if (!tree_->world_transform(root, position, e) ||
      !core_->native_position(root, position[2], e))
    return false;
  for (auto body : instance.bodies) {
    auto tree = registry_->tree_owner(body);
    if (!tree || !tree->world_transform(body, position, e) ||
        !core_->set_body_global_x(body, position[2].x, e))
      return false;
  }
  return true;
}
bool PodunkConcreteSceneGrassFactory::phase(FieldObjectId id, FieldTreePhase p,
                                            float dt, bool paused,
                                            std::string &e) {
  Node *node = nullptr;
  if (!actual(id, node, e))
    return false;
  auto &instance = instances_.at(node->root);
  switch (p) {
  case FieldTreePhase::ReadyScript:
    if (node->role != GrassNativeRole::Area || node->source_ready ||
        !instance.source_constructed)
      return fail(e, "Grass source Ready ownership rejected");
    for (size_t k = 1; k < 6; ++k) {
      auto *child = tree_->state(instance.nodes[k]);
      if (!child || child->parent != id || !nodes_.at(instance.nodes[k]).ready)
        return fail(e, "Grass original onready child reference missing");
    }
    instance.onready = true;
    node->source_ready = true;
    return true;
  case FieldTreePhase::ReadyNative:
    if (node->ready)
      return fail(e, "Grass native Ready repeated without request_ready");
    if (node->role == GrassNativeRole::Graph) {
      if (!nodes_.at(instance.nodes[3]).ready)
        return fail(e, "Grass graph actual AnimationPlayer not Ready");
    }
    node->ready = true;
    return true;
  case FieldTreePhase::EnterNative:
    node->entered = true;
    return true;
  case FieldTreePhase::ExitNative:
    if (node->role == GrassNativeRole::Area) {
      if (instance.monitor && !world_->static_monitor_exit(id, e))
        return false;
      instance.monitor = false;
      if (instance.geometry &&
          !space_->remove_player_shape(instance.nodes[1], e))
        return false;
      instance.geometry = false;
    }
    node->entered = false;
    return true;
  case FieldTreePhase::Physics:
    if (node->role != GrassNativeRole::Area || !instance.published ||
        !tree_->can_process(id, paused))
      return fail(e, "Grass source physics owner/can_process rejected");
    return update_positions(id, e) && core_->native_physics(id, dt, e);
  case FieldTreePhase::IdleInternal:
    if (!instance.published || !tree_->can_process(id, paused))
      return fail(e, "Grass native idle owner/can_process rejected");
    if (node->role == GrassNativeRole::Graph) {
      FieldGrassDraw before, after;
      bool physics, graph, timer;
      double left;
      if (!core_->native_pose(node->root, before, physics, graph, timer, left,
                              e) ||
          !core_->native_graph(node->root, e) ||
          !core_->native_pose(node->root, after, physics, graph, timer, left,
                              e))
        return false;
      return before.frame == after.frame ||
             signals_->emit(instance.nodes[2], "frame_changed", {}, e);
    }
    if (node->role == GrassNativeRole::Timer) {
      bool timeout = false;
      if (!core_->native_timer(node->root, dt, timeout, e) ||
          !synchronize(node->root, e))
        return false;
      if (timeout) {
        for (const auto &c : data_->connections())
          if (c.kind == 3)
            return signals_->emit(id, c.signal, {}, e);
      }
      return true;
    }
    return fail(e, "Grass unsupported internal leaf clock");
  case FieldTreePhase::TreeExiting:
    if (node->role == GrassNativeRole::Area) {
      for (const auto &c : data_->connections())
        if (c.kind == 4)
          return signals_->emit(id, c.signal, {}, e);
    }
    return true;
  case FieldTreePhase::ReadySignal:
    return signals_->emit(id, "ready", {}, e);
  case FieldTreePhase::TreeEntered:
    return signals_->emit(id, "tree_entered", {}, e);
  case FieldTreePhase::TreeExited:
    return signals_->emit(id, "tree_exited", {}, e);
  case FieldTreePhase::Deleting:
    return release(id, e);
  case FieldTreePhase::EnterScript:
  case FieldTreePhase::ExitScript:
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::NodeAdded:
  case FieldTreePhase::NodeRemoved:
  case FieldTreePhase::ChildEntered:
  case FieldTreePhase::ChildExiting:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::PathChanged:
  case FieldTreePhase::VisibilityChanged:
  case FieldTreePhase::Hide:
  case FieldTreePhase::TransformChanged:
  case FieldTreePhase::LocalTransformChanged:
    // These native Node/Canvas states are held and mutated by this exact Tree;
    // no source callback is declared for them in the complete checked recipe.
    return true;
  default:
    return fail(e,
                "Grass native/source phase is outside admitted source methods");
  }
}
bool PodunkConcreteSceneGrassFactory::signal_declaration(
    FieldObjectId id, std::string_view signal, uint32_t &n,
    std::string &e) const {
  if (!owns(id))
    return fail(e, "Grass signal emitter not owned");
  auto found = nodes_.find(id);
  if (found == nodes_.end()) {
    auto owner = registry_->native_reference(id);
    if (!owner || owner->binding().family != 0x454e0071)
      return fail(e, "Grass actual Reference signal emitter missing");
    if (signal == data_->tween_signal() &&
        (std::string_view(owner->native_class()) == "SceneTreeTween" ||
         std::string_view(owner->native_class()) == "PropertyTweener")) {
      n = 0;
      return true;
    }
    if (std::string_view(owner->native_class()) == "SceneTreeTween" &&
        (signal == "step_finished" || signal == "loop_finished")) {
      n = 1;
      return true;
    }
    return fail(e, "Grass actual Reference signal undeclared");
  }
  const auto &node = found->second;
  if (signal == "tree_entered" || signal == "tree_exiting" ||
      signal == "tree_exited" || signal == "ready") {
    n = 0;
    return true;
  }
  if (node.role == GrassNativeRole::Sprite &&
      (signal == "frame_changed" || signal == "texture_changed")) {
    n = 0;
    return true;
  }
  for (const auto &c : data_->connections())
    if (c.emitter == node.binding.stable_id && c.signal == signal) {
      n = (c.kind == 1 || c.kind == 2) ? 1 : 0;
      return true;
    }
  if (node.role == GrassNativeRole::Area &&
      (signal == "body_shape_entered" || signal == "body_shape_exited" ||
       signal == "area_shape_entered" || signal == "area_shape_exited")) {
    n = 4;
    return true;
  }
  if (node.role == GrassNativeRole::Area &&
      (signal == "area_entered" || signal == "area_exited")) {
    n = 1;
    return true;
  }
  return fail(e, "Grass native signal declaration unknown");
}
bool PodunkConcreteSceneGrassFactory::handles_callback(
    const FieldDeferredMessage &m) const {
  auto found = nodes_.find(m.object);
  if (found == nodes_.end())
    return false;
  const auto &node = found->second;
  if (node.role != GrassNativeRole::Area)
    return false;
  return std::any_of(data_->connections().begin(), data_->connections().end(),
                     [&](const auto &c) {
                       return c.method == m.member &&
                              c.receiver == node.binding.stable_id;
                     });
}
bool PodunkConcreteSceneGrassFactory::deferred(const FieldDeferredMessage &m,
                                               std::string &e) {
  if (!handles_callback(m) || m.kind != FieldDeferredKind::Call)
    return fail(e, "Grass source callback unknown");
  Node *node = nullptr;
  if (!actual(m.object, node, e))
    return false;
  auto &instance = instances_.at(node->root);
  const GrassNativeConnection *connection = nullptr;
  for (const auto &c : data_->connections())
    if (c.method == m.member)
      connection = &c;
  if (!connection)
    return false;
  if (connection->kind == 4) {
    if (!m.args.empty() || !instance.timer_alive)
      return fail(e, "Grass source tree_exiting arguments/timer invalid");
    if (!tree_->queue_free(instance.nodes[5], e))
      return false;
    instance.timer_alive = false;
    return true;
  }
  if (!instance.published)
    return fail(e, "Grass callback before set_grass publication");
  if (connection->kind == 3) {
    if (!m.args.empty())
      return fail(e, "Grass timeout callback arguments invalid");
    return create_tween(node->root, true, e);
  }
  if (m.args.size() != 1 || !std::holds_alternative<FieldObjectRef>(m.args[0]))
    return fail(e, "Grass body callback actual object argument invalid");
  auto body = std::get<FieldObjectRef>(m.args[0]).id;
  if (!body)
    return fail(e, "Grass source body object unavailable");
  if (connection->kind == 1) {
    const bool visible = (tree_->state(node->root)->flags & 2) != 0;
    if (!visible)
      return true;
    auto bodytree = registry_->tree_owner(body);
    FieldTransform position;
    if (!bodytree || !bodytree->world_transform(body, position, e))
      return fail(e, "Grass source body global_position unavailable");
    FieldGrassDraw pose;
    bool physics, graph, timer;
    double left;
    if (!core_->native_pose(node->root, pose, physics, graph, timer, left, e) ||
        !core_->body_entered(node->root, body, position[2].x, true, e))
      return false;
    instance.bodies.push_back(body);
    if (left == 0 && !create_tween(node->root, false, e))
      return false;
  } else {
    if (!core_->body_exited(node->root, body, e, instance.timer_alive))
      return false;
    auto i = std::find(instance.bodies.begin(), instance.bodies.end(), body);
    if (i != instance.bodies.end())
      instance.bodies.erase(i);
  }
  return synchronize(node->root, e);
}
bool PodunkConcreteSceneGrassFactory::make_reference(
    const char *type, std::string_view proof, uint32_t stable,
    std::shared_ptr<Reference> &out, std::string &e) {
  FieldGlobalExternalBinding b;
  if (!registry_->allocate_object(b.object, e))
    return false;
  b.family = 0x454e0071;
  b.capability = 1;
  b.source.identity = data_->identity();
  b.source.stable_id = stable;
  b.source.role = 5;
  b.source.native_class = type;
  b.source.source = std::string(proof);
  if (!data_->source_hash(proof, b.source.source_sha))
    return fail(e, "Grass actual native Reference reviewed source missing");
  b.source.identity.source_sha256 = b.source.source_sha;
  if (std::string_view(type) == "GDScriptFunctionState") {
    b.source.script = b.source.source;
    b.source.script_sha = b.source.source_sha;
  }
  auto owner = std::make_shared<Reference>(*this, b);
  if (!registry_->publish_native_reference(b.source, b.object, owner, e))
    return false;
  references_[b.object] = owner;
  out = std::move(owner);
  return true;
}
bool PodunkConcreteSceneGrassFactory::create_tween(FieldObjectId root,
                                                   bool waiter,
                                                   std::string &e) {
  if (tweens_.size() >= 8192)
    return fail(e, "Grass actual Tween owner capacity exhausted");
  auto &i = instances_.at(root);
  Tween t;
  t.root = root;
  t.sprite = i.nodes[2];
  t.duration = float(waiter ? data_->profile().exit_tween
                            : data_->profile().enter_tween);
  if (!make_reference("SceneTreeTween", data_->tween_source(),
                      data_->node(GrassNativeRole::Sprite), t.owner, e) ||
      !make_reference("PropertyTweener", data_->tween_source(),
                      data_->node(GrassNativeRole::Sprite), t.property, e))
    return false;
  if (waiter) {
    if (!make_reference("GDScriptFunctionState", data_->script(),
                        data_->node(GrassNativeRole::Area), t.waiter, e) ||
        !signals_->connect(t.owner->binding().object, data_->tween_signal(),
                           t.waiter->binding().object, "_signal_callback",
                           FieldSignalOneShot,
                           {FieldObjectRef{t.waiter->binding().object}}, e))
      return false;
  }
  tweens_.push_back(std::move(t));
  return true;
}
bool PodunkConcreteSceneGrassFactory::finish_waiter(FieldObjectId id,
                                                    std::string &e) {
  for (auto &t : tweens_)
    if (t.waiter && t.waiter->binding().object == id) {
      if (!t.dead)
        return fail(e, "Grass coroutine resumed before actual Tween finished");
      if (!registry_->object_exists(t.root)) {
        t.waiter.reset();
        return true;
      }
      FieldGrassDraw pose;
      bool physics, graph, timer;
      double left;
      if (!core_->native_pose(t.root, pose, physics, graph, timer, left, e) ||
          !core_->native_scale(t.root, pose.scale_y, true, e) ||
          !synchronize(t.root, e))
        return false;
      references_.erase(id);
      t.waiter.reset();
      return true;
    }
  return fail(e, "Grass actual coroutine waiter unknown");
}
bool PodunkConcreteSceneGrassFactory::tween_frame(uint64_t epoch, float delta,
                                                  bool paused, std::string &e) {
  if (poisoned_ || !epoch || epoch <= tween_epoch_ || !std::isfinite(delta) ||
      delta < 0 || delta > 1)
    return fail(e, "Grass actual SceneTree Tween clock/order rejected");
  tween_epoch_ = epoch;
  for (auto i = nodes_.begin(); i != nodes_.end();)
    if (i->second.released && !registry_->object_exists(i->first))
      i = nodes_.erase(i);
    else
      ++i;
  for (auto i = instances_.begin(); i != instances_.end();)
    if (!registry_->object_exists(i->first))
      i = instances_.erase(i);
    else
      ++i;
  for (auto i = references_.begin(); i != references_.end();)
    if (i->second.expired())
      i = references_.erase(i);
    else
      ++i;
  if (tweens_.empty())
    return true;
  auto last = std::prev(tweens_.end());
  for (auto i = tweens_.begin(); i != tweens_.end();) {
    const bool final = i == last;
    auto &t = *i;
    bool remove = t.dead || !registry_->object_exists(t.root) ||
                  !registry_->object_exists(t.sprite);
    if (!remove) {
      const auto *s = tree_->state(t.root);
      if (s && s->inside && tree_->can_process(t.root, paused) && delta > 0) {
        t.elapsed += delta;
        const float time = std::min(t.duration, t.elapsed);
        const float scale = data_->profile().squash +
                            (1 - data_->profile().squash) * (time / t.duration);
        if (!core_->native_scale(t.root, scale, false, e) ||
            !synchronize(t.root, e))
          return false;
        if (time >= t.duration) {
          if (!signals_->emit(t.property->binding().object,
                              data_->tween_signal(), {}, e) ||
              !signals_->emit(t.owner->binding().object, "step_finished",
                              {int64_t(0)}, e))
            return false;
          t.dead = true;
          if (!signals_->emit(t.owner->binding().object, data_->tween_signal(),
                              {}, e))
            return false;
        }
      }
    }
    if (remove) {
      if (t.waiter) {
        bool connected = false;
        if (!signals_->connected(
                t.owner->binding().object, data_->tween_signal(),
                t.waiter->binding().object, "_signal_callback", connected, e))
          return false;
        if (connected && !signals_->disconnect(
                             t.owner->binding().object, data_->tween_signal(),
                             t.waiter->binding().object, "_signal_callback", e))
          return false;
      }
      if (!signals_->release(t.owner->binding().object, e) ||
          !signals_->release(t.property->binding().object, e))
        return false;
      references_.erase(t.owner->binding().object);
      references_.erase(t.property->binding().object);
      if (t.waiter)
        references_.erase(t.waiter->binding().object);
      i = tweens_.erase(i);
    } else
      ++i;
    if (final)
      break;
  }
  return true;
}
bool PodunkConcreteSceneGrassFactory::release(FieldObjectId id,
                                              std::string &e) {
  auto i = nodes_.find(id);
  if (i == nodes_.end())
    return fail(e, "Grass actual native release unknown");
  if (i->second.released)
    return true;
  const auto root = i->second.root;
  auto &instance = instances_.at(root);
  if (i->second.role == GrassNativeRole::Area) {
    if (instance.monitor && !world_->static_monitor_exit(root, e))
      return false;
    if (instance.geometry && !space_->remove_player_shape(instance.nodes[1], e))
      return false;
    if (!space_->retire_player_owner(root, e))
      return false;
    if (instance.published && !core_->retire_native_instance(root, e))
      return false;
    instance.monitor = instance.geometry = instance.published = false;
  }
  i->second.released = true;
  return true;
}
bool PodunkConcreteSceneGrassFactory::owns_drawable(FieldObjectId id) const {
  auto i = nodes_.find(id);
  return i != nodes_.end() && !i->second.released &&
         i->second.role == GrassNativeRole::Sprite;
}
bool PodunkConcreteSceneGrassFactory::admit_canvas(
    FieldObjectId id, const FieldNodeDescriptor &n,
    const FieldIdentity &identity, const FieldNodeTreeRuntime &tree,
    bool &drawable, std::string &e) const {
  auto node = nodes_.find(id);
  const auto *s = tree.state(id);
  if (!data_ || &tree != tree_.get() || node == nodes_.end() ||
      node->second.released || !same(identity, data_->identity()) ||
      !data_->native_matches(n) || registry_->tree_owner(id) != tree_ ||
      !registry_->object_exists(id) || !s || !s->inside || !s->bound ||
      s->binding.family != 0x454e0071 || s->binding.capability != 1 ||
      !instances_.at(node->second.root).published)
    return fail(e, "Grass actual foreign Canvas owner/Ready/source rejected");
  drawable = owns_drawable(id);
  return true;
}
bool PodunkConcreteSceneGrassFactory::draw_leaf(
    const FieldCanvasOrderSlot &slot, const FieldTransform &viewport,
    bool pixel_snap, std::string &e) {
  if (!owns_drawable(slot.object) || !pixel_snap)
    return fail(e, "Grass actual Canvas slot/pixel source policy rejected");
  auto &node = nodes_.at(slot.object);
  FieldGrassDraw pose;
  bool physics, graph, timer;
  double left;
  FieldTransform root, world;
  FieldColor color;
  if (!core_->native_pose(node.root, pose, physics, graph, timer, left, e) ||
      !tree_->world_transform(node.root, root, e) ||
      !tree_->world_transform(slot.object, world, e) ||
      !tree_->effective_color(slot.object, color, e))
    return false;
  for (auto at = slot.object; at;) {
    auto *s = tree_->state(at);
    if (!s)
      return fail(e, "Grass actual Canvas material ancestor unavailable");
    if (s->flags & 32u)
      return fail(
          e,
          "Grass actual inherited material requires its typed shader renderer");
    if (!(s->flags & 16u))
      break;
    at = s->canvas_parent;
  }
  if (root[0].x != 1 || root[0].y != 0 || root[1].x != 0 || root[1].y != 1 ||
      viewport[0].x != 1 || viewport[0].y != 0 || viewport[1].x != 0 ||
      viewport[1].y != 1 || color != FieldColor{1, 1, 1, 1})
    return fail(e, "Grass native Canvas non-default transform/material "
                   "requires actual shader owner");
  pose.position = root[2];
  if (!renderer_.draw(pose, -viewport[2].x, -viewport[2].y))
    return fail(e, "Grass actual GPU resource draw failed");
  return true;
}
bool PodunkConcreteSceneGrassFactory::shutdown(std::string &e) {
  for (const auto &n : nodes_)
    if (!n.second.released)
      return fail(e, "Grass GPU release requires actual subtree exit/deletion "
                     "and frame fence");
  tweens_.clear();
  renderer_.free();
  return true;
}
} // namespace encore::ctr
