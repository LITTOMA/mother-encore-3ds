#include "podunk_npc_return_timer.hpp"
#include <algorithm>
#include <cmath>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *m) {
  e = m;
  return false;
}
} // namespace
bool PodunkNpcReturnTimers::prepare(
    const FieldNativeRootData &native, const FieldNpcWorldData &data,
    const FieldNpcData &npcs, FieldGlobalRegistry &registry,
    FieldObjectSignals &signals, FieldNodeTreeRuntime &tree,
    FieldNpcRuntime &runtime, PodunkSceneNpcWorld &world, std::string &e) {
  if (data_ || !data.valid() || !npcs.valid() || !native.valid() ||
      data.identity().upstream_commit != npcs.source_pin() ||
      signals.registry() != &registry)
    return fail(e, "NPC ReturnDirection native/source owners unavailable");
  const FieldNpcWorldCallback *binding = nullptr;
  for (const auto &v : data.callbacks())
    if (v.op == 2)
      binding = &v;
  if (!binding || binding->arity || binding->method.empty() ||
      !timers_.initialize(native, registry, signals, e))
    return false;
  data_ = &data;
  npcs_ = &npcs;
  registry_ = &registry;
  signals_ = &signals;
  tree_ = &tree;
  runtime_ = &runtime;
  world_ = &world;
  method_ = binding->method;
  e.clear();
  return true;
}
bool PodunkNpcReturnTimers::create(FieldObjectId object, double seconds,
                                   uint64_t waiter, std::string &e) {
  auto d = tree_ ? tree_->descriptor(object) : nullptr;
  FieldIdentity actual{};
  const FieldNpcInstance *body = nullptr;
  if (d && runtime_->data() == npcs_)
    for (const auto &v : runtime_->npcs())
      if (v.id == d->id && !v.destroyed)
        body = &v;
  if (!data_ || !d || !body || !body->ready || !waiter ||
      !std::isfinite(seconds) ||
      seconds != npcs_->parameter(FieldNpcParameter::ReturnDelay) ||
      !data_->npc(d->id) || registry_->tree_owner(object).get() != tree_ ||
      !tree_->object_identity(object, actual) ||
      actual.scene_id != data_->identity().scene_id ||
      actual.upstream_commit != data_->identity().upstream_commit ||
      actual.source_sha256 != data_->identity().source_sha256 ||
      std::find(body->return_waiters.begin(), body->return_waiters.end(),
                waiter) == body->return_waiters.end() ||
      !world_->native_ready(object, e))
    return fail(e, "NPC ReturnDirection requires actual source waiter/body");
  for (const auto &entry : waiting_)
    if (entry.second.target == object && entry.second.waiter == waiter &&
        !entry.second.timer.expired())
      return fail(e, "NPC duplicate source SceneTreeTimer waiter");
  std::shared_ptr<FieldSceneTreeTimer> timer;
  // Original source omits the second argument: native default
  // process_pause=true.
  if (!timers_.create_timer(float(seconds), true, timer, e))
    return false;
  const auto id = timer->binding().object;
  if (!signals_->connect(id, FieldSceneTreeTimers::timeout_signal(), object,
                         method_, 0, {}, e))
    return false;
  waiting_.emplace(id, Waiting{object, d->id, waiter, timer, false});
  e.clear();
  return true;
}
bool PodunkNpcReturnTimers::idle(uint64_t epoch, float dt, bool paused,
                                 std::string &e) {
  if (!data_ || !timers_.idle(epoch, dt, paused, e))
    return false;
  for (auto i = waiting_.begin(); i != waiting_.end();)
    if (i->second.timer.expired())
      i = waiting_.erase(i);
    else
      ++i;
  e.clear();
  return true;
}
bool PodunkNpcReturnTimers::handles_callback(
    const FieldDeferredMessage &m) const {
  if (!data_ || m.kind != FieldDeferredKind::Call || m.member != method_)
    return false;
  const auto *d = tree_->descriptor(m.object);
  return d && data_->npc(d->id);
}
bool PodunkNpcReturnTimers::deferred(const FieldDeferredMessage &m,
                                     std::string &e) {
  auto at = waiting_.find(timers_.emitting());
  auto timer = at == waiting_.end() ? nullptr : at->second.timer.lock();
  if (!handles_callback(m) || !m.args.empty() || !timer ||
      at->second.returned || at->second.target != m.object ||
      timer->time_left() >= 0 || !registry_->object_exists(m.object))
    return fail(e,
                "NPC return method requires actual synchronous timer signal");
  if (!world_->return_direction_timeout(at->second.source, at->second.waiter,
                                        e))
    return false;
  at->second.returned = true;
  e.clear();
  return true;
}
bool PodunkNpcReturnTimers::signal_declaration(FieldObjectId id,
                                               std::string_view name,
                                               uint32_t &arity,
                                               std::string &e) const {
  return timers_.signal_declaration(id, name, arity, e);
}
bool PodunkNpcReturnTimers::owns(FieldObjectId id) const {
  return timers_.owns(id);
}
bool PodunkNpcReturnTimers::shutdown(std::string &e) {
  if (!timers_.shutdown(e))
    return false;
  waiting_.clear();
  e.clear();
  return true;
}
} // namespace encore::ctr
