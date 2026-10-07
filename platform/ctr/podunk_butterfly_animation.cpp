#include "podunk_butterfly_animation.hpp"
#include <cmath>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) { e = s; return false; }
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
bool playing(const FieldButterflyState &s, uint32_t role) {
  return role == 1 ? s.fly_playing : s.orbit_playing;
}
bool generic(FieldTreePhase p) {
  switch (p) {
  case FieldTreePhase::PostEnterNative: case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented: case FieldTreePhase::ChildMoved:
  case FieldTreePhase::Deleting: case FieldTreePhase::PathChanged:
    return true;
  default: return false;
  }
}
}
bool PodunkButterflyAnimation::prepare(
    const FieldButterflyData &d, const FieldNodeTreeData &n,
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r, FieldButterflyRuntime &v,
    PodunkSceneNpcWorld &w, std::string &e) {
  std::array<uint8_t, 32> a{}, b{};
  if (data_ || !d.valid() || !n.valid() || !r.kernel() ||
      (t.object_domain() && t.object_domain() != r.kernel()) ||
      d.scene() != n.source_scene() || d.source_pin() != n.identity().upstream_commit ||
      !d.source_hash(d.scene(), a) || a != n.identity().source_sha256 ||
      !d.source_hash(d.script(), a) || !n.source_hash(d.script(), b) || a != b ||
      !d.clip(1) || !d.clip(2))
    return fail(e, "Butterfly animation checked source/domain mismatch");
  std::map<uint32_t, Player> p;
  for (const auto &s : d.bindings()) {
    const auto *root = n.record(s.id);
    if (!root || root->script != d.script() || root->script_sha != a ||
        root->path != s.node || root->ready != s.ready_ordinal ||
        root->class_index >= n.classes().size() ||
        n.classes()[root->class_index] != "KinematicBody2D")
      return fail(e, "Butterfly animation root source identity rejected");
    for (uint32_t role = 1; role <= 2; ++role) {
      const uint32_t leaf = role == 1 ? s.fly_id : s.orbit_id;
      const uint32_t ready = role == 1 ? s.fly_ordinal : s.orbit_ordinal;
      const auto *node = n.record(leaf);
      if (!node || node->parent != s.id || node->ready != ready ||
          ready >= s.ready_ordinal || !node->script.empty() || node->script_methods ||
          node->native_generated || node->class_index >= n.classes().size() ||
          n.classes()[node->class_index] != "AnimationPlayer" ||
          !p.emplace(leaf, Player{&s, leaf, role, 0, true, false, false}).second)
        return fail(e, "Butterfly actual native AnimationPlayer closure rejected");
    }
  }
  if (p.empty() || p.size() != d.bindings().size() * 2)
    return fail(e, "Butterfly two-player source closure empty/incomplete");
  data_ = &d; nodes_ = &n; tree_ = &t; registry_ = &r; runtime_ = &v; world_ = &w;
  players_ = std::move(p); e.clear(); return true;
}
bool PodunkButterflyAnimation::apply(FieldButterflyHost &h, std::string &e) {
  if (!data_ || applied_ || !h.admit_ready || !h.publish || !h.body_snapshot)
    return fail(e, "Butterfly actual Sprite/Area/source admission ports missing");
  auto admit = h.admit_ready;
  h.admit_ready = [this, admit](const auto &b, auto &error) {
    return source_ready(b, error) && admit(b, error);
  };
  auto publisher = h.publish;
  h.publish = [this, publisher](const auto &b, const auto &s, auto &error) {
    return publisher(b, s, error) && publish(b, s, error);
  };
  applied_ = true; e.clear(); return true;
}
bool PodunkButterflyAnimation::owns(const FieldNodeDescriptor &d) const {
  const auto *n = nodes_ ? nodes_->record(d.id) : nullptr;
  return n && players_.count(d.id) && d.native_class == "AnimationPlayer" &&
         d.path == n->path && d.parent == n->parent && d.class_index == n->class_index &&
         d.script.empty() && !d.script_methods && d.script_sha == n->script_sha &&
         !d.native_generated;
}
bool PodunkButterflyAnimation::owns(FieldObjectId id) const { return objects_.count(id) != 0; }
bool PodunkButterflyAnimation::construct(FieldObjectId id,
    const FieldNodeDescriptor &d, const FieldIdentity &i, std::string &e) {
  const auto *s = tree_ ? tree_->state(id) : nullptr;
  if (!owns(d) || !same(i, nodes_->identity()) || objects_.count(id) ||
      registry_->tree_owner(id).get() != tree_ || !registry_->object_exists(id) ||
      tree_->object_domain() != registry_->kernel() || !s || s->inside || s->parent ||
      s->ready_notified || !s->name.empty() || players_.at(d.id).object)
    return fail(e, "Butterfly AnimationPlayer actual constructor rejected");
  players_.at(d.id).object = id; objects_.emplace(id, d.id);
  e.clear(); return true;
}
const PodunkButterflyAnimation::Player *PodunkButterflyAnimation::actual(
    FieldObjectId id, std::string &e) const {
  const auto at = objects_.find(id);
  const auto *s = tree_ ? tree_->state(id) : nullptr;
  const auto *d = tree_ ? tree_->descriptor(id) : nullptr;
  FieldIdentity i;
  if (at == objects_.end() || !s || !s->alive || !d || !owns(*d) ||
      !tree_->object_identity(id, i) || !same(i, nodes_->identity()) ||
      registry_->tree_owner(id).get() != tree_ || !registry_->object_exists(id)) {
    fail(e, "Butterfly AnimationPlayer actual owner/identity lost"); return nullptr;
  }
  const auto &p = players_.at(at->second);
  if (p.object != id || (s->parent && (!tree_->descriptor(s->parent) ||
      tree_->descriptor(s->parent)->id != p.source->id))) {
    fail(e, "Butterfly AnimationPlayer actual parent mismatch"); return nullptr;
  }
  e.clear(); return &p;
}
bool PodunkButterflyAnimation::bind(FieldObjectId id, FieldNodeBinding &out, std::string &e) {
  const auto *p = actual(id, e); if (!p) return false;
  const auto *d = tree_->descriptor(id);
  out = {nodes_->identity(), p->leaf, d->class_index, 0x454e003c, 3,
         d->script_sha, d->native_class}; e.clear(); return true;
}
bool PodunkButterflyAnimation::sync(Player &p, std::string &e) {
  if (!actual(p.object, e)) return false;
  const auto *s = runtime_->state(p.source->id);
  const bool processing = p.entered && p.ready && p.active && s && s->ready &&
                          s->alive && playing(*s, p.role);
  return processing ? tree_->add_group(p.object, "idle_process_internal", e)
                    : tree_->remove_group(p.object, "idle_process_internal", e);
}
bool PodunkButterflyAnimation::source_ready(const FieldButterflyBinding &b, std::string &e) const {
  const auto body = tree_->source_object(b.id);
  const auto *node = tree_->state(body);
  if (data_->binding(b.id) != &b || runtime_->content() != data_ || !node ||
      !node->inside || !node->ready_notified || !world_->native_entered(body, e))
    return fail(e, "Butterfly source Ready requires same actual Kinematic body");
  for (auto leaf : {b.fly_id, b.orbit_id}) {
    const auto &p = players_.at(leaf);
    const auto *s = tree_->state(p.object);
    if (!actual(p.object, e) || !p.entered || !p.ready || !s || !s->inside ||
        !s->ready_notified || s->parent != body)
      return fail(e, "Butterfly source onready actual AnimationPlayer not Ready");
  }
  e.clear(); return true;
}
bool PodunkButterflyAnimation::publish(const FieldButterflyBinding &b,
    const FieldButterflyState &s, std::string &e) {
  if (data_->binding(b.id) != &b || runtime_->state(b.id) != &s || !s.ready ||
      runtime_->content() != data_)
    return fail(e, "Butterfly publisher foreign source/runtime state");
  const auto body = tree_->source_object(b.id);
  if (!world_->native_entered(body, e) || !world_->publish_position(b.id, runtime_->world_position(b.id), e) ||
      !tree_->set_process(body, false, s.process, e) || !tree_->set_visible(body, s.visible, e))
    return false;
  return sync(players_.at(b.fly_id), e) && sync(players_.at(b.orbit_id), e);
}
bool PodunkButterflyAnimation::finish_factory(std::string &e) const {
  if (!applied_ || objects_.size() != players_.size())
    return fail(e, "Butterfly actual AnimationPlayer factory closure incomplete");
  for (const auto &entry : players_) {
    const auto &p = entry.second;
    const auto *s = tree_->state(p.object);
    if (!actual(p.object, e) || tree_->source_object(p.leaf) != p.object || !s ||
        s->inside || s->parent != tree_->source_object(p.source->id))
      return fail(e, "Butterfly AnimationPlayer actual factory parent/index mismatch");
  }
  e.clear(); return true;
}
bool PodunkButterflyAnimation::set_active(FieldObjectId id, bool enabled, std::string &e) {
  const auto *p = actual(id, e); if (!p) return false;
  auto &v = players_.at(p->leaf); v.active = enabled;
  return sync(v, e);
}
bool PodunkButterflyAnimation::active(FieldObjectId id, bool &out, std::string &e) const {
  const auto *p = actual(id, e); if (!p) return false;
  out = p->active; e.clear(); return true;
}
bool PodunkButterflyAnimation::playback(FieldObjectId id, bool &running, float &time,
    std::string &e) const {
  const auto *p = actual(id, e); if (!p) return false;
  const auto *s = runtime_->state(p->source->id);
  if (!s || runtime_->content() != data_) return fail(e, "Butterfly shared playback state missing");
  running = playing(*s, p->role); time = p->role == 1 ? s->fly_time : s->orbit_time;
  e.clear(); return true;
}
bool PodunkButterflyAnimation::phase(FieldObjectId id, FieldTreePhase phase,
    float dt, bool paused, bool /*update_pending*/, std::string &e) {
  const auto *p = actual(id, e); if (!p) return false;
  auto &v = players_.at(p->leaf); const auto *s = tree_->state(id);
  switch (phase) {
  case FieldTreePhase::EnterNative:
    if (v.entered || !s->inside || !s->parent)
      return fail(e, "Butterfly AnimationPlayer duplicate/invalid Enter");
    v.entered = true; return sync(v, e);
  case FieldTreePhase::ReadyNative:
    if (!v.entered || v.ready || !s->inside || !s->ready_notified)
      return fail(e, "Butterfly AnimationPlayer actual Ready order rejected");
    v.ready = true; return sync(v, e);
  case FieldTreePhase::ExitNative:
    if (!v.entered) return fail(e, "Butterfly AnimationPlayer Exit without Enter");
    v.entered = false; return sync(v, e);
  case FieldTreePhase::IdleInternal:
    if (!std::isfinite(dt) || dt < 0 || !v.entered || !v.ready || !s->inside ||
        runtime_->content() != data_)
      return fail(e, "Butterfly AnimationPlayer actual idle boundary rejected");
    // Original AnimationPlayer internal idle does not gate on OS rendering.
    if (v.active && tree_->can_process(id, paused))
      return runtime_->idle_leaf(v.leaf, dt, true, e);
    break;
  default:
    if (!generic(phase)) return fail(e, "Butterfly AnimationPlayer unsupported native notification");
    break;
  }
  e.clear(); return true;
}
bool PodunkButterflyAnimation::deferred(const FieldDeferredMessage &m, std::string &e) {
  if (!actual(m.object, e)) return false;
  if ((m.kind == FieldDeferredKind::Set && m.member == "active") ||
      (m.kind == FieldDeferredKind::Call && m.member == "set_active")) {
    if (m.args.size() != 1 || !std::holds_alternative<bool>(m.args[0]))
      return fail(e, "Butterfly AnimationPlayer active argument rejected");
    return set_active(m.object, std::get<bool>(m.args[0]), e);
  }
  return fail(e, "Butterfly AnimationPlayer unsupported external playback method");
}
bool PodunkButterflyAnimation::release(FieldObjectId id, std::string &e) {
  const auto *p = actual(id, e); if (!p) return false;
  auto &v = players_.at(p->leaf); const auto *s = tree_->state(id);
  if (v.entered || s->inside || !s->children.empty())
    return fail(e, "Butterfly AnimationPlayer release before actual Exit");
  objects_.erase(id); v.object = 0; v.ready = false; v.active = true;
  e.clear(); return true;
}
} // namespace encore::ctr
