#include "podunk_butterfly_timers.hpp"
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) { e = s; return false; }
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
// Native adapter endpoints, not game script methods or content bindings.
const char *method(uint32_t slot) {
  return slot ? "_native_butterfly_timeout_odd" : "_native_butterfly_timeout_even";
}
}
bool PodunkButterflyTimers::prepare(const FieldButterflyData &d,
    const FieldNodeTreeData &n, const FieldNativeTimerData &td,
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r, PodunkSceneTimers &timers,
    FieldObjectSignals &signals, FieldButterflyRuntime &runtime, std::string &e) {
  std::array<uint8_t, 32> sha{}, other{};
  if (data_ || !d.valid() || !n.valid() || !td.valid() || !r.kernel() ||
      (t.object_domain() && t.object_domain() != r.kernel()) || signals.registry() != &r ||
      (timers.registry() && timers.registry() != &r) ||
      d.source_pin() != n.identity().upstream_commit || d.scene() != n.source_scene() ||
      !d.source_hash(d.scene(), sha) || sha != n.identity().source_sha256 ||
      !d.source_hash(d.script(), sha) || !n.source_hash(d.script(), other) || sha != other)
    return fail(e, "Butterfly native Timer source/shared owners rejected");
  std::map<uint32_t, Waits> rows;
  for (const auto &b : d.bindings()) {
    const auto *root = n.record(b.id); const auto *leaf = n.record(b.timer_id);
    const auto *timer = td.record(n.identity(), b.timer_id);
    if (!root || root->script != d.script() || root->script_sha != sha ||
        root->ready != b.ready_ordinal || !leaf || leaf->parent != b.id ||
        leaf->class_index >= n.classes().size() || n.classes()[leaf->class_index] != "Timer" ||
        !leaf->script.empty() || leaf->script_methods || !timer ||
        timer->script_sha != leaf->script_sha || timer->wait != b.timer_wait ||
        timer->mode != 1 || timer->flags != 1 || leaf->ready != b.timer_ordinal ||
        leaf->ready >= root->ready || !rows.emplace(b.id, Waits{&b}).second)
      return fail(e, "Butterfly actual one-shot Timer source closure rejected");
  }
  if (rows.empty()) return fail(e, "Butterfly source Timer closure empty");
  data_ = &d; nodes_ = &n; timer_data_ = &td; tree_ = &t; registry_ = &r;
  timers_ = &timers; signals_ = &signals; runtime_ = &runtime; waits_ = std::move(rows);
  e.clear(); return true;
}
bool PodunkButterflyTimers::apply(FieldButterflyHost &host, std::string &e) {
  if (!data_ || applied_ || !host.admit_ready || host.timer_start_wait || host.timer_cancel)
    return fail(e, "Butterfly native Timer source port duplicate/unprepared");
  auto admit = host.admit_ready;
  host.admit_ready = [this, admit](const auto &b, auto &error) {
    return ready(b, error) && admit(b, error);
  };
  host.timer_start_wait = [this](auto id, auto phase, auto &error) { return start(id, phase, error); };
  host.timer_cancel = [this](auto id, auto &error) { return cancel(id, error); };
  applied_ = true; e.clear(); return true;
}
bool PodunkButterflyTimers::actual(const Waits &w, FieldObjectId &root,
    FieldObjectId &timer, std::string &e) const {
  if (!w.source || runtime_->content() != data_ || timers_->registry() != registry_ ||
      signals_->registry() != registry_ || tree_->object_domain() != registry_->kernel())
    return fail(e, "Butterfly native Timer actual owner mismatch");
  root = tree_->source_object(w.source->id); timer = tree_->source_object(w.source->timer_id);
  if ((w.root && root != w.root) || (w.timer && timer != w.timer))
    return fail(e, "Butterfly native Timer allocated ObjectIDs replaced");
  const auto *r = tree_->state(root); const auto *s = tree_->state(timer);
  const auto *rd = tree_->descriptor(root); const auto *sd = tree_->descriptor(timer);
  FieldIdentity ri, ti;
  if (!r || !s || !r->alive || !s->alive || !rd || !sd ||
      rd->id != w.source->id || rd->script != data_->script() ||
      sd->id != w.source->timer_id || sd->native_class != "Timer" || !sd->script.empty() ||
      s->parent != root || registry_->tree_owner(root).get() != tree_ ||
      registry_->tree_owner(timer).get() != tree_ || !registry_->object_exists(root) ||
      !registry_->object_exists(timer) || !tree_->object_identity(root, ri) ||
      !tree_->object_identity(timer, ti) || !same(ri, nodes_->identity()) || !same(ri, ti) ||
      !timer_data_->record(ti, sd->id) || !timers_->owns(timer))
    return fail(e, "Butterfly native Timer live source/parent identity rejected");
  e.clear(); return true;
}
bool PodunkButterflyTimers::ready(const FieldButterflyBinding &b, std::string &e) const {
  const auto at = waits_.find(b.id); FieldObjectId root, timer;
  if (at == waits_.end() || data_->binding(b.id) != &b || !actual(at->second, root, timer, e))
    return fail(e, "Butterfly onready actual Timer source missing");
  const auto *r = tree_->state(root); const auto *s = tree_->state(timer);
  const auto *native = timers_->state(timer);
  if (!r->inside || !r->ready_notified || !s->inside || !s->ready_notified ||
      !native || !native->one_shot || native->mode != 1 || native->wait != b.timer_wait || native->autostart)
    return fail(e, "Butterfly onready same native Timer not Ready/source properties changed");
  e.clear(); return true;
}
bool PodunkButterflyTimers::finish_factory(std::string &e) {
  if (!applied_) return fail(e, "Butterfly native Timer bridge not applied");
  for (auto &entry : waits_) {
    FieldObjectId root, timer;
    if (!actual(entry.second, root, timer, e) || tree_->state(root)->inside || tree_->state(timer)->inside)
      return fail(e, "Butterfly native Timer factory actual source closure incomplete");
    entry.second.root = root; entry.second.timer = timer;
  }
  e.clear(); return true;
}
bool PodunkButterflyTimers::start(uint32_t id, uint32_t phase, std::string &e) {
  auto at = waits_.find(id); FieldObjectId root, timer;
  if (!applied_ || phase > 1 || at == waits_.end() || !actual(at->second, root, timer, e) ||
      !runtime_->borrowed_timer() || !ready(*at->second.source, e))
    return fail(e, "Butterfly actual Timer start/yield source rejected");
  auto &w = at->second;
  if ((phase == 1) != w.dispatching)
    return fail(e, "Butterfly native Timer source continuation order rejected");
  // Original Timer.start() precedes yield's actual signal connection.
  if (!timers_->start(timer, 0, e)) return false;
  const uint32_t slot = w.dispatching ? 1 - w.dispatch_slot : (w.slots[1] ? 1 : 0);
  bool linked = false;
  if (!signals_->connected(timer, "timeout", root, method(slot), linked, e)) return false;
  if (linked != w.slots[slot]) return fail(e, "Butterfly native timeout connection ownership mismatch");
  if (!linked && !signals_->connect(timer, "timeout", root, method(slot), FieldSignalOneShot,
                                   {FieldObjectRef{timer}}, e)) return false;
  w.slots[slot] = true; e.clear(); return true;
}
bool PodunkButterflyTimers::handles_method(const FieldDeferredMessage &m) const {
  const auto *d = tree_ ? tree_->descriptor(m.object) : nullptr;
  return d && waits_.count(d->id) && m.kind == FieldDeferredKind::Call &&
         (m.member == method(0) || m.member == method(1));
}
bool PodunkButterflyTimers::deferred(const FieldDeferredMessage &m, std::string &e) {
  if (!handles_method(m) || m.args.size() != 1 || !std::holds_alternative<FieldObjectRef>(m.args[0]))
    return fail(e, "Butterfly native Timer callback signature rejected");
  auto &w = waits_.at(tree_->descriptor(m.object)->id); FieldObjectId root, timer;
  const uint32_t slot = m.member == method(0) ? 0 : 1;
  if (!actual(w, root, timer, e) || root != m.object ||
      std::get<FieldObjectRef>(m.args[0]).id != timer || !w.slots[slot] || w.dispatching)
    return fail(e, "Butterfly native Timer callback source/one-shot cohort rejected");
  const auto *s = timers_->state(timer); bool linked = false;
  if (!s || s->processing || s->left >= 0 || !s->one_shot ||
      !signals_->connected(timer, "timeout", root, method(slot), linked, e) || !linked)
    return fail(e, "Butterfly callback lacks actual native one-shot timeout boundary");
  // Bus removes ONE_SHOT after dispatch. A resumed second yield uses the
  // other endpoint so that cleanup cannot erase its newly created connection.
  w.dispatching = true; w.dispatch_slot = slot;
  const bool ok = runtime_->actual_timer_timeout(w.source->id, e);
  w.dispatching = false; w.slots[slot] = false;
  return ok;
}
bool PodunkButterflyTimers::cancel(uint32_t id, std::string &e) {
  auto at = waits_.find(id); FieldObjectId root, timer;
  if (at == waits_.end())
    return fail(e, "Butterfly native Timer cancellation source missing");
  auto &w = at->second;
  // Native PREDELETE destroys children before the root's release. The real
  // ObjectDB signal owner then retires emitter slots; no dead Timer methods
  // or fabricated release are dispatched here.
  const auto *root_state = tree_->state(w.root);
  const auto *root_descriptor = tree_->descriptor(w.root);
  FieldIdentity identity;
  if (w.root && w.timer && root_state && root_descriptor && !root_state->inside &&
      root_descriptor->id == id && registry_->tree_owner(w.root).get() == tree_ &&
      tree_->object_identity(w.root, identity) && same(identity, nodes_->identity()) &&
      !tree_->state(w.timer) && !registry_->object_exists(w.timer)) {
    w.slots[0] = w.slots[1] = false; e.clear(); return true;
  }
  if (!actual(w, root, timer, e)) return false;
  for (uint32_t slot = 0; slot < 2; ++slot) {
    bool linked = false;
    if (!signals_->connected(timer, "timeout", root, method(slot), linked, e) ||
        (linked && !signals_->disconnect(timer, "timeout", root, method(slot), e))) return false;
    w.slots[slot] = false;
  }
  // Object destruction owns Timer removal. No synthetic stop/reset on an
  // ordinary scene exit or on the Butterfly's screen-exit callback.
  e.clear(); return true;
}
} // namespace encore::ctr
