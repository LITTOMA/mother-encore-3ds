#include "podunk_scene_timers.hpp"
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
bool equal(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
} // namespace
bool PodunkSceneTimers::prepare(const FieldNativeTimerData &d,
                                const FieldNodeTreeData &s,
                                FieldNodeTreeRuntime &t, FieldGlobalRegistry &r,
                                FieldObjectSignals &signals, std::string &e) {
  if (data_ || !d.valid() || !s.valid() || t.object_domain() != r.kernel() ||
      signals.registry() != &r || r.poisoned())
    return fail(e, "Scene Timer checked sources/actual owners rejected");
  for (const auto &n : s.records()) {
    if (n.native_class != "Timer")
      continue;
    const auto *v = d.record(s.identity(), n.id);
    if (!v || v->script_sha != n.script_sha || !n.script.empty())
      return fail(e, "Scene Timer source property/script owner differs");
  }
  if (!timers_.initialize(
          d,
          [&signals](FieldObjectId id, std::string &x) {
            return signals.emit(id, "timeout", {}, x);
          },
          [&r](FieldObjectId id) { return r.tree_owner(id).get(); }, e))
    return false;
  data_ = &d;
  source_ = &s;
  tree_ = &t;
  registry_ = &r;
  signals_ = &signals;
  return true;
}
bool PodunkSceneTimers::actual(FieldObjectId id, std::string &e) const {
  FieldIdentity identity;
  const auto *n = tree_ ? tree_->descriptor(id) : nullptr;
  const auto *s = tree_ ? tree_->state(id) : nullptr;
  const auto i = instances_.find(id);
  if (!data_ || i == instances_.end() || !n || !s || !s->alive ||
      registry_->poisoned() || !registry_->object_exists(id) ||
      registry_->tree_owner(id).get() != tree_ ||
      !tree_->object_identity(id, identity) ||
      !equal(identity, source_->identity()) || n->native_class != "Timer" ||
      !n->script.empty() || n->id != i->second.binding.stable_id ||
      !data_->record(identity, n->id))
    return fail(e, "Scene Timer actual live ObjectDB/source binding rejected");
  return true;
}
bool PodunkSceneTimers::construct(FieldObjectId id,
                                  const FieldNodeDescriptor &n,
                                  const FieldIdentity &identity,
                                  std::string &e) {
  if (!data_ || instances_.count(id) || !equal(identity, source_->identity()) ||
      !tree_->descriptor(id) || tree_->descriptor(id)->id != n.id ||
      tree_->descriptor(id)->script_sha != n.script_sha ||
      n.native_class != "Timer" || !n.script.empty() ||
      registry_->tree_owner(id).get() != tree_ || !registry_->object_exists(id))
    return fail(e, "Scene Timer native construction owner rejected");
  Instance instance;
  if (!timers_.attach(*tree_, id, instance.binding, e))
    return false;
  instances_.emplace(id, std::move(instance));
  return true;
}
bool PodunkSceneTimers::bind(FieldObjectId id, FieldNodeBinding &out,
                             std::string &e) const {
  if (!actual(id, e))
    return false;
  out = instances_.at(id).binding;
  return true;
}
bool PodunkSceneTimers::finish_factory(std::string &e) const {
  if (!source_)
    return fail(e, "Scene Timer factory is unprepared");
  for (const auto &n : source_->records()) {
    if (n.native_class != "Timer")
      continue;
    const auto i =
        std::find_if(instances_.begin(), instances_.end(), [&n](const auto &v) {
          return v.second.binding.stable_id == n.id;
        });
    if (i == instances_.end() || !actual(i->first, e))
      return fail(e, "Scene Timer full native source factory is incomplete");
  }
  return true;
}
bool PodunkSceneTimers::owns(FieldObjectId id) const {
  return instances_.count(id) != 0;
}
bool PodunkSceneTimers::phase(FieldObjectId id, FieldTreePhase p, float delta,
                              bool paused, std::string &e) {
  if (!actual(id, e))
    return false;
  auto &i = instances_.at(id);
  const auto *s = tree_->state(id);
  switch (p) {
  case FieldTreePhase::EnterNative:
    if (!s->inside || i.entered)
      return fail(e, "Scene Timer Enter state differs");
    i.entered = true;
    return true;
  case FieldTreePhase::ReadyNative:
    if (!i.entered || !s->inside)
      return fail(e, "Scene Timer Ready outside tree");
    if (!timers_.ready(id, e))
      return false;
    i.ready = true;
    return true;
  case FieldTreePhase::IdleInternal:
  case FieldTreePhase::PhysicsInternal:
    if (!i.entered || !i.ready || !s->inside)
      return fail(e, "Scene Timer internal process before native Ready");
    return timers_.process(id, p, delta, paused, e);
  case FieldTreePhase::ExitNative:
    if (!i.entered)
      return fail(e, "Scene Timer Exit before native Enter");
    // Timer has no native Exit callback resetting time_left. Keep its source
    // state across ordinary subtree migration; Tree controls process
    // eligibility.
    i.entered = false;
    return true;
  case FieldTreePhase::Deleting:
    if (i.entered)
      return fail(e, "Scene Timer deletion before actual Exit");
    if (!timers_.release(id, e))
      return false;
    instances_.erase(id);
    return true;
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::PathChanged:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
    return true; // These notifications have no Timer subclass callback.
  default:
    return fail(e, "Scene Timer unexpected source/script notification");
  }
}
bool PodunkSceneTimers::start(FieldObjectId id, float v, std::string &e) {
  return actual(id, e) && timers_.start(id, v, e);
}
bool PodunkSceneTimers::stop(FieldObjectId id, std::string &e) {
  return actual(id, e) && timers_.stop(id, e);
}
bool PodunkSceneTimers::set_wait(FieldObjectId id, float v, std::string &e) {
  return actual(id, e) && timers_.set_wait(id, v, e);
}
bool PodunkSceneTimers::time_left(FieldObjectId id, float &out,
                                  std::string &e) const {
  if (!actual(id, e))
    return false;
  out = timers_.time_left(id);
  return true;
}
const FieldNativeTimerState *PodunkSceneTimers::state(FieldObjectId id) const {
  return instances_.count(id) ? timers_.state(id) : nullptr;
}
bool PodunkSceneTimers::deferred(const FieldDeferredMessage &m,
                                 std::string &e) {
  if (!actual(m.object, e))
    return false;
  auto number = [&](float &out) {
    if (m.args.size() != 1)
      return false;
    double value = 0;
    if (const auto *v = std::get_if<double>(&m.args.front()))
      value = *v;
    else if (const auto *v = std::get_if<int64_t>(&m.args.front()))
      value = double(*v);
    else
      return false;
    if (!std::isfinite(value) ||
        std::abs(value) > double(std::numeric_limits<float>::max()))
      return false;
    out = float(value);
    return true;
  };
  if (m.kind == FieldDeferredKind::Call && m.member == "start") {
    float v = -1;
    if (!m.args.empty() && !number(v))
      return fail(e, "Scene Timer start arguments rejected");
    return start(m.object, v, e);
  }
  if (m.kind == FieldDeferredKind::Call && m.member == "stop" && m.args.empty())
    return stop(m.object, e);
  if (m.kind == FieldDeferredKind::Set && m.member == "wait_time") {
    float v = 0;
    if (!number(v))
      return fail(e, "Scene Timer wait argument rejected");
    return set_wait(m.object, v, e);
  }
  if (m.kind == FieldDeferredKind::Set && m.args.size() == 1) {
    const auto *v = std::get_if<bool>(&m.args.front());
    if (v && m.member == "paused")
      return timers_.set_paused(m.object, *v, e);
    if (v && m.member == "one_shot")
      return timers_.set_one_shot(m.object, *v, e);
    if (v && m.member == "autostart")
      return timers_.set_autostart(m.object, *v, e);
  }
  return fail(e, "Scene Timer unsupported native member/arguments");
}
} // namespace encore::ctr
