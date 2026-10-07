#include "podunk_native_root.hpp"
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
bool zero(const std::array<uint8_t, 32> &h) {
  return std::all_of(h.begin(), h.end(), [](uint8_t v) { return !v; });
}
class NativeRootObject final : public FieldGlobalExternalObject {
  PodunkNativeRoot *owner_;
  FieldGlobalExternalBinding binding_;
  bool kernel_;

public:
  NativeRootObject(PodunkNativeRoot &o, FieldGlobalExternalBinding b,
                   bool kernel)
      : owner_(&o), binding_(std::move(b)), kernel_(kernel) {}
  FieldGlobalExternalBinding binding() const override { return binding_; }
  bool state(FieldGlobalExternalState &s, std::string &e) const override {
    return owner_->snapshot(kernel_, s, e);
  }
  bool deferred(const FieldDeferredMessage &m, std::string &e) override {
    return owner_->deferred(kernel_, m, e);
  }
  bool persist_append(FieldObjectId, std::string &e) override {
    return fail(e, "Native root does not own global.persistNodes source array");
  }
  bool assign_stable_canvas(FieldObjectId, std::string &e) override {
    return fail(
        e,
        "Native root does not own UiManager._stable_canvas_layer source field");
  }
};
} // namespace
bool PodunkNativeRoot::initialize(const FieldNativeRootData &data,
                                  const FieldGlobalRegistryData &source,
                                  FieldGlobalRegistry &registry,
                                  C3D_RenderTarget *target,
                                  PodunkUiExternalFactory *other,
                                  std::string &e) {
  if (data_ || !data.valid() || !source.valid() ||
      !same(data.identity(), source.identity()) ||
      data.root_name() != source.root_name() ||
      data.root_native() != source.root_native() ||
      data.kernel_native() != source.kernel_native() || other == this)
    return fail(e, "Native root actual source owner initialization rejected");
  data_ = &data;
  source_ = &source;
  registry_ = &registry;
  target_ = target;
  other_ = other;
  state_.canvas = {Vec2{1, 0}, Vec2{0, 1}, Vec2{0, 0}};
  e.clear();
  return true;
}
bool PodunkNativeRoot::construct(
    FieldObjectId id, const FieldGlobalExternalSpec &s,
    std::unique_ptr<FieldGlobalExternalObject> &out, std::string &e) {
  if (!data_ || !id || out || state_.failed)
    return fail(e, "Native root source constructor state rejected");
  if (s.role != 1 && s.role != 2) {
    if (!other_)
      return fail(e, "Native root missing actual source autoload constructor");
    return other_->construct(id, s, out, e);
  }
  const bool kernel = s.role == 1;
  if (!same(s.identity, source_->identity()) || s.stable_id ||
      !s.source.empty() || !s.script.empty() || !zero(s.source_sha) ||
      !zero(s.script_sha) ||
      s.native_class !=
          (kernel ? data_->kernel_native() : data_->root_native()) ||
      s.name != (kernel ? std::string{} : data_->root_name()) ||
      (kernel ? kernel_ != 0 : root_ != 0) || (!kernel && !kernel_))
    return fail(e, "Native root source native constructor binding rejected");
  FieldGlobalExternalBinding binding{id, s, 0x454e004c, 1};
  out = std::make_unique<NativeRootObject>(*this, binding, kernel);
  if (kernel)
    kernel_ = id;
  else {
    root_ = id;
    state_.handle_input_locally = (data_->flags() & 1) != 0;
    state_.audio_listener = (data_->flags() & 2) != 0;
    state_.audio_listener_2d = (data_->flags() & 4) != 0;
    // Both original _update_listener methods are commented no-op bodies;
    // the actual listener booleans remain queryable native object state.
  }
  e.clear();
  return true;
}
bool PodunkNativeRoot::snapshot(bool kernel, FieldGlobalExternalState &out,
                                std::string &e) const {
  if (!data_ || (kernel ? !kernel_ : !root_))
    return fail(e, "Native root actual constructor absent");
  out = {};
  out.name = kernel ? std::string{} : data_->root_name();
  if (kernel)
    out.current_scene = state_.current_scene;
  else {
    out.children = state_.children;
    out.inside = state_.inside;
    out.ready = state_.ready;
  }
  e.clear();
  return true;
}
bool PodunkNativeRoot::poison(const std::string &why, std::string &e) {
  state_.failed = true;
  failure_ = why.empty() ? "Native root source lifecycle failed" : why;
  e = failure_;
  return false;
}
bool PodunkNativeRoot::actual_target(std::string &e) const {
  if (!target_ || !target_->frameBuf.colorBuf || target_->frameBuf.width == 0 ||
      target_->frameBuf.height == 0)
    return fail(e, "Native Viewport actual Citro2D target absent");
  const float width =
      target_->linked ? target_->frameBuf.height : target_->frameBuf.width;
  const float height =
      target_->linked ? target_->frameBuf.width : target_->frameBuf.height;
  if (width <= 0 || height <= 0 || width > 1024 || height > 1024)
    return fail(e, "Native Viewport actual target extent unsupported");
  if (state_.initialized && (state_.size.x != width || state_.size.y != height))
    return fail(
        e, "Native Viewport target resized without actual resize notification");
  e.clear();
  return true;
}
bool PodunkNativeRoot::bind_object_signals(FieldObjectSignals &signals, std::string &e) {
  if (!data_ || signals_ || signals.registry() != registry_ || state_.inside ||
      state_.failed || !registry_->object_exists(kernel_) ||
      !registry_->object_exists(root_))
    return fail(e, "Native root shared signal ObjectDB binding rejected");
  signals_ = &signals; e.clear(); return true;
}
bool PodunkNativeRoot::signal_declaration(FieldObjectId emitter,
                                          std::string_view signal,
                                          uint32_t &arity, std::string &e) const {
  if (!data_ || state_.failed || !registry_->object_exists(emitter))
    return fail(e, "Native root signal source object unavailable");
  if (emitter == kernel_) {
    if (signal == "tree_changed") arity = 0;
    else if (signal == "node_added" || signal == "node_removed") arity = 1;
    else return fail(e, "Unknown native SceneTree signal");
  } else if (emitter == root_) {
    if (signal == "tree_entered" || signal == "tree_exiting" ||
        signal == "tree_exited" || signal == "ready" || signal == "size_changed") arity = 0;
    else if (signal == "child_entered_tree" || signal == "child_exiting_tree") arity = 1;
    else return fail(e, "Unknown native root Viewport signal");
  } else return fail(e, "Native root signal belongs to another owner");
  e.clear(); return true;
}
bool PodunkNativeRoot::connect_signal(bool kernel, std::string signal,
                                      FieldObjectId target, std::string method,
                                      std::string &e) {
  uint32_t arity = 0; const auto emitter = kernel ? kernel_ : root_;
  if (!signals_ || !signal_declaration(emitter, signal, arity, e))
    return fail(e, "Native root actual shared signal dispatcher missing");
  return signals_->connect(emitter, signal, target, method, 0, {}, e);
}
bool PodunkNativeRoot::disconnect_signal(bool kernel, std::string_view signal,
                                         FieldObjectId target, std::string_view method,
                                         std::string &e) {
  if (!signals_) return fail(e, "Native root actual shared signal dispatcher missing");
  return signals_->disconnect(kernel ? kernel_ : root_, signal, target, method, e);
}
bool PodunkNativeRoot::emit(bool kernel, std::string_view signal,
                            FieldObjectId arg, std::string &e) {
  const auto emitter = kernel ? kernel_ : root_; uint32_t arity = 0;
  if (!signal_declaration(emitter, signal, arity, e)) return false;
  if (!signals_) { e.clear(); return true; } // No connect can succeed before binding.
  std::vector<FieldDeferredValue> args;
  if (arg) args.emplace_back(FieldObjectRef{arg});
  return signals_->emit(emitter, signal, args, e);
}
bool PodunkNativeRoot::bind_parent_observer(ParentObserver observer,
                                            std::string &e) {
  if (!data_ || !observer || parent_observer_ || !state_.children.empty() ||
      state_.failed)
    return fail(e, "Native root actual parent observer binding rejected");
  parent_observer_ = std::move(observer);
  e.clear();
  return true;
}
bool PodunkNativeRoot::bind_child_notifications(ChildNotification notification,
                                                std::string &e) {
  if (!data_ || !notification || child_notification_ ||
      !state_.children.empty() || state_.failed)
    return fail(
        e, "Native root actual child source notification dispatcher rejected");
  child_notification_ = std::move(notification);
  e.clear();
  return true;
}
bool PodunkNativeRoot::stage_child(FieldObjectId id, std::string &e) {
  if (!data_ || state_.initialized || state_.inside || state_.failed ||
      state_.blocked || !registry_->object_exists(id) || id == root_ ||
      id == kernel_ || external_parent(id))
    return fail(
        e, "Native root original out-of-tree child staging boundary rejected");
  auto tree = external_.count(id) ? std::shared_ptr<FieldNodeTreeRuntime>()
                                    : registry_->tree_owner(id);
  External *external = nullptr;
  FieldGlobalExternalState prior;
  if (tree) {
    const auto *n = tree->state(id);
    if (tree->object_domain() != kernel_ || tree->root() != id || !n ||
        n->parent || n->inside || !parent_observer_ || !child_notification_)
      return fail(
          e,
          "Native root original detached source scene/parent adapter pending");
  } else if (!actual_external(id, external, prior, e) || prior.parent ||
             prior.inside || prior.ready)
    return fail(e, "Native root original autoload constructor absent");
  state_.children.push_back(id);
  if (external && (!external->lifecycle->stage_parent(root_, e) ||
                   !external->object->state(prior, e) ||
                   prior.parent != root_ || prior.inside || prior.ready))
    return poison("Native root staged actual external parent assignment failed",
                  e);
  if (tree) {
    FieldObjectId parent = 0;
    if (!parent_observer_(root_, id, e) ||
        !registry_->resolve_path(id, "..", parent, e) || parent != root_)
      return poison(
          "Native root staged actual source parent/NodePath observer pending",
          e);
  }
  if (!(tree ? child_notification_(id, FieldTreePhase::Parented, e)
             : external->lifecycle->native_notification(
                   FieldTreePhase::Parented, e)))
    return poison(e, e);
  // Root is still outside-tree. No Enter/Ready/RNG occurs here.
  e.clear();
  return true;
}
bool PodunkNativeRoot::stage_current_scene(FieldObjectId id, std::string &e) {
  if (!data_ || state_.initialized || state_.inside || state_.failed ||
      state_.current_scene || state_.blocked ||
      state_.children.size() != source_->autoloads().size())
    return fail(e, "Native original main scene staging boundary rejected");
  auto tree = registry_->tree_owner(id);
  FieldIdentity actual;
  std::array<uint8_t, 32> sha;
  const auto *node = tree ? tree->state(id) : nullptr;
  if (!tree || tree->root() != id || tree->object_domain() != kernel_ ||
      !node || !node->alive || node->inside || node->parent ||
      external_parent(id) || !tree->object_identity(id, actual) ||
      !source_->source_hash(source_->main_scene(), sha) ||
      actual.upstream_commit != data_->identity().upstream_commit ||
      actual.source_sha256 != sha)
    return fail(e, "Native original main PackedScene source binding rejected");
  // Official SceneTree::add_current_scene assigns before root.add_child.
  // Failure retains this actual assignment; no synthetic source rollback.
  state_.current_scene = id;
  if (!registry_->observe_bootstrap_tree_current_scene(id, e))
    return poison(e, e);
  if (!stage_child(id, e))
    return poison(e, e);
  e.clear();
  return true;
}
bool PodunkNativeRoot::begin_root_enter(std::string &e) {
  if (!data_ || !registry_->object_exists(kernel_) ||
      !registry_->object_exists(root_) || registry_->kernel() != kernel_ ||
      registry_->root() != root_ || state_.initialized || state_.inside ||
      state_.failed || !actual_target(e))
    return fail(e, "Native SceneTree actual init/root/GPU binding rejected");
  state_.size = {float(target_->linked ? target_->frameBuf.height
                                       : target_->frameBuf.width),
                 float(target_->linked ? target_->frameBuf.width
                                       : target_->frameBuf.height)};
  state_.initialized = true; // SceneTree::init before root->_set_tree(this).
  state_.inside = true;
  world_viewports_.emplace(root_, WorldViewport{target_});
  native_groups_["_viewports"].push_back(root_);
  state_.world_registered = true;
  state_.active = true;
  known_nodes_.insert(root_);
  // Node::_propagate_enter_tree: native body, tree_entered, node_added.
  if (!emit(false, "tree_entered", 0, e) || !emit(true, "node_added", root_, e))
    return poison(e, e);
  e.clear();
  return true;
}
bool PodunkNativeRoot::finish_root_ready(std::string &e) {
  if (!state_.inside || state_.failed || !state_.ready_notified ||
      state_.ready || state_.blocked)
    return poison("Native root postorder Ready boundary rejected", e);
  // Root Node/Viewport have no POST_ENTER_TREE body. Their actual READY body
  // enables internal physics. No 3D class is admitted by this 2D source slice.
  state_.internal_physics = data_->flags() & 8;
  if (state_.internal_physics)
    native_groups_["physics_process_internal"].push_back(root_);
  state_.ready = true;
  if (!emit(false, "ready", 0, e))
    return poison(e, e);
  // Node::_set_tree tree_changed follows the complete Enter/Ready traversal.
  if (!emit(true, "tree_changed", 0, e))
    return poison(e, e);
  e.clear();
  return true;
}
bool PodunkNativeRoot::initialize_project_tree(std::string &e) {
  if (!data_ || state_.initialized || state_.inside || state_.failed ||
      state_.blocked ||
      state_.children.size() != source_->autoloads().size() + 1 ||
      state_.current_scene != state_.children.back())
    return fail(e,
                "Native original project startup staged source roster absent");
  for (size_t i = 0; i < source_->autoloads().size(); ++i) {
    auto found = external_.find(state_.children[i]);
    FieldGlobalExternalState actual;
    External *owner = nullptr;
    if (found == external_.end() ||
        found->second.binding.source.stable_id != source_->autoloads()[i].id ||
        !actual_external(state_.children[i], owner, actual, e) ||
        actual.parent != root_ || actual.inside || actual.ready)
      return fail(e,
                  "Native startup autoload actual constructor/order differs");
  }
  auto main = registry_->tree_owner(state_.children.back());
  FieldIdentity actual;
  std::array<uint8_t, 32> sha;
  FieldObjectId parent = 0;
  if (!main || !main->object_identity(state_.children.back(), actual) ||
      !source_->source_hash(source_->main_scene(), sha) ||
      actual.upstream_commit != data_->identity().upstream_commit ||
      actual.source_sha256 != sha ||
      !registry_->resolve_path(main->root(), "..", parent, e) ||
      parent != root_)
    return fail(e, "Native startup actual main scene/source parent absent");
  if (!begin_root_enter(e))
    return false;
  ++state_.blocked; // Original root Enter traversal blocks child-list mutation.
  for (size_t i = 0; i < state_.children.size(); ++i) {
    const auto id = state_.children[i];
    auto tree = external_.count(id) ? std::shared_ptr<FieldNodeTreeRuntime>()
                                    : registry_->tree_owner(id);
    External *owner = nullptr;
    FieldGlobalExternalState observed;
    if (tree) {
      if (!tree->enter_branch_only(e) || tree->lifecycle_pending() ||
          !tree->state(id) || !tree->state(id)->inside ||
          tree->state(id)->ready_notified)
        return poison(
            e.empty() ? "Native startup actual Enter-only incomplete" : e, e);
    } else if (!actual_external(id, owner, observed, e) ||
               !owner->lifecycle->enter(root_, e) ||
               !owner->object->state(observed, e) || observed.parent != root_ ||
               !observed.inside || observed.ready)
      return poison(
          e.empty() ? "Native startup external Enter-only incomplete" : e, e);
    if (!known_nodes_.count(id) || !entered_notified_.count(id))
      return poison("Native startup Enter lacks actual kernel/parent signals",
                    e);
  }
  --state_.blocked;
  // Root ready_notified is set before its children: additions into entered
  // main-scene descendants retain each actual parent's readiness state.
  state_.ready_notified = true;
  ++state_.blocked;
  for (size_t i = 0; i < state_.children.size(); ++i) {
    const auto id = state_.children[i];
    auto tree = external_.count(id) ? std::shared_ptr<FieldNodeTreeRuntime>()
                                    : registry_->tree_owner(id);
    External *owner = nullptr;
    FieldGlobalExternalState observed;
    if (tree) {
      if (!tree->ready_entered_branch(e) || tree->lifecycle_pending() ||
          !tree->state(id) || !tree->state(id)->ready_notified)
        return poison(
            e.empty() ? "Native startup actual branch Ready incomplete" : e, e);
    } else if (!actual_external(id, owner, observed, e) ||
               !owner->lifecycle->ready(e) ||
               !owner->object->state(observed, e) || !observed.inside ||
               observed.parent != root_ || !observed.ready)
      return poison(
          e.empty() ? "Native startup external source Ready incomplete" : e, e);
  }
  --state_.blocked;
  return finish_root_ready(e);
}
bool PodunkNativeRoot::initialize_tree(std::string &e) {
  if (!state_.children.empty())
    return fail(
        e, "Native empty-root init cannot replace original project startup");
  if (!begin_root_enter(e))
    return false;
  state_.ready_notified = true;
  return finish_root_ready(e);
}
bool PodunkNativeRoot::register_external_child(
    FieldGlobalExternalObject &object, PodunkExternalNodeLifecycle &lifecycle,
    std::string &e) {
  auto b = object.binding();
  FieldGlobalExternalState state;
  if (!data_ || !b.object || b.object == kernel_ || b.object == root_ ||
      b.source.role != 3 ||
      b.source.identity.upstream_commit != data_->identity().upstream_commit ||
      !b.family || !b.capability || external_.count(b.object) ||
      !object.state(state, e) || state.name != b.source.name || state.parent ||
      state.inside || state.ready)
    return fail(e, "Native root actual external source constructor rejected");
  auto a =
      std::find_if(source_->autoloads().begin(), source_->autoloads().end(),
                   [&](const auto &x) { return x.id == b.source.stable_id; });
  if (a == source_->autoloads().end() || a->name != b.source.name ||
      a->path != b.source.source || a->native_class != b.source.native_class ||
      a->script != b.source.script || a->script_sha != b.source.script_sha ||
      a->source_sha != b.source.source_sha)
    return fail(
        e, "Native root external child original autoload binding rejected");
  auto tree = registry_->tree_owner(b.object);
  if (!state.children.empty() || tree) {
    FieldIdentity identity;
    const auto *n = tree ? tree->state(b.object) : nullptr;
    const auto *d = tree ? tree->descriptor(b.object) : nullptr;
    const bool global_branch=b.source.stable_id==source_->global_autoload()&&b.family==0x454e0055&&b.capability==1;
    const bool named_audio_branch=b.family==0x454e0073&&b.capability==1&&tree&&tree->object_count()==4;
    if ((!global_branch&&!named_audio_branch) || !tree ||
        tree->root() != b.object || tree->object_domain() != kernel_ ||
        !n || !d || !tree->object_identity(b.object, identity) ||
        identity.upstream_commit != b.source.identity.upstream_commit ||
        identity.source_sha256 != b.source.source_sha ||
        d->native_class != b.source.native_class ||
        d->script != b.source.script || d->script_sha != b.source.script_sha ||
        n->name != state.name || n->parent || n->inside ||
        n->ready_notified || n->children != state.children)
      return fail(e, "Native root actual global constructor branch differs");
    std::vector<FieldObjectId> pending{b.object};
    std::set<FieldObjectId> seen;
    for (size_t at = 0; at < pending.size(); ++at) {
      const auto id = pending[at];const auto *actual = tree->state(id);
      FieldIdentity original;
      if (!seen.insert(id).second || registry_->tree_owner(id) != tree ||
          !actual || actual->inside || actual->ready_notified ||
          !tree->object_identity(id, original) || !same(original, identity))
        return fail(e, "Native root global constructor foreign/entered child");
      for (auto child : actual->children) {
        const auto *c = tree->state(child);
        if (!c || c->parent != id)
          return fail(e, "Native root global constructor child parent differs");
        pending.push_back(child);
      }
    }
  }
  external_.emplace(b.object, External{&object, &lifecycle, b});
  e.clear();
  return true;
}
bool PodunkNativeRoot::actual_external(FieldObjectId id, External *&out,
                                       FieldGlobalExternalState &state,
                                       std::string &e) {
  auto i = external_.find(id);
  if (i == external_.end() || !registry_->object_exists(id) ||
      !i->second.object || !i->second.lifecycle)
    return fail(e, "Native root actual external child owner absent");
  auto b = i->second.object->binding();
  if (b.object != id || b.family != i->second.binding.family ||
      b.capability != i->second.binding.capability ||
      !same(b.source.identity, i->second.binding.source.identity) ||
      b.source.stable_id != i->second.binding.source.stable_id ||
      b.source.script_sha != i->second.binding.source.script_sha ||
      !i->second.object->state(state, e))
    return fail(e, "Native root external child owner changed");
  out = &i->second;
  e.clear();
  return true;
}
bool PodunkNativeRoot::add_child(FieldObjectId id, std::string &e) {
  return attach_child(id, false, e);
}
bool PodunkNativeRoot::add_continuation_child(FieldObjectId id,
                                             std::string &e) {
  return attach_child(id, true, e);
}
bool PodunkNativeRoot::attach_child(FieldObjectId id, bool continuation,
                                    std::string &e) {
  if (!state_.inside || state_.failed || state_.blocked ||
      !registry_->object_exists(id) || id == root_ || id == kernel_ ||
      std::find(state_.children.begin(), state_.children.end(), id) !=
          state_.children.end())
    return fail(e, "Native root actual add_child source boundary rejected");
  auto tree = external_.count(id) ? std::shared_ptr<FieldNodeTreeRuntime>()
                                    : registry_->tree_owner(id);
  External *external = nullptr;
  FieldGlobalExternalState prior;
  if (continuation && (tree || !state_.ready_notified ||
                       !external_.count(id)))
    return fail(e, "Native continuation requires an external session owner and ready root");
  if (tree) {
    auto *s = tree->state(id);
    if (tree->object_domain() != kernel_ || tree->root() != id || !s ||
        s->parent || s->inside)
      return fail(e, "Native root original detached scene root rejected");
  } else if (!actual_external(id, external, prior, e) || prior.parent ||
             prior.inside)
    return fail(e, "Native root detached external child state rejected");
  if (tree && (!parent_observer_ || !child_notification_))
    return fail(
        e, "Native root pre-Ready actual Registry parent observer pending");
  state_.children.push_back(id);
  if (tree) {
    FieldObjectId parent = 0;
    if (!parent_observer_(root_, id, e) ||
        !registry_->resolve_path(id, "..", parent, e) || parent != root_)
      return poison(
          "Native root actual source parent/NodePath insertion not observed",
          e);
  }
  if (external && (!external->lifecycle->stage_parent(root_, e) ||
                   !external->object->state(prior, e) ||
                   prior.parent != root_ || prior.inside))
    return poison("Native root actual external parent insertion failed", e);
  // Keep actual structures on failure. Source side effects are not rolled back
  // and cannot be replayed in this session after unknown Ready.
  if (!(tree ? child_notification_(id, FieldTreePhase::Parented, e)
             : external->lifecycle->native_notification(
                   FieldTreePhase::Parented, e)))
    return poison(e, e);
  bool entered = tree ? (state_.ready_notified ? tree->enter(e)
                                               : tree->enter_branch_only(e))
                      : external->lifecycle->enter(root_, e);
  if (!entered)
    return poison(e, e);
  if (external) {
    FieldGlobalExternalState now;
    if (!external->object->state(now, e) || now.parent != root_ ||
        !now.inside || !known_nodes_.count(id) ||
        !entered_notified_.count(id) ||
        (state_.ready_notified &&
         !(continuation ? external->lifecycle->adopt_continuation_ready(e)
                        : external->lifecycle->ready(e))))
      return poison(e, e);
    if (!external->object->state(now, e) ||
        (continuation
             ? (now.ready || !external->lifecycle->continuation_native_ready())
             : now.ready != state_.ready_notified))
      return poison("Native root external child did not execute source Ready",
                    e);
  } else {
    const auto *s = tree->state(id);
    if (!s || !s->inside || s->ready_notified != state_.ready_notified ||
        tree->lifecycle_pending())
      return poison("Native root actual scene Ready unfinished", e);
  }
  if (!known_nodes_.count(id) || !entered_notified_.count(id))
    return poison(
        "Native root actual pre-Ready kernel/parent notifications not observed",
        e);
  if (!emit(true, "tree_changed", 0, e))
    return poison(e, e);
  e.clear();
  return true;
}
bool PodunkNativeRoot::remove_child(FieldObjectId id, std::string &e) {
  auto position = std::find(state_.children.begin(), state_.children.end(), id);
  if (!state_.inside || state_.failed || state_.blocked ||
      position == state_.children.end())
    return fail(e, "Native root remove_child actual source parent rejected");

  auto tree = external_.count(id) ? std::shared_ptr<FieldNodeTreeRuntime>()
                                    : registry_->tree_owner(id);
  if (tree) {
    if (!tree->exit(e))
      return poison(e, e);
    auto *s = tree->state(id);
    if (!s || s->inside || tree->lifecycle_pending())
      return poison("Native root actual source exit unfinished", e);
  } else {
    External *owner = nullptr;
    FieldGlobalExternalState state;
    if (!actual_external(id, owner, state, e) || state.parent != root_ ||
        !state.inside || !owner->lifecycle->exit(e) ||
        !owner->object->state(state, e) || state.parent != root_ ||
        state.inside)
      return poison(e, e);
  }
  if (tree) {
    if (!child_notification_ ||
        !child_notification_(id, FieldTreePhase::Unparented, e))
      return poison(e, e);
  } else {
    External *owner = nullptr;
    FieldGlobalExternalState actual;
    if (!actual_external(id, owner, actual, e) ||
        !owner->lifecycle->native_notification(FieldTreePhase::Unparented, e))
      return poison(e, e);
  }
  if (known_nodes_.count(id) || !exited_notified_.count(id))
    return poison("Native root actual child exit kernel/parent notifications "
                  "not observed",
                  e);
  entered_notified_.erase(id);
  exited_notified_.erase(id);
  state_.children.erase(position);
  if (!tree) {
    External *owner = nullptr;
    FieldGlobalExternalState actual;
    if (!actual_external(id, owner, actual, e) ||
        !owner->lifecycle->stage_parent(0, e) ||
        !owner->object->state(actual, e) || actual.parent || actual.inside)
      return poison("Native root external source parent removal unfinished", e);
  }
  if (tree) {
    FieldObjectId parent = 0;
    std::string lookup_error;
    if (!parent_observer_ || !parent_observer_(0, id, e) ||
        registry_->resolve_path(id, "..", parent, lookup_error))
      return poison("Native root actual source parent removal not observed", e);
  }
  if (state_.current_scene == id)
    state_.current_scene = 0;
  for (auto &g : input_)
    g.erase(id);
  if (!emit(true, "tree_changed", 0, e))
    return poison(e, e);
  e.clear();
  return true;
}
bool PodunkNativeRoot::move_child(FieldObjectId id, int32_t index,
                                  std::string &e) {
  if (!state_.inside || state_.failed || state_.blocked)
    return fail(e, "Native root source move_child outside Tree/blocked");
  auto i = std::find(state_.children.begin(), state_.children.end(), id);
  if (i == state_.children.end() || index < 0 ||
      uint64_t(index) > state_.children.size())
    return fail(e, "Native root source move_child index rejected");
  size_t at = size_t(index);
  if (at == state_.children.size())
    --at;
  if (size_t(i - state_.children.begin()) == at) {
    e.clear();
    return true;
  }
  auto tree = external_.count(id) ? std::shared_ptr<FieldNodeTreeRuntime>()
                                    : registry_->tree_owner(id);
  External *owner = nullptr;
  FieldGlobalExternalState actual;
  if (tree && !child_notification_)
    return fail(e, "Native root move_child actual native notification pending");
  if (!tree && !actual_external(id, owner, actual, e))
    return false;
  state_.children.erase(i);
  state_.children.insert(state_.children.begin() + at, id);
  if (!emit(true, "tree_changed", 0, e) ||
      (tree ? !child_notification_(id, FieldTreePhase::ChildMoved, e)
            : !owner->lifecycle->native_notification(FieldTreePhase::ChildMoved,
                                                     e)))
    return poison(e, e);
  e.clear();
  return true;
}
FieldObjectId PodunkNativeRoot::external_parent(FieldObjectId id) const {
  return std::find(state_.children.begin(), state_.children.end(), id) !=
                 state_.children.end()
             ? root_
             : 0;
}
bool PodunkNativeRoot::node_notification(FieldObjectId id, FieldTreePhase phase,
                                         std::string &e) {
  if (!state_.inside || state_.failed || !registry_->object_exists(id) ||
      id == root_ || id == kernel_)
    return fail(e, "Native root actual node notification receiver rejected");
  auto tree = registry_->tree_owner(id);
  if (tree) {
    const auto *n = tree->state(id);
    if (tree->object_domain() != kernel_ || !n || !n->alive)
      return fail(e, "Native root source notification foreign Tree");
  } else if (!external_.count(id))
    return fail(e,
                "Native root source notification unregistered external owner");
  if (phase == FieldTreePhase::ReadyNative ||
      phase == FieldTreePhase::ReadyScript) {
    FieldObjectId branch = id;
    if (tree) {
      const FieldNodeState *node = tree->state(branch);
      while (node && node->parent) {
        branch = node->parent;
        node = tree->state(branch);
      }
    }
    if (!external_parent(branch) || !known_nodes_.count(branch) ||
        !entered_notified_.count(branch))
      return fail(e, "Native root source Ready precedes actual parent/kernel "
                     "enter notifications");
    e.clear();
    return true; // boundary only; caller must execute actual typed Ready body.
  }
  if (phase == FieldTreePhase::NodeAdded) {
    if (!known_nodes_.insert(id).second)
      return fail(e, "Native kernel duplicate node_added");
    if (!emit(true, "node_added", id, e))
      return poison(e, e);
  } else if (phase == FieldTreePhase::NodeRemoved) {
    if (!known_nodes_.erase(id))
      return fail(e, "Native kernel node_removed without node_added");
    if (state_.current_scene == id)
      state_.current_scene = 0;
    if (!emit(true, "node_removed", id, e))
      return poison(e, e);
  } else if (phase == FieldTreePhase::ChildEntered) {
    if (external_parent(id)) {
      if (!entered_notified_.insert(id).second)
        return fail(e, "Native root duplicate child_entered_tree");
      exited_notified_.erase(id);
      if (!emit(false, "child_entered_tree", id, e))
        return poison(e, e);
    }
  } else if (phase == FieldTreePhase::ChildExiting) {
    if (external_parent(id)) {
      if (!entered_notified_.count(id) || !exited_notified_.insert(id).second)
        return fail(e, "Native root child_exiting_tree source order rejected");
      if (!emit(false, "child_exiting_tree", id, e))
        return poison(e, e);
    }
  } else
    return fail(
        e, "Native kernel source notification requires specific typed phase");
  e.clear();
  return true;
}
bool PodunkNativeRoot::set_current_scene(FieldObjectId id, std::string &e) {
  if (!state_.initialized || state_.failed)
    return fail(e, "Native SceneTree current_scene before actual init");
  if (id && (std::find(state_.children.begin(), state_.children.end(), id) ==
                 state_.children.end() ||
             !registry_->tree_owner(id)))
    return fail(e, "Native SceneTree current_scene must be actual root child");
  state_.current_scene = id;
  e.clear();
  return true;
}
bool PodunkNativeRoot::set_canvas_transform(const FieldTransform &t,
                                            std::string &e) {
  if (!state_.inside || state_.failed || !actual_target(e))
    return fail(e, "Native Viewport actual canvas unavailable");
  for (const auto &v : t)
    if (!std::isfinite(v.x) || !std::isfinite(v.y))
      return fail(e, "Native Viewport canvas nonfinite");
  float determinant = t[0].x * t[1].y - t[0].y * t[1].x;
  if (!std::isfinite(determinant) || determinant == 0)
    return fail(e, "Native Viewport canvas inverse unsupported");
  state_.canvas = t;
  e.clear();
  return true;
}
bool PodunkNativeRoot::world_rect(FieldMapRect &out, std::string &e) const {
  auto registered = world_viewports_.find(root_);
  if (!state_.inside || state_.failed || registered == world_viewports_.end() ||
      registered->second.target != target_ || !actual_target(e))
    return fail(e, "Native World2D actual viewport registration absent");
  const auto &t = state_.canvas;
  const float det = t[0].x * t[1].y - t[0].y * t[1].x;
  if (!std::isfinite(det) || det == 0)
    return fail(e, "Native World2D viewport inverse nonfinite");
  FieldMapRect candidate{};
  bool first = true;
  for (const Vec2 point : {Vec2{0, 0}, Vec2{state_.size.x, 0},
                           Vec2{0, state_.size.y}, state_.size}) {
    const float x = point.x - t[2].x, y = point.y - t[2].y;
    Vec2 world{(t[1].y * x - t[1].x * y) / det,
               (-t[0].y * x + t[0].x * y) / det};
    if (!std::isfinite(world.x) || !std::isfinite(world.y))
      return fail(e, "Native World2D viewport rectangle overflow");
    if (first) {
      candidate = {world, world};
      first = false;
    } else {
      candidate.minimum.x = std::min(candidate.minimum.x, world.x);
      candidate.minimum.y = std::min(candidate.minimum.y, world.y);
      candidate.maximum.x = std::max(candidate.maximum.x, world.x);
      candidate.maximum.y = std::max(candidate.maximum.y, world.y);
    }
  }
  out = candidate;
  e.clear();
  return true;
}
bool PodunkNativeRoot::native_group(std::string_view name,
                                    std::vector<FieldObjectId> &out,
                                    std::string &e) const {
  if (!state_.inside || state_.failed)
    return fail(e, "Native kernel group outside initialized source Tree");
  auto found = native_groups_.find(std::string(name));
  if (found == native_groups_.end())
    return fail(e, "Native kernel group not owned by root consumer");
  out = found->second;
  e.clear();
  return true;
}
bool PodunkNativeRoot::begin_draw(std::string &e) {
  if (!state_.inside || !state_.ready || !state_.active || state_.failed ||
      !actual_target(e))
    return fail(e, "Native Viewport actual GPU draw outside active Tree");
  C2D_Flush();
  if (!C3D_FrameDrawOn(target_))
    return fail(e, "Native Viewport GPU frame is not active");
  C2D_SceneTarget(target_);
  C3D_Mtx view;
  Mtx_Identity(&view);
  view.r[0].x = state_.canvas[0].x;
  view.r[0].y = state_.canvas[1].x;
  view.r[0].w = state_.canvas[2].x;
  view.r[1].x = state_.canvas[0].y;
  view.r[1].y = state_.canvas[1].y;
  view.r[1].w = state_.canvas[2].y;
  C2D_ViewRestore(&view);
  e.clear();
  return true;
}
bool PodunkNativeRoot::clear_target(std::string &e) {
  if (!state_.inside || state_.failed || !actual_target(e))
    return fail(e, "Native Viewport actual target clear unavailable");
  const auto &c = data_->clear_color();
  C2D_TargetClear(target_, C2D_Color32f(c[0], c[1], c[2], c[3]));
  e.clear();
  return true;
}
bool PodunkNativeRoot::input_registration(FieldObjectId id, uint32_t kind,
                                          bool enabled, std::string &e) {
  auto tree = registry_ ? registry_->tree_owner(id) : nullptr;
  auto *s = tree ? tree->state(id) : nullptr;
  if (kind >= input_.size() || state_.failed || !tree ||
      tree->object_domain() != kernel_ || !s || !s->alive ||
      (enabled && (!state_.inside || !s->inside)))
    return fail(e, "Native Viewport input actual source object rejected");
  if (enabled)
    input_[kind].insert(id);
  else
    input_[kind].erase(id);
  e.clear();
  return true;
}
bool PodunkNativeRoot::input_objects(uint32_t kind,
                                     std::vector<FieldObjectId> &out,
                                     std::string &e) const {
  if (kind >= input_.size() || !state_.inside || state_.failed)
    return fail(e, "Native Viewport input group source state rejected");
  std::vector<FieldObjectId> order;
  for (auto root : state_.children) {
    auto tree = registry_->tree_owner(root);
    if (!tree)
      continue;
    std::vector<FieldObjectId> stack{root};
    std::set<FieldObjectId> seen;
    while (!stack.empty()) {
      auto id = stack.back();
      stack.pop_back();
      auto *s = tree->state(id);
      if (!s || !s->alive || !s->inside || !seen.insert(id).second)
        return fail(e, "Native Viewport actual input tree order rejected");
      if (input_[kind].count(id)) {
        if (!s->input_enabled[kind])
          return fail(e, "Native Viewport input registration differs from "
                         "actual native Node");
        order.push_back(id);
      }
      for (auto i = s->children.rbegin(); i != s->children.rend(); ++i)
        stack.push_back(*i);
    }
  }
  if (order.size() != input_[kind].size())
    return fail(
        e,
        "Native Viewport registered input object has no actual root ancestry");
  out = std::move(order);
  e.clear();
  return true;
}
bool PodunkNativeRoot::enqueue(FieldDeferredMessage m, std::string &e) {
  if (!registry_ || state_.failed)
    return fail(e, "Native SceneTree global queue unavailable");
  return registry_->enqueue(std::move(m), e);
}
bool PodunkNativeRoot::deferred(bool kernel, const FieldDeferredMessage &m,
                                std::string &e) {
  if (m.object != (kernel ? kernel_ : root_) || state_.failed)
    return fail(e, "Native root deferred actual ObjectID rejected");
  if (kernel &&
      ((m.kind == FieldDeferredKind::Set && m.member == "current_scene") ||
       (m.kind == FieldDeferredKind::Call &&
        m.member == "set_current_scene"))) {
    if (m.args.size() != 1)
      return fail(e, "Native SceneTree current_scene signature rejected");
    if (std::holds_alternative<std::monostate>(m.args[0]))
      return set_current_scene(0, e);
    auto *v = std::get_if<FieldObjectRef>(&m.args[0]);
    return v ? set_current_scene(v->id, e)
             : fail(e, "Native SceneTree current_scene type rejected");
  }
  if (!kernel && m.kind == FieldDeferredKind::Call &&
      (m.member == "add_child" || m.member == "remove_child" ||
       m.member == "move_child")) {
    if (m.args.empty() || !std::holds_alternative<FieldObjectRef>(m.args[0]))
      return fail(e, "Native Viewport child signature rejected");
    auto child = std::get<FieldObjectRef>(m.args[0]).id;
    if (m.member == "move_child") {
      if (m.args.size() != 2 || !std::holds_alternative<int64_t>(m.args[1]))
        return fail(e, "Native Viewport move_child signature rejected");
      auto at = std::get<int64_t>(m.args[1]);
      if (at < std::numeric_limits<int32_t>::min() ||
          at > std::numeric_limits<int32_t>::max())
        return fail(e, "Native Viewport child index overflow");
      return move_child(child, int32_t(at), e);
    }
    if (m.args.size() != 1)
      return fail(e,
                  "Native Viewport child optional readable name unsupported");
    return m.member == "add_child" ? add_child(child, e)
                                   : remove_child(child, e);
  }
  return fail(e, "Native SceneTree/Viewport method, property or notification "
                 "lacks actual consumer");
}
bool PodunkNativeRoot::finalize_tree(std::string &e) {
  if (!state_.inside || state_.failed)
    return fail(e, "Native SceneTree actual finalization boundary rejected");
  while (!state_.children.empty())
    if (!remove_child(state_.children.back(), e))
      return false;
  if (!emit(false, "tree_exiting", 0, e))
    return poison(e, e);
  world_viewports_.erase(root_);
  native_groups_.clear();
  known_nodes_.erase(root_);
  state_.inside = false;
  state_.ready = false;
  state_.ready_notified = false;
  state_.active = false;
  state_.world_registered = false;
  state_.internal_physics = false;
  state_.current_scene = 0;
  for (auto &g : input_)
    g.clear();
  if (!emit(true, "node_removed", root_, e) ||
      !emit(false, "tree_exited", 0, e))
    return poison(e, e);
  e.clear();
  return true;
}
std::vector<std::string> PodunkNativeRoot::pending_native() const {
  return {
      "Actual source autoload/script constructors and child Ready are "
      "separately owned",
      "Viewport internal physics GUI/tooltips/object-picking consumer pending",
      "SceneTree pause native notification dispatcher pending",
      "3D node, camera, listener and rendering consumers not admitted",
      "Original project startup still requires every actual typed autoload "
      "constructor/Enter/Ready and exact main PackedScene body"};
}
} // namespace encore::ctr
