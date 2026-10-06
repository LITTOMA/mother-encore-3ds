#include "encore/field_native_timer.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool FieldNativeTimers::initialize(const FieldNativeTimerData &d, Timeout t,
                                   Owner owner, std::string &e) {
  if (data_ || !d.valid() || !t || !owner)
    return fail(e, "Native Timer actual source/signal owner rejected");
  data_ = &d;
  timeout_ = std::move(t);
  owner_ = std::move(owner);
  return true;
}
FieldNativeTimers::Timer *FieldNativeTimers::timer(FieldObjectId id,
                                                   std::string &e) {
  auto i = timers_.find(id);
  auto *current = owner_ ? owner_(id) : nullptr;
  FieldIdentity identity;
  auto *descriptor = current ? current->descriptor(id) : nullptr;
  if (i == timers_.end() || !current || !current->state(id) ||
      !current->state(id)->alive || !descriptor ||
      descriptor->native_class != "Timer" ||
      !current->object_identity(id, identity) ||
      !data_->record(identity, descriptor->id)) {
    fail(e, "Native Timer ObjectDB owner missing");
    return nullptr;
  }
  i->second.tree = current;
  return &i->second;
}
bool FieldNativeTimers::attach(FieldNodeTreeRuntime &t, FieldObjectId id,
                               FieldNodeBinding &out, std::string &e) {
  FieldIdentity identity;
  auto *n = t.descriptor(id);
  auto *s = t.state(id);
  if (!data_ || owner_(id) != &t || !n || !s || !s->alive ||
      n->native_class != "Timer" || !t.object_identity(id, identity) ||
      timers_.count(id))
    return fail(e, "Native Timer actual class/ObjectID rejected");
  auto *d = data_->record(identity, n->id);
  if (!d || d->script_sha != n->script_sha)
    return fail(e, "Native Timer source property binding differs");
  Timer v;
  v.tree = &t;
  v.state.wait = d->wait;
  v.state.mode = d->mode;
  v.state.one_shot = d->flags & 1;
  v.state.autostart = d->flags & 2;
  timers_.emplace(id, v);
  out = {identity, n->id,         n->class_index, 0x454e0044,
         1,        n->script_sha, "Timer"};
  return true;
}
bool FieldNativeTimers::schedule(Timer &t, FieldObjectId id, bool processing,
                                 std::string &e) {
  const char *group =
      t.state.mode ? "idle_process_internal" : "physics_process_internal";
  bool ok = processing && !t.state.paused ? t.tree->add_group(id, group, e)
                                          : t.tree->remove_group(id, group, e);
  if (ok)
    t.state.processing = processing;
  return ok;
}
bool FieldNativeTimers::ready(FieldObjectId id, std::string &e) {
  auto *t = timer(id, e);
  if (!t || !t->tree->state(id)->inside)
    return fail(e, "Native Timer Ready outside actual Tree");
  if (!t->state.autostart)
    return true;
  if (!start(id, -1, e))
    return false;
  t->state.autostart = false;
  return true;
}
bool FieldNativeTimers::start(FieldObjectId id, float seconds, std::string &e) {
  auto *t = timer(id, e);
  if (!t || !std::isfinite(seconds) || !t->tree->state(id)->inside)
    return fail(e, "Native Timer start outside actual Tree/nonfinite time");
  if (seconds > 0)
    t->state.wait = seconds;
  t->state.left = t->state.wait;
  return schedule(*t, id, true, e);
}
bool FieldNativeTimers::stop(FieldObjectId id, std::string &e) {
  auto *t = timer(id, e);
  if (!t)
    return false;
  t->state.left = -1;
  t->state.autostart = false;
  return schedule(*t, id, false, e);
}
bool FieldNativeTimers::process(FieldObjectId id, FieldTreePhase phase,
                                float dt, bool paused, std::string &e) {
  auto *t = timer(id, e);
  if (!t || !std::isfinite(dt) || dt < 0)
    return fail(e, "Native Timer delta/owner rejected");
  if (phase != FieldTreePhase::IdleInternal &&
      phase != FieldTreePhase::PhysicsInternal)
    return fail(e, "Native Timer unexpected process notification");
  auto &s = t->state;
  if (!s.processing || s.paused || !t->tree->can_process(id, paused) ||
      (s.mode == 1) != (phase == FieldTreePhase::IdleInternal))
    return true;
  std::vector<FieldObjectId> members;
  if (!t->tree->group(s.mode ? "idle_process_internal"
                             : "physics_process_internal",
                      members, e))
    return false;
  if (std::find(members.begin(), members.end(), id) == members.end())
    return true;
  s.left -= dt;
  // Strictly negative, at most one timeout per native notification. Preserve
  // repeat overshoot and finish stop before synchronous reentrant callbacks.
  if (s.left >= 0)
    return true;
  if (s.one_shot) {
    if (!stop(id, e))
      return false;
  } else
    s.left += s.wait;
  return timeout_(id, e);
}
bool FieldNativeTimers::set_wait(FieldObjectId id, float v, std::string &e) {
  auto *t = timer(id, e);
  if (!t || !std::isfinite(v) || v <= 0)
    return fail(e, "Native Timer wait must be positive");
  t->state.wait = v;
  return true;
}
bool FieldNativeTimers::set_one_shot(FieldObjectId id, bool v, std::string &e) {
  auto *t = timer(id, e);
  if (!t)
    return false;
  t->state.one_shot = v;
  return true;
}
bool FieldNativeTimers::set_autostart(FieldObjectId id, bool v,
                                      std::string &e) {
  auto *t = timer(id, e);
  if (!t)
    return false;
  t->state.autostart = v;
  return true;
}
bool FieldNativeTimers::set_paused(FieldObjectId id, bool v, std::string &e) {
  auto *t = timer(id, e);
  if (!t)
    return false;
  if (v == t->state.paused)
    return true;
  t->state.paused = v;
  return schedule(*t, id, t->state.processing, e);
}
bool FieldNativeTimers::set_mode(FieldObjectId id, uint32_t v, std::string &e) {
  auto *t = timer(id, e);
  if (!t || v > 1)
    return fail(e, "Native Timer unknown process mode");
  if (v == t->state.mode)
    return true;
  const char *old =
      t->state.mode ? "idle_process_internal" : "physics_process_internal";
  std::vector<FieldObjectId> members;
  if (!t->tree->group(old, members, e))
    return false;
  if (std::find(members.begin(), members.end(), id) != members.end()) {
    if (!t->tree->remove_group(id, old, e) ||
        !t->tree->add_group(
            id, v ? "idle_process_internal" : "physics_process_internal", e))
      return false;
  }
  t->state.mode = v;
  return true;
}
bool FieldNativeTimers::release(FieldObjectId id, std::string &e) {
  auto *t = timer(id, e);
  if (!t)
    return false;
  if (!stop(id, e))
    return false;
  timers_.erase(id);
  return true;
}
const FieldNativeTimerState *FieldNativeTimers::state(FieldObjectId id) const {
  auto i = timers_.find(id);
  return i == timers_.end() ? nullptr : &i->second.state;
}
float FieldNativeTimers::time_left(FieldObjectId id) const {
  auto *s = state(id);
  return s ? std::max(s->left, 0.f) : 0.f;
}
} // namespace encore::upstream
