#include "podunk_global_native.hpp"
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
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
} // namespace
bool PodunkGlobalNative::initialize(const FieldGlobalConstructorData &d,
                                    FieldGlobalRegistry &r,
                                    PodunkNativeRoot &root, std::string &e) {
  if (data_ || !d.valid() || !r.kernel() || r.poisoned())
    return fail(e, "global native actual source registry required");
  data_ = &d;
  registry_ = &r;
  root_ = &root;
  e.clear();
  return true;
}
bool PodunkGlobalNative::construct(FieldNodeTreeRuntime &t, FieldObjectId id,
                                   const FieldNodeDescriptor &d,
                                   std::string &e) {
  if (!data_ || objects_.count(id) ||
      t.object_domain() != registry_->kernel() || !registry_->object_exists(id))
    return fail(e, "global native actual allocation missing/duplicate");
  const auto *s = t.state(id);
  const auto *actual = t.descriptor(id);
  FieldIdentity identity;
  if (!s || !s->alive || s->inside || s->ready_notified || !actual ||
      actual->id != d.id || actual->script_sha != d.script_sha ||
      !t.object_identity(id, identity))
    return fail(e, "global native constructor actual node cursor rejected");
  const FieldNodeDescriptor *source = nullptr;
  if (same(identity, data_->identity()))
    source = data_->recipe().record(d.id);
  else if (same(identity, data_->transition_identity()) &&
           d.id == data_->transition_node().id)
    source = &data_->transition_node();
  if (!source || source->native_class != d.native_class ||
      source->script != d.script || source->script_sha != d.script_sha ||
      (d.native_class != "Node" && d.native_class != "Node2D" &&
       d.native_class != "Timer"))
    return fail(e, "global native source class/identity unknown");
  // The actual typed Tree already owns ordered native transform, pause,
  // visibility and source properties. Keep an ObjectDB owner of that same node.
  Native n;
  n.source = d.id;
  n.identity = identity;
  n.native = d.native_class;
  n.last_world = s->world;
  objects_.emplace(id, std::move(n));
  e.clear();
  return true;
}
bool PodunkGlobalNative::phase(FieldNodeTreeRuntime &t, FieldObjectId id,
                               const FieldNodeBinding &b, FieldTreePhase p,
                               std::string &e) {
  auto i = objects_.find(id);
  const auto *s = t.state(id);
  const auto *d = t.descriptor(id);
  FieldIdentity identity;
  if (!data_ || registry_->poisoned() || i == objects_.end() ||
      !registry_->object_exists(id) ||
      t.object_domain() != registry_->kernel() || !s || !s->alive || !d ||
      d->id != i->second.source || d->native_class != i->second.native ||
      b.stable_id != d->id || b.native_class != d->native_class ||
      b.script_sha != d->script_sha || !t.object_identity(id, identity) ||
      !same(identity, i->second.identity))
    return fail(e, "global native actual phase owner differs");
  auto &n = i->second;
  switch (p) {
  case FieldTreePhase::EnterNative:
    if (!s->inside || n.entered)
      return fail(e, "global native enter cursor rejected");
    n.entered = true;
    n.post_entered = false;
    break;
  case FieldTreePhase::PostEnterNative:
    if (!s->inside || !n.entered || n.post_entered)
      return fail(e, "global native post-enter cursor rejected");
    n.post_entered = true;
    break;
  case FieldTreePhase::ReadyNative:
    if (!s->inside || !n.post_entered || !s->ready_notified || s->ready_first ||
        n.ready || !root_->node_notification(id, p, e))
      return fail(e, "global native Ready cursor rejected");
    n.ready = true;
    break;
  case FieldTreePhase::ExitNative:
    if (!s->inside || !n.entered)
      return fail(e, "global native exit cursor rejected");
    n.entered = false;
    n.post_entered = false;
    break;
  case FieldTreePhase::TransformChanged:
  case FieldTreePhase::LocalTransformChanged:
    if (n.native != "Node2D" || !t.world_transform(id, n.last_world, e))
      return fail(e, "global native Node2D transform owner rejected");
    for (auto v : n.last_world)
      if (!std::isfinite(v.x) || !std::isfinite(v.y))
        return fail(e, "global native Node2D nonfinite transform");
    break;
  case FieldTreePhase::VisibilityChanged:
  case FieldTreePhase::Hide:
    if (n.native != "Node2D")
      return fail(e, "global native nonCanvas visibility rejected");
    (void)t.visible_in_tree(id);
    break;
  case FieldTreePhase::Parented:
    if (!s->parent && !root_->external_parent(id))
      return fail(e, "global native actual parent absent");
    break;
  case FieldTreePhase::Unparented:
    break;
  case FieldTreePhase::TreeEntered:
    if (!s->inside || !n.entered || !emit(id, Signal::TreeEntered, 0, e))
      return false;
    break;
  case FieldTreePhase::NodeAdded:
  case FieldTreePhase::NodeRemoved:
    if (!s->inside || !root_->node_notification(id, p, e))
      return false;
    break;
  case FieldTreePhase::ChildEntered:
  case FieldTreePhase::ChildExiting: {
    if (!s->inside || !root_->node_notification(id, p, e))
      return false;
    if (s->parent && objects_.count(s->parent) &&
        !emit(s->parent,
              p == FieldTreePhase::ChildEntered ? Signal::ChildEntered
                                                : Signal::ChildExiting,
              id, e))
      return false;
    break;
  }
  case FieldTreePhase::ReadySignal:
    if (!s->inside || !n.ready || !emit(id, Signal::Ready, 0, e))
      return false;
    break;
  case FieldTreePhase::TreeExiting:
    if (!s->inside || !n.entered || !emit(id, Signal::TreeExiting, 0, e))
      return false;
    break;
  case FieldTreePhase::TreeExited:
    if (s->inside || n.entered || !emit(id, Signal::TreeExited, 0, e))
      return false;
    break;
  // Node's moved/path notifications have no extra native body for these
  // reviewed classes. The typed Tree has already changed actual names/order.
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::PathChanged:
    break;
  default:
    return fail(e,
                "global native script/timer/process phase has separate owner");
  }
  e.clear();
  return true;
}
bool PodunkGlobalNative::input_registration(FieldObjectId id, uint32_t kind,
                                            bool active, std::string &e) {
  if (!data_ || !objects_.count(id))
    return fail(e, "global native input object missing");
  return root_->input_registration(id, kind, active, e);
}
bool PodunkGlobalNative::release(FieldObjectId id, const FieldNodeBinding &b,
                                 std::string &e) {
  auto i = objects_.find(id);
  if (i == objects_.end() || b.stable_id != i->second.source ||
      i->second.entered)
    return fail(e, "global native release actual owner still entered/missing");
  objects_.erase(i);
  e.clear();
  return true;
}
bool PodunkGlobalNative::connect_signal(FieldObjectId id, Signal signal,
                                        FieldObjectId target,
                                        std::string method, bool once,
                                        std::string &e) {
  auto i = objects_.find(id);
  if (!data_ || i == objects_.end() || uint32_t(signal) < 1 ||
      uint32_t(signal) > 6 || !registry_->object_exists(target) ||
      method.empty() || method.size() > 4096 ||
      method.find('\0') != method.npos)
    return fail(e, "global native signal target/source rejected");
  for (const auto &c : i->second.connections)
    if (c.signal == signal && c.target == target && c.method == method)
      return fail(e, "global native duplicate signal connection");
  i->second.connections.push_back(
      {signal, target, std::move(method), once, ++connection_serial_});
  e.clear();
  return true;
}
bool PodunkGlobalNative::emit(FieldObjectId id, Signal signal,
                              FieldObjectId child, std::string &e) {
  auto i = objects_.find(id);
  if (i == objects_.end())
    return fail(e, "global native signal owner expired");
  // Snapshot serials, not Ref holders. Source callbacks may change connections.
  const auto connections = i->second.connections;
  for (const auto &c : connections) {
    if (c.signal != signal)
      continue;
    i = objects_.find(id);
    if (i == objects_.end())
      return fail(e, "global native signal owner deleted during emission");
    auto live =
        std::find_if(i->second.connections.begin(), i->second.connections.end(),
                     [&](const auto &x) { return x.serial == c.serial; });
    if (live == i->second.connections.end())
      continue;
    if (!registry_->object_exists(c.target)) {
      i->second.connections.erase(live);
      continue;
    }
    if (c.one_shot)
      i->second.connections.erase(live);
    FieldDeferredMessage message;
    message.object = c.target;
    message.member = c.method;
    if (child)
      message.args.push_back(FieldObjectRef{child});
    // Synchronous actual ObjectDB dispatch; never enqueue a deferred
    // substitute.
    if (!registry_->dispatch(message, e))
      return false;
  }
  e.clear();
  return true;
}
} // namespace encore::ctr
