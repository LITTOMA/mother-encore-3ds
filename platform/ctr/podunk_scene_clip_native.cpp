#include "podunk_scene_clip_native.hpp"
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
bool PodunkSceneClipNative::prepare(
    const SceneClipNativeData &d, const FieldNodeTreeData &t,
    FieldNodeTreeRuntime &tree, FieldGlobalRegistry &r,
    FieldObjectSignals &signals, PodunkSceneTimers &timers,
    PodunkSceneNative &native, FieldOpenableDoorRuntime &o,
    FieldPresentRuntime &p, FieldEmoteRuntime &em, FieldBushRuntime &b,
    std::string &e) {
  if (data_ || !d.valid() || !t.valid() || !same(d.identity(), t.identity()) ||
      r.poisoned() || signals.registry() != &r ||
      (tree.object_domain() && tree.object_domain() != r.kernel()))
    return fail(e, "Scene clips checked source/actual owners rejected");
  data_ = &d;
  source_ = &t;
  tree_ = &tree;
  registry_ = &r;
  signals_ = &signals;
  timers_ = &timers;
  native_ = &native;
  openable_ = &o;
  present_ = &p;
  emote_ = &em;
  bush_ = &b;
  e.clear();
  return true;
}
bool PodunkSceneClipNative::apply(FieldOpenableHost &o, FieldPresentHost &p,
                                  FieldEmoteHost &em, FieldBushHost &b,
                                  std::string &e) {
  if (!data_ || applied_ || o.native_animation || p.native_animation ||
      em.native_animation || b.native_animation || o.native_timer_start ||
      o.native_timer_left || p.native_frame || em.native_frame ||
      b.native_frame)
    return fail(e, "Scene clips Host already owned/unprepared");
  o.native_animation = [this](uint32_t id, uint32_t clip, uint32_t ev,
                              std::string &v) {
    return event(SceneClipOwner::Openable, id, clip, ev, v);
  };
  p.native_animation = [this](uint32_t id, uint32_t clip, uint32_t ev,
                              std::string &v) {
    return event(SceneClipOwner::Present, id, clip, ev, v);
  };
  em.native_animation = [this](uint32_t id, uint32_t clip, uint32_t ev,
                               std::string &v) {
    return event(SceneClipOwner::Emote, id, clip, ev, v);
  };
  b.native_animation = [this](uint32_t id, uint32_t clip, uint32_t ev,
                              std::string &v) {
    return event(SceneClipOwner::Bush, id, clip, ev, v);
  };
  o.native_timer_start = [this](uint32_t id, float wait, std::string &v) {
    const auto *n = data_->owner(SceneClipOwner::Openable, id);
    FieldObjectId actualTimer = n ? tree_->source_object(n->timer) : 0;
    if (!actualTimer || timers_->registry() != registry_ ||
        !timers_->owns(actualTimer))
      return fail(v, "Openable actual source Timer missing");
    return timers_->start(actualTimer, wait, v);
  };
  o.native_timer_left = [this](uint32_t id, float &left, std::string &v) {
    const auto *n = data_->owner(SceneClipOwner::Openable, id);
    FieldObjectId actualTimer = n ? tree_->source_object(n->timer) : 0;
    if (!actualTimer || timers_->registry() != registry_ ||
        !timers_->owns(actualTimer))
      return fail(v, "Openable actual source Timer missing");
    return timers_->time_left(actualTimer, left, v);
  };
  p.native_frame = [this](uint32_t id, uint32_t v, std::string &e) {
    return frame(SceneClipOwner::Present, id, v, e);
  };
  em.native_frame = [this](uint32_t id, uint32_t v, std::string &e) {
    return frame(SceneClipOwner::Emote, id, v, e);
  };
  b.native_frame = [this](uint32_t id, uint32_t v, std::string &e) {
    return frame(SceneClipOwner::Bush, id, v, e);
  };
  applied_ = true;
  e.clear();
  return true;
}
bool PodunkSceneClipNative::owns(const FieldNodeDescriptor &n) const {
  return data_ && data_->record(n.id) && n.native_class == "AnimationPlayer" &&
         n.script.empty();
}
bool PodunkSceneClipNative::owns(FieldObjectId id) const {
  return leaves_.count(id) != 0;
}
bool PodunkSceneClipNative::actual(FieldObjectId id, std::string &e) const {
  auto i = leaves_.find(id);
  const auto *n = tree_ ? tree_->descriptor(id) : nullptr;
  const auto *s = tree_ ? tree_->state(id) : nullptr;
  FieldIdentity identity;
  if (!data_ || i == leaves_.end() || !n || !s || !s->alive ||
      registry_->poisoned() || !registry_->object_exists(id) ||
      registry_->tree_owner(id).get() != tree_ ||
      !tree_->object_identity(id, identity) ||
      !same(identity, data_->identity()) || n->id != i->second.source->id ||
      !owns(*n))
    return fail(e, "Scene clips actual AP/ObjectDB/source rejected");
  return true;
}
bool PodunkSceneClipNative::find(uint32_t stable, FieldObjectId &out,
                                 std::string &e) const {
  auto i = actuals_.find(stable);
  if (i == actuals_.end() || !actual(i->second, e))
    return fail(e, "Scene clips AP not actually allocated");
  out = i->second;
  return true;
}
bool PodunkSceneClipNative::construct(FieldObjectId id,
                                      const FieldNodeDescriptor &n,
                                      const FieldIdentity &identity,
                                      std::string &e) {
  if (!owns(n) || leaves_.count(id) || !same(identity, data_->identity()) ||
      !registry_->object_exists(id) ||
      registry_->tree_owner(id).get() != tree_ || !tree_->descriptor(id) ||
      tree_->descriptor(id)->id != n.id)
    return fail(e, "Scene clips actual constructor allocation rejected");
  Leaf leaf;
  leaf.source = data_->record(n.id);
  leaf.binding = {identity, n.id,         n.class_index, 0x454e006d,
                  1,        n.script_sha, n.native_class};
  leaves_.emplace(id, std::move(leaf));
  actuals_.emplace(n.id, id);
  e.clear();
  return true;
}
bool PodunkSceneClipNative::bind(FieldObjectId id, FieldNodeBinding &out,
                                 std::string &e) {
  if (!actual(id, e))
    return false;
  out = leaves_.at(id).binding;
  return true;
}
bool PodunkSceneClipNative::frame(SceneClipOwner k, uint32_t owner,
                                  uint32_t value, std::string &e) {
  const auto *r = data_->owner(k, owner);
  auto actual = r ? tree_->source_object(r->frame_target) : 0;
  if (!r || !actual || !registry_->object_exists(actual) ||
      registry_->tree_owner(actual).get() != tree_ || !native_->owns(actual))
    return fail(e, "Scene clips actual Sprite frame owner absent");
  return native_->sprite_set_frame(actual, value, e);
}
bool PodunkSceneClipNative::event(SceneClipOwner kind, uint32_t owner,
                                  uint32_t clip, uint32_t ev, std::string &e) {
  const auto *n = data_->owner(kind, owner);
  FieldObjectId id = 0;
  if (!n || !find(n->id, id, e))
    return fail(e, "Scene clips source event target absent");
  std::string name;
  switch (kind) {
  case SceneClipOwner::Openable: {
    const auto *d = openable_->data();
    const auto *r = d ? d->record(owner) : nullptr;
    const auto *c =
        r ? d->clip(r->profile, FieldOpenableClipRole(clip)) : nullptr;
    if (!c)
      return fail(e, "Scene clips Openable source animation lost");
    name = c->name;
    break;
  }
  case SceneClipOwner::Present: {
    const auto *d = present_->content();
    if (!d || clip < 1 || clip > 2)
      return fail(e, "Scene clips Present source animation lost");
    name = d->clip(clip == 2).name;
    break;
  }
  case SceneClipOwner::Emote: {
    const auto *d = emote_->data();
    const auto *c = d ? d->clip(clip) : nullptr;
    if (ev == 3 && !clip) {
      if (!tree_->remove_group(id, data_->internal_group(), e))
        return false;
      return true;
    }
    if (!c)
      return fail(e, "Scene clips Emote source animation lost");
    name = c->name;
    break;
  }
  case SceneClipOwner::Bush: {
    const auto *d = bush_->data();
    const auto *c = d ? d->clip(FieldBushClipRole(clip)) : nullptr;
    if (!c)
      return fail(e, "Scene clips Bush source animation lost");
    name = c->name;
    break;
  }
  }
  if (ev == 1) {
    if (!tree_->add_group(id, data_->internal_group(), e))
      return false;
    return signals_->emit(id, data_->started_signal(), {name}, e);
  }
  if (ev != 2 && ev != 3)
    return fail(e, "Scene clips unknown native animation event rejected");
  if (!tree_->remove_group(id, data_->internal_group(), e))
    return false;
  return ev == 3 || signals_->emit(id, data_->finished_signal(), {name}, e);
}
bool PodunkSceneClipNative::phase(FieldObjectId id, FieldTreePhase p, float dt,
                                  bool paused, bool, std::string &e) {
  if (!actual(id, e))
    return false;
  auto &l = leaves_.at(id);
  const auto *n = tree_->state(id);
  switch (p) {
  case FieldTreePhase::EnterNative:
    if (l.entered || !n->inside)
      return fail(e, "Scene clips actual Enter rejected");
    l.entered = true;
    return true;
  case FieldTreePhase::ReadyNative:
    if (!l.entered || !n->ready_notified || n->ready_first)
      return fail(e, "Scene clips actual Ready cursor rejected");
    l.ready = true;
    return true;
  case FieldTreePhase::IdleInternal: {
    if (!applied_ || !finished_ || !l.ready || !l.entered || !n->inside ||
        !tree_->can_process(id, paused))
      return fail(e, "Scene clips internal owner phase rejected");
    switch (l.source->kind) {
    case SceneClipOwner::Openable:
      if (!openable_->native_animation_frame(l.source->owner, dt)) {
        e = openable_->error();
        return false;
      }
      return true;
    case SceneClipOwner::Present:
      return present_->idle_animation_leaf(l.source->owner, dt, true, e);
    case SceneClipOwner::Emote:
      if (!emote_->idle_frame(l.source->owner, dt)) {
        e = emote_->error();
        return false;
      }
      return true;
    case SceneClipOwner::Bush:
      if (!bush_->idle_frame(l.source->owner, dt)) {
        e = bush_->error();
        return false;
      }
      return true;
    }
    return fail(e, "Scene clips unknown owner rejected");
  }
  case FieldTreePhase::ExitNative:
    if (!l.entered)
      return fail(e, "Scene clips actual Exit rejected");
    l.entered = false;
    return true;
  case FieldTreePhase::Deleting:
    return release(id, e);
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::PathChanged:
    return true;
  default:
    return fail(e, "Scene clips unsupported phase rejected");
  }
}
bool PodunkSceneClipNative::deferred(const FieldDeferredMessage &,
                                     std::string &e) {
  return fail(e, "Scene clips unknown native deferred method rejected");
}
bool PodunkSceneClipNative::release(FieldObjectId id, std::string &e) {
  if (!actual(id, e) || leaves_.at(id).entered)
    return fail(e, "Scene clips release before actual Exit rejected");
  actuals_.erase(leaves_.at(id).source->id);
  leaves_.erase(id);
  return true;
}
bool PodunkSceneClipNative::declaration(FieldObjectId id, std::string_view name,
                                        uint32_t &arity, std::string &e) const {
  if (!actual(id, e) ||
      (name != data_->started_signal() && name != data_->finished_signal()))
    return fail(e, "Scene clips unknown native signal rejected");
  arity = 1;
  return true;
}
bool PodunkSceneClipNative::method_owned(const FieldDeferredMessage &m) const {
  if (!data_ || m.kind != FieldDeferredKind::Call)
    return false;
  const auto *n = tree_->descriptor(m.object);
  if (!n)
    return false;
  const auto *v = data_->owner(SceneClipOwner::Openable, n->id);
  return v && m.member == v->timer_method;
}
bool PodunkSceneClipNative::source_method(const FieldDeferredMessage &m,
                                          std::string &e) {
  if (!method_owned(m) || !m.args.empty() || !finished_ ||
      !registry_->object_exists(m.object) ||
      registry_->tree_owner(m.object).get() != tree_)
    return fail(e, "Scene clips source Timer method/actual owner rejected");
  const auto *n = tree_->descriptor(m.object);
  if (!openable_->native_timer_timeout(n->id)) {
    e = openable_->error();
    return false;
  }
  return true;
}
bool PodunkSceneClipNative::finish_factory(std::string &e) {
  if (!data_ || !applied_ || finished_ ||
      leaves_.size() != data_->records().size() ||
      timers_->registry() != registry_)
    return fail(e, "Scene clips factory closure incomplete");
  for (const auto &r : data_->records()) {
    FieldObjectId ap = 0;
    if (!find(r.id, ap, e))
      return false;
    if (r.kind != SceneClipOwner::Openable)
      continue;
    auto timer = tree_->source_object(r.timer),
         owner = tree_->source_object(r.owner);
    if (!timer || !owner || !timers_->owns(timer) ||
        !registry_->object_exists(owner) ||
        registry_->tree_owner(owner).get() != tree_ ||
        tree_->descriptor(timer)->parent != r.owner)
      return fail(e, "Scene clips actual source Timer/parent missing");
    if (!signals_->connect(timer, data_->timeout_signal(), owner,
                           r.timer_method, FieldSignalPersist, {}, e))
      return false;
  }
  finished_ = true;
  e.clear();
  return true;
}
} // namespace encore::ctr
